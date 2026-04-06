#include "screens/dashboard/WidgetGrid.h"

#include "screens/dashboard/widgets/CommoditiesWidget.h"
#include "screens/dashboard/widgets/CryptoWidget.h"
#include "screens/dashboard/widgets/EconomicCalendarWidget.h"
#include "screens/dashboard/widgets/ForexWidget.h"
#include "screens/dashboard/widgets/IndicesWidget.h"
#include "screens/dashboard/widgets/MarketSentimentWidget.h"
#include "screens/dashboard/widgets/NewsWidget.h"
#include "screens/dashboard/widgets/PerformanceWidget.h"
#include "screens/dashboard/widgets/PortfolioSummaryWidget.h"
#include "screens/dashboard/widgets/QuickTradeWidget.h"
#include "screens/dashboard/widgets/RiskMetricsWidget.h"
#include "screens/dashboard/widgets/ScreenerWidget.h"
#include "screens/dashboard/widgets/SectorHeatmapWidget.h"
#include "screens/dashboard/widgets/StockQuoteWidget.h"
#include "screens/dashboard/widgets/TopMoversWidget.h"
#include "screens/dashboard/widgets/WatchlistWidget.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

namespace fincept::screens {

WidgetGrid::WidgetGrid(QWidget* parent) : QWidget(parent) {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(
        QString("QScrollArea{border:none;background:transparent;}"
                "QScrollBar:vertical{width:6px;background:transparent;}"
                "QScrollBar::handle:vertical{background:%1;border-radius:3px;min-height:20px;}"
                "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}")
        .arg(ui::colors::BORDER_MED()));

    setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed,
            this, [this](const ui::ThemeTokens&) {
                setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
            });

    auto* content = new QWidget;
    grid_ = new QGridLayout(content);
    grid_->setContentsMargins(4, 4, 4, 4);
    grid_->setSpacing(4);

    // Row 0: Indices | Sector Heatmap | Top Movers
    auto* indices = widgets::create_indices_widget();
    auto* heatmap = new widgets::SectorHeatmapWidget;
    auto* movers = new widgets::TopMoversWidget;

    grid_->addWidget(indices, 0, 0);
    grid_->addWidget(heatmap, 0, 1);
    grid_->addWidget(movers, 0, 2);
    widget_count_ += 3;

    // Row 1: Forex | Commodities | Crypto
    auto* forex = widgets::create_forex_widget();
    auto* commodities = widgets::create_commodities_widget();
    auto* crypto = widgets::create_crypto_widget();

    grid_->addWidget(forex, 1, 0);
    grid_->addWidget(commodities, 1, 1);
    grid_->addWidget(crypto, 1, 2);
    widget_count_ += 3;

    // Row 2: News | Performance | Market Sentiment
    auto* news = new widgets::NewsWidget;
    auto* perf = new widgets::PerformanceWidget;
    auto* sentiment = new widgets::MarketSentimentWidget;

    grid_->addWidget(news, 2, 0);
    grid_->addWidget(perf, 2, 1);
    grid_->addWidget(sentiment, 2, 2);
    widget_count_ += 3;

    // Row 3: Stock Quote | Watchlist
    auto* quote = new widgets::StockQuoteWidget("AAPL");
    auto* watchlist = new widgets::WatchlistWidget;

    grid_->addWidget(quote, 3, 0);
    grid_->addWidget(watchlist, 3, 1, 1, 2); // span 2 columns
    widget_count_ += 2;

    // Row 4: Economic Calendar | Screener | Risk Metrics
    auto* econ_cal = new widgets::EconomicCalendarWidget;
    auto* screener = new widgets::ScreenerWidget;
    auto* risk = new widgets::RiskMetricsWidget;

    grid_->addWidget(econ_cal, 4, 0);
    grid_->addWidget(screener, 4, 1);
    grid_->addWidget(risk, 4, 2);
    widget_count_ += 3;

    // Row 5: Portfolio Summary | Quick Trade (spans 2 cols)
    auto* portfolio = new widgets::PortfolioSummaryWidget;
    auto* quick_trade = new widgets::QuickTradeWidget;

    grid_->addWidget(portfolio, 5, 0);
    grid_->addWidget(quick_trade, 5, 1, 1, 2); // span 2 columns
    widget_count_ += 2;

    grid_->setColumnStretch(0, 1);
    grid_->setColumnStretch(1, 1);
    grid_->setColumnStretch(2, 1);

    scroll->setWidget(content);

    auto* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(0, 0, 0, 0);
    main_layout->addWidget(scroll);
}

} // namespace fincept::screens
