#include "waveformscrollbar.h"
#include "view.h"

#include "../config/appconfig.h"
#include "../theme/thememanager.h"
#include <QPainter>
#include <QMouseEvent>
#include <QTimerEvent>
#include <algorithm>
#include <cmath>

using namespace pv::view;

static QColor themeColor(const QString &token, const QColor &fallback)
{
    QColor color = AppConfig::Instance().GetThemeColor(token);
    return color.isValid() ? color : fallback;
}

const int WaveformScrollBar::MinThumbWidth;

WaveformScrollBar::WaveformScrollBar(View &view, QWidget *parent)
    : QWidget(parent), _view(view)
{
    setMouseTracking(true);
}

void WaveformScrollBar::update_state(int64_t offset, int64_t min_offset,
                                      int64_t max_offset, int64_t total_pixels,
                                      int viewport_pixels)
{
    _offset          = offset;
    _min_offset      = min_offset;
    _max_offset      = max_offset;
    _total_pixels    = std::max(total_pixels, (int64_t)1);
    _viewport_pixels = std::max(viewport_pixels, 1);
    update();
}

// ── Geometry helpers ──────────────────────────────────────────────────────────

QRect WaveformScrollBar::left_arrow_rect() const
{
    return QRect(0, 0, ArrowWidth, height());
}

QRect WaveformScrollBar::right_arrow_rect() const
{
    return QRect(width() - ArrowWidth, 0, ArrowWidth, height());
}

QRect WaveformScrollBar::track_rect() const
{
    return QRect(ArrowWidth, 0, width() - 2 * ArrowWidth, height());
}

QRect WaveformScrollBar::thumb_rect() const
{
    const QRect tr   = track_rect();
    const int   tw   = tr.width();
    if (tw <= 0) return tr;

    const int64_t scroll_range = std::max(_max_offset - _min_offset, (int64_t)1);
    const double  thumb_ratio  = std::min(1.0, (double)_viewport_pixels / (double)_total_pixels);
    int           thumb_w      = std::max((int)(tw * thumb_ratio), MinThumbWidth);
    thumb_w = std::min(thumb_w, tw);

    const int usable = tw - thumb_w;
    const double pos_ratio = (usable > 0)
        ? (double)(_offset - _min_offset) / (double)scroll_range
        : 0.0;

    const int thumb_x = tr.left() + (int)(usable * std::max(0.0, std::min(1.0, pos_ratio)));
    return QRect(thumb_x, tr.top() + 2, thumb_w, tr.height() - 4);
}

// ── Paint ─────────────────────────────────────────────────────────────────────

void WaveformScrollBar::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    AppConfig &app = AppConfig::Instance();
    const bool isDark = app.IsDarkStyle();
    QColor bgColor    = isDark ? QColor(30, 30, 30)   : QColor(220, 220, 220);
    QColor trackColor = isDark ? QColor(50, 50, 50)   : QColor(200, 200, 200);
    QColor thumbColor = (_hover_thumb)
        ? (isDark ? QColor(140, 140, 140) : QColor(100, 100, 100))
        : (isDark ? QColor(90,  90,  90)  : QColor(140, 140, 140));
    QColor arrowFg = isDark ? QColor(160, 160, 160) : QColor(80, 80, 80);
    if (!pv::theme::ThemeManager::isLegacyTheme(app.frameOptions.style)) {
        QColor track = themeColor("@bg-base", isDark ? QColor("#202020") : QColor("#F8F8F8"));
        QColor thumb = themeColor("@bg-overlay", isDark ? QColor("#3A3A3A") : QColor("#D0D0D0"));
        QColor border = themeColor("@border-strong", isDark ? QColor("#37373B") : QColor("#D5D5D5"));
        bgColor = border;
        trackColor = track;
        thumbColor = thumb;
    }
    const QColor arrowBgLeft  = _hover_left  ? (isDark ? QColor(70,70,70) : QColor(180,180,180)) : bgColor;
    const QColor arrowBgRight = _hover_right ? (isDark ? QColor(70,70,70) : QColor(180,180,180)) : bgColor;

    // Background
    p.fillRect(rect(), bgColor);

    // Track
    const QRect tr = track_rect();
    p.fillRect(tr, trackColor);

    // Thumb (only if scrolling is possible)
    if (_max_offset > _min_offset) {
        const QRect thumb = thumb_rect();
        p.setBrush(thumbColor);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(thumb, 3, 3);
    }

    // Left arrow button
    const QRect la = left_arrow_rect();
    p.fillRect(la, arrowBgLeft);
    {
        p.setPen(QPen(arrowFg, 1.5));
        const int cx = la.center().x(), cy = la.center().y(), s = 3;
        QPolygon arrow;
        arrow << QPoint(cx + s, cy - s) << QPoint(cx - s, cy) << QPoint(cx + s, cy + s);
        p.drawPolyline(arrow);
    }

    // Right arrow button
    const QRect ra = right_arrow_rect();
    p.fillRect(ra, arrowBgRight);
    {
        p.setPen(QPen(arrowFg, 1.5));
        const int cx = ra.center().x(), cy = ra.center().y(), s = 3;
        QPolygon arrow;
        arrow << QPoint(cx - s, cy - s) << QPoint(cx + s, cy) << QPoint(cx - s, cy + s);
        p.drawPolyline(arrow);
    }
}

// ── Mouse events ──────────────────────────────────────────────────────────────

void WaveformScrollBar::mousePressEvent(QMouseEvent *e)
{
    if (!(e->button() & Qt::LeftButton)) return;

    const QPoint pt = e->pos();

    if (left_arrow_rect().contains(pt)) {
        apply_step(-1);
        start_step_timer(-1);
        return;
    }
    if (right_arrow_rect().contains(pt)) {
        apply_step(+1);
        start_step_timer(+1);
        return;
    }

    const QRect thumb = thumb_rect();
    if (thumb.contains(pt)) {
        _dragging         = true;
        _drag_start_x     = pt.x();
        _drag_start_offset = _offset;
        grabMouse();
        return;
    }

    // Click in track (page step)
    if (track_rect().contains(pt)) {
        const int page = std::max(_viewport_pixels / 2, 1);
        const int64_t new_off = (pt.x() < thumb.left())
            ? std::max(_offset - (int64_t)page, _min_offset)
            : std::min(_offset + (int64_t)page, _max_offset);
        emit offset_changed(new_off);
    }
}

void WaveformScrollBar::mouseMoveEvent(QMouseEvent *e)
{
    const QPoint pt = e->pos();

    if (_dragging) {
        const int    dx           = pt.x() - _drag_start_x;
        const QRect  tr           = track_rect();
        const int    thumb_w      = thumb_rect().width();
        const int    usable_track = tr.width() - thumb_w;
        if (usable_track > 0) {
            const int64_t scroll_range = std::max(_max_offset - _min_offset, (int64_t)1);
            const int64_t delta = (int64_t)std::round((double)dx * scroll_range / usable_track);
            const int64_t new_off = std::max(_min_offset,
                                    std::min(_max_offset, _drag_start_offset + delta));
            emit offset_changed(new_off);
        }
        return;
    }

    // Hover highlights
    const bool hl_thumb = thumb_rect().contains(pt);
    const bool hl_left  = left_arrow_rect().contains(pt);
    const bool hl_right = right_arrow_rect().contains(pt);
    if (hl_thumb != _hover_thumb || hl_left != _hover_left || hl_right != _hover_right) {
        _hover_thumb = hl_thumb;
        _hover_left  = hl_left;
        _hover_right = hl_right;
        update();
    }
    setCursor(hl_thumb ? Qt::SizeHorCursor : Qt::ArrowCursor);
}

void WaveformScrollBar::mouseReleaseEvent(QMouseEvent *e)
{
    (void)e;
    if (_dragging)
        releaseMouse();
    _dragging = false;
    stop_step_timer();
    unsetCursor();
}

void WaveformScrollBar::leaveEvent(QEvent *e)
{
    (void)e;
    if (_hover_thumb || _hover_left || _hover_right) {
        _hover_thumb = _hover_left = _hover_right = false;
        update();
    }
    unsetCursor();
}

// ── Step timer (button hold) ──────────────────────────────────────────────────

void WaveformScrollBar::start_step_timer(int direction)
{
    stop_step_timer();
    _step_direction = direction;
    _step_timer_id  = startTimer(100);
}

void WaveformScrollBar::stop_step_timer()
{
    if (_step_timer_id) {
        killTimer(_step_timer_id);
        _step_timer_id = 0;
    }
    _step_direction = 0;
}

void WaveformScrollBar::timerEvent(QTimerEvent *e)
{
    if (e->timerId() == _step_timer_id)
        apply_step(_step_direction);
}

void WaveformScrollBar::apply_step(int direction)
{
    const int64_t step = std::max((int64_t)(_viewport_pixels / 10), (int64_t)1);
    const int64_t new_off = std::max(_min_offset,
                            std::min(_max_offset, _offset + direction * step));
    if (new_off != _offset)
        emit offset_changed(new_off);
}
