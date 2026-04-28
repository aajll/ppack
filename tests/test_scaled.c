/*
 * @file: test_scaled.c
 * @brief Scaled (linear scale/offset) field round-trips.
 */

#include "ppack.h"
#include "test_fixtures.h"
#include "test_harness.h"
#include <ppack_platform.h>
#include <stddef.h>

TEST_CASE(test_pack_scaled_uint16)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        float input_val = 123.45f;

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_UINT16,
             .start_bit = 0,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_scaled_t, field_uint16_scaled),
             .scale = 0.01f,
             .offset = 0.0f,
             .behaviour = PPACK_BEHAVIOUR_SCALED},
        };

        test_struct_scaled_t data = {.field_uint16_scaled = input_val};
        int ret = ppack_pack(&data, payload, 64, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        test_struct_scaled_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, 64, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);

        float diff = unpacked.field_uint16_scaled - 123.45f;
        if (diff < 0.0f) {
                diff = -diff;
        }
        TEST_ASSERT(diff < 0.1f);
}

TEST_CASE(test_pack_scaled_int16_with_offset)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        float input_val = -25.5f;

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_INT16,
             .start_bit = 0,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_scaled_t, field_int16_scaled),
             .scale = 0.1f,
             .offset = 10.0f,
             .behaviour = PPACK_BEHAVIOUR_SCALED},
        };

        test_struct_scaled_t data = {.field_int16_scaled = input_val};
        int ret = ppack_pack(&data, payload, 64, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        test_struct_scaled_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, 64, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);

        float diff = unpacked.field_int16_scaled - (-25.5f);
        if (diff < 0.0f) {
                diff = -diff;
        }
        TEST_ASSERT(diff < 0.2f);
}

TEST_CASE(test_pack_scaled_uint32)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        float input_val = 12345.678f;

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_UINT32,
             .start_bit = 0,
             .bit_length = 32,
             .ptr_offset = offsetof(test_struct_scaled_t, field_uint32_scaled),
             .scale = 0.001f,
             .offset = 0.0f,
             .behaviour = PPACK_BEHAVIOUR_SCALED},
        };

        test_struct_scaled_t data = {.field_uint32_scaled = input_val};
        int ret = ppack_pack(&data, payload, 64, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        test_struct_scaled_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, 64, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);

        float diff = unpacked.field_uint32_scaled - 12345.678f;
        if (diff < 0.0f) {
                diff = -diff;
        }
        TEST_ASSERT(diff < 1.0f);
}

TEST_CASE(test_pack_scaled_int32)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        float input_val = -500.0f;

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_INT32,
             .start_bit = 0,
             .bit_length = 32,
             .ptr_offset = offsetof(test_struct_scaled_t, field_int32_scaled),
             .scale = 0.1f,
             .offset = 0.0f,
             .behaviour = PPACK_BEHAVIOUR_SCALED},
        };

        test_struct_scaled_t data = {.field_int32_scaled = input_val};
        int ret = ppack_pack(&data, payload, 64, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        test_struct_scaled_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, 64, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);

        float diff = unpacked.field_int32_scaled - (-500.0f);
        if (diff < 0.0f) {
                diff = -diff;
        }
        TEST_ASSERT(diff < 0.2f);
}

void
run_scaled_tests(void)
{
        run_test(test_pack_scaled_uint16, "test_pack_scaled_uint16");
        run_test(test_pack_scaled_int16_with_offset,
                 "test_pack_scaled_int16_with_offset");
        run_test(test_pack_scaled_uint32, "test_pack_scaled_uint32");
        run_test(test_pack_scaled_int32, "test_pack_scaled_int32");
}
