#pragma once
#include "screens/markets/MarketPanelConfig.h"

#include <QVector>

namespace fincept::screens {

/// Persists user-configured market panels via SettingsRepository.
/// Falls back to the 9 built-in defaults on first run.
class MarketPanelStore {
  public:
    static MarketPanelStore& instance();

    /// Load all panels (from DB or defaults).
    QVector<MarketPanelConfig> load();

    /// Persist the full panel list.
    void save(const QVector<MarketPanelConfig>& panels);

    /// Reset to factory defaults and persist.
    void reset_to_defaults();

    /// True when the last load() could not read the stored panels (the defaults
    /// are shown and save() refuses to overwrite the stored set this session).
    bool load_failed() const;

  private:
    MarketPanelStore() = default;
    static QVector<MarketPanelConfig> build_defaults();

    static constexpr const char* kSettingsKey = "markets_panel_configs";
    static constexpr const char* kCategory = "markets_panels";
};

} // namespace fincept::screens
