#include "screens/dashboard/DashboardScreen.h"
#include "ui/theme/Theme.h"
#include <QVBoxLayout>
#include <QShowEvent>
#include <QHideEvent>

namespace fincept::screens {

DashboardScreen::DashboardScreen(QWidget* parent) : QWidget(parent) {
    setStyleSheet(QString("background: %1;").arg(ui::colors::BG_BASE));

    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // ── Top Toolbar ──
    toolbar_ = new DashboardToolBar;
    vl->addWidget(toolbar_);

    // ── Scrolling Ticker Bar ──
    ticker_bar_ = new TickerBar;
    vl->addWidget(ticker_bar_);

    // ── Main Content: Widget Grid + Market Pulse ──
    content_split_ = new QSplitter(Qt::Horizontal);
    content_split_->setHandleWidth(1);
    content_split_->setStyleSheet(
        "QSplitter::handle { background: #1a1a1a; }"
        "QSplitter::handle:hover { background: #333333; }");

    widget_grid_  = new WidgetGrid;
    market_pulse_ = new MarketPulsePanel;

    content_split_->addWidget(widget_grid_);
    content_split_->addWidget(market_pulse_);
    content_split_->setStretchFactor(0, 3);
    content_split_->setStretchFactor(1, 1);
    content_split_->setSizes({900, 280});

    vl->addWidget(content_split_, 1);

    // ── Bottom Status Bar ──
    status_bar_ = new DashboardStatusBar;
    vl->addWidget(status_bar_);

    // Update widget count in toolbar and status bar
    toolbar_->set_widget_count(widget_grid_->widget_count());
    status_bar_->set_widget_count(widget_grid_->widget_count());

    // ── Connect toolbar signals ──
    connect(toolbar_, &DashboardToolBar::toggle_pulse_clicked, this, [this]() {
        pulse_visible_ = !pulse_visible_;
        market_pulse_->setVisible(pulse_visible_);
    });
}

void DashboardScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (ticker_bar_) ticker_bar_->resume();
}

void DashboardScreen::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    if (ticker_bar_) ticker_bar_->pause();
}

} // namespace fincept::screens
