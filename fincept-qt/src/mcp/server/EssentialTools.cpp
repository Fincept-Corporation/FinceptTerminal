// EssentialTools.cpp — see EssentialTools.h.
//
// Curated against the tool registrations in src/mcp/tools/** (names as passed to
// ToolDef::name / the make_* helpers / the data-connector manifest). Every
// research entry below is registered WITHOUT is_destructive; McpHttpServer
// re-checks that at runtime anyway and silently skips anything renamed, disabled
// or flagged destructive later, so a stale entry can never widen what a client
// is offered. Keep the list short — it is the whole tool block an external
// agent pays for on every turn; everything else is one tool_list away.

#include "mcp/server/EssentialTools.h"

namespace fincept::mcp {

QStringList essential_meta_tool_names() {
    static const QStringList kMeta = {
        QStringLiteral("tool_list"),     // BM25 search over the ~900-tool catalogue
        QStringLiteral("tool_describe"), // full schema for one tool
        QStringLiteral("tool_invoke"),   // generic dispatcher for anything found via tool_list
        QStringLiteral("job_status"),    // long-running tool protocol
        QStringLiteral("job_result"),
        QStringLiteral("job_cancel"),
        QStringLiteral("job_list"),
        QStringLiteral("result_fetch"),  // page through results parked by the size cap
    };
    return kMeta;
}

QStringList essential_tool_names() {
    static const QStringList kAll = [] {
        QStringList names = essential_meta_tool_names();
        names += {
            // ── Markets & company research (yfinance-backed, no API key) ─────
            QStringLiteral("get_quote"),
            QStringLiteral("get_history"),
            QStringLiteral("lookup_symbol"),
            QStringLiteral("load_equity_symbol"),
            QStringLiteral("get_equity_info"),
            QStringLiteral("get_equity_financials"),
            QStringLiteral("get_equity_technicals"),
            QStringLiteral("get_equity_peers"),

            // ── News ──────────────────────────────────────────────────────────
            QStringLiteral("get_news"),
            QStringLiteral("get_top_news"),
            QStringLiteral("search_news"),
            QStringLiteral("get_equity_news"),
            QStringLiteral("get_news_summary"),
            QStringLiteral("get_threat_alerts"),

            // ── SEC EDGAR ─────────────────────────────────────────────────────
            QStringLiteral("edgar_find_company"),
            QStringLiteral("edgar_search_filings"),
            QStringLiteral("edgar_get_financials"),
            QStringLiteral("edgar_get_financial_metrics"),
            QStringLiteral("edgar_10k_sections"),
            QStringLiteral("edgar_10k_search"),
            QStringLiteral("edgar_8k_events"),
            QStringLiteral("edgar_insider_summary"),
            QStringLiteral("edgar_13f_top_holdings"),

            // ── Macro & markets data (keyless connectors) ────────────────────
            QStringLiteral("search_dbnomics"),
            QStringLiteral("list_dbnomics_series"),
            QStringLiteral("get_dbnomics_observations"),
            QStringLiteral("data_imf"),    // IMF economic indicators, direction of trade
            QStringLiteral("data_ecb"),    // ECB yield curve, FX reference rates
            QStringLiteral("data_multpl"), // S&P 500 valuation series (Shiller PE, yields)
            QStringLiteral("data_cboe"),   // CBOE options chains, index snapshots, futures curve

            // ── The user's portfolios & watchlists (reads) ──────────────────
            QStringLiteral("list_portfolios"),
            QStringLiteral("get_portfolio"),
            QStringLiteral("get_portfolio_assets"),
            QStringLiteral("get_portfolio_snapshots"),
            QStringLiteral("get_transactions"),
            QStringLiteral("get_holdings"),
            QStringLiteral("get_watchlists"),

            // ── Paper trading (reads) ─────────────────────────────────────────
            QStringLiteral("pt_list_portfolios"),
            QStringLiteral("pt_get_positions"),
            QStringLiteral("pt_get_orders"),
            QStringLiteral("pt_get_stats"),

            // ── Geopolitics & the user's research workspace (reads) ──────────
            QStringLiteral("fetch_geopolitics_events"),
            QStringLiteral("get_notes"),
            QStringLiteral("get_note"),
            QStringLiteral("report_get_state"),
        };
        return names;
    }();
    return kAll;
}

} // namespace fincept::mcp
