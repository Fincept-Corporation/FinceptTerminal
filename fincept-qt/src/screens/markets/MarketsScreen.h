#pragma once
#include "screens/markets/MarketPanel.h"
#include "screens/markets/MarketPanelConfig.h"

#include <QDateTime>
#include <QGridLayout>
#include <QHideEvent>
#include <QLabel>
#include <QShowEvent>
#include <QTimer>
#include <QVector>
#include <QWidget>

namespace fincept::screens {

/// Markets terminal — global + regional market panels with auto-refresh.
/// Panels are user-configurable: add, edit, delete, reorder.
class MarketsScreen : public QWidget {
    Q_OBJECT
  public:
    explicit MarketsScreen(QWidget* parent = nullptr);

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

  private:
    void build_panel_grid();
    void clear_panel_grid();
    void open_editor(const QString& panel_id);  // empty id = new panel
    void on_panel_delete(const QString& panel_id);

    QWidget* build_header();
    QWidget* build_controls();
    void refresh_all();
    void refresh_theme();

    QVector<MarketPanelConfig> configs_;
    QVector<MarketPanel*>      panels_;

    QGridLayout* panel_grid_    = nullptr;
    QWidget*     grid_content_  = nullptr;

    QTimer* auto_refresh_timer_ = nullptr;
    QTimer* session_timer_      = nullptr;
    QTimer* clock_timer_        = nullptr;

    QLabel* status_label_      = nullptr;
    QLabel* last_update_label_ = nullptr;

    bool refresh_in_progress_ = false;
    bool auto_update_         = true;
    int  update_interval_ms_  = 600000;
    QDateTime last_refresh_time_;

    QWidget* header_widget_   = nullptr;
    QWidget* controls_widget_ = nullptr;

    static constexpr int kMinRefreshIntervalSec = 300;
};

} // namespace fincept::screens
