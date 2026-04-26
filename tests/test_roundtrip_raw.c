/*
 * @file: test_roundtrip_raw.c
 * @brief Raw (non-scaled) pack/unpack round-trips at full type widths.
 */

#include "ppack.h"
#include "test_fixtures.h"
#include "test_harness.h"
#include <ppack_platform.h>
#include <stddef.h>

TEST_CASE(test_pack_unpack_uint16)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {.field_uint16 = 0x1234};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_UINT16,
             .start_bit = 0,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_t, field_uint16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);
        TEST_ASSERT(READ_LOGICAL_BYTE(payload, 0) == 0x34);
        TEST_ASSERT(READ_LOGICAL_BYTE(payload, 1) == 0x12);

        test_struct_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
        TEST_ASSERT(unpacked.field_uint16 == 0x1234);
}

TEST_CASE(test_pack_unpack_int16)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {.field_int16 = -1234};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_INT16,
             .start_bit = 0,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_t, field_int16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        test_struct_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
        TEST_ASSERT(unpacked.field_int16 == -1234);
}

TEST_CASE(test_pack_unpack_uint32)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {.field_uint32 = 0x12345678};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_UINT32,
             .start_bit = 0,
             .bit_length = 32,
             .ptr_offset = offsetof(test_struct_t, field_uint32),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        test_struct_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
        TEST_ASSERT(unpacked.field_uint32 == 0x12345678);
}

TEST_CASE(test_pack_unpack_int32)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {.field_int32 = -12345678};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_INT32,
             .start_bit = 0,
             .bit_length = 32,
             .ptr_offset = offsetof(test_struct_t, field_int32),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        test_struct_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
        TEST_ASSERT(unpacked.field_int32 == -12345678);
}

TEST_CASE(test_pack_unpack_f32)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {.field_f32 = 3.14159f};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_F32,
             .start_bit = 0,
             .bit_length = 32,
             .ptr_offset = offsetof(test_struct_t, field_f32),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        test_struct_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);

        float diff = unpacked.field_f32 - data.field_f32;
        if (diff < 0.0f) {
                diff = -diff;
        }
        TEST_ASSERT(diff < 0.0001f);
}

TEST_CASE(test_pack_unpack_uint8)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {.field_uint8 = 0xAB};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_UINT8,
             .start_bit = 0,
             .bit_length = 8,
             .ptr_offset = offsetof(test_struct_t, field_uint8),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        test_struct_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
        TEST_ASSERT(unpacked.field_uint8 == 0xAB);
}

TEST_CASE(test_pack_unpack_bits)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {.field_bits = 0xDEADBEEF};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_BITS,
             .start_bit = 0,
             .bit_length = 32,
             .ptr_offset = offsetof(test_struct_t, field_bits),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        test_struct_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
        TEST_ASSERT(unpacked.field_bits == 0xDEADBEEF);
}

void
run_roundtrip_raw_tests(void)
{
        run_test(test_pack_unpack_uint16, "test_pack_unpack_uint16");
        run_test(test_pack_unpack_int16, "test_pack_unpack_int16");
        run_test(test_pack_unpack_uint32, "test_pack_unpack_uint32");
        run_test(test_pack_unpack_int32, "test_pack_unpack_int32");
        run_test(test_pack_unpack_f32, "test_pack_unpack_f32");
        run_test(test_pack_unpack_uint8, "test_pack_unpack_uint8");
        run_test(test_pack_unpack_bits, "test_pack_unpack_bits");
}
