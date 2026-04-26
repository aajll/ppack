/*
 * @file: test_layout.c
 * @brief Field layout and multi-field placement.
 */

#include "ppack.h"
#include "test_fixtures.h"
#include "test_harness.h"
#include <ppack_platform.h>
#include <stddef.h>

TEST_CASE(test_pack_unpack_u16_offset)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {.field_u16 = 0x1234};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_UINT16,
             .start_bit = 16,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_t, field_u16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);
        TEST_ASSERT(READ_LOGICAL_BYTE(payload, 2) == 0x34);
        TEST_ASSERT(READ_LOGICAL_BYTE(payload, 3) == 0x12);

        test_struct_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
        TEST_ASSERT(unpacked.field_u16 == 0x1234);
}

TEST_CASE(test_pack_unpack_spanning_boundary)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {.field_u32 = 0x12345678};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_UINT32,
             .start_bit = 8,
             .bit_length = 32,
             .ptr_offset = offsetof(test_struct_t, field_u32),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        test_struct_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
        TEST_ASSERT(unpacked.field_u32 == 0x12345678);
}

TEST_CASE(test_pack_multiple_fields)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {
            .field_u16 = 0x1234,
            .field_s16 = -5678,
            .field_u8 = 0xAB,
        };

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_UINT16,
             .start_bit = 0,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_t, field_u16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
            {.type = PPACK_TYPE_INT16,
             .start_bit = 16,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_t, field_s16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
            {.type = PPACK_TYPE_UINT8,
             .start_bit = 32,
             .bit_length = 8,
             .ptr_offset = offsetof(test_struct_t, field_u8),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, fields, 3);
        TEST_ASSERT(ret == PPACK_SUCCESS);
        TEST_ASSERT(READ_LOGICAL_BYTE(payload, 0) == 0x34);
        TEST_ASSERT(READ_LOGICAL_BYTE(payload, 1) == 0x12);
        TEST_ASSERT(READ_LOGICAL_BYTE(payload, 2) == 0xd2);
        TEST_ASSERT(READ_LOGICAL_BYTE(payload, 3) == 0xe9);

        test_struct_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, fields, 3);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
        TEST_ASSERT(unpacked.field_u16 == 0x1234);
        TEST_ASSERT(unpacked.field_s16 == -5678);
        TEST_ASSERT(unpacked.field_u8 == 0xAB);
}

void
run_layout_tests(void)
{
        run_test(test_pack_unpack_u16_offset, "test_pack_unpack_u16_offset");
        run_test(test_pack_unpack_spanning_boundary,
                 "test_pack_unpack_spanning_boundary");
        run_test(test_pack_multiple_fields, "test_pack_multiple_fields");
}
