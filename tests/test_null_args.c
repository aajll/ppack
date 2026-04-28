/*
 * @file: test_null_args.c
 * @brief NULL / invalid-argument handling.
 */

#include "ppack.h"
#include "test_fixtures.h"
#include "test_harness.h"
#include <ppack_platform.h>
#include <stddef.h>

TEST_CASE(test_pack_null_base_ptr)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_UINT16,
             .start_bit = 0,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_t, field_uint16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(NULL, payload, 64, fields, 1);
        TEST_ASSERT(ret == -PPACK_ERR_NULLPTR);
}

TEST_CASE(test_pack_null_payload)
{
        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_UINT16,
             .start_bit = 0,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_t, field_uint16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };
        test_struct_t data = {.field_uint16 = 0x1234};

        int ret = ppack_pack(&data, NULL, 64, fields, 1);
        TEST_ASSERT(ret == -PPACK_ERR_NULLPTR);
}

TEST_CASE(test_pack_null_fields)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {.field_uint16 = 0x1234};

        int ret = ppack_pack(&data, payload, 64, NULL, 1);
        TEST_ASSERT(ret == -PPACK_ERR_INVALARG);
}

TEST_CASE(test_unpack_null_base_ptr)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_UINT16,
             .start_bit = 0,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_t, field_uint16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_unpack(NULL, payload, 64, fields, 1);
        TEST_ASSERT(ret == -PPACK_ERR_NULLPTR);
}

TEST_CASE(test_unpack_null_payload)
{
        test_struct_t data = {0};
        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_UINT16,
             .start_bit = 0,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_t, field_uint16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_unpack(&data, NULL, 64, fields, 1);
        TEST_ASSERT(ret == -PPACK_ERR_NULLPTR);
}

TEST_CASE(test_unpack_null_fields)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {0};

        int ret = ppack_unpack(&data, payload, 64, NULL, 1);
        TEST_ASSERT(ret == -PPACK_ERR_INVALARG);
}

void
run_null_arg_tests(void)
{
        run_test(test_pack_null_base_ptr, "test_pack_null_base_ptr");
        run_test(test_pack_null_payload, "test_pack_null_payload");
        run_test(test_pack_null_fields, "test_pack_null_fields");
        run_test(test_unpack_null_base_ptr, "test_unpack_null_base_ptr");
        run_test(test_unpack_null_payload, "test_unpack_null_payload");
        run_test(test_unpack_null_fields, "test_unpack_null_fields");
}
