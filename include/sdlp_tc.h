#ifndef SDLP_TC_H
#define SDLP_TC_H

#include "sdlp_common.h"

#define TC_PRIMARY_HEADER_SIZE 5
#define TC_FRAME_ERROR_CONTROL_SIZE 2

/* CCSDS 232.0-B-4, 4.1.2.7.2: the whole Transfer Frame is at most 1024 octets. */
#define TC_MAX_FRAME_SIZE 1024

/* Maximum Transfer Frame Data Field: the frame minus the primary header and the
 * Frame Error Control Field (4.1.3.1.2). When a Segment Header is present it consumes
 * one further octet of the data field (enforced in sdlp_tc_create_frame). */
#define TC_MAX_DATA_SIZE (TC_MAX_FRAME_SIZE - TC_PRIMARY_HEADER_SIZE - TC_FRAME_ERROR_CONTROL_SIZE)

/* TC Transfer Frame types: combined states of the Bypass Flag and the Control
 * Command Flag (CCSDS 232.0-B-4, table 4-1). Bypass=0 with Control Command=1 is
 * reserved for future application. */
typedef enum
{
    SDLP_TC_FRAME_TYPE_AD = 0, /* Bypass=0, CC=0: FDU, Sequence-Controlled (AD) service */
    SDLP_TC_FRAME_TYPE_BD,     /* Bypass=1, CC=0: FDU, Expedited (BD) service */
    SDLP_TC_FRAME_TYPE_BC      /* Bypass=1, CC=1: Control Commands for the FARM */
} sdlp_tc_frame_type_t;

/* Control Commands carried by Type-BC frames (CCSDS 232.0-B-4, 4.1.3.3). */
#define TC_CONTROL_CMD_UNLOCK 0x00u /* Unlock: a single 'all zeroes' octet (4.1.3.3.2) */
#define TC_CONTROL_CMD_UNLOCK_LENGTH 1u
#define TC_CONTROL_CMD_SET_VR_OCTET0 0x82u /* Set V(R): '10000010 00000000 XXXXXXXX' (4.1.3.3.3)   \
                                            */
#define TC_CONTROL_CMD_SET_VR_OCTET1 0x00u
#define TC_CONTROL_CMD_SET_VR_LENGTH 3u

#ifdef TC_SEGMENT_HEADER_ENABLED

#    define TC_SEGMENT_HEADER_SIZE 1

/* TC Segment Header Sequence Flags (CCSDS 232.0-B-4, table 4-2).
 * Bit 0 is the MSB and bit 1 is the LSB of the 2-bit field (bits 0-1 of the segment header octet).
 * Bit 0 | Bit 1 | Interpretation
 *   0       0     Continuing portion of SDU on one MAP
 *   0       1     First portion of SDU on one MAP
 *   1       0     Last portion of SDU on one MAP
 *   1       1     No segmentation (one complete SDU or multiple packets) */
typedef enum
{
    TC_SEQ_FLAG_CONTINUE = 0x00, /* 00: Continuing portion of SDU */
    TC_SEQ_FLAG_FIRST = 0x01,    /* 01: First portion of SDU */
    TC_SEQ_FLAG_LAST = 0x02,     /* 10: Last portion of SDU */
    TC_SEQ_FLAG_NO_SEG = 0x03    /* 11: No segmentation (complete SDU or multiple packets) */
} sdlp_tc_seq_flag_t;

typedef struct
{
    uint8_t sequence_flags : 2; /* bits 0-1: Sequence Flags */
    uint8_t map_id : 6;         /* bits 2-7: MAP Identifier */
} sdlp_tc_segment_header_t;

#endif /* TC_SEGMENT_HEADER_ENABLED */

typedef struct
{
    uint16_t transfer_frame_version : 2;
    uint16_t bypass_flag : 1;
    uint16_t control_command_flag : 1;
    uint16_t reserved : 2;
    uint16_t spacecraft_id : 10;
    uint16_t virtual_channel_id : 6;
    uint16_t frame_length : 10;
    uint8_t frame_sequence_number;
} sdlp_tc_header_t;

typedef struct
{
    sdlp_tc_header_t header;
#ifdef TC_SEGMENT_HEADER_ENABLED
    sdlp_tc_segment_header_t segment_header;
#endif
    uint8_t data[TC_MAX_DATA_SIZE];
    uint16_t data_length;
    uint16_t fecf;
} sdlp_tc_frame_t;

int sdlp_tc_create_frame(sdlp_tc_frame_t *frame,
                         uint16_t spacecraft_id,
                         uint8_t virtual_channel_id,
                         uint8_t frame_seq_num,
                         const uint8_t *data,
                         uint16_t data_length);

/* Select the Transfer Frame type by setting the Bypass and Control Command Flags
 * (CCSDS 232.0-B-4, 4.1.2.3). Also recomputes the Frame Length, as Type-BC frames
 * carry no Segment Header (4.1.3.2.2.1.3). sdlp_tc_create_frame produces Type-AD. */
int sdlp_tc_set_frame_type(sdlp_tc_frame_t *frame, sdlp_tc_frame_type_t type);

/* Build a complete Type-BC frame carrying the Unlock Control Command (4.1.3.3.2).
 * The Frame Sequence Number is set to zero (COP does not use it for Type-B frames). */
int sdlp_tc_create_unlock_frame(sdlp_tc_frame_t *frame,
                                uint16_t spacecraft_id,
                                uint8_t virtual_channel_id);

/* Build a complete Type-BC frame carrying the Set V(R) Control Command (4.1.3.3.3).
 * vr is the value the FARM should load into the Receiver_Frame_Sequence_Number. */
int sdlp_tc_create_set_vr_frame(sdlp_tc_frame_t *frame,
                                uint16_t spacecraft_id,
                                uint8_t virtual_channel_id,
                                uint8_t vr);

int sdlp_tc_encode_frame(const sdlp_tc_frame_t *frame,
                         uint8_t *buffer,
                         size_t buffer_size,
                         size_t *encoded_size);

int sdlp_tc_decode_frame(const uint8_t *buffer, size_t buffer_size, sdlp_tc_frame_t *frame);

#ifdef TC_SEGMENT_HEADER_ENABLED
/* Set the segment header fields on a TC frame.
 * Must not be called for frames with control_command_flag set (per CCSDS 232.0-B-4 4.1.3.2.2.1.3).
 * sequence_flags: one of the sdlp_tc_seq_flag_t values.
 * map_id: Multiplexer Access Point Identifier (0-63). */
int sdlp_tc_set_segment_header(sdlp_tc_frame_t *frame,
                               sdlp_tc_seq_flag_t sequence_flags,
                               uint8_t map_id);
#endif

#endif
