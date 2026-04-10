#pragma once
#include "screens/markets/MarketPanelConfig.h"
#include "services/markets/MarketDataService.h"

#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QWidget>

namespace fincept::screens {

/// Single market category panel — resizable Bloomberg-style workstation tile.
/// Fills all height given by parent QSplitter.
/// Dynamic row count: recalculated on resizeEvent().
/// Per-panel column configuration via [COLS] inline dropdown.
/// Always-visible [EDIT] [DEL] text buttons.
/// Inline error/retry state.
class MarketPanel : public QWidget {
    Q_OBJECT
  public:
    explicit MarketPanel(const MarketPanelConfig& config, QWidget* parent = nullptr);

    void refresh();
    const QString& panel_id() const { return config_.id; }
    void update_config(const MarketPanelConfig& cfg);

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
    void refresh_theme();
    void open_cols_dropdown();

    MarketPanelConfig              config_;
    QVector<services::QuoteData>   cached_quotes_;  // all fetched data; display subset shown
    bool has_data_    = false;
    bool fetch_failed_ = false;

    // Header widgets (28px)
    QWidget*     header_      = nullptr;
    QLabel*      title_label_ = nullptr;
    QPushButton* cols_btn_    = nullptr;
    QPushButton* edit_btn_    = nullptr;
    QPushButton* delete_btn_  = nullptr;

    // Body: data table or error state
    QWidget*      body_         = nullptr;
    QTableWidget* table_        = nullptr;
    QWidget*      error_widget_ = nullptr;
    QLabel*       error_label_  = nullptr;
    QPushButton*  retry_btn_    = nullptr;

    static constexpr int kHeaderH    = 28;
    static constexpr int kColHeaderH = 22;
    static constexpr int kRowH       = 22;
};

} // namespace fincept::screens
