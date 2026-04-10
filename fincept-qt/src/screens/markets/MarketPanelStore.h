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

  private:
    MarketPanelStore() = default;
    static QVector<MarketPanelConfig> build_defaults();

    static constexpr const char* kSettingsKey = "markets_panel_configs";
    static constexpr const char* kCategory    = "markets_panels";
};

} // namespace fincept::screens
