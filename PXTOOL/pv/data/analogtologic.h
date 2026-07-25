#pragma once

#include <cstdint>
#include <vector>

namespace pv::data {

class AnalogSnapshot;

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
};

} // namespace pv::data
