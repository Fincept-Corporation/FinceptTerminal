#include "screens/dashboard/canvas/WidgetRegistry.h"

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
#include "screens/dashboard/widgets/RecentFilesWidget.h"
#include "screens/dashboard/widgets/RiskMetricsWidget.h"
#include "screens/dashboard/widgets/ScreenerWidget.h"
#include "screens/dashboard/widgets/SectorHeatmapWidget.h"
#include "screens/dashboard/widgets/StockQuoteWidget.h"
#include "screens/dashboard/widgets/TopMoversWidget.h"
#include "screens/dashboard/widgets/VideoPlayerWidget.h"
#include "screens/dashboard/widgets/WatchlistWidget.h"

namespace fincept::screens {

WidgetRegistry& WidgetRegistry::instance() {
    static WidgetRegistry inst;
    return inst;
}

WidgetRegistry::WidgetRegistry() {
    // 12-column grid: default_w, default_h, min_w, min_h

    // ── Markets ───────────────────────────────────────────────────────────────
    register_widget({"indices", "Market Indices", "Markets", "Major global indices — SPY, QQQ, DIA, IWM", 4, 5, 3, 4,
                     []() { return widgets::create_indices_widget(); }});

    register_widget({"forex", "Forex Pairs", "Markets", "Major FX pairs — EURUSD, GBPUSD, USDJPY", 4, 4, 2, 3,
                     []() { return widgets::create_forex_widget(); }});

    register_widget({"crypto", "Crypto Markets", "Markets", "Top cryptocurrencies — BTC, ETH, BNB, SOL", 4, 4, 2, 3,
                     []() { return widgets::create_crypto_widget(); }});

    register_widget({"commodities", "Commodities", "Markets", "Gold, Oil, Natural Gas, Copper", 4, 4, 2, 3,
                     []() { return widgets::create_commodities_widget(); }});

    register_widget({"sector_heatmap", "Sector Heatmap", "Markets", "S&P 500 sector performance heatmap", 6, 5, 3, 4,
                     []() { return new widgets::SectorHeatmapWidget; }});

    register_widget({"top_movers", "Top Movers", "Markets", "Biggest gainers and losers today", 6, 5, 3, 4,
                     []() { return new widgets::TopMoversWidget; }});

    register_widget({"sentiment", "Market Sentiment", "Markets", "Fear & greed, bull/bear indicators", 4, 4, 2, 3,
                     []() { return new widgets::MarketSentimentWidget; }});

    // ── Research ──────────────────────────────────────────────────────────────
    register_widget({"news", "News Feed", "Research", "Latest financial news headlines", 8, 4, 3, 3,
                     []() { return new widgets::NewsWidget; }});

    register_widget({"stock_quote", "Stock Quote", "Research", "Single stock detail — price, volume, chart", 4, 5, 2, 3,
                     []() { return new widgets::StockQuoteWidget("AAPL"); }});

    register_widget({"screener", "Stock Screener", "Research", "Filter stocks by fundamentals and technicals", 6, 5, 3,
                     4, []() { return new widgets::ScreenerWidget; }});

    register_widget({"econ_calendar", "Economic Calendar", "Research", "Upcoming macro events and releases", 4, 4, 2, 3,
                     []() { return new widgets::EconomicCalendarWidget; }});

    // ── Portfolio ─────────────────────────────────────────────────────────────
    register_widget({"watchlist", "Watchlist", "Portfolio", "Your saved symbols with live prices", 6, 4, 2, 3,
                     []() { return new widgets::WatchlistWidget; }});

    register_widget({"performance", "Performance", "Portfolio", "Portfolio P&L — today, week, month, YTD", 4, 5, 3, 4,
                     []() { return new widgets::PerformanceWidget; }});

    register_widget({"portfolio_summary", "Portfolio Summary", "Portfolio",
                     "Holdings overview with allocation breakdown", 6, 4, 2, 3,
                     []() { return new widgets::PortfolioSummaryWidget; }});

    register_widget({"risk_metrics", "Risk Metrics", "Portfolio", "Volatility, beta, drawdown, Sharpe ratio", 4, 5, 3,
                     4, []() { return new widgets::RiskMetricsWidget; }});

    // ── Trading ───────────────────────────────────────────────────────────────
    register_widget({"quick_trade", "Quick Trade", "Trading", "Fast order entry for crypto and equities", 4, 5, 2, 3,
                     []() { return new widgets::QuickTradeWidget; }});

    // ── Tools ────────────────────────────────────────────────────────────────
    register_widget({"video_player", "Live TV / Streams", "Tools",
                     "Financial TV — Bloomberg, CNBC, Reuters and custom streams", 4, 5, 3, 4,
                     []() { return widgets::create_video_player_widget(); }});

    register_widget({"recent_files", "Recent Files", "Tools",
                     "Recently saved files — exports, reports, notebooks and more", 4, 4, 2, 3,
                     []() { return new widgets::RecentFilesWidget; }});
}

void WidgetRegistry::register_widget(WidgetMeta meta) {
    registry_.insert(meta.type_id, std::move(meta));
}

const WidgetMeta* WidgetRegistry::find(const QString& type_id) const {
    auto it = registry_.find(type_id);
    return (it != registry_.end()) ? &it.value() : nullptr;
}

QVector<WidgetMeta> WidgetRegistry::all() const {
    QVector<WidgetMeta> result;
    result.reserve(registry_.size());
    for (auto it = registry_.cbegin(); it != registry_.cend(); ++it)
        result.append(it.value());
    return result;
}

QVector<WidgetMeta> WidgetRegistry::by_category(const QString& category) const {
    QVector<WidgetMeta> result;
    for (const auto& m : registry_) {
        if (category.isEmpty() || m.category == category)
            result.append(m);
    }
    return result;
}

} // namespace fincept::screens
