#pragma once
#include "screens/dashboard/DashboardStatusBar.h"
#include "screens/dashboard/DashboardToolBar.h"
#include "screens/dashboard/MarketPulsePanel.h"
#include "screens/dashboard/TickerBar.h"
#include "screens/dashboard/canvas/DashboardCanvas.h"
#include "services/markets/MarketDataService.h"

namespace fincept::ui {
class NotifToast;
} // namespace fincept::ui

#include <QHash>
#include <QHideEvent>
#include <QScrollArea>
#include <QShowEvent>
#include <QSplitter>
#include <QTimer>
#include <QWidget>

namespace fincept::screens {

/// Main dashboard screen — toolbar, ticker, draggable widget grid, market pulse, status bar.
///
/// The ticker driver subscribes to `market:quote:<sym>` on the DataHub for
/// every user-configured ticker symbol. A cached per-symbol QuoteData is
/// pushed to the TickerBar on each delivery via `rebuild_ticker_from_cache()`.
/// `symbols_changed` wipes old subscriptions and re-subscribes to the new
/// set. Cadence is owned by the hub scheduler, not a local refresh timer.
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
    void on_refresh_clicked();

    void hub_resubscribe_ticker();
    void hub_unsubscribe_ticker();
    void rebuild_ticker_from_cache();

    DashboardToolBar* toolbar_ = nullptr;
    TickerBar* ticker_bar_ = nullptr;
    QScrollArea* scroll_area_ = nullptr;
    DashboardCanvas* canvas_ = nullptr;
    QSplitter* content_split_ = nullptr;
    MarketPulsePanel* market_pulse_ = nullptr;
    DashboardStatusBar* status_bar_ = nullptr;
    fincept::ui::NotifToast* notif_toast_ = nullptr;
    QTimer* save_timer_ = nullptr;
    bool pulse_visible_ = true;
    bool layout_restored_ = false;
    bool split_sized_ = false;

    QHash<QString, services::QuoteData> ticker_cache_;
    QStringList ticker_subscribed_;
    bool hub_active_ = false;
};

} // namespace fincept::screens
