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

#ifndef DSVIEW_PV_DATA_ANALOGPACKETADAPTER_H
#define DSVIEW_PV_DATA_ANALOGPACKETADAPTER_H

#include <cstdint>
#include <vector>

#include <QString>
#include <QVector>

extern "C" {
#include "libsigrok.h"
}

namespace pv {
namespace data {

struct AnalogChannelRef {
    AnalogChannelRef(const QString &name, uint16_t index);
    explicit AnalogChannelRef(sr_channel *channel);

    QString name;
    uint16_t index = 0;
    sr_channel *channel = nullptr;
};

struct Unsigned8AnalogConversion {
    uint32_t sourceIndex = 0;
    double zeroOffset = 0.0;
    double unitsPerCode = 0.0;
};

struct AnalogPacket {
    AnalogPacket();
    ~AnalogPacket();

    AnalogPacket(const AnalogPacket &) = delete;
    AnalogPacket &operator=(const AnalogPacket &) = delete;
    AnalogPacket(AnalogPacket &&other) noexcept;
    AnalogPacket &operator=(AnalogPacket &&other) noexcept;

    sr_datafeed_packet packet{};
    sr_datafeed_analog analog{};
    sr_datafeed_meta meta{};
    uint64_t samplerate = 0;
    std::vector<float> samples;
    GSList *channels = nullptr;
    sr_analog_encoding encoding{};
    sr_analog_meaning meaning{};
    sr_analog_spec spec{};
    sr_config samplerateConfig{};

private:
    void release();
    void rebind();
    void moveFrom(AnalogPacket &&other) noexcept;

    QVector<sr_channel *> ownedChannels_;

    friend AnalogPacket makeAnalogPacket(
        const QVector<AnalogChannelRef> &channels,
        const std::vector<float> &interleavedSamples,
        uint64_t samplesPerChannel,
        uint64_t samplerate,
        int mq,
        int unit);
};

AnalogPacket makeAnalogPacket(
    const QVector<AnalogChannelRef> &channels,
    const std::vector<float> &interleavedSamples,
    uint64_t samplesPerChannel,
    uint64_t samplerate,
    int mq,
    int unit);

std::vector<float> convertUnsigned8AnalogSamples(
    const uint8_t *interleavedSamples,
    uint64_t samplesPerChannel,
    uint32_t sourceChannelCount,
    const QVector<Unsigned8AnalogConversion> &conversions);

} // namespace data
} // namespace pv

#endif // DSVIEW_PV_DATA_ANALOGPACKETADAPTER_H
