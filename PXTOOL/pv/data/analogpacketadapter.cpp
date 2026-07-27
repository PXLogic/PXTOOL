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

#include "analogpacketadapter.h"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

#include <glib.h>

extern "C" {
#include "libsigrok-internal.h"
}

namespace pv {
namespace data {

AnalogChannelRef::AnalogChannelRef(const QString &channelName,
                                   uint16_t channelIndex)
    : name(channelName), index(channelIndex)
{
}

AnalogChannelRef::AnalogChannelRef(sr_channel *sourceChannel)
    : name(sourceChannel && sourceChannel->name
               ? QString::fromUtf8(sourceChannel->name)
               : QString()),
      index(sourceChannel ? sourceChannel->index : 0),
      channel(sourceChannel)
{
}

AnalogPacket::AnalogPacket() = default;

AnalogPacket::~AnalogPacket()
{
    release();
}

AnalogPacket::AnalogPacket(AnalogPacket &&other) noexcept
{
    moveFrom(std::move(other));
}

AnalogPacket &AnalogPacket::operator=(AnalogPacket &&other) noexcept
{
    if (this != &other) {
        release();
        moveFrom(std::move(other));
    }
    return *this;
}

void AnalogPacket::release()
{
    g_slist_free(channels);
    channels = nullptr;

    g_slist_free(meta.config);
    meta.config = nullptr;

    if (samplerateConfig.data) {
        g_variant_unref(samplerateConfig.data);
        samplerateConfig.data = nullptr;
    }

    for (sr_channel *channel : ownedChannels_) {
        g_free(channel->name);
        g_free(channel);
    }
    ownedChannels_.clear();
}

void AnalogPacket::rebind()
{
    packet.payload = &analog;
    analog.data = samples.data();
    analog.encoding = &encoding;
    analog.meaning = &meaning;
    analog.spec = &spec;
    meaning.channels = channels;
    if (meta.config)
        meta.config->data = &samplerateConfig;
}

void AnalogPacket::moveFrom(AnalogPacket &&other) noexcept
{
    packet = other.packet;
    analog = other.analog;
    meta = other.meta;
    samplerate = other.samplerate;
    samples = std::move(other.samples);
    channels = other.channels;
    encoding = other.encoding;
    meaning = other.meaning;
    spec = other.spec;
    samplerateConfig = other.samplerateConfig;
    ownedChannels_ = std::move(other.ownedChannels_);

    other.ownedChannels_.clear();
    other.channels = nullptr;
    other.meta.config = nullptr;
    other.samplerateConfig.data = nullptr;
    other.packet.payload = nullptr;
    other.analog.data = nullptr;
    other.analog.encoding = nullptr;
    other.analog.meaning = nullptr;
    other.analog.spec = nullptr;
    other.meaning.channels = nullptr;

    rebind();
}

AnalogPacket makeAnalogPacket(
    const QVector<AnalogChannelRef> &channelRefs,
    const std::vector<float> &interleavedSamples,
    uint64_t samplesPerChannel,
    uint64_t samplerate,
    int mq,
    int unit)
{
    if (channelRefs.isEmpty())
        throw std::invalid_argument("Analog packet requires at least one channel");
    if (samplesPerChannel == 0 || samplerate == 0)
        throw std::invalid_argument("Analog packet requires samples and samplerate");
    if (samplesPerChannel > (std::numeric_limits<uint32_t>::max)())
        throw std::invalid_argument("Analog packet sample count is too large");

    const uint64_t channelCount = static_cast<uint64_t>(channelRefs.size());
    if (samplesPerChannel >
        (std::numeric_limits<uint64_t>::max)() / channelCount)
        throw std::invalid_argument("Analog packet sample count overflows");
    if (interleavedSamples.size() != samplesPerChannel * channelCount)
        throw std::invalid_argument("Analog samples are not channel-interleaved");

    AnalogPacket result;
    result.samples = interleavedSamples;
    result.samplerate = samplerate;

    for (const AnalogChannelRef &ref : channelRefs) {
        sr_channel *channel = ref.channel;
        if (channel) {
            if (channel->type != SR_CHANNEL_ANALOG || !channel->enabled ||
                !channel->name)
                throw std::invalid_argument("Invalid borrowed analog channel");
        } else {
            if (ref.name.isEmpty())
                throw std::invalid_argument("Analog channel requires a name");
            channel = g_new0(sr_channel, 1);
            channel->index = ref.index;
            channel->type = SR_CHANNEL_ANALOG;
            channel->enabled = TRUE;
            channel->name = g_strdup(ref.name.toUtf8().constData());
            result.ownedChannels_.append(channel);
        }

        if (g_slist_find(result.channels, channel))
            throw std::invalid_argument("Analog channels must be unique");
        result.channels = g_slist_append(result.channels, channel);
    }

    if (sr_analog_init(&result.analog, &result.encoding, &result.meaning,
                       &result.spec, 5) != SR_OK)
        throw std::runtime_error("Failed to initialize analog packet");

    result.packet.type = SR_DF_ANALOG;
    result.packet.status = SR_PKT_OK;
    result.analog.num_samples = static_cast<uint32_t>(samplesPerChannel);
    result.meaning.mq = static_cast<sr_mq>(mq);
    result.meaning.unit = static_cast<sr_unit>(unit);
    result.meaning.mqflags = static_cast<sr_mqflag>(0);

    result.samplerateConfig.key = SR_CONF_SAMPLERATE;
    result.samplerateConfig.data =
        g_variant_ref_sink(g_variant_new_uint64(samplerate));
    result.meta.config = g_slist_append(nullptr, &result.samplerateConfig);
    result.rebind();

    return result;
}

std::vector<float> convertUnsigned8AnalogSamples(
    const uint8_t *interleavedSamples,
    uint64_t samplesPerChannel,
    uint32_t sourceChannelCount,
    const QVector<Unsigned8AnalogConversion> &conversions)
{
    if (!interleavedSamples || samplesPerChannel == 0 ||
        sourceChannelCount == 0 || conversions.isEmpty())
        throw std::invalid_argument("Invalid unsigned analog sample source");

    const uint64_t outputChannelCount =
        static_cast<uint64_t>(conversions.size());
    if (samplesPerChannel >
        (std::numeric_limits<size_t>::max)() / outputChannelCount)
        throw std::invalid_argument("Converted analog sample count overflows");

    for (const Unsigned8AnalogConversion &conversion : conversions) {
        if (conversion.sourceIndex >= sourceChannelCount ||
            !std::isfinite(conversion.zeroOffset) ||
            !std::isfinite(conversion.unitsPerCode) ||
            conversion.unitsPerCode == 0.0)
            throw std::invalid_argument("Invalid analog channel conversion");
    }

    std::vector<float> converted(
        static_cast<size_t>(samplesPerChannel * outputChannelCount));
    for (uint64_t sample = 0; sample < samplesPerChannel; ++sample) {
        for (int channel = 0; channel < conversions.size(); ++channel) {
            const Unsigned8AnalogConversion &conversion = conversions[channel];
            const uint8_t raw = interleavedSamples[
                sample * sourceChannelCount + conversion.sourceIndex];
            const double value =
                (conversion.zeroOffset - raw) * conversion.unitsPerCode;
            if (!std::isfinite(value) ||
                value > (std::numeric_limits<float>::max)() ||
                value < -(std::numeric_limits<float>::max)())
                throw std::invalid_argument("Converted analog value is invalid");
            converted[sample * outputChannelCount + channel] =
                static_cast<float>(value);
        }
    }

    return converted;
}

} // namespace data
} // namespace pv
