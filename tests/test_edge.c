/*
 * @file: test_edge.c
 * @brief Misc / edge cases — invalid types, empty fields, extreme values,
 *        single-bit and overlapping fields, full-payload sanity.
 */

#include "ppack.h"
#include "test_fixtures.h"
#include "test_harness.h"
#include <ppack_platform.h>
#include <stddef.h>

TEST_CASE(test_invalid_field_type)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {.field_uint16 = 0x1234};

        const struct ppack_field fields[] = {
            {.type = (enum ppack_type)99,
             .start_bit = 0,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_t, field_uint16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, 64, fields, 1);
        TEST_ASSERT(ret == -PPACK_ERR_NOTFOUND);
}

TEST_CASE(test_unpack_invalid_field_type)
{
        /* Unpack-side mirror of test_invalid_field_type — covers the
         * default switch arm in ppack_unpack. */
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {0};

        const struct ppack_field fields[] = {
            {.type = (enum ppack_type)99,
             .start_bit = 0,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_t, field_uint16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_unpack(&data, payload, 64, fields, 1);
        TEST_ASSERT(ret == -PPACK_ERR_NOTFOUND);
}

TEST_CASE(test_empty_field_list)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {.field_uint16 = 0x1234};

        int ret = ppack_pack(&data, payload, 64, NULL, 0);
        TEST_ASSERT(ret == -PPACK_ERR_INVALARG);
}

TEST_CASE(test_unpack_zero_field_count)
{
        /* Covers the field_count == 0 short-circuit in ppack_unpack
         * (the first half of the OR; the NULL fields half is covered
         * by test_unpack_null_fields). */
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {0};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_UINT16,
             .start_bit = 0,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_t, field_uint16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_unpack(&data, payload, 64, fields, 0);
        TEST_ASSERT(ret == -PPACK_ERR_INVALARG);
}

TEST_CASE(test_small_bit_field)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {.field_uint8 = 0x0F};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_UINT8,
             .start_bit = 0,
             .bit_length = 4,
             .ptr_offset = offsetof(test_struct_t, field_uint8),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, 64, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        test_struct_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, 64, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
        TEST_ASSERT(unpacked.field_uint8 == 0x0F);
}

TEST_CASE(test_6bit_field)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {.field_uint8 = 0x3F};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_UINT8,
             .start_bit = 0,
             .bit_length = 6,
             .ptr_offset = offsetof(test_struct_t, field_uint8),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, 64, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        test_struct_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, 64, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
        TEST_ASSERT(unpacked.field_uint8 == 0x3F);
}

TEST_CASE(test_adjacent_bit_fields)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {
            .field_uint8 = 0x55,
            .field_bits = 0xAA,
        };

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_UINT8,
             .start_bit = 0,
             .bit_length = 8,
             .ptr_offset = offsetof(test_struct_t, field_uint8),
             .behaviour = PPACK_BEHAVIOUR_RAW},
            {.type = PPACK_TYPE_BITS,
             .start_bit = 8,
             .bit_length = 8,
             .ptr_offset = offsetof(test_struct_t, field_bits),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, 64, fields, 2);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        test_struct_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, 64, fields, 2);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
        TEST_ASSERT(unpacked.field_uint8 == 0x55);
        TEST_ASSERT(unpacked.field_bits == 0xAA);
}

TEST_CASE(test_negative_int32)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {.field_int32 = -1};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_INT32,
             .start_bit = 0,
             .bit_length = 32,
             .ptr_offset = offsetof(test_struct_t, field_int32),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, 64, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        test_struct_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, 64, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
        TEST_ASSERT(unpacked.field_int32 == -1);
}

TEST_CASE(test_max_uint16)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {.field_uint16 = 0xFFFF};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_UINT16,
             .start_bit = 0,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_t, field_uint16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, 64, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        test_struct_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, 64, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
        TEST_ASSERT(unpacked.field_uint16 == 0xFFFF);
}

TEST_CASE(test_max_int16)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {.field_int16 = 32767};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_INT16,
             .start_bit = 0,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_t, field_int16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, 64, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        test_struct_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, 64, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
        TEST_ASSERT(unpacked.field_int16 == 32767);
}

TEST_CASE(test_min_int16)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {.field_int16 = -32768};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_INT16,
             .start_bit = 0,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_t, field_int16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, 64, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        test_struct_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, 64, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
        TEST_ASSERT(unpacked.field_int16 == -32768);
}

TEST_CASE(test_zero_values)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {.field_uint16 = 0};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_UINT16,
             .start_bit = 0,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_t, field_uint16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, 64, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        test_struct_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, 64, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
        TEST_ASSERT(unpacked.field_uint16 == 0);
}

TEST_CASE(test_negative_float)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {.field_f32 = -3.14159f};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_F32,
             .start_bit = 0,
             .bit_length = 32,
             .ptr_offset = offsetof(test_struct_t, field_f32),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, 64, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        test_struct_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, 64, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);

        float diff = unpacked.field_f32 - (-3.14159f);
        if (diff < 0.0f) {
                diff = -diff;
        }
        TEST_ASSERT(diff < 0.0001f);
}

TEST_CASE(test_float_zero)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {.field_f32 = 0.0f};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_F32,
             .start_bit = 0,
             .bit_length = 32,
             .ptr_offset = offsetof(test_struct_t, field_f32),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, 64, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        test_struct_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, 64, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
        TEST_ASSERT(unpacked.field_f32 == 0.0f);
}

TEST_CASE(test_1bit_field)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {.field_uint8 = 1};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_UINT8,
             .start_bit = 0,
             .bit_length = 1,
             .ptr_offset = offsetof(test_struct_t, field_uint8),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, 64, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        test_struct_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, 64, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
        TEST_ASSERT(unpacked.field_uint8 == 1);
}

TEST_CASE(test_1bit_field_zero)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {.field_uint8 = 0};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_UINT8,
             .start_bit = 5,
             .bit_length = 1,
             .ptr_offset = offsetof(test_struct_t, field_uint8),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, 64, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        test_struct_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, 64, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
        TEST_ASSERT(unpacked.field_uint8 == 0);
}

TEST_CASE(test_overlapping_fields)
{
        /* Overlapping fields are defined behaviour: last write wins. */
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {.field_uint8 = 0xFF};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_UINT8,
             .start_bit = 0,
             .bit_length = 8,
             .ptr_offset = offsetof(test_struct_t, field_uint8),
             .behaviour = PPACK_BEHAVIOUR_RAW},
            {.type = PPACK_TYPE_UINT8,
             .start_bit = 4,
             .bit_length = 8,
             .ptr_offset = offsetof(test_struct_t, field_uint8),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, 64, fields, 2);
        TEST_ASSERT(ret == PPACK_SUCCESS);
}

TEST_CASE(test_full_payload)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {
            .field_uint16 = 0x1234,
            .field_int16 = 0x5678,
            .field_uint32 = 0x9ABCDEF0,
        };

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_UINT16,
             .start_bit = 0,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_t, field_uint16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
            {.type = PPACK_TYPE_INT16,
             .start_bit = 16,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_t, field_int16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
            {.type = PPACK_TYPE_UINT32,
             .start_bit = 32,
             .bit_length = 32,
             .ptr_offset = offsetof(test_struct_t, field_uint32),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, 64, fields, 3);
        TEST_ASSERT(ret == PPACK_SUCCESS);
        TEST_ASSERT(READ_LOGICAL_BYTE(payload, 0) == 0x34);
        TEST_ASSERT(READ_LOGICAL_BYTE(payload, 1) == 0x12);
        TEST_ASSERT(READ_LOGICAL_BYTE(payload, 2) == 0x78);
        TEST_ASSERT(READ_LOGICAL_BYTE(payload, 3) == 0x56);
        TEST_ASSERT(READ_LOGICAL_BYTE(payload, 4) == 0xF0);
        TEST_ASSERT(READ_LOGICAL_BYTE(payload, 5) == 0xDE);
        TEST_ASSERT(READ_LOGICAL_BYTE(payload, 6) == 0xBC);
        TEST_ASSERT(READ_LOGICAL_BYTE(payload, 7) == 0x9A);

        test_struct_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, 64, fields, 3);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
        TEST_ASSERT(unpacked.field_uint16 == 0x1234);
        TEST_ASSERT(unpacked.field_int16 == 0x5678);
        TEST_ASSERT(unpacked.field_uint32 == 0x9ABCDEF0);
}

void
run_edge_tests(void)
{
        run_test(test_invalid_field_type, "test_invalid_field_type");
        run_test(test_unpack_invalid_field_type,
                 "test_unpack_invalid_field_type");
        run_test(test_empty_field_list, "test_empty_field_list");
        run_test(test_unpack_zero_field_count, "test_unpack_zero_field_count");
        run_test(test_small_bit_field, "test_small_bit_field");
        run_test(test_6bit_field, "test_6bit_field");
        run_test(test_adjacent_bit_fields, "test_adjacent_bit_fields");
        run_test(test_negative_int32, "test_negative_int32");
        run_test(test_max_uint16, "test_max_uint16");
        run_test(test_max_int16, "test_max_int16");
        run_test(test_min_int16, "test_min_int16");
        run_test(test_zero_values, "test_zero_values");
        run_test(test_negative_float, "test_negative_float");
        run_test(test_float_zero, "test_float_zero");
        run_test(test_1bit_field, "test_1bit_field");
        run_test(test_1bit_field_zero, "test_1bit_field_zero");
        run_test(test_overlapping_fields, "test_overlapping_fields");
        run_test(test_full_payload, "test_full_payload");
}
