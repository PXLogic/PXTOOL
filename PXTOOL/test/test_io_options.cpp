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
 */

#include <boost/test/unit_test.hpp>

extern "C" {
#include "libsigrok-internal.h"

struct sr_dev_inst *make_test_sdi(void);
}

BOOST_AUTO_TEST_SUITE(io_migration_options)

BOOST_AUTO_TEST_CASE(creates_and_frees_output_with_default_options)
{
    const sr_output_module *module = sr_output_find(const_cast<char *>("null"));
    BOOST_REQUIRE(module != nullptr);

    const sr_output *output = sr_output_new(module, nullptr, make_test_sdi());
    BOOST_REQUIRE(output != nullptr);
    BOOST_CHECK_EQUAL(sr_output_free(output), SR_OK);
}

BOOST_AUTO_TEST_SUITE_END()
