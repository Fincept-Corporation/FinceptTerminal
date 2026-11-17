#pragma once
// HarnessEligibility.h — may this harness answer terminal AI (AI Chat, Quick
// Chat, Quick Ask, the tool picker, Alpha Arena, Chat Mode tasks)? One pure
// rule, used by ModelRouter, the chat backend (re-checked right before a call),
// Settings and the model picker, so they can never disagree.
//
// Owner decisions (2026-11-17) implemented here:
//   1. Terminal AI never runs on a consumer subscription sign-in (vendor terms:
//      Anthropic and Google forbid third-party products routing through
//      subscription credentials; Codex unconfirmed → same rule). A harness is
//      eligible only when it authenticates with an API key: its sign-in check
//      reports an API-key or cloud-provider login, or one of the variables
//      that headless runs are documented to authenticate with
//      (descriptor.terminal_key_env_vars) reaches the child — NAMES only,
//      never values. Otherwise: "Workbench only — sign in with an API key".
//   2. Using a harness for terminal AI needs the user's one-time consent per
//      harness (a dialog naming the vendor); being the AUTOMATIC default also
//      needs "Use for terminal AI" switched on.
// Plus what a chat call technically needs: an adapter that builds chat calls
// (caps.chat) with an enforced read-only mode on this OS, an installed CLI, a
// sign-in that is not known to be signed out, and no rate-limit cooldown.
//
// QtCore only.

#include "services/harness/HarnessTypes.h"

#include <QString>
#include <QStringList>

namespace fincept::harness {

enum class TerminalState {
    Ready,              // eligible, consented and enabled: may be the automatic default
    EligibleNotEnabled, // eligible and consented, "Use for terminal AI" off: explicit choice only
    ConsentNeeded,      // eligible, but the user has not confirmed the data-sharing dialog yet
    NotInstalled,
    Detecting,
    ChatUnsupported,    // adapter refuses terminal-AI calls (BETA, read-only not verified for chat)
    ReadOnlyUnverified, // read-only cannot be verified on this OS (Codex on Windows)
    SignedOut,
    SubscriptionOnly,   // signed in with a personal subscription and no API key: Workbench only
    KeyStripped,        // an API key is set but the "subscription only" policy removes it from runs
    CoolingDown,        // rate limited / refused a moment ago
};

struct TerminalEligibilityInput {
    HarnessDescriptor descriptor;
    HarnessStatus status;
    /// descriptor.terminal_key_env_vars that are set in the environment the
    /// child gets (after the env policy). Names only.
    QStringList key_vars_in_child;
    /// descriptor.terminal_key_env_vars set in the system environment at all
    /// (before the policy) — tells KeyStripped from SubscriptionOnly.
    QStringList key_vars_in_system;
    bool enabled_for_terminal = false;
    bool consent = false;
    qint64 now_ms = 0;
    /// The chat backend refused this harness until then (it reported a
    /// subscription session although a key was expected). 0 = none.
    qint64 refused_until_ms = 0;
};

struct TerminalEligibility {
    TerminalState state = TerminalState::NotInstalled;
    /// Technically allowed now: API-key authentication, chat support with an
    /// enforced read-only mode, installed, not cooling down.
    bool eligible = false;
    /// eligible + the user's consent: may answer where the user chose it.
    bool usable = false;
    /// usable + "Use for terminal AI": may be the automatic default.
    bool usable_by_default = false;
    /// Why it may: "api_key" (API-key sign-in), "cloud" (Bedrock / Vertex /
    /// Foundry), "env_key" (a headless key variable reaches the child).
    QString basis;
    /// The chat backend checks the CLI's own report that it uses an API key
    /// (Claude Code's init.apiKeySource) — off for a cloud basis.
    bool require_api_key_source = true;
    QString label;  // short: "Ready", "Workbench only", "Signed out", "Rate limited until 14:05"
    QString reason; // one or two plain sentences (what is wrong and what to do)
    QString docs_url; // the vendor's API-key page (SubscriptionOnly / KeyStripped / SignedOut)
};

TerminalEligibility terminal_eligibility(const TerminalEligibilityInput& in);

/// Stable id of a state ("ready", "workbench_only", …) for logs and tests.
QString terminal_state_id(TerminalState s);

/// The child's environment for a terminal-AI call keeps API keys unless the
/// user's env policy is "subscription only" (and the harness is not an
/// API-key-only one). The names in `system_names` that reach the child.
QStringList key_vars_reaching_child(const HarnessDescriptor& d, const QStringList& system_names, bool subscription_only);

/// One stdout line of a running terminal-AI call: non-empty = the CLI reports a
/// session that is NOT authenticated with an API key (Claude Code's system/init
/// apiKeySource "none" — its subscription sign-in), so the call must stop
/// before any output. Empty for every other line and harness.
QString subscription_session_refusal(const QString& harness_id, const QString& line);

} // namespace fincept::harness
