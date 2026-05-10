/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
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
#include "../ui/langresource.h"
#include "../appcontrol.h"
#include "../config/appconfig.h"
#include "../ui/fn.h"
#include "../log.h"
 
using namespace std;

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
        int row_bottom = t->get_v_offset() + t->get_totalHeight() / 2;
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
 
    QFont font(painter.font());
    float fSize = AppConfig::Instance().appOptions.fontSize;
    if (fSize > 10)
        fSize = 10;
    font.setPointSizeF(fSize);
    painter.setFont(font);

    for(auto t : traces)
	{
        t->paint_label(painter, w, dragging ? QPoint(-1, -1) : _mouse_point, fore);
	}

    // Draw trigger sidebar toggle button (top-right corner of header)
    {
        const bool tv = _view.is_triggers_visible();
        const int btnSize = 14;
        const int btnX = w - btnSize - 2;
        const int btnY = 2;
        QRect btnRect(btnX, btnY, btnSize, btnSize);

        // Background circle
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, true);
        QColor bgColor = fore;
        bgColor.setAlpha(btnRect.contains(_mouse_point) ? 80 : 40);
        painter.setBrush(bgColor);
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(btnRect, 3, 3);

        // Arrow: "◀" when expanded (click to collapse), "▶" when collapsed (click to expand)
        painter.setPen(QPen(fore, 1.5));
        const int cx = btnRect.center().x();
        const int cy = btnRect.center().y();
        const int as = 4; // arrow half-size
        if (tv) {
            // Left-pointing triangle (collapse)
            QPolygon arrow;
            arrow << QPoint(cx + as, cy - as)
                  << QPoint(cx - as, cy)
                  << QPoint(cx + as, cy + as);
            painter.drawPolyline(arrow);
        } else {
            // Right-pointing triangle (expand)
            QPolygon arrow;
            arrow << QPoint(cx - as, cy - as)
                  << QPoint(cx + as, cy)
                  << QPoint(cx - as, cy + as);
            painter.drawPolyline(arrow);
        }
        painter.restore();
    }

    // Draw bottom divider for traces with height override (visual drag hint)
    painter.setPen(QPen(QColor(120, 120, 120, 160), 1));
    for (auto t : traces) {
        if (!t->enabled() || t->rows_size() == 0)
            continue;
        if (t->get_height_override() <= 0)
            continue;
        int row_bottom = t->get_v_offset() + t->get_totalHeight() / 2;
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

    _mouse_is_down = true;

    std::vector<Trace*> traces;
    _view.get_traces(ALL_VIEW, traces);
    int action;

    const bool instant = _view.session().is_instant();
    if (instant && _view.session().is_running_status()) {
        return;
    }

	if (event->button() & Qt::LeftButton) {
		_mouse_down_point = event->pos();

        // Toggle trigger sidebar button (top-right corner)
        {
            const int btnSize = 14;
            const int btnX = width() - btnSize - 2;
            QRect btnRect(btnX, 2, btnSize, btnSize);
            if (btnRect.contains(event->pos())) {
                _view.set_triggers_visible(!_view.is_triggers_visible());
                update();
                return;
            }
        }

        // Resize handle check — takes priority over reorder in LOGIC mode
        Trace* rt = get_resize_trace(event->pos());
        if (rt) {
            _resize_trace   = rt;
            _resize_start_y = event->pos().y();
            _resize_start_h = (rt->get_height_override() > 0)
                                  ? rt->get_height_override()
                                  : _view.get_signalHeight();
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
        std::vector<Trace*> traces;
        _view.get_traces(ALL_VIEW, traces);
        // Vertical scrolling
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

        for (auto t : traces)
        {
            if (t->mouse_wheel(width(), pos, shift))
                break;
        }

        update();
    }
}

void Header::changeName(QMouseEvent *event)
{
    if (_context_trace != NULL
        && _context_trace->get_type() != SR_CHANNEL_DSO
        && event->button() == Qt::LeftButton) 
    {
        header_resize();
        QFont font = this->font();
        float fsize = AppConfig::Instance().appOptions.fontSize;
        font.setPointSizeF(fsize <= 10 ? fsize: 10);
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
        const QColor new_color = QColorDialog::getColor(_context_trace->get_colour(), this, L_S(STR_PAGE_DLG, S_ID(IDS_DLG_SET_CHANNEL_COLOUR), "Set Channel Colour"));
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
        int delta = event->pos().y() - _resize_start_y;
        int newH  = qMax(_resize_start_h + delta, (int)Trace::HeightOverrideMin);
        _resize_trace->set_height_override(newH);
        _view.signals_changed(nullptr);
        update();
        return;
    }

    // Hover: show resize cursor near row bottom
    if (_drag_traces.empty()) {
        if (get_resize_trace(event->pos()))
            setCursor(Qt::SplitVCursor);
        else
            unsetCursor();
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
	update();
}

void Header::contextMenuEvent(QContextMenuEvent *event)
{
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

    QAction *chosen = menu.exec(event->globalPos());

    if (chosen == resetOne) {
        t->clear_height_override();
        _view.signals_changed(nullptr);
    } else if (chosen == resetAll) {
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
}


} // namespace view
} // namespace pv
