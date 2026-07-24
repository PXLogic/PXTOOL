# FX2LAFW Scan Slice Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add the first real upstream-derived `fx2lafw` device path as a DSView build-gated, scan-only driver slice.

**Architecture:** Keep PXTOOL on the existing DSView `ds_*` facade and add a new allowlisted driver under `libsigrok/hardware/upstream-fx2lafw/`. This first slice imports only the stable `fx2lafw` profile/config/channel contract and DSView-adapted USB scan path; firmware upload and asynchronous acquisition remain separate follow-up slices.

**Tech Stack:** C/C++17 style already used by DSView, CMake, GLib/GVariant/GSList, libusb, Boost.Test, existing vendored `libsigrok`, local upstream reference at `/Users/yuanji/Desktop/project/libsigrok`.

## Global Constraints

- Do not directly replace DSView's entire `libsigrok/` directory with upstream in one step.
- Do not rewrite the PXTOOL UI as part of this slice.
- Do not change DSLogic, DSCope, PXLogic, demo, file, decode, or session behavior.
- Keep `/Users/yuanji/Desktop/project/libsigrok` as a development reference source only; DSView must not depend on that path at build or runtime.
- When importing upstream libsigrok source into DSView, replace the upstream file header with the DSView/PXTOOL copyright and GPL notice style used by nearby files.
- Preserve the current in-source CMake build model for this branch.
- Keep the `fx2lafw` path disabled by default behind `DSVIEW_ENABLE_UPSTREAM_FX2LAFW`.
- This slice must not attempt firmware upload or asynchronous acquisition.
- Devices found by this slice must be tagged `DS_DEVICE_SOURCE_UPSTREAM_COMPAT`.

---

## File Structure

- Create `libsigrok/hardware/upstream-fx2lafw/fx2lafw.h`: DSView-private declarations for the scan-only driver, profile lookup helpers, and exported `fx2lafw_driver_info`.
- Create `libsigrok/hardware/upstream-fx2lafw/fx2lafw.c`: DSView-adapted scan-only implementation based on upstream `src/hardware/fx2lafw/api.c` profile data and config semantics.
- Modify `libsigrok/hwdriver.c`: register `fx2lafw_driver_info` only when `HAVE_UPSTREAM_FX2LAFW` is defined.
- Modify `CMakeLists.txt`: add `DSVIEW_ENABLE_UPSTREAM_FX2LAFW` option, compile definition, and gated source.
- Modify `PXTOOL/test/CMakeLists.txt`: compile the scan-only driver into `DSView-test` only when the option is enabled.
- Create `PXTOOL/test/test_upstream_fx2lafw.cpp`: Boost tests for profile lookup, config list/get/set, default channel state, and no-hardware scan stability.
- Modify `CMakeLists.txt`: add `DSVIEW_ENABLE_UPSTREAM_FX2LAFW` and `HAVE_UPSTREAM_FX2LAFW` before focused tests rely on the flag.
- Modify `docs/libsigrok-upstream-driver-inventory.md`: record that the selected next slice is scan-only `fx2lafw`.

## Task 1: FX2LAFW Profile and Config Contract

**Files:**
- Create: `libsigrok/hardware/upstream-fx2lafw/fx2lafw.h`
- Create: `libsigrok/hardware/upstream-fx2lafw/fx2lafw.c`
- Create: `PXTOOL/test/test_upstream_fx2lafw.cpp`
- Modify: `CMakeLists.txt`
- Modify: `PXTOOL/test/CMakeLists.txt`

**Interfaces:**
- Consumes: DSView `struct sr_dev_inst`, `sr_dev_inst_new()`, `sr_channel_new()`, `SR_CONF_*`, `DS_DEVICE_SOURCE_UPSTREAM_COMPAT`.
- Produces:
  - `SR_PRIV struct sr_dev_driver fx2lafw_driver_info`
  - `SR_PRIV const struct fx2lafw_profile *fx2lafw_profile_find(uint16_t vid, uint16_t pid, const char *manufacturer, const char *product)`
  - `SR_PRIV int fx2lafw_profile_channel_count(const struct fx2lafw_profile *profile)`

- [ ] **Step 1: Write failing Boost tests**

Create `PXTOOL/test/test_upstream_fx2lafw.cpp`:

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

#include <boost/test/unit_test.hpp>

extern "C" {
#include "libsigrok-internal.h"
#ifdef HAVE_UPSTREAM_FX2LAFW
#include "hardware/upstream-fx2lafw/fx2lafw.h"
#endif
}

BOOST_AUTO_TEST_SUITE(upstream_fx2lafw)

BOOST_AUTO_TEST_CASE(profile_lookup_matches_saleae_logic)
{
#ifdef HAVE_UPSTREAM_FX2LAFW
    const fx2lafw_profile *profile = fx2lafw_profile_find(
        0x0925, 0x3881, "", "");

    BOOST_REQUIRE(profile != nullptr);
    BOOST_CHECK_EQUAL(profile->vendor, "Saleae");
    BOOST_CHECK_EQUAL(profile->model, "Logic");
    BOOST_CHECK_EQUAL(fx2lafw_profile_channel_count(profile), 8);
#else
    BOOST_TEST_MESSAGE("upstream fx2lafw is disabled for this build");
#endif
}

BOOST_AUTO_TEST_CASE(profile_lookup_supports_16_channel_sigrok_fx2)
{
#ifdef HAVE_UPSTREAM_FX2LAFW
    const fx2lafw_profile *profile = fx2lafw_profile_find(
        0x1d50, 0x608d, "sigrok", "fx2lafw");

    BOOST_REQUIRE(profile != nullptr);
    BOOST_CHECK_EQUAL(profile->vendor, "sigrok");
    BOOST_CHECK_EQUAL(profile->model, "FX2 LA (16ch)");
    BOOST_CHECK_EQUAL(fx2lafw_profile_channel_count(profile), 16);
#else
    BOOST_TEST_MESSAGE("upstream fx2lafw is disabled for this build");
#endif
}

BOOST_AUTO_TEST_CASE(driver_exposes_dsview_supported_configs)
{
#ifdef HAVE_UPSTREAM_FX2LAFW
    GVariant *options = nullptr;
    BOOST_REQUIRE_EQUAL(fx2lafw_driver_info.config_list(
        SR_CONF_DEVICE_OPTIONS, &options, nullptr, nullptr), SR_OK);
    BOOST_REQUIRE(options != nullptr);

    bool has_samplerate = false;
    bool has_limit_samples = false;
    bool has_valid_channels = false;
    bool has_probe_enable = false;
    gsize option_count = 0;
    const int32_t *items = static_cast<const int32_t *>(
        g_variant_get_fixed_array(options, &option_count, sizeof(int32_t)));
    for (gsize i = 0; i < option_count; i++) {
        has_samplerate |= items[i] == SR_CONF_SAMPLERATE;
        has_limit_samples |= items[i] == SR_CONF_LIMIT_SAMPLES;
        has_valid_channels |= items[i] == SR_CONF_VLD_CH_NUM;
        has_probe_enable |= items[i] == SR_CONF_PROBE_EN;
    }
    g_variant_unref(options);

    BOOST_CHECK(has_samplerate);
    BOOST_CHECK(has_limit_samples);
    BOOST_CHECK(has_valid_channels);
    BOOST_CHECK(has_probe_enable);
#else
    BOOST_TEST_MESSAGE("upstream fx2lafw is disabled for this build");
#endif
}

BOOST_AUTO_TEST_SUITE_END()
```

- [ ] **Step 2: Add the CMake option and wire the failing test into `DSView-test`**

Modify `CMakeLists.txt` near `DSVIEW_ENABLE_UPSTREAM_COMPAT_DEMO`:

```cmake
option(DSVIEW_ENABLE_UPSTREAM_FX2LAFW "Enable the upstream-compat fx2lafw scan-only driver" OFF)
```

Modify the compile definition area:

```cmake
if(DSVIEW_ENABLE_UPSTREAM_FX2LAFW)
    add_compile_definitions(HAVE_UPSTREAM_FX2LAFW)
endif()
```

Modify `PXTOOL/test/CMakeLists.txt` so the executable source list includes:

```cmake
    test_upstream_fx2lafw.cpp
```

Add a gated source block after the existing upstream-demo test source block:

```cmake
if(DSVIEW_ENABLE_UPSTREAM_FX2LAFW)
    target_sources(DSView-test PRIVATE
        ../../libsigrok/hardware/upstream-fx2lafw/fx2lafw.c
    )
endif()
```

- [ ] **Step 3: Run the test and verify RED**

Run:

```bash
cmake . -DENABLE_TESTS=ON -DDSVIEW_ENABLE_UPSTREAM_FX2LAFW=ON
make DSView-test -j$(sysctl -n hw.ncpu 2>/dev/null || echo 8)
```

Expected: FAIL because `hardware/upstream-fx2lafw/fx2lafw.h` does not exist yet.

- [ ] **Step 4: Create the DSView header**

Create `libsigrok/hardware/upstream-fx2lafw/fx2lafw.h`:

```c
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

#ifndef UPSTREAM_FX2LAFW_H
#define UPSTREAM_FX2LAFW_H

#include "libsigrok-internal.h"

#define FX2LAFW_DEV_CAPS_16BIT (1 << 0)

struct fx2lafw_profile {
    uint16_t vid;
    uint16_t pid;
    const char *vendor;
    const char *model;
    const char *firmware;
    uint32_t dev_caps;
    const char *usb_manufacturer;
    const char *usb_product;
};

SR_PRIV const struct fx2lafw_profile *fx2lafw_profile_find(
    uint16_t vid, uint16_t pid, const char *manufacturer, const char *product);
SR_PRIV int fx2lafw_profile_channel_count(const struct fx2lafw_profile *profile);
SR_PRIV struct sr_dev_driver fx2lafw_driver_info;

#endif
```

- [ ] **Step 5: Create the minimal implementation**

Create `libsigrok/hardware/upstream-fx2lafw/fx2lafw.c`:

```c
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

#include "fx2lafw.h"
#include "device_source.h"

#include <stdio.h>
#include <string.h>

#undef LOG_PREFIX
#define LOG_PREFIX "upstream-fx2lafw: "

struct fx2lafw_context {
    const struct fx2lafw_profile *profile;
    uint64_t samplerate;
    uint64_t limit_samples;
};

static const struct fx2lafw_profile supported_fx2[] = {
    {0x08a9, 0x0014, "CWAV", "USBee AX", "fx2lafw-cwav-usbeeax.fw", 0, NULL, NULL},
    {0x08a9, 0x0015, "CWAV", "USBee DX", "fx2lafw-cwav-usbeedx.fw", FX2LAFW_DEV_CAPS_16BIT, NULL, NULL},
    {0x08a9, 0x0009, "CWAV", "USBee SX", "fx2lafw-cwav-usbeesx.fw", 0, NULL, NULL},
    {0x08a9, 0x0005, "CWAV", "USBee ZX", "fx2lafw-cwav-usbeezx.fw", 0, NULL, NULL},
    {0x0925, 0x3881, "Saleae", "Logic", "fx2lafw-saleae-logic.fw", 0, NULL, NULL},
    {0x04b4, 0x8613, "Cypress", "FX2", "fx2lafw-cypress-fx2.fw", FX2LAFW_DEV_CAPS_16BIT, NULL, NULL},
    {0x16d0, 0x0498, "Braintechnology", "USB-LPS", "fx2lafw-braintechnology-usb-lps.fw", FX2LAFW_DEV_CAPS_16BIT, NULL, NULL},
    {0x1d50, 0x608c, "sigrok", "FX2 LA (8ch)", "fx2lafw-sigrok-fx2-8ch.fw", 0, NULL, NULL},
    {0x1d50, 0x608d, "sigrok", "FX2 LA (16ch)", "fx2lafw-sigrok-fx2-16ch.fw", FX2LAFW_DEV_CAPS_16BIT, NULL, NULL},
    {0x1d50, 0x608f, "sigrok", "usb-c-grok", "fx2lafw-usb-c-grok.fw", 0, NULL, NULL},
    {0, 0, NULL, NULL, NULL, 0, NULL, NULL},
};

static const uint64_t samplerates[] = {
    SR_KHZ(20), SR_KHZ(25), SR_KHZ(50), SR_KHZ(100),
    SR_KHZ(200), SR_KHZ(250), SR_KHZ(500),
    SR_MHZ(1), SR_MHZ(2), SR_MHZ(3), SR_MHZ(4), SR_MHZ(6),
    SR_MHZ(8), SR_MHZ(12), SR_MHZ(16), SR_MHZ(24), SR_MHZ(48),
};

static const int32_t devopts[] = {
    SR_CONF_SAMPLERATE,
    SR_CONF_LIMIT_SAMPLES,
    SR_CONF_VLD_CH_NUM,
    SR_CONF_PROBE_EN,
};

SR_PRIV const struct fx2lafw_profile *fx2lafw_profile_find(
    uint16_t vid, uint16_t pid, const char *manufacturer, const char *product)
{
    for (int i = 0; supported_fx2[i].vid; i++) {
        const struct fx2lafw_profile *profile = &supported_fx2[i];
        if (profile->vid != vid || profile->pid != pid)
            continue;
        if (profile->usb_manufacturer && (!manufacturer || strcmp(profile->usb_manufacturer, manufacturer)))
            continue;
        if (profile->usb_product && (!product || strcmp(profile->usb_product, product)))
            continue;
        return profile;
    }
    return NULL;
}

SR_PRIV int fx2lafw_profile_channel_count(const struct fx2lafw_profile *profile)
{
    if (!profile)
        return 0;
    return (profile->dev_caps & FX2LAFW_DEV_CAPS_16BIT) ? 16 : 8;
}

static int hw_init(struct sr_context *ctx)
{
    (void)ctx;
    return SR_OK;
}

static int hw_cleanup(void)
{
    return SR_OK;
}

static GSList *hw_scan(GSList *options)
{
    (void)options;
    return NULL;
}

static int config_get(int id, GVariant **data, const struct sr_dev_inst *sdi,
    const struct sr_channel *ch, const struct sr_channel_group *cg)
{
    const struct fx2lafw_context *devc;

    (void)cg;
    if (!data)
        return SR_ERR_ARG;
    if (!sdi || !sdi->priv)
        return SR_ERR_ARG;

    devc = sdi->priv;
    switch (id) {
    case SR_CONF_SAMPLERATE:
        *data = g_variant_new_uint64(devc->samplerate);
        return SR_OK;
    case SR_CONF_LIMIT_SAMPLES:
        *data = g_variant_new_uint64(devc->limit_samples);
        return SR_OK;
    case SR_CONF_VLD_CH_NUM:
        *data = g_variant_new_int16(fx2lafw_profile_channel_count(devc->profile));
        return SR_OK;
    case SR_CONF_PROBE_EN:
        if (!ch)
            return SR_ERR_ARG;
        *data = g_variant_new_boolean(ch->enabled);
        return SR_OK;
    default:
        return SR_ERR_NA;
    }
}

static int config_set(int id, GVariant *data, struct sr_dev_inst *sdi,
    struct sr_channel *ch, struct sr_channel_group *cg)
{
    struct fx2lafw_context *devc;

    (void)cg;
    if (!data || !sdi || !sdi->priv)
        return SR_ERR_ARG;

    devc = sdi->priv;
    switch (id) {
    case SR_CONF_SAMPLERATE:
        devc->samplerate = g_variant_get_uint64(data);
        return SR_OK;
    case SR_CONF_LIMIT_SAMPLES:
        devc->limit_samples = g_variant_get_uint64(data);
        return SR_OK;
    case SR_CONF_PROBE_EN:
        if (!ch)
            return SR_ERR_ARG;
        ch->enabled = g_variant_get_boolean(data);
        return SR_OK;
    default:
        return SR_ERR_NA;
    }
}

static GVariant *samplerates_variant(void)
{
    GVariantBuilder builder;
    GVariant *values;

    values = g_variant_new_from_data(G_VARIANT_TYPE("at"),
        samplerates, ARRAY_SIZE(samplerates) * sizeof(uint64_t),
        TRUE, NULL, NULL);
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(&builder, "{sv}", "samplerates", values);
    return g_variant_builder_end(&builder);
}

static int config_list(int key, GVariant **data, const struct sr_dev_inst *sdi,
    const struct sr_channel_group *cg)
{
    (void)sdi;
    (void)cg;
    if (!data)
        return SR_ERR_ARG;

    switch (key) {
    case SR_CONF_DEVICE_OPTIONS:
        *data = g_variant_new_from_data(G_VARIANT_TYPE("ai"),
            devopts, ARRAY_SIZE(devopts) * sizeof(int32_t), TRUE, NULL, NULL);
        return SR_OK;
    case SR_CONF_SAMPLERATE:
        *data = samplerates_variant();
        return SR_OK;
    default:
        return SR_ERR_NA;
    }
}

SR_PRIV struct sr_dev_driver fx2lafw_driver_info = {
    .name = "fx2lafw",
    .longname = "fx2lafw (upstream compat scan-only)",
    .api_version = 1,
    .driver_type = DRIVER_TYPE_HARDWARE,
    .init = hw_init,
    .cleanup = hw_cleanup,
    .scan = hw_scan,
    .config_get = config_get,
    .config_set = config_set,
    .config_list = config_list,
    .priv = NULL,
};
```

- [ ] **Step 6: Run the focused test and verify GREEN**

Run:

```bash
cmake . -DENABLE_TESTS=ON -DDSVIEW_ENABLE_UPSTREAM_FX2LAFW=ON
make DSView-test -j$(sysctl -n hw.ncpu 2>/dev/null || echo 8)
./build.macOS/DSView-test --run_test=upstream_fx2lafw
```

Expected: `upstream_fx2lafw` passes.

- [ ] **Step 7: Commit**

Run:

```bash
git add CMakeLists.txt PXTOOL/test/CMakeLists.txt PXTOOL/test/test_upstream_fx2lafw.cpp libsigrok/hardware/upstream-fx2lafw/fx2lafw.c libsigrok/hardware/upstream-fx2lafw/fx2lafw.h
git commit -m "feat: add fx2lafw profile contract"
```

## Task 2: Build Gate and Driver Registry

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `libsigrok/hwdriver.c`

**Interfaces:**
- Consumes: `SR_PRIV struct sr_dev_driver fx2lafw_driver_info`.
- Produces:
  - optional registration in `sr_driver_list()`
  - app build source inclusion for `libsigrok/hardware/upstream-fx2lafw/fx2lafw.c`

- [ ] **Step 1: Add app source inclusion behind the existing gate**

Modify `set(libsigrok_SOURCES ...)` after the upstream-demo block:

```cmake
if(DSVIEW_ENABLE_UPSTREAM_FX2LAFW)
    list(APPEND libsigrok_SOURCES
        libsigrok/hardware/upstream-fx2lafw/fx2lafw.c
    )
endif()
```

- [ ] **Step 2: Register the driver behind the compile flag**

Modify `libsigrok/hwdriver.c` near the other guarded externs:

```c
#ifdef HAVE_UPSTREAM_FX2LAFW
extern SR_PRIV struct sr_dev_driver fx2lafw_driver_info;
#endif
```

Modify `drivers_list[]` after the upstream-demo entry:

```c
#ifdef HAVE_UPSTREAM_FX2LAFW
    &fx2lafw_driver_info,
#endif
```

- [ ] **Step 3: Build with the gate enabled**

Run:

```bash
cmake . -DENABLE_TESTS=ON -DDSVIEW_ENABLE_UPSTREAM_COMPAT_DEMO=ON -DDSVIEW_ENABLE_UPSTREAM_FX2LAFW=ON
make DSView-test -j$(sysctl -n hw.ncpu 2>/dev/null || echo 8)
make DSView -j$(sysctl -n hw.ncpu 2>/dev/null || echo 8)
```

Expected: both targets build.

- [ ] **Step 4: Build with the gate disabled**

Run:

```bash
cmake . -DENABLE_TESTS=ON -DDSVIEW_ENABLE_UPSTREAM_COMPAT_DEMO=ON -DDSVIEW_ENABLE_UPSTREAM_FX2LAFW=OFF
make DSView-test -j$(sysctl -n hw.ncpu 2>/dev/null || echo 8)
make DSView -j$(sysctl -n hw.ncpu 2>/dev/null || echo 8)
```

Expected: both targets build, and the focused `upstream_fx2lafw` test reports the disabled-build message rather than linking the driver source.

- [ ] **Step 5: Commit**

Run:

```bash
git add CMakeLists.txt libsigrok/hwdriver.c
git commit -m "feat: gate fx2lafw upstream driver"
```

## Task 3: Scan-Only USB Discovery

**Files:**
- Modify: `libsigrok/hardware/upstream-fx2lafw/fx2lafw.c`
- Modify: `PXTOOL/test/test_upstream_fx2lafw.cpp`

**Interfaces:**
- Consumes:
  - `fx2lafw_profile_find(uint16_t, uint16_t, const char *, const char *)`
  - `fx2lafw_profile_channel_count(const struct fx2lafw_profile *)`
  - DSView `sr_usb_dev_inst_new()`, `sr_usb_dev_inst_free()`, `sr_channel_new()`, `sr_dev_inst_new()`
- Produces:
  - `fx2lafw_driver_info.scan()` that returns zero or more DSView `sr_dev_inst` objects.

- [ ] **Step 1: Add a no-hardware scan stability test**

Append this test to `PXTOOL/test/test_upstream_fx2lafw.cpp`:

```cpp
BOOST_AUTO_TEST_CASE(scan_without_options_does_not_crash)
{
#ifdef HAVE_UPSTREAM_FX2LAFW
    GSList *devices = fx2lafw_driver_info.scan(nullptr);
    g_slist_free_full(devices, reinterpret_cast<GDestroyNotify>(sr_dev_inst_free));
#else
    BOOST_TEST_MESSAGE("upstream fx2lafw is disabled for this build");
#endif
}
```

- [ ] **Step 2: Add a device construction helper**

Add this helper to `libsigrok/hardware/upstream-fx2lafw/fx2lafw.c` before `hw_scan()`:

```c
static struct sr_dev_inst *create_device_from_profile(
    const struct fx2lafw_profile *profile, uint8_t bus, uint8_t address,
    const char *serial_num, const char *connection_id)
{
    struct fx2lafw_context *devc;
    struct sr_dev_inst *sdi;
    int channel_count;

    sdi = sr_dev_inst_new(LOGIC, SR_ST_INACTIVE,
        profile->vendor, profile->model, NULL);
    if (!sdi)
        return NULL;

    devc = g_malloc0(sizeof(*devc));
    devc->profile = profile;
    devc->samplerate = samplerates[0];
    devc->limit_samples = 0;
    sdi->priv = devc;
    sdi->driver = &fx2lafw_driver_info;
    sdi->dev_type = DEV_TYPE_USB;
    sdi->serial_num = g_strdup(serial_num ? serial_num : "");
    sdi->connection_id = g_strdup(connection_id ? connection_id : "");
    sdi->conn = sr_usb_dev_inst_new(bus, address);
    ds_device_source_set(sdi, DS_DEVICE_SOURCE_UPSTREAM_COMPAT);

    channel_count = fx2lafw_profile_channel_count(profile);
    for (int i = 0; i < channel_count; i++) {
        char name[8];
        struct sr_channel *probe;

        snprintf(name, sizeof(name), "D%d", i);
        probe = sr_channel_new(i, SR_CHANNEL_LOGIC, TRUE, name);
        if (!probe) {
            sr_dev_inst_free(sdi);
            return NULL;
        }
        sdi->channels = g_slist_append(sdi->channels, probe);
    }

    return sdi;
}
```

- [ ] **Step 3: Implement scan-only USB discovery**

Replace `hw_scan()` in `libsigrok/hardware/upstream-fx2lafw/fx2lafw.c` with:

```c
static GSList *hw_scan(GSList *options)
{
    libusb_device **devlist;
    GSList *devices;
    int device_count;

    (void)options;

    devices = NULL;
    device_count = libusb_get_device_list(NULL, &devlist);
    if (device_count < 0)
        return NULL;

    for (int i = 0; i < device_count; i++) {
        struct libusb_device_descriptor desc;
        libusb_device_handle *handle;
        char manufacturer[64];
        char product[64];
        char serial_num[64];
        char connection_id[64];
        const struct fx2lafw_profile *profile;

        manufacturer[0] = '\0';
        product[0] = '\0';
        serial_num[0] = '\0';
        connection_id[0] = '\0';

        if (libusb_get_device_descriptor(devlist[i], &desc) < 0)
            continue;

        profile = fx2lafw_profile_find(desc.idVendor, desc.idProduct, "", "");
        if (!profile)
            continue;

        if (libusb_open(devlist[i], &handle) == 0) {
            if (desc.iManufacturer)
                libusb_get_string_descriptor_ascii(handle, desc.iManufacturer,
                    (unsigned char *)manufacturer, sizeof(manufacturer));
            if (desc.iProduct)
                libusb_get_string_descriptor_ascii(handle, desc.iProduct,
                    (unsigned char *)product, sizeof(product));
            if (desc.iSerialNumber)
                libusb_get_string_descriptor_ascii(handle, desc.iSerialNumber,
                    (unsigned char *)serial_num, sizeof(serial_num));
            libusb_close(handle);
        }

        profile = fx2lafw_profile_find(desc.idVendor, desc.idProduct, manufacturer, product);
        if (!profile)
            continue;

        snprintf(connection_id, sizeof(connection_id), "%u.%u",
            libusb_get_bus_number(devlist[i]),
            libusb_get_device_address(devlist[i]));

        struct sr_dev_inst *sdi = create_device_from_profile(profile,
            libusb_get_bus_number(devlist[i]),
            libusb_get_device_address(devlist[i]),
            serial_num,
            connection_id);
        if (sdi)
            devices = g_slist_append(devices, sdi);
    }

    libusb_free_device_list(devlist, 1);
    return devices;
}
```

- [ ] **Step 4: Run focused tests**

Run:

```bash
cmake . -DENABLE_TESTS=ON -DDSVIEW_ENABLE_UPSTREAM_FX2LAFW=ON
make DSView-test -j$(sysctl -n hw.ncpu 2>/dev/null || echo 8)
./build.macOS/DSView-test --run_test=upstream_fx2lafw
```

Expected: tests pass with or without FX2 hardware attached.

- [ ] **Step 5: Run app build**

Run:

```bash
make DSView -j$(sysctl -n hw.ncpu 2>/dev/null || echo 8)
```

Expected: app build succeeds.

- [ ] **Step 6: Commit**

Run:

```bash
git add libsigrok/hardware/upstream-fx2lafw/fx2lafw.c PXTOOL/test/test_upstream_fx2lafw.cpp
git commit -m "feat: scan fx2lafw upstream devices"
```

## Task 4: Inventory and Verification Handoff

**Files:**
- Modify: `docs/libsigrok-upstream-driver-inventory.md`

**Interfaces:**
- Consumes: completed scan-only `fx2lafw` slice.
- Produces: documented next limitations and follow-up path.

- [ ] **Step 1: Update inventory decision**

Append this section to `docs/libsigrok-upstream-driver-inventory.md`:

```markdown
## Selected Slice: fx2lafw Scan-Only

The next implementation slice is `fx2lafw` scan-only support behind
`DSVIEW_ENABLE_UPSTREAM_FX2LAFW`.

Included:

- upstream-derived `fx2lafw` VID/PID/profile table
- DSView/PXTOOL-normalized file headers
- DSView `sr_dev_inst` and `sr_channel` construction
- `SR_CONF_SAMPLERATE`, `SR_CONF_LIMIT_SAMPLES`, `SR_CONF_VLD_CH_NUM`, and `SR_CONF_PROBE_EN`
- no-hardware scan stability test

Deferred:

- EZ-USB firmware upload
- firmware resource packaging
- asynchronous libusb transfer loop
- soft trigger integration
- `SR_DF_LOGIC` acquisition forwarding into `ds_data_forward()`

Reason:

The upstream driver depends on firmware renumeration, upstream `std_*`
helpers, session source management, and soft trigger state. Those should be
ported as explicit follow-up slices after scan/config/channel behavior is
proven in DSView.
```

- [ ] **Step 2: Run final verification**

Run:

```bash
cmake . -DENABLE_TESTS=ON -DDSVIEW_ENABLE_UPSTREAM_COMPAT_DEMO=ON -DDSVIEW_ENABLE_UPSTREAM_FX2LAFW=ON
make DSView-test -j$(sysctl -n hw.ncpu 2>/dev/null || echo 8)
./build.macOS/DSView-test
make DSView -j$(sysctl -n hw.ncpu 2>/dev/null || echo 8)
```

Expected:

- all Boost tests pass
- `DSView` builds
- `fx2lafw` is still disabled by default unless `DSVIEW_ENABLE_UPSTREAM_FX2LAFW=ON` is supplied

- [ ] **Step 3: Commit**

Run:

```bash
git add docs/libsigrok-upstream-driver-inventory.md
git commit -m "docs: record fx2lafw scan slice"
```

## Self-Review

Spec coverage:

- Keeps `/Users/yuanji/Desktop/project/libsigrok` as reference only: yes, copied profile data is committed into DSView and no build path references the local upstream checkout.
- Header normalization: yes, every new source file uses the DSView/PXTOOL header.
- Allowlist: yes, `DSVIEW_ENABLE_UPSTREAM_FX2LAFW` defaults OFF.
- No behavior changes to native devices: yes, driver registration is compile-gated and disabled by default.
- First real upstream driver path: yes, starts `fx2lafw` with profile/config/scan only.

Planning marker scan:

- No intentionally unresolved planning markers are left in this plan.

Type consistency:

- `fx2lafw_profile_find()`, `fx2lafw_profile_channel_count()`, and `fx2lafw_driver_info` are declared in Task 1 and consumed by later tasks.
- CMake flag and compile definition are consistently named `DSVIEW_ENABLE_UPSTREAM_FX2LAFW` and `HAVE_UPSTREAM_FX2LAFW`.
