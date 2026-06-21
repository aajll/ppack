/*
 * @file: test_mau_layout.c
 * @brief Verify the physical memory layout of ppack payloads on 16-bit MAU.
 *
 * This test ensures that when PPACK_SIMULATE_16BIT_MAU is active:
 * 1. One unit is used per 8 bits of payload.
 * 2. The payload data resides in the lower 8 bits of each unit.
 * 3. The upper 8 bits of each unit remain zero.
 */

#include "ppack.h"
#include "test_harness.h"
#include <ppack_platform.h>
#include <stddef.h>
#include <stdint.h>

TEST_CASE(test_16bit_mau_physical_layout)
{
#if !PPACK_IS_16BIT_MAU
        /* This test is only meaningful on 16-bit MAU targets */
        return;
#endif

        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};

        typedef struct {
                uint16_t val;
        } layout_t;

        /* Field: 16 bits, value 0xABCD.
         * Bit 0-7: 0xCD (byte 0)
         * Bit 8-15: 0xAB (byte 1)
         */
        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_UINT16,
             .start_bit = 0,
             .bit_length = 16,
             .ptr_offset = offsetof(layout_t, val),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        layout_t src = {.val = 0xABCD};
        /* Note: ppack_pack takes a base_ptr. We'll pass &src.
         * The ptr_offset for the field is 0.
         */
        int ret = ppack_pack(&src, payload, 64, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        /* On 16-bit MAU, we expect:
         * payload[0] = 0x00CD
         * payload[1] = 0x00AB
         * payload[2..7] = 0x0000
         */
        TEST_ASSERT(payload[0] == 0x00CDu);
        TEST_ASSERT(payload[1] == 0x00ABu);

        for (uint16_t i = 2; i < PPACK_PAYLOAD_UNITS; ++i) {
                TEST_ASSERT(payload[i] == 0u);
        }
}

void
run_mau_layout_tests(void)
{
        run_test(test_16bit_mau_physical_layout,
                 "test_16bit_mau_physical_layout");
}
