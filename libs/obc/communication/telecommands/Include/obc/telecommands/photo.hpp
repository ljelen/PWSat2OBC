#ifndef LIBS_OBC_COMMUNICATION_TELECOMMANDS_INCLUDE_OBC_TELECOMMANDS_PHOTO_HPP_
#define LIBS_OBC_COMMUNICATION_TELECOMMANDS_INCLUDE_OBC_TELECOMMANDS_PHOTO_HPP_

#pragma once

#include "photo/fwd.hpp"
#include "telecommunication/downlink.h"
#include "telecommunication/telecommand_handling.h"

namespace obc
{
    namespace telecommands
    {
        /**
         * @brief Take photo telecommand
         * @ingroup obc_telecommands
         * @telecommand
         *
         * Code: 0x1F
         * Parameters:
         *  - Correlation ID (8-bit)
         *  - Camera identifier
         *  - Photo resolution
         *  - Number of photographs to take
         *  - Output file name (string, null-terminated, up to 30 characters including terminator)
         */
        class TakePhoto final : public telecommunication::uplink::Telecommand<0x1F>
        {
          public:
            /**
             * @brief ctor
             * @param[in] photoService Reference to service capable of taking photos
             */
            TakePhoto(services::photo::IPhotoService& photoService);

            virtual void Handle(devices::comm::ITransmitter& transmitter, gsl::span<const std::uint8_t> parameters) override;

          private:
            services::photo::IPhotoService& _photoService;
        };

        /**
         * @brief Purge photo service state.
         * @ingroup obc_telecommands
         * @telecommand
         * 
         * This command is responsible for resetting photo service state and removing any enqueued commands that have 
         * not yet been executed.
         *
         * Code: 0x22
         * Parameters:
         *  - Correlation ID (8-bit)
         */
        class PurgePhoto final : public telecommunication::uplink::Telecommand<0x22>
        {
          public:
            /**
             * @brief ctor
             * @param[in] photoService Reference to service capable of taking photos
             */
            PurgePhoto(services::photo::IPhotoService& photoService);

            virtual void Handle(devices::comm::ITransmitter& transmitter, gsl::span<const std::uint8_t> parameters) override;

          private:
            services::photo::IPhotoService& _photoService;
        };
    }
}

#endif /* LIBS_OBC_COMMUNICATION_TELECOMMANDS_INCLUDE_OBC_TELECOMMANDS_PHOTO_HPP_ */
