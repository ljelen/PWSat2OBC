#include <cstdint>
#include <gsl/span>

#include "logger/logger.h"
#include "system.h"
#include "utils.h"

#include "yaffs.h"

using gsl::span;
using std::size_t;
using drivers::spi::ISPIInterface;
using namespace devices::n25q;

static inline size_t BlockSize(BlockMapping blockMapping)
{
    switch (blockMapping)
    {
        case BlockMapping::Sector:
            return 64_KB;
        case BlockMapping::SubSector:
            return 4_KB;
        default:
            return 4_KB;
    }
}

N25QYaffsDeviceBase::N25QYaffsDeviceBase(const char* mountPoint,
    const devices::n25q::BlockMapping blockMapping,
    const std::size_t chunkSize,
    const std::size_t totalSize,
    N25QDriver& driver)
    : _driver(driver), //
      _blockMapping(blockMapping)
{
    memset(&this->_device, 0, sizeof(this->_device));

    this->_device.param.name = mountPoint;

    this->_device.param.inband_tags = true;
    this->_device.param.is_yaffs2 = true;
    this->_device.param.total_bytes_per_chunk = chunkSize;
    this->_device.param.chunks_per_block = BlockSize(blockMapping) / this->_device.param.total_bytes_per_chunk;
    this->_device.param.spare_bytes_per_chunk = 0;
    this->_device.param.start_block = 1;
    this->_device.param.n_reserved_blocks = 3;
    this->_device.param.no_tags_ecc = true;
    this->_device.param.always_check_erased = true;
    this->_device.param.disable_bad_block_marking = true;

    this->_device.driver_context = this;
    this->_device.drv.drv_read_chunk_fn = N25QYaffsDeviceBase::ReadChunk;
    this->_device.drv.drv_write_chunk_fn = N25QYaffsDeviceBase::WriteChunk;
    this->_device.drv.drv_erase_fn = N25QYaffsDeviceBase::EraseBlock;
    this->_device.drv.drv_mark_bad_fn = N25QYaffsDeviceBase::MarkBadBlock;
    this->_device.drv.drv_check_bad_fn = N25QYaffsDeviceBase::CheckBadBlock;

    auto blockSize = this->_device.param.chunks_per_block * this->_device.param.total_bytes_per_chunk;

    this->_device.param.end_block = totalSize / blockSize //
        - this->_device.param.start_block                 //
        - this->_device.param.n_reserved_blocks;
}

OSResult N25QYaffsDeviceBase::Mount(services::fs::IYaffsDeviceOperations& deviceOperations)
{
    auto result = deviceOperations.AddDeviceAndMount(&this->_device);
    if (OS_RESULT_SUCCEEDED(result))
    {
        LOGF(LOG_LEVEL_INFO, "[Device %s] Mounted successfully", this->_device.param.name);
        return OSResult::Success;
    }
    else
    {
        LOGF(LOG_LEVEL_ERROR, "[Device %s] Mount failed: %d", this->_device.param.name, num(result));
        return result;
    }
}

int N25QYaffsDeviceBase::ReadChunk(struct yaffs_dev* dev, //
    int nand_chunk,
    u8* data,
    int data_len,
    u8* oob,
    int oob_len,
    enum yaffs_ecc_result* ecc_result)
{
    UNREFERENCED_PARAMETER(oob);

    if (oob_len > 0)
    {
        LOGF(LOG_LEVEL_ERROR, "Trying to read OOB %d bytes", oob_len);
        return YAFFS_FAIL;
    }

    auto This = reinterpret_cast<N25QYaffsDeviceBase*>(dev->driver_context);

    *ecc_result = yaffs_ecc_result::YAFFS_ECC_RESULT_NO_ERROR;

    auto baseAddress = nand_chunk * dev->param.total_bytes_per_chunk;

    span<uint8_t> buffer(data, data_len);

    This->_driver.ReadMemory(baseAddress, buffer);

    return YAFFS_OK;
}

int N25QYaffsDeviceBase::WriteChunk(struct yaffs_dev* dev, //
    int nand_chunk,
    const u8* data,
    int data_len,
    const u8* oob,
    int oob_len)
{
    UNREFERENCED_PARAMETER(oob);

    if (oob_len > 0)
    {
        LOGF(LOG_LEVEL_ERROR, "Trying to write OOB %d bytes", oob_len);
        return YAFFS_FAIL;
    }

    auto This = reinterpret_cast<N25QYaffsDeviceBase*>(dev->driver_context);

    auto baseAddress = nand_chunk * dev->param.total_bytes_per_chunk;

    span<const uint8_t> buffer(data, data_len);

    auto result = This->_driver.WriteMemory(baseAddress, buffer);

    if (result != OperationResult::Success)
    {
        LOGF(LOG_LEVEL_ERROR, "[Device %s] Write to chunk %d failed Error %d", dev->param.name, nand_chunk, num(result));
        return YAFFS_FAIL;
    }

    return YAFFS_OK;
}

int N25QYaffsDeviceBase::EraseBlock(struct yaffs_dev* dev, int block_no)
{
    auto This = reinterpret_cast<N25QYaffsDeviceBase*>(dev->driver_context);

    LOGF(LOG_LEVEL_INFO, "[Device %s] Erasing block %d", dev->param.name, block_no);

    auto baseAddress = block_no * dev->param.chunks_per_block * dev->param.total_bytes_per_chunk;

    auto result = OperationResult::Failure;

    switch (This->_blockMapping)
    {
        case BlockMapping::Sector:
            result = This->_driver.EraseSector(baseAddress);
            break;

        case BlockMapping::SubSector:
            result = This->_driver.EraseSubSector(baseAddress);
            break;
    }

    if (result != OperationResult::Success)
    {
        LOGF(LOG_LEVEL_ERROR, "[Device %s] Erase block failed: %d Error %d", dev->param.name, block_no, num(result));
        return YAFFS_FAIL;
    }

    return YAFFS_OK;
}

int N25QYaffsDeviceBase::MarkBadBlock(struct yaffs_dev* dev, int block_no)
{
    UNREFERENCED_PARAMETER(dev);
    LOGF(LOG_LEVEL_WARNING, "[Device %s] Marking bad block %d", dev->param.name, block_no);

    return YAFFS_OK;
}

int N25QYaffsDeviceBase::CheckBadBlock(struct yaffs_dev* dev, int block_no)
{
    UNUSED(dev, block_no);

    return YAFFS_OK;
}