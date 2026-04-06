#include "services/workspace/WorkspaceTemplates.h"
#include <QUuid>

namespace fincept {

// ── Helper to build a GridItem ────────────────────────────────────────────────

static screens::GridItem make_item(const QString& id, int x, int y, int w, int h) {
    screens::GridItem item;
    item.id          = id;
    item.instance_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    item.cell        = {x, y, w, h, 2, 3};
    item.is_static   = false;
    return item;
}

// ── Template implementations ──────────────────────────────────────────────────

WorkspaceDef WorkspaceTemplates::blank() {
    WorkspaceDef ws;
    ws.metadata.template_id = "blank";
    ws.active_screen        = "dashboard";
    ws.open_screens         = {"dashboard"};
    // Empty dashboard layout — user starts fresh
    ws.dashboard_layout.cols  = 12;
    ws.dashboard_layout.row_h = 60;
    ws.dashboard_layout.margin= 4;
    return ws;
}

WorkspaceDef WorkspaceTemplates::equity_trader() {
    WorkspaceDef ws;
    ws.metadata.template_id = "equity_trader";
    ws.active_screen        = "equity_trading";
    ws.open_screens         = {"dashboard", "equity_trading", "watchlist", "portfolio"};

    ws.dashboard_layout.cols  = 12;
    ws.dashboard_layout.row_h = 60;
    ws.dashboard_layout.margin= 4;
    ws.dashboard_layout.items = {
        make_item("indices",   0, 0, 8, 4),
        make_item("news",      8, 0, 4, 4),
        make_item("portfolio", 0, 4, 6, 4),
        make_item("watchlist", 6, 4, 6, 4),
    };

    // Equity trading screen defaults
    WorkspaceScreenState eq;
    eq.screen_id          = "equity_trading";
    eq.state["broker_id"] = "fyers";
    eq.state["symbol"]    = "RELIANCE";
    eq.state["exchange"]  = "NSE";
    eq.state["trading_mode"] = "paper";
    ws.screen_states.append(eq);

    return ws;
}

WorkspaceDef WorkspaceTemplates::crypto_trader() {
    WorkspaceDef ws;
    ws.metadata.template_id = "crypto_trader";
    ws.active_screen        = "crypto_trading";
    ws.open_screens         = {"dashboard", "crypto_trading", "watchlist"};

    ws.dashboard_layout.cols  = 12;
    ws.dashboard_layout.row_h = 60;
    ws.dashboard_layout.margin= 4;
    ws.dashboard_layout.items = {
        make_item("crypto_prices", 0, 0, 8, 4),
        make_item("news",          8, 0, 4, 4),
        make_item("portfolio",     0, 4, 12, 4),
    };

    WorkspaceScreenState cr;
    cr.screen_id             = "crypto_trading";
    cr.state["exchange_id"]  = "kraken";
    cr.state["symbol"]       = "BTC/USDT";
    cr.state["trading_mode"] = "paper";
    ws.screen_states.append(cr);

    return ws;
}

WorkspaceDef WorkspaceTemplates::portfolio_manager() {
    WorkspaceDef ws;
    ws.metadata.template_id = "portfolio_manager";
    ws.active_screen        = "portfolio";
    ws.open_screens         = {"dashboard", "portfolio", "watchlist", "equity_research"};

    ws.dashboard_layout.cols  = 12;
    ws.dashboard_layout.row_h = 60;
    ws.dashboard_layout.margin= 4;
    ws.dashboard_layout.items = {
        make_item("portfolio",   0, 0, 8, 5),
        make_item("news",        8, 0, 4, 5),
        make_item("watchlist",   0, 5, 6, 4),
        make_item("indices",     6, 5, 6, 4),
    };

    return ws;
}

WorkspaceDef WorkspaceTemplates::research_analyst() {
    WorkspaceDef ws;
    ws.metadata.template_id = "research_analyst";
    ws.active_screen        = "equity_research";
    ws.open_screens         = {"equity_research", "news", "ma_analytics", "surface_analytics", "dashboard"};

    ws.dashboard_layout.cols  = 12;
    ws.dashboard_layout.row_h = 60;
    ws.dashboard_layout.margin= 4;
    ws.dashboard_layout.items = {
        make_item("news",      0, 0, 6, 5),
        make_item("indices",   6, 0, 6, 5),
        make_item("portfolio", 0, 5, 12, 4),
    };

    WorkspaceScreenState er;
    er.screen_id       = "equity_research";
    er.state["symbol"] = "AAPL";
    er.state["tab"]    = 0;
    ws.screen_states.append(er);

    return ws;
}

WorkspaceDef WorkspaceTemplates::macro_economist() {
    WorkspaceDef ws;
    ws.metadata.template_id = "macro_economist";
    ws.active_screen        = "economics";
    ws.open_screens         = {"economics", "markets", "gov_data", "dbnomics", "geopolitics", "dashboard"};

    ws.dashboard_layout.cols  = 12;
    ws.dashboard_layout.row_h = 60;
    ws.dashboard_layout.margin= 4;
    ws.dashboard_layout.items = {
        make_item("indices",  0, 0, 8, 4),
        make_item("news",     8, 0, 4, 4),
        make_item("calendar", 0, 4, 12, 5),
    };

    return ws;
}

// ── Public API ────────────────────────────────────────────────────────────────

WorkspaceDef WorkspaceTemplates::make(const QString& template_id) {
    if (template_id == "equity_trader")    return equity_trader();
    if (template_id == "crypto_trader")    return crypto_trader();
    if (template_id == "portfolio_manager")return portfolio_manager();
    if (template_id == "research_analyst") return research_analyst();
    if (template_id == "macro_economist")  return macro_economist();
    return blank();
}

QVector<WorkspaceSummary> WorkspaceTemplates::available() {
    QVector<WorkspaceSummary> list;
    auto add = [&](const QString& id, const QString& name, const QString& desc) {
        WorkspaceSummary s;
        s.id          = id;
        s.name        = name;
        s.description = desc;
        s.template_id = id;
        list.append(s);
    };
    add("blank",             "Blank",             "Start with an empty workspace");
    add("equity_trader",     "Equity Trader",     "Equity trading with watchlist and portfolio");
    add("crypto_trader",     "Crypto Trader",     "Crypto trading with live price feed");
    add("portfolio_manager", "Portfolio Manager", "Portfolio overview with research tools");
    add("research_analyst",  "Research Analyst",  "Equity research, news, M&A and surface analytics");
    add("macro_economist",   "Macro Economist",   "Economics, gov data, DBnomics and geopolitics");
    return list;
}

} // namespace fincept
