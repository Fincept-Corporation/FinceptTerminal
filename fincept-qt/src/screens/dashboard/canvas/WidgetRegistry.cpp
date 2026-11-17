#include "screens/dashboard/canvas/WidgetRegistry.h"

#include "core/market/MarketProfile.h"

#include "screens/dashboard/widgets/AgentErrorsWidget.h"
#include "screens/dashboard/widgets/AiQuickAskWidget.h"
#include "screens/dashboard/widgets/AutomationRunsWidget.h"
#include "screens/dashboard/widgets/BrokerHoldingsWidget.h"
#include "screens/dashboard/widgets/CommoditiesWidget.h"
#include "screens/dashboard/widgets/ConnectionHealthWidget.h"
#include "screens/dashboard/widgets/CryptoTickerWidget.h"
#include "screens/dashboard/widgets/CryptoWidget.h"
#include "screens/dashboard/widgets/DashboardCandleWidget.h"
#include "screens/dashboard/widgets/DataApiTiles.h"
#include "screens/dashboard/widgets/EconomicCalendarWidget.h"
#include "screens/dashboard/widgets/FiiDiiFlowsWidget.h"
#include "screens/dashboard/widgets/ForexWidget.h"
#include "screens/dashboard/widgets/GeopoliticsEventsWidget.h"
#include "screens/dashboard/widgets/IndicesWidget.h"
#include "screens/dashboard/widgets/KalshiPricesWidget.h"
#include "screens/dashboard/widgets/MacroSeriesWidget.h"
#include "screens/dashboard/widgets/MarginUsageWidget.h"
#include "screens/dashboard/widgets/MaritimeVesselsWidget.h"
#include "screens/dashboard/widgets/MarketGridTiles.h"
#include "screens/dashboard/widgets/MarketQuoteStripWidget.h"
#include "screens/dashboard/widgets/MarketSentimentWidget.h"
#include "screens/dashboard/widgets/NewsCategoryWidget.h"
#include "screens/dashboard/widgets/NewsWidget.h"
#include "screens/dashboard/widgets/NotesWidget.h"
#include "screens/dashboard/widgets/OpenPositionsWidget.h"
#include "screens/dashboard/widgets/OptionsSnapshotWidget.h"
#include "screens/dashboard/widgets/OrderBookMiniWidget.h"
#include "screens/dashboard/widgets/PerformanceWidget.h"
#include "screens/dashboard/widgets/PolymarketPriceWidget.h"
#include "screens/dashboard/widgets/PortfolioPerformanceWidget.h"
#include "screens/dashboard/widgets/PortfolioRiskWidget.h"
#include "screens/dashboard/widgets/PortfolioSummaryWidget.h"
#include "screens/dashboard/widgets/QuickTradeWidget.h"
#include "screens/dashboard/widgets/RecentFilesWidget.h"
#include "screens/dashboard/widgets/RiskMetricsWidget.h"
#include "screens/dashboard/widgets/ScanAlertsWidget.h"
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
#include "screens/dashboard/widgets/WorldClockWidget.h"

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
    // Quote-list tiles: rows default to the user's market region (MarketProfile)
    // and follow region changes until the user edits the list (gear icon).
    register_widget({"indices", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Market Indices"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Markets"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry",
                                       "Headline indices of your market region with day range, 5-day trend and "
                                       "session state — editable list"),
                     4, 5, 3, 4, [](const QJsonObject& cfg, QWidget* p) { return widgets::create_indices_widget(cfg, p); }});

    register_widget({"forex", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "FX Rates"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Markets"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry",
                                       "Your region's currency pairs and the dollar index — editable list"),
                     4, 4, 2, 3, [](const QJsonObject& cfg, QWidget* p) { return widgets::create_forex_widget(cfg, p); }});

    register_widget({"crypto", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Crypto Markets"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Markets"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry",
                                       "Major coins (Yahoo pairs), BTC / ETH also in your currency — editable list"),
                     4, 4, 2, 3, [](const QJsonObject& cfg, QWidget* p) { return widgets::create_crypto_widget(cfg, p); }});

    register_widget({"commodities", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Commodities"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Markets"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry",
                                       "Energy, metals and agriculture futures grouped by complex — editable list"),
                     4, 4, 2, 3, [](const QJsonObject& cfg, QWidget* p) { return widgets::create_commodities_widget(cfg, p); }});

    register_widget({"global_indices", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Global Indices"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Markets"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry",
                                       "World headline indices by region with session state and time to the next "
                                       "open / close"),
                     5, 6, 3, 4, [](const QJsonObject& cfg, QWidget* p) {
                         return new widgets::QuoteTableWidget(widgets::QuoteListKind::GlobalIndices, cfg, p);
                     }});

    register_widget({"world_clock", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "World Market Clock"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Markets"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry",
                                       "Exchange local times and today's sessions on one timeline, with holidays "
                                       "and countdowns"),
                     6, 4, 3, 3, [](const QJsonObject& cfg, QWidget* p) { return new widgets::WorldClockWidget(cfg, p); }});

    register_widget({"yield_curve", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Yield Curve"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Markets"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry",
                                       "US Treasury (today vs 1M / 1Y ago) or euro-area AAA curve, 2s10s and 3m10y "
                                       "spreads — Fincept data API, refresh on demand"),
                     5, 5, 3, 4, [](const QJsonObject& cfg, QWidget* p) { return new widgets::YieldCurveWidget(cfg, p); }});

    register_widget({"volatility", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Volatility"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Markets"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry",
                                       "VIX family live (9D, 30D, 3M, 6M, VVIX) and the VIX futures term structure"),
                     5, 6, 3, 4, [](const QJsonObject&, QWidget* p) { return new widgets::VolatilityWidget(p); }});

    register_widget({"crypto_overview", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Crypto Overview"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Markets"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry",
                                       "Total crypto market cap, 24 h change, BTC / ETH dominance and trending coins"),
                     5, 5, 3, 4, [](const QJsonObject&, QWidget* p) { return new widgets::CryptoOverviewWidget(p); }});

    register_widget({"fx_matrix", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "FX Cross Matrix"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Markets"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry",
                                       "Today's move of every major currency against every other, your base "
                                       "currency first"),
                     5, 5, 3, 4, [](const QJsonObject&, QWidget* p) { return new widgets::FxMatrixWidget(p); }});

    register_widget({"market_breadth", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Breadth (Basket)"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Markets"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry",
                                       "Advancers / decliners, above 50-day average, near 52-week highs — of a named "
                                       "basket (region large caps or sector set), not the whole exchange"),
                     4, 4, 3, 3, [](const QJsonObject& cfg, QWidget* p) { return new widgets::MarketBreadthWidget(cfg, p); }});

    register_widget({"sector_heatmap", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Sector Map"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Markets"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry",
                                       "Your region's sector set as a treemap — 1D / 1W / 1M / YTD returns"),
                     6, 5, 3, 4, [](const QJsonObject& cfg, QWidget* p) { return new widgets::SectorHeatmapWidget(cfg, p); }});

    register_widget({"top_movers", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Top Movers"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Markets"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry",
                                       "Gainers, losers and most active among your region's large caps (or your own "
                                       "list), with relative volume"),
                     6, 5, 3, 4, [](const QJsonObject& cfg, QWidget* p) { return new widgets::TopMoversWidget(cfg, p); }});

    // Same id as the old breadth-of-24-stocks "sentiment" tile, which it replaces.
    register_widget({"sentiment", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Risk-On / Risk-Off"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Markets"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry",
                                       "Documented 0–100 composite of equity trend, VIX, credit, AUD/JPY and "
                                       "copper/gold — shown only when every input is in"),
                     4, 4,
                     2, 3, [](const QJsonObject&, QWidget* p) { return new widgets::MarketSentimentWidget(p); }});

    // ── Research ──────────────────────────────────────────────────────────────
    register_widget({"news", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "News Feed"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Research"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry",
                                       "Headlines with sentiment, impact and tickers — filter by region, impact or "
                                       "your holdings"),
                     8, 4, 3, 3, [](const QJsonObject& cfg, QWidget* p) { return new widgets::NewsWidget(cfg, p); }});

    register_widget(
        {"stock_quote", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Stock Quote"),
         QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Research"),
         QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry",
                           "One instrument — price, open, ranges, relative volume, session state and a daily chart"),
         4, 5, 2, 3, [](const QJsonObject& cfg, QWidget* p) {
             // A fresh tile starts on the region's largest stock, not AAPL everywhere.
             const auto& movers = core::market::MarketProfileStore::instance().current().movers;
             const QString fallback = movers.isEmpty() ? QStringLiteral("AAPL") : movers.first();
             return new widgets::StockQuoteWidget(cfg.value("symbol").toString(fallback), p);
         }});

    register_widget({"candle_chart", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Candle Chart"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Research"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry",
                                       "OHLC candles with volume, crosshair, zoom and 1D–5Y timeframes"),
                     5, 5, 3, 4, [](const QJsonObject& cfg, QWidget* p) { return new widgets::DashboardCandleWidget(cfg, p); }});

    register_widget(
        {"screener", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Stock Screener"),
         QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Research"),
         QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry",
                           "Value, dividend, quality, growth and defensive screens over your region's large caps "
                           "(Yahoo screener)"),
         6, 5, 3, 4, [](const QJsonObject& cfg, QWidget* p) { return new widgets::ScreenerWidget(cfg, p); }});

    // 6 x 4 default / 3 min: eight columns need ~460 px; at 4 columns the
    // EVENT column collapsed to nothing (the tile also drops low-priority
    // columns when narrow — see EconomicCalendarWidget::resizeEvent).
    register_widget({"earnings_calendar", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Earnings Calendar"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Research"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry",
                                       "Earnings reports of the next 14 days — your region, all, or only your "
                                       "portfolio and watchlist symbols"),
                     6, 5, 3, 3, [](const QJsonObject& cfg, QWidget* p) { return new widgets::EarningsCalendarWidget(cfg, p); }});

    register_widget({"econ_calendar", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Economic Calendar"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Research"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry",
                                       "Upcoming macro releases in your time zone — filter by country (your region "
                                       "by default) and importance"),
                     6, 4, 3, 3, [](const QJsonObject& cfg, QWidget* p) { return new widgets::EconomicCalendarWidget(cfg, p); }});

    // ── Portfolio ─────────────────────────────────────────────────────────────
    register_widget({"watchlist", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Watchlist"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Portfolio"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry",
                                       "One of your saved watchlists (or a list kept in this tile) with live "
                                       "prices, ranges and trends"),
                     6, 4,
                     2, 3, [](const QJsonObject& cfg, QWidget* p) {
                         // The watchlist persists its binding / symbol list
                         // through the tile config; seeded in the constructor.
                         return new widgets::WatchlistWidget(cfg, p);
                     }});

    // "performance" is the portfolio's own performance (what its name always
    // promised); the index table it used to show is "market_performance".
    register_widget({"performance", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Portfolio Performance"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Portfolio"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry",
                                       "Value, today's P&L and time-weighted MTD / YTD / 1Y returns of a portfolio "
                                       "vs its benchmark"),
                     4, 5, 3, 4, [](const QJsonObject& cfg, QWidget* p) { return new widgets::PortfolioPerformanceWidget(cfg, p); }});

    register_widget({"market_performance", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Market Performance"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Markets"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry",
                                       "1D / 1W / 1M / YTD / 1Y returns of your region's indices — editable list"),
                     4, 5, 3, 3, [](const QJsonObject& cfg, QWidget* p) { return new widgets::PerformanceWidget(cfg, p); }});

    register_widget(
        {"portfolio_summary", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Portfolio Summary"),
         QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Portfolio"),
         QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry",
                           "Value, day and unrealised P&L in the portfolio's base currency, allocation donut by "
                           "sector or holding, largest positions"),
         6, 4, 2, 3, [](const QJsonObject& cfg, QWidget* p) {
             // The tile persists its chosen portfolio through config(); the
             // constructor applies it.
             return new widgets::PortfolioSummaryWidget(cfg, p);
         }});

    // "risk_metrics" was filed under Portfolio but measured the market; it now
    // measures the portfolio, and the market view is "market_risk".
    register_widget({"risk_metrics", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Portfolio Risk"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Portfolio"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry",
                                       "Ex-ante volatility, beta, VaR / expected shortfall, tracking error and each "
                                       "holding's risk share"),
                     4, 5, 3, 4, [](const QJsonObject& cfg, QWidget* p) { return new widgets::PortfolioRiskWidget(cfg, p); }});

    register_widget({"market_risk", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Market Risk"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Markets"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry",
                                       "Your region's volatility gauge and regime, realised vs implied volatility, "
                                       "drawdown from the 52-week high, VIX term ratio"),
                     4, 5, 3, 4,
                     [](const QJsonObject& cfg, QWidget* p) { return new widgets::RiskMetricsWidget(cfg, p); }});

    // ── Trading ───────────────────────────────────────────────────────────────
    register_widget({"quick_trade", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Quick Trade"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Trading"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry",
                                       "Quote lookup + order estimate — opens the Equity Trading order ticket"),
                     4, 5, 2, 3, [](const QJsonObject& cfg, QWidget* p) { return new widgets::QuickTradeWidget(cfg, p); }});

    register_widget({"open_positions", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Open Positions"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Trading"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry",
                                       "Live open positions for a broker account — pick via gear icon"),
                     6, 5, 3, 3, [](const QJsonObject& cfg, QWidget* p) { return new widgets::OpenPositionsWidget(cfg, p); }});

    register_widget({"working_orders", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Working Orders"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Trading"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry",
                                       "Pending/working orders for a broker account — click × to cancel"),
                     6, 5, 3, 3, [](const QJsonObject& cfg, QWidget* p) { return new widgets::OrderBookMiniWidget(cfg, p); }});

    register_widget({"margin_usage", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Margin Usage"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Trading"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry",
                                       "Broker account funds — available, used margin, total, usage %"),
                     3, 4, 2, 3, [](const QJsonObject& cfg, QWidget* p) { return new widgets::MarginUsageWidget(cfg, p); }});

    register_widget({"today_pnl", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Today P&L"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Trading"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry",
                                       "Aggregate broker account P&L — total, day, realized, open positions"),
                     3, 4, 2, 3, [](const QJsonObject& cfg, QWidget* p) { return new widgets::TodayPnLWidget(cfg, p); }});

    register_widget(
        {"holdings", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Holdings"),
         QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Portfolio"),
         QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Long-term broker holdings — avg cost, LTP, P&L %"), 6,
         5, 3, 3, [](const QJsonObject& cfg, QWidget* p) { return new widgets::BrokerHoldingsWidget(cfg, p); }});

    // ── Tools ────────────────────────────────────────────────────────────────
    register_widget(
        {"video_player", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Live TV / Streams"),
         QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Tools"),
         QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Financial TV — major networks and custom streams"), 4,
         5, 3, 4, [](const QJsonObject&, QWidget* p) { return new widgets::VideoPlayerWidget(p); }});

    register_widget({"recent_files", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Recent Files"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Tools"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry",
                                       "Recently saved files — exports, reports, notebooks and more"),
                     4, 4, 2, 3, [](const QJsonObject&, QWidget* p) { return new widgets::RecentFilesWidget(p); }});

    register_widget({"quote_strip", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Quote Strip"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Markets"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry",
                                       "Configurable live quote list — pick symbols via gear icon"),
                     3, 5, 2, 3, [](const QJsonObject& cfg, QWidget* p) { return new widgets::MarketQuoteStripWidget(cfg, p); }});

    register_widget({"crypto_ticker", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Crypto Ticker"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Markets"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry",
                                       "Live Kraken / HyperLiquid ticker strip — configurable pair list"),
                     3, 5, 2, 3, [](const QJsonObject& cfg, QWidget* p) { return new widgets::CryptoTickerWidget(cfg, p); }});

    register_widget({"polymarket_prices", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Polymarket"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Markets"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry",
                                       "Live prediction-market prices — configurable asset list"),
                     3, 5, 2, 3, [](const QJsonObject& cfg, QWidget* p) { return new widgets::PolymarketPriceWidget(cfg, p); }});

    register_widget({"agent_errors", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Agent Errors"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Tools"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry",
                                       "Recent agent execution failures — subscribes to agent:error:*"),
                     5, 4, 3, 3, [](const QJsonObject& cfg, QWidget* p) { return new widgets::AgentErrorsWidget(cfg, p); }});

    register_widget({"sparklines", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Sparklines"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Markets"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry",
                                       "Configurable sparkline strip — subscribes to market:sparkline:*"),
                     4, 5, 3, 3, [](const QJsonObject& cfg, QWidget* p) { return new widgets::SparklineStripWidget(cfg, p); }});

    register_widget({"trade_tape", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Trade Tape"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Markets"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry",
                                       "Live trade prints for a crypto pair — ws:<exchange>:trades:<pair>"),
                     4, 5, 3, 4, [](const QJsonObject& cfg, QWidget* p) { return new widgets::TradeTapeWidget(cfg, p); }});

    register_widget({"news_category", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "News — Category"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Research"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry",
                                       "Category-filtered news headlines — news:category:<category>"),
                     5, 5, 3, 3, [](const QJsonObject& cfg, QWidget* p) { return new widgets::NewsCategoryWidget(cfg, p); }});

    register_widget({"web_scraper", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Web Scraper"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Tools"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry",
                                       "Scrape tables from any URL — auto-detects HTML, JSON, CSV, XML/RSS"),
                     6, 5, 3, 3, [](const QJsonObject& cfg, QWidget* p) { return new widgets::WebScraperWidget(cfg, p); }});

    // ── Geopolitics ──────────────────────────────────────────────────────────
    register_widget({"geopolitics_events", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Geopolitics Events"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Geopolitics"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry",
                                       "Live conflict / political events — subscribes to geopolitics:events"),
                     6, 5, 3, 3, [](const QJsonObject& cfg, QWidget* p) { return new widgets::GeopoliticsEventsWidget(cfg, p); }});

    register_widget(
        {"maritime_vessels", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Maritime Vessels"),
         QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Geopolitics"),
         QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry",
                           "Live vessel positions — configurable IMO list, subscribes to maritime:vessel:*"),
         5, 5, 3, 3, [](const QJsonObject& cfg, QWidget* p) { return new widgets::MaritimeVesselsWidget(cfg, p); }});

    register_widget({"notes", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Notes"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Tools"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry",
                                       "Recent / favorite financial notes — click to open Notes screen"),
                     4, 5, 2, 3, [](const QJsonObject& cfg, QWidget* p) { return new widgets::NotesWidget(cfg, p); }});

    // Free text kept in the tile's own config (not the Notes screen's database).
    // Every template places one, seeded with that desk's checklist.
    register_widget({"desk_note", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Desk Note"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Tools"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry",
                                       "A note you write and edit right in the tile — templates seed it with what "
                                       "that desk watches and a daily checklist"),
                     4, 6, 3, 3, [](const QJsonObject& cfg, QWidget* p) { return new widgets::DeskNoteWidget(cfg, p); }});

    // ── Connectivity: health, alerts, automations, macro, AI ─────────────────
    register_widget(
        {"connection_health", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Connection Health"),
         QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Tools"),
         QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry",
                           "Fincept API, Python env, broker accounts and exchange WebSockets — live status"),
         4, 4, 3, 3, [](const QJsonObject& cfg, QWidget* p) { return new widgets::ConnectionHealthWidget(cfg, p); }});

    register_widget({"scan_alerts", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Scan Alerts"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Trading"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry",
                                       "Recent Algo Trading scan/alert triggers — symbol, condition, time"),
                     4, 5, 3, 3, [](const QJsonObject& cfg, QWidget* p) { return new widgets::ScanAlertsWidget(cfg, p); }});

    register_widget({"automation_runs", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Automation Runs"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Trading"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry",
                                       "Status of algo deployments and workflow runs — double-click to open"),
                     6, 5, 4, 3, [](const QJsonObject& cfg, QWidget* p) { return new widgets::AutomationRunsWidget(cfg, p); }});

    register_widget({"macro_series", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Macro Series"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Research"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry",
                                       "Economic time series (FRED or DBnomics) — line chart, latest value and "
                                       "change; set series via gear icon"),
                     4, 4, 3, 3, [](const QJsonObject& cfg, QWidget* p) { return new widgets::MacroSeriesWidget(cfg, p); }});

    register_widget(
        {"ai_quick_ask", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "AI Quick Ask"),
         QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Tools"),
         QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Ask the configured AI model a quick question"), 4, 6, 3,
         4, [](const QJsonObject& cfg, QWidget* p) { return new widgets::AiQuickAskWidget(cfg, p); }});

    register_widget(
        {"options_snapshot", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Options Snapshot"),
         QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Trading"),
         QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry",
                           "PCR, max pain and ATM IV for one underlying (nearest expiry) — needs a connected F&O "
                           "broker; set underlying via gear icon"),
         4, 3, 3, 3, [](const QJsonObject& cfg, QWidget* p) { return new widgets::OptionsSnapshotWidget(cfg, p); }});

    register_widget({"fii_dii_flows", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "FII / DII Flows"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Markets"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry",
                                       "Daily FII / DII net buy-sell in ₹ Cr (NSE cash) — chart + table, "
                                       "configurable session count"),
                     4, 5, 3, 4, [](const QJsonObject& cfg, QWidget* p) { return new widgets::FiiDiiFlowsWidget(cfg, p); }});

    register_widget({"kalshi_prices", QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Kalshi"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry", "Markets"),
                     QT_TRANSLATE_NOOP("fincept::screens::WidgetRegistry",
                                       "Kalshi prediction-market prices (YES, 24 h change) — no account needed; "
                                       "configurable ticker list"),
                     3, 5, 2, 3, [](const QJsonObject& cfg, QWidget* p) { return new widgets::KalshiPricesWidget(cfg, p); }});
}

QString WidgetRegistry::display_name_tr(const WidgetMeta& m) {
    return QCoreApplication::translate("fincept::screens::WidgetRegistry", m.display_name.toUtf8().constData());
}

QString WidgetRegistry::description_tr(const WidgetMeta& m) {
    return QCoreApplication::translate("fincept::screens::WidgetRegistry", m.description.toUtf8().constData());
}

QString WidgetRegistry::category_tr(const QString& category) {
    return QCoreApplication::translate("fincept::screens::WidgetRegistry", category.toUtf8().constData());
}

void WidgetRegistry::register_widget(WidgetMeta meta) {
    registry_.insert(meta.type_id, meta); // QMap::insert copies; std::move would be a no-op
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
