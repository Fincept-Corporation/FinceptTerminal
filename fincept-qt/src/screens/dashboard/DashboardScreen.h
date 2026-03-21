#pragma once
#include <QWidget>
#include <QSplitter>
#include <QShowEvent>
#include <QHideEvent>
#include "screens/dashboard/DashboardToolBar.h"
#include "screens/dashboard/TickerBar.h"
#include "screens/dashboard/WidgetGrid.h"
#include "screens/dashboard/MarketPulsePanel.h"
#include "screens/dashboard/DashboardStatusBar.h"

namespace fincept::screens {

/// Main dashboard screen — toolbar, ticker, widget grid, market pulse, status bar.
class DashboardScreen : public QWidget {
    Q_OBJECT
public:
    explicit DashboardScreen(QWidget* parent = nullptr);

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    DashboardToolBar*   toolbar_      = nullptr;
    TickerBar*          ticker_bar_   = nullptr;
    WidgetGrid*         widget_grid_  = nullptr;
    MarketPulsePanel*   market_pulse_ = nullptr;
    DashboardStatusBar* status_bar_   = nullptr;
    QSplitter*          content_split_= nullptr;
    bool                pulse_visible_= true;
};

} // namespace fincept::screens
