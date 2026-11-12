// SPDX-License-Identifier: GPL-3.0-or-later
//
// DeviceArbiter: who owns the RTL-SDR stick right now (docs/ARCHITECTURE.md).
//
// A stick can be opened by one process at a time, and inside Zarescope two
// parties want it: the user (live view, listening) and the pass scheduler
// (recording a satellite pass). The arbiter is the single place that says
// who's in charge, as a small state machine:
//
//   Idle ──sourceOpened──▶ Live(User)
//   Idle | Live(User) ──claimForPass──▶ Live(Scheduler) ──releasePass──▶ back
//   Live ──beginHandoff──▶ HandoffPending ──handoffDone──▶ External
//   External ──beginReclaim──▶ Reclaiming ──reclaimDone──▶ back (Live or Idle)
//
// A scheduled pass outranks the user's live view: claimForPass succeeds
// from Live(User), and the user's controls stay locked until the pass is
// released. In v1 SatDump decodes our recording file afterwards, so the
// handoff to an external process (HandoffPending → External → Reclaiming)
// isn't used yet; it is here, tested, for rtl_433 in v2.
//
// Pure state, no threads: the UI thread owns it.

#pragma once

#include <string_view>

namespace zarescope::device {

enum class DeviceState { Idle, Live, HandoffPending, External, Reclaiming };
enum class DeviceOwner { None, User, Scheduler, External };

class DeviceArbiter {
public:
    // The user opened or closed a source (stick or recording).
    void sourceOpened();
    void sourceClosed();

    // A pass is starting: the scheduler takes over. False when the stick is
    // with an external process, or a pass already holds it.
    bool claimForPass();
    // The pass is over: back to the user (Live), or Idle if nothing was
    // open before the pass.
    void releasePass();

    // v2: hand the stick to another process and take it back. Each returns
    // false (and changes nothing) when called in the wrong state.
    bool beginHandoff();
    bool handoffDone();
    bool beginReclaim();
    bool reclaimDone();

    [[nodiscard]] DeviceState state() const { return m_state; }
    [[nodiscard]] DeviceOwner owner() const { return m_owner; }
    // The user's controls (tuning, gain, device) are usable.
    [[nodiscard]] bool userMayControl() const {
        return m_owner == DeviceOwner::User || m_owner == DeviceOwner::None;
    }

private:
    DeviceState m_state = DeviceState::Idle;
    DeviceOwner m_owner = DeviceOwner::None;
    bool m_userHadSource = false;  // what releasePass() returns to
};

std::string_view stateName(DeviceState state);

}  // namespace zarescope::device
