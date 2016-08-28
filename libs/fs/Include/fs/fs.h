#ifndef LIBS_FS_INCLUDE_FS_FS_H_
#define LIBS_FS_INCLUDE_FS_FS_H_

#include <stdbool.h>
#include <stdint.h>
#include "base/os.h"
#include "system.h"

EXTERNC_BEGIN

#include <yaffs_guts.h>

/**
 * @defgroup fs File system
 *
 * @brief File system API. POSIX-compatible
 *
 * Wrappers for YAFFS file system.
 * Partitions:
 * - / - NAND memory, 1MB, 1 page per chunk
 *
 * @{
 */

/** @brief Directory handle */
typedef void* FSDirectoryHandle;

/** @brief File handle */
typedef int FSFileHandle;

// yaffs imposes 2GB file size limit
/** @brief Type that represents file size. */
typedef int32_t FSFileSize;

/**
 * @brief Type that represents file opening status.
 */
typedef struct
{
    /** Operation status. */
    OSResult Status;
    /** Opened file handle. */
    FSFileHandle FileHandle;
} FSOpenResult;

/**
 * @brief Type that represents file read/write operation status.
 */
typedef struct
{
    /** Operation status. */
    OSResult Status;
    /** Number of bytes transferred. */
    FSFileSize BytesTransferred;
} FSIOResult;

/**
 * @brief Structure exposing file system API
 */
typedef struct FileSystemTag FileSystem;

/**
 * @brief Enumerator of all possible file opening modes.
 */
typedef enum {
    /** Open file only if it already exist, fail if it does not exist. */
    FsOpenExisting = 0,

    /** Open file, create a new one if it does not exist. */
    FsOpenAlways = O_CREAT,

    /** Always create new file, if it exists truncate its content to zero. */
    FsOpenCreateAlways = O_CREAT | O_EXCL | O_TRUNC,

    /** If set, the file offset shall be set to the end of the file prior to each write. */
    FsOpenAppend = O_APPEND,
} FSFileOpenFlags;

/**
 * @brief Enumerator of all possible file access modes.
 */
typedef enum {
    /** Open for reading only. */
    FsReadOnly = O_RDONLY,
    /** Open for writing only. */
    FsWriteOnly = O_WRONLY,
    /** Open for reading and writing. */
    FsReadWrite = O_RDWR,
} FSFileAccessMode;

/**
 * @brief Structure exposing file system API
 */
typedef struct FileSystemTag
{
    /**
     * @brief Opens file
     * @param[in] path Path to file
     * @param[in] openFlag File opening flags. @see FSFileOpenFlags for details.
     * @param[in] accessMode Requested file access mode. @see FSFileAccessMode for details.
     * @return Operation status. @see FSOpenResult for details.
     */
    FSOpenResult (*open)(FileSystem* fileSystem, const char* path, FSFileOpenFlags openFlag, FSFileAccessMode accessMode);

    /**
     * @brief Truncates file to given size
     * @param[in] file File handle
     * @param[in] length Desired length
     * @return Operation status.
     */
    OSResult (*ftruncate)(FileSystem* fileSystem, FSFileHandle file, FSFileSize length);

    /**
     * @brief Writes data to file
     * @param[in] file File handle
     * @param[in] buffer Data buffer
     * @param[in] size Size of data
     * @return Operation status. @see FSIOResult for details.
     */
    FSIOResult (*write)(FileSystem* fileSystem, FSFileHandle file, const void* buffer, FSFileSize size);

    /**
     * @brief Reads data from file
     * @param[in] file File handle
     * @param[out] buffer Data buffer
     * @param[in] size Size of data
     * @return Operation status. @see FSIOResult for details.
     */
    FSIOResult (*read)(FileSystem* fileSystem, FSFileHandle file, void* buffer, FSFileSize size);

    /**
     * @brief Closes file
     * @param[in] file File handle
     * @return Operation status.
     */
    OSResult (*close)(FileSystem* fileSystem, FSFileHandle file);

    /**
     * @brief Opens directory
     * @param[in] dirname Directory path
     * @return Directory handle on success. NULL on error.
     */
    FSDirectoryHandle (*openDirectory)(FileSystem* fileSystem, const char* dirname);

    /**
     * @brief Reads name of next entry in directory
     * @param[in] directory Directory handle
     * @return Entry name. NULL if no more entries found.
     */
    char* (*readDirectory)(FileSystem* fileSystem, FSDirectoryHandle directory);

    /**
     * @brief Closes directory
     * @param[in] directory Directory handle
     * @return Operation status.
     */
    OSResult (*closeDirectory)(FileSystem* fileSystem, FSDirectoryHandle directory);

    /**
     * @brief Gets last error code
     * @return Error code, @see errno.h
     */
    int (*getLastError)(FileSystem* fileSystem);
} FileSystem;

/**
 * @brief Initializes file system interface
 * @param[inout] fs File system interface
 * @param[in] rootDevice root device driver
 * @return true if initialization was successful
 */
bool FileSystemInitialize(FileSystem* fs, struct yaffs_dev* rootDevice);

/**
 * @brief This method is responsible for writing contents of the passed buffer to the selected file.
 *
 * If the selected file exists it will be overwritten, if it does not exist it will be created.
 * @param[in] fs FileSystem interface for accessing files.
 * @param[in] file Path to file that should be saved.
 * @param[in] buffer Pointer to buffer that contains data that should be saved.
 * @param[in] length Size of data in bytes the buffer.
 *
 * @return Operation status. True in case of success, false otherwise.
 */
bool FileSystemSaveToFile(FileSystem* fs, const char* file, const uint8_t* buffer, FSFileSize length);

/**
 * @brief This procedure is responsible for reading the the contents of the specified file.
 *
 * @param[in] fs FileSystem interface for accessing files.
 * @param[in] filePath Path to the file that contains timer state.
 * @param[in] buffer Pointer to buffer that should be updated with the file contents.
 * @param[in] length Size of the requested data.
 *
 * @return Operation status.
 * @retval true Requested data has been successfully read.
 * @retval false Either selected file was not found, file could not be opened or did not contain
 * requested amount of data.
 */
bool FileSystemReadFile(FileSystem* fs, const char* const filePath, uint8_t* buffer, FSFileSize length);

/** @} */

EXTERNC_END

#endif /* LIBS_FS_INCLUDE_FS_FS_H_ */
