#include <em_cmu.h>
#include <em_gpio.h>

#include "base/os.h"
#include "io_map.h"
#include "logger/logger.h"

#include "i2c.h"

void I2CIRQHandler(I2CBus* bus)
{
    I2C_TransferReturn_TypeDef status = I2C_Transfer((I2C_TypeDef*)bus->HWInterface);

    if (status == i2cTransferInProgress)
    {
        return;
    }

    if (!System.QueueSendISR(bus->ResultQueue, &status, NULL))
    {
        LOG_ISR(LOG_LEVEL_ERROR, "Error queueing i2c result");
    }

    System.EndSwitchingISR(NULL);
}

static I2CResult ExecuteTransfer(I2CBus* bus, I2C_TransferSeq_TypeDef* seq)
{
    if (OS_RESULT_FAILED(System.TakeSemaphore(bus->Lock, MAX_DELAY)))
    {
        LOGF(LOG_LEVEL_ERROR, "[I2C] Taking semaphore failed. Address: %X", seq->addr);
        return I2CResultFailure;
    }

    I2C_TypeDef* hw = (I2C_TypeDef*)bus->HWInterface;

    I2C_TransferReturn_TypeDef rawResult = (I2CResult)I2C_TransferInit(hw, seq);

    if (rawResult != i2cTransferInProgress)
    {
        System.GiveSemaphore(bus->Lock);
        return (I2CResult)rawResult;
    }

    if (!System.QueueReceive(bus->ResultQueue, &rawResult, I2C_TIMEOUT * 1000)) // I2C_TIMEOUT * 1000
    {
        I2CResult ret = I2CResultTimeout;

        LOG(LOG_LEVEL_ERROR, "Didn't received i2c transfer result");

        hw->CMD = I2C_CMD_STOP | I2C_CMD_ABORT;

        while (HAS_FLAG(hw->STATUS, I2C_STATUS_PABORT))
        {
        }

        if (GPIO_PinInGet((GPIO_Port_TypeDef)bus->IO.Port, bus->IO.SCL) == 0)
        {
            LOG(LOG_LEVEL_ERROR, "SCL latched at low level");

            ret = I2CResultClockLatched;
        }

        System.GiveSemaphore(bus->Lock);

        return ret;
    }

    System.GiveSemaphore(bus->Lock);

    return (I2CResult)rawResult;
}

static I2CResult Write(I2CBus* bus, const I2CAddress address, const uint8_t* data, size_t length)
{
    I2C_TransferSeq_TypeDef seq = //
        {
            .addr = address,         //
            .flags = I2C_FLAG_WRITE, //
            .buf =                   //
            {
                {.len = length, .data = (uint8_t*)data}, //
                {.len = 0, .data = NULL}                 //
            }                                            //
        };

    return ExecuteTransfer(bus, &seq);
}

static I2CResult WriteRead(
    I2CBus* bus, const I2CAddress address, const uint8_t* inData, size_t inLength, uint8_t* outData, size_t outLength)
{
    I2C_TransferSeq_TypeDef seq = //
        {
            .addr = address,              //
            .flags = I2C_FLAG_WRITE_READ, //
            .buf =                        //
            {
                {.len = inLength, .data = (uint8_t*)inData}, //
                {.len = outLength, .data = outData}          //
            }                                                //
        };

    return ExecuteTransfer(bus, &seq);
}

void I2CSetupInterface(I2CBus* bus,
    I2C_TypeDef* hw,
    uint16_t location,
    GPIO_Port_TypeDef port,
    uint16_t sdaPin,
    uint16_t sclPin,
    CMU_Clock_TypeDef clock,
    IRQn_Type irq)
{
    bus->Extra = NULL;
    bus->HWInterface = hw;
    bus->IO.Port = (uint16_t)port;
    bus->IO.SCL = sclPin;
    bus->IO.SDA = sdaPin;

    bus->Write = Write;
    bus->WriteRead = WriteRead;

    bus->ResultQueue = System.CreateQueue(1, sizeof(I2C_TransferReturn_TypeDef));

    bus->Lock = System.CreateBinarySemaphore();
    System.GiveSemaphore(bus->Lock);

    CMU_ClockEnable(clock, true);

    GPIO_PinModeSet(port, sdaPin, gpioModeWiredAndPullUpFilter, 1);
    GPIO_PinModeSet(port, sclPin, gpioModeWiredAndPullUpFilter, 1);

    I2C_Init_TypeDef init = I2C_INIT_DEFAULT;
    init.clhr = i2cClockHLRStandard;
    init.enable = true;

    I2C_Init(hw, &init);
    hw->ROUTE = I2C_ROUTE_SCLPEN | I2C_ROUTE_SDAPEN | location;

    I2C_IntEnable(hw, I2C_IEN_TXC);

    NVIC_SetPriority(irq, I2C_IRQ_PRIORITY);
    NVIC_EnableIRQ(irq);
}
