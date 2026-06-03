#include <boost/test/unit_test.hpp>

#include "../pv/utility/diskcachesettings.h"

using pv::utility::DiskCacheSettings;

BOOST_AUTO_TEST_SUITE(DiskCacheSettingsText)

BOOST_AUTO_TEST_CASE(parse_ram_hot_window_text)
{
    uint64_t gb = 99;

    BOOST_CHECK(DiskCacheSettings::parse_ram_limit_text("32 MB", gb));
    BOOST_CHECK_EQUAL(gb, 0);

    BOOST_CHECK(DiskCacheSettings::parse_ram_limit_text("4", gb));
    BOOST_CHECK_EQUAL(gb, 4);

    BOOST_CHECK(DiskCacheSettings::parse_ram_limit_text("4 GB", gb));
    BOOST_CHECK_EQUAL(gb, 4);

    BOOST_CHECK(!DiskCacheSettings::parse_ram_limit_text("", gb));
    BOOST_CHECK(!DiskCacheSettings::parse_ram_limit_text("unlimited", gb));
}

BOOST_AUTO_TEST_CASE(parse_disk_total_depth_text)
{
    uint64_t gb = 99;

    BOOST_CHECK(DiskCacheSettings::parse_disk_limit_text("128", gb));
    BOOST_CHECK_EQUAL(gb, 128);

    BOOST_CHECK(DiskCacheSettings::parse_disk_limit_text("128 GB", gb));
    BOOST_CHECK_EQUAL(gb, 128);

    BOOST_CHECK(DiskCacheSettings::parse_disk_limit_text("unlimited", gb));
    BOOST_CHECK_EQUAL(gb, 0);

    BOOST_CHECK(!DiskCacheSettings::parse_disk_limit_text("", gb));
    BOOST_CHECK(!DiskCacheSettings::parse_disk_limit_text("32 MB", gb));
}

BOOST_AUTO_TEST_CASE(format_size_text)
{
    BOOST_CHECK_EQUAL(DiskCacheSettings::format_ram_limit_text(0).toStdString(), "32 MB");
    BOOST_CHECK_EQUAL(DiskCacheSettings::format_ram_limit_text(4).toStdString(), "4 GB");
    BOOST_CHECK_EQUAL(DiskCacheSettings::format_disk_limit_text(0).toStdString(), "Unlimited");
    BOOST_CHECK_EQUAL(DiskCacheSettings::format_disk_limit_text(128).toStdString(), "128 GB");
    BOOST_CHECK_EQUAL(DiskCacheSettings::format_disk_limit_text(1024).toStdString(), "1 TB");
}

BOOST_AUTO_TEST_SUITE_END()
