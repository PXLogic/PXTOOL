// PXTOOL/pv/view/waveformscrollbar.h
#ifndef DSVIEW_PV_VIEW_WAVEFORMSCROLLBAR_H
#define DSVIEW_PV_VIEW_WAVEFORMSCROLLBAR_H

#include <QWidget>
#include <stdint.h>

namespace pv {
namespace view {

class View;

class WaveformScrollBar : public QWidget
{
    Q_OBJECT
public:
    static const int ArrowWidth  = 16;
    static const int MinThumbWidth = 20;

    explicit WaveformScrollBar(View &view, QWidget *parent);

    // Called by View::update_scroll() to push current state
    void update_state(int64_t offset, int64_t min_offset, int64_t max_offset,
                      int64_t total_pixels, int viewport_pixels);

signals:
    void offset_changed(int64_t new_offset);

protected:
    void paintEvent(QPaintEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void leaveEvent(QEvent *e) override;
    void timerEvent(QTimerEvent *e) override;

private:
    QRect left_arrow_rect() const;
    QRect right_arrow_rect() const;
    QRect track_rect() const;
    QRect thumb_rect() const;

    void start_step_timer(int direction);  // direction: -1 or +1
    void stop_step_timer();
    void apply_step(int direction);

private:
    View       &_view;

    int64_t     _offset        = 0;
    int64_t     _min_offset    = 0;
    int64_t     _max_offset    = 0;
    int64_t     _total_pixels  = 1;
    int         _viewport_pixels = 1;

    bool        _dragging      = false;
    int         _drag_start_x  = 0;
    int64_t     _drag_start_offset = 0;

    bool        _hover_thumb   = false;
    bool        _hover_left    = false;
    bool        _hover_right   = false;

    int         _step_timer_id  = 0;
    int         _step_direction = 0;   // -1 = left, +1 = right
};

} // namespace view
} // namespace pv

#endif // DSVIEW_PV_VIEW_WAVEFORMSCROLLBAR_H