#include "cunit.h"
#include "test_runners.h"

#include <stddef.h>
#include <stdint.h>

#include "sdlp_common.h"
#include "sdlp_tm.h"

static int test_tm_create_frame_invalid_params(void) {
	sdlp_tm_frame_t frame;
	uint8_t payload[1] = {0xAA};

	ASSERT_EQ_INT(SDLP_ERROR_INVALID_PARAM,
								sdlp_tm_create_frame(NULL, 1, 1, payload, 1));
	ASSERT_EQ_INT(SDLP_ERROR_INVALID_PARAM,
								sdlp_tm_create_frame(&frame, 1, 1, NULL, 1));
	ASSERT_EQ_INT(SDLP_ERROR_INVALID_PARAM,
								sdlp_tm_create_frame(&frame, 1, 1, payload, TM_MAX_DATA_SIZE + 1));

	return 0;
}

static int test_tm_encode_decode_roundtrip(void) {
	sdlp_tm_frame_t frame;
	sdlp_tm_frame_t decoded;
	const uint8_t payload[] = {0x10, 0x20, 0x30, 0x40, 0x50};
	uint8_t encoded[TM_PRIMARY_HEADER_SIZE + TM_MAX_DATA_SIZE + TM_FRAME_ERROR_CONTROL_SIZE];
	size_t encoded_size = 0;

	ASSERT_EQ_INT(SDLP_SUCCESS,
								sdlp_tm_create_frame(&frame, 0x7FF, 0x1F, payload, (uint16_t)sizeof(payload)));
	ASSERT_EQ_INT(SDLP_SUCCESS,
								sdlp_tm_encode_frame(&frame, encoded, sizeof(encoded), &encoded_size));

	ASSERT_EQ_INT(TM_PRIMARY_HEADER_SIZE + (int)sizeof(payload) + TM_FRAME_ERROR_CONTROL_SIZE,
								(int)encoded_size);

	ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tm_decode_frame(encoded, encoded_size, &decoded));
	ASSERT_EQ_INT(SDLP_VERSION, decoded.header.transfer_frame_version);
	ASSERT_EQ_INT((int)(0x7FF & 0x3FF), decoded.header.spacecraft_id);
	ASSERT_EQ_INT((int)(0x1F & 0x07), decoded.header.virtual_channel_id);
	ASSERT_EQ_INT(frame.header.master_channel_frame_count,
								decoded.header.master_channel_frame_count);
	ASSERT_EQ_INT((int)sizeof(payload), decoded.data_length);
	ASSERT_EQ_MEM(payload, decoded.data, sizeof(payload));
	/* The default Data Field Status round-trips: Sync Flag = 0 with a '11' Segment
	 * Length Identifier (CCSDS 132.0-B-3, 4.1.2.7.5.2). */
	ASSERT_EQ_INT(0, decoded.header.transfer_frame_data_field_status.sync_flag);
	ASSERT_EQ_INT(TM_SEGMENT_LENGTH_ID_NO_SEGMENTATION,
								decoded.header.transfer_frame_data_field_status.segment_length_id);

	return 0;
}

static int test_tm_data_field_status_codec(void) {
	sdlp_tm_data_field_status_t status = {0};
	sdlp_tm_data_field_status_t parsed = {0};
	uint16_t raw;

	status.secondary_header_flag = 1;
	status.sync_flag = 0;
	status.packet_order_flag = 0;
	status.segment_length_id = TM_SEGMENT_LENGTH_ID_NO_SEGMENTATION;
	status.first_header_pointer = 0x123;

	/* MSB-first layout: shf<<15 | slid<<11 | fhp = 0x8000 | 0x1800 | 0x123. */
	raw = sdlp_tm_pack_data_field_status(&status);
	ASSERT_EQ_INT(0x9923, raw);

	sdlp_tm_unpack_data_field_status(raw, &parsed);
	ASSERT_EQ_INT(1, parsed.secondary_header_flag);
	ASSERT_EQ_INT(0, parsed.sync_flag);
	ASSERT_EQ_INT(0, parsed.packet_order_flag);
	ASSERT_EQ_INT(TM_SEGMENT_LENGTH_ID_NO_SEGMENTATION, parsed.segment_length_id);
	ASSERT_EQ_INT(0x123, parsed.first_header_pointer);

	return 0;
}

static int test_tm_encode_buffer_too_small(void) {
	sdlp_tm_frame_t frame;
	const uint8_t payload[] = {0x01, 0x02, 0x03};
	uint8_t encoded[TM_PRIMARY_HEADER_SIZE + 2 + TM_FRAME_ERROR_CONTROL_SIZE];
	size_t encoded_size = 0;

	ASSERT_EQ_INT(SDLP_SUCCESS,
								sdlp_tm_create_frame(&frame, 2, 1, payload, (uint16_t)sizeof(payload)));
	ASSERT_EQ_INT(SDLP_ERROR_BUFFER_TOO_SMALL,
								sdlp_tm_encode_frame(&frame, encoded, sizeof(encoded), &encoded_size));

	return 0;
}

static int test_tm_fecf_passthrough(void) {
	sdlp_tm_frame_t frame;
	sdlp_tm_frame_t decoded;
	const uint8_t payload[] = {0xDE, 0xAD, 0xBE, 0xEF};
	uint8_t encoded[TM_PRIMARY_HEADER_SIZE + TM_MAX_DATA_SIZE + TM_FRAME_ERROR_CONTROL_SIZE];
	size_t encoded_size = 0;

	ASSERT_EQ_INT(SDLP_SUCCESS,
								sdlp_tm_create_frame(&frame, 3, 2, payload, (uint16_t)sizeof(payload)));
	frame.fecf = 0xABCD;
	ASSERT_EQ_INT(SDLP_SUCCESS,
								sdlp_tm_encode_frame(&frame, encoded, sizeof(encoded), &encoded_size));

	/* The FECF is serialized verbatim (big-endian) in the trailing two bytes. */
	ASSERT_EQ_INT(0xAB, encoded[encoded_size - 2]);
	ASSERT_EQ_INT(0xCD, encoded[encoded_size - 1]);

	/* Decode no longer validates the FECF: it always succeeds and surfaces the
	 * field as-is. */
	ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tm_decode_frame(encoded, encoded_size, &decoded));
	ASSERT_EQ_INT(0xABCD, decoded.fecf);

	return 0;
}

static int test_tm_frame_counts_per_channel(void) {
	sdlp_tm_frame_t f;
	const uint8_t payload[1] = {0xA5};
	const uint16_t scid_a = 0x055; /* SCIDs not used by other tests => fresh counters */
	const uint16_t scid_b = 0x056;

	/* First frame on (SCID A, VC 0): both counts start at 0. */
	ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tm_create_frame(&f, scid_a, 0, payload, 1));
	ASSERT_EQ_INT(0, f.header.master_channel_frame_count);
	ASSERT_EQ_INT(0, f.header.virtual_channel_frame_count);

	/* A different VC of the same Master Channel: the MC count advances, the new VC
	 * count is independent and starts at 0. */
	ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tm_create_frame(&f, scid_a, 1, payload, 1));
	ASSERT_EQ_INT(1, f.header.master_channel_frame_count);
	ASSERT_EQ_INT(0, f.header.virtual_channel_frame_count);

	/* Back to VC 0: the MC count keeps advancing, VC 0 resumes its own sequence. */
	ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tm_create_frame(&f, scid_a, 0, payload, 1));
	ASSERT_EQ_INT(2, f.header.master_channel_frame_count);
	ASSERT_EQ_INT(1, f.header.virtual_channel_frame_count);

	/* A different Master Channel keeps entirely separate counts. */
	ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tm_create_frame(&f, scid_b, 0, payload, 1));
	ASSERT_EQ_INT(0, f.header.master_channel_frame_count);
	ASSERT_EQ_INT(0, f.header.virtual_channel_frame_count);

	return 0;
}

static int test_tm_secondary_header_roundtrip(void) {
	sdlp_tm_frame_t frame;
	sdlp_tm_frame_t decoded;
	const uint8_t payload[] = {0x10, 0x20, 0x30};
	const uint8_t sh_data[] = {0xAA, 0xBB, 0xCC, 0xDD};
	uint8_t encoded[TM_PRIMARY_HEADER_SIZE + TM_SECONDARY_HEADER_ID_SIZE +
	                TM_SECONDARY_HEADER_MAX_DATA + TM_MAX_DATA_SIZE + TM_FRAME_ERROR_CONTROL_SIZE];
	size_t encoded_size = 0;

	ASSERT_EQ_INT(SDLP_SUCCESS,
								sdlp_tm_create_frame(&frame, 0x100, 1, payload, (uint16_t)sizeof(payload)));
	ASSERT_EQ_INT(SDLP_SUCCESS,
								sdlp_tm_set_secondary_header(&frame, sh_data, (uint8_t)sizeof(sh_data)));
	ASSERT_EQ_INT(1, frame.header.transfer_frame_data_field_status.secondary_header_flag);

	ASSERT_EQ_INT(SDLP_SUCCESS,
								sdlp_tm_encode_frame(&frame, encoded, sizeof(encoded), &encoded_size));
	/* primary(6) + ID(1) + secondary data(4) + payload(3) + FECF(2) */
	ASSERT_EQ_INT(TM_PRIMARY_HEADER_SIZE + TM_SECONDARY_HEADER_ID_SIZE + (int)sizeof(sh_data) +
								(int)sizeof(payload) + TM_FRAME_ERROR_CONTROL_SIZE,
								(int)encoded_size);

	ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tm_decode_frame(encoded, encoded_size, &decoded));
	ASSERT_EQ_INT(1, decoded.header.transfer_frame_data_field_status.secondary_header_flag);
	ASSERT_EQ_INT((int)sizeof(sh_data), decoded.secondary_header.length);
	ASSERT_EQ_MEM(sh_data, decoded.secondary_header.data, sizeof(sh_data));
	/* The Data Field must be recovered intact after the Secondary Header. */
	ASSERT_EQ_INT((int)sizeof(payload), decoded.data_length);
	ASSERT_EQ_MEM(payload, decoded.data, sizeof(payload));

	return 0;
}

static int test_tm_set_secondary_header_invalid(void) {
	sdlp_tm_frame_t frame;
	const uint8_t payload[1] = {0x01};
	const uint8_t sh_data[1] = {0xFF};

	ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tm_create_frame(&frame, 1, 0, payload, 1));
	ASSERT_EQ_INT(SDLP_ERROR_INVALID_PARAM, sdlp_tm_set_secondary_header(NULL, sh_data, 1));
	ASSERT_EQ_INT(SDLP_ERROR_INVALID_PARAM, sdlp_tm_set_secondary_header(&frame, NULL, 1));
	ASSERT_EQ_INT(SDLP_ERROR_INVALID_PARAM, sdlp_tm_set_secondary_header(&frame, sh_data, 0));
	ASSERT_EQ_INT(SDLP_ERROR_INVALID_PARAM,
								sdlp_tm_set_secondary_header(&frame, sh_data, TM_SECONDARY_HEADER_MAX_DATA + 1));
	/* A rejected call must leave the Secondary Header Flag clear. */
	ASSERT_EQ_INT(0, frame.header.transfer_frame_data_field_status.secondary_header_flag);

	return 0;
}

test_result_t test_tm_run_all(void) {
	test_result_t result;

	RUN_TEST(test_tm_create_frame_invalid_params);
	RUN_TEST(test_tm_encode_decode_roundtrip);
	RUN_TEST(test_tm_data_field_status_codec);
	RUN_TEST(test_tm_encode_buffer_too_small);
	RUN_TEST(test_tm_fecf_passthrough);
	RUN_TEST(test_tm_frame_counts_per_channel);
	RUN_TEST(test_tm_secondary_header_roundtrip);
	RUN_TEST(test_tm_set_secondary_header_invalid);

	/* cunit's counters have internal linkage, so this translation unit tallies
	 * only its own tests. */
	result.total = cunit_total_tests;
	result.passed = cunit_total_tests - cunit_overall_failures;
	return result;
}
