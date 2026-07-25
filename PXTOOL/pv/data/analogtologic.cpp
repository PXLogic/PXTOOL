#include "analogtologic.h"

#include "analogsnapshot.h"

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

} // namespace pv::data
