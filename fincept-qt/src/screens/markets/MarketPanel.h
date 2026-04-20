#pragma once
#include "screens/markets/MarketPanelConfig.h"
#include "services/markets/MarketDataService.h"

#include <QHash>
#include <QLabel>
#include <QPushButton>
#include <QSet>
#include <QTableWidget>
#include <QTimer>
#include <QWidget>

namespace fincept::screens {

/// Single market category panel — resizable workstation tile.
/// Fills all height given by parent QSplitter.
/// Dynamic row count: recalculated on resizeEvent().
/// Per-panel column configuration via [COLS] inline dropdown.
/// Always-visible [EDIT] [DEL] text buttons.
/// Inline error/retry state.
///
/// `refresh()` re-subscribes the panel to `market:quote:<sym>` on the
/// DataHub for each configured symbol, then force-kicks the hub. Each
/// delivery updates `row_cache_` and re-renders.
/// `refresh_finished` is emitted once the initial set of quotes has arrived
/// (or on timeout) so the parent `MarketsScreen::refresh_all()` counter drains.
class MarketPanel : public QWidget {
    Q_OBJECT
  public:
    explicit MarketPanel(const MarketPanelConfig& config, QWidget* parent = nullptr);

    void refresh();
    const QString& panel_id() const { return config_.id; }
    void update_config(const MarketPanelConfig& cfg);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

  signals:
    void refresh_finished();
    void edit_requested(const QString& panel_id);
    void delete_requested(const QString& panel_id);
    void config_changed(const MarketPanelConfig& cfg);  // emitted when columns change

  protected:
    void resizeEvent(QResizeEvent* event) override;

  private:
    void build_ui();
    void setup_table_columns();
    void populate(const QVector<services::QuoteData>& quotes);
    void update_visible_rows();
    void show_error(const QString& msg);
    void show_data();
    void show_loading();
    void hide_loading();
    void tick_loading_anim();
    void refresh_theme();
    void open_cols_dropdown();

    void hub_resubscribe();
    void hub_unsubscribe_all();
    void rebuild_from_cache();

    MarketPanelConfig              config_;
    QVector<services::QuoteData>   cached_quotes_;  // all fetched data; display subset shown
    bool has_data_    = false;
    bool fetch_failed_ = false;

    QHash<QString, services::QuoteData> row_cache_;
    QSet<QString> pending_initial_;  // symbols awaiting first delivery for refresh_finished
    bool refresh_inflight_ = false;
    bool hub_active_ = false;

    // Header widgets (28px)
    QWidget*     header_      = nullptr;
    QLabel*      title_label_ = nullptr;
    QPushButton* cols_btn_    = nullptr;
    QPushButton* edit_btn_    = nullptr;
    QPushButton* delete_btn_  = nullptr;

    // Body: data table or error state
    QWidget*      body_           = nullptr;
    QTableWidget* table_          = nullptr;
    QWidget*      error_widget_   = nullptr;
    QLabel*       error_label_    = nullptr;
    QPushButton*  retry_btn_      = nullptr;

    // Loading overlay (shown until first data arrives)
    QWidget*      loading_widget_ = nullptr;
    QLabel*       loading_label_  = nullptr;
    QTimer*       loading_timer_  = nullptr;
    int           loading_frame_  = 0;

    static constexpr int kHeaderH    = 28;
    static constexpr int kColHeaderH = 22;
    static constexpr int kRowH       = 22;
};

} // namespace fincept::screens
