#pragma once
#include "screens/markets/MarketPanel.h"

#include <QDateTime>
#include <QHideEvent>
#include <QLabel>
#include <QShowEvent>
#include <QTimer>
#include <QVector>
#include <QWidget>

namespace fincept::screens {

/// Markets terminal — global + regional market panels with auto-refresh.
/// Mirrors Tauri's MarketsTab layout.
class MarketsScreen : public QWidget {
    Q_OBJECT
  public:
    explicit MarketsScreen(QWidget* parent = nullptr);

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

  private:
    QVector<MarketPanel*> global_panels_;
    QVector<MarketPanel*> regional_panels_;
    QTimer* auto_refresh_timer_ = nullptr;
    QLabel* status_label_ = nullptr;
    bool refresh_in_progress_ = false;
    bool auto_update_ = true;
    int update_interval_ms_ = 600000; // 10 min default
    QDateTime last_refresh_time_;
    static constexpr int kMinRefreshIntervalSec = 300; // 5 min

    QWidget* build_header();
    QWidget* build_controls();
    void refresh_all();
    void refresh_theme();

    QWidget* header_widget_   = nullptr;
    QWidget* controls_widget_ = nullptr;

    QLabel* last_update_label_ = nullptr;
};

} // namespace fincept::screens
