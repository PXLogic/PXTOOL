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

#include "libsigrok-internal.h"

#include <stdbool.h>
#include <string.h>

static uint64_t test_samplerate;
enum { TEST_LOGIC_PREFIX_CAPACITY = 256 };

static struct {
    unsigned int header_packets;
    unsigned int meta_packets;
    unsigned int logic_packets;
    uint64_t logic_samples;
    unsigned int analog_packets;
    uint64_t analog_samples;
    uint64_t samplerate;
    uint64_t sample_limit;
    uint8_t logic_prefix[TEST_LOGIC_PREFIX_CAPACITY];
    size_t logic_prefix_len;
    bool saw_end;
} test_input_observer;

struct sr_dev_inst *make_test_sdi(void)
{
    static struct sr_channel channel = {
        .index = 0,
        .type = SR_CHANNEL_LOGIC,
        .enabled = TRUE,
        .name = "D0",
    };
    static struct sr_dev_inst sdi;
    static gboolean initialized;

    if (!initialized) {
        sdi.channels = g_slist_append(NULL, &channel);
        initialized = TRUE;
    }
    sdi.priv = NULL;

    return &sdi;
}

struct sr_dev_inst *make_test_sdi_with_samplerate(uint64_t samplerate)
{
    struct sr_dev_inst *sdi = make_test_sdi();

    test_samplerate = samplerate;
    sdi->priv = &test_samplerate;
    return sdi;
}

uint64_t test_sdi_samplerate_get(const struct sr_dev_inst *sdi)
{
    if (!sdi || !sdi->priv)
        return 0;

    return *(const uint64_t *)sdi->priv;
}

void test_input_observer_reset(void)
{
    memset(&test_input_observer, 0, sizeof(test_input_observer));
}

static unsigned int test_logic_channel_count(const struct sr_dev_inst *sdi)
{
    const GSList *item;
    unsigned int count = 0;

    if (!sdi)
        return 0;

    for (item = sdi->channels; item; item = item->next) {
        const struct sr_channel *channel = item->data;
        if (channel && channel->type == SR_CHANNEL_LOGIC && channel->enabled)
            count++;
    }

    return count;
}

void test_input_observer_record_packet(const struct sr_dev_inst *sdi,
    const struct sr_datafeed_packet *packet)
{
    const struct sr_datafeed_logic *logic;
    const struct sr_datafeed_analog *analog;
    const struct sr_datafeed_meta *meta;
    const GSList *item;
    const struct sr_config *config;

    if (!packet)
        return;

    switch (packet->type) {
    case SR_DF_HEADER:
        test_input_observer.header_packets++;
        break;
    case SR_DF_META:
        test_input_observer.meta_packets++;
        meta = packet->payload;
        if (!meta)
            break;
        for (item = meta->config; item; item = item->next) {
            config = item->data;
            if (config && config->key == SR_CONF_SAMPLERATE && config->data)
                test_input_observer.samplerate =
                    g_variant_get_uint64(config->data);
            if (config && config->key == SR_CONF_LIMIT_SAMPLES && config->data)
                test_input_observer.sample_limit =
                    g_variant_get_uint64(config->data);
        }
        break;
    case SR_DF_LOGIC:
        logic = packet->payload;
        test_input_observer.logic_packets++;
        if (logic && logic->unitsize) {
            unsigned int logic_channels = test_logic_channel_count(sdi);
            size_t copy_len = logic->length;
            if (copy_len > TEST_LOGIC_PREFIX_CAPACITY - test_input_observer.logic_prefix_len)
                copy_len = TEST_LOGIC_PREFIX_CAPACITY - test_input_observer.logic_prefix_len;
            if (copy_len && logic->data) {
                memcpy(&test_input_observer.logic_prefix[test_input_observer.logic_prefix_len],
                       logic->data, copy_len);
                test_input_observer.logic_prefix_len += copy_len;
            }
            if (logic->format == LA_CROSS_DATA && logic_channels &&
                logic->length % (logic_channels * 8) == 0)
                test_input_observer.logic_samples += logic->length * 8 /
                    logic_channels;
            else
                test_input_observer.logic_samples += logic->length / logic->unitsize;
        }
        break;
    case SR_DF_ANALOG:
        analog = packet->payload;
        test_input_observer.analog_packets++;
        if (analog)
            test_input_observer.analog_samples += analog->num_samples;
        break;
    case SR_DF_END:
        test_input_observer.saw_end = true;
        break;
    default:
        break;
    }
}

unsigned int test_input_observer_logic_packets(void)
{
    return test_input_observer.logic_packets;
}

unsigned int test_input_observer_header_packets(void)
{
    return test_input_observer.header_packets;
}

unsigned int test_input_observer_meta_packets(void)
{
    return test_input_observer.meta_packets;
}

uint64_t test_input_observer_logic_samples(void)
{
    return test_input_observer.logic_samples;
}

unsigned int test_input_observer_analog_packets(void)
{
    return test_input_observer.analog_packets;
}

uint64_t test_input_observer_analog_samples(void)
{
    return test_input_observer.analog_samples;
}

uint64_t test_input_observer_samplerate(void)
{
    return test_input_observer.samplerate;
}

uint64_t test_input_observer_sample_limit(void)
{
    return test_input_observer.sample_limit;
}

bool test_input_observer_saw_end(void)
{
    return test_input_observer.saw_end;
}

size_t test_input_observer_logic_prefix_len(void)
{
    return test_input_observer.logic_prefix_len;
}

const uint8_t *test_input_observer_logic_prefix(void)
{
    return test_input_observer.logic_prefix;
}
