// PXTOOL/pv/view/verticalscrollbar.h
#ifndef DSVIEW_PV_VIEW_VERTICALSCROLLBAR_H
#define DSVIEW_PV_VIEW_VERTICALSCROLLBAR_H

#include <QWidget>
#include <stdint.h>

namespace pv {
namespace view {

class View;

class VerticalScrollBar : public QWidget
{
    Q_OBJECT
public:
    static const int ArrowHeight   = 16;
    static const int MinThumbHeight = 20;
    static const int Width          = 14;

    explicit VerticalScrollBar(View &view, QWidget *parent);

    // Called by View::update_scroll() to push current state.
    // offset        – current scroll position in pixels
    // max_offset    – maximum scroll position (content_height - viewport_height)
    // total_pixels  – total content height in pixels
    // viewport_px   – visible viewport height in pixels
    void update_state(int offset, int max_offset,
                      int total_pixels, int viewport_px);

    int offset() const { return _offset; }

signals:
    void v_offset_changed(int new_offset);

protected:
    void paintEvent(QPaintEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void leaveEvent(QEvent *e) override;
    void timerEvent(QTimerEvent *e) override;

private:
    QRect up_arrow_rect()   const;
    QRect down_arrow_rect() const;
    QRect track_rect()      const;
    QRect thumb_rect()      const;

    void start_step_timer(int direction);
    void stop_step_timer();
    void apply_step(int direction);

private:
    View   &_view;

    int     _offset         = 0;
    int     _max_offset     = 0;
    int     _total_pixels   = 1;
    int     _viewport_px    = 1;

    bool    _dragging       = false;
    int     _drag_start_y   = 0;
    int     _drag_start_off = 0;

    bool    _hover_thumb    = false;
    bool    _hover_up       = false;
    bool    _hover_down     = false;

    int     _step_timer_id  = 0;
    int     _step_direction = 0;
};

} // namespace view
} // namespace pv

#endif // DSVIEW_PV_VIEW_VERTICALSCROLLBAR_H