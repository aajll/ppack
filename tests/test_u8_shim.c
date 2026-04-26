/*
 * @file: test_u8_shim.c
 * @brief TI C2000 16-bit-MAU uint8_t alias-shim behaviour.
 *
 * Exercises the path where a user's `uint8_t` struct member is
 * 16-bit storage (on TI) or simulated as such via PPACK_SIMULATE_16BIT_MAU
 * + the ppack_u8_t alias.
 */

#include "ppack.h"
#include "test_fixtures.h"
#include "test_harness.h"
#include <ppack_platform.h>
#include <stddef.h>
#include <stdint.h>

TEST_CASE(test_u8_pack_masks_oversized_source)
{
        /* Source holds bits above the low 8; pack must emit only 0x34 */
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {0};
        data.field_u8 = (ppack_u8_t)0x1234u;

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_UINT8,
             .start_bit = 0,
             .bit_length = 8,
             .ptr_offset = offsetof(test_struct_t, field_u8),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);
        TEST_ASSERT(READ_LOGICAL_BYTE(payload, 0) == 0x34u);
        TEST_ASSERT(READ_LOGICAL_BYTE(payload, 1) == 0x00u);
}

TEST_CASE(test_u8_pack_bitlen16_masks_upper)
{
        /*
         * bit_length > 8 is where write_bits no longer masks internally,
         * so the library's explicit `& 0xFFu` is the only guard. Without
         * it on TI, an out-of-range source leaks its upper 8 bits onto
         * the wire.
         */
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {0};
        data.field_u8 = (ppack_u8_t)0xABCDu;

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_UINT8,
             .start_bit = 0,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_t, field_u8),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);
        TEST_ASSERT(READ_LOGICAL_BYTE(payload, 0) == 0xCDu);
        TEST_ASSERT(READ_LOGICAL_BYTE(payload, 1) == 0x00u);
}

TEST_CASE(test_u8_unpack_clears_upper_bits)
{
        /*
         * On TI, field_u8 is 16-bit storage. Unpack must overwrite the
         * whole storage unit, not just the low 8 bits - otherwise junk
         * left over from a prior write would leak into subsequent reads.
         */
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t source = {0};
        source.field_u8 = (ppack_u8_t)0x33u;

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_UINT8,
             .start_bit = 0,
             .bit_length = 8,
             .ptr_offset = offsetof(test_struct_t, field_u8),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int pack_ret = ppack_pack(&source, payload, fields, 1);
        TEST_ASSERT(pack_ret == PPACK_SUCCESS);

        test_struct_t dest = {0};
        dest.field_u8 = (ppack_u8_t)0xAA55u; /* Pre-existing junk */

        int unpack_ret = ppack_unpack(&dest, payload, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
        TEST_ASSERT(dest.field_u8 == (ppack_u8_t)0x33u);
        TEST_ASSERT(((uint16_t)dest.field_u8 & 0xFF00u) == 0u);
}

TEST_CASE(test_u8_adjacent_oversized_inputs)
{
        /*
         * Two adjacent U8 fields, both with source values > 255. On a
         * 16-bit-MAU target both members sit in separate 16-bit words
         * but the wire must still contain only the low bytes.
         */
        typedef struct {
                ppack_u8_t a;
                ppack_u8_t b;
        } u8_pair_t;

        u8_pair_t data = {
            .a = (ppack_u8_t)0xAB12u,
            .b = (ppack_u8_t)0xCD34u,
        };
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_UINT8,
             .start_bit = 0,
             .bit_length = 8,
             .ptr_offset = offsetof(u8_pair_t, a),
             .behaviour = PPACK_BEHAVIOUR_RAW},
            {.type = PPACK_TYPE_UINT8,
             .start_bit = 8,
             .bit_length = 8,
             .ptr_offset = offsetof(u8_pair_t, b),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, fields, 2);
        TEST_ASSERT(ret == PPACK_SUCCESS);
        TEST_ASSERT(READ_LOGICAL_BYTE(payload, 0) == 0x12u);
        TEST_ASSERT(READ_LOGICAL_BYTE(payload, 1) == 0x34u);
}

void
run_u8_shim_tests(void)
{
        run_test(test_u8_pack_masks_oversized_source,
                 "test_u8_pack_masks_oversized_source");
        run_test(test_u8_pack_bitlen16_masks_upper,
                 "test_u8_pack_bitlen16_masks_upper");
        run_test(test_u8_unpack_clears_upper_bits,
                 "test_u8_unpack_clears_upper_bits");
        run_test(test_u8_adjacent_oversized_inputs,
                 "test_u8_adjacent_oversized_inputs");
}
