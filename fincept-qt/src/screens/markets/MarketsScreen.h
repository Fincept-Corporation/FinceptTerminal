#pragma once
#include "screens/markets/MarketPanel.h"
#include "screens/markets/MarketPanelConfig.h"

#include <QComboBox>
#include <QDateTime>
#include <QEvent>
#include <QHideEvent>
#include <QLabel>
#include <QPushButton>
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
    void changeEvent(QEvent* event) override;

  private:
    /// Re-apply tr() lookups to every widget whose text we keep a handle to.
    /// Called from changeEvent() on QEvent::LanguageChange.
    void retranslateUi();
    /// Re-applies the AUTO button's localized label + amber/dim styling.
    void update_auto_style();

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
    QWidget*     header_bar_     = nullptr;
    QLabel*      brand_label_    = nullptr;
    QLabel*      session_label_  = nullptr;
    QLabel*      ny_label_       = nullptr;
    QLabel*      lon_label_      = nullptr;
    QLabel*      tok_label_      = nullptr;
    QLabel*      status_label_   = nullptr;
    QLabel*      last_upd_label_ = nullptr;
    QPushButton* refresh_btn_    = nullptr;
    QPushButton* auto_btn_       = nullptr;
    QPushButton* add_panel_btn_  = nullptr;
    QPushButton* reset_btn_      = nullptr;
    QComboBox*   interval_combo_ = nullptr;

    // Current status badge state — so retranslateUi() can re-apply the right
    // localized label (● READY / ● LOADING / ● TIMEOUT) on language switch.
    enum class StatusState { Ready, Loading, Timeout };
    StatusState status_state_ = StatusState::Ready;

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
