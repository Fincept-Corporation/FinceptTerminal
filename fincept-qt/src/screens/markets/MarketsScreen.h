#pragma once
#include "screens/markets/MarketPanel.h"

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
    QTimer* loading_animation_timer_ = nullptr;
    QLabel* status_label_ = nullptr;
    bool loading_in_progress_ = false;
    bool refresh_queued_ = false;
    int pending_panels_ = 0;
    int loading_frame_ = 0;
    bool auto_update_ = true;
    int update_interval_ms_ = 600000; // 10 min default

    QWidget* build_header();
    QWidget* build_controls();
    void refresh_all();
    void begin_loading();
    void finish_loading();
    void on_panel_refresh_finished();
    QString loading_text();

    QLabel* last_update_label_ = nullptr;
};

} // namespace fincept::screens
