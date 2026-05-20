/*
 * This file is part of the DSView project.
 * Copyright (C) 2024 DreamSourceLab <support@dreamsourcelab.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DSVIEW_PV_DATA_GLITCHFILTER_H
#define DSVIEW_PV_DATA_GLITCHFILTER_H

#include <QObject>
#include <QThread>
#include <vector>
#include <cstdint>

#define GLITCH_FILTER_MAX_CH 64

namespace pv {
namespace data {

class LogicSnapshot;

// Which pulse polarity to remove for a given channel.
//   Both     – remove any short run regardless of level (default)
//   HighOnly – remove only short HIGH runs (0→1→0 spikes on a LOW line)
//   LowOnly  – remove only short LOW  runs (1→0→1 dips  on a HIGH line)
enum class FilterMode : uint8_t {
    Both     = 0,
    HighOnly = 1,
    LowOnly  = 2,
};

// Per-channel configuration. threshold==0 OR enabled==false means disabled.
struct GlitchFilterConfig {
    bool       enabled[GLITCH_FILTER_MAX_CH]   = {};
    bool       invert[GLITCH_FILTER_MAX_CH]    = {};
    uint32_t   threshold[GLITCH_FILTER_MAX_CH] = {};
    FilterMode mode[GLITCH_FILTER_MAX_CH]       = {}; // default = Both

    bool any_enabled() const {
        for (int i = 0; i < GLITCH_FILTER_MAX_CH; i++)
            if (invert[i] || (enabled[i] && threshold[i] > 0)) return true;
        return false;
    }
};

// One contiguous flipped region; undo means writing back !new_value.
struct FlippedRegion {
    uint64_t start;     // inclusive
    uint64_t end;       // exclusive
    int      sig_index;
    bool     new_value; // value we wrote
};

class GlitchFilter {
public:
    // Apply per `cfg` for channels [0, total_ch). Returns the undo log.
    // Skips loop snapshots (returns empty log; caller should pre-check).
    //
    // PRECONDITIONS:
    //   - The capture must be ended (no active writes from device feed).
    //   - No other thread may write to `snap` for the duration of apply().
    //   - Reads from `snap` are safe (each LogicSnapshot operation locks
    //     internally), but inter-call consistency is the caller's
    //     responsibility.
    static std::vector<FlippedRegion> apply(LogicSnapshot *snap,
                                             const GlitchFilterConfig &cfg,
                                             int total_ch);

    // Flip the recorded regions back to their original value.
    //
    // PRECONDITIONS:
    //   - Must be called on the SAME snapshot used in apply().
    //   - The snapshot must not have been reused for a new capture between
    //     apply() and undo() (region indices are absolute sample positions).
    //   - No other thread may write to `snap` for the duration of undo().
    static void undo(LogicSnapshot *snap,
                     const std::vector<FlippedRegion> &regions);
};

// QThread worker so the algorithm runs off the UI thread.
// Lives in the UI thread (created/destroyed from the dock); only run() executes
// on the worker thread. The `finished_ok` signal is auto-queued to receivers
// in the UI thread.
class GlitchFilterWorker : public QThread {
    Q_OBJECT
public:
    GlitchFilterWorker(LogicSnapshot *snap,
                       const GlitchFilterConfig &cfg,
                       int total_ch,
                       QObject *parent = nullptr);

    const std::vector<FlippedRegion>& undo_log() const { return _undo_log; }

signals:
    void finished_ok();

protected:
    void run() override;

private:
    LogicSnapshot              *_snap;
    GlitchFilterConfig          _cfg;
    int                         _total_ch;
    std::vector<FlippedRegion>  _undo_log;
};

} // namespace data
} // namespace pv

#endif // DSVIEW_PV_DATA_GLITCHFILTER_H
