#ifndef EXAMPLE_CRC_H
#define EXAMPLE_CRC_H

#include <stddef.h>
#include <stdint.h>

/* CRC-16-CCITT (polynomial 0x1021, initial value 0xFFFF).
 *
 * The SDLP library no longer computes or validates the Frame Error Control
 * Field; error control is the application's responsibility. This helper is
 * provided solely so the examples can demonstrate populating and verifying the
 * FECF, and is intentionally kept out of the library's public API. */
static inline uint16_t example_crc16(const uint8_t *data, size_t length)
{
    uint16_t crc = 0xFFFF;

    for (size_t i = 0; i < length; i++)
    {
        crc ^= (uint16_t)data[i] << 8;

        for (int j = 0; j < 8; j++)
        {
            if (crc & 0x8000)
            {
                crc = (crc << 1) ^ 0x1021;
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
