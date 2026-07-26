#include <boost/test/unit_test.hpp>

#include "pv/data/analogpacketadapter.h"
#include "pv/data/analogsnapshot.h"
#include "pv/data/analogtologic.h"

BOOST_AUTO_TEST_SUITE(analog_to_logic)

BOOST_AUTO_TEST_CASE(threshold_converts_each_sample)
{
    pv::data::AnalogSnapshot source;
    const auto packet = pv::data::makeAnalogPacket(
        {{"A0", 0}}, {-1.0F, 0.0F, 0.1F, -0.1F}, 4, 1'000'000,
        SR_MQ_VOLTAGE, SR_UNIT_VOLT);
    BOOST_REQUIRE(source.first_payload(packet.analog, 4, packet.channels));

    pv::data::AnalogToLogic::Config config;
    config.mode = pv::data::AnalogToLogic::Mode::Threshold;
    config.threshold = 0.0;
    const auto bits = pv::data::AnalogToLogic::convert(source, 0, config);
    const std::vector<uint8_t> expected{0, 1, 1, 0};
    BOOST_CHECK_EQUAL_COLLECTIONS(bits.begin(), bits.end(),
                                  expected.begin(), expected.end());
}

BOOST_AUTO_TEST_CASE(schmitt_holds_state_between_thresholds)
{
    pv::data::AnalogSnapshot source;
    const auto packet = pv::data::makeAnalogPacket(
        {{"A0", 0}}, {-1.0F, 0.6F, 0.0F, -0.6F, 0.0F}, 5, 1'000'000,
        SR_MQ_VOLTAGE, SR_UNIT_VOLT);
    BOOST_REQUIRE(source.first_payload(packet.analog, 5, packet.channels));

    pv::data::AnalogToLogic::Config config;
    config.mode = pv::data::AnalogToLogic::Mode::Schmitt;
    config.low = -0.5;
    config.high = 0.5;
    const auto bits = pv::data::AnalogToLogic::convert(source, 0, config);
    const std::vector<uint8_t> expected{0, 1, 1, 0, 0};
    BOOST_CHECK_EQUAL_COLLECTIONS(bits.begin(), bits.end(), expected.begin(), expected.end());
}

BOOST_AUTO_TEST_SUITE_END()
