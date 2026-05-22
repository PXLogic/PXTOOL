/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2013 DreamSourceLab <support@dreamsourcelab.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */


#ifndef DSVIEW_PV_SEARCHDOCK_H
#define DSVIEW_PV_SEARCHDOCK_H

#include <QDockWidget>
#include <QPushButton>
#include <QToolButton>
#include <QLabel>
#include <QRadioButton>
#include <QScrollArea>
#include <QSlider>
#include <QLineEdit>
#include <QSpinBox>
#include <QTableWidget>
#include <QCheckBox>
#include <QRegularExpressionValidator>

#include <QVector>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QGroupBox>

#include <map>
#include <vector>

#include "../widgets/fakelineedit.h"
#include "../widgets/searchedgeflagedit.h"
#include "../ui/dscombobox.h"
#include "../interface/icallbacks.h"
#include "../ui/uimanager.h"

namespace pv {

class SigSession;

namespace view {
    class View;
}

namespace widgets {
    class FakeLineEdit;
    class SearchEdgeFlagEdit;
}

namespace dock {

class SearchDock : public QWidget, public IUiWindow
{
    Q_OBJECT

public:
    SearchDock(QWidget *parent, pv::view::View &view, SigSession *session);
    ~SearchDock();

private:     
    void retranslateUi();
    void reStyle();

    void build_controls(QScrollArea *scroll);
    void build_nav_bar(QWidget *host);
    void build_toggle_row(QWidget *host);
    void build_editor_container(QWidget *host);
    void build_results_panel();
    void update_results_size();
    void rebuild_channel_rows();
    void refresh_pattern_summary();

    void run_full_search();
    void clear_results();
    void navigate_to_result(int idx);
    static QString format_time(int64_t sample, uint64_t samplerate);

    //IUiWindow
    void UpdateLanguage() override;
    void UpdateTheme() override;
    void UpdateFont() override;

public slots:
    void on_previous();
    void on_next();
    void on_search_all();
    void on_toggle_editor(bool force_expand = false);
    void on_channel_edit_finished();
    void on_device_updated();

private:
    SigSession *_session;
    view::View &_view;
    std::map<uint16_t, QString> _pattern;

    // NavBar widgets (top, outside the scroll area logically — sits in the same host
    // but stays at the top because EditorContainer can be collapsed/expanded).
    QPushButton _pre_button;
    QPushButton _nxt_button;
    widgets::FakeLineEdit* _search_value = nullptr;
    QPushButton *_search_button = nullptr;
    QPushButton *_search_all_btn = nullptr;

    // Toggle + editor section
    QToolButton *_toggle_btn = nullptr;
    QWidget     *_editor_container = nullptr;
    QGridLayout *_channel_grid = nullptr;
    QLabel      *_legend_lbl = nullptr;
    bool         _editor_expanded = false;

    // Channel rows; parallel arrays kept in sync by rebuild_channel_rows().
    QVector<widgets::SearchEdgeFlagEdit *> _channel_edits;
    QVector<uint16_t>                      _channel_indices;

    // Shared validator instance for all channel line edits; reused across rebuilds
    // to avoid leaking validators on each device_updated.
    QRegularExpressionValidator *_value_validator = nullptr;

    // Inner scroll area that wraps only the controls (nav + toggle + editor).
    QScrollArea *_controls_scroll = nullptr;
    QVBoxLayout *_outer_layout = nullptr;

    // Results panel — lives OUTSIDE the controls scroll area so its
    // own scrollbar is not intercepted by the outer scroll.
    QWidget      *_results_panel = nullptr;
    QLabel       *_results_title_lbl = nullptr;
    QLabel       *_result_count_lbl = nullptr;
    QTableWidget *_result_table = nullptr;
    QVector<int64_t> _result_positions;
    int              _result_current = -1;
};

} // namespace dock
} // namespace pv

#endif // DSVIEW_PV_SEARCHDOCK_H
