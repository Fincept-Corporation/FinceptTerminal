#pragma once
// ArenaModelRegistry — unified model discovery + round-time credential
// resolution across all 3 LLM settings layers (spec §3.4). Keys are NEVER
// cached or stored here.
//
// Plus an agent-harness layer (source_kind "harness", source_ref = the harness
// id, provider "harness:<id>"): one option per model of each installed harness
// that can answer terminal AI. Ready only when the harness may serve terminal
// AI (API-key authentication, consented — see harness::terminal_eligibility);
// never pre-selected, and it carries the call-volume warning (`warning`). A row
// that is not ready says why (`not_ready_reason`, `detail`).
#include "core/result/Result.h"

#include <QObject>
#include <QString>
#include <QVector>

namespace fincept::arena {

struct ArenaModelOption {
    QString provider, model_id, display_name, color_hex;
    QString source_kind; // "profile" | "model" | "provider" | "harness"
    QString source_ref;  // profile/model row id; provider name for "provider"; harness id for "harness"
    bool ready = false;  // best-effort UI hint as of list time;
                         // resolve_credentials() is authoritative at round time
    /// Not ready: the short reason shown on the row ("needs API key", "Needs your
    /// OK", "Workbench only — sign in with an API key", "Signed out", "Rate limited
    /// until 14:05"; ArenaModelReadiness.h). Empty when ready.
    QString not_ready_reason;
    /// Tooltip: the full sentence (what is wrong and what to do).
    QString detail;
    /// Ready harness competitor: the call-volume warning (kept out of
    /// display_name, which becomes the agent's name on the leaderboard).
    QString warning;
    /// A harness only waiting for the user's one-time OK: the row opens the
    /// consent dialog (ui::request_harness_terminal_consent).
    bool needs_consent = false;
};
struct ArenaCredentials {
    QString api_key, base_url;
};

// All methods are GUI-thread-only (the singleton settings repositories are
// not synchronized).
class ArenaModelRegistry : public QObject {
    Q_OBJECT
  public:
    static ArenaModelRegistry& instance();
    /// Merged, deduplicated by (provider, model_id). Priority: profile > model > provider > harness.
    QVector<ArenaModelOption> available_models() const;
    /// Round-time resolution; reads Settings storage live. A harness needs no
    /// key here (it authenticates with the user's own CLI setup).
    Result<ArenaCredentials> resolve_credentials(const QString& source_kind, const QString& source_ref,
                                                 const QString& provider) const;
  signals:
    void models_changed();

  private:
    ArenaModelRegistry(); // connects LlmService::config_changed → models_changed (+ harness changes)
    /// The harness layer as it would be listed (ids, readiness, models): a
    /// harness change re-lists the wizard only when this changed.
    QString harness_layer_signature() const;
    void on_harness_state_changed();

    QString harness_signature_;
};

} // namespace fincept::arena
