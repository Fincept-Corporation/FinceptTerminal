#pragma once
#include "screens/dashboard/DashboardStatusBar.h"
#include "screens/dashboard/DashboardToolBar.h"
#include "screens/dashboard/MarketPulsePanel.h"
#include "screens/dashboard/TickerBar.h"
#include "screens/dashboard/canvas/DashboardCanvas.h"

namespace fincept::ui {
class NotifToast;
} // namespace fincept::ui

#include <QHideEvent>
#include <QScrollArea>
#include <QShowEvent>
#include <QSplitter>
#include <QTimer>
#include <QWidget>

namespace fincept::screens {

/// Main dashboard screen — toolbar, ticker, draggable widget grid, market pulse, status bar.
class DashboardScreen : public QWidget {
    Q_OBJECT
  public:
    explicit DashboardScreen(QWidget* parent = nullptr);

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

  private:
    void refresh_theme();
    void build_default_layout();
    void save_layout();
    void restore_layout();
    void refresh_ticker();

    DashboardToolBar* toolbar_ = nullptr;
    TickerBar* ticker_bar_ = nullptr;
    QScrollArea* scroll_area_ = nullptr;
    DashboardCanvas* canvas_ = nullptr;
    QSplitter* content_split_ = nullptr;
    MarketPulsePanel* market_pulse_ = nullptr;
    DashboardStatusBar* status_bar_ = nullptr;
    fincept::ui::NotifToast* notif_toast_ = nullptr;
    QTimer* save_timer_ = nullptr;
    QTimer* ticker_refresh_timer_ = nullptr;
    bool pulse_visible_ = true;
    bool layout_restored_ = false;
    bool split_sized_ = false;
};

} // namespace fincept::screens
