// SPDX-License-Identifier: GPL-3.0-or-later
//
// PassScheduler: decides which satellite pass to record, and how (M5).
//
// Pure logic, no clock of its own and no threads: the caller passes the
// time in, so tests run a whole day of passes with a fake clock. What it
// decides:
//
//   eligible   the satellite is active in the catalog, has an LRPT downlink,
//              a SatDump pipeline and a recording sample rate
//   due        the eligible pass that is up right now (AOS ≤ now < LOS),
//              unless it was already recorded or skipped. Two passes can
//              overlap (Meteor-M N2-3 and N2-4 share 137.9 MHz): the higher
//              one wins. Starting late is fine while at least
//              kMinRemaining of the pass is left.
//   next       the first eligible pass still to come
//
// The actual work (taking the stick, tuning, recording, running SatDump)
// is app::PassAutomation's.

#pragma once

#include "sat/Orbit.h"
#include "sat/SatCatalog.h"
#include "sat/SatelliteService.h"

#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace zarescope::sat {

// Everything needed to record and decode one pass.
struct RecordingPlan {
    PlannedPass pass;
    int64_t frequencyHz = 0;
    uint32_t sampleRateSps = 0;
    std::string pipeline;
    std::vector<std::string> satdumpArgs;
};

class PassScheduler {
public:
    // Joining a pass already in progress is worth it only with this much left
    // (or half the pass, when the pass is shorter than twice this).
    static constexpr auto kMinRemaining = std::chrono::minutes(2);
    // Predictions move by a few seconds when a TLE is refreshed; a pass is
    // "the same pass" within this much of its AOS.
    static constexpr auto kSamePassTolerance = std::chrono::minutes(10);

    explicit PassScheduler(std::vector<Satellite> catalog);

    // The plan for a pass, or std::nullopt when its satellite can't be
    // recorded (inactive, unknown, no LRPT downlink, no pipeline or rate).
    [[nodiscard]] std::optional<RecordingPlan> planFor(const PlannedPass& pass) const;

    // The pass to record at `now`, if any (see the top of the file).
    [[nodiscard]] std::optional<RecordingPlan> due(Clock::time_point now,
                                                   const std::vector<PlannedPass>& passes) const;

    // The next eligible pass that hasn't started yet at `now`.
    [[nodiscard]] std::optional<PlannedPass> next(Clock::time_point now,
                                                  const std::vector<PlannedPass>& passes) const;

    // A pass that was recorded, failed or was skipped is never due again.
    void markDone(const PlannedPass& pass);
    [[nodiscard]] bool isDone(const PlannedPass& pass) const;

private:
    std::vector<Satellite> m_catalog;
    std::vector<PlannedPass> m_done;  // only the last day's worth is kept
};

// The products folder name for a pass: "2026-11-12_0330Z_Meteor-M_N2-4".
// Only letters, digits, '-' and '_' survive from the satellite name.
std::string productFolderName(Clock::time_point aos, std::string_view satellite);

}  // namespace zarescope::sat
