#ifndef LIBS_FS_INCLUDE_FS_FS_H_
#define LIBS_FS_INCLUDE_FS_FS_H_

#include <stdbool.h>
#include <cstdint>
#include <gsl/span>
#include "base/os.h"
#include "system.h"
#include "yaffs.hpp"

namespace services
{
    namespace fs
    {
        /**
         * @defgroup fs File system
         *
         * @brief File system API.
         *
         * @{
         */

        /** @brief Directory handle */
        using DirectoryHandle = void*;

        /** @brief File handle */
        using FileHandle = int;

        // yaffs imposes 2GB file size limit
        /** @brief Type that represents file size. */
        using FileSize = std::int32_t;

        /**
         * @brief General I/O operation result
         */
        template <typename TResult> struct IOOperationResult
        {
            /**
             * @brief Ctor
             * @param status Operation status
             * @param result Result
             */
            IOOperationResult(OSResult status, TResult result);

            /**
             * @brief Converts to true if operation succeeded
             */
            inline operator bool() const;

            /** Operation status. */
            const OSResult Status;
            /** Operation result */
            const TResult Result;
        };

        template <typename TResult>
        IOOperationResult<TResult>::IOOperationResult(OSResult status, TResult result) : Status(status), Result(result)
        {
        }

        template <typename TResult> IOOperationResult<TResult>::operator bool() const
        {
            return OS_RESULT_SUCCEEDED(this->Status);
        }

        /** @brief Type that represents file opening status. */
        using FileOpenResult = IOOperationResult<FileHandle>;

        /** @brief Type that represents directory opening status. */
        using DirectoryOpenResult = IOOperationResult<DirectoryHandle>;

        /** @brief Read/Write operation result */
        using IOResult = IOOperationResult<gsl::span<const uint8_t>>;

        /**
         * @brief Enumerator of all possible file opening modes.
         */
        enum class FileOpen
        {
            /** Open file only if it already exist, fail if it does not exist. */
            Existing = 0,

            /** Opens a file and truncates it so that its size is zero bytes, only if it exists. */
            TruncateExisting = O_TRUNC,

            /** Open file, create a new one if it does not exist. */
            OpenAlways = O_CREAT,

            /** Always create new file, if it exists truncate its content to zero. */
            CreateAlways = O_CREAT | O_TRUNC,

            /** Creates a new file, only if it does not already exist, fail if it exists. */
            CreateNew = O_CREAT | O_EXCL,

            /** If set, the file offset shall be set to the end of the file prior to each write.
             * Fail if file does not exist */
            AppendExisting = O_APPEND,

            /** If set, the file offset shall be set to the end of the file prior to each write.
             * Create a new file if it does not exist. */
            AppendAlways = O_APPEND | O_CREAT,
        };

        /**
         * @brief Enumerator of all possible file access modes.
         */
        enum class FileAccess
        {
            /** Open only for reading. */
            ReadOnly = O_RDONLY,
            /** Open only for writing. */
            WriteOnly = O_WRONLY,
            /** Open for reading and writing. */
            ReadWrite = O_RDWR,
        };

        /**
         * @brief Determines base for seek operation
         */
        enum class SeekOrigin
        {
            Begin,   //!< Begining of file
            Current, //!< Current position
            End      //!< End of file
        };

        /**
         * @brief Structure exposing file system API
         */
        struct IFileSystem
        {
            /**
             * @brief Opens file
             * @param[in] path Path to file
             * @param[in] openFlag File opening flags. @see FSFileOpenFlags for details.
             * @param[in] accessMode Requested file access mode. @see FSFileAccessMode for details.
             * @return Operation status. @see FSFileOpenResult for details.
             */
            virtual FileOpenResult Open(const char* path, FileOpen openFlag, FileAccess accessMode) = 0;

            /**
             * @brief Unlinks a file
             * @param[in] path Path to file
             * @return Operation status.
             */
            virtual OSResult Unlink(const char* path) = 0;

            /**
             * @brief Moves specified file to target location.
             *
             * If moved file does not exist this function will return failure. If the target file exists
             * it will be overwritten be the moved file.
             * @param[in] from Path to file to be moved.
             * @param[in] to Path to new file location.
             * @return Operation status. @see FSFileOpenResult for details.
             */
            virtual OSResult Move(const char* from, const char* to) = 0;

            /**
             * @brief Copies specified file to target location.
             *
             * If copied file does not exist this function will return failure. If the target file exists
             * it will be overwritten be the copied file.
             * @param[in] from Path to file to be copied.
             * @param[in] to Path to copy file location.
             * @return Operation status. @see FSFileOpenResult for details.
             */
            virtual OSResult Copy(const char* from, const char* to) = 0;

            /**
             * @brief Truncates file to given size
             * @param[in] file File handle
             * @param[in] length Desired length
             * @return Operation status.
             */
            virtual OSResult TruncateFile(FileHandle file, FileSize length) = 0;

            /**
             * @brief Writes data to file
             * @param[in] file File handle
             * @param[in] buffer Data buffer
             * @return Operation status. @see FSIOResult for details.
             */
            virtual IOResult Write(FileHandle file, gsl::span<const std::uint8_t> buffer) = 0;

            /**
             * @brief Reads data from file
             * @param[in] file File handle
             * @param[out] buffer Data buffer
             * @return Operation status. @see FSIOResult for details.
             */
            virtual IOResult Read(FileHandle file, gsl::span<std::uint8_t> buffer) = 0;

            /**
             * @brief Closes file
             * @param[in] file File handle
             * @return Operation status.
             */
            virtual OSResult Close(FileHandle file) = 0;

            /**
             * @brief Opens directory
             * @param[in] dirname Directory path
             * @return Directory handle on success. @see FSDirectoryOpenResult for details.
             */
            virtual DirectoryOpenResult OpenDirectory(const char* dirname) = 0;

            /**
             * @brief Reads name of next entry in directory.
             * @param[in] directory Directory handle
             * @return Entry name. NULL if no more entries found.
             */
            virtual char* ReadDirectory(DirectoryHandle directory) = 0;

            /**
             * @brief Closes directory
             * @param[in] directory Directory handle
             * @return Operation status.
             */
            virtual OSResult CloseDirectory(DirectoryHandle directory) = 0;

            /**
             * @brief Checks if path is directory
             * @param[in] path Path
             * @return True if path is directory, false otherwise.
             */
            virtual bool IsDirectory(const char* path) = 0;

            /**
             * @brief Formats partition at given mount point. Partition is
             * unmounted before format and mounted again afterwards.
             * @param[in] mountPoint Partition mount point
             * @return Operation status
             */
            virtual OSResult Format(const char* mountPoint) = 0;

            /**
             * @brief Creates new directory
             * @param[in] path Path to directory that should be created
             * @return Operation status
             */
            virtual OSResult MakeDirectory(const char* path) = 0;

            /**
             * @brief Checks if path exists
             * @param[in] path Path to check
             * @return true if path exists
             */
            virtual bool Exists(const char* path) = 0;

            /**
             * @brief Gets file size
             * @param file File handle
             * @return Size of file
             */
            virtual FileSize GetFileSize(FileHandle file) = 0;

            /**
             * @brief Gets file size
             * @param dir Directory containing file
             * @param file File name
             * @return Size of file or -1 in case of error
             */
            virtual FileSize GetFileSize(const char* dir, const char* file) = 0;

            /**
             * @brief Changes position in file
             * @param file File handle
             * @param origin Point from which seeking starts
             * @param offset Offset
             * @return Operation result
             */
            virtual OSResult Seek(FileHandle file, SeekOrigin origin, FileSize offset) = 0;

            /**
             * @brief Queries the specified device for its free space size.
             * @param devicePath Path to the device mount point.
             * @return The specified device's free space in bytes or 0xffffffff in case of errors.
             */
            virtual std::uint32_t GetFreeSpace(const char* devicePath) = 0;
        };

        /**
         * @brief Wrapper over file handle
         */
        class File : private NotCopyable
        {
          public:
            /** @brief Desctructor */
            ~File();

            /**
             * @brief default ctor
             */
            File();

            /**
             * @brief Move ctor
             * @param other Other file
             */
            File(File&& other) noexcept;

            /**
             * @brief Factory method that opens file
             * @param fs File system
             * @param path File path
             * @param mode Open mode
             * @param access Access
             * @return File instance
             */
            File(IFileSystem& fs, const char* path, FileOpen mode, FileAccess access);

            /**
             * @brief Move operator
             * @param other Other file
             * @return Reference to this
             */
            File& operator=(File&& other) noexcept;

            /** @brief Implicit cast to bool, true if file opened successfully*/
            inline operator bool();

            /**
             * @brief Reads from file
             * @param buffer Buffer
             * @return Operation result
             */
            IOResult Read(gsl::span<uint8_t> buffer);

            /**
             * @brief Writes to file
             * @param buffer Buffer
             * @return Operation result
             */
            IOResult Write(gsl::span<const uint8_t> buffer);

            /**
             * @brief Truncates file to desired size
             * @param size Desired size
             * @return Operation result
             */
            OSResult Truncate(FileSize size);

            /**
             * @brief Returns size of opened file
             * @return File size
             */
            FileSize Size();

            /**
             * @brief Closes file
             * @return Operation result
             *
             * @brief After calling this method object becomes unusable
             */
            OSResult Close();

            /**
             * @brief Seeks position in file
             * @param origin Determines what is used as starting point for offset
             * @param offset Position offset
             * @return Operation result
             */
            OSResult Seek(SeekOrigin origin, FileSize offset);

          private:
            /** @brief File system interface */
            IFileSystem* _fs;
            /** @brief File handle */
            FileHandle _handle;
            /** @brief Flag indicating whether file is opened successfully */
            bool _valid;
        };

        File::operator bool()
        {
            return this->_fs != nullptr && this->_valid;
        }

        /**
         * @brief This method is responsible for writing contents of the passed buffer to the selected file.
         *
         * If the selected file exists it will be overwritten, if it does not exist it will be created.
         * @param[in] fs FileSystem interface for accessing files.
         * @param[in] file Path to file that should be saved.
         * @param[in] buffer Buffer that contains data that should be saved.
         *
         * @return Operation status. True in case of success, false otherwise.
         */
        bool SaveToFile(IFileSystem& fs, const char* file, gsl::span<const std::uint8_t> buffer);

        /**
         * @brief This procedure is responsible for reading the the contents of the specified file.
         *
         * @param[in] fs FileSystem interface for accessing files.
         * @param[in] filePath Path to the file that contains timer state.
         * @param[in] buffer Buffer that should be updated with the file contents.
         *
         * @return Operation status.
         * @retval true Requested data has been successfully read.
         * @retval false Either selected file was not found, file could not be opened or did not contain
         * requested amount of data.
         */
        bool ReadFromFile(IFileSystem& fs, const char* const filePath, gsl::span<std::uint8_t> buffer);

        /** @} */
    }
}

#endif /* LIBS_FS_INCLUDE_FS_FS_H_ */
