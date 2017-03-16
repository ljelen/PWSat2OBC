#ifndef LIBS_OBC_COMMUNICATION_TELECOMMANDS_INCLUDE_OBC_TELECOMMANDS_FILE_SYSTEM_HPP_
#define LIBS_OBC_COMMUNICATION_TELECOMMANDS_INCLUDE_OBC_TELECOMMANDS_FILE_SYSTEM_HPP_

#include "fs/fs.h"
#include "telecommunication/downlink.h"
#include "telecommunication/telecommand_handling.h"

namespace obc
{
    namespace telecommands
    {
        /**
         * @brief Generic mechanism for sending file part-by-part
         * @ingroup telecommands
         */
        class FileSender final
        {
          public:
            /**
             * @brief Ctor
             * @param path Path to file that will be send
             * @param apid APID that will be used in downlink frames
             * @param transmitter Transmitter
             * @param fs File system
             */
            FileSender(const char* path,
                telecommunication::downlink::DownlinkAPID apid,
                devices::comm::ITransmitFrame& transmitter,
                services::fs::IFileSystem& fs);

            /**
             * @brief Checks if requested operation is valid
             * @retval true Everything is ok
             * @retval false Requested file does not exist
             */
            bool IsValid();
            /**
             * @brief Sends single part of file
             * @param seq Sequence number indicating which part of file should be sent
             * @return Operation result
             */
            bool SendPart(std::uint32_t seq);

          private:
            /** @brief File to send */
            services::fs::File _file;
            /** @brief APID used is downlink frames */
            telecommunication::downlink::DownlinkAPID _apid;
            /** @brief Transmitter */
            devices::comm::ITransmitFrame& _transmitter;
            /** @brief Opened file size */
            services::fs::FileSize _fileSize;
            /** @brief Last sequence number available for file */
            std::uint32_t _lastSeq;
        };

        /**
         * @brief Download any file
         * @ingroup telecommands
         */
        class DownladFileTelecommand final : public telecommunication::uplink::IHandleTeleCommand
        {
          public:
            /**
             * @brief Ctor
             * @param fs File system
             */
            DownladFileTelecommand(services::fs::IFileSystem& fs);

            virtual std::uint8_t CommandCode() const override;

            virtual void Handle(devices::comm::ITransmitFrame& transmitter, gsl::span<const std::uint8_t> parameters) override;

          private:
            /** @brief File system */
            services::fs::IFileSystem& _fs;
        };
    }
}

#endif /* LIBS_OBC_COMMUNICATION_TELECOMMANDS_INCLUDE_OBC_TELECOMMANDS_FILE_SYSTEM_HPP_ */