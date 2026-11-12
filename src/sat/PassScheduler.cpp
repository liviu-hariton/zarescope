// SPDX-License-Identifier: GPL-3.0-or-later

#include "sat/PassScheduler.h"

#include <algorithm>
#include <cctype>
#include <format>

namespace zarescope::sat {

PassScheduler::PassScheduler(std::vector<Satellite> catalog) : m_catalog(std::move(catalog)) {}

std::optional<RecordingPlan> PassScheduler::planFor(const PlannedPass& pass) const {
    const auto sat = std::find_if(m_catalog.begin(), m_catalog.end(),
                                  [&](const Satellite& s) { return s.noradId == pass.noradId; });
    if (sat == m_catalog.end() || !sat->active || sat->satdumpPipeline.empty() ||
        sat->recordSampleRateSps == 0) {
        return std::nullopt;
    }
    const auto link = std::find_if(sat->downlinks.begin(), sat->downlinks.end(),
                                   [](const Downlink& d) { return d.mode == "lrpt"; });
    if (link == sat->downlinks.end()) {
        return std::nullopt;
    }
    return RecordingPlan{pass, link->frequencyHz, sat->recordSampleRateSps, sat->satdumpPipeline,
                         sat->satdumpArgs};
}

std::optional<RecordingPlan> PassScheduler::due(Clock::time_point now,
                                                const std::vector<PlannedPass>& passes) const {
    std::optional<RecordingPlan> best;
    for (const PlannedPass& pass : passes) {
        // Joining late needs kMinRemaining left, or half the pass for a
        // pass shorter than twice that (a simulated one).
        const auto needed = std::min<Clock::duration>(kMinRemaining,
                                                      (pass.pass.los - pass.pass.aos) / 2);
        const bool up = pass.pass.aos <= now && now + needed <= pass.pass.los;
        if (!up || isDone(pass)) {
            continue;
        }
        auto plan = planFor(pass);
        if (plan && (!best || pass.pass.maxElevationDeg > best->pass.pass.maxElevationDeg)) {
            best = std::move(plan);
        }
    }
    return best;
}

std::optional<PlannedPass> PassScheduler::next(Clock::time_point now,
                                               const std::vector<PlannedPass>& passes) const {
    std::optional<PlannedPass> first;
    for (const PlannedPass& pass : passes) {
        if (pass.pass.aos > now && !isDone(pass) && planFor(pass) &&
            (!first || pass.pass.aos < first->pass.aos)) {
            first = pass;
        }
    }
    return first;
}

void PassScheduler::markDone(const PlannedPass& pass) {
    // Forget passes older than a day: they can never be due again.
    const auto cutoff = pass.pass.aos - std::chrono::hours(24);
    std::erase_if(m_done, [&](const PlannedPass& p) { return p.pass.los < cutoff; });
    if (!isDone(pass)) {
        m_done.push_back(pass);
    }
}

bool PassScheduler::isDone(const PlannedPass& pass) const {
    return std::any_of(m_done.begin(), m_done.end(), [&](const PlannedPass& p) {
        const auto apart = p.pass.aos > pass.pass.aos ? p.pass.aos - pass.pass.aos
                                                      : pass.pass.aos - p.pass.aos;
        return p.noradId == pass.noradId && apart < kSamePassTolerance;
    });
}

std::string productFolderName(Clock::time_point aos, std::string_view satellite) {
    using namespace std::chrono;
    const auto day = floor<days>(aos);
    const year_month_day date{day};
    const hh_mm_ss time{floor<minutes>(aos - day)};
    std::string name = std::format("{:04}-{:02}-{:02}_{:02}{:02}Z_", static_cast<int>(date.year()),
                                   static_cast<unsigned>(date.month()),
                                   static_cast<unsigned>(date.day()), time.hours().count(),
                                   time.minutes().count());
    for (const char c : satellite) {
        const auto u = static_cast<unsigned char>(c);
        if (std::isalnum(u) != 0 || c == '-' || c == '_') {
            name += c;
        } else if (c == ' ' && name.back() != '_') {
            name += '_';
        }
    }
    return name;
}

}  // namespace zarescope::sat
