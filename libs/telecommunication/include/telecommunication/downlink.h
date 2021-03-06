#ifndef LIBS_TELECOMMUNICATION_INCLUDE_TELECOMMUNICATION_DOWNLINK_H_
#define LIBS_TELECOMMUNICATION_INCLUDE_TELECOMMUNICATION_DOWNLINK_H_

#include <cstdint>
#include "base/writer.h"
#include "comm/comm.hpp"
#include "gsl/span"

namespace telecommunication
{
    namespace downlink
    {
        /**
         * @ingroup telecomm_handling
         * @{
         */

        /**
         * @brief Byte that can be used to detect beacon frame.
         */
        constexpr std::uint8_t BeaconMarker = 0xCD;

        /**
         * @brief Downlink APID definition
         *
         * @remark All values are 6-bit
         * @remark @ref DownlinkAPID::LastItem must be last item on the enum list
         */
        enum class DownlinkAPID : std::uint8_t
        {
            Pong = 0x01,                       //!< Pong
            Operation = 0x2,                   //!< Operation command
            ErrorCounters = 0x3,               //!< Error counters list
            ProgramUpload = 0x4,               //!< Program upload operation status
            PeriodicMessage = 0x5,             //!< Periodic message
            PersistentState = 0x6,             //!< Persistent state content
            BootSlotsInfo = 0x7,               //!< Boot slots info
            CompileInfo = 0x8,                 //!< Software Build information
            EraseFlash = 0x9,                  //!< Erase flash
            FileRemove = 0xA,                  //!< Removing file
            FileSend = 0xB,                    //!< Sending file
            FileList = 0xC,                    //!< List files operation
            Forbidden = (BeaconMarker & 0x3f), //!< Reserved apid due to beacon collision (0xCD & 0x3f = 0x0D)
            Photo = 0xE,                       //!< Take Photo
            SunS = 0xF,                        //!< SunS telemetry
            Experiment = 0x10,                 //!< Experiment commands results
            ErrorCounterConfiguration = 0x11,  //!< Generic acknowledge frame
            PurgePhoto = 0x12,                 //!< Purge Photo command
            Powercycle = 0x13,                 //!< Power Cycle
            Sail = 0x14,                       //!< Sail
            TimeCorrection = 0x15,             //!< Time Correction
            TimeSet = 0x16,                    //!< Time setting
            Comm = 0x17,                       //!< Commands for Comm subsystem
            SetBitrate = 0x18,                 //!< SetBitrate command
            DisableOverheatSubmode = 0x19,     //!< DisableOverheatSubmode command
            I2C = 0x1A,                        //!< I2C Raw
            PeriodicSet = 0x1B,                //!< Set periodic message
            SailExperiment = 0x1C,             //!< Automatically sent sail experiment data
            CopyBootTable = 0x1D,              //!< Automatically sent during copy boot slots experiment
            SetInternalDetumblingMode = 0x1E,  //!< Disabling imtq built-in detumbling command
            SetAdcsMode = 0x1F,                //!< Set Adcs Mode telecommand
            DisableSailDeployment = 0x20,      //!< Disable automatic sail deployment
            MemoryContent = 0x21,              //!< Memory contents
            BeaconError = 0x22,                //!< Beacon Error
            DisableAntennaDeployment = 0x23,   //!< Disable automatic antenna deployment
            Telemetry = 0x3F,                  //!< TelemetryLong
            LastItem                           //!< LastItem
        };

        /**
         * @brief Standard responses definition
         */
        enum class DownlinkGenericResponse : std::uint8_t
        {
            Success = 0,          //!< Success
            MalformedRequest = 1, //!< Malformed request
            ExperimentError = 2,  //!< Error in experiment execution
        };

        /**
         * @brief Type that represents raw downlink frame.
         */
        class RawFrame
        {
          public:
            /**
             * @brief Initializes new @ref RawFrame instance
             */
            RawFrame();

            /**
             * @brief Returns writer that can be used to fill payload part of frame
             * @return Writer
             */
            Writer& PayloadWriter();

            /**
             * @brief Returns underlying byte representation of the frame
             * @return
             */
            gsl::span<const uint8_t> Frame();

          private:
            /** @brief Buffer in which frame is built */
            std::array<uint8_t, devices::comm::MaxDownlinkFrameSize> _frame;

            /** @brief Writer instance used to build frame payload */
            Writer _payloadWriter;
        };

        inline Writer& RawFrame::PayloadWriter()
        {
            return this->_payloadWriter;
        }

        inline gsl::span<const uint8_t> RawFrame::Frame()
        {
            return this->_payloadWriter.Capture();
        }

        /**
         * @brief Downlink frame implementation
         */
        class DownlinkFrame
        {
          public:
            /**
             * @brief Initializes new @ref DownlinkFrame instance
             * @param apid APID
             * @param seq Sequence number
             */
            DownlinkFrame(DownlinkAPID apid, std::uint32_t seq);

            /**
             * @brief Returns writer that can be used to fill payload part of frame
             * @return Writer
             */
            inline Writer& PayloadWriter();

            /**
             * @brief Returns underlying byte representation of the frame
             * @return
             */
            inline gsl::span<const uint8_t> Frame();

            /** @brief Size of header size */
            static constexpr std::uint8_t HeaderSize = 3;
            /** @brief Maximum size of payload inside single frame */
            static constexpr std::uint8_t MaxPayloadSize = devices::comm::MaxDownlinkFrameSize - HeaderSize;

          private:
            /** @brief Buffer in which frame is built */
            std::array<uint8_t, devices::comm::MaxDownlinkFrameSize> _frame;

            /** @brief Writer instance used to build frame payload */
            Writer _payloadWriter;
        };

        inline Writer& DownlinkFrame::PayloadWriter()
        {
            return this->_payloadWriter;
        }

        inline gsl::span<const uint8_t> DownlinkFrame::Frame()
        {
            return gsl::make_span(this->_frame).subspan(0, HeaderSize + this->_payloadWriter.GetDataLength());
        }

        /**
         * @brief Helper class for building correlated downlink frame
         */
        class CorrelatedDownlinkFrame : public DownlinkFrame
        {
          public:
            /**
             * @brief Initializes new @ref DownlinkFrame instance
             * @param apid APID
             * @param seq Sequence number
             * @param correlationId Identifier of the request that prompted sending this frame.
             */
            CorrelatedDownlinkFrame(DownlinkAPID apid, std::uint32_t seq, std::uint8_t correlationId);

            /** @brief Maximum size of payload inside single frame */
            static constexpr std::uint8_t MaxPayloadSize = DownlinkFrame::MaxPayloadSize - 1;
        };

        /** @} */
    }
}

#endif /* LIBS_TELECOMMUNICATION_INCLUDE_TELECOMMUNICATION_DOWNLINK_H_ */
