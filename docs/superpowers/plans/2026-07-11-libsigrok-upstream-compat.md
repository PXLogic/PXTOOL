# Libsigrok Upstream Compatibility Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the first safe DSView-to-upstream libsigrok compatibility slice without regressing DSLogic, DSCope, PXLogic, demo, file, decode, or session workflows.

**Architecture:** Keep PXTOOL on the existing `ds_*` API while adding internal device source classification, capability queries, and an allowlisted upstream-compat driver path. The first proof of concept uses a DSView-native upstream-compat demo driver shape rather than copying upstream demo code, because upstream demo depends on newer upstream core/session structs and would pull too much core migration into the first slice.

**Tech Stack:** C/C++17 style already used by DSView, CMake, GLib/GVariant/GSList, libusb, Qt, Boost.Test, existing vendored `libsigrok` and `libsigrokdecode`.

## Global Constraints

- Do not directly replace DSView's entire `libsigrok/` directory with upstream in one step.
- Do not rewrite the PXTOOL UI as part of the first migration.
- Do not promise full support for all 87 upstream hardware drivers in the first phase.
- Do not remove DSView-specific `SR_CONF_*` keys until equivalent capability-based behavior exists.
- Do not change DSLogic, DSCope, or PXLogic behavior as a side effect of adding upstream drivers.
- Keep `/Users/yuanji/Desktop/project/libsigrok` as a development reference source only; DSView must not depend on that path at build or runtime.
- When importing upstream libsigrok source into DSView, replace the upstream file header with the DSView/PXTOOL copyright and GPL notice style used by nearby files.
- Preserve the current CMake build model for this slice.
- Use an allowlist for any upstream-compat driver path.
- Commit after each task.

---

## File Structure

- Modify `libsigrok/libsigrok-internal.h`: add device-source and capability declarations behind DSView's internal API.
- Modify `libsigrok/libsigrok.h`: add small public capability query API for PXTOOL without exposing upstream structs.
- Create `libsigrok/device_source.c`: source-kind and capability helpers, deliberately independent of driver implementations.
- Create `libsigrok/device_source.h`: private declarations for source/capability helpers.
- Modify `libsigrok/dsdevice.c`: initialize source-kind defaults for new device instances.
- Modify `libsigrok/lib_main.c`: include source/capability helpers, preserve existing behavior, expose capability query facade, and tag demo/file/native devices.
- Modify `libsigrok/hwdriver.c`: register an optional upstream-compat demo driver through a CMake flag.
- Create `libsigrok/hardware/upstream-demo/upstream_demo.c`: DSView-native proof-of-concept driver that mimics an upstream waveform-capable device enough to validate scan/config/acquire.
- Create `libsigrok/hardware/upstream-demo/upstream_demo.h`: private POC driver declaration.
- Modify `CMakeLists.txt`: add option and source list entries for the upstream-compat POC.
- Modify `PXTOOL/test/CMakeLists.txt`: include new libsigrok helper sources in tests.
- Create `PXTOOL/test/test_device_source.cpp`: Boost tests for source-kind and capability mapping helpers.
- Modify `PXTOOL/pv/deviceagent.h` and `PXTOOL/pv/deviceagent.cpp`: add convenience capability methods while keeping existing call sites working.
- Modify a narrow UI path only after capability tests exist, likely `PXTOOL/pv/toolbars/samplingbar.cpp`: hide/disable controls based on capability rather than driver-name checks where safe.
- Update `docs/superpowers/specs/2026-07-11-libsigrok-upstream-compat-design.md` only if implementation discoveries require tightening the spec.

## Task 1: Device Source and Capability Helpers

**Files:**
- Create: `libsigrok/device_source.h`
- Create: `libsigrok/device_source.c`
- Modify: `libsigrok/libsigrok-internal.h`
- Modify: `libsigrok/libsigrok.h`
- Modify: `libsigrok/dsdevice.c`
- Modify: `libsigrok/lib_main.c`
- Test: `PXTOOL/test/test_device_source.cpp`
- Modify: `PXTOOL/test/CMakeLists.txt`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: existing `struct sr_dev_inst`, `struct sr_dev_driver`, `SR_CONF_*`, `DEV_TYPE_*`, and `ds_get_actived_device_config_list()`.
- Produces:
  - `enum ds_device_source_kind`
  - `enum ds_device_capability`
  - `SR_PRIV void ds_device_source_set(struct sr_dev_inst *sdi, int source_kind)`
  - `SR_PRIV int ds_device_source_get(const struct sr_dev_inst *sdi)`
  - `SR_PRIV gboolean ds_device_supports_config_key(const struct sr_dev_inst *sdi, int key)`
  - `SR_PRIV gboolean ds_device_supports_capability(const struct sr_dev_inst *sdi, int capability)`
  - `SR_API int ds_actived_device_supports_config_key(int key)`
  - `SR_API int ds_actived_device_supports_capability(int capability)`

- [ ] **Step 1: Add failing Boost tests for source kind defaults and key support**

Create `PXTOOL/test/test_device_source.cpp`:

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
#include "device_source.h"
}

BOOST_AUTO_TEST_SUITE(device_source)

BOOST_AUTO_TEST_CASE(new_device_defaults_to_native_source)
{
    sr_dev_inst *sdi = sr_dev_inst_new(LOGIC, SR_ST_INACTIVE, "Vendor", "Model", "1.0");

    BOOST_REQUIRE(sdi != nullptr);
    BOOST_CHECK_EQUAL(ds_device_source_get(sdi), DS_DEVICE_SOURCE_NATIVE);

    sr_dev_inst_free(sdi);
}

BOOST_AUTO_TEST_CASE(source_kind_can_be_set)
{
    sr_dev_inst *sdi = sr_dev_inst_new(LOGIC, SR_ST_INACTIVE, "Vendor", "Model", "1.0");

    BOOST_REQUIRE(sdi != nullptr);
    ds_device_source_set(sdi, DS_DEVICE_SOURCE_UPSTREAM_COMPAT);

    BOOST_CHECK_EQUAL(ds_device_source_get(sdi), DS_DEVICE_SOURCE_UPSTREAM_COMPAT);

    sr_dev_inst_free(sdi);
}

BOOST_AUTO_TEST_CASE(null_device_has_no_capabilities)
{
    BOOST_CHECK_EQUAL(ds_device_source_get(nullptr), DS_DEVICE_SOURCE_UNKNOWN);
    BOOST_CHECK(!ds_device_supports_config_key(nullptr, SR_CONF_SAMPLERATE));
    BOOST_CHECK(!ds_device_supports_capability(nullptr, DS_DEVICE_CAP_WAVEFORM));
}

BOOST_AUTO_TEST_SUITE_END()
```

- [ ] **Step 2: Wire the failing test into the test target**

Modify `PXTOOL/test/CMakeLists.txt`:

```cmake
add_executable(DSView-test
    test.cpp
    test_channeltint.cpp
    test_device_source.cpp
    test_diskcachesettings.cpp
    test_logsearch.cpp
    test_theme_qss.cpp
    ../pv/view/channeltint.cpp
    ../pv/dock/logsearch.cpp
    ../../libsigrok/device_source.c
    ../../libsigrok/dsdevice.c
    ../../libsigrok/log.c
    ../../libsigrok/strutil.c
)

target_include_directories(DSView-test PRIVATE
    ..
    ../..
    ../../common
    ../../libsigrok
    ${GLIB_INCLUDE_DIRS}
    ${LIBUSB_1_INCLUDE_DIRS}
    ${QT_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
)

target_link_libraries(DSView-test
    ${QT_LIBRARIES}
    -lglib-2.0
    ${LIBUSB_1_LIBRARIES}
)
```

Keep the existing include directories and link libraries that are already present, and add the GLib/libusb entries shown above if they are missing.

- [ ] **Step 3: Run test build and verify it fails because helpers do not exist**

Run:

```bash
cmake --build build.macOS --target DSView-test
```

Expected: FAIL with missing `device_source.h` or missing `ds_device_source_*` declarations.

- [ ] **Step 4: Add source-kind and capability declarations**

Modify `libsigrok/libsigrok.h` near the DSView public API section:

```c
enum ds_device_capability {
    DS_DEVICE_CAP_WAVEFORM = 1,
    DS_DEVICE_CAP_LOGIC = 2,
    DS_DEVICE_CAP_ANALOG = 3,
    DS_DEVICE_CAP_DSO = 4,
    DS_DEVICE_CAP_ADVANCED_TRIGGER = 5,
    DS_DEVICE_CAP_STREAM = 6,
    DS_DEVICE_CAP_DISK_CACHE = 7,
};

SR_API int ds_actived_device_supports_config_key(int key);
SR_API int ds_actived_device_supports_capability(int capability);
```

Modify `libsigrok/libsigrok-internal.h` after `enum sr_dev_driver_type`:

```c
enum ds_device_source_kind
{
    DS_DEVICE_SOURCE_UNKNOWN = 0,
    DS_DEVICE_SOURCE_NATIVE = 1,
    DS_DEVICE_SOURCE_UPSTREAM_COMPAT = 2,
    DS_DEVICE_SOURCE_FILE = 3,
    DS_DEVICE_SOURCE_DEMO = 4
};
```

Add a field to `struct sr_dev_inst` near `dev_type`:

```c
    /** Internal source kind: native, upstream-compat, file, or demo. */
    int source_kind;
```

- [ ] **Step 5: Create private helper header**

Create `libsigrok/device_source.h`:

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

#ifndef DEVICE_SOURCE_H
#define DEVICE_SOURCE_H

#include "libsigrok-internal.h"

SR_PRIV void ds_device_source_set(struct sr_dev_inst *sdi, int source_kind);
SR_PRIV int ds_device_source_get(const struct sr_dev_inst *sdi);
SR_PRIV gboolean ds_device_supports_config_key(const struct sr_dev_inst *sdi, int key);
SR_PRIV gboolean ds_device_supports_capability(const struct sr_dev_inst *sdi, int capability);

#endif
```

- [ ] **Step 6: Implement helper source**

Create `libsigrok/device_source.c`:

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

#include "device_source.h"

static gboolean variant_option_list_contains(GVariant *data, int key)
{
    GVariantIter iter;
    int32_t item;

    if (!data)
        return FALSE;

    if (!g_variant_is_of_type(data, G_VARIANT_TYPE("ai")))
        return FALSE;

    g_variant_iter_init(&iter, data);
    while (g_variant_iter_next(&iter, "i", &item)) {
        if ((int)item == key)
            return TRUE;
    }

    return FALSE;
}

void ds_device_source_set(struct sr_dev_inst *sdi, int source_kind)
{
    if (!sdi)
        return;

    sdi->source_kind = source_kind;
}

int ds_device_source_get(const struct sr_dev_inst *sdi)
{
    if (!sdi)
        return DS_DEVICE_SOURCE_UNKNOWN;

    if (sdi->source_kind != DS_DEVICE_SOURCE_UNKNOWN)
        return sdi->source_kind;

    switch (sdi->dev_type) {
    case DEV_TYPE_DEMO:
        return DS_DEVICE_SOURCE_DEMO;
    case DEV_TYPE_FILELOG:
        return DS_DEVICE_SOURCE_FILE;
    case DEV_TYPE_USB:
        return DS_DEVICE_SOURCE_NATIVE;
    default:
        return DS_DEVICE_SOURCE_UNKNOWN;
    }
}

gboolean ds_device_supports_config_key(const struct sr_dev_inst *sdi, int key)
{
    GVariant *data;
    int ret;

    if (!sdi || !sdi->driver || !sdi->driver->config_list)
        return FALSE;

    data = NULL;
    ret = sdi->driver->config_list(SR_CONF_DEVICE_OPTIONS, &data, sdi, NULL);
    if (ret != SR_OK || !data)
        return FALSE;

    const gboolean supported = variant_option_list_contains(data, key);
    g_variant_unref(data);
    return supported;
}

gboolean ds_device_supports_capability(const struct sr_dev_inst *sdi, int capability)
{
    if (!sdi)
        return FALSE;

    switch (capability) {
    case DS_DEVICE_CAP_WAVEFORM:
        return sdi->mode == LOGIC || sdi->mode == ANALOG || sdi->mode == DSO;
    case DS_DEVICE_CAP_LOGIC:
        return sdi->mode == LOGIC;
    case DS_DEVICE_CAP_ANALOG:
        return sdi->mode == ANALOG;
    case DS_DEVICE_CAP_DSO:
        return sdi->mode == DSO;
    case DS_DEVICE_CAP_ADVANCED_TRIGGER:
        return ds_device_supports_config_key(sdi, SR_CONF_HAVE_ADVANCED_TRIGGER);
    case DS_DEVICE_CAP_STREAM:
        return ds_device_supports_config_key(sdi, SR_CONF_STREAM);
    case DS_DEVICE_CAP_DISK_CACHE:
        return ds_device_supports_config_key(sdi, SR_CONF_DISK_CACHE_ENABLE)
            || ds_device_supports_config_key(sdi, SR_CONF_DISK_CACHE_PATH);
    default:
        return FALSE;
    }
}
```

- [ ] **Step 7: Initialize source kind for new device instances**

Modify `libsigrok/dsdevice.c` in `sr_dev_inst_new()` after `sdi->handle = (ds_device_handle)sdi;`:

```c
    sdi->source_kind = DS_DEVICE_SOURCE_NATIVE;
```

- [ ] **Step 8: Expose active-device capability facade**

Modify `libsigrok/lib_main.c` includes:

```c
#include "device_source.h"
```

Add after `ds_get_actived_device_mode()`:

```c
SR_API int ds_actived_device_supports_config_key(int key)
{
    if (lib_ctx.actived_device_instance == NULL)
        return 0;

    return ds_device_supports_config_key(lib_ctx.actived_device_instance, key) ? 1 : 0;
}

SR_API int ds_actived_device_supports_capability(int capability)
{
    if (lib_ctx.actived_device_instance == NULL)
        return 0;

    return ds_device_supports_capability(lib_ctx.actived_device_instance, capability) ? 1 : 0;
}
```

- [ ] **Step 9: Add helper source to app build**

Modify `CMakeLists.txt` in `set(libsigrok_SOURCES ...)`:

```cmake
    libsigrok/device_source.c
```

Place it near `libsigrok/dsdevice.c`.

- [ ] **Step 10: Run tests**

Run:

```bash
cmake --build build.macOS --target DSView-test
./build.macOS/DSView-test --run_test=device_source
```

Expected: build succeeds and `device_source` tests PASS.

- [ ] **Step 11: Commit**

```bash
git add CMakeLists.txt PXTOOL/test/CMakeLists.txt PXTOOL/test/test_device_source.cpp libsigrok/device_source.c libsigrok/device_source.h libsigrok/dsdevice.c libsigrok/lib_main.c libsigrok/libsigrok-internal.h libsigrok/libsigrok.h
git commit -m "feat: add device capability helpers"
```

## Task 2: DeviceAgent Capability Accessors

**Files:**
- Modify: `PXTOOL/pv/deviceagent.h`
- Modify: `PXTOOL/pv/deviceagent.cpp`
- Test: build existing application and test target

**Interfaces:**
- Consumes:
  - `SR_API int ds_actived_device_supports_config_key(int key)`
  - `SR_API int ds_actived_device_supports_capability(int capability)`
- Produces:
  - `bool DeviceAgent::supports_config(int key)`
  - `bool DeviceAgent::supports_capability(int capability)`
  - `bool DeviceAgent::supports_waveform()`
  - `bool DeviceAgent::supports_stream()`
  - `bool DeviceAgent::supports_advanced_trigger()`

- [ ] **Step 1: Add method declarations**

Modify `PXTOOL/pv/deviceagent.h` in the public section near existing config helpers:

```cpp
    bool supports_config(int key);

    bool supports_capability(int capability);

    bool supports_waveform();

    bool supports_stream();

    bool supports_advanced_trigger();
```

- [ ] **Step 2: Implement methods**

Modify `PXTOOL/pv/deviceagent.cpp` after `DeviceAgent::get_hardware_operation_mode()` or another nearby config helper:

```cpp
bool DeviceAgent::supports_config(int key)
{
    if (!have_instance())
        return false;

    return ds_actived_device_supports_config_key(key) > 0;
}

bool DeviceAgent::supports_capability(int capability)
{
    if (!have_instance())
        return false;

    return ds_actived_device_supports_capability(capability) > 0;
}

bool DeviceAgent::supports_waveform()
{
    return supports_capability(DS_DEVICE_CAP_WAVEFORM);
}

bool DeviceAgent::supports_stream()
{
    return supports_capability(DS_DEVICE_CAP_STREAM);
}

bool DeviceAgent::supports_advanced_trigger()
{
    return supports_capability(DS_DEVICE_CAP_ADVANCED_TRIGGER);
}
```

- [ ] **Step 3: Build**

Run:

```bash
cmake --build build.macOS --target DSView-test
cmake --build build.macOS --target DSView
```

Expected: both targets build. If the target name is not `DSView` in this checkout, run `cmake --build build.macOS` and record the actual target name in the commit message body.

- [ ] **Step 4: Commit**

```bash
git add PXTOOL/pv/deviceagent.h PXTOOL/pv/deviceagent.cpp
git commit -m "feat: expose device capabilities in DeviceAgent"
```

## Task 3: CMake Allowlist for Upstream-Compat Driver Path

**Files:**
- Modify: `CMakeLists.txt`
- Create: `libsigrok/hardware/upstream-demo/upstream_demo.h`
- Create: `libsigrok/hardware/upstream-demo/upstream_demo.c`
- Modify: `libsigrok/hwdriver.c`

**Interfaces:**
- Consumes: existing DSView `struct sr_dev_driver` callback shape.
- Produces:
  - CMake option `DSVIEW_ENABLE_UPSTREAM_COMPAT_DEMO`
  - `SR_PRIV struct sr_dev_driver upstream_demo_driver_info`
  - optional registration in `sr_driver_list()`

- [ ] **Step 1: Add disabled-by-default CMake option**

Modify `CMakeLists.txt` near project options:

```cmake
option(DSVIEW_ENABLE_UPSTREAM_COMPAT_DEMO "Enable the upstream-compat demo driver proof of concept" OFF)
```

Modify `set(libsigrok_SOURCES ...)`:

```cmake
if(DSVIEW_ENABLE_UPSTREAM_COMPAT_DEMO)
    list(APPEND libsigrok_SOURCES
        libsigrok/hardware/upstream-demo/upstream_demo.c
    )
endif()
```

Modify target compile definitions before `add_executable()` or after it with project target:

```cmake
if(DSVIEW_ENABLE_UPSTREAM_COMPAT_DEMO)
    add_compile_definitions(HAVE_UPSTREAM_COMPAT_DEMO)
endif()
```

- [ ] **Step 2: Create POC driver header with DSView header style**

Create `libsigrok/hardware/upstream-demo/upstream_demo.h`:

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

#ifndef UPSTREAM_COMPAT_DEMO_H
#define UPSTREAM_COMPAT_DEMO_H

#include "libsigrok-internal.h"

SR_PRIV struct sr_dev_driver upstream_demo_driver_info;

#endif
```

- [ ] **Step 3: Create minimal POC driver**

Create `libsigrok/hardware/upstream-demo/upstream_demo.c`:

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

#include "upstream_demo.h"
#include "device_source.h"
#include <string.h>

#undef LOG_PREFIX
#define LOG_PREFIX "upstream-demo: "

struct upstream_demo_context {
    uint64_t samplerate;
    uint64_t limit_samples;
};

static const uint64_t samplerates[] = {
    SR_KHZ(100),
    SR_MHZ(1),
    SR_MHZ(10),
};

static const int32_t trigger_matches[] = {
    SR_TRIGGER_ZERO,
    SR_TRIGGER_ONE,
    SR_TRIGGER_RISING,
    SR_TRIGGER_FALLING,
    SR_TRIGGER_EDGE,
};

static const int32_t devopts[] = {
    SR_CONF_SAMPLERATE,
    SR_CONF_LIMIT_SAMPLES,
    SR_CONF_TRIGGER_MATCH,
};

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

    struct sr_dev_inst *sdi = sr_dev_inst_new(LOGIC, SR_ST_INACTIVE,
        "DSView", "Upstream Compat Demo", "0.1");
    if (!sdi)
        return NULL;

    struct upstream_demo_context *devc = g_malloc0(sizeof(*devc));
    devc->samplerate = SR_MHZ(1);
    devc->limit_samples = SR_KHZ(1);
    sdi->priv = devc;
    sdi->driver = &upstream_demo_driver_info;
    sdi->dev_type = DEV_TYPE_DEMO;
    ds_device_source_set(sdi, DS_DEVICE_SOURCE_UPSTREAM_COMPAT);

    for (int i = 0; i < 8; i++) {
        char name[8];
        snprintf(name, sizeof(name), "D%d", i);
        struct sr_channel *probe = sr_channel_new(i, SR_CHANNEL_LOGIC, TRUE, name);
        if (!probe) {
            sr_dev_inst_free(sdi);
            return NULL;
        }
        sdi->channels = g_slist_append(sdi->channels, probe);
    }

    return g_slist_append(NULL, sdi);
}

static const GSList *hw_dev_mode_list(const struct sr_dev_inst *sdi)
{
    (void)sdi;
    return g_slist_append(NULL, (gpointer)&sr_mode_list[0]);
}

static int config_get(int id, GVariant **data, const struct sr_dev_inst *sdi,
    const struct sr_channel *ch, const struct sr_channel_group *cg)
{
    (void)ch;
    (void)cg;

    if (!sdi || !sdi->priv || !data)
        return SR_ERR_ARG;

    struct upstream_demo_context *devc = sdi->priv;
    switch (id) {
    case SR_CONF_SAMPLERATE:
        *data = g_variant_new_uint64(devc->samplerate);
        return SR_OK;
    case SR_CONF_LIMIT_SAMPLES:
        *data = g_variant_new_uint64(devc->limit_samples);
        return SR_OK;
    default:
        return SR_ERR_NA;
    }
}

static int config_set(int id, GVariant *data, struct sr_dev_inst *sdi,
    struct sr_channel *ch, struct sr_channel_group *cg)
{
    (void)ch;
    (void)cg;

    if (!sdi || !sdi->priv || !data)
        return SR_ERR_ARG;

    struct upstream_demo_context *devc = sdi->priv;
    switch (id) {
    case SR_CONF_SAMPLERATE:
        devc->samplerate = g_variant_get_uint64(data);
        return SR_OK;
    case SR_CONF_LIMIT_SAMPLES:
        devc->limit_samples = g_variant_get_uint64(data);
        return SR_OK;
    default:
        return SR_ERR_NA;
    }
}

static GVariant *array_i32_variant(const int32_t *items, size_t count)
{
    GVariantBuilder builder;

    g_variant_builder_init(&builder, G_VARIANT_TYPE("ai"));
    for (size_t i = 0; i < count; i++)
        g_variant_builder_add(&builder, "i", items[i]);

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
        *data = array_i32_variant(devopts, ARRAY_SIZE(devopts));
        return SR_OK;
    case SR_CONF_SAMPLERATE:
        *data = g_variant_new_from_data(G_VARIANT_TYPE("at"),
            samplerates, ARRAY_SIZE(samplerates) * sizeof(uint64_t),
            TRUE, NULL, NULL);
        return SR_OK;
    case SR_CONF_TRIGGER_MATCH:
        *data = array_i32_variant(trigger_matches, ARRAY_SIZE(trigger_matches));
        return SR_OK;
    default:
        return SR_ERR_NA;
    }
}

static int hw_dev_open(struct sr_dev_inst *sdi)
{
    if (!sdi)
        return SR_ERR_ARG;

    sdi->status = SR_ST_ACTIVE;
    return SR_OK;
}

static int hw_dev_close(struct sr_dev_inst *sdi)
{
    if (!sdi)
        return SR_ERR_ARG;

    sdi->status = SR_ST_INACTIVE;
    return SR_OK;
}

static int hw_dev_destroy(struct sr_dev_inst *sdi)
{
    if (!sdi)
        return SR_OK;

    sr_dev_inst_free(sdi);
    return SR_OK;
}

static int hw_dev_acquisition_start(struct sr_dev_inst *sdi, void *cb_data)
{
    (void)cb_data;

    if (!sdi || !sdi->priv)
        return SR_ERR_ARG;

    struct upstream_demo_context *devc = sdi->priv;
    uint8_t sample_data[128];
    struct sr_datafeed_logic logic;
    struct sr_datafeed_packet packet;

    memset(sample_data, 0xaa, sizeof(sample_data));
    logic.length = sizeof(sample_data);
    logic.unitsize = 1;
    logic.data = sample_data;

    packet.type = SR_DF_LOGIC;
    packet.status = SR_PKT_OK;
    packet.payload = &logic;
    ds_data_forward(sdi, &packet);

    packet.type = SR_DF_END;
    packet.status = SR_PKT_OK;
    packet.payload = NULL;
    ds_data_forward(sdi, &packet);

    (void)devc;
    return SR_OK;
}

static int hw_dev_acquisition_stop(const struct sr_dev_inst *sdi, void *cb_data)
{
    (void)sdi;
    (void)cb_data;
    return SR_OK;
}

SR_PRIV struct sr_dev_driver upstream_demo_driver_info = {
    .name = "upstream-demo",
    .longname = "Upstream Compat Demo",
    .api_version = 1,
    .driver_type = DRIVER_TYPE_DEMO,
    .init = hw_init,
    .cleanup = hw_cleanup,
    .scan = hw_scan,
    .dev_mode_list = hw_dev_mode_list,
    .config_get = config_get,
    .config_set = config_set,
    .config_list = config_list,
    .dev_open = hw_dev_open,
    .dev_close = hw_dev_close,
    .dev_destroy = hw_dev_destroy,
    .dev_status_get = NULL,
    .dev_acquisition_start = hw_dev_acquisition_start,
    .dev_acquisition_stop = hw_dev_acquisition_stop,
    .priv = NULL,
};
```

- [ ] **Step 4: Register the optional driver**

Modify `libsigrok/hwdriver.c` near driver externs:

```c
#ifdef HAVE_UPSTREAM_COMPAT_DEMO
extern SR_PRIV struct sr_dev_driver upstream_demo_driver_info;
#endif
```

Modify `drivers_list[]`:

```c
#ifdef HAVE_UPSTREAM_COMPAT_DEMO
    &upstream_demo_driver_info,
#endif
```

- [ ] **Step 5: Build default configuration**

Run:

```bash
cmake --build build.macOS --target DSView-test
cmake --build build.macOS --target DSView
```

Expected: builds without the upstream demo compiled in.

- [ ] **Step 6: Configure and build with upstream-compat demo enabled**

Run:

```bash
cmake -S . -B build.upstream-compat -DDSVIEW_ENABLE_UPSTREAM_COMPAT_DEMO=ON
cmake --build build.upstream-compat --target DSView-test
cmake --build build.upstream-compat --target DSView
```

Expected: builds with `HAVE_UPSTREAM_COMPAT_DEMO`.

- [ ] **Step 7: Commit**

```bash
git add CMakeLists.txt libsigrok/hardware/upstream-demo/upstream_demo.c libsigrok/hardware/upstream-demo/upstream_demo.h libsigrok/hwdriver.c
git commit -m "feat: add upstream compat demo driver gate"
```

## Task 4: Scan Integration for Upstream-Compat Demo

**Files:**
- Modify: `libsigrok/lib_main.c`
- Modify: `libsigrok/hardware/upstream-demo/upstream_demo.c`
- Test: manual run with `DSVIEW_ENABLE_UPSTREAM_COMPAT_DEMO=ON`

**Interfaces:**
- Consumes: optional `upstream_demo_driver_info` registered in `sr_driver_list()`.
- Produces: upstream-compat demo appears in `ds_get_device_list()` only when the flag is enabled.

- [ ] **Step 1: Add scan helper behavior to include demo-source devices**

In `libsigrok/lib_main.c`, locate the code path that scans all hardware devices, currently around `process_attach_event()` and `make_demo_device_to_list()`. Add a narrow helper:

```c
static void append_scanned_devices_from_driver(struct sr_dev_driver *driver, GSList *options)
{
    GSList *devices;
    GSList *l;

    if (!driver || !driver->scan)
        return;

    devices = driver->scan(options);
    for (l = devices; l; l = l->next) {
        struct sr_dev_inst *sdi = (struct sr_dev_inst *)l->data;
        if (!sdi)
            continue;
        sdi->driver = driver;
        if (sdi->source_kind == DS_DEVICE_SOURCE_UNKNOWN)
            ds_device_source_set(sdi, DS_DEVICE_SOURCE_NATIVE);
        lib_ctx.device_list = g_slist_append(lib_ctx.device_list, sdi);
    }
    g_slist_free(devices);
}
```

Then call it only for the upstream-compat demo driver under `#ifdef HAVE_UPSTREAM_COMPAT_DEMO` after `make_demo_device_to_list()` in `ds_lib_init()`:

```c
#ifdef HAVE_UPSTREAM_COMPAT_DEMO
    append_scanned_devices_from_driver(&upstream_demo_driver_info, NULL);
#endif
```

Add the extern guarded by `#ifdef HAVE_UPSTREAM_COMPAT_DEMO` near the other file-scope declarations:

```c
#ifdef HAVE_UPSTREAM_COMPAT_DEMO
extern SR_PRIV struct sr_dev_driver upstream_demo_driver_info;
#endif
```

- [ ] **Step 2: Build with upstream-compat demo enabled**

Run:

```bash
cmake -S . -B build.upstream-compat -DDSVIEW_ENABLE_UPSTREAM_COMPAT_DEMO=ON
cmake --build build.upstream-compat --target DSView
```

Expected: build succeeds.

- [ ] **Step 3: Smoke-run app manually**

Run:

```bash
open build.upstream-compat/PXTOOL.app
```

Expected: application starts. Device selector includes the normal DSView demo device and an `Upstream Compat Demo` device. Selecting it should not crash.

- [ ] **Step 4: Commit**

```bash
git add libsigrok/lib_main.c libsigrok/hardware/upstream-demo/upstream_demo.c
git commit -m "feat: scan upstream compat demo devices"
```

## Task 5: Capability-Based UI Guard for Sampling Controls

**Files:**
- Modify: `PXTOOL/pv/toolbars/samplingbar.cpp`
- Test: manual UI smoke with native demo and upstream-compat demo

**Interfaces:**
- Consumes:
  - `DeviceAgent::supports_config(int key)`
  - `DeviceAgent::supports_stream()`
  - `DeviceAgent::supports_advanced_trigger()`
- Produces: sampling controls avoid querying unsupported configs for upstream-compat or later upstream devices.

- [ ] **Step 1: Identify unsupported config reads in sampling bar**

Open `PXTOOL/pv/toolbars/samplingbar.cpp` and find places that read:

```cpp
SR_CONF_STREAM
SR_CONF_STREAM_BUFF
SR_CONF_DISK_CACHE_ENABLE
SR_CONF_DISK_CACHE_PATH
SR_CONF_HW_DEPTH
SR_CONF_MAX_DSO_SAMPLERATE
SR_CONF_MAX_DSO_SAMPLELIMITS
SR_CONF_HAVE_ADVANCED_TRIGGER
```

For each block, add a guard before the read:

```cpp
if (!_device_agent->supports_config(SR_CONF_STREAM)) {
    // Keep existing default UI state for devices without stream mode.
} else {
    _device_agent->get_config_bool(SR_CONF_STREAM, stream_mode);
}
```

For advanced trigger:

```cpp
const bool has_advanced_trigger = _device_agent->supports_advanced_trigger();
```

Use existing local variable names and UI update patterns in the file.

- [ ] **Step 2: Build**

Run:

```bash
cmake --build build.macOS --target DSView
cmake --build build.upstream-compat --target DSView
```

Expected: both builds succeed.

- [ ] **Step 3: Manual UI smoke**

Run:

```bash
open build.upstream-compat/PXTOOL.app
```

Expected:

- DSView native demo still shows existing controls.
- Upstream Compat Demo can be selected.
- Stream/disk-cache/advanced-trigger controls do not appear enabled unless supported.
- Sampling rate and sample count remain available because the POC driver supports them.

- [ ] **Step 4: Commit**

```bash
git add PXTOOL/pv/toolbars/samplingbar.cpp
git commit -m "fix: guard sampling controls by device capability"
```

## Task 6: Upstream Import Inventory and Next Driver Decision

**Files:**
- Create: `docs/libsigrok-upstream-driver-inventory.md`
- Modify: `docs/superpowers/specs/2026-07-11-libsigrok-upstream-compat-design.md` only if the decision changes the spec.

**Interfaces:**
- Consumes: local upstream reference `/Users/yuanji/Desktop/project/libsigrok`.
- Produces: explicit next-driver allowlist with dependency and source-file list.

- [ ] **Step 1: Create inventory document**

Create `docs/libsigrok-upstream-driver-inventory.md`:

```markdown
# Libsigrok Upstream Driver Inventory

Date: 2026-07-11

## Rules

- `/Users/yuanji/Desktop/project/libsigrok` is a reference source only.
- Copied source files must use DSView/PXTOOL file headers.
- Drivers are enabled through an allowlist.
- First real upstream driver should be waveform-producing.

## Candidate: fx2lafw

Purpose: USB logic analyzer class.

Likely source files:

- `src/hardware/fx2lafw/api.c`
- `src/hardware/fx2lafw/protocol.c`
- `src/hardware/fx2lafw/protocol.h`
- upstream USB helpers as needed
- upstream EZ-USB firmware helpers as needed

Dependencies:

- libusb
- firmware files for selected profiles

Risks:

- firmware renumeration flow
- async USB transfer lifecycle
- upstream session and trigger struct differences

Decision: recommended as first real USB logic analyzer after the POC driver.

## Candidate: rigol-ds

Purpose: SCPI oscilloscope class.

Likely source files:

- `src/hardware/rigol-ds/api.c`
- `src/hardware/rigol-ds/protocol.c`
- `src/hardware/rigol-ds/protocol.h`
- SCPI helpers
- serial or TCP helpers depending on selected connection mode

Dependencies:

- libserialport for serial mode
- TCP socket support for network mode

Risks:

- SCPI stack import size
- analog data mapping into DSView DSO path

Decision: second real driver class after USB logic analyzer path is stable.

## Candidate: hantek-4032l

Purpose: USB logic analyzer class.

Likely source files:

- `src/hardware/hantek-4032l/api.c`
- `src/hardware/hantek-4032l/protocol.c`
- `src/hardware/hantek-4032l/protocol.h`

Dependencies:

- libusb

Risks:

- device availability for manual smoke tests
- trigger/config mapping differences

Decision: useful alternate first real USB logic analyzer if hardware is available.
```

- [ ] **Step 2: Verify inventory against local upstream files**

Run:

```bash
test -f /Users/yuanji/Desktop/project/libsigrok/src/hardware/fx2lafw/api.c
test -f /Users/yuanji/Desktop/project/libsigrok/src/hardware/rigol-ds/api.c
test -f /Users/yuanji/Desktop/project/libsigrok/src/hardware/hantek-4032l/api.c
```

Expected: all commands exit 0.

- [ ] **Step 3: Commit**

```bash
git add -f docs/libsigrok-upstream-driver-inventory.md
git commit -m "docs: inventory first upstream driver candidates"
```

## Task 7: Final Verification and Handoff

**Files:**
- No source changes expected.

**Interfaces:**
- Consumes: outputs of Tasks 1-6.
- Produces: verified first-slice branch ready for the next real-driver implementation plan.

- [ ] **Step 1: Run default build verification**

Run:

```bash
cmake --build build.macOS --target DSView-test
./build.macOS/DSView-test
cmake --build build.macOS --target DSView
```

Expected:

- Boost tests pass.
- Default app build passes.
- No upstream-compat demo is compiled into the default build.

- [ ] **Step 2: Run upstream-compat build verification**

Run:

```bash
cmake -S . -B build.upstream-compat -DDSVIEW_ENABLE_UPSTREAM_COMPAT_DEMO=ON
cmake --build build.upstream-compat --target DSView-test
./build.upstream-compat/DSView-test --run_test=device_source
cmake --build build.upstream-compat --target DSView
```

Expected:

- Device-source tests pass.
- Upstream-compat build passes.

- [ ] **Step 3: Manual UI smoke**

Run:

```bash
open build.upstream-compat/PXTOOL.app
```

Expected:

- Native DSView demo still works.
- Upstream Compat Demo appears when flag is enabled.
- Selecting it does not crash.
- Sampling controls only show supported controls.
- Starting capture produces a short logic waveform or a clean end-of-stream with no crash.

- [ ] **Step 4: Record verification outcome**

Add a short note to the final response with:

```text
Default build: pass/fail
Upstream-compat build: pass/fail
Manual smoke: pass/fail/not run
Known limitations: first slice uses POC upstream-compat driver; real upstream drivers remain next work.
```

- [ ] **Step 5: Commit any verification doc updates**

If verification required doc changes:

```bash
git add <changed-doc-files>
git commit -m "docs: record upstream compat verification"
```

If no files changed, do not create an empty commit.

---

## Self-Review

Spec coverage:

- Existing DSView devices protected: Tasks 1, 2, 5, and 7 keep default paths and require default build verification.
- `ds_*` compatibility facade retained: Tasks 1 and 2 add APIs without removing existing calls.
- Device manager/source kind: Task 1 adds source-kind model.
- Capability mapper: Task 1 adds helper functions; Task 5 uses them in UI.
- Allowlist mechanism: Task 3 adds disabled-by-default CMake gate.
- Upstream copied-header rule: Global Constraints and Task 6 inventory rule capture it. The POC driver uses DSView headers because it is DSView-native, not copied upstream code.
- First waveform proof of concept: Tasks 3 and 4 add upstream-compat demo.
- Next real driver path: Task 6 records candidate drivers and dependencies.

Placeholder scan:

- No placeholder red flags are intentionally left.

Type consistency:

- Capability names are defined in Task 1 and consumed in Tasks 2 and 5.
- Source-kind names are defined in Task 1 and consumed in Tasks 3 and 4.
- Build flag is `DSVIEW_ENABLE_UPSTREAM_COMPAT_DEMO` consistently across Tasks 3, 4, and 7.
