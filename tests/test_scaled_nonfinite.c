/*
 * @file: test_scaled_nonfinite.c
 * @brief Non-finite inputs on SCALED fields: NaN is rejected with
 *        -PPACK_ERR_INVALARG (casting NaN to an integer is undefined
 *        behaviour); +/-infinity saturates to the destination clamp
 *        bounds like any other out-of-range value.
 */

#include "ppack.h"
#include "test_fixtures.h"
#include "test_harness.h"
#include <math.h>
#include <ppack_platform.h>
#include <stddef.h>

TEST_CASE(test_pack_nonfinite_uint16_scaled)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_scaled_t data = {.field_uint16_scaled = NAN};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_UINT16,
             .start_bit = 0,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_scaled_t, field_uint16_scaled),
             .scale = 1.0f,
             .offset = 0.0f,
             .behaviour = PPACK_BEHAVIOUR_SCALED},
        };

        /* NaN has no defined integer representation: rejected. */
        int ret = ppack_pack(&data, payload, 64, fields, 1);
        TEST_ASSERT(ret == -PPACK_ERR_INVALARG);

        /* +Inf saturates to the destination maximum (65535). */
        data.field_uint16_scaled = INFINITY;
        ret = ppack_pack(&data, payload, 64, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);
        ASSERT_PAYLOAD_BYTES_EQ(payload, 0, 2, 0xFF, 0xFF);

        /* -Inf saturates to the destination minimum (0). */
        data.field_uint16_scaled = -INFINITY;
        ret = ppack_pack(&data, payload, 64, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);
        ASSERT_PAYLOAD_BYTES_EQ(payload, 0, 2, 0x00, 0x00);
}

TEST_CASE(test_pack_nonfinite_int16_scaled)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_scaled_t data = {.field_int16_scaled = NAN};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_INT16,
             .start_bit = 0,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_scaled_t, field_int16_scaled),
             .scale = 1.0f,
             .offset = 0.0f,
             .behaviour = PPACK_BEHAVIOUR_SCALED},
        };

        int ret = ppack_pack(&data, payload, 64, fields, 1);
        TEST_ASSERT(ret == -PPACK_ERR_INVALARG);

        /* +Inf saturates to 32767 (0x7FFF little-endian). */
        data.field_int16_scaled = INFINITY;
        ret = ppack_pack(&data, payload, 64, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);
        ASSERT_PAYLOAD_BYTES_EQ(payload, 0, 2, 0xFF, 0x7F);

        /* -Inf saturates to -32768 (0x8000 little-endian). */
        data.field_int16_scaled = -INFINITY;
        ret = ppack_pack(&data, payload, 64, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);
        ASSERT_PAYLOAD_BYTES_EQ(payload, 0, 2, 0x00, 0x80);
}

TEST_CASE(test_pack_nonfinite_uint32_scaled)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_scaled_t data = {.field_uint32_scaled = NAN};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_UINT32,
             .start_bit = 0,
             .bit_length = 32,
             .ptr_offset = offsetof(test_struct_scaled_t, field_uint32_scaled),
             .scale = 1.0f,
             .offset = 0.0f,
             .behaviour = PPACK_BEHAVIOUR_SCALED},
        };

        int ret = ppack_pack(&data, payload, 64, fields, 1);
        TEST_ASSERT(ret == -PPACK_ERR_INVALARG);

        /* +Inf saturates to the float-exact clamp 4294967040
         * (0xFFFFFF00 little-endian). */
        data.field_uint32_scaled = INFINITY;
        ret = ppack_pack(&data, payload, 64, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);
        ASSERT_PAYLOAD_BYTES_EQ(payload, 0, 4, 0x00, 0xFF, 0xFF, 0xFF);

        /* -Inf saturates to 0. */
        data.field_uint32_scaled = -INFINITY;
        ret = ppack_pack(&data, payload, 64, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);
        ASSERT_PAYLOAD_BYTES_EQ(payload, 0, 4, 0x00, 0x00, 0x00, 0x00);
}

TEST_CASE(test_pack_nonfinite_int32_scaled)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_scaled_t data = {.field_int32_scaled = NAN};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_INT32,
             .start_bit = 0,
             .bit_length = 32,
             .ptr_offset = offsetof(test_struct_scaled_t, field_int32_scaled),
             .scale = 1.0f,
             .offset = 0.0f,
             .behaviour = PPACK_BEHAVIOUR_SCALED},
        };

        int ret = ppack_pack(&data, payload, 64, fields, 1);
        TEST_ASSERT(ret == -PPACK_ERR_INVALARG);

        /* +Inf saturates to the float-exact clamp 2147483520
         * (0x7FFFFF80 little-endian). */
        data.field_int32_scaled = INFINITY;
        ret = ppack_pack(&data, payload, 64, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);
        ASSERT_PAYLOAD_BYTES_EQ(payload, 0, 4, 0x80, 0xFF, 0xFF, 0x7F);

        /* -Inf saturates to -2147483648 (0x80000000 little-endian). */
        data.field_int32_scaled = -INFINITY;
        ret = ppack_pack(&data, payload, 64, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);
        ASSERT_PAYLOAD_BYTES_EQ(payload, 0, 4, 0x00, 0x00, 0x00, 0x80);
}

TEST_CASE(test_pack_nonfinite_descriptor_rejected)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_scaled_t data = {.field_uint16_scaled = 1.0f};

        /* A NaN offset poisons the scaled result even for a finite
         * source value: rejected the same way as a NaN input. */
        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_UINT16,
             .start_bit = 0,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_scaled_t, field_uint16_scaled),
             .scale = 1.0f,
             .offset = NAN,
             .behaviour = PPACK_BEHAVIOUR_SCALED},
        };

        int ret = ppack_pack(&data, payload, 64, fields, 1);
        TEST_ASSERT(ret == -PPACK_ERR_INVALARG);
}

void
run_scaled_nonfinite_tests(void)
{
        run_test(test_pack_nonfinite_uint16_scaled,
                 "test_pack_nonfinite_uint16_scaled");
        run_test(test_pack_nonfinite_int16_scaled,
                 "test_pack_nonfinite_int16_scaled");
        run_test(test_pack_nonfinite_uint32_scaled,
                 "test_pack_nonfinite_uint32_scaled");
        run_test(test_pack_nonfinite_int32_scaled,
                 "test_pack_nonfinite_int32_scaled");
        run_test(test_pack_nonfinite_descriptor_rejected,
                 "test_pack_nonfinite_descriptor_rejected");
}
