#pragma once
#include "screens/markets/MarketPanel.h"
#include "screens/markets/MarketPanelConfig.h"

#include <QDateTime>
#include <QHideEvent>
#include <QLabel>
#include <QShowEvent>
#include <QSplitter>
#include <QTimer>
#include <QVector>
#include <QWidget>

namespace fincept::screens {

/// Markets terminal — workstation with QSplitter panel layout.
/// Panels fill screen height; horizontal and vertical handles are user-resizable.
/// Splitter state is persisted to SettingsRepository.
class MarketsScreen : public QWidget {
    Q_OBJECT
  public:
    explicit MarketsScreen(QWidget* parent = nullptr);

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

  private:
    // Layout
    void build_splitter_layout();
    void rebuild_splitter_layout();
    void wire_panel(MarketPanel* p);
    void save_splitter_state();
    void restore_splitter_state();
    int  column_with_fewest_panels() const;

    // Panel management
    void open_editor(const QString& panel_id);
    void open_editor_for_new_panel(int col_index);
    void on_panel_delete(const QString& panel_id);
    void on_panel_config_changed(const MarketPanelConfig& cfg);

    // Header bar (single 36px strip)
    QWidget* build_header_bar();
    void     update_session_status();
    void     update_clocks();
    void     refresh_theme();

    // Refresh
    void refresh_all();

    // Data
    QVector<MarketPanelConfig> configs_;
    QVector<MarketPanel*>      panels_;

    // Splitter tree
    QSplitter*          h_splitter_   = nullptr;  // horizontal — columns
    QVector<QSplitter*> col_splitters_;            // vertical — panels within each column

    // Header bar widgets
    QWidget* header_bar_    = nullptr;
    QLabel*  session_label_ = nullptr;
    QLabel*  ny_label_      = nullptr;
    QLabel*  lon_label_     = nullptr;
    QLabel*  tok_label_     = nullptr;
    QLabel*  status_label_  = nullptr;
    QLabel*  last_upd_label_ = nullptr;

    // Timers
    QTimer* auto_refresh_timer_ = nullptr;
    QTimer* session_timer_      = nullptr;
    QTimer* clock_timer_        = nullptr;
    QTimer* refresh_timeout_    = nullptr;

    // State
    bool      refresh_in_progress_ = false;
    bool      auto_update_         = true;
    int       update_interval_ms_  = 600000;
    int       pending_refreshes_   = 0;
    QDateTime last_refresh_time_;

    static constexpr int kMinRefreshIntervalSec = 300;
    static constexpr int kRefreshTimeoutMs      = 10000;
    static constexpr int kNumColumns            = 3;
};

} // namespace fincept::screens
