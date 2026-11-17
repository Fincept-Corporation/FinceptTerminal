#pragma once
// ModelRouter.h — THE answer to "which model answers feature X".
//
// One resolution rule for every AI feature that goes through LlmService (AI
// Chat, the Quick Chat bubble, Dashboard Quick Ask, the workflow tool picker)
// and for Chat Mode's harness engine. Nothing else on these routes reads
// llm_configs.is_active directly.
//
// Order (first usable entry answers; the rest are the fallback chain):
//   1. Explicit choice for the feature: llm_profile_assignments(feature,
//      context_id), then the feature's type-level row — a profile may name a
//      harness (provider "harness:<id>", model, reasoning_effort, no key).
//   2. The connected harness: the user's default harness (else the first one
//      switched on) when it is eligible (API-key authentication — never a
//      personal subscription; owner decision 1), consented (decision 2),
//      switched on for terminal AI, the feature's harness fit is not NO, and
//      the feature's data policy allows it (features that send the user's
//      financial data need the per-feature switch).
//   3. A configured API provider: the global default profile, then the active
//      llm_configs row when it has a key (or needs none) and is not Fincept.
//   4. Fincept LLM.
// A harness that is skipped is reported with the reason (SkippedChoice), so
// the UI can say "Claude Code: Workbench only — sign in with an API key".
//
// Threading: resolve*() is callable from any thread. It works on a snapshot of
// the inputs (profiles, assignments, the active provider row, harness states,
// settings) that the GUI thread refreshes when any of them changes; workers
// never touch HarnessRegistry or the settings store.
//
// The rule itself (resolve_with, decide_fallback, the feature table) is pure
// QtCore code in ModelRouter.cpp, unit-tested with hand-built RouterStates; the
// live singleton is in ModelRouter_Live.cpp.

#include "services/harness/HarnessEligibility.h"
#include "services/harness/HarnessTypes.h"

#include <QHash>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QVector>

#include <optional>

class QTimer;

namespace fincept::ai_chat {

// ── Features ─────────────────────────────────────────────────────────────────

/// How well a harness (a CLI agent run per call) can serve the feature.
enum class HarnessFit { Good, Ok, No };

struct FeatureSpec {
    const char* key;
    const char* label;
    HarnessFit harness_fit;
    /// Sends the user's holdings / positions / account data to the model:
    /// excluded from the automatic harness default until the user allows it
    /// for this feature (owner decision 2).
    bool sends_user_financial_data;
    bool needs_streaming;
    bool needs_tools;
    bool needs_json;
    /// A harness only by explicit choice, never as the automatic default
    /// (Alpha Arena: ~480 calls per agent per day at the default cadence).
    bool explicit_harness_only;
    /// Goes through ModelRouter in this build (false = still on its own route;
    /// listed so Settings can say so).
    bool routed;
    /// Why a not-routed feature is not (Settings).
    const char* not_routed_reason;
};

/// Every AI feature of the terminal, routed or not (registration order).
const QVector<FeatureSpec>& feature_specs();
/// nullptr for an unknown key.
const FeatureSpec* feature_spec(const QString& key);

inline constexpr const char* kFeatureAiChat = "ai_chat";
inline constexpr const char* kFeatureQuickChat = "quick_chat";
inline constexpr const char* kFeatureQuickAsk = "quick_ask";
inline constexpr const char* kFeatureToolPicker = "workflow_tool_picker";
inline constexpr const char* kFeatureChatModeHarness = "chat_mode_harness";
inline constexpr const char* kFeatureArena = "arena";

// ── Choices ──────────────────────────────────────────────────────────────────

struct ModelChoice {
    enum class Kind { Harness, Api, Fincept };
    Kind kind = Kind::Fincept;
    QString harness_id; // Harness
    QString provider;   // Api: provider id ("openai"); Harness: "harness:<id>"; Fincept: "fincept"
    QString model;
    QString effort;     // Harness: reasoning effort ("" = the CLI's default)
    QString profile_id; // the llm_profiles row this came from ("" = registry defaults / active row)
    QString label;      // "Claude Code · sonnet · high", "OpenAI · gpt-5", "Fincept LLM"
    /// explicit | harness_default | profile | active_provider | fincept_fallback
    QString reason;
    QString vendor;     // who is billed: "Anthropic", "OpenAI", "Fincept"
    /// Harness: the call checks the CLI's own report that it uses an API key.
    bool require_api_key_source = true;

    bool is_harness() const { return kind == Kind::Harness; }
    bool is_fincept() const { return kind == Kind::Fincept; }
    /// The provider string stored with messages: "harness:<id>", "openai", "fincept".
    QString provider_id() const;
    /// Same model on the same route (dedupe).
    bool same_route(const ModelChoice& o) const;
};

/// A candidate the rule passed over, with the reason (shown in pickers and Settings).
struct SkippedChoice {
    QString what;   // "Claude Code", "Profile “Fast Groq”"
    QString reason; // "Workbench only — …", "no API key"
    QString harness_id;
};

struct RouteResult {
    QVector<ModelChoice> chain; // first = primary; never empty (Fincept LLM closes it)
    QVector<SkippedChoice> skipped;
};

// ── Inputs (a snapshot; built live or by a test) ─────────────────────────────

struct RouterHarness {
    QString id;
    QString display_name;
    QString vendor;
    harness::TerminalEligibility eligibility;
    QString default_model;  // registry default ("" = the CLI's default)
    QString default_effort;
    bool enabled_for_terminal = false; // the user's "Use for terminal AI" switch
};

struct RouterProfile {
    QString id;
    QString name;
    QString provider; // "harness:<id>" for a harness profile
    QString model;
    QString effort;
    bool is_default = false;
    bool has_key = false;       // an API key is stored (or inherited from the provider row)
    bool key_required = true;   // ProviderCatalog::requires_api_key(provider)
};

struct RouterActiveRow {
    bool present = false;
    QString provider;
    QString model;
    bool has_key = false;
    bool key_required = true;
};

struct RouterState {
    QVector<RouterHarness> harnesses; // registration order
    QString default_harness;          // HarnessRegistry::default_harness()
    /// "<feature>|<context_id>" (context may be empty: the type-level row) → profile id
    QHash<QString, QString> assignments;
    QHash<QString, RouterProfile> profiles;
    RouterActiveRow active_row;
    /// feature → "a harness may be its automatic default" (absent = the spec's default)
    QHash<QString, bool> harness_default_allowed;
    /// Provider id → display name / vendor (ProviderCatalog), for labels.
    QHash<QString, QString> provider_names;
};

/// When a harness is unavailable before any output (owner decision 3).
enum class UnavailableBehaviour { Fallback, Ask, Fail };

struct FallbackDecision {
    bool fall_back = false; // answer with chain[next_index] now
    int next_index = -1;
    bool ask = false;       // stop and offer the next entry to the user
    QString notice;         // "Claude Code is rate limited — answered by Fincept LLM"
};

// ── The router ───────────────────────────────────────────────────────────────

class ModelRouter : public QObject {
    Q_OBJECT
  public:
    static ModelRouter& instance();

    // ── The rule (pure) ──
    static RouteResult resolve_with(const RouterState& state, const QString& feature, const QString& context_id = {});
    /// After chain[failed_index] (a harness) failed with `kind`: fall back, ask or fail.
    static FallbackDecision decide_fallback(const QVector<ModelChoice>& chain, int failed_index,
                                            harness::FailureKind kind, bool output_started, bool cancelled,
                                            bool subscription_detected, UnavailableBehaviour behaviour);
    /// "<feature>|<context_id>"
    static QString assignment_key(const QString& feature, const QString& context_id);
    /// A harness may be the feature's automatic default unless the user said otherwise.
    static bool default_allowed_for(const RouterState& state, const QString& feature);
    /// "Claude Code · sonnet · high"
    static QString harness_label(const QString& display_name, const QString& model, const QString& effort);
    /// "0 credits · billed to your Anthropic API key" (cloud: "… to your cloud provider account").
    static QString billing_note(const ModelChoice& choice);

    // ── Live (any thread) ──
    RouteResult route(const QString& feature, const QString& context_id = {}) const;
    QVector<ModelChoice> resolve_chain(const QString& feature, const QString& context_id = {}) const;
    ModelChoice resolve(const QString& feature, const QString& context_id = {}) const;
    RouterState state() const;
    /// The harness rows as they are now (Settings, the picker).
    QVector<RouterHarness> harness_states() const;
    /// One harness row computed from the registry right now (not the snapshot).
    /// GUI thread only (the chat backend's last check before a call).
    std::optional<RouterHarness> current_harness(const QString& harness_id) const;

    // ── Settings (GUI thread) ──
    UnavailableBehaviour unavailable_behaviour() const;
    void set_unavailable_behaviour(UnavailableBehaviour b);
    bool harness_default_allowed(const QString& feature) const;
    void set_harness_default_allowed(const QString& feature, bool allowed);

    /// The user's explicit choice for a feature (the model picker). A harness /
    /// Fincept choice is stored as a profile (created or reused); "Auto" =
    /// clear_choice(). Returns false (with `error`) when it cannot be stored.
    bool set_choice(const QString& feature, const QString& context_id, const ModelChoice& choice, QString* error = nullptr);
    void clear_choice(const QString& feature, const QString& context_id = {});
    /// The explicit choice's profile id ("" = Auto).
    QString choice_profile_id(const QString& feature, const QString& context_id = {}) const;

    /// The chat backend saw a harness answer without its API key (a
    /// subscription session): keep it out of terminal AI for a while.
    void refuse_harness_for(const QString& harness_id, qint64 ms);

    /// Re-read every input now (GUI thread). Called on the change signals.
    void refresh();

  signals:
    /// Any input changed: pickers and badges re-resolve.
    void routes_changed();

  private:
    ModelRouter();
    void refresh_now();
    void schedule_refresh();

    mutable QMutex mutex_;
    RouterState state_;
    bool have_state_ = false;
    bool refresh_pending_ = false;
    QHash<QString, qint64> refused_until_; // harness id → epoch ms
    UnavailableBehaviour behaviour_ = UnavailableBehaviour::Fallback;
    QTimer* expiry_timer_ = nullptr; // GUI thread: re-resolve when a cooldown ends
};

QString unavailable_behaviour_id(UnavailableBehaviour b);
UnavailableBehaviour unavailable_behaviour_from_id(const QString& id);

} // namespace fincept::ai_chat
