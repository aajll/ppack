/*
 * @file: test_scale_zero.c
 * @brief Rejection of scale==0 (and any scaling on PPACK_TYPE_UINT8).
 */

#include "ppack.h"
#include "test_fixtures.h"
#include "test_harness.h"
#include <ppack_platform.h>
#include <stddef.h>

TEST_CASE(test_pack_scale_zero_uint16_rejected)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_scaled_t data = {.field_uint16_scaled = 1.0f};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_UINT16,
             .start_bit = 0,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_scaled_t, field_uint16_scaled),
             .scale = 0.0f,
             .offset = 0.0f,
             .behaviour = PPACK_BEHAVIOUR_SCALED},
        };

        int ret = ppack_pack(&data, payload, fields, 1);
        TEST_ASSERT(ret == -PPACK_ERR_OVERFLOW);
}

TEST_CASE(test_pack_scale_zero_int16_rejected)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_scaled_t data = {.field_int16_scaled = -1.0f};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_INT16,
             .start_bit = 0,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_scaled_t, field_int16_scaled),
             .scale = 0.0f,
             .offset = 0.0f,
             .behaviour = PPACK_BEHAVIOUR_SCALED},
        };

        int ret = ppack_pack(&data, payload, fields, 1);
        TEST_ASSERT(ret == -PPACK_ERR_OVERFLOW);
}

TEST_CASE(test_pack_scale_zero_uint32_rejected)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_scaled_t data = {.field_uint32_scaled = 1.0f};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_UINT32,
             .start_bit = 0,
             .bit_length = 32,
             .ptr_offset = offsetof(test_struct_scaled_t, field_uint32_scaled),
             .scale = 0.0f,
             .offset = 0.0f,
             .behaviour = PPACK_BEHAVIOUR_SCALED},
        };

        int ret = ppack_pack(&data, payload, fields, 1);
        TEST_ASSERT(ret == -PPACK_ERR_OVERFLOW);
}

TEST_CASE(test_pack_scale_zero_int32_rejected)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_scaled_t data = {.field_int32_scaled = 1.0f};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_INT32,
             .start_bit = 0,
             .bit_length = 32,
             .ptr_offset = offsetof(test_struct_scaled_t, field_int32_scaled),
             .scale = 0.0f,
             .offset = 0.0f,
             .behaviour = PPACK_BEHAVIOUR_SCALED},
        };

        int ret = ppack_pack(&data, payload, fields, 1);
        TEST_ASSERT(ret == -PPACK_ERR_OVERFLOW);
}

TEST_CASE(test_pack_scale_uint8_rejected)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_scaled_t data = {0};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_UINT8,
             .start_bit = 0,
             .bit_length = 8,
             .ptr_offset = offsetof(test_struct_t, field_uint8),
             .scale = 1.0f,
             .offset = 0.0f,
             .behaviour = PPACK_BEHAVIOUR_SCALED},
        };

        int ret = ppack_pack(&data, payload, fields, 1);
        TEST_ASSERT(ret == -PPACK_ERR_INVALARG);

        int unpack_ret = ppack_unpack(&data, payload, fields, 1);
        TEST_ASSERT(unpack_ret == -PPACK_ERR_INVALARG);
}

void
run_scale_zero_tests(void)
{
        run_test(test_pack_scale_zero_uint16_rejected,
                 "test_pack_scale_zero_uint16_rejected");
        run_test(test_pack_scale_zero_int16_rejected,
                 "test_pack_scale_zero_int16_rejected");
        run_test(test_pack_scale_zero_uint32_rejected,
                 "test_pack_scale_zero_uint32_rejected");
        run_test(test_pack_scale_zero_int32_rejected,
                 "test_pack_scale_zero_int32_rejected");
        run_test(test_pack_scale_uint8_rejected, "test_pack_scale_uint8_rejected");
}
