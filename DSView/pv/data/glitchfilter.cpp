/*
 * This file is part of the DSView project.
 * Copyright (C) 2024 DreamSourceLab <support@dreamsourcelab.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "glitchfilter.h"
#include "logicsnapshot.h"
#include <cassert>
#include <algorithm>

static_assert(GLITCH_FILTER_MAX_CH == CHANNEL_MAX_COUNT,
              "GlitchFilter's channel cap must match LogicSnapshot's CHANNEL_MAX_COUNT");

namespace pv {
namespace data {

std::vector<FlippedRegion> GlitchFilter::apply(LogicSnapshot *snap,
                                                const GlitchFilterConfig &cfg,
                                                int total_ch)
{
    assert(snap);
    std::vector<FlippedRegion> undo_log;

    if (snap->is_loop()) return undo_log;       // refuse loop captures
    if (!cfg.any_enabled()) return undo_log;

    const uint64_t total_samples = snap->get_ring_sample_count();
    if (total_samples == 0) return undo_log;

    for (int ch = 0; ch < total_ch && ch < GLITCH_FILTER_MAX_CH; ch++) {
        if (!cfg.enabled[ch] || cfg.threshold[ch] == 0) continue;
        if (!snap->has_data(ch)) continue;

        const uint64_t threshold = cfg.threshold[ch];
        const FilterMode mode    = cfg.mode[ch];
        uint64_t pos       = 0;
        // safe_back is the start of the most recently KEPT run (run length
        // strictly > threshold, or skipped due to FilterMode). Because a kept
        // run's length is > threshold, the backstep formula
        // `next_pos - threshold - 1` is guaranteed to land at or after
        // safe_back, so we never re-examine data we already confirmed cannot
        // be a glitch. This monotonic non-decreasing invariant is what
        // guarantees algorithm termination.
        uint64_t safe_back = 0;

        while (pos < total_samples) {
            bool current_val = snap->get_sample(pos, ch);

            // Find next edge after pos (search from pos+1 to total_samples-1).
            uint64_t next_pos = pos + 1;
            bool found = (next_pos < total_samples)
                       ? snap->get_nxt_edge(next_pos, current_val,
                                             total_samples - 1, 0.0, ch)
                       : false;
            if (!found) next_pos = total_samples;

            uint64_t run_len = next_pos - pos;

            if (run_len <= threshold) {
                // Skip runs whose polarity does not match the requested mode.
                // Treat them as "kept" (safe_back advances) so the monotonic
                // termination invariant is preserved.
                bool skip = (mode == FilterMode::HighOnly && !current_val)
                         || (mode == FilterMode::LowOnly  &&  current_val);
                if (skip) {
                    safe_back = pos;
                    pos = next_pos;
                    continue;
                }

                // Flip the entire short run in a single locked operation.
                bool flip_to = !current_val;
                snap->set_sample_block(pos, next_pos, ch, flip_to);
                undo_log.push_back({pos, next_pos, ch, flip_to});

                // Adjacent short pulses may merge after flipping. Re-examine
                // from `safe_back` (earliest unconfirmed position).
                uint64_t backstep = (next_pos > threshold + 1)
                                  ? (next_pos - threshold - 1) : 0;
                pos = std::max(safe_back, backstep);
            } else {
                safe_back = pos;
                pos = next_pos;
            }
        }
    }

    return undo_log;
}

void GlitchFilter::undo(LogicSnapshot *snap,
                         const std::vector<FlippedRegion> &regions)
{
    assert(snap);
    // Undo in reverse order so overlapping flips (shouldn't happen, but safe)
    // unwind in LIFO order.
    for (auto it = regions.rbegin(); it != regions.rend(); ++it) {
        bool original = !it->new_value;
        snap->set_sample_block(it->start, it->end, it->sig_index, original);
    }
}

// ---------------------------------------------------------------------------
// GlitchFilterWorker
// ---------------------------------------------------------------------------

GlitchFilterWorker::GlitchFilterWorker(LogicSnapshot *snap,
                                       const GlitchFilterConfig &cfg,
                                       int total_ch,
                                       QObject *parent)
    : QThread(parent), _snap(snap), _cfg(cfg), _total_ch(total_ch)
{
}

void GlitchFilterWorker::run()
{
    _undo_log = GlitchFilter::apply(_snap, _cfg, _total_ch);
    emit finished_ok();
}

} // namespace data
} // namespace pv
