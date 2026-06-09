#include "verticalscrollbar.h"
#include "view.h"

#include "../config/appconfig.h"
#include <QPainter>
#include <QMouseEvent>
#include <QTimerEvent>
#include <algorithm>
#include <cmath>

using namespace pv::view;

// out-of-class definition required by pre-C++17 when passed by const-ref (std::max)
const int VerticalScrollBar::MinThumbHeight;

VerticalScrollBar::VerticalScrollBar(View &view, QWidget *parent)
    : QWidget(parent), _view(view)
{
    setMouseTracking(true);
}

void VerticalScrollBar::update_state(int offset, int max_offset,
                                     int total_pixels, int viewport_px)
{
    _offset       = offset;
    _max_offset   = std::max(max_offset, 0);
    _total_pixels = std::max(total_pixels, 1);
    _viewport_px  = std::max(viewport_px, 1);
    update();
}

// ── Geometry helpers ──────────────────────────────────────────────────────────

QRect VerticalScrollBar::up_arrow_rect() const
{
    return QRect(0, 0, width(), ArrowHeight);
}

QRect VerticalScrollBar::down_arrow_rect() const
{
    return QRect(0, height() - ArrowHeight, width(), ArrowHeight);
}

QRect VerticalScrollBar::track_rect() const
{
    return QRect(0, ArrowHeight, width(), height() - 2 * ArrowHeight);
}

QRect VerticalScrollBar::thumb_rect() const
{
    const QRect  tr  = track_rect();
    const int    th  = tr.height();
    if (th <= 0) return tr;

    const double thumb_ratio = std::min(1.0, (double)_viewport_px / (double)_total_pixels);
    int thumb_h = std::max((int)(th * thumb_ratio), MinThumbHeight);
    thumb_h = std::min(thumb_h, th);

    const int    usable    = th - thumb_h;
    const double pos_ratio = (_max_offset > 0)
        ? std::max(0.0, std::min(1.0, (double)_offset / (double)_max_offset))
        : 0.0;

    const int thumb_y = tr.top() + (int)(usable * pos_ratio);
    return QRect(tr.left() + 2, thumb_y, tr.width() - 4, thumb_h);
}

// ── Paint ─────────────────────────────────────────────────────────────────────

void VerticalScrollBar::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    const bool   isDark     = (AppConfig::Instance().frameOptions.style == "dark");
    const QColor bgColor    = isDark ? QColor(30, 30, 30)   : QColor(220, 220, 220);
    const QColor trackColor = isDark ? QColor(50, 50, 50)   : QColor(200, 200, 200);
    const QColor thumbColor = _hover_thumb
        ? (isDark ? QColor(140, 140, 140) : QColor(100, 100, 100))
        : (isDark ? QColor(90,  90,  90)  : QColor(140, 140, 140));
    const QColor arrowBgUp   = _hover_up   ? (isDark ? QColor(70,70,70) : QColor(180,180,180)) : bgColor;
    const QColor arrowBgDown = _hover_down ? (isDark ? QColor(70,70,70) : QColor(180,180,180)) : bgColor;
    const QColor arrowFg     = isDark ? QColor(160, 160, 160) : QColor(80, 80, 80);

    p.fillRect(rect(), bgColor);

    // Track
    p.fillRect(track_rect(), trackColor);

    // Thumb
    if (_max_offset > 0) {
        p.setBrush(thumbColor);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(thumb_rect(), 3, 3);
    }

    // Up arrow
    const QRect ua = up_arrow_rect();
    p.fillRect(ua, arrowBgUp);
    {
        p.setPen(QPen(arrowFg, 1.5));
        const int cx = ua.center().x(), cy = ua.center().y(), s = 3;
        QPolygon arrow;
        arrow << QPoint(cx - s, cy + s) << QPoint(cx, cy - s) << QPoint(cx + s, cy + s);
        p.drawPolyline(arrow);
    }

    // Down arrow
    const QRect da = down_arrow_rect();
    p.fillRect(da, arrowBgDown);
    {
        p.setPen(QPen(arrowFg, 1.5));
        const int cx = da.center().x(), cy = da.center().y(), s = 3;
        QPolygon arrow;
        arrow << QPoint(cx - s, cy - s) << QPoint(cx, cy + s) << QPoint(cx + s, cy - s);
        p.drawPolyline(arrow);
    }
}

// ── Mouse events ──────────────────────────────────────────────────────────────

void VerticalScrollBar::mousePressEvent(QMouseEvent *e)
{
    if (!(e->button() & Qt::LeftButton)) return;
    const QPoint pt = e->pos();

    if (up_arrow_rect().contains(pt)) {
        apply_step(-1);
        start_step_timer(-1);
        return;
    }
    if (down_arrow_rect().contains(pt)) {
        apply_step(+1);
        start_step_timer(+1);
        return;
    }

    const QRect thumb = thumb_rect();
    if (thumb.contains(pt)) {
        _dragging        = true;
        _drag_start_y    = pt.y();
        _drag_start_off  = _offset;
        grabMouse();
        return;
    }

    // Page step on track click
    if (track_rect().contains(pt)) {
        const int page = std::max(_viewport_px / 2, 1);
        const int new_off = (pt.y() < thumb.top())
            ? std::max(_offset - page, 0)
            : std::min(_offset + page, _max_offset);
        emit v_offset_changed(new_off);
    }
}

void VerticalScrollBar::mouseMoveEvent(QMouseEvent *e)
{
    const QPoint pt = e->pos();

    if (_dragging) {
        const int    dy        = pt.y() - _drag_start_y;
        const QRect  tr        = track_rect();
        const int    thumb_h   = thumb_rect().height();
        const int    usable    = tr.height() - thumb_h;
        if (usable > 0) {
            const int delta   = (int)std::round((double)dy * _max_offset / usable);
            const int new_off = std::max(0, std::min(_max_offset, _drag_start_off + delta));
            emit v_offset_changed(new_off);
        }
        return;
    }

    const bool hl_thumb = thumb_rect().contains(pt);
    const bool hl_up    = up_arrow_rect().contains(pt);
    const bool hl_down  = down_arrow_rect().contains(pt);
    if (hl_thumb != _hover_thumb || hl_up != _hover_up || hl_down != _hover_down) {
        _hover_thumb = hl_thumb;
        _hover_up    = hl_up;
        _hover_down  = hl_down;
        update();
    }
    setCursor(hl_thumb ? Qt::SizeVerCursor : Qt::ArrowCursor);
}

void VerticalScrollBar::mouseReleaseEvent(QMouseEvent *e)
{
    (void)e;
    if (_dragging)
        releaseMouse();
    _dragging = false;
    stop_step_timer();
    unsetCursor();
}

void VerticalScrollBar::leaveEvent(QEvent *e)
{
    (void)e;
    if (_hover_thumb || _hover_up || _hover_down) {
        _hover_thumb = _hover_up = _hover_down = false;
        update();
    }
    unsetCursor();
}

// ── Step timer ────────────────────────────────────────────────────────────────

void VerticalScrollBar::start_step_timer(int direction)
{
    stop_step_timer();
    _step_direction = direction;
    _step_timer_id  = startTimer(100);
}

void VerticalScrollBar::stop_step_timer()
{
    if (_step_timer_id) {
        killTimer(_step_timer_id);
        _step_timer_id = 0;
    }
    _step_direction = 0;
}

void VerticalScrollBar::timerEvent(QTimerEvent *e)
{
    if (e->timerId() == _step_timer_id)
        apply_step(_step_direction);
}

void VerticalScrollBar::apply_step(int direction)
{
    const int step    = std::max(_viewport_px / 10, 1);
    const int new_off = std::max(0, std::min(_max_offset, _offset + direction * step));
    if (new_off != _offset)
        emit v_offset_changed(new_off);
}
