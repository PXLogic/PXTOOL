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
