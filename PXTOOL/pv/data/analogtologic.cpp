#include "analogtologic.h"

#include "analogsnapshot.h"
#include "logicsnapshot.h"

#include <stdexcept>

namespace pv::data {

std::vector<uint8_t> AnalogToLogic::convert(const AnalogSnapshot &source,
                                            uint32_t channel,
                                            const Config &config)
{
    if (config.mode == Mode::Disabled)
        return {};
    if (channel >= source.channel_count())
        throw std::invalid_argument("Analog channel is out of range");
    if (config.mode == Mode::Schmitt && config.low >= config.high)
        throw std::invalid_argument("Schmitt low threshold must be below high threshold");

    std::vector<uint8_t> result;
    result.reserve(source.sample_count());
    bool state = false;
    for (uint64_t sample = 0; sample < source.sample_count(); ++sample) {
        const double value = source.sample_as_double(channel, sample);
        if (config.mode == Mode::Threshold)
            state = value >= config.threshold;
        else if (value >= config.high)
            state = true;
        else if (value <= config.low)
            state = false;
        result.push_back(state ? 1 : 0);
    }
    return result;
}

bool AnalogToLogic::build_snapshot(const AnalogSnapshot &source,
                                   uint32_t channel_order,
                                   const Config &config,
                                   sr_channel *channel,
                                   LogicSnapshot &target)
{
    if (!channel || config.mode == Mode::Disabled)
        return false;

    const std::vector<uint8_t> bits = convert(source, channel_order, config);
    target.clear();
    if (bits.empty())
        return false;

    std::vector<uint8_t> packed((bits.size() + 7) / 8, 0);
    for (size_t i = 0; i < bits.size(); ++i)
        if (bits[i])
            packed[i / 8] |= static_cast<uint8_t>(1u << (i % 8));

    sr_datafeed_logic logic{};
    logic.data = packed.data();
    logic.length = packed.size();
    logic.unitsize = 1;
    GSList *channels = g_slist_append(nullptr, channel);
    target.first_payload(logic, bits.size(), channels, false);
    g_slist_free(channels);
    target.capture_ended();
    return target.get_sample_count() == bits.size() && target.has_data(channel->index);
}

} // namespace pv::data
