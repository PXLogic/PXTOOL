/*
 * This file is part of the PXTOOL project.
 * PXTOOL is based on PulseView.
 *
 * Copyright (C) 2026 DreamSourceLab <support@dreamsourcelab.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#ifndef TEST_DATAFEED_STUB_H
#define TEST_DATAFEED_STUB_H

#include <stddef.h>
#include <stdint.h>

struct test_captured_datafeed_packet {
    uint16_t type;
    uint16_t status;
    uint64_t logic_length;
    int logic_format;
    uint16_t logic_unitsize;
    const void *logic_data;
};

#ifdef __cplusplus
extern "C" {
#endif

void test_datafeed_reset(void);
const test_captured_datafeed_packet *test_datafeed_last_packet(void);
uint64_t test_input_observer_sample_limit(void);
bool test_input_observer_analog_is_standard_float(void);
unsigned int test_input_observer_analog_channel_count(void);
int test_input_observer_analog_channel_index(unsigned int index);
size_t test_input_observer_analog_prefix_length(void);
float test_input_observer_analog_prefix(unsigned int index);

#ifdef __cplusplus
}
#endif

#endif
