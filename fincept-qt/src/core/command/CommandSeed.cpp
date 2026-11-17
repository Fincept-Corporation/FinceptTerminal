// CommandSeed.cpp — see CommandSeed.h. Tables only: titles through QT_TRANSLATE_NOOP("CommandSeed", …),
// aliases / keywords in English (search words, kept in every language).
#include "core/command/CommandSeed.h"

#include <QCoreApplication>
#include <QHash>
#include <QRegularExpression>

namespace fincept::command::seed {

namespace {

QString cs_tr(const char* text) {
    return QCoreApplication::translate("CommandSeed", text);
}

QStringList cs_words(const char* list) {
    if (!list || !*list)
        return {};
    static const QRegularExpression sep(QStringLiteral("\\s*,\\s*"));
    return QString::fromUtf8(list).split(sep, Qt::SkipEmptyParts);
}

// ── Screens ────────────────────────────────────────────────────────────────────
// aliases: the old command bar's (each one still opens the same screen; tst_command_seed pins them).
struct ScreenRow {
    const char* id;
    const char* title;
    const char* aliases;
    const char* keywords;
    const char* description;
};

const ScreenRow kScreens[] = {
    {"dashboard", QT_TRANSLATE_NOOP("CommandSeed", "Dashboard"), "dash, home, overview, main", "dashboard, home, summary, widgets",
     QT_TRANSLATE_NOOP("CommandSeed", "Main overview screen")},
    {"markets", QT_TRANSLATE_NOOP("CommandSeed", "Markets"), "mkts, markets, market, live", "markets, stocks, quotes, prices, heatmap, movers",
     QT_TRANSLATE_NOOP("CommandSeed", "Live market data")},
    {"news", QT_TRANSLATE_NOOP("CommandSeed", "News"), "news, headlines, feed", "news, headlines, articles, wire",
     QT_TRANSLATE_NOOP("CommandSeed", "Financial news feed")},
    {"portfolio", QT_TRANSLATE_NOOP("CommandSeed", "Portfolio"), "port, portfolio, pf, holdings", "portfolio, holdings, positions, pnl",
     QT_TRANSLATE_NOOP("CommandSeed", "Portfolio management")},
    {"backtesting", QT_TRANSLATE_NOOP("CommandSeed", "Backtesting"), "bktest, backtest, bt", "backtest, strategy, historical, simulation",
     QT_TRANSLATE_NOOP("CommandSeed", "Strategy backtesting")},
    {"watchlist", QT_TRANSLATE_NOOP("CommandSeed", "Watchlist"), "watch, watchlist, wl", "watchlist, favorites, track",
     QT_TRANSLATE_NOOP("CommandSeed", "Manage watchlists")},
    {"crypto_trading", QT_TRANSLATE_NOOP("CommandSeed", "Crypto Trading"), "trade, trading, crypto, kraken", "trading, crypto, exchange, bitcoin",
     QT_TRANSLATE_NOOP("CommandSeed", "Cryptocurrency trading")},
    {"ai_chat", QT_TRANSLATE_NOOP("CommandSeed", "AI Chat"), "ai, chat, assistant, bot", "ai, chat, assistant, llm",
     QT_TRANSLATE_NOOP("CommandSeed", "AI assistant")},
    {"notes", QT_TRANSLATE_NOOP("CommandSeed", "Notes"), "notes, note", "notes, reports, documents, journal",
     QT_TRANSLATE_NOOP("CommandSeed", "Notes and reports")},
    {"profile", QT_TRANSLATE_NOOP("CommandSeed", "Profile"), "prof, profile, account", "profile, account, user, billing",
     QT_TRANSLATE_NOOP("CommandSeed", "User profile & account")},
    {"settings", QT_TRANSLATE_NOOP("CommandSeed", "Settings"), "settings, prefs, config", "settings, preferences, options",
     QT_TRANSLATE_NOOP("CommandSeed", "Application settings")},
    {"equity_trading", QT_TRANSLATE_NOOP("CommandSeed", "Equity Trading"), "eqtrade, stocks, equities", "equity, stocks, trading, broker, orders",
     QT_TRANSLATE_NOOP("CommandSeed", "Stock trading interface")},
    {"algo_trading", QT_TRANSLATE_NOOP("CommandSeed", "Algo Trading"), "algo, algotrading", "algo, algorithmic, automated, strategies",
     QT_TRANSLATE_NOOP("CommandSeed", "Algorithmic trading")},
    {"alpha_arena", QT_TRANSLATE_NOOP("CommandSeed", "Alpha Arena"), "alpha, arena, alphaarena", "alpha, competition, leaderboard",
     QT_TRANSLATE_NOOP("CommandSeed", "Trading competition platform")},
    {"polymarket", QT_TRANSLATE_NOOP("CommandSeed", "Prediction Markets"), "poly, polymarket, kalshi, prediction",
     "polymarket, kalshi, prediction, markets, betting", QT_TRANSLATE_NOOP("CommandSeed", "Polymarket + Kalshi prediction markets")},
    {"derivatives", QT_TRANSLATE_NOOP("CommandSeed", "Derivatives"), "deriv, derivatives, calc, calculator",
     "derivatives, pricing, calculator, bsm, options, swaps", QT_TRANSLATE_NOOP("CommandSeed", "Pricing, option chains, strategies, curves and credit")},
    {"fno", QT_TRANSLATE_NOOP("CommandSeed", "F&O"), "fno, options, chain, strategy, sensibull",
     "options, chain, strategy, payoff, oi, fno, futures", QT_TRANSLATE_NOOP("CommandSeed", "Option chain, strategy builder, OI analytics")},
    {"equity_research", QT_TRANSLATE_NOOP("CommandSeed", "Equity Research"), "rsrch, research, equity", "research, equity, fundamental, company",
     QT_TRANSLATE_NOOP("CommandSeed", "Equity research tools")},
    {"screener", QT_TRANSLATE_NOOP("CommandSeed", "Screener"), "scrn, screener, filter, scan", "screener, filter, scan",
     QT_TRANSLATE_NOOP("CommandSeed", "Stock screener")},
    {"calendars", QT_TRANSLATE_NOOP("CommandSeed", "Calendars"), "cal, calendar, calendars, earnings, ipo, splits",
     "calendar, earnings, ipo, split, economic, events", QT_TRANSLATE_NOOP("CommandSeed", "Earnings, IPO, split and economic calendars")},
    {"ma_analytics", QT_TRANSLATE_NOOP("CommandSeed", "M&A Analytics"), "ma, mna, mergers", "ma, mergers, acquisitions, deals",
     QT_TRANSLATE_NOOP("CommandSeed", "Mergers and acquisitions")},
    {"alt_investments", QT_TRANSLATE_NOOP("CommandSeed", "Alt. Investments"), "altinv, alt, alternatives", "alternative, investments, hedge",
     QT_TRANSLATE_NOOP("CommandSeed", "Alternative investment analysis")},
    {"surface_analytics", QT_TRANSLATE_NOOP("CommandSeed", "Surface Analytics"), "surface, volsurface, 3dviz, 3d",
     "surface, volatility, correlation, pca", QT_TRANSLATE_NOOP("CommandSeed", "Options vol surface & correlation")},
    {"economics", QT_TRANSLATE_NOOP("CommandSeed", "Economics"), "econ, economics, indicators", "economics, macro, indicators, gdp, inflation",
     QT_TRANSLATE_NOOP("CommandSeed", "Economic indicators")},
    {"gov_data", QT_TRANSLATE_NOOP("CommandSeed", "GOVT Data"), "govt, gov, government, treasury", "government, treasury, bonds, open data",
     QT_TRANSLATE_NOOP("CommandSeed", "Government securities & open data")},
    {"dbnomics", QT_TRANSLATE_NOOP("CommandSeed", "DBnomics"), "dbn, dbnomics, database", "dbnomics, database, economic",
     QT_TRANSLATE_NOOP("CommandSeed", "Economic database")},
    {"akshare", QT_TRANSLATE_NOOP("CommandSeed", "AKShare Data"), "aks, akshare, chinese", "akshare, chinese, china",
     QT_TRANSLATE_NOOP("CommandSeed", "Chinese financial data")},
    {"asia_markets", QT_TRANSLATE_NOOP("CommandSeed", "Asia Markets"), "asia, apac, asian", "asia, asian, apac",
     QT_TRANSLATE_NOOP("CommandSeed", "Asian market data")},
    {"geopolitics", QT_TRANSLATE_NOOP("CommandSeed", "Geopolitics"), "geo, geopolitics, politics", "geopolitics, politics, global, conflict",
     QT_TRANSLATE_NOOP("CommandSeed", "Geopolitical analysis")},
    {"maritime", QT_TRANSLATE_NOOP("CommandSeed", "Maritime"), "marine, maritime, shipping", "maritime, shipping, vessels, ports",
     QT_TRANSLATE_NOOP("CommandSeed", "Maritime intelligence")},
    {"relationship_map", QT_TRANSLATE_NOOP("CommandSeed", "Relationship Map"), "relmap, relationships, map", "relationship, map, network",
     QT_TRANSLATE_NOOP("CommandSeed", "Entity relationship mapping")},
    {"ai_quant_lab", QT_TRANSLATE_NOOP("CommandSeed", "AI Quant Lab"), "quantlab, quant, lab", "quant, quantitative, lab, factors, ml",
     QT_TRANSLATE_NOOP("CommandSeed", "Quantitative analysis lab")},
    {"quantlib", QT_TRANSLATE_NOOP("CommandSeed", "QuantLib"), "qlcore, ql, quantlib", "quantlib, math, finance, models",
     QT_TRANSLATE_NOOP("CommandSeed", "Quantitative finance suite")},
    {"agent_config", QT_TRANSLATE_NOOP("CommandSeed", "Agent Config"), "agents, agent, config", "agents, ai, configuration, agent studio",
     QT_TRANSLATE_NOOP("CommandSeed", "Configure AI agents")},
    {"agent_workbench", QT_TRANSLATE_NOOP("CommandSeed", "Agent runs (Workbench)"), "workbench, claude, codex, awb",
     "agents, claude, codex, mcp, automation, schedule",
     QT_TRANSLATE_NOOP("CommandSeed", "Run Claude Code / Codex agents on live terminal data; schedules; connect external agents")},
    {"mcp_servers", QT_TRANSLATE_NOOP("CommandSeed", "MCP Servers"), "mcp, servers", "mcp, servers, protocol, tools",
     QT_TRANSLATE_NOOP("CommandSeed", "Model Context Protocol servers")},
    {"node_editor", QT_TRANSLATE_NOOP("CommandSeed", "Node Editor"), "nodes, workflow, node", "nodes, workflow, visual, automation",
     QT_TRANSLATE_NOOP("CommandSeed", "Visual workflow editor")},
    {"code_editor", QT_TRANSLATE_NOOP("CommandSeed", "Code Editor"), "code, editor, dev", "code, editor, programming, notebook, python, jupyter",
     QT_TRANSLATE_NOOP("CommandSeed", "Code development and notebooks")},
    {"excel", QT_TRANSLATE_NOOP("CommandSeed", "Excel"), "excel, spreadsheet, xls", "excel, spreadsheet, workbook",
     QT_TRANSLATE_NOOP("CommandSeed", "Excel workbook integration")},
    {"report_builder", QT_TRANSLATE_NOOP("CommandSeed", "Report Builder"), "report, reports, builder", "report, builder, document, pdf",
     QT_TRANSLATE_NOOP("CommandSeed", "Create reports")},
    {"data_sources", QT_TRANSLATE_NOOP("CommandSeed", "Data Sources"), "datasrc, datasources, sources", "data, sources, connections, connectors",
     QT_TRANSLATE_NOOP("CommandSeed", "Manage data sources")},
    {"data_mapping", QT_TRANSLATE_NOOP("CommandSeed", "Data Mapping"), "datamap, mapping, schema", "data, mapping, schema, api",
     QT_TRANSLATE_NOOP("CommandSeed", "API integration & schema transform")},
    {"trade_viz", QT_TRANSLATE_NOOP("CommandSeed", "Trade Visualization"), "tradeviz, tradegraph", "trade, visualization, flow",
     QT_TRANSLATE_NOOP("CommandSeed", "Trade flow visualization")},
    {"crypto_center", QT_TRANSLATE_NOOP("CommandSeed", "Crypto Center"), "cc, cryptocenter, wallet", "crypto, wallet, solana, fncpt",
     QT_TRANSLATE_NOOP("CommandSeed", "Solana wallet — $FNCPT and SOL balances")},
    {"file_manager", QT_TRANSLATE_NOOP("CommandSeed", "File Manager"), "files, filemanager, fm", "files, documents, uploads, imports",
     QT_TRANSLATE_NOOP("CommandSeed", "Managed files and imports")},
    {"docs", QT_TRANSLATE_NOOP("CommandSeed", "Docs"), "docs, manual, documentation", "docs, manual, guide",
     QT_TRANSLATE_NOOP("CommandSeed", "Terminal documentation")},
    {"help", QT_TRANSLATE_NOOP("CommandSeed", "Help Center"), "helpcenter, faq", "help, faq, guide",
     QT_TRANSLATE_NOOP("CommandSeed", "Help and FAQs")},
    {"about", QT_TRANSLATE_NOOP("CommandSeed", "About"), "about, info, version", "about, information, version",
     QT_TRANSLATE_NOOP("CommandSeed", "About Fincept Terminal")},
    {"support", QT_TRANSLATE_NOOP("CommandSeed", "Support"), "support, ticket", "support, ticket, assistance, problem",
     QT_TRANSLATE_NOOP("CommandSeed", "Support tickets")},
    {"contact", QT_TRANSLATE_NOOP("CommandSeed", "Contact"), "contact", "contact, email, sales",
     QT_TRANSLATE_NOOP("CommandSeed", "Contact Fincept")},
    {"terms", QT_TRANSLATE_NOOP("CommandSeed", "Terms of Service"), "terms, tos", "terms, legal, service",
     QT_TRANSLATE_NOOP("CommandSeed", "Terms of service")},
    {"privacy", QT_TRANSLATE_NOOP("CommandSeed", "Privacy Policy"), "privacy", "privacy, policy, data",
     QT_TRANSLATE_NOOP("CommandSeed", "Privacy policy")},
    {"trademarks", QT_TRANSLATE_NOOP("CommandSeed", "Trademarks"), "trademarks", "trademarks, legal",
     QT_TRANSLATE_NOOP("CommandSeed", "Trademark notices")},
    {"chat_mode", QT_TRANSLATE_NOOP("CommandSeed", "Chat Mode"), "chatmode", "chat, conversations, runs, agent runs",
     QT_TRANSLATE_NOOP("CommandSeed", "The full-window chat with agent runs (F9)")},
};

// ── Sub-tabs / panels / modules — each screen's OWN ids (from the screens' code, 2026-11-17) ──
struct SectionRow {
    const char* screen;
    const char* section;
    const char* title;
    const char* keywords;
};

const SectionRow kSections[] = {
    // Derivatives (DerivativesScreen::Page; pricer instruments of DerivPricerPage)
    {"derivatives", "pricer", QT_TRANSLATE_NOOP("CommandSeed", "Pricer"), "price, calculator, valuation"},
    {"derivatives", "chain", QT_TRANSLATE_NOOP("CommandSeed", "Chain & Vol"), "option chain, volatility, smile"},
    {"derivatives", "strategy", QT_TRANSLATE_NOOP("CommandSeed", "Strategy"), "payoff, spread, straddle"},
    {"derivatives", "scenarios", QT_TRANSLATE_NOOP("CommandSeed", "Scenarios"), "stress, what if"},
    {"derivatives", "curves", QT_TRANSLATE_NOOP("CommandSeed", "Curves"), "yield curve, rates"},
    {"derivatives", "credit", QT_TRANSLATE_NOOP("CommandSeed", "Credit"), "cds, default"},
    {"derivatives", "book", QT_TRANSLATE_NOOP("CommandSeed", "Book"), "positions, greeks"},
    {"derivatives", "pricer:bond", QT_TRANSLATE_NOOP("CommandSeed", "Bond pricer"), "bonds, fixed income, yield"},
    {"derivatives", "pricer:equity", QT_TRANSLATE_NOOP("CommandSeed", "Equity option pricer"), "black scholes, bsm, call, put"},
    {"derivatives", "pricer:fx", QT_TRANSLATE_NOOP("CommandSeed", "FX option pricer"), "garman kohlhagen, currency option"},
    {"derivatives", "pricer:swap", QT_TRANSLATE_NOOP("CommandSeed", "Interest rate swap pricer"), "irs, swap"},
    {"derivatives", "pricer:cds", QT_TRANSLATE_NOOP("CommandSeed", "Credit default swap pricer"), "cds, credit"},
    // Report Builder
    {"report_builder", "library", QT_TRANSLATE_NOOP("CommandSeed", "Reports library"), "reports, saved reports"},
    {"report_builder", "editor", QT_TRANSLATE_NOOP("CommandSeed", "Report editor"), "edit report"},
    // Notes views
    {"notes", "board", QT_TRANSLATE_NOOP("CommandSeed", "Notes board"), "kanban, lanes"},
    {"notes", "wall", QT_TRANSLATE_NOOP("CommandSeed", "Notes wall"), "canvas, free"},
    {"notes", "list", QT_TRANSLATE_NOOP("CommandSeed", "Notes list"), "table"},
    // Chat Mode
    {"chat_mode", "conversations", QT_TRANSLATE_NOOP("CommandSeed", "Conversations"), "chat history"},
    {"chat_mode", "runs", QT_TRANSLATE_NOOP("CommandSeed", "Agent runs"), "runs, tasks"},
    // Geopolitics
    {"geopolitics", "monitor", QT_TRANSLATE_NOOP("CommandSeed", "Monitor"), "events, conflicts"},
    {"geopolitics", "hdx", QT_TRANSLATE_NOOP("CommandSeed", "HDX Data"), "humanitarian"},
    {"geopolitics", "relations", QT_TRANSLATE_NOOP("CommandSeed", "Relations"), "countries"},
    {"geopolitics", "trade", QT_TRANSLATE_NOOP("CommandSeed", "Trade"), "tariffs, trade flows"},
    // Maritime (no tabs: the right column's searches, MaritimeScreen::open_section)
    {"maritime", "vessel", QT_TRANSLATE_NOOP("CommandSeed", "Vessel search"), "imo, track vessel, ship"},
    {"maritime", "places", QT_TRANSLATE_NOOP("CommandSeed", "Shipping area search"), "place, area, region, sea"},
    {"maritime", "ports", QT_TRANSLATE_NOOP("CommandSeed", "Port search"), "ports, harbour, harbor"},
    // M&A Analytics modules
    {"ma_analytics", "valuation", QT_TRANSLATE_NOOP("CommandSeed", "Valuation Toolkit"), "dcf, valuation"},
    {"ma_analytics", "merger", QT_TRANSLATE_NOOP("CommandSeed", "Merger Analysis"), "accretion, dilution"},
    {"ma_analytics", "deals", QT_TRANSLATE_NOOP("CommandSeed", "Deal Database"), "deals, transactions"},
    {"ma_analytics", "startup", QT_TRANSLATE_NOOP("CommandSeed", "Startup Valuation"), "venture, vc"},
    {"ma_analytics", "fairness", QT_TRANSLATE_NOOP("CommandSeed", "Valuation Range Check"), "fairness opinion"},
    {"ma_analytics", "industry", QT_TRANSLATE_NOOP("CommandSeed", "Industry Metrics"), "sector, multiples"},
    {"ma_analytics", "advanced", QT_TRANSLATE_NOOP("CommandSeed", "Advanced Analytics"), "monte carlo, lbo"},
    {"ma_analytics", "comparison", QT_TRANSLATE_NOOP("CommandSeed", "Deal Comparison"), "compare deals"},
    // Alt. Investments modes
    {"alt_investments", "analyzers", QT_TRANSLATE_NOOP("CommandSeed", "Analyzers"), "hedge funds, bonds, annuities"},
    {"alt_investments", "private_markets", QT_TRANSLATE_NOOP("CommandSeed", "Private Markets"), "private equity, private credit"},
    {"alt_investments", "real_estate", QT_TRANSLATE_NOOP("CommandSeed", "Real Estate"), "reit, property"},
    {"alt_investments", "fund_analytics", QT_TRANSLATE_NOOP("CommandSeed", "Fund Analytics"), "fund"},
    {"alt_investments", "allocation", QT_TRANSLATE_NOOP("CommandSeed", "Allocation & Liquidity"), "allocation, liquidity"},
    {"alt_investments", "credit_dd", QT_TRANSLATE_NOOP("CommandSeed", "Credit & Due Diligence"), "due diligence, credit"},
    // QuantLib modules
    {"quantlib", "core", QT_TRANSLATE_NOOP("CommandSeed", "QuantLib Core"), ""},
    {"quantlib", "analysis", QT_TRANSLATE_NOOP("CommandSeed", "QuantLib Analysis"), ""},
    {"quantlib", "curves", QT_TRANSLATE_NOOP("CommandSeed", "QuantLib Curves"), "yield curve"},
    {"quantlib", "economics", QT_TRANSLATE_NOOP("CommandSeed", "QuantLib Economics"), ""},
    {"quantlib", "instruments", QT_TRANSLATE_NOOP("CommandSeed", "QuantLib Instruments"), ""},
    {"quantlib", "ml", QT_TRANSLATE_NOOP("CommandSeed", "QuantLib Machine Learning"), "ml"},
    {"quantlib", "models", QT_TRANSLATE_NOOP("CommandSeed", "QuantLib Models"), ""},
    {"quantlib", "numerical", QT_TRANSLATE_NOOP("CommandSeed", "QuantLib Numerical"), ""},
    {"quantlib", "physics", QT_TRANSLATE_NOOP("CommandSeed", "QuantLib Physics"), ""},
    {"quantlib", "portfolio", QT_TRANSLATE_NOOP("CommandSeed", "QuantLib Portfolio"), ""},
    {"quantlib", "pricing", QT_TRANSLATE_NOOP("CommandSeed", "QuantLib Pricing"), ""},
    {"quantlib", "regulatory", QT_TRANSLATE_NOOP("CommandSeed", "QuantLib Regulatory"), "basel"},
    {"quantlib", "risk", QT_TRANSLATE_NOOP("CommandSeed", "QuantLib Risk"), "var"},
    {"quantlib", "scheduling", QT_TRANSLATE_NOOP("CommandSeed", "QuantLib Scheduling"), "calendar, schedule"},
    {"quantlib", "solver", QT_TRANSLATE_NOOP("CommandSeed", "QuantLib Solver"), ""},
    {"quantlib", "statistics", QT_TRANSLATE_NOOP("CommandSeed", "QuantLib Statistics"), ""},
    {"quantlib", "stochastic", QT_TRANSLATE_NOOP("CommandSeed", "QuantLib Stochastic"), "monte carlo"},
    {"quantlib", "volatility", QT_TRANSLATE_NOOP("CommandSeed", "QuantLib Volatility"), "garch"},
    // Agent Config (Agent Studio) views
    {"agent_config", "agents", QT_TRANSLATE_NOOP("CommandSeed", "Agents"), "agent studio"},
    {"agent_config", "create", QT_TRANSLATE_NOOP("CommandSeed", "Create agent"), "new agent"},
    {"agent_config", "teams", QT_TRANSLATE_NOOP("CommandSeed", "Agent teams"), "teams"},
    {"agent_config", "workflows", QT_TRANSLATE_NOOP("CommandSeed", "Agent workflows"), "workflows"},
    {"agent_config", "planner", QT_TRANSLATE_NOOP("CommandSeed", "Agent planner"), "planner"},
    {"agent_config", "tools", QT_TRANSLATE_NOOP("CommandSeed", "Agent tools"), "tools"},
    {"agent_config", "chat", QT_TRANSLATE_NOOP("CommandSeed", "Agent chat"), "chat"},
    {"agent_config", "system", QT_TRANSLATE_NOOP("CommandSeed", "Agent system"), "system"},
    // Runs (the former Agent Workbench is part of Agent Config now)
    {"agent_config", "run", QT_TRANSLATE_NOOP("CommandSeed", "New agent run"), "run, claude, codex, workbench"},
    {"agent_config", "runs", QT_TRANSLATE_NOOP("CommandSeed", "Agent runs"), "history, workbench runs"},
    {"agent_config", "schedules", QT_TRANSLATE_NOOP("CommandSeed", "Agent schedules"), "schedule, recurring"},
    {"agent_config", "harnesses", QT_TRANSLATE_NOOP("CommandSeed", "Coding agents"), "harness, claude code, codex, gemini"},
    {"agent_config", "connect", QT_TRANSLATE_NOOP("CommandSeed", "Connect a coding agent"), "mcp, connect"},
    {"agent_workbench", "schedules", QT_TRANSLATE_NOOP("CommandSeed", "Schedules"), "scheduled runs, cron"},
    {"agent_workbench", "harnesses", QT_TRANSLATE_NOOP("CommandSeed", "Harnesses"), "harness"},
    {"agent_workbench", "connect", QT_TRANSLATE_NOOP("CommandSeed", "Connect external agents"), "connect, mcp"},
    // News
    {"news", "wire", QT_TRANSLATE_NOOP("CommandSeed", "News wire"), "every headline"},
    {"news", "clusters", QT_TRANSLATE_NOOP("CommandSeed", "News stories"), "clusters, stories"},
    // Markets views
    {"markets", "panels", QT_TRANSLATE_NOOP("CommandSeed", "Market panels"), "quotes"},
    {"markets", "heatmap", QT_TRANSLATE_NOOP("CommandSeed", "Heatmap"), "heat map, sectors"},
    {"markets", "movers", QT_TRANSLATE_NOOP("CommandSeed", "Top movers"), "gainers, losers, movers"},
    // Backtesting tabs + engines
    {"backtesting", "overview", QT_TRANSLATE_NOOP("CommandSeed", "Backtest overview"), "summary"},
    {"backtesting", "performance", QT_TRANSLATE_NOOP("CommandSeed", "Backtest performance"), "returns, equity curve"},
    {"backtesting", "risk", QT_TRANSLATE_NOOP("CommandSeed", "Backtest risk"), "drawdown"},
    {"backtesting", "trades", QT_TRANSLATE_NOOP("CommandSeed", "Backtest trades"), "trades"},
    {"backtesting", "exposure", QT_TRANSLATE_NOOP("CommandSeed", "Exposure"), "exposure"},
    {"backtesting", "costs", QT_TRANSLATE_NOOP("CommandSeed", "Costs"), "fees, slippage"},
    {"backtesting", "optimize", QT_TRANSLATE_NOOP("CommandSeed", "Optimise"), "optimize, parameters"},
    {"backtesting", "walk_forward", QT_TRANSLATE_NOOP("CommandSeed", "Walk-forward"), "walk forward"},
    {"backtesting", "monte_carlo", QT_TRANSLATE_NOOP("CommandSeed", "Monte Carlo"), "simulation"},
    {"backtesting", "compare", QT_TRANSLATE_NOOP("CommandSeed", "Compare runs"), "comparison"},
    {"backtesting", "assumptions", QT_TRANSLATE_NOOP("CommandSeed", "Assumptions"), "settings"},
    {"backtesting", "runs", QT_TRANSLATE_NOOP("CommandSeed", "Runs"), "history, results, saved runs"},
    {"backtesting", "research", QT_TRANSLATE_NOOP("CommandSeed", "Research"), "notes"},
    {"backtesting", "engine:vectorbt", QT_TRANSLATE_NOOP("CommandSeed", "VectorBT engine"), "engine"},
    {"backtesting", "engine:backtestingpy", QT_TRANSLATE_NOOP("CommandSeed", "Backtesting.py engine"), "engine"},
    {"backtesting", "engine:zipline", QT_TRANSLATE_NOOP("CommandSeed", "Zipline engine"), "engine"},
    {"backtesting", "engine:bt", QT_TRANSLATE_NOOP("CommandSeed", "BT engine"), "engine"},
    {"backtesting", "engine:fincept", QT_TRANSLATE_NOOP("CommandSeed", "Fincept engine"), "engine"},
    // Economics sources (EconomicsScreen source ids)
    {"economics", "worldbank", QT_TRANSLATE_NOOP("CommandSeed", "World Bank"), "wb"},
    {"economics", "imf", QT_TRANSLATE_NOOP("CommandSeed", "IMF"), "international monetary fund"},
    {"economics", "fred", QT_TRANSLATE_NOOP("CommandSeed", "FRED"), "federal reserve economic data"},
    {"economics", "fred_analytics", QT_TRANSLATE_NOOP("CommandSeed", "FRED Analytics"), "fred"},
    {"economics", "market_models", QT_TRANSLATE_NOOP("CommandSeed", "Market Models"), "models"},
    {"economics", "oecd", QT_TRANSLATE_NOOP("CommandSeed", "OECD"), ""},
    {"economics", "ecb", QT_TRANSLATE_NOOP("CommandSeed", "ECB"), "european central bank"},
    {"economics", "eurostat", QT_TRANSLATE_NOOP("CommandSeed", "Eurostat"), "europe"},
    {"economics", "bls", QT_TRANSLATE_NOOP("CommandSeed", "BLS"), "labor statistics, jobs, cpi"},
    {"economics", "census", QT_TRANSLATE_NOOP("CommandSeed", "US Census"), "census"},
    {"economics", "econdb", QT_TRANSLATE_NOOP("CommandSeed", "EconDB"), ""},
    {"economics", "trading_economics", QT_TRANSLATE_NOOP("CommandSeed", "Trading Economics"), ""},
    {"economics", "econ_calendar", QT_TRANSLATE_NOOP("CommandSeed", "Economic calendar"), "events, releases"},
    {"economics", "bcb", QT_TRANSLATE_NOOP("CommandSeed", "BCB Brazil"), "brazil"},
    {"economics", "akshare_cn", QT_TRANSLATE_NOOP("CommandSeed", "AkShare China"), "china"},
    {"economics", "ons", QT_TRANSLATE_NOOP("CommandSeed", "ONS UK"), "uk"},
    {"economics", "global_cb", QT_TRANSLATE_NOOP("CommandSeed", "Central Banks"), "rates, policy"},
    {"economics", "federal_reserve", QT_TRANSLATE_NOOP("CommandSeed", "Federal Reserve"), "fed"},
    {"economics", "fiscal_data", QT_TRANSLATE_NOOP("CommandSeed", "Fiscal Data"), "treasury, debt"},
    {"economics", "nber", QT_TRANSLATE_NOOP("CommandSeed", "NBER Cycles"), "recession, business cycle"},
    {"economics", "owid", QT_TRANSLATE_NOOP("CommandSeed", "Our World In Data"), "owid"},
    {"economics", "statcan", QT_TRANSLATE_NOOP("CommandSeed", "StatCan"), "canada"},
    {"economics", "ilostat", QT_TRANSLATE_NOOP("CommandSeed", "ILO"), "labour"},
    {"economics", "un_comtrade", QT_TRANSLATE_NOOP("CommandSeed", "UN Comtrade"), "trade"},
    {"economics", "wb_health", QT_TRANSLATE_NOOP("CommandSeed", "WB Health"), "health"},
    {"economics", "bis", QT_TRANSLATE_NOOP("CommandSeed", "BIS"), "bank for international settlements"},
    {"economics", "adb", QT_TRANSLATE_NOOP("CommandSeed", "ADB"), "asian development bank"},
    {"economics", "cftc", QT_TRANSLATE_NOOP("CommandSeed", "CFTC"), "commitments of traders, cot"},
    {"economics", "eia", QT_TRANSLATE_NOOP("CommandSeed", "EIA"), "energy, oil"},
    {"economics", "wto", QT_TRANSLATE_NOOP("CommandSeed", "WTO"), "trade"},
    {"economics", "unesco", QT_TRANSLATE_NOOP("CommandSeed", "UNESCO"), "education"},
    {"economics", "bea", QT_TRANSLATE_NOOP("CommandSeed", "BEA"), "gdp"},
    {"economics", "fincept", QT_TRANSLATE_NOOP("CommandSeed", "Fincept Macro"), "macro"},
    // Portfolio detail views
    {"portfolio", "sectors", QT_TRANSLATE_NOOP("CommandSeed", "Sectors"), "allocation"},
    {"portfolio", "perf_risk", QT_TRANSLATE_NOOP("CommandSeed", "Performance & Risk"), "returns, volatility"},
    {"portfolio", "optimization", QT_TRANSLATE_NOOP("CommandSeed", "Optimize portfolio"), "optimization, efficient frontier"},
    {"portfolio", "quantstats", QT_TRANSLATE_NOOP("CommandSeed", "QuantStats"), "tearsheet"},
    {"portfolio", "reports", QT_TRANSLATE_NOOP("CommandSeed", "Portfolio reports"), "pme"},
    {"portfolio", "indices", QT_TRANSLATE_NOOP("CommandSeed", "Indices"), "benchmark"},
    {"portfolio", "risk", QT_TRANSLATE_NOOP("CommandSeed", "Risk management"), "var, stress"},
    {"portfolio", "tax_lots", QT_TRANSLATE_NOOP("CommandSeed", "Tax lots"), "tax"},
    {"portfolio", "income", QT_TRANSLATE_NOOP("CommandSeed", "Income"), "dividends"},
    {"portfolio", "planning", QT_TRANSLATE_NOOP("CommandSeed", "Planning"), "goals, retirement"},
    {"portfolio", "economics", QT_TRANSLATE_NOOP("CommandSeed", "Portfolio economics"), "macro"},
    // Equity Research tabs
    {"equity_research", "overview", QT_TRANSLATE_NOOP("CommandSeed", "Company overview"), "chart, quote"},
    {"equity_research", "financials", QT_TRANSLATE_NOOP("CommandSeed", "Financials"), "income statement, balance sheet, cash flow"},
    {"equity_research", "filings", QT_TRANSLATE_NOOP("CommandSeed", "Filings"), "sec, 10-k"},
    {"equity_research", "analysis", QT_TRANSLATE_NOOP("CommandSeed", "Analysis"), "ratios"},
    {"equity_research", "technicals", QT_TRANSLATE_NOOP("CommandSeed", "Technicals"), "indicators, rsi, macd"},
    {"equity_research", "peers", QT_TRANSLATE_NOOP("CommandSeed", "Peers"), "comparables, compare"},
    {"equity_research", "news", QT_TRANSLATE_NOOP("CommandSeed", "Company news"), "headlines"},
    {"equity_research", "sentiment", QT_TRANSLATE_NOOP("CommandSeed", "Sentiment"), "social"},
    {"equity_research", "valuation", QT_TRANSLATE_NOOP("CommandSeed", "Valuation"), "dcf, fair value"},
    {"equity_research", "dividends", QT_TRANSLATE_NOOP("CommandSeed", "Dividends"), "yield"},
    {"equity_research", "earnings", QT_TRANSLATE_NOOP("CommandSeed", "Earnings"), "eps"},
    {"equity_research", "options", QT_TRANSLATE_NOOP("CommandSeed", "Options"), "option chain"},
    {"equity_research", "analyst", QT_TRANSLATE_NOOP("CommandSeed", "Analyst ratings"), "targets, recommendations"},
    {"equity_research", "ownership", QT_TRANSLATE_NOOP("CommandSeed", "Ownership"), "institutional, insiders"},
    // AI Quant Lab modules
    {"ai_quant_lab", "factor_discovery", QT_TRANSLATE_NOOP("CommandSeed", "Factor Discovery"), "factors"},
    {"ai_quant_lab", "model_library", QT_TRANSLATE_NOOP("CommandSeed", "Model Library"), "models"},
    {"ai_quant_lab", "backtesting", QT_TRANSLATE_NOOP("CommandSeed", "Quant Lab backtesting"), "backtest"},
    {"ai_quant_lab", "live_signals", QT_TRANSLATE_NOOP("CommandSeed", "Live Signals"), "signals"},
    {"ai_quant_lab", "deep_agent", QT_TRANSLATE_NOOP("CommandSeed", "Deep Agent"), "agent"},
    {"ai_quant_lab", "rl_trading", QT_TRANSLATE_NOOP("CommandSeed", "RL Trading"), "reinforcement learning"},
    {"ai_quant_lab", "online_learning", QT_TRANSLATE_NOOP("CommandSeed", "Online Learning"), ""},
    {"ai_quant_lab", "meta_learning", QT_TRANSLATE_NOOP("CommandSeed", "Meta Learning"), ""},
    {"ai_quant_lab", "hft", QT_TRANSLATE_NOOP("CommandSeed", "High Frequency Trading"), "hft"},
    {"ai_quant_lab", "rolling_retraining", QT_TRANSLATE_NOOP("CommandSeed", "Rolling Retraining"), ""},
    {"ai_quant_lab", "advanced_models", QT_TRANSLATE_NOOP("CommandSeed", "Advanced Models"), ""},
    {"ai_quant_lab", "feature_engineering", QT_TRANSLATE_NOOP("CommandSeed", "Feature Engineering"), "features"},
    {"ai_quant_lab", "portfolio_opt", QT_TRANSLATE_NOOP("CommandSeed", "Portfolio Optimization"), "optimization"},
    {"ai_quant_lab", "factor_evaluation", QT_TRANSLATE_NOOP("CommandSeed", "Factor Evaluation"), "ic, factors"},
    {"ai_quant_lab", "strategy_builder", QT_TRANSLATE_NOOP("CommandSeed", "Strategy Builder"), "strategy"},
    {"ai_quant_lab", "data_processors", QT_TRANSLATE_NOOP("CommandSeed", "Data Processors"), "data"},
    {"ai_quant_lab", "quant_reporting", QT_TRANSLATE_NOOP("CommandSeed", "Quant Reporting"), "reports"},
    {"ai_quant_lab", "cfa_quant", QT_TRANSLATE_NOOP("CommandSeed", "CFA Quant"), "cfa"},
    {"ai_quant_lab", "gs_quant", QT_TRANSLATE_NOOP("CommandSeed", "GS Quant"), "goldman"},
    {"ai_quant_lab", "statsmodels", QT_TRANSLATE_NOOP("CommandSeed", "Statsmodels"), "regression, arima"},
    {"ai_quant_lab", "functime", QT_TRANSLATE_NOOP("CommandSeed", "Functime"), "forecasting"},
    {"ai_quant_lab", "fortitudo", QT_TRANSLATE_NOOP("CommandSeed", "Fortitudo"), "entropy pooling"},
    {"ai_quant_lab", "gluonts", QT_TRANSLATE_NOOP("CommandSeed", "GluonTS"), "forecasting"},
    // Surface Analytics categories
    {"surface_analytics", "equity_deriv", QT_TRANSLATE_NOOP("CommandSeed", "Equity derivative surfaces"), "vol surface, skew"},
    {"surface_analytics", "fixed_income", QT_TRANSLATE_NOOP("CommandSeed", "Fixed income surfaces"), "yield curve"},
    {"surface_analytics", "fx", QT_TRANSLATE_NOOP("CommandSeed", "FX surfaces"), "currency volatility"},
    {"surface_analytics", "credit", QT_TRANSLATE_NOOP("CommandSeed", "Credit surfaces"), "spreads"},
    {"surface_analytics", "commodities", QT_TRANSLATE_NOOP("CommandSeed", "Commodity surfaces"), "futures curve"},
    {"surface_analytics", "risk", QT_TRANSLATE_NOOP("CommandSeed", "Risk surfaces"), "correlation, pca"},
    {"surface_analytics", "macro", QT_TRANSLATE_NOOP("CommandSeed", "Macro surfaces"), "macro"},
    // F&O tabs (FnoScreen::SubTab)
    {"fno", "chain", QT_TRANSLATE_NOOP("CommandSeed", "F&O option chain"), "chain"},
    {"fno", "builder", QT_TRANSLATE_NOOP("CommandSeed", "F&O strategy builder"), "payoff"},
    {"fno", "oi", QT_TRANSLATE_NOOP("CommandSeed", "Open interest"), "oi"},
    {"fno", "multi_straddle", QT_TRANSLATE_NOOP("CommandSeed", "Multi-straddle"), "straddle"},
    {"fno", "fii_dii", QT_TRANSLATE_NOOP("CommandSeed", "FII / DII"), "institutional flows"},
    {"fno", "screener", QT_TRANSLATE_NOOP("CommandSeed", "F&O screener"), "screener"},
    {"fno", "positions", QT_TRANSLATE_NOOP("CommandSeed", "F&O positions"), "positions"},
    // Trading screens' zones and blotter tabs (EquityTradingScreen::open_section; "ticket" is not
    // seeded: a ticket opens only through equity.open_order_ticket with a symbol)
    {"equity_trading", "watchlist", QT_TRANSLATE_NOOP("CommandSeed", "Trading watchlist"), "watchlist, symbols"},
    {"equity_trading", "chart", QT_TRANSLATE_NOOP("CommandSeed", "Trading chart"), "chart, candles, price chart"},
    {"equity_trading", "positions", QT_TRANSLATE_NOOP("CommandSeed", "Positions"), "open positions"},
    {"equity_trading", "holdings", QT_TRANSLATE_NOOP("CommandSeed", "Holdings"), "demat"},
    {"equity_trading", "orders", QT_TRANSLATE_NOOP("CommandSeed", "Orders"), "order book"},
    {"equity_trading", "fills", QT_TRANSLATE_NOOP("CommandSeed", "Fills"), "trades, executions, filled orders"},
    {"equity_trading", "funds", QT_TRANSLATE_NOOP("CommandSeed", "Funds"), "margin, cash"},
    {"equity_trading", "stats", QT_TRANSLATE_NOOP("CommandSeed", "Trading stats"), "statistics, performance, p&l"},
    {"crypto_trading", "positions", QT_TRANSLATE_NOOP("CommandSeed", "Crypto positions"), "positions"},
    {"crypto_trading", "orders", QT_TRANSLATE_NOOP("CommandSeed", "Crypto orders"), "open orders"},
    {"crypto_trading", "history", QT_TRANSLATE_NOOP("CommandSeed", "Crypto order history"), "history"},
    // Algo Trading tabs
    {"algo_trading", "builder", QT_TRANSLATE_NOOP("CommandSeed", "Strategy builder"), "build strategy"},
    {"algo_trading", "strategies", QT_TRANSLATE_NOOP("CommandSeed", "My strategies"), "strategies"},
    {"algo_trading", "scanner", QT_TRANSLATE_NOOP("CommandSeed", "Scanner"), "scan"},
    {"algo_trading", "alerts", QT_TRANSLATE_NOOP("CommandSeed", "Alerts"), "alerts"},
    {"algo_trading", "dashboard", QT_TRANSLATE_NOOP("CommandSeed", "Algo dashboard"), "deployments"},
    {"algo_trading", "universe", QT_TRANSLATE_NOOP("CommandSeed", "Universe"), "symbols"},
    // Prediction markets views
    {"polymarket", "trending", QT_TRANSLATE_NOOP("CommandSeed", "Trending markets"), "trending"},
    {"polymarket", "markets", QT_TRANSLATE_NOOP("CommandSeed", "Prediction markets list"), "markets"},
    {"polymarket", "events", QT_TRANSLATE_NOOP("CommandSeed", "Prediction events"), "events"},
    {"polymarket", "sports", QT_TRANSLATE_NOOP("CommandSeed", "Sports markets"), "sports"},
    {"polymarket", "resolved", QT_TRANSLATE_NOOP("CommandSeed", "Resolved markets"), "settled"},
    // Calendars tabs
    {"calendars", "earnings", QT_TRANSLATE_NOOP("CommandSeed", "Earnings calendar"), "earnings"},
    {"calendars", "ipos", QT_TRANSLATE_NOOP("CommandSeed", "IPO calendar"), "ipo"},
    {"calendars", "splits", QT_TRANSLATE_NOOP("CommandSeed", "Splits calendar"), "splits"},
    {"calendars", "economic", QT_TRANSLATE_NOOP("CommandSeed", "Economic calendar"), "macro events"},
    // GOVT Data providers
    {"gov_data", "us-treasury", QT_TRANSLATE_NOOP("CommandSeed", "US Treasury"), "treasury, bonds"},
    {"gov_data", "us-congress", QT_TRANSLATE_NOOP("CommandSeed", "US Congress"), "bills"},
    {"gov_data", "canada-gov", QT_TRANSLATE_NOOP("CommandSeed", "Canada Open Gov"), "canada"},
    {"gov_data", "swiss", QT_TRANSLATE_NOOP("CommandSeed", "Swiss Open Data"), "switzerland"},
    {"gov_data", "france", QT_TRANSLATE_NOOP("CommandSeed", "France Open Data"), "france"},
    {"gov_data", "hk", QT_TRANSLATE_NOOP("CommandSeed", "Hong Kong Gov"), "hong kong"},
    {"gov_data", "openafrica", QT_TRANSLATE_NOOP("CommandSeed", "openAFRICA"), "africa"},
    {"gov_data", "universal-ckan", QT_TRANSLATE_NOOP("CommandSeed", "CKAN Portals"), "ckan"},
    {"gov_data", "australia", QT_TRANSLATE_NOOP("CommandSeed", "Australia Gov"), "australia"},
    {"gov_data", "uk", QT_TRANSLATE_NOOP("CommandSeed", "UK Open Data"), "united kingdom"},
    // Profile sections
    {"profile", "overview", QT_TRANSLATE_NOOP("CommandSeed", "Account overview"), "profile"},
    {"profile", "usage", QT_TRANSLATE_NOOP("CommandSeed", "Usage"), "credits used"},
    {"profile", "security", QT_TRANSLATE_NOOP("CommandSeed", "Account security"), "password, 2fa"},
    {"profile", "billing", QT_TRANSLATE_NOOP("CommandSeed", "Billing"), "invoices, plan, subscription"},
    {"profile", "support", QT_TRANSLATE_NOOP("CommandSeed", "Account support"), "help"},
    // Data Sources lists (DataSourcesScreen::open_section; connectors come as "connector:<name>"
    // rows from the search box's Data group)
    {"data_sources", "catalog", QT_TRANSLATE_NOOP("CommandSeed", "Catalogue connectors"), "connectors, catalogue, catalog"},
    {"data_sources", "connections", QT_TRANSLATE_NOOP("CommandSeed", "Saved connections"), "connections, databases"},
};

// ── Settings: sections (SettingsScreen::open_section ids) and fields within them ──
struct SettingRow {
    const char* section;
    const char* focus; // "" = the section itself
    const char* title;
    const char* keywords;
};

const SettingRow kSettings[] = {
    {"general", "", QT_TRANSLATE_NOOP("CommandSeed", "General settings"), "language, startup, general"},
    {"general", "language", QT_TRANSLATE_NOOP("CommandSeed", "Language"), "locale, translation"},
    {"appearance", "", QT_TRANSLATE_NOOP("CommandSeed", "Appearance"), "look, display"},
    {"appearance", "theme", QT_TRANSLATE_NOOP("CommandSeed", "Theme"), "dark, light, system, colors, mode"},
    {"appearance", "font_size", QT_TRANSLATE_NOOP("CommandSeed", "Font size"), "text size, zoom, bigger, smaller"},
    {"appearance", "font_family", QT_TRANSLATE_NOOP("CommandSeed", "Font"), "typeface, font family"},
    {"appearance", "density", QT_TRANSLATE_NOOP("CommandSeed", "Density"), "compact, comfortable, spacing"},
    {"appearance", "animations", QT_TRANSLATE_NOOP("CommandSeed", "Animations"), "motion, reduce motion"},
    {"appearance", "ticker_bar", QT_TRANSLATE_NOOP("CommandSeed", "Ticker bar"), "ticker tape"},
    {"appearance", "chat_bubble", QT_TRANSLATE_NOOP("CommandSeed", "AI chat bubble"), "floating chat"},
    {"currency_format", "", QT_TRANSLATE_NOOP("CommandSeed", "Currency & number format"), "currency, decimals, thousands separator, base currency"},
    {"notifications", "", QT_TRANSLATE_NOOP("CommandSeed", "Notifications"), "alerts, email, telegram, sound"},
    {"keybindings", "", QT_TRANSLATE_NOOP("CommandSeed", "Keyboard shortcuts"), "hotkeys, keybindings, keys"},
    {"voice", "", QT_TRANSLATE_NOOP("CommandSeed", "Voice"), "speech, microphone"},
    {"logging", "", QT_TRANSLATE_NOOP("CommandSeed", "Logging"), "log level, logs, diagnostics"},
    {"developer", "", QT_TRANSLATE_NOOP("CommandSeed", "Developer"), "debug, developer options"},
    {"profiles", "", QT_TRANSLATE_NOOP("CommandSeed", "Profiles"), "user profiles, switch profile"},
    {"credentials", "", QT_TRANSLATE_NOOP("CommandSeed", "API keys & credentials"), "api key, keys, token, credentials, secrets"},
    {"security", "", QT_TRANSLATE_NOOP("CommandSeed", "Security & PIN"), "pin, lock, password, auto lock"},
    {"data_sources", "", QT_TRANSLATE_NOOP("CommandSeed", "Data source settings"), "providers, data"},
    {"llm_config", "", QT_TRANSLATE_NOOP("CommandSeed", "LLM models"), "ai model, openai, anthropic, ollama, llm, provider"},
    {"ai_agents", "", QT_TRANSLATE_NOOP("CommandSeed", "AI agents (MCP)"), "agent permissions, approvals"},
    {"mcp_servers", "", QT_TRANSLATE_NOOP("CommandSeed", "MCP server settings"), "mcp"},
    {"python_env", "", QT_TRANSLATE_NOOP("CommandSeed", "Python environment"), "python, packages, venv"},
    {"storage", "", QT_TRANSLATE_NOOP("CommandSeed", "Storage & cache"), "cache, disk, data folder, clear cache"},
    {"cloud_sync", "", QT_TRANSLATE_NOOP("CommandSeed", "Cloud sync"), "sync, backup"},
};

// ── Docs topics (DocsScreen topic ids) ──
struct DocRow {
    const char* topic;
    const char* title;
    const char* keywords;
};

const DocRow kDocs[] = {
    {"welcome", QT_TRANSLATE_NOOP("CommandSeed", "Welcome guide"), "introduction, start"},
    {"getting_started", QT_TRANSLATE_NOOP("CommandSeed", "First steps"), "getting started, beginner, tutorial, how to"},
    {"shortcuts", QT_TRANSLATE_NOOP("CommandSeed", "Keyboard shortcuts guide"), "keys, hotkeys, cheat sheet"},
    {"paper_trading", QT_TRANSLATE_NOOP("CommandSeed", "Paper trading guide"), "paper, practice, simulated"},
    {"dashboard", QT_TRANSLATE_NOOP("CommandSeed", "Guide: Dashboard"), "how to"},
    {"markets", QT_TRANSLATE_NOOP("CommandSeed", "Guide: Markets"), "how to"},
    {"news", QT_TRANSLATE_NOOP("CommandSeed", "Guide: News"), "how to"},
    {"watchlist", QT_TRANSLATE_NOOP("CommandSeed", "Guide: Watchlist"), "how to"},
    {"screener", QT_TRANSLATE_NOOP("CommandSeed", "Guide: Screener"), "how to"},
    {"crypto_trading", QT_TRANSLATE_NOOP("CommandSeed", "Guide: Crypto Trading"), "how to"},
    {"crypto_center", QT_TRANSLATE_NOOP("CommandSeed", "Guide: Crypto Center"), "how to"},
    {"equity_trading", QT_TRANSLATE_NOOP("CommandSeed", "Guide: Equity Trading"), "how to, broker"},
    {"fno", QT_TRANSLATE_NOOP("CommandSeed", "Guide: F&O"), "how to"},
    {"algo_trading", QT_TRANSLATE_NOOP("CommandSeed", "Guide: Algo Trading"), "how to"},
    {"backtesting", QT_TRANSLATE_NOOP("CommandSeed", "Guide: Backtesting"), "how to"},
    {"equity_research", QT_TRANSLATE_NOOP("CommandSeed", "Guide: Equity Research"), "how to"},
    {"surface_analytics", QT_TRANSLATE_NOOP("CommandSeed", "Guide: Surface Analytics"), "how to"},
    {"derivatives", QT_TRANSLATE_NOOP("CommandSeed", "Guide: Derivatives"), "how to"},
    {"portfolio", QT_TRANSLATE_NOOP("CommandSeed", "Guide: Portfolio"), "how to"},
    {"ma_analytics", QT_TRANSLATE_NOOP("CommandSeed", "Guide: M&A Analytics"), "how to"},
    {"ai_quant_lab", QT_TRANSLATE_NOOP("CommandSeed", "Guide: AI Quant Lab"), "how to"},
    {"quantlib", QT_TRANSLATE_NOOP("CommandSeed", "Guide: QuantLib Suite"), "how to"},
    {"ai_chat", QT_TRANSLATE_NOOP("CommandSeed", "Guide: AI Chat"), "how to"},
    {"agent_config", QT_TRANSLATE_NOOP("CommandSeed", "Guide: Agent Config"), "how to"},
    {"alpha_arena", QT_TRANSLATE_NOOP("CommandSeed", "Guide: Alpha Arena"), "how to"},
    {"agent_workbench", QT_TRANSLATE_NOOP("CommandSeed", "Guide: Agent Workbench"), "how to"},
    {"dbnomics", QT_TRANSLATE_NOOP("CommandSeed", "Guide: DBnomics"), "how to"},
    {"economics", QT_TRANSLATE_NOOP("CommandSeed", "Guide: Economics"), "how to"},
    {"akshare", QT_TRANSLATE_NOOP("CommandSeed", "Guide: AkShare Data"), "how to"},
    {"gov_data", QT_TRANSLATE_NOOP("CommandSeed", "Guide: Government Data"), "how to"},
    {"asia_markets", QT_TRANSLATE_NOOP("CommandSeed", "Guide: Asia Markets"), "how to"},
    {"trade_viz", QT_TRANSLATE_NOOP("CommandSeed", "Guide: Trade Visualization"), "how to"},
    {"data_sources", QT_TRANSLATE_NOOP("CommandSeed", "Guide: Data Sources"), "how to"},
    {"geopolitics", QT_TRANSLATE_NOOP("CommandSeed", "Guide: Geopolitics"), "how to"},
    {"maritime", QT_TRANSLATE_NOOP("CommandSeed", "Guide: Maritime"), "how to"},
    {"polymarket", QT_TRANSLATE_NOOP("CommandSeed", "Guide: Prediction Markets"), "how to"},
    {"alt_investments", QT_TRANSLATE_NOOP("CommandSeed", "Guide: Alt. Investments"), "how to"},
    {"relationship_map", QT_TRANSLATE_NOOP("CommandSeed", "Guide: Relationship Map"), "how to"},
    {"report_builder", QT_TRANSLATE_NOOP("CommandSeed", "Guide: Report Builder"), "how to"},
    {"node_editor", QT_TRANSLATE_NOOP("CommandSeed", "Guide: Node Editor"), "how to"},
    {"code_editor", QT_TRANSLATE_NOOP("CommandSeed", "Guide: Code Editor"), "how to, notebooks"},
    {"excel", QT_TRANSLATE_NOOP("CommandSeed", "Guide: Excel"), "how to"},
    {"notes", QT_TRANSLATE_NOOP("CommandSeed", "Guide: Notes"), "how to"},
    {"mcp_servers", QT_TRANSLATE_NOOP("CommandSeed", "Guide: MCP Servers"), "how to"},
    {"data_mapping", QT_TRANSLATE_NOOP("CommandSeed", "Guide: Data Mapping"), "how to"},
    {"file_manager", QT_TRANSLATE_NOOP("CommandSeed", "Guide: File Manager"), "how to"},
    {"settings", QT_TRANSLATE_NOOP("CommandSeed", "Guide: Settings"), "how to"},
    {"profile", QT_TRANSLATE_NOOP("CommandSeed", "Guide: Profile"), "how to"},
};

// ── Actions the shell runs (ToolBar::action_triggered ids) and common intents ──
struct ActionRow {
    const char* id;
    const char* title;
    const char* keywords;
    const char* subtitle;
    const char* toolbar_action; // "" = not a toolbar action
    const char* screen;         // navigation intent ("" = none)
    const char* section;
    const char* risk;
};

const ActionRow kActions[] = {
    {"shell.new_window", QT_TRANSLATE_NOOP("CommandSeed", "New window"), "open window, second monitor",
     QT_TRANSLATE_NOOP("CommandSeed", "Opens another terminal window"), "new_window", "", "", ""},
    {"shell.close_window", QT_TRANSLATE_NOOP("CommandSeed", "Close window"), "close",
     QT_TRANSLATE_NOOP("CommandSeed", "Closes this window (its layout is kept)"), "close_window", "", "", ""},
    {"shell.close_all_windows", QT_TRANSLATE_NOOP("CommandSeed", "Close all windows"), "close all",
     QT_TRANSLATE_NOOP("CommandSeed", "Closes every terminal window"), "close_all_windows", "", "", ""},
    {"shell.layout_new", QT_TRANSLATE_NOOP("CommandSeed", "New layout"), "workspace, layout",
     QT_TRANSLATE_NOOP("CommandSeed", "Starts a new saved layout"), "layout_new", "", "", ""},
    {"shell.layout_open", QT_TRANSLATE_NOOP("CommandSeed", "Open layout…"), "load layout, workspace, switch layout",
     QT_TRANSLATE_NOOP("CommandSeed", "Opens the list of saved layouts"), "layout_open", "", "", ""},
    {"shell.layout_save", QT_TRANSLATE_NOOP("CommandSeed", "Save layout"), "save workspace",
     QT_TRANSLATE_NOOP("CommandSeed", "Saves the current panels as a layout"), "layout_save", "", "", ""},
    {"shell.layout_save_as", QT_TRANSLATE_NOOP("CommandSeed", "Save layout as…"), "save workspace as",
     QT_TRANSLATE_NOOP("CommandSeed", "Saves the current panels under a new name"), "layout_save_as", "", "", ""},
    {"shell.import_layout", QT_TRANSLATE_NOOP("CommandSeed", "Import layout"), "import",
     QT_TRANSLATE_NOOP("CommandSeed", "Imports a layout file"), "import_data", "", "", ""},
    {"shell.export_layout", QT_TRANSLATE_NOOP("CommandSeed", "Export layout"), "export",
     QT_TRANSLATE_NOOP("CommandSeed", "Exports the current layout to a file"), "export_data", "", "", "file"},
    {"shell.check_updates", QT_TRANSLATE_NOOP("CommandSeed", "Check for updates"), "update, upgrade version, new version",
     QT_TRANSLATE_NOOP("CommandSeed", "Checks whether a newer version is available"), "check_updates", "", "", ""},
    {"shell.logout", QT_TRANSLATE_NOOP("CommandSeed", "Log out"), "sign out, logout, log off",
     QT_TRANSLATE_NOOP("CommandSeed", "Signs you out of Fincept"), "logout", "", "", "account"},
    {"shell.panel_watchlist", QT_TRANSLATE_NOOP("CommandSeed", "Float panel: Watchlist"), "float, pop out, panel",
     QT_TRANSLATE_NOOP("CommandSeed", "Opens Watchlist as a floating panel"), "panel_watchlist", "", "", ""},
    {"shell.panel_news", QT_TRANSLATE_NOOP("CommandSeed", "Float panel: News"), "float, pop out, panel",
     QT_TRANSLATE_NOOP("CommandSeed", "Opens News as a floating panel"), "panel_news", "", "", ""},
    {"shell.panel_markets", QT_TRANSLATE_NOOP("CommandSeed", "Float panel: Markets"), "float, pop out, panel",
     QT_TRANSLATE_NOOP("CommandSeed", "Opens Markets as a floating panel"), "panel_markets", "", "", ""},
    {"shell.panel_portfolio", QT_TRANSLATE_NOOP("CommandSeed", "Float panel: Portfolio"), "float, pop out, panel",
     QT_TRANSLATE_NOOP("CommandSeed", "Opens Portfolio as a floating panel"), "panel_portfolio", "", "", ""},
    {"shell.panel_ai_chat", QT_TRANSLATE_NOOP("CommandSeed", "Float panel: AI Chat"), "float, pop out, panel",
     QT_TRANSLATE_NOOP("CommandSeed", "Opens AI Chat as a floating panel"), "panel_ai_chat", "", "", ""},
    {"shell.perspective_trading", QT_TRANSLATE_NOOP("CommandSeed", "Workspace: Crypto trading"), "quick switch, perspective",
     QT_TRANSLATE_NOOP("CommandSeed", "Arranges the window for crypto trading"), "perspective_trading", "", "", ""},
    {"shell.perspective_equity", QT_TRANSLATE_NOOP("CommandSeed", "Workspace: Equity trading"), "quick switch, perspective",
     QT_TRANSLATE_NOOP("CommandSeed", "Arranges the window for equity trading"), "perspective_equity", "", "", ""},
    {"shell.perspective_research", QT_TRANSLATE_NOOP("CommandSeed", "Workspace: Research"), "quick switch, perspective",
     QT_TRANSLATE_NOOP("CommandSeed", "Arranges the window for equity research"), "perspective_research", "", "", ""},
    {"shell.perspective_portfolio", QT_TRANSLATE_NOOP("CommandSeed", "Workspace: Portfolio"), "quick switch, perspective",
     QT_TRANSLATE_NOOP("CommandSeed", "Arranges the window around your portfolio"), "perspective_portfolio", "", "", ""},
    {"shell.perspective_markets", QT_TRANSLATE_NOOP("CommandSeed", "Workspace: Markets"), "quick switch, perspective",
     QT_TRANSLATE_NOOP("CommandSeed", "Arranges the window around live markets"), "perspective_markets", "", "", ""},
    {"shell.perspective_economics", QT_TRANSLATE_NOOP("CommandSeed", "Workspace: Economics"), "quick switch, perspective, macro",
     QT_TRANSLATE_NOOP("CommandSeed", "Arranges the window for macro research"), "perspective_economics", "", "", ""},
    {"shell.perspective_quant", QT_TRANSLATE_NOOP("CommandSeed", "Workspace: Quant"), "quick switch, perspective",
     QT_TRANSLATE_NOOP("CommandSeed", "Arranges the window for quant work"), "perspective_quant", "", "", ""},
    // Theme (applied at once and saved like Settings > Appearance)
    {"theme.light", QT_TRANSLATE_NOOP("CommandSeed", "Switch to light theme"), "theme light, light mode, day, white",
     QT_TRANSLATE_NOOP("CommandSeed", "Applies the Light theme now and keeps it"), "", "", "", ""},
    {"theme.dark", QT_TRANSLATE_NOOP("CommandSeed", "Switch to dark theme"), "theme dark, dark mode, night, black",
     QT_TRANSLATE_NOOP("CommandSeed", "Applies the Dark theme now and keeps it"), "", "", "", ""},
    {"theme.system", QT_TRANSLATE_NOOP("CommandSeed", "Use the system theme"), "theme system, auto theme, follow os",
     QT_TRANSLATE_NOOP("CommandSeed", "Follows your computer's light / dark setting"), "", "", "", ""},
    // Common intents (open the screen that does it; the screen keeps its own confirmations)
    // Equity Trading has no "accounts" section: these open the screen only.
    {"intent.connect_broker", QT_TRANSLATE_NOOP("CommandSeed", "Connect a broker"), "broker, zerodha, account, login broker, connect",
     QT_TRANSLATE_NOOP("CommandSeed", "Opens Equity Trading — connect or choose the account there"), "", "equity_trading", "", ""},
    {"intent.paper", QT_TRANSLATE_NOOP("CommandSeed", "Switch to paper trading"), "paper, practice, simulated, demo account",
     QT_TRANSLATE_NOOP("CommandSeed", "Opens Equity Trading — choose the PAPER account there"), "", "equity_trading", "", ""},
    {"intent.new_notebook", QT_TRANSLATE_NOOP("CommandSeed", "New notebook"), "python, jupyter, notebook, code",
     QT_TRANSLATE_NOOP("CommandSeed", "Opens the Code Editor with a new notebook"), "", "code_editor", "new", ""},
    {"intent.add_widget", QT_TRANSLATE_NOOP("CommandSeed", "Add dashboard widget"), "widget, tile, dashboard",
     QT_TRANSLATE_NOOP("CommandSeed", "Opens the Dashboard widget picker"), "", "dashboard", "add_widget", ""},
    {"intent.new_note", QT_TRANSLATE_NOOP("CommandSeed", "New note"), "write, journal, note",
     QT_TRANSLATE_NOOP("CommandSeed", "Opens Notes with a new note"), "", "notes", "new", ""},
    {"intent.new_report", QT_TRANSLATE_NOOP("CommandSeed", "New report"), "report, document, write",
     QT_TRANSLATE_NOOP("CommandSeed", "Starts a new, empty report in the Report Builder"), "", "report_builder", "new", ""},
    {"intent.new_mapping", QT_TRANSLATE_NOOP("CommandSeed", "New data mapping"), "mapping, schema, map fields",
     QT_TRANSLATE_NOOP("CommandSeed", "Opens Data Mapping with a new mapping"), "", "data_mapping", "new", ""},
    {"intent.new_chat", QT_TRANSLATE_NOOP("CommandSeed", "New AI chat"), "chat, ask ai, conversation",
     QT_TRANSLATE_NOOP("CommandSeed", "Opens AI Chat on a new conversation"), "", "ai_chat", "new", ""},
    {"intent.new_competition", QT_TRANSLATE_NOOP("CommandSeed", "New Alpha Arena competition"), "arena, competition, models",
     QT_TRANSLATE_NOOP("CommandSeed", "Opens the Alpha Arena wizard; nothing runs until you start it there"), "", "alpha_arena",
     "new", "spend"},
    {"intent.export_report", QT_TRANSLATE_NOOP("CommandSeed", "Export report"), "pdf, docx, export, download report",
     QT_TRANSLATE_NOOP("CommandSeed", "Opens your reports to export one"), "", "report_builder", "library", ""},
    {"intent.new_alert", QT_TRANSLATE_NOOP("CommandSeed", "New price alert"), "alert, notify, price alert",
     QT_TRANSLATE_NOOP("CommandSeed", "Opens Watchlist where alerts are set"), "", "watchlist", "alerts", ""},
    {"intent.new_watchlist", QT_TRANSLATE_NOOP("CommandSeed", "New watchlist"), "create watchlist, list",
     QT_TRANSLATE_NOOP("CommandSeed", "Opens Watchlist to create a list"), "", "watchlist", "new", ""},
    {"intent.new_portfolio", QT_TRANSLATE_NOOP("CommandSeed", "New portfolio"), "create portfolio",
     QT_TRANSLATE_NOOP("CommandSeed", "Opens Portfolio to create one"), "", "portfolio", "new", ""},
    {"intent.new_strategy", QT_TRANSLATE_NOOP("CommandSeed", "New algo strategy"), "strategy, algo, build",
     QT_TRANSLATE_NOOP("CommandSeed", "Opens the Algo Trading strategy builder"), "", "algo_trading", "builder", ""},
    {"intent.run_backtest", QT_TRANSLATE_NOOP("CommandSeed", "Run a backtest"), "backtest, test strategy, simulate",
     QT_TRANSLATE_NOOP("CommandSeed", "Opens Backtesting to set up and run a test"), "", "backtesting", "", ""},
    {"intent.report_problem", QT_TRANSLATE_NOOP("CommandSeed", "Report a problem"), "bug, issue, feedback, support",
     QT_TRANSLATE_NOOP("CommandSeed", "Opens Support to send a report"), "", "support", "new", ""},
};

CommandItem cs_item(const QString& id, const QString& title, const QStringList& keywords, const char* group) {
    CommandItem it;
    it.id = id;
    it.title = title;
    it.keywords = keywords;
    it.group = QString::fromLatin1(group);
    it.owner = QString::fromLatin1(kOwner);
    return it;
}

} // namespace

QList<ScreenInfo> screens() {
    QList<ScreenInfo> out;
    for (const ScreenRow& r : kScreens)
        out.append({QString::fromLatin1(r.id), cs_tr(r.title), cs_words(r.aliases), cs_words(r.keywords), cs_tr(r.description)});
    return out;
}

QString screen_title(const QString& id) {
    for (const ScreenRow& r : kScreens)
        if (id == QLatin1String(r.id))
            return cs_tr(r.title);
    return {};
}

QList<CommandItem> screen_items() {
    QList<CommandItem> out;
    for (const ScreenRow& r : kScreens) {
        CommandItem it = cs_item(QStringLiteral("screen:") + QString::fromLatin1(r.id), cs_tr(r.title), cs_words(r.keywords),
                                 groups::kScreen);
        it.aliases = cs_words(r.aliases);
        it.screen_id = QString::fromLatin1(r.id);
        it.subtitle = cs_tr(r.description);
        it.priority = 5; // a screen beats a same-tier sub-tab / setting of the same name
        out.append(it);
    }
    return out;
}

QList<CommandItem> section_items() {
    QList<CommandItem> out;
    for (const SectionRow& r : kSections) {
        const QString screen = QString::fromLatin1(r.screen);
        const QString section = QString::fromLatin1(r.section);
        QString id_part = section;
        id_part.replace(QLatin1Char(':'), QLatin1Char('.'));
        CommandItem it = cs_item(screen + QLatin1Char('.') + id_part, cs_tr(r.title), cs_words(r.keywords), groups::kScreen);
        it.screen_id = screen;
        it.section = section;
        it.context = screen_title(screen);
        out.append(it);
    }
    return out;
}

QList<CommandItem> setting_items() {
    QList<CommandItem> out;
    for (const SettingRow& r : kSettings) {
        const QString section = QString::fromLatin1(r.section);
        const QString focus = QString::fromLatin1(r.focus);
        CommandItem it = cs_item(QStringLiteral("settings.") + section + (focus.isEmpty() ? QString() : QLatin1Char('.') + focus),
                                 cs_tr(r.title), cs_words(r.keywords), groups::kSetting);
        it.keywords.append(QStringLiteral("settings"));
        it.screen_id = QStringLiteral("settings");
        it.section = section;
        it.focus = focus;
        it.context = cs_tr(QT_TRANSLATE_NOOP("CommandSeed", "Settings"));
        out.append(it);
    }
    return out;
}

QList<CommandItem> help_items() {
    QList<CommandItem> out;
    for (const DocRow& r : kDocs) {
        CommandItem it = cs_item(QStringLiteral("docs.") + QString::fromLatin1(r.topic), cs_tr(r.title), cs_words(r.keywords),
                                 groups::kHelp);
        it.keywords.append(QStringLiteral("docs"));
        it.keywords.append(QStringLiteral("help"));
        it.screen_id = QStringLiteral("docs");
        it.section = QString::fromLatin1(r.topic);
        it.context = cs_tr(QT_TRANSLATE_NOOP("CommandSeed", "Docs"));
        out.append(it);
    }
    CommandItem tour = cs_item(QStringLiteral("help.tour"), cs_tr(QT_TRANSLATE_NOOP("CommandSeed", "Replay the welcome tour")),
                               cs_words("tour, onboarding, introduction, how to start"), groups::kHelp);
    tour.args.insert(QStringLiteral("action_id"), QStringLiteral("help.replay_tour"));
    tour.subtitle = cs_tr(QT_TRANSLATE_NOOP("CommandSeed", "Shows the short tour of the terminal again"));
    out.append(tour);
    CommandItem faq = cs_item(QStringLiteral("help.center"), cs_tr(QT_TRANSLATE_NOOP("CommandSeed", "Help Center & FAQ")),
                              cs_words("faq, questions, help"), groups::kHelp);
    faq.screen_id = QStringLiteral("help");
    out.append(faq);
    return out;
}

QList<CommandItem> action_items() {
    QList<CommandItem> out;
    for (const ActionRow& r : kActions) {
        CommandItem it = cs_item(QString::fromLatin1(r.id), cs_tr(r.title), cs_words(r.keywords), groups::kAction);
        it.subtitle = cs_tr(r.subtitle);
        if (*r.toolbar_action)
            it.args.insert(QStringLiteral("toolbar_action"), QString::fromLatin1(r.toolbar_action));
        it.screen_id = QString::fromLatin1(r.screen);
        it.section = QString::fromLatin1(r.section);
        it.agent_risk = QString::fromLatin1(r.risk);
        out.append(it);
    }
    // Theme rows carry the choice.
    for (CommandItem& it : out) {
        if (it.id == QLatin1String("theme.light"))
            it.args.insert(QStringLiteral("theme"), QStringLiteral("Light"));
        else if (it.id == QLatin1String("theme.dark"))
            it.args.insert(QStringLiteral("theme"), QStringLiteral("Dark"));
        else if (it.id == QLatin1String("theme.system"))
            it.args.insert(QStringLiteral("theme"), QStringLiteral("System"));
    }
    return out;
}

QList<CommandItem> all() {
    QList<CommandItem> out = screen_items();
    out += section_items();
    out += setting_items();
    out += help_items();
    out += action_items();
    return out;
}

} // namespace fincept::command::seed
