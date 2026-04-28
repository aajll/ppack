/*
 * @file: test_wire_lockdown.c
 * @brief Wire-format lockdown tests for the 64-bit (CAN classic) payload.
 *
 * These tests assert the exact byte sequence produced by ppack_pack
 * for a fixed set of inputs at payload_bits=64. The wire format has
 * been stable across every release of the library; it is the contract
 * that downstream firmware nodes (e.g. Power Module / Supervisory
 * controller CAN bus) rely on.
 *
 * Do not edit the expected byte arrays in these tests. A change to
 * any expected byte is a BREAKING change to the over-the-wire
 * protocol, regardless of the library version, and must be
 * coordinated with every downstream consumer.
 *
 * Lockdown for non-default payload sizes (e.g. CAN-FD 128 / 256 / 512)
 * lives in test_payload_size.c.
 */

#include "ppack.h"
#include "test_fixtures.h"
#include "test_harness.h"
#include <ppack_platform.h>
#include <stddef.h>
#include <stdint.h>

TEST_CASE(test_wire_lockdown_64bit_byte_aligned)
{
        /* Layout:
         *   bits  0..15  PPACK_TYPE_UINT16  (uint16_t  = 0x1234)
         *   bits 16..31  PPACK_TYPE_INT16  (int16_t   = -100   = 0xFF9C)
         *   bits 32..63  PPACK_TYPE_UINT32  (uint32_t  = 0xDEADBEEF)
         */
        test_struct_t data = {
            .field_uint16 = 0x1234u,
            .field_int16 = -100,
            .field_uint32 = 0xDEADBEEFu,
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

        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        TEST_ASSERT(ppack_pack(&data, payload, 64, fields, 3) == PPACK_SUCCESS);

        static const uint8_t expected[8] = {
            0x34u, 0x12u, 0x9Cu, 0xFFu, 0xEFu, 0xBEu, 0xADu, 0xDEu,
        };
        for (uint16_t i = 0; i < 8; ++i) {
                TEST_ASSERT(READ_LOGICAL_BYTE(payload, i) == expected[i]);
        }
}

TEST_CASE(test_wire_lockdown_64bit_float_and_small_types)
{
        /* Layout:
         *   bits  0..31  PPACK_TYPE_F32  (float    = 1.0f = 0x3F800000)
         *   bits 32..47  PPACK_TYPE_UINT16  (uint16_t = 0xCAFE)
         *   bits 48..55  PPACK_TYPE_UINT8   (uint8_t  = 0xAB)
         *   bits 56..63  PPACK_TYPE_BITS (uint32_t = 0xCD, 8-bit field)
         */
        test_struct_t data = {0};
        data.field_f32 = 1.0f;
        data.field_uint16 = 0xCAFEu;
        data.field_uint8 = (ppack_u8_t)0xABu;
        data.field_bits = 0xCDu;

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_F32,
             .start_bit = 0,
             .bit_length = 32,
             .ptr_offset = offsetof(test_struct_t, field_f32),
             .behaviour = PPACK_BEHAVIOUR_RAW},
            {.type = PPACK_TYPE_UINT16,
             .start_bit = 32,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_t, field_uint16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
            {.type = PPACK_TYPE_UINT8,
             .start_bit = 48,
             .bit_length = 8,
             .ptr_offset = offsetof(test_struct_t, field_uint8),
             .behaviour = PPACK_BEHAVIOUR_RAW},
            {.type = PPACK_TYPE_BITS,
             .start_bit = 56,
             .bit_length = 8,
             .ptr_offset = offsetof(test_struct_t, field_bits),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        TEST_ASSERT(ppack_pack(&data, payload, 64, fields, 4) == PPACK_SUCCESS);

        static const uint8_t expected[8] = {
            0x00u, 0x00u, 0x80u, 0x3Fu, 0xFEu, 0xCAu, 0xABu, 0xCDu,
        };
        for (uint16_t i = 0; i < 8; ++i) {
                TEST_ASSERT(READ_LOGICAL_BYTE(payload, i) == expected[i]);
        }
}

TEST_CASE(test_wire_lockdown_64bit_cross_byte_boundary)
{
        /* Layout: a single PPACK_TYPE_UINT16 = 0xABCD shifted left by 4 bits.
         * Verifies the LSB-first / little-endian bit ordering on a non-
         * byte-aligned start position. Bits 0..3 and 20..63 are zero. */
        test_struct_t data = {.field_uint16 = 0xABCDu};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_UINT16,
             .start_bit = 4,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_t, field_uint16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        TEST_ASSERT(ppack_pack(&data, payload, 64, fields, 1) == PPACK_SUCCESS);

        static const uint8_t expected[8] = {
            0xD0u, 0xBCu, 0x0Au, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        };
        for (uint16_t i = 0; i < 8; ++i) {
                TEST_ASSERT(READ_LOGICAL_BYTE(payload, i) == expected[i]);
        }
}

void
run_wire_lockdown_tests(void)
{
        run_test(test_wire_lockdown_64bit_byte_aligned,
                 "test_wire_lockdown_64bit_byte_aligned");
        run_test(test_wire_lockdown_64bit_float_and_small_types,
                 "test_wire_lockdown_64bit_float_and_small_types");
        run_test(test_wire_lockdown_64bit_cross_byte_boundary,
                 "test_wire_lockdown_64bit_cross_byte_boundary");
}
