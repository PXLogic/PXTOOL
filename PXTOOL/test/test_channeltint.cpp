#include <boost/test/unit_test.hpp>

#include <QColor>
#include <QRect>

#include <libsigrok.h>

#include "../pv/view/channeltint.h"

BOOST_AUTO_TEST_SUITE(channel_tint)

BOOST_AUTO_TEST_CASE(light_theme_uses_subtle_alpha)
{
    const QColor tint = pv::view::channel_tint_color(
        QColor(238, 178, 17), QColor(250, 250, 250), true, 1);

    BOOST_TEST(tint.isValid());
    BOOST_TEST(tint.red() == 238);
    BOOST_TEST(tint.green() == 178);
    BOOST_TEST(tint.blue() == 17);
    BOOST_TEST(tint.alpha() == 24);
}

BOOST_AUTO_TEST_CASE(dark_theme_uses_visible_alpha)
{
    const QColor tint = pv::view::channel_tint_color(
        QColor(17, 133, 209), QColor(24, 24, 24), true, 1);

    BOOST_TEST(tint.isValid());
    BOOST_TEST(tint.red() == 17);
    BOOST_TEST(tint.green() == 133);
    BOOST_TEST(tint.blue() == 209);
    BOOST_TEST(tint.alpha() == 38);
}

BOOST_AUTO_TEST_CASE(ineligible_trace_returns_invalid_color)
{
    BOOST_TEST(!pv::view::channel_tint_color(QColor(), QColor(250, 250, 250), true, 1).isValid());
    BOOST_TEST(!pv::view::channel_tint_color(QColor(1, 2, 3), QColor(250, 250, 250), false, 1).isValid());
    BOOST_TEST(!pv::view::channel_tint_color(QColor(1, 2, 3), QColor(250, 250, 250), true, 0).isValid());
}

BOOST_AUTO_TEST_CASE(row_rect_expands_by_margin_and_clips_to_viewport)
{
    const QRect rect = pv::view::channel_tint_rect(800, 300, 60, 40, 5);

    BOOST_TEST(rect.left() == 0);
    BOOST_TEST(rect.right() == 799);
    BOOST_TEST(rect.top() == 35);
    BOOST_TEST(rect.bottom() == 84);
}

BOOST_AUTO_TEST_CASE(row_rect_clips_top_and_bottom_edges)
{
    const QRect top = pv::view::channel_tint_rect(800, 300, 8, 40, 5);
    const QRect bottom = pv::view::channel_tint_rect(800, 300, 292, 40, 5);

    BOOST_TEST(top.top() == 0);
    BOOST_TEST(top.bottom() == 32);
    BOOST_TEST(bottom.top() == 267);
    BOOST_TEST(bottom.bottom() == 299);
}

BOOST_AUTO_TEST_CASE(invalid_geometry_returns_empty_rect)
{
    BOOST_TEST(pv::view::channel_tint_rect(0, 300, 60, 40, 5).isEmpty());
    BOOST_TEST(pv::view::channel_tint_rect(800, 0, 60, 40, 5).isEmpty());
    BOOST_TEST(pv::view::channel_tint_rect(800, 300, 60, 0, 5).isEmpty());
}

BOOST_AUTO_TEST_CASE(applies_only_to_row_oriented_trace_types)
{
    BOOST_TEST(pv::view::channel_tint_accepts_signal_type(SR_CHANNEL_LOGIC));
    BOOST_TEST(pv::view::channel_tint_accepts_signal_type(SR_CHANNEL_DECODER));
    BOOST_TEST(pv::view::channel_tint_accepts_signal_type(SR_CHANNEL_GROUP));
    BOOST_TEST(pv::view::channel_tint_accepts_signal_type(SR_CHANNEL_ANALOG));
    BOOST_TEST(pv::view::channel_tint_accepts_signal_type(SR_CHANNEL_MATH));

    BOOST_TEST(!pv::view::channel_tint_accepts_signal_type(SR_CHANNEL_DSO));
    BOOST_TEST(!pv::view::channel_tint_accepts_signal_type(SR_CHANNEL_FFT));
    BOOST_TEST(!pv::view::channel_tint_accepts_signal_type(SR_CHANNEL_LISSAJOUS));
}

BOOST_AUTO_TEST_SUITE_END()
