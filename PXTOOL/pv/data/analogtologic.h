#pragma once

#include <cstdint>
#include <vector>
#include <libsigrok.h>

namespace pv::data {

class AnalogSnapshot;
class LogicSnapshot;

class AnalogToLogic
{
public:
    enum class Mode { Disabled, Threshold, Schmitt };

    struct Config {
        Mode mode = Mode::Disabled;
        double threshold = 0.0;
        double low = -0.1;
        double high = 0.1;
    };

    static std::vector<uint8_t> convert(const AnalogSnapshot &source,
                                        uint32_t channel,
                                        const Config &config);

    // Rebuild an owned LogicSnapshot from converted single-channel bits.
    // The caller owns `channel`; it is only borrowed while ingesting.
    static bool build_snapshot(const AnalogSnapshot &source,
                               uint32_t channel_order,
                               const Config &config,
                               sr_channel *channel,
                               LogicSnapshot &target);
};

} // namespace pv::data
