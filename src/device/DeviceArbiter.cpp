// SPDX-License-Identifier: GPL-3.0-or-later

#include "device/DeviceArbiter.h"

namespace zarescope::device {

void DeviceArbiter::sourceOpened() {
    if (m_state == DeviceState::Idle) {
        m_state = DeviceState::Live;
        m_owner = DeviceOwner::User;
    }
}

void DeviceArbiter::sourceClosed() {
    // During a pass the scheduler opens and closes the stick itself; only
    // the user's own source going away returns the arbiter to Idle.
    if (m_state == DeviceState::Live && m_owner == DeviceOwner::User) {
        m_state = DeviceState::Idle;
        m_owner = DeviceOwner::None;
    }
}

bool DeviceArbiter::claimForPass() {
    const bool fromIdle = m_state == DeviceState::Idle;
    const bool fromUser = m_state == DeviceState::Live && m_owner == DeviceOwner::User;
    if (!fromIdle && !fromUser) {
        return false;
    }
    m_userHadSource = fromUser;
    m_state = DeviceState::Live;
    m_owner = DeviceOwner::Scheduler;
    return true;
}

void DeviceArbiter::releasePass() {
    if (m_owner != DeviceOwner::Scheduler) {
        return;
    }
    if (m_userHadSource) {
        m_state = DeviceState::Live;
        m_owner = DeviceOwner::User;
    } else {
        m_state = DeviceState::Idle;
        m_owner = DeviceOwner::None;
    }
}

bool DeviceArbiter::beginHandoff() {
    const bool userOrIdle = m_state == DeviceState::Idle ||
                            (m_state == DeviceState::Live && m_owner == DeviceOwner::User);
    if (!userOrIdle) {
        return false;  // a pass holds the stick, or a handoff is already under way
    }
    m_userHadSource = m_state == DeviceState::Live && m_owner == DeviceOwner::User;
    m_state = DeviceState::HandoffPending;
    m_owner = DeviceOwner::External;
    return true;
}

bool DeviceArbiter::handoffDone() {
    if (m_state != DeviceState::HandoffPending) {
        return false;
    }
    m_state = DeviceState::External;
    return true;
}

bool DeviceArbiter::beginReclaim() {
    if (m_state != DeviceState::External) {
        return false;
    }
    m_state = DeviceState::Reclaiming;
    return true;
}

bool DeviceArbiter::reclaimDone() {
    if (m_state != DeviceState::Reclaiming) {
        return false;
    }
    m_state = m_userHadSource ? DeviceState::Live : DeviceState::Idle;
    m_owner = m_userHadSource ? DeviceOwner::User : DeviceOwner::None;
    return true;
}

std::string_view stateName(DeviceState state) {
    switch (state) {
        case DeviceState::Idle: return "Idle";
        case DeviceState::Live: return "Live";
        case DeviceState::HandoffPending: return "HandoffPending";
        case DeviceState::External: return "External";
        case DeviceState::Reclaiming: return "Reclaiming";
    }
    return "?";
}

}  // namespace zarescope::device
