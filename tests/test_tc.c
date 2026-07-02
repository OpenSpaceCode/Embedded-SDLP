#include "cunit.h"
#include "sdlp_common.h"
#include "sdlp_tc.h"
#include "test_runners.h"

#include <stddef.h>
#include <stdint.h>

static int test_tc_create_frame_invalid_params(void)
{
    sdlp_tc_frame_t frame;
    uint8_t payload[1] = {0x55};

    ASSERT_EQ_INT(SDLP_ERROR_INVALID_PARAM, sdlp_tc_create_frame(NULL, 1, 1, 1, payload, 1));
    ASSERT_EQ_INT(SDLP_ERROR_INVALID_PARAM, sdlp_tc_create_frame(&frame, 1, 1, 1, NULL, 1));
    ASSERT_EQ_INT(SDLP_ERROR_INVALID_PARAM,
                  sdlp_tc_create_frame(&frame, 1, 1, 1, payload, TC_MAX_DATA_SIZE + 1));
    ASSERT_EQ_INT(SDLP_ERROR_INVALID_PARAM, sdlp_tc_create_frame(&frame, 1, 1, 1, payload, 0));

    return 0;
}

static int test_tc_encode_decode_roundtrip(void)
{
    sdlp_tc_frame_t frame;
    sdlp_tc_frame_t decoded;
    const uint8_t payload[] = {0x01, 0x23, 0x45, 0x67};
    uint8_t encoded[TC_PRIMARY_HEADER_SIZE + TC_MAX_DATA_SIZE + TC_FRAME_ERROR_CONTROL_SIZE];
    size_t encoded_size = 0;

    ASSERT_EQ_INT(
        SDLP_SUCCESS,
        sdlp_tc_create_frame(&frame, 0x7FF, 0x7F, 0x9A, payload, (uint16_t)sizeof(payload)));
    ASSERT_EQ_INT(SDLP_SUCCESS,
                  sdlp_tc_encode_frame(&frame, encoded, sizeof(encoded), &encoded_size));

    ASSERT_EQ_INT(TC_PRIMARY_HEADER_SIZE + (int)sizeof(payload) + TC_FRAME_ERROR_CONTROL_SIZE,
                  (int)encoded_size);

    ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tc_decode_frame(encoded, encoded_size, &decoded));
    ASSERT_EQ_INT(SDLP_VERSION, decoded.header.transfer_frame_version);
    ASSERT_EQ_INT((int)(0x7FF & 0x3FF), decoded.header.spacecraft_id);
    ASSERT_EQ_INT((int)(0x7F & 0x3F), decoded.header.virtual_channel_id);
    ASSERT_EQ_INT(0x9A, decoded.header.frame_sequence_number);
    /* Frame Length = total octets in the frame - 1 (CCSDS 232.0-B-4, 4.1.2.7.2). */
    ASSERT_EQ_INT(TC_PRIMARY_HEADER_SIZE + (int)sizeof(payload) + TC_FRAME_ERROR_CONTROL_SIZE - 1,
                  decoded.header.frame_length);
    ASSERT_EQ_INT((int)sizeof(payload), decoded.data_length);
    ASSERT_EQ_MEM(payload, decoded.data, sizeof(payload));

    return 0;
}

static int test_tc_encode_buffer_too_small(void)
{
    sdlp_tc_frame_t frame;
    const uint8_t payload[] = {0xAB, 0xCD, 0xEF};
    uint8_t encoded[TC_PRIMARY_HEADER_SIZE + 2 + TC_FRAME_ERROR_CONTROL_SIZE];
    size_t encoded_size = 0;

    ASSERT_EQ_INT(SDLP_SUCCESS,
                  sdlp_tc_create_frame(&frame, 5, 3, 7, payload, (uint16_t)sizeof(payload)));
    ASSERT_EQ_INT(SDLP_ERROR_BUFFER_TOO_SMALL,
                  sdlp_tc_encode_frame(&frame, encoded, sizeof(encoded), &encoded_size));

    return 0;
}

static int test_tc_fecf_passthrough(void)
{
    sdlp_tc_frame_t frame;
    sdlp_tc_frame_t decoded;
    const uint8_t payload[] = {0x11, 0x22, 0x33, 0x44};
    uint8_t encoded[TC_PRIMARY_HEADER_SIZE + TC_MAX_DATA_SIZE + TC_FRAME_ERROR_CONTROL_SIZE];
    size_t encoded_size = 0;

    ASSERT_EQ_INT(
        SDLP_SUCCESS,
        sdlp_tc_create_frame(&frame, 0x12, 0x05, 0x42, payload, (uint16_t)sizeof(payload)));
    frame.fecf = 0x1234;
    ASSERT_EQ_INT(SDLP_SUCCESS,
                  sdlp_tc_encode_frame(&frame, encoded, sizeof(encoded), &encoded_size));

    /* The FECF is serialized verbatim (big-endian) in the trailing two bytes. */
    ASSERT_EQ_INT(0x12, encoded[encoded_size - 2]);
    ASSERT_EQ_INT(0x34, encoded[encoded_size - 1]);

    /* Decode no longer validates the FECF: it always succeeds and surfaces the
     * field as-is. */
    ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tc_decode_frame(encoded, encoded_size, &decoded));
    ASSERT_EQ_INT(0x1234, decoded.fecf);

    return 0;
}

static int test_tc_decode_invalid_frame_length(void)
{
    sdlp_tc_frame_t frame;
    sdlp_tc_frame_t decoded;
    const uint8_t payload[] = {0x01, 0x02, 0x03, 0x04};
    uint8_t encoded[TC_PRIMARY_HEADER_SIZE + TC_MAX_DATA_SIZE + TC_FRAME_ERROR_CONTROL_SIZE];
    size_t encoded_size = 0;

    ASSERT_EQ_INT(
        SDLP_SUCCESS,
        sdlp_tc_create_frame(&frame, 0x21, 0x06, 0x07, payload, (uint16_t)sizeof(payload)));
    ASSERT_EQ_INT(SDLP_SUCCESS,
                  sdlp_tc_encode_frame(&frame, encoded, sizeof(encoded), &encoded_size));

    /* Corrupt the Frame Length low byte so it no longer matches the octet count. */
    encoded[3] ^= 0x01;

    ASSERT_EQ_INT(SDLP_ERROR_INVALID_FRAME, sdlp_tc_decode_frame(encoded, encoded_size, &decoded));

    return 0;
}

static int test_tc_frame_type_bd_roundtrip(void)
{
    sdlp_tc_frame_t frame;
    sdlp_tc_frame_t decoded;
    const uint8_t payload[] = {0x5A, 0xA5};
    uint8_t encoded[TC_PRIMARY_HEADER_SIZE + TC_MAX_DATA_SIZE + TC_FRAME_ERROR_CONTROL_SIZE];
    size_t encoded_size = 0;

    ASSERT_EQ_INT(
        SDLP_SUCCESS,
        sdlp_tc_create_frame(&frame, 0x44, 0x02, 0x10, payload, (uint16_t)sizeof(payload)));
    /* Default is Type-AD. */
    ASSERT_EQ_INT(0, frame.header.bypass_flag);
    ASSERT_EQ_INT(0, frame.header.control_command_flag);

    ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tc_set_frame_type(&frame, SDLP_TC_FRAME_TYPE_BD));
    ASSERT_EQ_INT(1, frame.header.bypass_flag);
    ASSERT_EQ_INT(0, frame.header.control_command_flag);

    ASSERT_EQ_INT(SDLP_SUCCESS,
                  sdlp_tc_encode_frame(&frame, encoded, sizeof(encoded), &encoded_size));
    ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tc_decode_frame(encoded, encoded_size, &decoded));
    ASSERT_EQ_INT(1, decoded.header.bypass_flag);
    ASSERT_EQ_INT(0, decoded.header.control_command_flag);
    ASSERT_EQ_MEM(payload, decoded.data, sizeof(payload));

    return 0;
}

static int test_tc_set_frame_type_invalid(void)
{
    sdlp_tc_frame_t frame;
    const uint8_t payload[1] = {0x01};

    ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tc_create_frame(&frame, 1, 1, 1, payload, 1));
    ASSERT_EQ_INT(SDLP_ERROR_INVALID_PARAM, sdlp_tc_set_frame_type(NULL, SDLP_TC_FRAME_TYPE_BD));
    ASSERT_EQ_INT(SDLP_ERROR_INVALID_PARAM,
                  sdlp_tc_set_frame_type(&frame, (sdlp_tc_frame_type_t)99));

    return 0;
}

static int test_tc_unlock_command(void)
{
    sdlp_tc_frame_t frame;
    sdlp_tc_frame_t decoded;
    uint8_t encoded[TC_PRIMARY_HEADER_SIZE + TC_MAX_DATA_SIZE + TC_FRAME_ERROR_CONTROL_SIZE];
    size_t encoded_size = 0;

    ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tc_create_unlock_frame(&frame, 0x30, 0x01));
    /* Type-BC: Bypass=1, Control Command=1 (table 4-1). */
    ASSERT_EQ_INT(1, frame.header.bypass_flag);
    ASSERT_EQ_INT(1, frame.header.control_command_flag);
    /* Frame Sequence Number is 'all zeroes' for Type-B frames. */
    ASSERT_EQ_INT(0, frame.header.frame_sequence_number);
    /* Unlock: a single 'all zeroes' octet (4.1.3.3.2). */
    ASSERT_EQ_INT(TC_CONTROL_CMD_UNLOCK_LENGTH, frame.data_length);
    ASSERT_EQ_INT(TC_CONTROL_CMD_UNLOCK, frame.data[0]);

    ASSERT_EQ_INT(SDLP_SUCCESS,
                  sdlp_tc_encode_frame(&frame, encoded, sizeof(encoded), &encoded_size));
    /* Type-BC frames never carry a Segment Header (4.1.3.2.2.1.3). */
    ASSERT_EQ_INT(TC_PRIMARY_HEADER_SIZE + (int)TC_CONTROL_CMD_UNLOCK_LENGTH +
                      TC_FRAME_ERROR_CONTROL_SIZE,
                  (int)encoded_size);

    ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tc_decode_frame(encoded, encoded_size, &decoded));
    ASSERT_EQ_INT(1, decoded.header.bypass_flag);
    ASSERT_EQ_INT(1, decoded.header.control_command_flag);
    ASSERT_EQ_INT(TC_CONTROL_CMD_UNLOCK_LENGTH, decoded.data_length);
    ASSERT_EQ_INT(TC_CONTROL_CMD_UNLOCK, decoded.data[0]);

    return 0;
}

static int test_tc_set_vr_command(void)
{
    sdlp_tc_frame_t frame;
    sdlp_tc_frame_t decoded;
    uint8_t encoded[TC_PRIMARY_HEADER_SIZE + TC_MAX_DATA_SIZE + TC_FRAME_ERROR_CONTROL_SIZE];
    size_t encoded_size = 0;

    ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tc_create_set_vr_frame(&frame, 0x30, 0x01, 0x7C));
    ASSERT_EQ_INT(1, frame.header.bypass_flag);
    ASSERT_EQ_INT(1, frame.header.control_command_flag);
    ASSERT_EQ_INT(0, frame.header.frame_sequence_number);
    /* Set V(R): '10000010 00000000 XXXXXXXX' (4.1.3.3.3). */
    ASSERT_EQ_INT(TC_CONTROL_CMD_SET_VR_LENGTH, frame.data_length);
    ASSERT_EQ_INT(TC_CONTROL_CMD_SET_VR_OCTET0, frame.data[0]);
    ASSERT_EQ_INT(TC_CONTROL_CMD_SET_VR_OCTET1, frame.data[1]);
    ASSERT_EQ_INT(0x7C, frame.data[2]);

    ASSERT_EQ_INT(SDLP_SUCCESS,
                  sdlp_tc_encode_frame(&frame, encoded, sizeof(encoded), &encoded_size));
    ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tc_decode_frame(encoded, encoded_size, &decoded));
    ASSERT_EQ_INT(TC_CONTROL_CMD_SET_VR_LENGTH, decoded.data_length);
    ASSERT_EQ_INT(0x7C, decoded.data[2]);

    return 0;
}

static int test_tc_decode_reserved_frame_type(void)
{
    sdlp_tc_frame_t frame;
    sdlp_tc_frame_t decoded;
    const uint8_t payload[] = {0x01, 0x02};
    uint8_t encoded[TC_PRIMARY_HEADER_SIZE + TC_MAX_DATA_SIZE + TC_FRAME_ERROR_CONTROL_SIZE];
    size_t encoded_size = 0;

    ASSERT_EQ_INT(
        SDLP_SUCCESS,
        sdlp_tc_create_frame(&frame, 0x11, 0x01, 0x01, payload, (uint16_t)sizeof(payload)));
    ASSERT_EQ_INT(SDLP_SUCCESS,
                  sdlp_tc_encode_frame(&frame, encoded, sizeof(encoded), &encoded_size));

    /* Force Bypass=0, Control Command=1: reserved for future application (table 4-1). */
    encoded[0] = (uint8_t)((encoded[0] & ~0x30u) | 0x10u);

    ASSERT_EQ_INT(SDLP_ERROR_INVALID_FRAME, sdlp_tc_decode_frame(encoded, encoded_size, &decoded));

    return 0;
}

test_result_t test_tc_run_all(void)
{
    test_result_t result;

    RUN_TEST(test_tc_create_frame_invalid_params);
    RUN_TEST(test_tc_encode_decode_roundtrip);
    RUN_TEST(test_tc_encode_buffer_too_small);
    RUN_TEST(test_tc_fecf_passthrough);
    RUN_TEST(test_tc_decode_invalid_frame_length);
    RUN_TEST(test_tc_frame_type_bd_roundtrip);
    RUN_TEST(test_tc_set_frame_type_invalid);
    RUN_TEST(test_tc_unlock_command);
    RUN_TEST(test_tc_set_vr_command);
    RUN_TEST(test_tc_decode_reserved_frame_type);

    /* cunit's counters have internal linkage, so this translation unit tallies
     * only its own tests. */
    result.total = cunit_total_tests;
    result.passed = cunit_total_tests - cunit_overall_failures;
    return result;
}
