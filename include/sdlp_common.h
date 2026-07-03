/**
 * @file sdlp_common.h
 * @brief Shared definitions for the CCSDS Space Data Link Protocols.
 *
 * Protocol version and the library-wide return codes used by the TM
 * (CCSDS 132.0-B-3) and TC (CCSDS 232.0-B-4) frame handlers.
 */

#ifndef SDLP_COMMON_H
#define SDLP_COMMON_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define SDLP_VERSION 0 /**< Transfer Frame Version Number — '00' for both TM and TC. */

#define SDLP_SUCCESS 0                 /**< Operation completed successfully. */
#define SDLP_ERROR_INVALID_PARAM -1    /**< NULL pointer or out-of-range argument. */
#define SDLP_ERROR_BUFFER_TOO_SMALL -2 /**< Output buffer smaller than the encoded frame. */
#define SDLP_ERROR_INVALID_FRAME -3    /**< Malformed or inconsistent frame on decode. */

#ifdef __cplusplus
}
#endif

#endif /* SDLP_COMMON_H */
