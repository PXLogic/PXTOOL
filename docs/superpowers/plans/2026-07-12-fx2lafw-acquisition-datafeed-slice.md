# FX2LAFW Acquisition Datafeed Slice Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add build-gated, logic-only acquisition for the upstream-compatible `fx2lafw` driver and feed received logic samples into DSView's existing data path.

**Architecture:** Keep the implementation local to `libsigrok/hardware/upstream-fx2lafw/fx2lafw.c` for this slice. Add testable helper APIs for command calculation, transfer sizing, test-device construction, and logic packet forwarding before adding libusb async transfer lifecycle. Use DSView's existing `sr_session_source_add()` and `ds_data_forward()` instead of importing upstream session infrastructure.

**Tech Stack:** C/C++17 style already used by DSView, CMake, GLib/GVariant/GSList, libusb, Boost.Test, existing vendored `libsigrok`, local upstream reference at `/Users/yuanji/Desktop/project/libsigrok`.

## Global Constraints

- Do not directly replace DSView's entire `libsigrok/` directory with upstream in one step.
- Do not rewrite the PXTOOL UI as part of this slice.
- Do not change DSLogic, DSCope, PXLogic, demo, file, decode, or session behavior.
- Keep `/Users/yuanji/Desktop/project/libsigrok` as a development reference source only; DSView must not depend on that path at build or runtime.
- When importing upstream libsigrok source into DSView, replace the upstream file header with the DSView/PXTOOL copyright and GPL notice style used by nearby files.
- Preserve the current in-source CMake build model for this branch.
- Keep the `fx2lafw` path disabled by default behind `DSVIEW_ENABLE_UPSTREAM_FX2LAFW`.
- Do not add unknown-origin `fx2lafw-*.fw` binary firmware files.
- Do not download firmware at build time.
- This slice must not implement upstream soft trigger support, DSView advanced trigger mapping, analog/MSO support, generic upstream USB helper extraction, or additional upstream drivers.
- Devices found by this path must remain tagged `DS_DEVICE_SOURCE_UPSTREAM_COMPAT`.

---

## File Structure

- Modify `libsigrok/hardware/upstream-fx2lafw/fx2lafw.h`: add acquisition constants, start-command struct, and DSView-private helper declarations used by tests and the driver.
- Modify `libsigrok/hardware/upstream-fx2lafw/fx2lafw.c`: add helper implementations, test-device factory, datafeed helper, start/stop callbacks, async transfer lifecycle, and `CMD_START` control transfer.
- Modify `PXTOOL/test/test_upstream_fx2lafw.cpp`: add hardware-independent tests for acquisition calculations, packet forwarding, callback exposure, and safe stop behavior.
- Create `PXTOOL/test/test_datafeed_stub.h`: shared DSView-test capture API for `ds_data_forward()` packets.
- Create `PXTOOL/test/test_datafeed_stub.cpp`: shared DSView-test implementation of `ds_data_forward()`.
- Modify `PXTOOL/test/test_upstream_demo.cpp`: remove its local `ds_data_forward()` stub so the shared stub owns the symbol.
- Modify `PXTOOL/test/CMakeLists.txt`: compile the shared test datafeed stub, and compile `libsigrok/std.c` for fx2lafw acquisition tests.
- Modify `docs/fx2lafw-hardware-smoke-test.md`: add acquisition smoke-test checklist.
- Modify `docs/libsigrok-upstream-driver-inventory.md`: record firmware resources as complete and acquisition/datafeed as the active slice.

## Task 1: Acquisition Helper Contract

**Files:**
- Modify: `libsigrok/hardware/upstream-fx2lafw/fx2lafw.h`
- Modify: `libsigrok/hardware/upstream-fx2lafw/fx2lafw.c`
- Modify: `PXTOOL/test/test_upstream_fx2lafw.cpp`

**Interfaces:**
- Consumes: existing `struct fx2lafw_profile`, `struct sr_dev_inst`, `struct sr_channel`, `samplerates[]`, `create_device_from_profile()`.
- Produces:
  - `#define FX2LAFW_CMD_START 0xb1`
  - `#define FX2LAFW_BULK_ENDPOINT (2 | LIBUSB_ENDPOINT_IN)`
  - `#define FX2LAFW_NUM_SIMUL_TRANSFERS 32`
  - `#define FX2LAFW_MAX_EMPTY_TRANSFERS (FX2LAFW_NUM_SIMUL_TRANSFERS * 2)`
  - `#define FX2LAFW_MAX_16BIT_SAMPLE_RATE SR_MHZ(12)`
  - `#define FX2LAFW_MAX_SAMPLE_DELAY (6 * 256)`
  - `struct fx2lafw_start_command`
  - `SR_PRIV struct sr_dev_inst *fx2lafw_dev_inst_new_for_profile(...)`
  - `SR_PRIV uint16_t fx2lafw_enabled_channel_mask(const struct sr_dev_inst *sdi);`
  - `SR_PRIV gboolean fx2lafw_sample_wide_for_channels(const struct sr_dev_inst *sdi);`
  - `SR_PRIV int fx2lafw_build_start_command(uint64_t samplerate, gboolean sample_wide, struct fx2lafw_start_command *command);`
  - `SR_PRIV size_t fx2lafw_transfer_buffer_size(uint64_t samplerate);`
  - `SR_PRIV unsigned int fx2lafw_transfer_count(uint64_t samplerate);`
  - `SR_PRIV unsigned int fx2lafw_transfer_timeout_ms(uint64_t samplerate);`

- [ ] **Step 1: Add failing helper tests**

In `PXTOOL/test/test_upstream_fx2lafw.cpp`, append these tests before `BOOST_AUTO_TEST_SUITE_END()`:

```cpp
BOOST_AUTO_TEST_CASE(acquisition_helpers_choose_sample_width_from_enabled_channels)
{
#ifdef HAVE_UPSTREAM_FX2LAFW
    const fx2lafw_profile *profile = fx2lafw_profile_find(
        0x1d50, 0x608d, "sigrok", "fx2lafw");
    BOOST_REQUIRE(profile != nullptr);

    sr_dev_inst *sdi = fx2lafw_dev_inst_new_for_profile(
        profile, 1, 2, SR_ST_ACTIVE, TRUE, 0);
    BOOST_REQUIRE(sdi != nullptr);

    BOOST_CHECK_EQUAL(fx2lafw_enabled_channel_mask(sdi), 0xffff);
    BOOST_CHECK_EQUAL(fx2lafw_sample_wide_for_channels(sdi), TRUE);

    for (GSList *l = sdi->channels; l; l = l->next) {
        struct sr_channel *channel = static_cast<struct sr_channel *>(l->data);
        if (channel->index > 7)
            channel->enabled = FALSE;
    }

    BOOST_CHECK_EQUAL(fx2lafw_enabled_channel_mask(sdi), 0x00ff);
    BOOST_CHECK_EQUAL(fx2lafw_sample_wide_for_channels(sdi), FALSE);

    sr_dev_inst_free(sdi);
#else
    BOOST_TEST_MESSAGE("upstream fx2lafw is disabled for this build");
#endif
}

BOOST_AUTO_TEST_CASE(acquisition_start_command_uses_upstream_clock_rules)
{
#ifdef HAVE_UPSTREAM_FX2LAFW
    fx2lafw_start_command command = {};

    BOOST_REQUIRE_EQUAL(fx2lafw_build_start_command(SR_MHZ(24), FALSE, &command), SR_OK);
    BOOST_CHECK_EQUAL(command.flags, FX2LAFW_CMD_START_FLAGS_CLK_48MHZ |
        FX2LAFW_CMD_START_FLAGS_SAMPLE_8BIT);
    BOOST_CHECK_EQUAL(command.sample_delay_h, 0);
    BOOST_CHECK_EQUAL(command.sample_delay_l, 1);

    command = {};
    BOOST_REQUIRE_EQUAL(fx2lafw_build_start_command(SR_MHZ(48), FALSE, &command), SR_OK);
    BOOST_CHECK_EQUAL(command.flags, FX2LAFW_CMD_START_FLAGS_CLK_48MHZ |
        FX2LAFW_CMD_START_FLAGS_SAMPLE_8BIT);
    BOOST_CHECK_EQUAL(command.sample_delay_h, 0);
    BOOST_CHECK_EQUAL(command.sample_delay_l, 0);

    command = {};
    BOOST_REQUIRE_EQUAL(fx2lafw_build_start_command(SR_MHZ(12), TRUE, &command), SR_OK);
    BOOST_CHECK_EQUAL(command.flags, FX2LAFW_CMD_START_FLAGS_CLK_48MHZ |
        FX2LAFW_CMD_START_FLAGS_SAMPLE_16BIT);
    BOOST_CHECK_EQUAL(command.sample_delay_h, 0);
    BOOST_CHECK_EQUAL(command.sample_delay_l, 3);

    BOOST_CHECK_EQUAL(fx2lafw_build_start_command(SR_MHZ(16), TRUE, &command), SR_ERR);
    BOOST_CHECK_EQUAL(fx2lafw_build_start_command(1234567, FALSE, &command), SR_ERR);
    BOOST_CHECK_EQUAL(fx2lafw_build_start_command(SR_MHZ(1), FALSE, nullptr), SR_ERR_ARG);
#else
    BOOST_TEST_MESSAGE("upstream fx2lafw is disabled for this build");
#endif
}

BOOST_AUTO_TEST_CASE(acquisition_transfer_sizing_matches_upstream_rules)
{
#ifdef HAVE_UPSTREAM_FX2LAFW
    BOOST_CHECK_EQUAL(fx2lafw_transfer_buffer_size(SR_MHZ(1)), 10240U);
    BOOST_CHECK_EQUAL(fx2lafw_transfer_count(SR_MHZ(1)), 32U);
    BOOST_CHECK_EQUAL(fx2lafw_transfer_timeout_ms(SR_MHZ(1)), 400U);

    BOOST_CHECK_EQUAL(fx2lafw_transfer_buffer_size(SR_KHZ(20)), 512U);
    BOOST_CHECK_EQUAL(fx2lafw_transfer_count(SR_KHZ(20)), 19U);
    BOOST_CHECK_EQUAL(fx2lafw_transfer_timeout_ms(SR_KHZ(20)), 608U);

    BOOST_CHECK_EQUAL(fx2lafw_transfer_buffer_size(0), 0U);
    BOOST_CHECK_EQUAL(fx2lafw_transfer_count(0), 0U);
    BOOST_CHECK_EQUAL(fx2lafw_transfer_timeout_ms(0), 0U);
#else
    BOOST_TEST_MESSAGE("upstream fx2lafw is disabled for this build");
#endif
}
```

- [ ] **Step 2: Declare acquisition constants and helpers**

In `libsigrok/hardware/upstream-fx2lafw/fx2lafw.h`, add these definitions after the existing `FX2LAFW_CMD_GET_FW_VERSION` line:

```c
#define FX2LAFW_CMD_START 0xb1
#define FX2LAFW_BULK_ENDPOINT (2 | LIBUSB_ENDPOINT_IN)
#define FX2LAFW_NUM_SIMUL_TRANSFERS 32
#define FX2LAFW_MAX_EMPTY_TRANSFERS (FX2LAFW_NUM_SIMUL_TRANSFERS * 2)
#define FX2LAFW_MAX_16BIT_SAMPLE_RATE SR_MHZ(12)
#define FX2LAFW_MAX_SAMPLE_DELAY (6 * 256)
#define FX2LAFW_CMD_START_FLAGS_SAMPLE_8BIT (0 << 5)
#define FX2LAFW_CMD_START_FLAGS_SAMPLE_16BIT (1 << 5)
#define FX2LAFW_CMD_START_FLAGS_CLK_30MHZ (0 << 6)
#define FX2LAFW_CMD_START_FLAGS_CLK_48MHZ (1 << 6)
```

Add this packed command struct after `struct fx2lafw_profile`:

```c
#pragma pack(push, 1)
struct fx2lafw_start_command {
	uint8_t flags;
	uint8_t sample_delay_h;
	uint8_t sample_delay_l;
};
#pragma pack(pop)
```

Add these declarations before `extern SR_PRIV struct sr_dev_driver fx2lafw_driver_info;`:

```c
SR_PRIV struct sr_dev_inst *fx2lafw_dev_inst_new_for_profile(
	const struct fx2lafw_profile *profile, uint8_t bus, uint8_t address,
	int status, gboolean firmware_loaded, gint64 fw_updated);
SR_PRIV uint16_t fx2lafw_enabled_channel_mask(const struct sr_dev_inst *sdi);
SR_PRIV gboolean fx2lafw_sample_wide_for_channels(const struct sr_dev_inst *sdi);
SR_PRIV int fx2lafw_build_start_command(uint64_t samplerate,
	gboolean sample_wide, struct fx2lafw_start_command *command);
SR_PRIV size_t fx2lafw_transfer_buffer_size(uint64_t samplerate);
SR_PRIV unsigned int fx2lafw_transfer_count(uint64_t samplerate);
SR_PRIV unsigned int fx2lafw_transfer_timeout_ms(uint64_t samplerate);
```

- [ ] **Step 3: Verify RED**

Run:

```bash
cmake . -DENABLE_TESTS=ON -DDSVIEW_ENABLE_UPSTREAM_COMPAT_DEMO=ON -DDSVIEW_ENABLE_UPSTREAM_FX2LAFW=ON
make DSView-test -j$(sysctl -n hw.ncpu 2>/dev/null || echo 8)
```

Expected: FAIL because `fx2lafw_dev_inst_new_for_profile`, acquisition command constants, and helper functions are declared or referenced but not implemented.

- [ ] **Step 4: Rename and expose the test-device factory**

In `libsigrok/hardware/upstream-fx2lafw/fx2lafw.c`, change:

```c
static struct sr_dev_inst *create_device_from_profile(
	const struct fx2lafw_profile *profile, uint8_t bus, uint8_t address,
	int status, gboolean firmware_loaded, gint64 fw_updated)
```

to:

```c
SR_PRIV struct sr_dev_inst *fx2lafw_dev_inst_new_for_profile(
	const struct fx2lafw_profile *profile, uint8_t bus, uint8_t address,
	int status, gboolean firmware_loaded, gint64 fw_updated)
```

Then update the call in `hw_scan()`:

```c
struct sr_dev_inst *sdi = fx2lafw_dev_inst_new_for_profile(profile,
	libusb_get_bus_number(devlist[i]), address, status, has_firmware,
	fw_updated);
```

- [ ] **Step 5: Implement helper functions**

Add these functions after `fx2lafw_profile_channel_count()`:

```c
SR_PRIV uint16_t fx2lafw_enabled_channel_mask(const struct sr_dev_inst *sdi)
{
	GSList *l;
	uint16_t mask;

	if (!sdi)
		return 0;

	mask = 0;
	for (l = sdi->channels; l; l = l->next) {
		struct sr_channel *channel = l->data;
		if (channel && channel->enabled && channel->index < 16)
			mask |= (uint16_t)(1U << channel->index);
	}

	return mask;
}

SR_PRIV gboolean fx2lafw_sample_wide_for_channels(const struct sr_dev_inst *sdi)
{
	return (fx2lafw_enabled_channel_mask(sdi) & 0xff00) != 0;
}

SR_PRIV int fx2lafw_build_start_command(uint64_t samplerate,
	gboolean sample_wide, struct fx2lafw_start_command *command)
{
	uint64_t delay;
	gboolean delay_valid;

	if (!command)
		return SR_ERR_ARG;
	memset(command, 0, sizeof(*command));

	if (samplerate == 0)
		return SR_ERR;
	if (sample_wide && samplerate > FX2LAFW_MAX_16BIT_SAMPLE_RATE)
		return SR_ERR;

	delay = 0;
	delay_valid = FALSE;
	if (SR_MHZ(48) % samplerate == 0) {
		delay = SR_MHZ(48) / samplerate - 1;
		if (delay <= FX2LAFW_MAX_SAMPLE_DELAY) {
			command->flags = FX2LAFW_CMD_START_FLAGS_CLK_48MHZ;
			delay_valid = TRUE;
		}
	}

	if (!delay_valid && SR_MHZ(30) % samplerate == 0) {
		delay = SR_MHZ(30) / samplerate - 1;
		command->flags = FX2LAFW_CMD_START_FLAGS_CLK_30MHZ;
		delay_valid = TRUE;
	}

	if (!delay_valid || delay > FX2LAFW_MAX_SAMPLE_DELAY)
		return SR_ERR;

	command->sample_delay_h = (delay >> 8) & 0xff;
	command->sample_delay_l = delay & 0xff;
	command->flags |= sample_wide ?
		FX2LAFW_CMD_START_FLAGS_SAMPLE_16BIT :
		FX2LAFW_CMD_START_FLAGS_SAMPLE_8BIT;

	return SR_OK;
}

static unsigned int fx2lafw_bytes_per_ms(uint64_t samplerate)
{
	if (samplerate == 0)
		return 0;

	return samplerate / 1000;
}

SR_PRIV size_t fx2lafw_transfer_buffer_size(uint64_t samplerate)
{
	size_t size;
	unsigned int bytes_per_ms;

	bytes_per_ms = fx2lafw_bytes_per_ms(samplerate);
	if (bytes_per_ms == 0)
		return 0;

	size = 10 * bytes_per_ms;
	return (size + 511) & ~((size_t)511);
}

SR_PRIV unsigned int fx2lafw_transfer_count(uint64_t samplerate)
{
	size_t buffer_size;
	unsigned int bytes_per_ms;
	unsigned int count;

	bytes_per_ms = fx2lafw_bytes_per_ms(samplerate);
	buffer_size = fx2lafw_transfer_buffer_size(samplerate);
	if (bytes_per_ms == 0 || buffer_size == 0)
		return 0;

	count = (500 * bytes_per_ms) / buffer_size;
	if (count == 0)
		count = 1;
	if (count > FX2LAFW_NUM_SIMUL_TRANSFERS)
		count = FX2LAFW_NUM_SIMUL_TRANSFERS;

	return count;
}

SR_PRIV unsigned int fx2lafw_transfer_timeout_ms(uint64_t samplerate)
{
	size_t total_size;
	unsigned int bytes_per_ms;
	unsigned int timeout;

	bytes_per_ms = fx2lafw_bytes_per_ms(samplerate);
	if (bytes_per_ms == 0)
		return 0;

	total_size = fx2lafw_transfer_buffer_size(samplerate) *
		fx2lafw_transfer_count(samplerate);
	timeout = total_size / bytes_per_ms;
	return timeout + timeout / 4;
}
```

- [ ] **Step 6: Verify GREEN**

Run:

```bash
make DSView-test -j$(sysctl -n hw.ncpu 2>/dev/null || echo 8)
./build.macOS/DSView-test --run_test=upstream_fx2lafw/acquisition_helpers*
./build.macOS/DSView-test --run_test=upstream_fx2lafw/acquisition_start_command*
./build.macOS/DSView-test --run_test=upstream_fx2lafw/acquisition_transfer_sizing*
./build.macOS/DSView-test --run_test=upstream_fx2lafw
```

Expected: PASS. The upstream fx2lafw suite should now include 14 tests.

- [ ] **Step 7: Commit**

```bash
git add PXTOOL/test/test_upstream_fx2lafw.cpp libsigrok/hardware/upstream-fx2lafw/fx2lafw.h libsigrok/hardware/upstream-fx2lafw/fx2lafw.c
git commit -m "feat: add fx2lafw acquisition helpers"
```

## Task 2: Logic Datafeed Adapter

**Files:**
- Create: `PXTOOL/test/test_datafeed_stub.h`
- Create: `PXTOOL/test/test_datafeed_stub.cpp`
- Modify: `PXTOOL/test/CMakeLists.txt`
- Modify: `PXTOOL/test/test_upstream_demo.cpp`
- Modify: `libsigrok/hardware/upstream-fx2lafw/fx2lafw.h`
- Modify: `libsigrok/hardware/upstream-fx2lafw/fx2lafw.c`
- Modify: `PXTOOL/test/test_upstream_fx2lafw.cpp`

**Interfaces:**
- Consumes: `ds_data_forward()`, `struct sr_datafeed_logic`, `struct sr_datafeed_packet`, `LA_CROSS_DATA`.
- Produces:
  - `SR_PRIV int fx2lafw_send_logic_packet(const struct sr_dev_inst *sdi, const uint8_t *data, size_t length, size_t unitsize);`
  - shared test helper `test_datafeed_reset()`
  - shared test helper `test_datafeed_last_packet()`

- [ ] **Step 1: Create a shared test datafeed stub**

Create `PXTOOL/test/test_datafeed_stub.h`:

```cpp
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

#include <stdint.h>

struct test_captured_datafeed_packet {
    uint16_t type;
    uint16_t status;
    uint64_t logic_length;
    int logic_format;
    uint16_t logic_unitsize;
    const void *logic_data;
};

void test_datafeed_reset(void);
const test_captured_datafeed_packet *test_datafeed_last_packet(void);

#endif
```

Create `PXTOOL/test/test_datafeed_stub.cpp`:

```cpp
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

#include "test_datafeed_stub.h"

#include <string.h>

extern "C" {
#include "libsigrok-internal.h"
}

static test_captured_datafeed_packet g_captured_packet;

void test_datafeed_reset(void)
{
    memset(&g_captured_packet, 0, sizeof(g_captured_packet));
}

const test_captured_datafeed_packet *test_datafeed_last_packet(void)
{
    return &g_captured_packet;
}

extern "C" int ds_data_forward(const struct sr_dev_inst *sdi,
    const struct sr_datafeed_packet *packet)
{
    (void)sdi;
    test_datafeed_reset();

    if (!packet)
        return SR_ERR_ARG;

    g_captured_packet.type = packet->type;
    g_captured_packet.status = packet->status;
    if (packet->type == SR_DF_LOGIC && packet->payload) {
        const struct sr_datafeed_logic *logic =
            static_cast<const struct sr_datafeed_logic *>(packet->payload);
        g_captured_packet.logic_length = logic->length;
        g_captured_packet.logic_format = logic->format;
        g_captured_packet.logic_unitsize = logic->unitsize;
        g_captured_packet.logic_data = logic->data;
    }

    return SR_OK;
}
```

- [ ] **Step 2: Wire the shared stub into tests**

In `PXTOOL/test/CMakeLists.txt`, add `test_datafeed_stub.cpp` to `DSView-test`:

```cmake
add_executable(DSView-test
    test.cpp
    test_channeltint.cpp
    test_datafeed_stub.cpp
    test_deviceagent_capability.cpp
```

In `PXTOOL/test/test_upstream_demo.cpp`, delete this local stub:

```cpp
#ifdef HAVE_UPSTREAM_COMPAT_DEMO
extern "C" int ds_data_forward(const struct sr_dev_inst *,
    const struct sr_datafeed_packet *)
{
    return SR_OK;
}
#endif
```

In `PXTOOL/test/test_upstream_fx2lafw.cpp`, add:

```cpp
#include "test_datafeed_stub.h"
```

- [ ] **Step 3: Add failing logic packet tests**

Append this test before `BOOST_AUTO_TEST_SUITE_END()`:

```cpp
BOOST_AUTO_TEST_CASE(logic_packet_adapter_forwards_cross_data)
{
#ifdef HAVE_UPSTREAM_FX2LAFW
    const fx2lafw_profile *profile = fx2lafw_profile_find(
        0x0925, 0x3881, "", "");
    BOOST_REQUIRE(profile != nullptr);

    sr_dev_inst *sdi = fx2lafw_dev_inst_new_for_profile(
        profile, 1, 2, SR_ST_ACTIVE, TRUE, 0);
    BOOST_REQUIRE(sdi != nullptr);

    uint8_t data[] = {0x12, 0x34, 0x56, 0x78};
    test_datafeed_reset();
    BOOST_REQUIRE_EQUAL(fx2lafw_send_logic_packet(sdi, data, sizeof(data), 1), SR_OK);
    const test_captured_datafeed_packet *packet = test_datafeed_last_packet();

    BOOST_CHECK_EQUAL(packet->type, SR_DF_LOGIC);
    BOOST_CHECK_EQUAL(packet->status, SR_PKT_OK);
    BOOST_CHECK_EQUAL(packet->logic_length, sizeof(data));
    BOOST_CHECK_EQUAL(packet->logic_format, LA_CROSS_DATA);
    BOOST_CHECK_EQUAL(packet->logic_unitsize, 1);
    BOOST_CHECK_EQUAL(packet->logic_data, data);

    BOOST_CHECK_EQUAL(fx2lafw_send_logic_packet(sdi, data, sizeof(data), 2), SR_OK);
    packet = test_datafeed_last_packet();
    BOOST_CHECK_EQUAL(packet->logic_unitsize, 2);

    BOOST_CHECK_EQUAL(fx2lafw_send_logic_packet(nullptr, data, sizeof(data), 1), SR_ERR_ARG);
    BOOST_CHECK_EQUAL(fx2lafw_send_logic_packet(sdi, nullptr, sizeof(data), 1), SR_ERR_ARG);
    BOOST_CHECK_EQUAL(fx2lafw_send_logic_packet(sdi, data, 0, 1), SR_ERR_ARG);
    BOOST_CHECK_EQUAL(fx2lafw_send_logic_packet(sdi, data, sizeof(data), 0), SR_ERR_ARG);

    sr_dev_inst_free(sdi);
#else
    BOOST_TEST_MESSAGE("upstream fx2lafw is disabled for this build");
#endif
}
```

- [ ] **Step 4: Declare the adapter**

In `libsigrok/hardware/upstream-fx2lafw/fx2lafw.h`, add:

```c
SR_PRIV int fx2lafw_send_logic_packet(const struct sr_dev_inst *sdi,
	const uint8_t *data, size_t length, size_t unitsize);
```

- [ ] **Step 5: Verify RED**

Run:

```bash
make DSView-test -j$(sysctl -n hw.ncpu 2>/dev/null || echo 8)
```

Expected: FAIL because `fx2lafw_send_logic_packet()` is declared or referenced but not implemented.

- [ ] **Step 6: Implement the adapter**

In `libsigrok/hardware/upstream-fx2lafw/fx2lafw.c`, add this function after the transfer sizing helpers:

```c
SR_PRIV int fx2lafw_send_logic_packet(const struct sr_dev_inst *sdi,
	const uint8_t *data, size_t length, size_t unitsize)
{
	struct sr_datafeed_logic logic;
	struct sr_datafeed_packet packet;

	if (!sdi || !data || length == 0 || unitsize == 0)
		return SR_ERR_ARG;

	memset(&logic, 0, sizeof(logic));
	memset(&packet, 0, sizeof(packet));

	logic.length = length;
	logic.format = LA_CROSS_DATA;
	logic.unitsize = unitsize;
	logic.data = (void *)data;

	packet.type = SR_DF_LOGIC;
	packet.status = SR_PKT_OK;
	packet.payload = &logic;

	return ds_data_forward(sdi, &packet);
}
```

- [ ] **Step 7: Verify GREEN**

Run:

```bash
make DSView-test -j$(sysctl -n hw.ncpu 2>/dev/null || echo 8)
./build.macOS/DSView-test --run_test=upstream_fx2lafw/logic_packet_adapter_forwards_cross_data
./build.macOS/DSView-test --run_test=upstream_fx2lafw
```

Expected: PASS. The upstream fx2lafw suite should now include 15 tests.

- [ ] **Step 8: Commit**

```bash
git add PXTOOL/test/CMakeLists.txt PXTOOL/test/test_datafeed_stub.h PXTOOL/test/test_datafeed_stub.cpp PXTOOL/test/test_upstream_demo.cpp PXTOOL/test/test_upstream_fx2lafw.cpp libsigrok/hardware/upstream-fx2lafw/fx2lafw.h libsigrok/hardware/upstream-fx2lafw/fx2lafw.c
git commit -m "feat: add fx2lafw logic datafeed adapter"
```

## Task 3: Acquisition Callback Wiring and Start Command

**Files:**
- Modify: `libsigrok/hardware/upstream-fx2lafw/fx2lafw.c`
- Modify: `PXTOOL/test/test_upstream_fx2lafw.cpp`
- Modify: `PXTOOL/test/CMakeLists.txt`

**Interfaces:**
- Consumes: `fx2lafw_build_start_command()`, `fx2lafw_sample_wide_for_channels()`, `libusb_control_transfer()`.
- Produces:
  - `static int command_start_acquisition(const struct sr_dev_inst *sdi)`
  - `static int hw_dev_acquisition_start(struct sr_dev_inst *sdi, void *cb_data)`
  - `static int hw_dev_acquisition_stop(const struct sr_dev_inst *sdi, void *cb_data)`
  - `fx2lafw_driver_info.dev_acquisition_start`
  - `fx2lafw_driver_info.dev_acquisition_stop`

- [ ] **Step 1: Add failing callback tests**

Append these tests before `BOOST_AUTO_TEST_SUITE_END()`:

```cpp
BOOST_AUTO_TEST_CASE(driver_exposes_acquisition_lifecycle)
{
#ifdef HAVE_UPSTREAM_FX2LAFW
    BOOST_REQUIRE(fx2lafw_driver_info.dev_acquisition_start != nullptr);
    BOOST_REQUIRE(fx2lafw_driver_info.dev_acquisition_stop != nullptr);
#else
    BOOST_TEST_MESSAGE("upstream fx2lafw is disabled for this build");
#endif
}

BOOST_AUTO_TEST_CASE(acquisition_start_requires_active_open_device)
{
#ifdef HAVE_UPSTREAM_FX2LAFW
    const fx2lafw_profile *profile = fx2lafw_profile_find(
        0x0925, 0x3881, "", "");
    BOOST_REQUIRE(profile != nullptr);

    sr_dev_inst *sdi = fx2lafw_dev_inst_new_for_profile(
        profile, 1, 2, SR_ST_INACTIVE, TRUE, 0);
    BOOST_REQUIRE(sdi != nullptr);

    BOOST_REQUIRE(fx2lafw_driver_info.dev_acquisition_start != nullptr);
    BOOST_CHECK_EQUAL(fx2lafw_driver_info.dev_acquisition_start(sdi, sdi),
        SR_ERR_DEVICE_CLOSED);

    sr_dev_inst_free(sdi);
#else
    BOOST_TEST_MESSAGE("upstream fx2lafw is disabled for this build");
#endif
}

BOOST_AUTO_TEST_CASE(acquisition_stop_without_running_is_safe)
{
#ifdef HAVE_UPSTREAM_FX2LAFW
    const fx2lafw_profile *profile = fx2lafw_profile_find(
        0x0925, 0x3881, "", "");
    BOOST_REQUIRE(profile != nullptr);

    sr_dev_inst *sdi = fx2lafw_dev_inst_new_for_profile(
        profile, 1, 2, SR_ST_ACTIVE, TRUE, 0);
    BOOST_REQUIRE(sdi != nullptr);

    BOOST_REQUIRE(fx2lafw_driver_info.dev_acquisition_stop != nullptr);
    BOOST_CHECK_EQUAL(fx2lafw_driver_info.dev_acquisition_stop(sdi, sdi), SR_OK);

    sr_dev_inst_free(sdi);
#else
    BOOST_TEST_MESSAGE("upstream fx2lafw is disabled for this build");
#endif
}
```

- [ ] **Step 2: Verify RED**

Run:

```bash
make DSView-test -j$(sysctl -n hw.ncpu 2>/dev/null || echo 8)
./build.macOS/DSView-test --run_test=upstream_fx2lafw/driver_exposes_acquisition_lifecycle
```

Expected: FAIL because the driver does not expose acquisition callbacks yet.

- [ ] **Step 3: Extend acquisition state**

In `struct fx2lafw_context`, add:

```c
	gboolean acquisition_running;
	gboolean acquisition_aborted;
	gboolean sample_wide;
	uint64_t sent_samples;
	unsigned int submitted_transfers;
	unsigned int num_transfers;
	unsigned int empty_transfer_count;
	struct libusb_transfer **transfers;
	gintptr event_source;
	gboolean event_source_added;
	gboolean end_sent;
```

In `fx2lafw_dev_inst_new_for_profile()`, initialize:

```c
	devc->event_source = -2;
```

- [ ] **Step 4: Implement start command and callback skeletons**

Add this helper after `command_get_fw_version()`:

```c
static int command_start_acquisition(const struct sr_dev_inst *sdi)
{
	struct fx2lafw_context *devc;
	struct sr_usb_dev_inst *usb;
	struct fx2lafw_start_command command;
	int ret;

	if (!sdi || !sdi->priv || !sdi->conn)
		return SR_ERR_ARG;

	devc = sdi->priv;
	usb = sdi->conn;
	if (!usb->devhdl)
		return SR_ERR_DEVICE_CLOSED;

	ret = fx2lafw_build_start_command(devc->samplerate,
		devc->sample_wide, &command);
	if (ret != SR_OK)
		return ret;

	ret = libusb_control_transfer(usb->devhdl,
		LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_OUT,
		FX2LAFW_CMD_START, 0x0000, 0x0000,
		(unsigned char *)&command, sizeof(command),
		FX2LAFW_USB_TIMEOUT_MS);
	if (ret < 0) {
		sr_err("Unable to send fx2lafw start command: %s.",
			libusb_error_name(ret));
		return SR_ERR;
	}
	if (ret != (int)sizeof(command)) {
		sr_err("Short fx2lafw start command write: %d bytes.", ret);
		return SR_ERR;
	}

	return SR_OK;
}
```

Add these skeleton callbacks before `fx2lafw_driver_info`:

```c
static int hw_dev_acquisition_start(struct sr_dev_inst *sdi, void *cb_data)
{
	struct fx2lafw_context *devc;

	(void)cb_data;

	if (!sdi || !sdi->priv || !sdi->conn)
		return SR_ERR_ARG;
	if (sdi->status != SR_ST_ACTIVE) {
		ds_set_last_error(SR_ERR_DEVICE_CLOSED);
		return SR_ERR_DEVICE_CLOSED;
	}

	devc = sdi->priv;
	devc->sample_wide = fx2lafw_sample_wide_for_channels(sdi);
	devc->sent_samples = 0;
	devc->empty_transfer_count = 0;
	devc->acquisition_aborted = FALSE;
	devc->end_sent = FALSE;

	return command_start_acquisition(sdi);
}

static int hw_dev_acquisition_stop(const struct sr_dev_inst *sdi, void *cb_data)
{
	(void)cb_data;

	if (!sdi || !sdi->priv)
		return SR_ERR_ARG;

	return SR_OK;
}
```

Wire them into `fx2lafw_driver_info`:

```c
		.dev_acquisition_start = hw_dev_acquisition_start,
		.dev_acquisition_stop = hw_dev_acquisition_stop,
```

- [ ] **Step 5: Verify GREEN for callback tests**

Run:

```bash
make DSView-test -j$(sysctl -n hw.ncpu 2>/dev/null || echo 8)
./build.macOS/DSView-test --run_test=upstream_fx2lafw/driver_exposes_acquisition_lifecycle
./build.macOS/DSView-test --run_test=upstream_fx2lafw/acquisition_start_requires_active_open_device
./build.macOS/DSView-test --run_test=upstream_fx2lafw/acquisition_stop_without_running_is_safe
./build.macOS/DSView-test --run_test=upstream_fx2lafw
```

Expected: PASS. The active-device hardware path is not exercised by these tests because test devices have no `devhdl`; the closed-device and stop-without-running behavior is verified.

- [ ] **Step 6: Commit**

```bash
git add PXTOOL/test/test_upstream_fx2lafw.cpp libsigrok/hardware/upstream-fx2lafw/fx2lafw.c
git commit -m "feat: wire fx2lafw acquisition callbacks"
```

## Task 4: Async Transfer Lifecycle

**Files:**
- Modify: `libsigrok/hardware/upstream-fx2lafw/fx2lafw.c`
- Modify: `PXTOOL/test/test_upstream_fx2lafw.cpp`

**Interfaces:**
- Consumes: `fx2lafw_transfer_*()` helpers, `fx2lafw_send_logic_packet()`, `std_session_send_df_header()`, `ds_data_forward()`, `sr_session_source_add()`, `sr_session_source_remove()`, libusb async transfer API.
- Produces:
  - submitted libusb bulk transfers on `FX2LAFW_BULK_ENDPOINT`
  - transfer callback that forwards logic samples and respects `limit_samples`
  - idempotent abort/finish cleanup

- [ ] **Step 1: Add lifecycle edge tests**

Append this test before `BOOST_AUTO_TEST_SUITE_END()`:

```cpp
BOOST_AUTO_TEST_CASE(acquisition_stop_is_idempotent_for_test_device)
{
#ifdef HAVE_UPSTREAM_FX2LAFW
    const fx2lafw_profile *profile = fx2lafw_profile_find(
        0x1d50, 0x608d, "sigrok", "fx2lafw");
    BOOST_REQUIRE(profile != nullptr);

    sr_dev_inst *sdi = fx2lafw_dev_inst_new_for_profile(
        profile, 1, 2, SR_ST_ACTIVE, TRUE, 0);
    BOOST_REQUIRE(sdi != nullptr);

    BOOST_CHECK_EQUAL(fx2lafw_driver_info.dev_acquisition_stop(sdi, sdi), SR_OK);
    BOOST_CHECK_EQUAL(fx2lafw_driver_info.dev_acquisition_stop(sdi, sdi), SR_OK);

    sr_dev_inst_free(sdi);
#else
    BOOST_TEST_MESSAGE("upstream fx2lafw is disabled for this build");
#endif
}
```

- [ ] **Step 2: Verify RED or current PASS before implementation**

Run:

```bash
make DSView-test -j$(sysctl -n hw.ncpu 2>/dev/null || echo 8)
./build.macOS/DSView-test --run_test=upstream_fx2lafw/acquisition_stop_is_idempotent_for_test_device
```

Expected: PASS if Task 3 already made stop idempotent for non-running devices. Record this as a characterization test in the report. If it fails, keep it as RED and fix in Step 4.

- [ ] **Step 3: Add finish, abort, event, and transfer helpers**

In `PXTOOL/test/CMakeLists.txt`, add `../../libsigrok/std.c` to the `DSVIEW_ENABLE_UPSTREAM_FX2LAFW` source block because `fx2lafw` now calls `std_session_send_df_header()` in tests:

```cmake
if(DSVIEW_ENABLE_UPSTREAM_FX2LAFW)
    target_sources(DSView-test PRIVATE
        ../../libsigrok/hardware/upstream-fx2lafw/fx2lafw.c
        ../../libsigrok/hardware/common/ezusb.c
        ../../libsigrok/std.c
    )
endif()
```

In `libsigrok/hardware/upstream-fx2lafw/fx2lafw.c`, add these helpers before `hw_dev_acquisition_start()`:

```c
static void fx2lafw_send_end_once(struct sr_dev_inst *sdi)
{
	struct fx2lafw_context *devc;
	struct sr_datafeed_packet packet;

	if (!sdi || !sdi->priv)
		return;

	devc = sdi->priv;
	if (devc->end_sent)
		return;

	memset(&packet, 0, sizeof(packet));
	packet.type = SR_DF_END;
	packet.status = SR_PKT_OK;
	packet.payload = NULL;
	ds_data_forward(sdi, &packet);
	devc->end_sent = TRUE;
}

static void fx2lafw_remove_event_source(struct fx2lafw_context *devc)
{
	if (!devc || !devc->event_source_added)
		return;

	sr_session_source_remove(devc->event_source);
	devc->event_source_added = FALSE;
}

static void fx2lafw_finish_acquisition(struct sr_dev_inst *sdi)
{
	struct fx2lafw_context *devc;

	if (!sdi || !sdi->priv)
		return;

	devc = sdi->priv;
	fx2lafw_remove_event_source(devc);
	fx2lafw_send_end_once(sdi);
	devc->acquisition_running = FALSE;
	devc->num_transfers = 0;
	g_free(devc->transfers);
	devc->transfers = NULL;
}

static void fx2lafw_free_transfer(struct libusb_transfer *transfer)
{
	struct sr_dev_inst *sdi;
	struct fx2lafw_context *devc;
	unsigned int i;

	if (!transfer)
		return;

	sdi = transfer->user_data;
	devc = sdi ? sdi->priv : NULL;

	if (devc) {
		for (i = 0; i < devc->num_transfers; i++) {
			if (devc->transfers && devc->transfers[i] == transfer) {
				devc->transfers[i] = NULL;
				break;
			}
		}
		if (devc->submitted_transfers > 0)
			devc->submitted_transfers--;
	}

	g_free(transfer->buffer);
	transfer->buffer = NULL;
	libusb_free_transfer(transfer);

	if (sdi && devc && devc->submitted_transfers == 0)
		fx2lafw_finish_acquisition(sdi);
}

static void fx2lafw_abort_acquisition(struct fx2lafw_context *devc)
{
	unsigned int i;

	if (!devc || devc->acquisition_aborted)
		return;

	devc->acquisition_aborted = TRUE;
	for (i = 0; i < devc->num_transfers; i++) {
		if (devc->transfers && devc->transfers[i])
			libusb_cancel_transfer(devc->transfers[i]);
	}
}

static void fx2lafw_resubmit_transfer(struct libusb_transfer *transfer)
{
	int ret;

	ret = libusb_submit_transfer(transfer);
	if (ret == LIBUSB_SUCCESS)
		return;

	sr_err("Unable to resubmit fx2lafw transfer: %s.", libusb_error_name(ret));
	fx2lafw_free_transfer(transfer);
}

static void LIBUSB_CALL fx2lafw_receive_transfer(struct libusb_transfer *transfer)
{
	struct sr_dev_inst *sdi;
	struct fx2lafw_context *devc;
	size_t unitsize;
	size_t sample_count;
	size_t bytes_to_send;
	gboolean packet_has_error;

	sdi = transfer ? transfer->user_data : NULL;
	devc = sdi ? sdi->priv : NULL;
	if (!transfer || !sdi || !devc)
		return;

	if (devc->acquisition_aborted) {
		fx2lafw_free_transfer(transfer);
		return;
	}

	packet_has_error = FALSE;
	switch (transfer->status) {
	case LIBUSB_TRANSFER_NO_DEVICE:
		fx2lafw_abort_acquisition(devc);
		fx2lafw_free_transfer(transfer);
		return;
	case LIBUSB_TRANSFER_COMPLETED:
	case LIBUSB_TRANSFER_TIMED_OUT:
		break;
	default:
		packet_has_error = TRUE;
		break;
	}

	if (transfer->actual_length == 0 || packet_has_error) {
		devc->empty_transfer_count++;
		if (devc->empty_transfer_count > FX2LAFW_MAX_EMPTY_TRANSFERS) {
			fx2lafw_abort_acquisition(devc);
			fx2lafw_free_transfer(transfer);
		} else {
			fx2lafw_resubmit_transfer(transfer);
		}
		return;
	}

	devc->empty_transfer_count = 0;
	unitsize = devc->sample_wide ? 2 : 1;
	sample_count = transfer->actual_length / unitsize;
	if (sample_count == 0) {
		fx2lafw_resubmit_transfer(transfer);
		return;
	}

	if (devc->limit_samples &&
			devc->sent_samples + sample_count > devc->limit_samples)
		sample_count = devc->limit_samples - devc->sent_samples;
	bytes_to_send = sample_count * unitsize;

	if (bytes_to_send > 0) {
		fx2lafw_send_logic_packet(sdi, transfer->buffer,
			bytes_to_send, unitsize);
		devc->sent_samples += sample_count;
	}

	if (devc->limit_samples && devc->sent_samples >= devc->limit_samples) {
		fx2lafw_abort_acquisition(devc);
		fx2lafw_free_transfer(transfer);
	} else {
		fx2lafw_resubmit_transfer(transfer);
	}
}

static int fx2lafw_handle_events(int fd, int revents, void *cb_data)
{
	struct sr_dev_inst *sdi;
	struct fx2lafw_driver_context *drvc;
	struct timeval tv;

	(void)fd;
	(void)revents;

	sdi = cb_data;
	if (!sdi || !sdi->priv)
		return FALSE;

	drvc = fx2lafw_driver_info.priv;
	if (!drvc || !drvc->libusb_ctx)
		return FALSE;

	tv.tv_sec = 0;
	tv.tv_usec = 0;
	libusb_handle_events_timeout(drvc->libusb_ctx, &tv);

	return TRUE;
}
```

- [ ] **Step 4: Add transfer submission helper**

Add this function after the helpers from Step 3:

```c
static int fx2lafw_start_transfers(struct sr_dev_inst *sdi)
{
	struct fx2lafw_context *devc;
	struct sr_usb_dev_inst *usb;
	unsigned int num_transfers;
	unsigned int timeout;
	size_t size;
	unsigned int i;

	if (!sdi || !sdi->priv || !sdi->conn)
		return SR_ERR_ARG;

	devc = sdi->priv;
	usb = sdi->conn;
	if (!usb->devhdl)
		return SR_ERR_DEVICE_CLOSED;

	size = fx2lafw_transfer_buffer_size(devc->samplerate);
	num_transfers = fx2lafw_transfer_count(devc->samplerate);
	timeout = fx2lafw_transfer_timeout_ms(devc->samplerate);
	if (size == 0 || num_transfers == 0 || timeout == 0)
		return SR_ERR;

	devc->transfers = g_try_malloc0(sizeof(*devc->transfers) * num_transfers);
	if (!devc->transfers)
		return SR_ERR_MALLOC;

	devc->num_transfers = num_transfers;
	devc->submitted_transfers = 0;
	for (i = 0; i < num_transfers; i++) {
		struct libusb_transfer *transfer;
		unsigned char *buffer;
		int ret;

		buffer = g_try_malloc(size);
		if (!buffer) {
			fx2lafw_abort_acquisition(devc);
			return SR_ERR_MALLOC;
		}

		transfer = libusb_alloc_transfer(0);
		if (!transfer) {
			g_free(buffer);
			fx2lafw_abort_acquisition(devc);
			return SR_ERR_MALLOC;
		}

		libusb_fill_bulk_transfer(transfer, usb->devhdl,
			FX2LAFW_BULK_ENDPOINT, buffer, size,
			fx2lafw_receive_transfer, sdi, timeout);
		ret = libusb_submit_transfer(transfer);
		if (ret != LIBUSB_SUCCESS) {
			sr_err("Unable to submit fx2lafw transfer: %s.",
				libusb_error_name(ret));
			g_free(buffer);
			libusb_free_transfer(transfer);
			fx2lafw_abort_acquisition(devc);
			return SR_ERR;
		}

		devc->transfers[i] = transfer;
		devc->submitted_transfers++;
	}

	return SR_OK;
}
```

- [ ] **Step 5: Replace the start/stop skeletons with full lifecycle**

In `hw_dev_acquisition_start()`, replace the final `return command_start_acquisition(sdi);` with:

```c
	int ret;
	unsigned int timeout;

	timeout = fx2lafw_transfer_timeout_ms(devc->samplerate);
	if (timeout == 0)
		return SR_ERR;

	devc->event_source = -2;
	ret = sr_session_source_add(devc->event_source, 0, timeout,
		fx2lafw_handle_events, sdi);
	if (ret != SR_OK)
		return ret;
	devc->event_source_added = TRUE;

	ret = fx2lafw_start_transfers(sdi);
	if (ret != SR_OK) {
		fx2lafw_remove_event_source(devc);
		return ret;
	}

	ret = std_session_send_df_header(sdi, LOG_PREFIX);
	if (ret != SR_OK) {
		fx2lafw_abort_acquisition(devc);
		return ret;
	}

	ret = command_start_acquisition(sdi);
	if (ret != SR_OK) {
		fx2lafw_abort_acquisition(devc);
		return ret;
	}

	devc->acquisition_running = TRUE;
	return SR_OK;
```

In `hw_dev_acquisition_stop()`, replace the body with:

```c
	struct fx2lafw_context *devc;

	(void)cb_data;

	if (!sdi || !sdi->priv)
		return SR_ERR_ARG;

	devc = sdi->priv;
	if (!devc->acquisition_running && devc->submitted_transfers == 0) {
		fx2lafw_remove_event_source(devc);
		return SR_OK;
	}

	fx2lafw_abort_acquisition(devc);
	if (devc->submitted_transfers == 0)
		fx2lafw_finish_acquisition((struct sr_dev_inst *)sdi);

	return SR_OK;
```

- [ ] **Step 6: Ensure close stops running acquisition first**

At the beginning of `hw_dev_close()` after validating `sdi`, add:

```c
	if (sdi->priv)
		hw_dev_acquisition_stop(sdi, sdi);
```

- [ ] **Step 7: Verify GREEN**

Run:

```bash
make DSView-test -j$(sysctl -n hw.ncpu 2>/dev/null || echo 8)
./build.macOS/DSView-test --run_test=upstream_fx2lafw/acquisition_stop_is_idempotent_for_test_device
./build.macOS/DSView-test --run_test=upstream_fx2lafw
```

Expected: PASS. The upstream fx2lafw suite should now include 19 tests.

- [ ] **Step 8: Commit**

```bash
git add PXTOOL/test/CMakeLists.txt PXTOOL/test/test_upstream_fx2lafw.cpp libsigrok/hardware/upstream-fx2lafw/fx2lafw.c
git commit -m "feat: stream fx2lafw logic transfers"
```

## Task 5: Documentation and Inventory

**Files:**
- Modify: `docs/fx2lafw-hardware-smoke-test.md`
- Modify: `docs/libsigrok-upstream-driver-inventory.md`

**Interfaces:**
- Consumes: acquisition behavior from Tasks 1 through 4.
- Produces: manual acquisition smoke gate and updated roadmap state.

- [ ] **Step 1: Update hardware smoke checklist**

In `docs/fx2lafw-hardware-smoke-test.md`, add this section before `## Stop Conditions`:

```markdown
## Acquisition

1. Complete either the firmware-loaded or bootloader-state open/close checklist above.
2. Keep all logic channels enabled for the first run.
3. Set a low samplerate such as `1 MHz`.
4. Set a finite sample limit such as `100000`.
5. Start acquisition.
6. Confirm DSView shows changing logic waveform data.
7. Stop acquisition from the UI.
8. Confirm stop returns promptly and exactly one acquisition end is observed in logs.
9. Start acquisition a second time without restarting DSView.
10. Confirm the second run starts, streams, stops, and closes cleanly.
11. For 16-channel profiles, enable at least one channel above `D7` and repeat
    a finite acquisition to exercise 16-bit transfer units.
12. During a controlled test only, unplug during acquisition and confirm DSView
    does not hang and the device can be scanned again after reconnect.
```

Add these stop conditions under `## Stop Conditions`:

```markdown
- Acquisition start succeeds but no `SR_DF_LOGIC` data reaches the waveform view.
- Stop does not return promptly.
- A second acquisition run fails after the first run stopped cleanly.
- Unplug during acquisition hangs the session thread.
```

- [ ] **Step 2: Update inventory**

In `docs/libsigrok-upstream-driver-inventory.md`, update the `Selected Slice: fx2lafw Open/Firmware Lifecycle` section:

```markdown
Status: implemented behind `DSVIEW_ENABLE_UPSTREAM_FX2LAFW`.

Scope:

- detect firmware-loaded devices during scan
- attempt fx2lafw firmware upload for bootloader-state devices only when the required firmware file is present in DSView's `PXTOOL/res/fx2lafw/` resource directory
- wait for re-enumeration in `dev_open()`
- open, claim interface 0, validate firmware major version, and close handles safely
- document the firmware resource manifest and hardware smoke gate
```

Then add this new section after it:

```markdown
## Selected Slice: fx2lafw Acquisition/Datafeed

Status: implemented behind `DSVIEW_ENABLE_UPSTREAM_FX2LAFW`; hardware smoke evidence still required.

Scope:

- compute fx2lafw start command and transfer sizing from samplerate and enabled channels
- start logic-only acquisition using FX2 `CMD_START`
- submit async libusb bulk transfers from endpoint `2 | LIBUSB_ENDPOINT_IN`
- forward received bytes as `SR_DF_LOGIC` packets through DSView's `ds_data_forward()` path
- cancel transfers and emit one end packet on stop or finite sample completion

Still deferred:

- manual acquisition smoke tests with real hardware
- adding licensed `fx2lafw-*.fw` binary files, if the project chooses to distribute them
- upstream soft trigger support
- DSView advanced trigger mapping
- analog/MSO support
- generic upstream USB helper extraction
```

Update `## Recommended Next Step` to:

```markdown
After fx2lafw acquisition hardware smoke testing, extract the reusable USB acquisition pieces or add a second USB logic analyzer such as `hantek-4032l`. Keep `rigol-ds` for a later SCPI-focused slice because it pulls in a larger shared transport surface.
```

- [ ] **Step 3: Verify docs and focused tests**

Run:

```bash
./build.macOS/DSView-test --run_test=upstream_fx2lafw
```

Expected: PASS.

- [ ] **Step 4: Commit**

```bash
git add -f docs/fx2lafw-hardware-smoke-test.md docs/libsigrok-upstream-driver-inventory.md
git commit -m "docs: add fx2lafw acquisition smoke gate"
```

## Task 6: Final Verification

**Files:**
- No new code files expected.

**Interfaces:**
- Consumes: Tasks 1 through 5.
- Produces: verified branch state.

- [ ] **Step 1: Run full verification**

Run:

```bash
cmake . -DENABLE_TESTS=ON -DDSVIEW_ENABLE_UPSTREAM_COMPAT_DEMO=ON -DDSVIEW_ENABLE_UPSTREAM_FX2LAFW=ON
make DSView-test -j$(sysctl -n hw.ncpu 2>/dev/null || echo 8)
./build.macOS/DSView-test
make DSView -j$(sysctl -n hw.ncpu 2>/dev/null || echo 8)
```

Expected: PASS. Existing non-fatal CMake warnings are acceptable only if all commands exit 0.

- [ ] **Step 2: Confirm app bundle still stages fx2lafw resources**

Run:

```bash
test -f build.macOS/PXTOOL.app/Contents/Resources/share/PXTOOL/res/fx2lafw/manifest.txt
```

Expected: PASS after `make DSView`.

- [ ] **Step 3: Inspect final status**

Run:

```bash
git status --short
```

Expected: clean working tree.

- [ ] **Step 4: Commit final verification note if needed**

If no files changed, do not create a commit. If documentation needed correction during verification, commit only that correction:

```bash
git add -f docs/fx2lafw-hardware-smoke-test.md docs/libsigrok-upstream-driver-inventory.md
git commit -m "docs: update fx2lafw acquisition verification notes"
```
