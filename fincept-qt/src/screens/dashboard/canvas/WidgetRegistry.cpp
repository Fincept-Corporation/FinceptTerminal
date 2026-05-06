#include "screens/dashboard/canvas/WidgetRegistry.h"

#include "screens/dashboard/widgets/AgentErrorsWidget.h"
#include "screens/dashboard/widgets/BrokerHoldingsWidget.h"
#include "screens/dashboard/widgets/CommoditiesWidget.h"
#include "screens/dashboard/widgets/CryptoTickerWidget.h"
#include "screens/dashboard/widgets/CryptoWidget.h"
#include "screens/dashboard/widgets/EconomicCalendarWidget.h"
#include "screens/dashboard/widgets/ForexWidget.h"
#include "screens/dashboard/widgets/GeopoliticsEventsWidget.h"
#include "screens/dashboard/widgets/IndicesWidget.h"
#include "screens/dashboard/widgets/MarginUsageWidget.h"
#include "screens/dashboard/widgets/MaritimeVesselsWidget.h"
#include "screens/dashboard/widgets/MarketQuoteStripWidget.h"
#include "screens/dashboard/widgets/MarketSentimentWidget.h"
#include "screens/dashboard/widgets/NewsCategoryWidget.h"
#include "screens/dashboard/widgets/NewsWidget.h"
#include "screens/dashboard/widgets/NotesWidget.h"
#include "screens/dashboard/widgets/OpenPositionsWidget.h"
#include "screens/dashboard/widgets/OrderBookMiniWidget.h"
#include "screens/dashboard/widgets/PerformanceWidget.h"
#include "screens/dashboard/widgets/PolymarketPriceWidget.h"
#include "screens/dashboard/widgets/PortfolioSummaryWidget.h"
#include "screens/dashboard/widgets/QuickTradeWidget.h"
#include "screens/dashboard/widgets/RecentFilesWidget.h"
#include "screens/dashboard/widgets/RiskMetricsWidget.h"
#include "screens/dashboard/widgets/ScreenerWidget.h"
#include "screens/dashboard/widgets/SectorHeatmapWidget.h"
#include "screens/dashboard/widgets/SparklineStripWidget.h"
#include "screens/dashboard/widgets/StockQuoteWidget.h"
#include "screens/dashboard/widgets/TodayPnLWidget.h"
#include "screens/dashboard/widgets/TopMoversWidget.h"
#include "screens/dashboard/widgets/TradeTapeWidget.h"
#include "screens/dashboard/widgets/VideoPlayerWidget.h"
#include "screens/dashboard/widgets/WatchlistWidget.h"
#include "screens/dashboard/widgets/WebScraperWidget.h"

namespace fincept::screens {

WidgetRegistry& WidgetRegistry::instance() {
    static WidgetRegistry inst;
    return inst;
}

WidgetRegistry::WidgetRegistry() {
    // 12-column grid: default_w, default_h, min_w, min_h
    // Factory receives per-instance config (empty QJsonObject for fresh widgets).
    // Existing widgets ignore it until they opt into configurable behaviour.

    // ── Markets ───────────────────────────────────────────────────────────────
    register_widget({"indices", "Market Indices", "Markets", "Major global indices — SPY, QQQ, DIA, IWM", 4, 5, 3, 4,
                     [](const QJsonObject&) { return widgets::create_indices_widget(); }});

    register_widget({"forex", "Forex Pairs", "Markets", "Major FX pairs — EURUSD, GBPUSD, USDJPY", 4, 4, 2, 3,
                     [](const QJsonObject&) { return widgets::create_forex_widget(); }});

    register_widget({"crypto", "Crypto Markets", "Markets", "Top cryptocurrencies — BTC, ETH, BNB, SOL", 4, 4, 2, 3,
                     [](const QJsonObject&) { return widgets::create_crypto_widget(); }});

    register_widget({"commodities", "Commodities", "Markets", "Gold, Oil, Natural Gas, Copper", 4, 4, 2, 3,
                     [](const QJsonObject&) { return widgets::create_commodities_widget(); }});

    register_widget({"sector_heatmap", "Sector Heatmap", "Markets", "S&P 500 sector performance heatmap", 6, 5, 3, 4,
                     [](const QJsonObject&) { return new widgets::SectorHeatmapWidget; }});

    register_widget({"top_movers", "Top Movers", "Markets", "Biggest gainers and losers today", 6, 5, 3, 4,
                     [](const QJsonObject&) { return new widgets::TopMoversWidget; }});

    register_widget({"sentiment", "Market Sentiment", "Markets", "Fear & greed, bull/bear indicators", 4, 4, 2, 3,
                     [](const QJsonObject&) { return new widgets::MarketSentimentWidget; }});

    // ── Research ──────────────────────────────────────────────────────────────
    register_widget({"news", "News Feed", "Research", "Latest financial news headlines", 8, 4, 3, 3,
                     [](const QJsonObject&) { return new widgets::NewsWidget; }});

    register_widget({"stock_quote", "Stock Quote", "Research", "Single stock detail — price, volume, chart", 4, 5, 2, 3,
                     [](const QJsonObject& cfg) {
                         const QString sym = cfg.value("symbol").toString("AAPL");
                         return new widgets::StockQuoteWidget(sym);
                     }});

    register_widget({"screener", "Stock Screener", "Research", "Filter stocks by fundamentals and technicals", 6, 5, 3,
                     4, [](const QJsonObject&) { return new widgets::ScreenerWidget; }});

    register_widget({"econ_calendar", "Economic Calendar", "Research", "Upcoming macro events and releases", 4, 4, 2, 3,
                     [](const QJsonObject&) { return new widgets::EconomicCalendarWidget; }});

    // ── Portfolio ─────────────────────────────────────────────────────────────
    register_widget({"watchlist", "Watchlist", "Portfolio", "Your saved symbols with live prices", 6, 4, 2, 3,
                     [](const QJsonObject&) { return new widgets::WatchlistWidget; }});

    register_widget({"performance", "Performance", "Portfolio", "Portfolio P&L — today, week, month, YTD", 4, 5, 3, 4,
                     [](const QJsonObject&) { return new widgets::PerformanceWidget; }});

    register_widget({"portfolio_summary", "Portfolio Summary", "Portfolio",
                     "Holdings overview with allocation breakdown", 6, 4, 2, 3,
                     [](const QJsonObject&) { return new widgets::PortfolioSummaryWidget; }});

    register_widget({"risk_metrics", "Risk Metrics", "Portfolio", "Volatility, beta, drawdown, Sharpe ratio", 4, 5, 3,
                     4, [](const QJsonObject&) { return new widgets::RiskMetricsWidget; }});

    // ── Trading ───────────────────────────────────────────────────────────────
    register_widget({"quick_trade", "Quick Trade", "Trading", "Fast order entry for crypto and equities", 4, 5, 2, 3,
                     [](const QJsonObject&) { return new widgets::QuickTradeWidget; }});

    register_widget({"open_positions", "Open Positions", "Trading",
                     "Live open positions for a broker account — pick via gear icon", 6, 5, 3, 3,
                     [](const QJsonObject& cfg) { return new widgets::OpenPositionsWidget(cfg); }});

    register_widget({"working_orders", "Working Orders", "Trading",
                     "Pending/working orders for a broker account — click × to cancel", 6, 5, 3, 3,
                     [](const QJsonObject& cfg) { return new widgets::OrderBookMiniWidget(cfg); }});

    register_widget({"margin_usage", "Margin Usage", "Trading",
                     "Broker account funds — available, used margin, total, usage %", 3, 4, 2, 3,
                     [](const QJsonObject& cfg) { return new widgets::MarginUsageWidget(cfg); }});

    register_widget({"today_pnl", "Today P&L", "Trading",
                     "Aggregate broker account P&L — total, day, realized, open positions", 3, 4, 2, 3,
                     [](const QJsonObject& cfg) { return new widgets::TodayPnLWidget(cfg); }});

    register_widget({"holdings", "Holdings", "Portfolio",
                     "Long-term broker holdings — avg cost, LTP, P&L %", 6, 5, 3, 3,
                     [](const QJsonObject& cfg) { return new widgets::BrokerHoldingsWidget(cfg); }});

    // ── Tools ────────────────────────────────────────────────────────────────
    register_widget({"video_player", "Live TV / Streams", "Tools",
                     "Financial TV — major networks and custom streams", 4, 5, 3, 4,
                     [](const QJsonObject&) { return widgets::create_video_player_widget(); }});

    register_widget({"recent_files", "Recent Files", "Tools",
                     "Recently saved files — exports, reports, notebooks and more", 4, 4, 2, 3,
                     [](const QJsonObject&) { return new widgets::RecentFilesWidget; }});

    register_widget({"quote_strip", "Quote Strip", "Markets",
                     "Configurable live quote list — pick symbols via gear icon", 3, 5, 2, 3,
                     [](const QJsonObject& cfg) { return new widgets::MarketQuoteStripWidget(cfg); }});

    register_widget({"crypto_ticker", "Crypto Ticker", "Markets",
                     "Live Kraken / HyperLiquid ticker strip — configurable pair list", 3, 5, 2, 3,
                     [](const QJsonObject& cfg) { return new widgets::CryptoTickerWidget(cfg); }});

    register_widget({"polymarket_prices", "Polymarket", "Markets",
                     "Live prediction-market prices — configurable asset list", 3, 5, 2, 3,
                     [](const QJsonObject& cfg) { return new widgets::PolymarketPriceWidget(cfg); }});

    register_widget({"agent_errors", "Agent Errors", "Tools",
                     "Recent agent execution failures — subscribes to agent:error:*", 5, 4, 3, 3,
                     [](const QJsonObject& cfg) { return new widgets::AgentErrorsWidget(cfg); }});

    register_widget({"sparklines", "Sparklines", "Markets",
                     "Configurable sparkline strip — subscribes to market:sparkline:*", 4, 5, 3, 3,
                     [](const QJsonObject& cfg) { return new widgets::SparklineStripWidget(cfg); }});

    register_widget({"trade_tape", "Trade Tape", "Markets",
                     "Live trade prints for a crypto pair — ws:<exchange>:trades:<pair>", 4, 5, 3, 4,
                     [](const QJsonObject& cfg) { return new widgets::TradeTapeWidget(cfg); }});

    register_widget({"news_category", "News — Category", "Research",
                     "Category-filtered news headlines — news:category:<category>", 5, 5, 3, 3,
                     [](const QJsonObject& cfg) { return new widgets::NewsCategoryWidget(cfg); }});

    register_widget({"web_scraper", "Web Scraper", "Tools",
                     "Scrape tables from any URL — auto-detects HTML, JSON, CSV, XML/RSS", 6, 5, 3,
                     3, [](const QJsonObject& cfg) { return new widgets::WebScraperWidget(cfg); }});

    // ── Geopolitics ──────────────────────────────────────────────────────────
    register_widget({"geopolitics_events", "Geopolitics Events", "Geopolitics",
                     "Live conflict / political events — subscribes to geopolitics:events", 6, 5, 3, 3,
                     [](const QJsonObject& cfg) { return new widgets::GeopoliticsEventsWidget(cfg); }});

    register_widget({"maritime_vessels", "Maritime Vessels", "Geopolitics",
                     "Live vessel positions — configurable IMO list, subscribes to maritime:vessel:*", 5, 5, 3, 3,
                     [](const QJsonObject& cfg) { return new widgets::MaritimeVesselsWidget(cfg); }});

    register_widget({"notes", "Notes", "Tools",
                     "Recent / favorite financial notes — click to open Notes screen", 4, 5, 2, 3,
                     [](const QJsonObject& cfg) { return new widgets::NotesWidget(cfg); }});
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
