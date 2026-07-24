/*
 * This file is part of the PXTOOL project.
 *
 * Copyright (C) 2026 DreamSourceLab <support@dreamsourcelab.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib.h>
#include <stdio.h>

#include "libsigrokdecode.h"

static struct srd_channel test_channels[] = {
    { "clk", "CLK", "Clock", 0, SRD_CHANNEL_SCLK, "test_chan_clk" },
};

static struct srd_decoder_option test_options[] = {
    { "mode", "test_opt_mode", "Mode", NULL, NULL },
};

static const char *test_inputs[] = { "logic" };
static const char *test_outputs[] = { "test" };

static struct srd_c_decoder test_decoder = {
    .id = "test-c-lifetime",
    .name = "test-c-lifetime",
    .longname = "C decoder lifetime test",
    .desc = "Exercises C decoder metadata ownership",
    .license = "gplv2+",
    .channels = test_channels,
    .num_channels = 1,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = test_options,
    .num_options = 1,
    .num_annotations = 0,
    .ann_labels = NULL,
    .num_annotation_rows = 0,
    .annotation_rows = NULL,
    .inputs = test_inputs,
    .num_inputs = 1,
    .outputs = test_outputs,
    .num_outputs = 1,
    .binary = NULL,
    .num_binary = 0,
    .tags = NULL,
    .num_tags = 0,
};

static int expect_copied_string(const char *label, const char *actual,
    const char *source)
{
    if (actual == source) {
        fprintf(stderr, "%s was not copied\n", label);
        return 1;
    }

    if (g_strcmp0(actual, source) != 0) {
        fprintf(stderr, "%s changed during copy\n", label);
        return 1;
    }

    return 0;
}

int main(void)
{
    struct srd_decoder *registered;
    struct srd_channel *registered_channel;
    struct srd_decoder_option *registered_option;
    int failed = 0;

    test_options[0].def = g_variant_new_string("native");
    test_options[0].values = NULL;
    test_options[0].values = g_slist_append(test_options[0].values,
        g_variant_new_string("native"));
    test_options[0].values = g_slist_append(test_options[0].values,
        g_variant_new_string("detect"));

    if (srd_c_decoder_register(&test_decoder) != SRD_OK) {
        fprintf(stderr, "failed to register test C decoder\n");
        return 1;
    }

    registered = srd_decoder_get_by_id(test_decoder.id);
    if (!registered || !registered->channels || !registered->options) {
        fprintf(stderr, "registered decoder metadata missing\n");
        return 1;
    }

    registered_channel = registered->channels->data;
    failed |= expect_copied_string("channel id", registered_channel->id,
        test_channels[0].id);
    failed |= expect_copied_string("channel name", registered_channel->name,
        test_channels[0].name);
    failed |= expect_copied_string("channel desc", registered_channel->desc,
        test_channels[0].desc);
    failed |= expect_copied_string("channel idn", registered_channel->idn,
        test_channels[0].idn);

    registered_option = registered->options->data;
    failed |= expect_copied_string("option id", registered_option->id,
        test_options[0].id);
    failed |= expect_copied_string("option idn", registered_option->idn,
        test_options[0].idn);
    failed |= expect_copied_string("option desc", registered_option->desc,
        test_options[0].desc);
    if (registered_option->values == test_options[0].values) {
        fprintf(stderr, "option values list was not copied\n");
        failed = 1;
    }

    if (failed)
        return 1;

    if (srd_decoder_unload_all() != SRD_OK) {
        fprintf(stderr, "failed to unload test C decoder\n");
        return 1;
    }

    return 0;
}
