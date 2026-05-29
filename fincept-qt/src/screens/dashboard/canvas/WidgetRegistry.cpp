#include "screens/dashboard/canvas/WidgetRegistry.h"

#include "screens/dashboard/widgets/AgentErrorsWidget.h"
#include "screens/dashboard/widgets/BrokerHoldingsWidget.h"
#include "screens/dashboard/widgets/CommoditiesWidget.h"
#include "screens/dashboard/widgets/CryptoTickerWidget.h"
#include "screens/dashboard/widgets/CryptoWidget.h"
#include "screens/dashboard/widgets/DashboardCandleWidget.h"
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

// Translation marker — every display_name / description / category literal
// passed into register_widget is wrapped with QT_TRANSLATE_NOOP so lupdate
// scans it under the WidgetRegistry context. The macro expands to the
// literal at runtime; display_name_tr() / description_tr() / category_tr()
// do the runtime translate() lookup.
WidgetRegistry::WidgetRegistry() {
    // 12-column grid: default_w, default_h, min_w, min_h
    // Factory receives per-instance config (empty QJsonObject for fresh widgets).
    // Existing widgets ignore it until they opt into configurable behaviour.

    // ── Markets ───────────────────────────────────────────────────────────────
    register_widget({"indices", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Market Indices"), QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Markets"), QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Major global indices — SPY, QQQ, DIA, IWM"), 4, 5, 3, 4,
                     [](const QJsonObject&) { return widgets::create_indices_widget(); }});

    register_widget({"forex", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Forex Pairs"), QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Markets"), QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Major FX pairs — EURUSD, GBPUSD, USDJPY"), 4, 4, 2, 3,
                     [](const QJsonObject&) { return widgets::create_forex_widget(); }});

    register_widget({"crypto", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Crypto Markets"), QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Markets"), QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Top cryptocurrencies — BTC, ETH, BNB, SOL"), 4, 4, 2, 3,
                     [](const QJsonObject&) { return widgets::create_crypto_widget(); }});

    register_widget({"commodities", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Commodities"), QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Markets"), QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Gold, Oil, Natural Gas, Copper"), 4, 4, 2, 3,
                     [](const QJsonObject&) { return widgets::create_commodities_widget(); }});

    register_widget({"sector_heatmap", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Sector Heatmap"), QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Markets"), QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "S&P 500 sector performance heatmap"), 6, 5, 3, 4,
                     [](const QJsonObject&) { return new widgets::SectorHeatmapWidget; }});

    register_widget({"top_movers", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Top Movers"), QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Markets"), QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Biggest gainers and losers today"), 6, 5, 3, 4,
                     [](const QJsonObject&) { return new widgets::TopMoversWidget; }});

    register_widget({"sentiment", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Market Sentiment"), QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Markets"), QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Fear & greed, bull/bear indicators"), 4, 4, 2, 3,
                     [](const QJsonObject&) { return new widgets::MarketSentimentWidget; }});

    // ── Research ──────────────────────────────────────────────────────────────
    register_widget({"news", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "News Feed"), QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Research"), QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Latest financial news headlines"), 8, 4, 3, 3,
                     [](const QJsonObject&) { return new widgets::NewsWidget; }});

    register_widget({"stock_quote", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Stock Quote"), QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Research"), QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Single stock detail — price, volume, chart"), 4, 5, 2, 3,
                     [](const QJsonObject& cfg) {
                         const QString sym = cfg.value("symbol").toString("AAPL");
                         return new widgets::StockQuoteWidget(sym);
                     }});

    register_widget({"candle_chart", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Candle Chart"), QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Research"), QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Candlestick chart for a single ticker — set symbol via gear icon"), 5, 5, 3, 4,
                     [](const QJsonObject& cfg) { return new widgets::DashboardCandleWidget(cfg); }});

    register_widget({"screener", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Stock Screener"), QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Research"), QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Filter stocks by fundamentals and technicals"), 6, 5, 3,
                     4, [](const QJsonObject&) { return new widgets::ScreenerWidget; }});

    register_widget({"econ_calendar", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Economic Calendar"), QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Research"), QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Upcoming macro events and releases"), 4, 4, 2, 3,
                     [](const QJsonObject&) { return new widgets::EconomicCalendarWidget; }});

    // ── Portfolio ─────────────────────────────────────────────────────────────
    register_widget({"watchlist", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Watchlist"), QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Portfolio"), QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Your saved symbols with live prices"), 6, 4, 2, 3,
                     [](const QJsonObject&) { return new widgets::WatchlistWidget; }});

    register_widget({"performance", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Performance"), QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Portfolio"), QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Portfolio P&L — today, week, month, YTD"), 4, 5, 3, 4,
                     [](const QJsonObject&) { return new widgets::PerformanceWidget; }});

    register_widget({"portfolio_summary", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Portfolio Summary"), QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Portfolio"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Holdings overview with allocation breakdown"), 6, 4, 2, 3,
                     [](const QJsonObject&) { return new widgets::PortfolioSummaryWidget; }});

    register_widget({"risk_metrics", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Risk Metrics"), QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Portfolio"), QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Volatility, beta, drawdown, Sharpe ratio"), 4, 5, 3,
                     4, [](const QJsonObject&) { return new widgets::RiskMetricsWidget; }});

    // ── Trading ───────────────────────────────────────────────────────────────
    register_widget({"quick_trade", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Quick Trade"), QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Trading"), QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Fast order entry for crypto and equities"), 4, 5, 2, 3,
                     [](const QJsonObject&) { return new widgets::QuickTradeWidget; }});

    register_widget({"open_positions", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Open Positions"), QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Trading"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Live open positions for a broker account — pick via gear icon"), 6, 5, 3, 3,
                     [](const QJsonObject& cfg) { return new widgets::OpenPositionsWidget(cfg); }});

    register_widget({"working_orders", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Working Orders"), QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Trading"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Pending/working orders for a broker account — click × to cancel"), 6, 5, 3, 3,
                     [](const QJsonObject& cfg) { return new widgets::OrderBookMiniWidget(cfg); }});

    register_widget({"margin_usage", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Margin Usage"), QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Trading"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Broker account funds — available, used margin, total, usage %"), 3, 4, 2, 3,
                     [](const QJsonObject& cfg) { return new widgets::MarginUsageWidget(cfg); }});

    register_widget({"today_pnl", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Today P&L"), QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Trading"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Aggregate broker account P&L — total, day, realized, open positions"), 3, 4, 2, 3,
                     [](const QJsonObject& cfg) { return new widgets::TodayPnLWidget(cfg); }});

    register_widget({"holdings", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Holdings"), QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Portfolio"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Long-term broker holdings — avg cost, LTP, P&L %"), 6, 5, 3, 3,
                     [](const QJsonObject& cfg) { return new widgets::BrokerHoldingsWidget(cfg); }});

    // ── Tools ────────────────────────────────────────────────────────────────
    register_widget({"video_player", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Live TV / Streams"), QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Tools"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Financial TV — major networks and custom streams"), 4, 5, 3, 4,
                     [](const QJsonObject&) { return widgets::create_video_player_widget(); }});

    register_widget({"recent_files", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Recent Files"), QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Tools"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Recently saved files — exports, reports, notebooks and more"), 4, 4, 2, 3,
                     [](const QJsonObject&) { return new widgets::RecentFilesWidget; }});

    register_widget({"quote_strip", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Quote Strip"), QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Markets"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Configurable live quote list — pick symbols via gear icon"), 3, 5, 2, 3,
                     [](const QJsonObject& cfg) { return new widgets::MarketQuoteStripWidget(cfg); }});

    register_widget({"crypto_ticker", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Crypto Ticker"), QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Markets"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Live Kraken / HyperLiquid ticker strip — configurable pair list"), 3, 5, 2, 3,
                     [](const QJsonObject& cfg) { return new widgets::CryptoTickerWidget(cfg); }});

    register_widget({"polymarket_prices", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Polymarket"), QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Markets"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Live prediction-market prices — configurable asset list"), 3, 5, 2, 3,
                     [](const QJsonObject& cfg) { return new widgets::PolymarketPriceWidget(cfg); }});

    register_widget({"agent_errors", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Agent Errors"), QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Tools"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Recent agent execution failures — subscribes to agent:error:*"), 5, 4, 3, 3,
                     [](const QJsonObject& cfg) { return new widgets::AgentErrorsWidget(cfg); }});

    register_widget({"sparklines", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Sparklines"), QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Markets"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Configurable sparkline strip — subscribes to market:sparkline:*"), 4, 5, 3, 3,
                     [](const QJsonObject& cfg) { return new widgets::SparklineStripWidget(cfg); }});

    register_widget({"trade_tape", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Trade Tape"), QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Markets"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Live trade prints for a crypto pair — ws:<exchange>:trades:<pair>"), 4, 5, 3, 4,
                     [](const QJsonObject& cfg) { return new widgets::TradeTapeWidget(cfg); }});

    register_widget({"news_category", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "News — Category"), QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Research"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Category-filtered news headlines — news:category:<category>"), 5, 5, 3, 3,
                     [](const QJsonObject& cfg) { return new widgets::NewsCategoryWidget(cfg); }});

    register_widget({"web_scraper", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Web Scraper"), QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Tools"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Scrape tables from any URL — auto-detects HTML, JSON, CSV, XML/RSS"), 6, 5, 3,
                     3, [](const QJsonObject& cfg) { return new widgets::WebScraperWidget(cfg); }});

    // ── Geopolitics ──────────────────────────────────────────────────────────
    register_widget({"geopolitics_events", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Geopolitics Events"), QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Geopolitics"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Live conflict / political events — subscribes to geopolitics:events"), 6, 5, 3, 3,
                     [](const QJsonObject& cfg) { return new widgets::GeopoliticsEventsWidget(cfg); }});

    register_widget({"maritime_vessels", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Maritime Vessels"), QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Geopolitics"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Live vessel positions — configurable IMO list, subscribes to maritime:vessel:*"), 5, 5, 3, 3,
                     [](const QJsonObject& cfg) { return new widgets::MaritimeVesselsWidget(cfg); }});

    register_widget({"notes", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Notes"), QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Tools"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Recent / favorite financial notes — click to open Notes screen"), 4, 5, 2, 3,
                     [](const QJsonObject& cfg) { return new widgets::NotesWidget(cfg); }});
}

QString WidgetRegistry::display_name_tr(const WidgetMeta& m) {
    return QCoreApplication::translate("fincept::screens::WidgetRegistry",
                                       m.display_name.toUtf8().constData());
}

QString WidgetRegistry::description_tr(const WidgetMeta& m) {
    return QCoreApplication::translate("fincept::screens::WidgetRegistry",
                                       m.description.toUtf8().constData());
}

QString WidgetRegistry::category_tr(const QString& category) {
    return QCoreApplication::translate("fincept::screens::WidgetRegistry",
                                       category.toUtf8().constData());
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
