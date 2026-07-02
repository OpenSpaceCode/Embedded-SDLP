#include "sdlp_tm.h"
#include <string.h>

/* Master Channel Frame Count and Virtual Channel Frame Count are maintained per
 * Master Channel (identified by Spacecraft ID, as the Transfer Frame Version Number
 * is fixed at 0) and, within each Master Channel, per Virtual Channel
 * (CCSDS 132.0-B-3, 4.1.2.5 and 4.1.2.6). Both counts are free-running modulo-256.
 *
 * State lives in a fixed-size table (no dynamic allocation). Up to
 * TM_MAX_MASTER_CHANNELS distinct Master Channels are tracked concurrently; frames
 * for any further Master Channels are emitted with zeroed counts. Not thread-safe:
 * the caller must serialize calls. */
#ifndef TM_MAX_MASTER_CHANNELS
#define TM_MAX_MASTER_CHANNELS 8
#endif

#define TM_VC_PER_MC 8 /* the TM Virtual Channel Identifier is 3 bits */

typedef struct {
    uint8_t in_use;
    uint16_t spacecraft_id;
    uint8_t mc_frame_count;
    uint8_t vc_frame_count[TM_VC_PER_MC];
} tm_master_channel_t;

static tm_master_channel_t tm_master_channels[TM_MAX_MASTER_CHANNELS];

/* Return the counter state for a Master Channel, allocating a slot on first use.
 * Returns NULL if the table is already full of other Master Channels. */
static tm_master_channel_t *tm_get_master_channel(uint16_t spacecraft_id) {
    for (size_t i = 0; i < TM_MAX_MASTER_CHANNELS; i++) {
        if (tm_master_channels[i].in_use && tm_master_channels[i].spacecraft_id == spacecraft_id) {
            return &tm_master_channels[i];
        }
    }
    for (size_t i = 0; i < TM_MAX_MASTER_CHANNELS; i++) {
        if (!tm_master_channels[i].in_use) {
            tm_master_channels[i].in_use = 1;
            tm_master_channels[i].spacecraft_id = spacecraft_id;
            tm_master_channels[i].mc_frame_count = 0;
            memset(tm_master_channels[i].vc_frame_count, 0,
                   sizeof(tm_master_channels[i].vc_frame_count));
            return &tm_master_channels[i];
        }
    }
    return NULL;
}

uint16_t sdlp_tm_pack_data_field_status(const sdlp_tm_data_field_status_t *status) {
    if (!status) {
        return 0;
    }
    return (uint16_t)(((uint16_t)(status->secondary_header_flag & 0x01u) << 15) |
                      ((uint16_t)(status->sync_flag & 0x01u) << 14) |
                      ((uint16_t)(status->packet_order_flag & 0x01u) << 13) |
                      ((uint16_t)(status->segment_length_id & 0x03u) << 11) |
                      ((uint16_t)(status->first_header_pointer & 0x07ffu)));
}

void sdlp_tm_unpack_data_field_status(uint16_t raw, sdlp_tm_data_field_status_t *status) {
    if (!status) {
        return;
    }
    status->secondary_header_flag = (uint8_t)((raw >> 15) & 0x01u);
    status->sync_flag = (uint8_t)((raw >> 14) & 0x01u);
    status->packet_order_flag = (uint8_t)((raw >> 13) & 0x01u);
    status->segment_length_id = (uint8_t)((raw >> 11) & 0x03u);
    status->first_header_pointer = (uint16_t)(raw & 0x07ffu);
}

int sdlp_tm_create_frame(sdlp_tm_frame_t *frame, uint16_t spacecraft_id, 
                          uint8_t virtual_channel_id, const uint8_t *data, 
                          uint16_t data_length) {
    if (!frame || !data || data_length > TM_MAX_DATA_SIZE) {
        return SDLP_ERROR_INVALID_PARAM;
    }

    memset(frame, 0, sizeof(sdlp_tm_frame_t));

    uint16_t scid = (uint16_t)(spacecraft_id & 0x3ffu);
    uint8_t vcid = (uint8_t)(virtual_channel_id & 0x07u);
    tm_master_channel_t *mc = tm_get_master_channel(scid);

    frame->header.transfer_frame_version = SDLP_VERSION;
    frame->header.spacecraft_id = scid;
    frame->header.virtual_channel_id = vcid;
    frame->header.ocf_flag = 0;
    if (mc != NULL) {
        frame->header.master_channel_frame_count = mc->mc_frame_count++;
        frame->header.virtual_channel_frame_count = mc->vc_frame_count[vcid]++;
    } else {
        frame->header.master_channel_frame_count = 0;
        frame->header.virtual_channel_frame_count = 0;
    }

    /* Default to a valid "Packets, no segmentation" Data Field Status: Sync Flag = 0
     * requires the Segment Length Identifier to be '11' (CCSDS 132.0-B-3, 4.1.2.7.5.2),
     * and the First Header Pointer marks a Packet starting at the first data octet. */
    frame->header.transfer_frame_data_field_status.secondary_header_flag = 0;
    frame->header.transfer_frame_data_field_status.sync_flag = 0;
    frame->header.transfer_frame_data_field_status.packet_order_flag = 0;
    frame->header.transfer_frame_data_field_status.segment_length_id =
        TM_SEGMENT_LENGTH_ID_NO_SEGMENTATION;
    frame->header.transfer_frame_data_field_status.first_header_pointer = 0;

    memcpy(frame->data, data, data_length);
    frame->data_length = data_length;

    return SDLP_SUCCESS;
}

int sdlp_tm_set_secondary_header(sdlp_tm_frame_t *frame, const uint8_t *data, uint8_t length) {
    if (!frame || !data || length == 0u || length > TM_SECONDARY_HEADER_MAX_DATA) {
        return SDLP_ERROR_INVALID_PARAM;
    }

    frame->header.transfer_frame_data_field_status.secondary_header_flag = 1;
    frame->secondary_header.version = 0; /* CCSDS 132.0-B-3, 4.1.3.2.2.2 */
    frame->secondary_header.length = length;
    memcpy(frame->secondary_header.data, data, length);

    return SDLP_SUCCESS;
}

int sdlp_tm_set_ocf(sdlp_tm_frame_t *frame, const uint8_t ocf[TM_OCF_SIZE]) {
    if (!frame || !ocf) {
        return SDLP_ERROR_INVALID_PARAM;
    }

    frame->header.ocf_flag = 1;
    memcpy(frame->ocf, ocf, TM_OCF_SIZE);

    return SDLP_SUCCESS;
}

int sdlp_tm_encode_frame(const sdlp_tm_frame_t *frame, uint8_t *buffer,
                          size_t buffer_size, size_t *encoded_size) {
    if (!frame || !buffer || !encoded_size) {
        return SDLP_ERROR_INVALID_PARAM;
    }
    
    int secondary_header_present =
        frame->header.transfer_frame_data_field_status.secondary_header_flag ? 1 : 0;
    int ocf_present = frame->header.ocf_flag ? 1 : 0;

    size_t required_size = TM_PRIMARY_HEADER_SIZE + frame->data_length +
                           TM_FRAME_ERROR_CONTROL_SIZE;

    if (secondary_header_present) {
        required_size += TM_SECONDARY_HEADER_ID_SIZE + frame->secondary_header.length;
    }
    if (ocf_present) {
        required_size += TM_OCF_SIZE;
    }

    if (buffer_size < required_size) {
        return SDLP_ERROR_BUFFER_TOO_SMALL;
    }
    
    size_t offset = 0;
    
    buffer[offset++] = (uint8_t)((frame->header.transfer_frame_version << 6) | 
                       ((frame->header.spacecraft_id >> 4) & 0x3fu));
    buffer[offset++] = (uint8_t)(((frame->header.spacecraft_id & 0x0fu) << 4) | 
                       ((frame->header.virtual_channel_id & 0x07u) << 1) | 
                       (frame->header.ocf_flag & 0x01u));
    buffer[offset++] = frame->header.master_channel_frame_count;
    buffer[offset++] = frame->header.virtual_channel_frame_count;
    
    uint16_t data_field_status =
        sdlp_tm_pack_data_field_status(&frame->header.transfer_frame_data_field_status);
    buffer[offset++] = (uint8_t)((data_field_status >> 8) & 0xffu);
    buffer[offset++] = (uint8_t)(data_field_status & 0xffu);

    /* Transfer Frame Secondary Header (CCSDS 132.0-B-3, 4.1.3): Identification Field
     * (Version '00' | Length = total size - 1 = Data Field length) then the Data Field. */
    if (secondary_header_present) {
        buffer[offset++] = (uint8_t)(((frame->secondary_header.version & 0x03u) << 6) |
                           (frame->secondary_header.length & 0x3fu));
        memcpy(&buffer[offset], frame->secondary_header.data, frame->secondary_header.length);
        offset += frame->secondary_header.length;
    }

    memcpy(&buffer[offset], frame->data, frame->data_length);
    offset += frame->data_length;

    /* Operational Control Field (CCSDS 132.0-B-3, 4.1.5): four octets following the
     * Data Field, present when the OCF Flag is set. The content is caller-supplied. */
    if (ocf_present) {
        memcpy(&buffer[offset], frame->ocf, TM_OCF_SIZE);
        offset += TM_OCF_SIZE;
    }

    /* The Frame Error Control Field is passed through verbatim; computing an
     * error-control value (e.g. CRC-16) is left to the application. */
    buffer[offset++] = (uint8_t)((frame->fecf >> 8) & 0xffu);
    buffer[offset++] = (uint8_t)(frame->fecf & 0xffu);

    *encoded_size = offset;
    
    return SDLP_SUCCESS;
}

int sdlp_tm_decode_frame(const uint8_t *buffer, size_t buffer_size, 
                          sdlp_tm_frame_t *frame) {
    if (!buffer || !frame || buffer_size < TM_PRIMARY_HEADER_SIZE + TM_FRAME_ERROR_CONTROL_SIZE) {
        return SDLP_ERROR_INVALID_PARAM;
    }
    
    memset(frame, 0, sizeof(sdlp_tm_frame_t));
    
    size_t offset = 0;
    
    frame->header.transfer_frame_version = (uint8_t)((buffer[offset] >> 6) & 0x03u);
    frame->header.spacecraft_id = (uint16_t)(((buffer[offset] & 0x3fu) << 4) | ((buffer[offset + 1] >> 4) & 0x0fu));
    offset++;
    
    frame->header.virtual_channel_id = (uint8_t)((buffer[offset] >> 1) & 0x07u);
    frame->header.ocf_flag = (uint8_t)(buffer[offset] & 0x01u);
    offset++;
    
    frame->header.master_channel_frame_count = buffer[offset++];
    frame->header.virtual_channel_frame_count = buffer[offset++];
    
    uint16_t data_field_status = (uint16_t)(((uint16_t)buffer[offset] << 8) | buffer[offset + 1]);
    sdlp_tm_unpack_data_field_status(data_field_status,
                                     &frame->header.transfer_frame_data_field_status);
    offset += 2;

    /* Transfer Frame Secondary Header (CCSDS 132.0-B-3, 4.1.3), present when the
     * Secondary Header Flag is set. Its size is signaled in the Identification Field. */
    size_t secondary_header_size = 0;
    if (frame->header.transfer_frame_data_field_status.secondary_header_flag) {
        uint8_t sh_length;

        /* Need the Identification Field plus at least one Data Field octet (4.1.3.1.3). */
        if (buffer_size < TM_PRIMARY_HEADER_SIZE + TM_SECONDARY_HEADER_ID_SIZE + 1u +
                          TM_FRAME_ERROR_CONTROL_SIZE) {
            return SDLP_ERROR_INVALID_FRAME;
        }

        frame->secondary_header.version = (uint8_t)((buffer[offset] >> 6) & 0x03u);
        sh_length = (uint8_t)(buffer[offset] & 0x3fu); /* total size - 1 = Data Field length */
        if (sh_length == 0u) {
            return SDLP_ERROR_INVALID_FRAME;
        }
        secondary_header_size = TM_SECONDARY_HEADER_ID_SIZE + sh_length;

        if (buffer_size < TM_PRIMARY_HEADER_SIZE + secondary_header_size +
                          TM_FRAME_ERROR_CONTROL_SIZE) {
            return SDLP_ERROR_INVALID_FRAME;
        }

        frame->secondary_header.length = sh_length;
        memcpy(frame->secondary_header.data, &buffer[offset + TM_SECONDARY_HEADER_ID_SIZE], sh_length);
        offset += secondary_header_size;
    }

    /* Operational Control Field (CCSDS 132.0-B-3, 4.1.5): four octets between the
     * Data Field and the Frame Error Control Field, present when the OCF Flag is set. */
    size_t ocf_size = frame->header.ocf_flag ? TM_OCF_SIZE : 0;

    size_t overhead = TM_PRIMARY_HEADER_SIZE + secondary_header_size + ocf_size +
                      TM_FRAME_ERROR_CONTROL_SIZE;
    if (buffer_size < overhead) {
        return SDLP_ERROR_INVALID_FRAME;
    }

    frame->data_length = (uint16_t)(buffer_size - overhead);

    if (frame->data_length > TM_MAX_DATA_SIZE) {
        return SDLP_ERROR_INVALID_FRAME;
    }

    memcpy(frame->data, &buffer[offset], frame->data_length);
    offset += frame->data_length;

    if (frame->header.ocf_flag) {
        memcpy(frame->ocf, &buffer[offset], TM_OCF_SIZE);
        offset += TM_OCF_SIZE;
    }

    /* The Frame Error Control Field is surfaced as-is; validating it (e.g. via
     * CRC-16) is left to the application. */
    frame->fecf = (uint16_t)(((uint16_t)buffer[offset] << 8) | buffer[offset + 1]);

    return SDLP_SUCCESS;
}
