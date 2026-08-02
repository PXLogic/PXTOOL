#include <boost/test/unit_test.hpp>

#include <fstream>
#include <regex>
#include <sstream>
#include <string>

namespace {

std::string read_file(const char *path)
{
#ifndef DSVIEW_SOURCE_DIR
#define DSVIEW_SOURCE_DIR "."
#endif
    const std::string full_path = std::string(DSVIEW_SOURCE_DIR) + "/" + path;
    std::ifstream input(full_path);
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

std::string qss_text_color(const std::string &qss, const char *selector)
{
    const std::string block = qss_block(qss, selector);
    if (block.empty())
        return std::string();

    std::smatch match;
    if (!std::regex_search(block, match,
            std::regex(R"((^|[;\s])color\s*:\s*(#[0-9A-Fa-f]{6}))")))
        return std::string();

    return match[2].str();
}

bool has_nonwhite_text_color(const std::string &qss, const char *selector)
{
    const std::string color = qss_text_color(qss, selector);
    return !color.empty() && color != "#ffffff" && color != "#FFFFFF";
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

bool checked_checkbox_uses_checkmark(const std::string &qss)
{
    const std::string block = qss_block(qss, "QCheckBox::indicator:checked");
    return block.find(":/icons/sidebar/checkbox-check.png") != std::string::npos;
}

bool measure_cursor_buttons_have_light_empty_state(const std::string &source)
{
    return source.find("#ffffff") != std::string::npos &&
        source.find("#d0d5dd") != std::string::npos &&
        source.find("#f5f0ff") != std::string::npos &&
        source.find("#7c3aed") != std::string::npos;
}

bool measure_restyle_refreshes_all_cursor_button_rows(const std::string &source)
{
    const std::string start_marker = "void MeasureDock::reStyle()";
    const std::string end_marker = "void MeasureDock::reload()";
    const std::string::size_type start = source.find(start_marker);
    const std::string::size_type end = source.find(end_marker);
    if (start == std::string::npos || end == std::string::npos || end <= start)
        return false;

    const std::string block = source.substr(start, end - start);
    return block.find("update_dist();") != std::string::npos &&
        block.find("update_edge();") != std::string::npos &&
        block.find("update_cursor_info();") != std::string::npos;
}

bool has_dsl_export_range_popup_device_sizing(const std::string &source)
{
    const std::string marker = "void StoreProgress::save_run(";
    const std::string::size_type start = source.find(marker);
    if (start == std::string::npos)
        return false;

    const std::string block = source.substr(start);
    return block.find("setPopupFitContents(false)") != std::string::npos &&
        block.find("view->setMinimumWidth") != std::string::npos &&
        block.find("addRow(tr(\"Start*\")") != std::string::npos;
}

bool dialog_button_box_marks_primary_and_secondary_actions(const std::string &source)
{
    return source.find("setObjectName(\"dialog_primary_button\")") != std::string::npos &&
        source.find("setObjectName(\"dialog_secondary_button\")") != std::string::npos;
}

bool dialog_footer_uses_purple_primary_action(const std::string &qss)
{
    const std::string primary = qss_block(qss, "QPushButton#dialog_primary_button");
    const std::string footer = qss_block(qss, "QWidget#dialog_footer_divider");
    return primary.find("#7c3aed") != std::string::npos &&
        footer.find("min-height: 1px") != std::string::npos;
}

bool legacy_dialogs_have_full_width_footer_dividers(const std::string &source)
{
    static const char *const dialogs[] = {
        "fftoptions.cpp", "lissajousoptions.cpp", "mathoptions.cpp",
        "interval.cpp", "regionoptions.cpp", "dsomeasure.cpp", "waitingdialog.cpp"
    };

    for (const char *dialog : dialogs) {
        if (read_file((std::string("PXTOOL/pv/dialogs/") + dialog).c_str())
                .find("dialog_footer_divider") == std::string::npos)
            return false;
    }

    return source.find("QWidget#dialog_footer_divider") != std::string::npos;
}

bool waiting_dialog_marks_late_save_action_as_primary()
{
    const std::string source = read_file("PXTOOL/pv/dialogs/waitingdialog.cpp");
    return source.find("_button_box.addButton(QDialogButtonBox::Save)") != std::string::npos &&
        source.find("save_button->setObjectName(\"dialog_primary_button\")") != std::string::npos;
}

bool has_compact_collapsed_search_editor_icon(const std::string &source)
{
    const std::string marker = "void SearchDock::on_toggle_editor(";
    const std::string::size_type start = source.find(marker);
    if (start == std::string::npos)
        return false;

    const std::string block = source.substr(start);
    return block.find("search-editor-collapsed.svg") != std::string::npos;
}

bool popup_items_match_device_dropdown(const std::string &qss, const char *dialog)
{
    const std::string selector = std::string(dialog) + " QComboBox QAbstractItemView::item";
    const std::string item = qss_block(qss, selector.c_str());
    const std::string selected = qss_block(qss, (selector + ":selected").c_str());
    const std::string hover = qss_block(qss, (selector + ":hover").c_str());
    return item.find("min-height: 20px") != std::string::npos &&
        item.find("padding: 4px 12px") != std::string::npos &&
        item.find("border: 1px solid transparent") != std::string::npos &&
        selected.find("background-color: #7c3aed") != std::string::npos &&
        selected.find("color: #ffffff") != std::string::npos &&
        hover.find("background-color: #7c3aed") != std::string::npos &&
        hover.find("color: #ffffff") != std::string::npos;
}

bool protocol_dialog_uses_primary_push_button(const char *path)
{
    const std::string source = read_file(path);
    return source.find("new QPushButton(tr(\"OK\"), this)") != std::string::npos &&
        source.find("ok_btn->setObjectName(\"device_ok_btn\")") != std::string::npos &&
        source.find("QDialogButtonBox") == std::string::npos;
}

bool popup_line_edit_inherits_source_object_name()
{
    const std::string source = read_file("PXTOOL/pv/dock/keywordlineedit.cpp");
    return source.find("setObjectName(editline->objectName() + \"_popup\")") !=
        std::string::npos;
}

bool has_centered_protocol_search_popup_cursor(const std::string &qss)
{
    const std::string block = qss_block(qss,
        "QDialog#decode_ann_search_edit_popup QLineEdit");
    return block.find("border: none") != std::string::npos &&
        block.find("padding: 0px 4px 2px 4px") != std::string::npos;
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

BOOST_AUTO_TEST_CASE(light_device_bar_labels_use_readable_text_color)
{
    const std::string light = read_file("PXTOOL/themes/light.qss");

    BOOST_TEST(has_nonwhite_text_color(light, "QToolBar#device_bar QLabel#device_bar_label"));
    BOOST_TEST(qss_text_color(light, "QToolBar#device_bar QLabel#device_bar_label") == "#6b7280");
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

BOOST_AUTO_TEST_CASE(checked_checkbox_indicators_show_checkmark)
{
    const std::string light = read_file("PXTOOL/themes/light.qss");
    const std::string dark = read_file("PXTOOL/themes/dark.qss");

    BOOST_TEST(checked_checkbox_uses_checkmark(light));
    BOOST_TEST(checked_checkbox_uses_checkmark(dark));
}

BOOST_AUTO_TEST_CASE(measure_cursor_buttons_define_light_empty_state)
{
    const std::string source = read_file("PXTOOL/pv/dock/measuredock.cpp");

    BOOST_TEST(measure_cursor_buttons_have_light_empty_state(source));
}

BOOST_AUTO_TEST_CASE(measure_restyle_refreshes_cursor_button_rows)
{
    const std::string source = read_file("PXTOOL/pv/dock/measuredock.cpp");

    BOOST_TEST(measure_restyle_refreshes_all_cursor_button_rows(source));
}

BOOST_AUTO_TEST_CASE(dsl_export_range_popup_matches_device_popup_sizing)
{
    const std::string source = read_file("PXTOOL/pv/dialogs/storeprogress.cpp");

    BOOST_TEST(has_dsl_export_range_popup_device_sizing(source));
}

BOOST_AUTO_TEST_CASE(dialog_button_boxes_use_standard_primary_and_secondary_actions)
{
    const std::string source = read_file("PXTOOL/pv/dialogs/dsdialog.cpp");

    BOOST_TEST(dialog_button_box_marks_primary_and_secondary_actions(source));
}

BOOST_AUTO_TEST_CASE(dialog_button_box_footers_match_export_data_actions)
{
    const std::string light = read_file("PXTOOL/themes/light.qss");
    const std::string dark = read_file("PXTOOL/themes/dark.qss");

    BOOST_TEST(dialog_footer_uses_purple_primary_action(light));
    BOOST_TEST(dialog_footer_uses_purple_primary_action(dark));
}

BOOST_AUTO_TEST_CASE(legacy_dialogs_define_full_width_footer_dividers)
{
    const std::string light = read_file("PXTOOL/themes/light.qss");
    const std::string dark = read_file("PXTOOL/themes/dark.qss");

    BOOST_TEST(legacy_dialogs_have_full_width_footer_dividers(light));
    BOOST_TEST(legacy_dialogs_have_full_width_footer_dividers(dark));
}

BOOST_AUTO_TEST_CASE(waiting_dialog_save_action_uses_the_primary_style)
{
    BOOST_TEST(waiting_dialog_marks_late_save_action_as_primary());
}

BOOST_AUTO_TEST_CASE(search_editor_uses_a_compact_collapsed_icon)
{
    const std::string source = read_file("PXTOOL/pv/dock/searchdock.cpp");

    BOOST_TEST(has_compact_collapsed_search_editor_icon(source));
}

BOOST_AUTO_TEST_CASE(protocol_list_popup_hover_matches_device_dropdown)
{
    const std::string light = read_file("PXTOOL/themes/light.qss");
    const std::string dark = read_file("PXTOOL/themes/dark.qss");

    BOOST_TEST(popup_items_match_device_dropdown(light, "QDialog#protocolListDialog"));
    BOOST_TEST(popup_items_match_device_dropdown(dark, "QDialog#protocolListDialog"));
}

BOOST_AUTO_TEST_CASE(protocol_export_popup_hover_matches_device_dropdown)
{
    const std::string light = read_file("PXTOOL/themes/light.qss");
    const std::string dark = read_file("PXTOOL/themes/dark.qss");

    BOOST_TEST(popup_items_match_device_dropdown(light, "QDialog#protocolExportDialog"));
    BOOST_TEST(popup_items_match_device_dropdown(dark, "QDialog#protocolExportDialog"));
}

BOOST_AUTO_TEST_CASE(protocol_dialog_ok_buttons_use_primary_style)
{
    BOOST_TEST(protocol_dialog_uses_primary_push_button(
        "PXTOOL/pv/dialogs/protocollist.cpp"));
    BOOST_TEST(protocol_dialog_uses_primary_push_button(
        "PXTOOL/pv/dialogs/protocolexp.cpp"));
}

BOOST_AUTO_TEST_CASE(protocol_search_popup_centers_its_edit_cursor)
{
    const std::string light = read_file("PXTOOL/themes/light.qss");
    const std::string dark = read_file("PXTOOL/themes/dark.qss");

    BOOST_TEST(popup_line_edit_inherits_source_object_name());
    BOOST_TEST(has_centered_protocol_search_popup_cursor(light));
    BOOST_TEST(has_centered_protocol_search_popup_cursor(dark));
}

BOOST_AUTO_TEST_SUITE_END()
