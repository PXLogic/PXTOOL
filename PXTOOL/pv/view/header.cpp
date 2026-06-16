/*
 * This file is part of the PXTOOL project.
 * PXTOOL is based on PulseView.
 *
 * Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
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

#include "header.h" 
  
#include <QColorDialog>
#include <QInputDialog>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QRect>
#include <QStyleOption>
#include <QApplication>
#include <assert.h>
#include <algorithm>
#include <QFont>

#include "view.h"
#include "trace.h"
#include "dsosignal.h"
#include "logicsignal.h"
#include "analogsignal.h"
#include "groupsignal.h"
#include "decodetrace.h"
#include "../sigsession.h"
#include "../dsvdef.h"
#include "../appcontrol.h"
#include "../config/appconfig.h"
#include "../ui/fn.h"
#include "../log.h"
 
using namespace std;

namespace {

QFont header_ui_font(const QFont &base)
{
    QFont font(base);
    const QString appFamily = QApplication::font().family();
    if (!appFamily.isEmpty())
        font.setFamily(appFamily);

    float fSize = AppConfig::Instance().appOptions.fontSize;
    if (fSize > 10)
        fSize = 10;
    font.setPixelSize(qRound(fSize));
    font.setWeight(QFont::Normal);
    font.setBold(false);
    return font;
}

}

namespace pv {
namespace view {

Header::Header(View &parent) :
	QWidget(&parent),
    _view(parent)
{
    _moveFlag = false;
    _colorFlag = false;
    _nameFlag = false;
    _context_trace = NULL;
    _mouse_is_down = false;
    
    nameEdit = new PopupLineEdit(this);
    nameEdit->setFixedWidth(100);
    nameEdit->hide();
    nameEdit->set_instant_mode(true);

	setMouseTracking(true);

    connect(nameEdit, SIGNAL(editingFinished()),
            this, SLOT(on_action_set_name_triggered()));


    ADD_UI(this);
}

Header::~Header()
{
    REMOVE_UI(this);
}

void Header::retranslateUi()
{
    update();
}


int Header::get_nameEditWidth()
{ 
    if (nameEdit->hasFocus())
        return nameEdit->width();
    else
        return 0;
}

pv::view::Trace* Header::get_mTrace(int &action, const QPoint &pt)
{
    const int w = width();
    std::vector<Trace*> traces;
    _view.get_traces(ALL_VIEW, traces);

    for(auto t : traces)
    {
        if ((action = t->pt_in_rect(t->get_y(), w, pt)))
            return t;
    }

    return NULL;
}

Trace* Header::get_resize_trace(const QPoint &pt)
{
    if (_view.session().get_device()->get_work_mode() != LOGIC)
        return nullptr;
    if (_view.session().is_working())
        return nullptr;

    std::vector<Trace*> traces;
    _view.get_traces(ALL_VIEW, traces);

    for (auto t : traces) {
        if (!t->enabled() || t->rows_size() == 0)
            continue;
        int row_bottom = t->get_v_offset() + t->get_totalHeight() / 2 + View::SignalMargin;
        if (pt.y() >= row_bottom - 6 && pt.y() <= row_bottom + 1)
            return t;
    }
    return nullptr;
}

void Header::paintEvent(QPaintEvent*)
{ 
    using pv::view::Trace;

    QStyleOption o;
    o.initFrom(this);
    QPainter painter(this);
    style()->drawPrimitive(QStyle::PE_Widget, &o, &painter, this);

	const int w = width();
    std::vector<Trace*> traces;
    _view.get_traces(ALL_VIEW, traces);

    const bool dragging = !_drag_traces.empty();
    QColor fore(QWidget::palette().color(QWidget::foregroundRole()));
    fore.setAlpha(View::ForeAlpha);
 
    QFont font = header_ui_font(painter.font());
    painter.setFont(font);

    for(auto t : traces)
	{
        t->paint_label(painter, w, dragging ? QPoint(-1, -1) : _mouse_point, fore);
	}

    // Draw channel divider lines — always visible, highlighted when hovered (synced with viewport)
    Trace* active_divider = _view.get_hovered_divider();
    for (auto t : traces) {
        if (!t->enabled() || t->rows_size() == 0)
            continue;
        int row_bottom = t->get_v_offset() + t->get_totalHeight() / 2 + View::SignalMargin;
        bool isActive = (t == active_divider);
        if (isActive) {
            painter.setPen(QPen(QColor(180, 180, 180, 220), 2));
        } else {
            painter.setPen(QPen(QColor(100, 100, 100, 100), 1));
        }
        painter.drawLine(0, row_bottom, w, row_bottom);
    }

	painter.end();
}

void Header::mouseDoubleClickEvent(QMouseEvent *event)
{
    assert(event);

    std::vector<Trace*> traces;

    _view.get_traces(ALL_VIEW, traces);

    if (event->button() & Qt::LeftButton) {
        _mouse_down_point = event->pos();

        // Save the offsets of any Traces which will be dragged
        for(auto t : traces){
            if (t->selected())
                _drag_traces.push_back(
                    make_pair(t, t->get_v_offset()));
        }

        // Select the Trace if it has been clicked
        for(auto t : traces){
            if (t->mouse_double_click(width(), event->pos()))
                break;
        }
    }

}

void Header::mousePressEvent(QMouseEvent *event)
{
	assert(event);

    // Only the left button starts a header drag. Right-button presses pop up
    // a modal context menu that swallows the matching release event, which
    // would otherwise leave _mouse_is_down stuck at true and block wheel
    // zoom on the viewport via header_is_draging().
    if (event->button() & Qt::LeftButton) {
        _mouse_is_down = true;
    }

    std::vector<Trace*> traces;
    _view.get_traces(ALL_VIEW, traces);
    int action;

    const bool instant = _view.session().is_instant();
    if (instant && _view.session().is_running_status()) {
        return;
    }

	if (event->button() & Qt::LeftButton) {
		_mouse_down_point = event->pos();

        // Resize handle check — takes priority over reorder in LOGIC mode
        Trace* rt = get_resize_trace(event->pos());
        if (rt) {
            _resize_trace   = rt;
            _resize_start_y = event->pos().y();
            _resize_start_total = rt->get_totalHeight();
            setCursor(Qt::SplitVCursor);
            return;
        }

        // Save the offsets of any Traces which will be dragged
        for(auto t : traces){
            if (t->selected())
                _drag_traces.push_back(
                    make_pair(t, t->get_v_offset()));
        }

        // Select the Trace if it has been clicked
        const auto mTrace = get_mTrace(action, event->pos());
        if (action == Trace::COLOR && mTrace) {
            _colorFlag = true;
        }
        else if (action == Trace::NAME && mTrace) {
            _nameFlag = true;
        }
        else if (action == Trace::LABEL && mTrace) {
            mTrace->select(true);

            if (~QApplication::keyboardModifiers() & Qt::ControlModifier)
                _drag_traces.clear();
            
            _drag_traces.push_back(make_pair(mTrace, mTrace->get_zero_vpos()));
            mTrace->set_old_v_offset(mTrace->get_v_offset());
        }

        for(auto t : traces){
            if (t->signal_type() == SR_CHANNEL_LOGIC && _view.session().is_working()){
                // Disable set trigger from left pannel when capturing.
                break;
            }
            if (t->mouse_press(width(), event->pos()))
                break;
        }

        if (~QApplication::keyboardModifiers() & Qt::ControlModifier) {
            // Unselect all other Traces because the Ctrl is not
            // pressed
            for(auto t : traces){
                if (t != mTrace)
                    t->select(false);
            }
        }
        update();
    }
}

void Header::mouseReleaseEvent(QMouseEvent *event)
{
	assert(event);

    _mouse_is_down = false;

    if (_resize_trace) {
        _resize_trace = nullptr;
        unsetCursor();
        _view.signals_changed(nullptr);
        return;
    }

    // judge for color / name / trigger / move
    int action;
    const auto mTrace = get_mTrace(action, event->pos());

    if (mTrace){
        if (action == Trace::COLOR && _colorFlag) {
            _context_trace = mTrace;
            changeColor(event);
            _view.set_all_update(true);
        }
        else if (action == Trace::NAME && _nameFlag) {
            _context_trace = mTrace;
            changeName(event);
        }
    }

    // Make view index by Y value;
    int mode = _view.session().get_device()->get_work_mode();    
    if (_moveFlag && mode == LOGIC)
    {
        std::vector<Trace*> traces;

        for (auto s : _view.session().get_decode_signals()){
            traces.push_back(s);
        }

        for (auto s : _view.session().get_signals()){
            traces.push_back(s);
        }
    
        sort(traces.begin(), traces.end(), View::compare_trace_y);

        int index = 0;
        for (auto t : traces){
            t->set_view_index(index++);
        }
    }

    if (_moveFlag) {
        _drag_traces.clear();
        _view.signals_changed(mTrace);
        _view.set_all_update(true);

        std::vector<Trace*> traces;
        _view.get_traces(ALL_VIEW, traces);

        for(auto t : traces){
            t->select(false);
        }            
    }

    _colorFlag = false;
    _nameFlag = false;
    _moveFlag = false;

    _view.normalize_layout();
}

void Header::wheelEvent(QWheelEvent *event)
{
    assert(event);

    int x = 0;
    int y = 0;
    int delta = 0;
    bool isVertical = true;
    QPoint pos;
    (void)x;
    (void)y;
     
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    x = (int)event->position().x();
    y = (int)event->position().y();    
    int anglex = event->angleDelta().x();
    int angley = event->angleDelta().y();

    pos.setX(x);
    pos.setY(y);

    if (anglex == 0 || ABS_VAL(angley) >= ABS_VAL(anglex)){
        delta = angley;
        isVertical = true;
    }
    else{
        delta = anglex;
        isVertical = false; //hori direction
    }
#else
    x = event->x();
    delta = event->delta();
    isVertical = event->orientation() == Qt::Vertical;
    pos = event->pos(); 
#endif

    if (isVertical)
    {
        // Vertical scrolling — drive the view's global scroll offset
        double shift = 0;

#ifdef Q_OS_DARWIN
        static bool active = true;
        static int64_t last_time;
        if (event->source() == Qt::MouseEventSynthesizedBySystem)
        {
            if (active)
            {
                last_time = QDateTime::currentMSecsSinceEpoch();
                shift = delta > 1.5 ? -1 : delta < -1.5 ? 1 : 0;
            }
            int64_t cur_time = QDateTime::currentMSecsSinceEpoch();
            if (cur_time - last_time > 100)
                active = true;
            else
                active = false;
        }
        else
        {
            shift = -delta / 80.0;
        }
#else
        shift = delta / 80.0;
#endif

        const int step      = (int)(shift * _view.get_signalHeight());
        const int new_off   = _view.v_scroll_offset() + step;
        _view.on_v_scroll_changed(new_off);
    }
}

void Header::changeName(QMouseEvent *event)
{
    if (_context_trace != NULL
        && _context_trace->get_type() != SR_CHANNEL_DSO
        && event->button() == Qt::LeftButton) 
    {
        header_resize();
        QFont font = header_ui_font(this->font());
        nameEdit->setFont(font);

        nameEdit->setText(_context_trace->get_name());
        nameEdit->selectAll();
        nameEdit->setFocus();
        nameEdit->show();
        header_updated();
    }
}

void Header::changeColor(QMouseEvent *event)
{
     if (_view.session().is_working() && _view.session().get_device()->get_work_mode() == ANALOG){
        //Disable to select color when working on analog mode.
        return;
    }

    if ((event->button() == Qt::LeftButton)) {
        const QColor new_color = QColorDialog::getColor(_context_trace->get_colour(), this, tr("Set Channel Colour"));
        if (new_color.isValid())
            _context_trace->set_colour(new_color);
    }
}

void Header::mouseMoveEvent(QMouseEvent *event)
{
	assert(event);

    if (_view.session().is_working() && _view.session().get_device()->get_work_mode() == LOGIC){
        //Disable the hover status of trig button on left pannel.
        return;
    }

	_mouse_point = event->pos();

    // Active resize drag
    if (_resize_trace) {
        const int rows = qMax(1, _resize_trace->rows_size());
        const int delta = event->pos().y() - _resize_start_y;
        const int min_total = rows * (int)Trace::HeightOverrideMin;
        const int new_total = qMax(min_total, _resize_start_total + delta);
        const int new_per_row = qMax((int)Trace::HeightOverrideMin, new_total / rows);
        _resize_trace->set_height_override(new_per_row);
        _view.signals_changed(nullptr);
        update();
        return;
    }

    // Hover: show resize cursor near row bottom, sync highlight with viewport
    if (_drag_traces.empty()) {
        Trace *ht = get_resize_trace(event->pos());
        if (ht) {
            setCursor(Qt::SplitVCursor);
        } else {
            unsetCursor();
        }
        _view.set_hovered_divider(ht);
    }

    // Move the Traces if we are dragging
    if (!_drag_traces.empty()) {
		const int delta = event->pos().y() - _mouse_down_point.y();

        for (auto i = _drag_traces.begin(); i != _drag_traces.end(); i++) {
            const auto t = (*i).first;
			if (t) {
                int y = (*i).second + delta;
                if (t->get_type() == SR_CHANNEL_DSO) {
                    DsoSignal *dsoSig = NULL;
                    if ((dsoSig = dynamic_cast<DsoSignal*>(t))) {
                        dsoSig->set_zero_vpos(y);
                        _moveFlag = true;
                        traces_moved();
                    }
                } else if (t->get_type() == SR_CHANNEL_MATH) {
                    MathTrace *mathTrace = NULL;
                    if ((mathTrace = dynamic_cast<MathTrace*>(t))) {
                       mathTrace->set_zero_vpos(y);
                       _moveFlag = true;
                       traces_moved();
                    }
                 } else if (t->get_type() == SR_CHANNEL_ANALOG) {
                    AnalogSignal *analogSig = NULL;
                    if ((analogSig = dynamic_cast<AnalogSignal*>(t))) {
                        analogSig->set_zero_vpos(y);
                        _moveFlag = true;
                        traces_moved();
                    }
                } else {
                    if (~QApplication::keyboardModifiers() & Qt::ControlModifier) {
                        const int y_snap =
                            ((y + View::SignalSnapGridSize / 2) /
                                View::SignalSnapGridSize) *
                                View::SignalSnapGridSize;
                        if (y_snap != t->get_v_offset()) {
                            _moveFlag = true;
                            t->set_v_offset(y_snap);
                        }
                    }
                }
			}
		}
	}
	update();
}

void Header::leaveEvent(QEvent*)
{
	_mouse_point = QPoint(-1, -1);
    _view.set_hovered_divider(nullptr);
	update();
}

void Header::contextMenuEvent(QContextMenuEvent *event)
{
    // The modal QMenu::exec() below consumes the right-button release event,
    // so make sure the header is not left in a "dragging" state which would
    // disable wheel zoom on the viewport.
    _mouse_is_down = false;

    int action;
    const auto t = get_mTrace(action, _mouse_point);

    if (!t)
        return;

    if (_view.session().get_device()->get_work_mode() != LOGIC)
        return;

    if (_view.session().is_working())
        return;

    QMenu menu(this);
    QAction *resetOne = menu.addAction(tr("Reset Row Height"));
    QAction *resetAll = menu.addAction(tr("Reset All Row Heights"));

    menu.addSeparator();
    QMenu *heightMenu = menu.addMenu(tr("Set Channel Height"));
    QAction *h_auto = heightMenu->addAction(tr("Auto (fit to view)"));
    heightMenu->addSeparator();
    QAction *h1x = heightMenu->addAction(tr("1X  (30px)"));
    QAction *h2x = heightMenu->addAction(tr("2X  (60px)"));
    QAction *h4x = heightMenu->addAction(tr("4X (120px)"));
    QAction *h8x = heightMenu->addAction(tr("8X (240px)"));

    // mark the active preset
    const int curMul = AppConfig::Instance().appOptions.signalHeightMultiple;
    h_auto->setCheckable(true); h_auto->setChecked(curMul == 0);
    h1x->setCheckable(true);    h1x->setChecked(curMul == 1);
    h2x->setCheckable(true);    h2x->setChecked(curMul == 2);
    h4x->setCheckable(true);    h4x->setChecked(curMul == 4);
    h8x->setCheckable(true);    h8x->setChecked(curMul == 8);

    ui::apply_application_menu_font(&menu);

    /* "Decode Engine" submenu intentionally removed: the C / Python choice
     * is now made up-front when the user picks the protocol from the
     * protocol-list (entries appear as e.g. "SPI(C)" and "SPI(Py)").
     * Per-trace switching here was confusing because the engine choice is
     * really a property of the trace itself, not a runtime toggle. */

    QAction *chosen = menu.exec(event->globalPos());

    if (chosen == h_auto || chosen == h1x || chosen == h2x
            || chosen == h4x || chosen == h8x) {
        int mul = 0;
        if      (chosen == h1x) mul = 1;
        else if (chosen == h2x) mul = 2;
        else if (chosen == h4x) mul = 4;
        else if (chosen == h8x) mul = 8;
        AppConfig::Instance().appOptions.signalHeightMultiple = mul;
        AppConfig::Instance().SaveApp();
        _view.reset_height_overrides();
        _view.signals_changed(nullptr);
    } else if (chosen == resetOne) {
        // Reset to 1X if in Auto mode so height is predictable
        if (AppConfig::Instance().appOptions.signalHeightMultiple == 0) {
            AppConfig::Instance().appOptions.signalHeightMultiple = 1;
            AppConfig::Instance().SaveApp();
        }
        t->clear_height_override();
        _view.signals_changed(nullptr);
    } else if (chosen == resetAll) {
        // Reset to 1X if in Auto mode so height is predictable
        if (AppConfig::Instance().appOptions.signalHeightMultiple == 0) {
            AppConfig::Instance().appOptions.signalHeightMultiple = 1;
            AppConfig::Instance().SaveApp();
        }
        _view.reset_height_overrides();
    }
}

void Header::on_action_set_name_triggered()
{
    auto context_Trace = _context_trace;
    if (!context_Trace)
		return;

    if (nameEdit->isModified()) {
        QString v = nameEdit->text().trimmed();
        if (v == "")
            v = QString::number(context_Trace->get_index());
        
        _view.session().set_trace_name(context_Trace, v);
    }

    nameEdit->hide();
    header_updated();
}

void Header::header_resize()
{
    if (_context_trace) {
        const int y = _context_trace->get_y();
        nameEdit->move(QPoint(_context_trace->get_leftWidth(), y - nameEdit->height() / 2));
    }
}

void Header::UpdateLanguage()
{
    retranslateUi();
}

void Header::UpdateTheme()
{
    retranslateUi();
}

void Header::UpdateFont()
{
    QFont font = header_ui_font(this->font());
    setFont(font);
    if (nameEdit)
        nameEdit->setFont(font);
}


} // namespace view
} // namespace pv
