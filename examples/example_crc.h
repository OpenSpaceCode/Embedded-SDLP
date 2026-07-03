/**
 * @file example_crc.h
 * @brief CRC-16-CCITT helper used only by the examples.
 *
 * The SDLP library does not compute or validate the Frame Error Control Field;
 * error control is the application's responsibility. This helper lets the
 * examples populate and verify the FECF, and is intentionally kept out of the
 * library's public API.
 */

#ifndef EXAMPLE_CRC_H
#define EXAMPLE_CRC_H

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Compute a CRC-16-CCITT (polynomial 0x1021, initial value 0xFFFF).
 *
 * @param[in] data   Input octets.
 * @param[in] length Number of octets.
 * @return The 16-bit CRC.
 */
static inline uint16_t example_crc16(const uint8_t *data, size_t length)
{
    uint16_t crc = 0xFFFFu;

    for (size_t i = 0; i < length; i++)
    {
        crc ^= (uint16_t)data[i] << 8;

        for (int j = 0; j < 8; j++)
        {
            if (crc & 0x8000u)
            {
                crc = (crc << 1) ^ 0x1021u;
            }
            else
            {
                crc = crc << 1;
            }
        }
    }

    return crc;
}

#endif
