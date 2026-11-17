#pragma once
// ArenaModelReadiness.h — why a competitor row of the New Competition wizard
// cannot be checked, in the few words the row has room for (the full sentence
// goes in the tooltip). One mapping from the terminal-AI eligibility rule
// (services/harness/HarnessEligibility.h) so the wizard says what is really
// wrong — "Needs your OK", "Workbench only — sign in with an API key",
// "Signed out", "Rate limited until 14:05" — instead of "needs API key" on
// every row.
//
// Header-only, QtCore only (plus the eligibility types). Unit test:
// tests/tst_arena_model_readiness.cpp.

#include "services/harness/HarnessEligibility.h"

#include <QCoreApplication>
#include <QString>

namespace fincept::arena::readiness {

/// Short reason for a harness competitor that may not answer the arena now;
/// empty when it may (TerminalEligibility::usable — an explicit choice does not
/// need "Use for terminal AI").
inline QString harness_not_ready_text(const harness::TerminalEligibility& e) {
    using harness::TerminalState;
    const auto t = [](const char* s) { return QCoreApplication::translate("ArenaModelReadiness", s); };
    if (e.usable)
        return {};
    switch (e.state) {
        case TerminalState::Ready:
        case TerminalState::EligibleNotEnabled:
            return {}; // usable states (usable is set for both)
        case TerminalState::ConsentNeeded:
            return t("Needs your OK");
        case TerminalState::SubscriptionOnly:
            return t("Workbench only — sign in with an API key");
        case TerminalState::KeyStripped:
            return t("Workbench only — your “subscription only” setting removes the API key");
        case TerminalState::SignedOut:
            return t("Signed out");
        case TerminalState::CoolingDown:
            // The rule's own label carries the time: "Rate limited until 14:05" / "Paused until 14:05".
            return e.label.isEmpty() ? t("Rate limited") : e.label;
        case TerminalState::ChatUnsupported:
        case TerminalState::ReadOnlyUnverified:
            return t("Workbench only");
        case TerminalState::NotInstalled:
            return t("Not installed");
        case TerminalState::Detecting:
            return t("Detecting…");
    }
    return e.label;
}

/// True when the only thing missing is the user's one-time consent: the row
/// can open the consent dialog instead of being dead.
inline bool harness_needs_consent(const harness::TerminalEligibility& e) {
    return !e.usable && e.state == harness::TerminalState::ConsentNeeded;
}

/// Short reason for an API-provider competitor without a usable key.
inline QString api_not_ready_text() {
    return QCoreApplication::translate("ArenaModelReadiness", "needs API key");
}

/// Tooltip of an API-provider competitor without a usable key.
inline QString api_not_ready_detail(const QString& provider_display_name) {
    return QCoreApplication::translate("ArenaModelReadiness",
                                       "Add an API key for %1 in Settings > LLM Configuration, then reopen this "
                                       "wizard.")
        .arg(provider_display_name);
}

/// The call-volume warning of a harness competitor (each decision is one
/// harness process, on the user's own API key and its rate limits).
inline QString harness_call_volume_warning(const QString& vendor) {
    return QCoreApplication::translate("ArenaModelReadiness",
                                       "explicit only: ≈480 calls per agent per day at the 3-minute cadence, billed "
                                       "to your %1 API key and its rate limits")
        .arg(vendor);
}

} // namespace fincept::arena::readiness
