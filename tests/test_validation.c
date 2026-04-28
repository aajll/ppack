/*
 * @file: test_validation.c
 * @brief Overflow and bit_length bounds validation.
 */

#include "ppack.h"
#include "test_fixtures.h"
#include "test_harness.h"
#include <ppack_platform.h>
#include <stddef.h>

TEST_CASE(test_pack_overflow_start_plus_len)
{
        /* start_bit=56, bit_length=16 → 56+16=72 > 64: must be rejected */
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {.field_uint16 = 0x1234};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_UINT16,
             .start_bit = 56,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_t, field_uint16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, 64, fields, 1);
        TEST_ASSERT(ret == -PPACK_ERR_OVERFLOW);
}

TEST_CASE(test_unpack_overflow_start_plus_len)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {0};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_UINT16,
             .start_bit = 56,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_t, field_uint16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_unpack(&data, payload, 64, fields, 1);
        TEST_ASSERT(ret == -PPACK_ERR_OVERFLOW);
}

TEST_CASE(test_pack_exact_boundary_passes)
{
        /* start_bit=48, bit_length=16 → 48+16=64: exactly on the limit, OK */
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {.field_uint16 = 0xABCD};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_UINT16,
             .start_bit = 48,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_t, field_uint16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, 64, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);
        TEST_ASSERT(READ_LOGICAL_BYTE(payload, 6) == 0xCD);
        TEST_ASSERT(READ_LOGICAL_BYTE(payload, 7) == 0xAB);
}

TEST_CASE(test_pack_bit_length_zero_rejected)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {.field_uint16 = 0x1234};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_UINT16,
             .start_bit = 0,
             .bit_length = 0,
             .ptr_offset = offsetof(test_struct_t, field_uint16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, 64, fields, 1);
        TEST_ASSERT(ret == -PPACK_ERR_INVALARG);
}

TEST_CASE(test_pack_bit_length_33_rejected)
{
        /* bit_length > 32 must be rejected (would cause UB shift) */
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {.field_uint32 = 0xDEADBEEF};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_UINT32,
             .start_bit = 0,
             .bit_length = 33,
             .ptr_offset = offsetof(test_struct_t, field_uint32),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, 64, fields, 1);
        TEST_ASSERT(ret == -PPACK_ERR_INVALARG);
}

void
run_validation_tests(void)
{
        run_test(test_pack_overflow_start_plus_len,
                 "test_pack_overflow_start_plus_len");
        run_test(test_unpack_overflow_start_plus_len,
                 "test_unpack_overflow_start_plus_len");
        run_test(test_pack_exact_boundary_passes,
                 "test_pack_exact_boundary_passes");
        run_test(test_pack_bit_length_zero_rejected,
                 "test_pack_bit_length_zero_rejected");
        run_test(test_pack_bit_length_33_rejected,
                 "test_pack_bit_length_33_rejected");
}
