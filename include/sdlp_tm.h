#ifndef SDLP_TM_H
#define SDLP_TM_H

#include "sdlp_common.h"

#define TM_PRIMARY_HEADER_SIZE 6
#define TM_FRAME_ERROR_CONTROL_SIZE 2
#define TM_MAX_DATA_SIZE 1024

/* Transfer Frame Secondary Header (CCSDS 132.0-B-3, 4.1.3): a 1-octet Identification
 * Field followed by a Data Field of 1 to 63 octets, so up to 64 octets in total. */
#define TM_SECONDARY_HEADER_ID_SIZE 1
#define TM_SECONDARY_HEADER_MAX_DATA 63

/* Operational Control Field (CCSDS 132.0-B-3, 4.1.5): a fixed 4-octet trailer field. */
#define TM_OCF_SIZE 4

/* Transfer Frame Data Field Status sub-field values (CCSDS 132.0-B-3, 4.1.2.7). */
#define TM_SEGMENT_LENGTH_ID_NO_SEGMENTATION                                                       \
    0x03u /* '11'; mandatory when Sync Flag = 0 (4.1.2.7.5.2) */
#define TM_FIRST_HEADER_POINTER_NO_PACKET                                                          \
    0x07ffu /* no Packet starts in the Data Field (4.1.2.7.6.4) */
#define TM_FIRST_HEADER_POINTER_ONLY_IDLE                                                          \
    0x07feu /* Only Idle Data (OID) Transfer Frame (4.1.2.7.6.5) */

/* Transfer Frame Data Field Status (2 octets, CCSDS 132.0-B-3, 4.1.2.7).
 * Wire layout, most significant bit first:
 *   secondary header flag (1) | synchronization flag (1) | packet order flag (1) |
 *   segment length identifier (2) | first header pointer (11). */
typedef struct
{
    uint8_t secondary_header_flag; /* presence of the Transfer Frame Secondary Header */
    uint8_t sync_flag;             /* 0 = Packets/Idle Data, 1 = VCA_SDU */
    uint8_t packet_order_flag;     /* reserved ('0') when sync_flag = 0 */
    uint8_t segment_length_id;     /* 2 bits; '11' when sync_flag = 0 */
    uint16_t first_header_pointer; /* 11 bits; offset of the first Packet or a special value */
} sdlp_tm_data_field_status_t;

typedef struct
{
    uint16_t transfer_frame_version : 2;
    uint16_t spacecraft_id : 10;
    uint16_t virtual_channel_id : 3;
    uint16_t ocf_flag : 1;
    uint8_t master_channel_frame_count;
    uint8_t virtual_channel_frame_count;
    sdlp_tm_data_field_status_t transfer_frame_data_field_status;
} sdlp_tm_header_t;

/* Transfer Frame Secondary Header (CCSDS 132.0-B-3, 4.1.3).
 * The 1-octet Identification Field carries a 2-bit Version Number (bits 0-1, '00')
 * and a 6-bit Length (bits 2-7). The wire Length equals the total Secondary Header
 * size minus one, which (the Identification Field being one octet) is exactly the
 * Data Field length. `length` == 0 means the Secondary Header is absent. */
typedef struct
{
    uint8_t version; /* Secondary Header Version Number ('00') */
    uint8_t length;  /* Data Field length in octets (1..63), 0 = absent */
    uint8_t data[TM_SECONDARY_HEADER_MAX_DATA]; /* Secondary Header Data Field */
} sdlp_tm_secondary_header_t;

typedef struct
{
    sdlp_tm_header_t header;
    sdlp_tm_secondary_header_t secondary_header;
    uint8_t data[TM_MAX_DATA_SIZE];
    uint16_t data_length;
    uint8_t ocf[TM_OCF_SIZE]; /* valid only when header.ocf_flag is set */
    uint16_t fecf;
} sdlp_tm_frame_t;

/* Pack the Transfer Frame Data Field Status into its 2-octet wire value
 * (CCSDS 132.0-B-3, 4.1.2.7). */
uint16_t sdlp_tm_pack_data_field_status(const sdlp_tm_data_field_status_t *status);

/* Parse a 2-octet Transfer Frame Data Field Status into its sub-fields. */
void sdlp_tm_unpack_data_field_status(uint16_t raw, sdlp_tm_data_field_status_t *status);

int sdlp_tm_create_frame(sdlp_tm_frame_t *frame,
                         uint16_t spacecraft_id,
                         uint8_t virtual_channel_id,
                         const uint8_t *data,
                         uint16_t data_length);

/* Attach a Transfer Frame Secondary Header (CCSDS 132.0-B-3, 4.1.3): set the
 * Secondary Header Flag and copy `length` (1..63) octets of Data Field content.
 * The Secondary Header Version Number is set to '00'. */
int sdlp_tm_set_secondary_header(sdlp_tm_frame_t *frame, const uint8_t *data, uint8_t length);

/* Attach a 4-octet Operational Control Field (CCSDS 132.0-B-3, 4.1.5): set the OCF
 * Flag and copy the OCF content. The content (e.g. a CLCW or an SDLS report) is
 * mission-specific and supplied verbatim by the caller. Leave this unset to emit a
 * frame with no OCF. */
int sdlp_tm_set_ocf(sdlp_tm_frame_t *frame, const uint8_t ocf[TM_OCF_SIZE]);

int sdlp_tm_encode_frame(const sdlp_tm_frame_t *frame,
                         uint8_t *buffer,
                         size_t buffer_size,
                         size_t *encoded_size);

int sdlp_tm_decode_frame(const uint8_t *buffer, size_t buffer_size, sdlp_tm_frame_t *frame);

#endif
