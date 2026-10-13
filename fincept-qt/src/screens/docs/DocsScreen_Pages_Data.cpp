// src/screens/docs/DocsScreen_Pages_Data.cpp
//
// Data-source, geopolitics, and tools/settings docs pages: DBnomics,
// Economics, AkShare, Gov Data, Geopolitics, Maritime, Polymarket,
// Alt Investments, Report Builder, Node Editor, Code Editor, Excel, Notes,
// MCP Servers, Data Mapping, Settings, Profile.
//
// Part of the partial-class split of DocsScreen.cpp.

#include "screens/docs/DocsScreen.h"
#include "screens/docs/DocsScreen_internal.h"
#include "ui/theme/Theme.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

using fincept::screens::docs_internal::SCROLL_SS;

QWidget* DocsScreen::page_dbnomics() {
    return make_page(
        tr("DBNOMICS"), tr("Access 100+ data providers with 500K+ economic datasets"),
        {
            {tr("OVERVIEW"), tr("DBnomics provides access to the world's largest open economic database. Browse "
                                "100+ data providers (IMF, World Bank, OECD, ECB, Fed, etc.) with 500K+ datasets "
                                "covering macroeconomics, finance, demographics, and trade.")},
            {tr("KEY FEATURES"), tr("■  Provider browser — 100+ statistical agencies and central banks\n"
                                    "■  Dataset search — Full-text search across 500K+ datasets\n"
                                    "■  Series explorer — Browse individual time series with metadata\n"
                                    "■  Single view — Detailed chart + data table for one series\n"
                                    "■  Comparison view — 2x2 chart grid comparing multiple series\n"
                                    "■  CSV export — Download any series for external analysis\n"
                                    "■  Loading animations and progressive data loading")},
            {tr("REAL-WORLD USAGE"), tr("■  Macro research: GDP growth, inflation, employment data by country\n"
                                        "■  Central bank analysis: Interest rates, money supply, balance sheets\n"
                                        "■  Trade analysis: Import/export data, trade balances, tariffs\n"
                                        "■  Demographics: Population, labor force, migration data")},
            {tr("SKILL LEVELS"), tr("BEGINNER: Browse providers, look up US GDP or inflation data\n"
                                    "INTERMEDIATE: Compare series across countries, export data for analysis\n"
                                    "ADVANCED: Build macro dashboards, track leading indicators\n"
                                    "PRO: Integrate DBnomics data into quantitative macro trading models")},
        });
}

QWidget* DocsScreen::page_economics() {
    return make_page(
        tr("ECONOMICS"), tr("Macroeconomic data and analysis tools"),
        {
            {tr("OVERVIEW"), tr("The Economics screen provides comprehensive macroeconomic data visualization "
                                "and analysis tools for tracking economic indicators, central bank policies, "
                                "and global economic trends.")},
            {tr("KEY FEATURES"), tr("■  Economic indicators dashboard — GDP, CPI, unemployment, PMI\n"
                                    "■  Central bank tracker — Fed, ECB, BOJ, BOE rate decisions\n"
                                    "■  Economic calendar — Upcoming releases with forecasts\n"
                                    "■  Historical comparisons — Indicator trends over time\n"
                                    "■  Country comparison — Side-by-side economic profiles")},
            {tr("REAL-WORLD USAGE"), tr("■  Macro trading: Position ahead of rate decisions\n"
                                        "■  Risk management: Monitor recession indicators\n"
                                        "■  FX trading: Track rate differentials for carry trades\n"
                                        "■  Asset allocation: Shift sectors based on economic cycle")},
            {tr("SKILL LEVELS"), tr("BEGINNER: Check upcoming economic events, understand key indicators\n"
                                    "INTERMEDIATE: Track leading vs lagging indicators, spot economic trends\n"
                                    "ADVANCED: Build economic models, forecast indicator releases\n"
                                    "PRO: Multi-factor macro models, economic surprise index trading")},
        });
}

QWidget* DocsScreen::page_akshare() {
    return make_page(tr("AKSHARE DATA"), tr("Chinese and Asian market data via AkShare"),
                     {
                         {tr("OVERVIEW"),
                          tr("AkShare provides access to Chinese financial market data including A-shares, "
                             "Hong Kong stocks, Chinese futures, and macro data from Chinese statistical agencies.")},
                         {tr("KEY FEATURES"), tr("■  A-Share market data — SSE, SZSE listed companies\n"
                                                 "■  Hong Kong market data — HKEX listings\n"
                                                 "■  Chinese futures — commodity and financial futures\n"
                                                 "■  Macro data — NBS statistics, PBOC data\n"
                                                 "■  Fund data — Mutual funds, ETFs listed in China")},
                         {tr("REAL-WORLD USAGE"), tr("■  China market exposure: Research A-share opportunities\n"
                                                     "■  Emerging market analysis: Compare Chinese vs global markets\n"
                                                     "■  Commodity trading: Track Chinese commodity demand\n"
                                                     "■  Macro research: Monitor Chinese economic indicators")},
                         {tr("SKILL LEVELS"), tr("BEGINNER: Browse Chinese market indices and major stocks\n"
                                                 "INTERMEDIATE: Compare A-share sectors, track PBOC policy\n"
                                                 "ADVANCED: Chinese commodity-equity correlation analysis\n"
                                                 "PRO: Cross-market arbitrage between A-shares, H-shares, and ADRs")},
                     });
}

QWidget* DocsScreen::page_gov_data() {
    return make_page(
        tr("GOVERNMENT DATA"), tr("Official government statistical data"),
        {
            {tr("OVERVIEW"), tr("Access official government data from statistical agencies worldwide including "
                                "census data, economic surveys, trade statistics, and regulatory filings.")},
            {tr("KEY FEATURES"), tr("■  Multi-country government data sources\n"
                                    "■  Census and demographic data\n"
                                    "■  Trade and commerce statistics\n"
                                    "■  Regulatory filings and disclosures\n"
                                    "■  Historical data series with long time horizons")},
            {tr("REAL-WORLD USAGE"), tr("■  Research: Access primary source economic data\n"
                                        "■  Policy analysis: Track government spending and fiscal policy\n"
                                        "■  Demographics: Population and labor force trends\n"
                                        "■  Trade analysis: Import/export flows by country and sector")},
            {tr("SKILL LEVELS"), tr("BEGINNER: Browse available datasets, download basic reports\n"
                                    "INTERMEDIATE: Cross-reference government data with market data\n"
                                    "ADVANCED: Build predictive models using government leading indicators\n"
                                    "PRO: Automated policy tracking, fiscal surprise detection")},
        });
}

// ============================================================================
// Geopolitics & Alt
// ============================================================================

QWidget* DocsScreen::page_geopolitics() {
    return make_page(
        tr("GEOPOLITICS"), tr("Conflict monitoring, HDX data, trade analysis, and relationship mapping"),
        {
            {tr("OVERVIEW"), tr("The Geopolitics screen provides a comprehensive view of global geopolitical events "
                                "with conflict monitoring, humanitarian data (HDX), trade flow analysis, and entity "
                                "relationship mapping for understanding geopolitical dynamics.")},
            {tr("KEY FEATURES"), tr("■  Conflict monitor — Active global conflicts with severity tracking\n"
                                    "■  HDX integration — Humanitarian Data Exchange for crisis data\n"
                                    "■  Trade analysis — Sanctions, tariffs, trade flow disruptions\n"
                                    "■  Relationship mapping — Entity relationships and influence networks\n"
                                    "■  Risk scoring — Country-level geopolitical risk assessments\n"
                                    "■  AI agent — Geopolitics-specialized analysis agent")},
            {tr("REAL-WORLD USAGE"), tr("■  Risk management: Monitor geopolitical risks to your portfolio\n"
                                        "■  Commodity trading: Track supply disruptions from conflicts\n"
                                        "■  FX trading: Geopolitical risk premium in currency pricing\n"
                                        "■  ESG investing: Humanitarian impact assessment")},
            {tr("SKILL LEVELS"), tr("BEGINNER: Monitor the conflict dashboard, understand risk scores\n"
                                    "INTERMEDIATE: Correlate geopolitical events with market movements\n"
                                    "ADVANCED: Build geopolitical risk models, trade around events\n"
                                    "PRO: Multi-factor geopolitical alpha, automated event-driven trading")},
        });
}

QWidget* DocsScreen::page_maritime() {
    return make_page(
        tr("MARITIME"), tr("Maritime shipping and trade flow data"),
        {
            {tr("OVERVIEW"), tr("The Maritime screen provides shipping and trade flow data for monitoring global "
                                "supply chains, commodity transport, and maritime trade patterns.")},
            {tr("KEY FEATURES"), tr("■  Vessel tracking — AIS data for major shipping routes\n"
                                    "■  Port analytics — Throughput, congestion, wait times\n"
                                    "■  Trade flows — Commodity shipping volumes by route\n"
                                    "■  Supply chain indicators — Shipping cost indices\n"
                                    "■  Geopolitical overlay — Chokepoint risk assessment")},
            {tr("REAL-WORLD USAGE"), tr("■  Commodity trading: Track oil tanker movements for supply signals\n"
                                        "■  Supply chain: Monitor port congestion for inflation signals\n"
                                        "■  Shipping stocks: Analyze Baltic Dry Index and freight rates\n"
                                        "■  Geopolitical risk: Chokepoint monitoring (Suez, Strait of Hormuz)")},
            {tr("SKILL LEVELS"), tr("BEGINNER: Browse shipping routes, understand trade flow basics\n"
                                    "INTERMEDIATE: Track commodity shipments, correlate with futures prices\n"
                                    "ADVANCED: Build supply chain disruption models\n"
                                    "PRO: Satellite-verified shipping data for alternative data alpha")},
        });
}

QWidget* DocsScreen::page_polymarket() {
    return make_page(
        tr("POLYMARKET"), tr("Prediction markets and event probability trading"),
        {
            {tr("OVERVIEW"), tr("The Polymarket screen provides access to prediction market data, allowing you to "
                                "track event probabilities and trade on real-world outcomes.")},
            {tr("KEY FEATURES"), tr("■  Event browser — Political, economic, sports, crypto events\n"
                                    "■  Probability charts — Historical probability movement\n"
                                    "■  Volume and liquidity data\n"
                                    "■  Category filtering and search\n"
                                    "■  Correlation with traditional markets")},
            {tr("REAL-WORLD USAGE"), tr("■  Election trading: Track political event probabilities\n"
                                        "■  Hedging: Use prediction markets to hedge event risk\n"
                                        "■  Sentiment gauge: Market-implied probabilities as sentiment\n"
                                        "■  Research: Compare prediction market accuracy with polls")},
            {tr("SKILL LEVELS"), tr("BEGINNER: Browse events, understand probability pricing\n"
                                    "INTERMEDIATE: Track probability changes, identify momentum\n"
                                    "ADVANCED: Arbitrage between prediction markets and traditional assets\n"
                                    "PRO: Build prediction market-enhanced trading models")},
        });
}

QWidget* DocsScreen::page_alt_investments() {
    return make_page(
        tr("ALTERNATIVE INVESTMENTS"), tr("Private equity, hedge funds, real assets, and alternatives data"),
        {
            {tr("OVERVIEW"), tr("The Alternative Investments screen covers non-traditional asset classes including "
                                "private equity, hedge funds, real estate, commodities, and other alternatives.")},
            {tr("KEY FEATURES"), tr("■  PE/VC deal flow and fund performance\n"
                                    "■  Hedge fund strategy tracking\n"
                                    "■  Real estate market data (REITs, cap rates)\n"
                                    "■  Commodity index tracking\n"
                                    "■  Correlation analysis with traditional assets")},
            {tr("REAL-WORLD USAGE"), tr("■  Portfolio diversification with alternative assets\n"
                                        "■  Hedge fund replication strategy development\n"
                                        "■  Real asset allocation for inflation hedging\n"
                                        "■  Private market opportunity screening")},
            {tr("SKILL LEVELS"), tr("BEGINNER: Understand alternative asset classes and their role\n"
                                    "INTERMEDIATE: Analyze REIT performance, commodity correlations\n"
                                    "ADVANCED: Factor-based alternative investment allocation\n"
                                    "PRO: Private market deal analysis, hedge fund replication, liquid alternatives")},
        });
}

// ============================================================================
// Tools
// ============================================================================

QWidget* DocsScreen::page_report_builder() {
    return make_page(tr("REPORT BUILDER"), tr("Drag-and-drop financial report creation"),
                     {
                         {tr("OVERVIEW"), tr("The Report Builder lets you create professional financial reports with a "
                                             "drag-and-drop interface. Combine charts, tables, text, and data from any "
                                             "terminal screen into presentation-ready documents.")},
                         {tr("KEY FEATURES"), tr("■  Component toolbar — Charts, tables, text blocks, images, headers\n"
                                                 "■  Drag-and-drop canvas with grid snapping\n"
                                                 "■  Properties panel — Customize colors, fonts, data sources\n"
                                                 "■  Multi-page support with page navigation\n"
                                                 "■  Export to PDF, PNG, or print directly\n"
                                                 "■  Template library for common report types")},
                         {tr("REAL-WORLD USAGE"), tr("■  Client reports: Weekly market summary for advisory clients\n"
                                                     "■  Investment memos: Structured investment thesis documents\n"
                                                     "■  Risk reports: Portfolio risk metrics for compliance\n"
                                                     "■  Research reports: Published equity research format")},
                         {tr("SKILL LEVELS"), tr("BEGINNER: Create a simple one-page report with a chart and text\n"
                                                 "INTERMEDIATE: Multi-page reports with tables and data binding\n"
                                                 "ADVANCED: Templated report generation, automated weekly reports\n"
                                                 "PRO: Branded client deliverables, automated report pipelines")},
                     });
}

QWidget* DocsScreen::page_node_editor() {
    return make_page(
        tr("NODE EDITOR"), tr("Visual workflow automation with drag-and-drop nodes"),
        {
            {tr("OVERVIEW"), tr("The Node Editor provides a visual programming environment for building "
                                "data processing and trading workflows by connecting nodes.")},
            {tr("KEY FEATURES"), tr("■  Drag-and-drop node placement\n"
                                    "■  Data source nodes — Market data, news, economic indicators\n"
                                    "■  Processing nodes — Filters, transformations, calculations\n"
                                    "■  Action nodes — Alerts, trades, report generation\n"
                                    "■  Workflow persistence and sharing\n"
                                    "■  Real-time execution monitoring")},
            {tr("REAL-WORLD USAGE"), tr("■  Alert systems: Price crosses MA → send notification\n"
                                        "■  Data pipelines: Fetch → filter → analyze → report\n"
                                        "■  Trading automation: Signal → validation → order → monitor\n"
                                        "■  Research workflows: Data collection → processing → visualization")},
            {tr("SKILL LEVELS"), tr("BEGINNER: Connect a data source to a simple alert node\n"
                                    "INTERMEDIATE: Multi-step workflows with conditional branching\n"
                                    "ADVANCED: Complex trading workflows with risk checks\n"
                                    "PRO: Production automation pipelines, multi-feed aggregation")},
        });
}

QWidget* DocsScreen::page_code_editor() {
    return make_page(
        tr("CODE EDITOR"), tr("Fincept Notebook — Jupyter-style Python notebooks inside the terminal"),
        {
            {tr("OVERVIEW"), tr("The Code Editor is a notebook: code and markdown cells saved as standard .ipynb "
                                "files. A LIBRARY of prebuilt finance, economics, trading, portfolio and quant "
                                "notebooks opens as editable copies.")},
            {tr("RUNNING CELLS"), tr("■  Ctrl+Enter runs a cell, Shift+Enter runs it and moves on, Ctrl+Shift+Enter "
                                     "runs the whole notebook top to bottom and stops at the first error\n"
                                     "■  Output streams into the cell while it runs; the value of the last line is "
                                     "shown like in Jupyter; errors show a traceback\n"
                                     "■  STOP (or Esc) interrupts running cells; a cell that passes the LIMIT is "
                                     "stopped with TimeoutError\n"
                                     "■  Each cell runs in its own process — variables do not carry over between "
                                     "cells, so define what a cell needs inside it")},
            {tr("ENVIRONMENTS"), tr("■  PY · NUMPY2 — the default analytics environment (pandas, numpy, scipy, "
                                    "statsmodels, yfinance, …)\n"
                                    "■  PY · NUMPY1 — vectorbt, backtesting.py, zipline, bt, ffn, PyPortfolioOpt, "
                                    "financepy\n"
                                    "■  The choice is saved with the notebook; relative file paths resolve against "
                                    "the notebook's folder")},
            {tr("FILES"), tr("■  NEW / OPEN / SAVE (Ctrl+N / Ctrl+O / Ctrl+S); outputs are saved in the .ipynb\n"
                             "■  Saved notebooks appear in the File Manager")},
        });
}

QWidget* DocsScreen::page_excel() {
    return make_page(
        tr("EXCEL"), tr("Multi-sheet spreadsheet with an Excel-compatible formula engine"),
        {
            {tr("OVERVIEW"), tr("A workbook of sheets with formulas, imported from or saved to .xlsx and .csv. "
                                "Formulas follow Excel's rules: operator precedence, error values (#DIV/0!, "
                                "#VALUE!, #REF!, #NAME?, #N/A) that propagate instead of turning into 0, and "
                                "references to other sheets (Sheet2!A1, 'My Sheet'!B2:C9).")},
            {tr("FUNCTIONS"), tr("■  Aggregates: SUM, AVERAGE, MIN, MAX, COUNT, COUNTA, MEDIAN, STDEV, VAR, LARGE, "
                                 "SMALL, PRODUCT, SUMPRODUCT, SUMIF, COUNTIF, AVERAGEIF, CORREL\n"
                                 "■  Financial: NPV, IRR, XNPV, XIRR, PMT, PV, FV, NPER, RATE\n"
                                 "■  Logic & lookup: IF, IFERROR, IFNA, AND, OR, NOT, VLOOKUP, HLOOKUP, INDEX, MATCH\n"
                                 "■  Math & text: ROUND / ROUNDUP / ROUNDDOWN, INT, MOD, ABS, SQRT, POWER, LN, LOG, "
                                 "EXP, CONCAT, LEFT, RIGHT, MID, LEN, TRIM, UPPER, LOWER, VALUE\n"
                                 "Hover the formula bar for the complete list.")},
            {tr("EDITING"), tr("■  Undo / redo (Ctrl+Z / Ctrl+Y) for edits, paste, clear and row/column changes\n"
                               "■  Copy/paste with Excel and Google Sheets: values go out as text; pasted "
                               "figures such as 1,234.50, $1,234, 12.5% or (1,200) count as numbers\n"
                               "■  Pasting inside the terminal keeps formulas and shifts relative references; "
                               "Ctrl+Shift+V pastes values only\n"
                               "■  Inserting or deleting rows and columns updates formula references\n"
                               "■  The status bar shows SUM / AVG / COUNT of the selection")},
            {tr("FILES"), tr("■  .xlsx import keeps formulas, dates, percentages and number formats; a formula "
                             "the engine cannot evaluate shows the value saved in the file (in italics)\n"
                             "■  .xlsx export writes numbers, dates and formulas as real cells; Ctrl+S saves an "
                             "open workbook in place\n"
                             "■  CSV import detects comma / semicolon / tab; CSV export writes computed values")},
        });
}

QWidget* DocsScreen::page_notes() {
    return make_page(
        tr("NOTES"), tr("Persistent note-taking for research and trade journals"),
        {
            {tr("OVERVIEW"), tr("The Notes screen provides a persistent note-taking environment for maintaining "
                                "research notes, trade journals, and analysis records.")},
            {tr("KEY FEATURES"), tr("■  Rich text editing\n"
                                    "■  Multiple note organization\n"
                                    "■  Persistent storage between sessions\n"
                                    "■  Search across all notes\n"
                                    "■  Timestamp tracking for each note")},
            {tr("REAL-WORLD USAGE"), tr("■  Trade journal: Record entry/exit rationale for every trade\n"
                                        "■  Research notes: Document investment theses and analysis\n"
                                        "■  Meeting notes: Record calls with management or analysts\n"
                                        "■  Watchlist notes: Add context to symbols you're tracking")},
            {tr("SKILL LEVELS"), tr("BEGINNER: Create your first trade journal entry\n"
                                    "INTERMEDIATE: Organized note system by category (trades, research, ideas)\n"
                                    "ADVANCED: Systematic trade journaling with performance review\n"
                                    "PRO: Integrated research workflow with notes linked to positions")},
        });
}

QWidget* DocsScreen::page_mcp_servers() {
    return make_page(
        tr("MCP SERVERS"), tr("Model Context Protocol server management"),
        {
            {tr("OVERVIEW"), tr("The MCP Servers screen manages Model Context Protocol servers that extend "
                                "AI agent capabilities with external tool access and data sources.")},
            {tr("KEY FEATURES"), tr("■  Server registration and management\n"
                                    "■  Tool discovery and configuration\n"
                                    "■  Connection status monitoring\n"
                                    "■  Custom server integration\n"
                                    "■  Security and access control")},
            {tr("REAL-WORLD USAGE"), tr("■  Extend AI agents with custom data sources\n"
                                        "■  Connect to proprietary APIs and databases\n"
                                        "■  Add specialized tools for domain-specific analysis\n"
                                        "■  Build custom research pipelines with external tools")},
            {tr("SKILL LEVELS"), tr("BEGINNER: Browse available MCP servers, understand the concept\n"
                                    "INTERMEDIATE: Connect pre-configured MCP servers to agents\n"
                                    "ADVANCED: Deploy custom MCP servers for proprietary data\n"
                                    "PRO: Build production MCP infrastructure, multi-server orchestration")},
        });
}

QWidget* DocsScreen::page_data_mapping() {
    return make_page(
        tr("DATA MAPPING"), tr("Turn any JSON API into a standard record (OHLCV, QUOTE, POSITION, …)"),
        {
            {tr("OVERVIEW"), tr("Data Mapping calls an HTTP API you describe, extracts fields with JSONPath and "
                                "validates the result against a standard schema. Saved mappings can be run "
                                "again from the MAPPINGS list; results are stored with a response cache.")},
            {tr("WORKFLOW"), tr("■  API CONFIG — URL, method, auth, headers, JSON body; TEST API REQUEST fetches a "
                                "sample response\n"
                                "■  SCHEMA — OHLCV, QUOTE, TICK, ORDER, POSITION, PORTFOLIO or INSTRUMENT\n"
                                "■  FIELD MAPPING — double-click a node of the sample response to use its path; "
                                "optional transform (to_number, unix_to_iso, upper, …) and default\n"
                                "■  CACHE — reuse a stored result for N seconds instead of calling the API\n"
                                "■  TEST & SAVE — runs the real engine on the sample and lists what did not map")},
            {tr("TEMPLATES"), tr("■  Ready-made mappings for CoinGecko, Binance, Kraken, Yahoo, Alpha Vantage, "
                                 "Twelve Data, Finnhub, CoinMarketCap, Alpaca, Tradier and DBnomics\n"
                                 "■  Put your API key in AUTH VALUE; {AUTH} in the URL, headers or body is "
                                 "replaced by it when the request is sent")},
            {tr("NOTES"), tr("■  The AUTH VALUE is stored unencrypted in the local database — use a scoped key\n"
                             "■  Requests run only when you press TEST or RUN; there is no scheduler")},
        });
}

QWidget* DocsScreen::page_data_sources() {
    return make_page(
        tr("DATA SOURCES"), tr("Data connector command center"),
        {
            {tr("OVERVIEW"), tr("Data Sources is where you browse, configure and test the connectors that feed "
                                "the terminal: market data providers, databases, cloud storage, files, APIs and "
                                "alternative-data feeds.")},
            {tr("KEY FEATURES"), tr("■  CATALOG — every built-in Python data connector: add a missing API key "
                                    "inline, pick a command, run it and view the result as a table or JSON, export "
                                    "CSV or copy the equivalent AI tool call\n"
                                    "■  CONNECTORS — searchable catalogue grouped by category, with the fields each "
                                    "connector needs\n"
                                    "■  + ADD CONNECTION — configure a connector with your own credentials\n"
                                    "■  TEST — verify a saved connection before relying on it\n"
                                    "■  SAVED CONNECTIONS — edit, enable/disable or delete; ENABLE ALL / DISABLE ALL\n"
                                    "■  IMPORT / EXPORT — move connection definitions between machines")},
            {tr("CATEGORIES"), tr("■  Market data, alternative data, open banking\n"
                                  "■  Relational, NoSQL, time-series databases and search / warehouse engines\n"
                                  "■  Cloud storage and file sources\n"
                                  "■  API streaming")},
            {tr("TIPS"), tr("■  Keys for the built-in providers can also be managed in Settings → Credentials")},
        });
}

QWidget* DocsScreen::page_asia_markets() {
    return make_page(
        tr("ASIA MARKETS"), tr("398+ stock endpoints for Chinese A/B shares, Hong Kong and US markets"),
        {
            {tr("OVERVIEW"), tr("The Asia Markets terminal is an explorer over the AKShare stock endpoints. Pick a "
                                "category, choose an endpoint, enter a symbol if it needs one, and press EXECUTE.")},
            {tr("CATEGORIES"), tr("■  Realtime and Historical quotes\n"
                                  "■  Financial statements and Holdings\n"
                                  "■  Fund Flow and Boards (sector / concept boards)\n"
                                  "■  Margin / HSGT (Stock Connect)\n"
                                  "■  Hot & News")},
            {tr("USAGE"), tr("■  Use the endpoint search box to find a function by name\n"
                             "■  Symbols are entered in the market's own format, e.g. 000001\n"
                             "■  Results can be shown as a TABLE or as raw JSON; REFRESH re-runs the query\n"
                             "■  Cached results are marked \"(cached)\"")},
        });
}

QWidget* DocsScreen::page_trade_viz() {
    return make_page(
        tr("TRADE VIZ"), tr("Bilateral trade flows as a chord diagram"),
        {
            {tr("OVERVIEW"), tr("Trade Viz shows a country's annual merchandise trade with its partners as a chord "
                                "diagram, next to a ranking table of the partners. Values are in USD millions.")},
            {tr("DATA"), tr("The figures come from UN Comtrade through the Economics service. A small static US "
                            "snapshot is shown only while the live data for your selection is unavailable.")},
        });
}

QWidget* DocsScreen::page_relationship_map() {
    return make_page(
        tr("RELATIONSHIP MAP"), tr("Corporate intelligence graph for a company"),
        {
            {tr("OVERVIEW"), tr("Search for an asset (for example AAPL, Tesla or RELIANCE) and press ANALYZE to build "
                                "a graph of the company and the entities around it.")},
            {tr("KEY FEATURES"), tr("■  Peers, institutional holders, mutual funds, insiders, officers and analysts "
                                    "as connected nodes\n"
                                    "■  Select a node to see its details (market cap, P/E, ROE, margins, growth, …)\n"
                                    "■  FILTERS and LEGEND panels to focus on the relationships you care about\n"
                                    "■  Layouts — FORCE, LAYERED and RADIAL; FIT (or Home) zooms to the whole graph\n"
                                    "■  A data-quality indicator shows how complete the result is")},
        });
}

QWidget* DocsScreen::page_agent_workbench() {
    return make_page(
        tr("AGENT WORKBENCH"), tr("Run Claude Code and Codex agents locally, wired to the terminal's tools"),
        {
            {tr("OVERVIEW"), tr("The Agent Workbench runs Claude Code or OpenAI Codex CLI agents on this machine as "
                                "background tasks that use the terminal's own tools over MCP. Runs keep going "
                                "when the screen is closed.")},
            {tr("TABS"), tr("■  RUN — write a task and follow the live transcript\n"
                            "■  RUNS — history of every run, sortable and filterable\n"
                            "■  SCHEDULES — recurring runs (e.g. a morning brief)\n"
                            "■  CONNECT — the terminal's MCP server and setup for standalone agents (Claude Code, "
                            "Codex, Cursor, Claude Desktop), with an activity log")},
            {tr("SETUP"), tr("■  Install the Claude Code or Codex CLI; set its path and the workspace under SETTINGS\n"
                             "■  Agents call the same tools AI Chat uses; actions that change data ask first")},
        });
}

QWidget* DocsScreen::page_file_manager() {
    return make_page(
        tr("FILE MANAGER"), tr("Everything the terminal has saved for you, in one place"),
        {
            {tr("OVERVIEW"), tr("The File Manager lists the files the terminal tracks — exports, backtest results, "
                                "notebooks, connector configurations and files you imported — with a quota bar "
                                "showing how much storage they use.")},
            {tr("KEY FEATURES"), tr("■  Filter chips by the tool that created a file, and sort by date\n"
                                    "■  Click a file card for an inline preview\n"
                                    "■  Import copies your own files into terminal storage; SAVE exports one back out\n"
                                    "■  Select several files for bulk delete (you are asked to confirm)\n"
                                    "■  Open a file in the screen that understands it — for example a notebook in "
                                    "the Code Editor")},
        });
}

// ============================================================================
// Community
// ============================================================================

QWidget* DocsScreen::page_settings() {
    return make_page(
        tr("SETTINGS"), tr("Application configuration and preferences"),
        {
            {tr("OVERVIEW"), tr("The Settings screen provides comprehensive application configuration "
                                "including display preferences, data sources, LLM configuration, MCP servers, "
                                "and integration settings.")},
            {tr("KEY FEATURES"), tr("■  Display preferences — Theme, font size, layout options\n"
                                    "■  Data configuration — API keys, refresh intervals, cache settings\n"
                                    "■  LLM Configuration — Model provider, API key, parameters\n"
                                    "■  MCP Servers — Server management and configuration\n"
                                    "■  Notification preferences\n"
                                    "■  Keyboard shortcut customization")},
            {tr("SECTIONS"), tr("Shell:\n"
                                "■  General — Language, currency, window behaviour\n"
                                "■  Appearance — Theme, fonts, density, ticker and chat bubble\n"
                                "■  Notifications — Alert channels (Telegram, Discord, Slack, email, …)\n"
                                "■  Keybindings — Rebind every shortcut\n"
                                "■  Voice — Speech-to-text and text-to-speech\n"
                                "■  Logging and Developer — Log levels, DataHub inspector, experimental options\n\n"
                                "Profile:\n"
                                "■  Profiles — Separate profiles with isolated data\n"
                                "■  Credentials — API keys for data providers and exchanges\n"
                                "■  Security — PIN, auto-lock and audit log\n"
                                "■  Data Sources, LLM Config, MCP Servers — Connectors, AI providers, tool servers\n"
                                "■  Python Env — Analytics runtime and packages\n"
                                "■  Storage & Cache — Disk usage and cleanup\n"
                                "■  Cloud Sync — Backup and sync")},
            {tr("SKILL LEVELS"), tr("BEGINNER: Set your timezone and preferred market\n"
                                    "INTERMEDIATE: Configure API keys for data providers and LLM\n"
                                    "ADVANCED: Fine-tune LLM parameters, set up MCP servers\n"
                                    "PRO: Full infrastructure configuration, custom integrations")},
        });
}

QWidget* DocsScreen::page_profile() {
    return make_page(
        tr("PROFILE"), tr("User account, subscription, and usage tracking"),
        {
            {tr("OVERVIEW"), tr("The Profile screen displays your account information, subscription status, "
                                "usage statistics, and billing history.")},
            {tr("KEY FEATURES"), tr("■  Overview — Account details, edit profile, credit balance and plan\n"
                                    "■  Usage — 30-day summary, daily usage and your most-used endpoints\n"
                                    "■  Security — API key (show / copy / regenerate), verification status, "
                                    "login history\n"
                                    "■  Billing — Current plan, credit balance and payment history, with UPGRADE "
                                    "TO PRO (free accounts) or MANAGE PLAN (paid) opening Plans & Pricing\n"
                                    "■  Support — Contact details; open tickets from the Support screen\n"
                                    "■  Logout and delete account")},
            {tr("REAL-WORLD USAGE"), tr("■  Monitor your plan and credit balance\n"
                                        "■  Move from Free to Pro ($10/month) when you need more credits\n"
                                        "■  Track API usage to stay within limits\n"
                                        "■  Review past payments in the payment history table")},
            {tr("SKILL LEVELS"), tr("BEGINNER: Check your account status and plan\n"
                                    "INTERMEDIATE: Monitor credit usage, plan API call budget\n"
                                    "ADVANCED: Optimize usage patterns for cost efficiency\n"
                                    "PRO: Budget credits across workflows using per-endpoint usage")},
        });
}

// ============================================================================
// Sidebar
// ============================================================================

} // namespace fincept::screens
