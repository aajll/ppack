/*
 * @file: test_fixtures.h
 * @brief Shared fixtures, payload-inspection macros, and struct typedefs
 *        used by every category of the ppack unit-test suite.
 */

#ifndef TEST_FIXTURES_H_
#define TEST_FIXTURES_H_

#include "ppack.h"
#include "test_harness.h"
#include <ppack_platform.h>
#include <stdint.h>

#define PPACK_LOGICAL_BYTES_PER_UNIT (PPACK_ADDR_UNIT_BITS / 8)

/*
 * Read a logical byte from the payload buffer, abstracting away
 * the underlying addressable unit (8-bit byte vs 16-bit word).
 */
#define READ_LOGICAL_BYTE(payload, byte_index)                                 \
        ((((ppack_byte_t *)(payload))[(byte_index)                             \
                                      / PPACK_LOGICAL_BYTES_PER_UNIT]          \
          >> (((byte_index) % PPACK_LOGICAL_BYTES_PER_UNIT) * 8))              \
         & 0xFFu)

/* Helper: assert a specific bit in the payload has the expected value */
#define ASSERT_PAYLOAD_BIT(payload, bit_pos, expected)                         \
        do {                                                                   \
                uint16_t byte_idx = (bit_pos) / 8;                             \
                uint8_t bit_idx = (bit_pos) % 8;                               \
                uint32_t byte_val = READ_LOGICAL_BYTE(payload, byte_idx);      \
                uint32_t bit = (byte_val >> bit_idx) & 1u;                     \
                TEST_ASSERT(bit == ((expected) ? 1u : 0u));                    \
        } while (0)

/* Helper: assert a range of logical bytes in the payload */
#define ASSERT_PAYLOAD_BYTES_EQ(payload, start_byte, count, ...)               \
        do {                                                                   \
                const uint8_t _expected[] = {__VA_ARGS__};                     \
                for (int _i = 0; _i < (count); ++_i) {                         \
                        TEST_ASSERT(                                           \
                            READ_LOGICAL_BYTE(payload, (start_byte) + _i)      \
                            == _expected[_i]);                                 \
                }                                                              \
        } while (0)

typedef struct {
        uint16_t field_u16;
        int16_t field_s16;
        uint32_t field_u32;
        int32_t field_s32;
        float field_f32;
        ppack_u8_t field_u8; /* Mirrors target-side uint8_t (16-bit on TI) */
        uint32_t field_bits;
} test_struct_t;

typedef struct {
        float field_u16_scaled;
        float field_s16_scaled;
        float field_u32_scaled;
        float field_s32_scaled;
} test_struct_scaled_t;

#endif /* TEST_FIXTURES_H_ */
