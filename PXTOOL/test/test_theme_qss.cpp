#include <boost/test/unit_test.hpp>

#include <fstream>
#include <regex>
#include <sstream>
#include <string>

namespace {

std::string read_file(const char *path)
{
    std::ifstream input(path);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::string qss_block(const std::string &qss, const char *selector)
{
    const std::string pattern = std::string(selector) + R"(\s*\{([^}]*)\})";
    std::smatch match;
    if (!std::regex_search(qss, match, std::regex(pattern)))
        return std::string();
    return match[1].str();
}

bool has_nonwhite_bottom_border(const std::string &qss, const char *selector)
{
    const std::string block = qss_block(qss, selector);
    if (block.empty())
        return false;

    std::smatch match;
    if (!std::regex_search(block, match,
            std::regex(R"(border-bottom\s*:\s*1px\s+solid\s+(#[0-9A-Fa-f]{6}))")))
        return false;

    const std::string color = match[1].str();
    return color != "#ffffff" && color != "#FFFFFF";
}

bool has_checkbox_svg_url(const std::string &qss)
{
    return std::regex_search(qss,
        std::regex(R"(image\s*:\s*url\([^)]*checkbox[^)]*\.svg\))",
            std::regex_constants::icase));
}

bool checked_checkbox_uses_accent(const std::string &qss)
{
    const std::string block = qss_block(qss, "QCheckBox::indicator:checked");
    return block.find("#7c3aed") != std::string::npos;
}

} // namespace

BOOST_AUTO_TEST_SUITE(theme_qss)

BOOST_AUTO_TEST_CASE(main_title_surfaces_define_theme_bottom_border)
{
    const std::string light = read_file("PXTOOL/themes/light.qss");
    const std::string dark = read_file("PXTOOL/themes/dark.qss");

    BOOST_TEST(has_nonwhite_bottom_border(light, "QMenuBar#main_menu_bar"));
    BOOST_TEST(has_nonwhite_bottom_border(light, "QWidget#main_window_title_bar"));
    BOOST_TEST(has_nonwhite_bottom_border(dark, "QMenuBar#main_menu_bar"));
    BOOST_TEST(has_nonwhite_bottom_border(dark, "QWidget#main_window_title_bar"));
}

BOOST_AUTO_TEST_CASE(checkbox_indicators_do_not_use_svg_assets)
{
    const std::string light = read_file("PXTOOL/themes/light.qss");
    const std::string dark = read_file("PXTOOL/themes/dark.qss");

    BOOST_TEST(!has_checkbox_svg_url(light));
    BOOST_TEST(!has_checkbox_svg_url(dark));
}

BOOST_AUTO_TEST_CASE(checked_checkbox_indicators_use_purple_accent)
{
    const std::string light = read_file("PXTOOL/themes/light.qss");
    const std::string dark = read_file("PXTOOL/themes/dark.qss");

    BOOST_TEST(checked_checkbox_uses_accent(light));
    BOOST_TEST(checked_checkbox_uses_accent(dark));
}

BOOST_AUTO_TEST_SUITE_END()
