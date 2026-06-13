#pragma once
// ArenaModelRegistry — unified model discovery + round-time credential
// resolution across all 3 LLM settings layers (spec §3.4). Keys are NEVER
// cached or stored here.
#include "core/result/Result.h"
#include <QObject>
#include <QString>
#include <QVector>

namespace fincept::arena {

struct ArenaModelOption {
    QString provider, model_id, display_name, color_hex;
    QString source_kind;   // "profile" | "model" | "provider"
    QString source_ref;    // profile/model row id; provider name for "provider"
    bool ready = false;    // best-effort UI hint as of list time;
                           // resolve_credentials() is authoritative at round time
};
struct ArenaCredentials { QString api_key, base_url; };

// All methods are GUI-thread-only (the singleton settings repositories are
// not synchronized).
class ArenaModelRegistry : public QObject {
    Q_OBJECT
  public:
    static ArenaModelRegistry& instance();
    /// Merged, deduplicated by (provider, model_id). Priority: profile > model > provider.
    QVector<ArenaModelOption> available_models() const;
    /// Round-time resolution; reads Settings storage live.
    Result<ArenaCredentials> resolve_credentials(const QString& source_kind,
                                                 const QString& source_ref,
                                                 const QString& provider) const;
  signals:
    void models_changed();
  private:
    ArenaModelRegistry();   // connects LlmService::config_changed → models_changed
};

} // namespace fincept::arena
