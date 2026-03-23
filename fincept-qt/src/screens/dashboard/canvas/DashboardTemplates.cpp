#include "screens/dashboard/canvas/DashboardTemplates.h"

namespace fincept::screens {

// Helper: create a GridItem with type + 12-col layout (no instance_id)
static GridItem gi(const char* id, int x, int y, int w, int h, int mw = 2, int mh = 3) {
    GridItem item;
    item.id = id;
    item.cell = {x, y, w, h, mw, mh};
    return item;
}

QVector<DashboardTemplate> all_dashboard_templates() {
    return {

        // ── Portfolio Manager (matches React default layout) ──────────────────
        {"portfolio_manager",
         "Portfolio Manager",
         "Holdings, performance, risk and watchlist",
         {
             gi("indices", 0, 0, 4, 5, 3, 4),
             gi("performance", 4, 0, 4, 5, 3, 4),
             gi("risk_metrics", 8, 0, 4, 5, 3, 4),
             gi("portfolio_summary", 0, 5, 6, 4),
             gi("watchlist", 6, 5, 6, 4),
             gi("news", 0, 9, 8, 4),
             gi("econ_calendar", 8, 9, 4, 4),
         }},

        // ── Hedge Fund ────────────────────────────────────────────────────────
        {"hedge_fund",
         "Hedge Fund",
         "Sector heatmap, screener, risk and macro",
         {
             gi("sector_heatmap", 0, 0, 6, 5, 3, 4),
             gi("screener", 6, 0, 6, 5, 3, 4),
             gi("risk_metrics", 0, 5, 4, 4),
             gi("performance", 4, 5, 4, 4),
             gi("econ_calendar", 8, 5, 4, 4),
             gi("news", 0, 9, 12, 4),
         }},

        // ── Crypto Trader ─────────────────────────────────────────────────────
        {"crypto_trader",
         "Crypto Trader",
         "Crypto prices, quick trade, sentiment and movers",
         {
             gi("crypto", 0, 0, 6, 4),
             gi("top_movers", 6, 0, 6, 5, 3, 4),
             gi("quick_trade", 0, 4, 4, 5),
             gi("watchlist", 4, 4, 8, 5),
             gi("sentiment", 0, 9, 6, 4),
             gi("news", 6, 9, 6, 4),
         }},

        // ── Equity Trader ─────────────────────────────────────────────────────
        {"equity_trader",
         "Equity Trader",
         "Indices, forex, commodities, screener and movers",
         {
             gi("indices", 0, 0, 4, 5, 3, 4),
             gi("forex", 4, 0, 4, 4),
             gi("commodities", 8, 0, 4, 4),
             gi("stock_quote", 0, 5, 4, 5),
             gi("screener", 4, 4, 8, 5, 3, 4),
             gi("top_movers", 0, 10, 6, 4),
             gi("news", 6, 9, 6, 4),
         }},

        // ── Macro Economist ───────────────────────────────────────────────────
        {"macro_economist",
         "Macro Economist",
         "Economic calendar, indices, commodities and news",
         {
             gi("econ_calendar", 0, 0, 6, 5, 3, 4),
             gi("indices", 6, 0, 6, 4),
             gi("commodities", 6, 4, 6, 4),
             gi("performance", 0, 5, 6, 4),
             gi("news", 0, 9, 8, 5),
             gi("risk_metrics", 8, 8, 4, 5),
         }},

        // ── Geopolitics Analyst ───────────────────────────────────────────────
        {"geopolitics",
         "Geopolitics Analyst",
         "News, sentiment, economic calendar and screener",
         {
             gi("news", 0, 0, 8, 5),
             gi("sentiment", 8, 0, 4, 4),
             gi("econ_calendar", 8, 4, 4, 5),
             gi("screener", 0, 5, 6, 5, 3, 4),
             gi("indices", 6, 5, 6, 4),
         }},
    };
}

} // namespace fincept::screens
