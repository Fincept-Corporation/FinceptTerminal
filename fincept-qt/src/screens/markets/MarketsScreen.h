#pragma once
#include "core/symbol/IGroupLinked.h"
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
///
/// Symbol-group linked: clicking a row publishes its ticker to the screen's link group, and a
/// symbol arriving from the group is highlighted in every panel that lists it.
class MarketsScreen : public QWidget, public fincept::IGroupLinked {
    Q_OBJECT
    Q_INTERFACES(fincept::IGroupLinked)
  public:
    explicit MarketsScreen(QWidget* parent = nullptr);

    // IGroupLinked
    void set_group(fincept::SymbolGroup g) override;
    fincept::SymbolGroup group() const override { return link_group_; }
    void on_group_symbol_changed(const fincept::SymbolRef& ref) override;
    fincept::SymbolRef current_symbol() const override;

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
    int column_with_fewest_panels() const;

    // Panel management
    void open_editor(const QString& panel_id);
    void open_editor_for_new_panel(int col_index);
    void on_panel_delete(const QString& panel_id);
    void on_panel_config_changed(const MarketPanelConfig& cfg);

    // Header bar (single 36px strip)
    QWidget* build_header_bar();
    void update_session_status();
    void update_clocks();
    void refresh_theme();

    // Refresh
    void refresh_all();

    // Symbol-group link. MarketPanel exposes no row API, so the screen works on each panel's
    // table through the public QTableWidget interface (column 0 carries the raw ticker in
    // Qt::UserRole — the same contract MarketPanel's own context menu relies on).
    void publish_row_symbol(const QString& ticker);
    void apply_link_highlight();
    void schedule_link_highlight();
    fincept::SymbolGroup link_group_ = fincept::SymbolGroup::None;
    QString linked_symbol_;      // ticker highlighted in the panels (from the group or a click)
    QString clicked_symbol_;     // last row the user clicked
    bool link_highlight_pending_ = false;
    bool applying_link_highlight_ = false;

    // Data
    QVector<MarketPanelConfig> configs_;
    QVector<MarketPanel*> panels_;

    // Splitter tree
    QSplitter* h_splitter_ = nullptr;   // horizontal — columns
    QVector<QSplitter*> col_splitters_; // vertical — panels within each column
    bool h_splitter_initialized_ = false; // horizontal widths applied once (restore/equalize) on first show

    // Header bar widgets
    QWidget* header_bar_ = nullptr;
    QLabel* brand_label_ = nullptr;
    QLabel* session_label_ = nullptr;
    QLabel* ny_label_ = nullptr;
    QLabel* lon_label_ = nullptr;
    QLabel* tok_label_ = nullptr;
    QLabel* status_label_ = nullptr;
    QLabel* last_upd_label_ = nullptr;
    QPushButton* refresh_btn_ = nullptr;
    QPushButton* auto_btn_ = nullptr;
    QPushButton* add_panel_btn_ = nullptr;
    QPushButton* reset_btn_ = nullptr;
    QComboBox* interval_combo_ = nullptr;

    // Current status badge state — so retranslateUi() can re-apply the right
    // localized label (● READY / ● LOADING / ● TIMEOUT) on language switch.
    enum class StatusState { Ready, Loading, Timeout };
    StatusState status_state_ = StatusState::Ready;

    // Timers
    QTimer* auto_refresh_timer_ = nullptr;
    QTimer* session_timer_ = nullptr;
    QTimer* clock_timer_ = nullptr;
    QTimer* refresh_timeout_ = nullptr;

    /// Lifetime guard for the per-refresh `refresh_finished` connections.
    /// Destroying it disconnects every in-flight panel callback, so a cycle
    /// that timed out cannot decrement the *next* cycle's counter.
    QObject* refresh_counter_ = nullptr;
    void abort_refresh_cycle();

    // State
    bool refresh_in_progress_ = false;
    bool auto_update_ = true;
    int update_interval_ms_ = 600000;
    int pending_refreshes_ = 0;
    QDateTime last_refresh_time_;

    static constexpr int kMinRefreshIntervalSec = 300;
    static constexpr int kRefreshTimeoutMs = 10000;
    static constexpr int kNumColumns = 3;
};

} // namespace fincept::screens
