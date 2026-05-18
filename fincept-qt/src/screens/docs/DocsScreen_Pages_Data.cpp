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
    return make_page("DBNOMICS", "Access 100+ data providers with 500K+ economic datasets",
                     {
                         {"OVERVIEW", "DBnomics provides access to the world's largest open economic database. Browse "
                                      "100+ data providers (IMF, World Bank, OECD, ECB, Fed, etc.) with 500K+ datasets "
                                      "covering macroeconomics, finance, demographics, and trade."},
                         {"KEY FEATURES", "■  Provider browser — 100+ statistical agencies and central banks\n"
                                          "■  Dataset search — Full-text search across 500K+ datasets\n"
                                          "■  Series explorer — Browse individual time series with metadata\n"
                                          "■  Single view — Detailed chart + data table for one series\n"
                                          "■  Comparison view — 2x2 chart grid comparing multiple series\n"
                                          "■  CSV export — Download any series for external analysis\n"
                                          "■  Loading animations and progressive data loading"},
                         {"REAL-WORLD USAGE", "■  Macro research: GDP growth, inflation, employment data by country\n"
                                              "■  Central bank analysis: Interest rates, money supply, balance sheets\n"
                                              "■  Trade analysis: Import/export data, trade balances, tariffs\n"
                                              "■  Demographics: Population, labor force, migration data"},
                         {"SKILL LEVELS", "BEGINNER: Browse providers, look up US GDP or inflation data\n"
                                          "INTERMEDIATE: Compare series across countries, export data for analysis\n"
                                          "ADVANCED: Build macro dashboards, track leading indicators\n"
                                          "PRO: Integrate DBnomics data into quantitative macro trading models"},
                     });
}

QWidget* DocsScreen::page_economics() {
    return make_page("ECONOMICS", "Macroeconomic data and analysis tools",
                     {
                         {"OVERVIEW", "The Economics screen provides comprehensive macroeconomic data visualization "
                                      "and analysis tools for tracking economic indicators, central bank policies, "
                                      "and global economic trends."},
                         {"KEY FEATURES", "■  Economic indicators dashboard — GDP, CPI, unemployment, PMI\n"
                                          "■  Central bank tracker — Fed, ECB, BOJ, BOE rate decisions\n"
                                          "■  Economic calendar — Upcoming releases with forecasts\n"
                                          "■  Historical comparisons — Indicator trends over time\n"
                                          "■  Country comparison — Side-by-side economic profiles"},
                         {"REAL-WORLD USAGE", "■  Macro trading: Position ahead of rate decisions\n"
                                              "■  Risk management: Monitor recession indicators\n"
                                              "■  FX trading: Track rate differentials for carry trades\n"
                                              "■  Asset allocation: Shift sectors based on economic cycle"},
                         {"SKILL LEVELS", "BEGINNER: Check upcoming economic events, understand key indicators\n"
                                          "INTERMEDIATE: Track leading vs lagging indicators, spot economic trends\n"
                                          "ADVANCED: Build economic models, forecast indicator releases\n"
                                          "PRO: Multi-factor macro models, economic surprise index trading"},
                     });
}

QWidget* DocsScreen::page_akshare() {
    return make_page(
        "AKSHARE DATA", "Chinese and Asian market data via AkShare",
        {
            {"OVERVIEW", "AkShare provides access to Chinese financial market data including A-shares, "
                         "Hong Kong stocks, Chinese futures, and macro data from Chinese statistical agencies."},
            {"KEY FEATURES", "■  A-Share market data — SSE, SZSE listed companies\n"
                             "■  Hong Kong market data — HKEX listings\n"
                             "■  Chinese futures — commodity and financial futures\n"
                             "■  Macro data — NBS statistics, PBOC data\n"
                             "■  Fund data — Mutual funds, ETFs listed in China"},
            {"REAL-WORLD USAGE", "■  China market exposure: Research A-share opportunities\n"
                                 "■  Emerging market analysis: Compare Chinese vs global markets\n"
                                 "■  Commodity trading: Track Chinese commodity demand\n"
                                 "■  Macro research: Monitor Chinese economic indicators"},
            {"SKILL LEVELS", "BEGINNER: Browse Chinese market indices and major stocks\n"
                             "INTERMEDIATE: Compare A-share sectors, track PBOC policy\n"
                             "ADVANCED: Chinese commodity-equity correlation analysis\n"
                             "PRO: Cross-market arbitrage between A-shares, H-shares, and ADRs"},
        });
}

QWidget* DocsScreen::page_gov_data() {
    return make_page("GOVERNMENT DATA", "Official government statistical data",
                     {
                         {"OVERVIEW", "Access official government data from statistical agencies worldwide including "
                                      "census data, economic surveys, trade statistics, and regulatory filings."},
                         {"KEY FEATURES", "■  Multi-country government data sources\n"
                                          "■  Census and demographic data\n"
                                          "■  Trade and commerce statistics\n"
                                          "■  Regulatory filings and disclosures\n"
                                          "■  Historical data series with long time horizons"},
                         {"REAL-WORLD USAGE", "■  Research: Access primary source economic data\n"
                                              "■  Policy analysis: Track government spending and fiscal policy\n"
                                              "■  Demographics: Population and labor force trends\n"
                                              "■  Trade analysis: Import/export flows by country and sector"},
                         {"SKILL LEVELS", "BEGINNER: Browse available datasets, download basic reports\n"
                                          "INTERMEDIATE: Cross-reference government data with market data\n"
                                          "ADVANCED: Build predictive models using government leading indicators\n"
                                          "PRO: Automated policy tracking, fiscal surprise detection"},
                     });
}

// ============================================================================
// Geopolitics & Alt
// ============================================================================

QWidget* DocsScreen::page_geopolitics() {
    return make_page(
        "GEOPOLITICS", "Conflict monitoring, HDX data, trade analysis, and relationship mapping",
        {
            {"OVERVIEW", "The Geopolitics screen provides a comprehensive view of global geopolitical events "
                         "with conflict monitoring, humanitarian data (HDX), trade flow analysis, and entity "
                         "relationship mapping for understanding geopolitical dynamics."},
            {"KEY FEATURES", "■  Conflict monitor — Active global conflicts with severity tracking\n"
                             "■  HDX integration — Humanitarian Data Exchange for crisis data\n"
                             "■  Trade analysis — Sanctions, tariffs, trade flow disruptions\n"
                             "■  Relationship mapping — Entity relationships and influence networks\n"
                             "■  Risk scoring — Country-level geopolitical risk assessments\n"
                             "■  AI agent — Geopolitics-specialized analysis agent"},
            {"REAL-WORLD USAGE", "■  Risk management: Monitor geopolitical risks to your portfolio\n"
                                 "■  Commodity trading: Track supply disruptions from conflicts\n"
                                 "■  FX trading: Geopolitical risk premium in currency pricing\n"
                                 "■  ESG investing: Humanitarian impact assessment"},
            {"SKILL LEVELS", "BEGINNER: Monitor the conflict dashboard, understand risk scores\n"
                             "INTERMEDIATE: Correlate geopolitical events with market movements\n"
                             "ADVANCED: Build geopolitical risk models, trade around events\n"
                             "PRO: Multi-factor geopolitical alpha, automated event-driven trading"},
        });
}

QWidget* DocsScreen::page_maritime() {
    return make_page("MARITIME", "Maritime shipping and trade flow data",
                     {
                         {"OVERVIEW", "The Maritime screen provides shipping and trade flow data for monitoring global "
                                      "supply chains, commodity transport, and maritime trade patterns."},
                         {"KEY FEATURES", "■  Vessel tracking — AIS data for major shipping routes\n"
                                          "■  Port analytics — Throughput, congestion, wait times\n"
                                          "■  Trade flows — Commodity shipping volumes by route\n"
                                          "■  Supply chain indicators — Shipping cost indices\n"
                                          "■  Geopolitical overlay — Chokepoint risk assessment"},
                         {"REAL-WORLD USAGE", "■  Commodity trading: Track oil tanker movements for supply signals\n"
                                              "■  Supply chain: Monitor port congestion for inflation signals\n"
                                              "■  Shipping stocks: Analyze Baltic Dry Index and freight rates\n"
                                              "■  Geopolitical risk: Chokepoint monitoring (Suez, Strait of Hormuz)"},
                         {"SKILL LEVELS", "BEGINNER: Browse shipping routes, understand trade flow basics\n"
                                          "INTERMEDIATE: Track commodity shipments, correlate with futures prices\n"
                                          "ADVANCED: Build supply chain disruption models\n"
                                          "PRO: Satellite-verified shipping data for alternative data alpha"},
                     });
}

QWidget* DocsScreen::page_polymarket() {
    return make_page(
        "POLYMARKET", "Prediction markets and event probability trading",
        {
            {"OVERVIEW", "The Polymarket screen provides access to prediction market data, allowing you to "
                         "track event probabilities and trade on real-world outcomes."},
            {"KEY FEATURES", "■  Event browser — Political, economic, sports, crypto events\n"
                             "■  Probability charts — Historical probability movement\n"
                             "■  Volume and liquidity data\n"
                             "■  Category filtering and search\n"
                             "■  Correlation with traditional markets"},
            {"REAL-WORLD USAGE", "■  Election trading: Track political event probabilities\n"
                                 "■  Hedging: Use prediction markets to hedge event risk\n"
                                 "■  Sentiment gauge: Market-implied probabilities as sentiment\n"
                                 "■  Research: Compare prediction market accuracy with polls"},
            {"SKILL LEVELS", "BEGINNER: Browse events, understand probability pricing\n"
                             "INTERMEDIATE: Track probability changes, identify momentum\n"
                             "ADVANCED: Arbitrage between prediction markets and traditional assets\n"
                             "PRO: Build prediction market-enhanced trading models"},
        });
}

QWidget* DocsScreen::page_alt_investments() {
    return make_page(
        "ALTERNATIVE INVESTMENTS", "Private equity, hedge funds, real assets, and alternatives data",
        {
            {"OVERVIEW", "The Alternative Investments screen covers non-traditional asset classes including "
                         "private equity, hedge funds, real estate, commodities, and other alternatives."},
            {"KEY FEATURES", "■  PE/VC deal flow and fund performance\n"
                             "■  Hedge fund strategy tracking\n"
                             "■  Real estate market data (REITs, cap rates)\n"
                             "■  Commodity index tracking\n"
                             "■  Correlation analysis with traditional assets"},
            {"REAL-WORLD USAGE", "■  Portfolio diversification with alternative assets\n"
                                 "■  Hedge fund replication strategy development\n"
                                 "■  Real asset allocation for inflation hedging\n"
                                 "■  Private market opportunity screening"},
            {"SKILL LEVELS", "BEGINNER: Understand alternative asset classes and their role\n"
                             "INTERMEDIATE: Analyze REIT performance, commodity correlations\n"
                             "ADVANCED: Factor-based alternative investment allocation\n"
                             "PRO: Private market deal analysis, hedge fund replication, liquid alternatives"},
        });
}

// ============================================================================
// Tools
// ============================================================================

QWidget* DocsScreen::page_report_builder() {
    return make_page("REPORT BUILDER", "Drag-and-drop financial report creation",
                     {
                         {"OVERVIEW", "The Report Builder lets you create professional financial reports with a "
                                      "drag-and-drop interface. Combine charts, tables, text, and data from any "
                                      "terminal screen into presentation-ready documents."},
                         {"KEY FEATURES", "■  Component toolbar — Charts, tables, text blocks, images, headers\n"
                                          "■  Drag-and-drop canvas with grid snapping\n"
                                          "■  Properties panel — Customize colors, fonts, data sources\n"
                                          "■  Multi-page support with page navigation\n"
                                          "■  Export to PDF, PNG, or print directly\n"
                                          "■  Template library for common report types"},
                         {"REAL-WORLD USAGE", "■  Client reports: Weekly market summary for advisory clients\n"
                                              "■  Investment memos: Structured investment thesis documents\n"
                                              "■  Risk reports: Portfolio risk metrics for compliance\n"
                                              "■  Research reports: Published equity research format"},
                         {"SKILL LEVELS", "BEGINNER: Create a simple one-page report with a chart and text\n"
                                          "INTERMEDIATE: Multi-page reports with tables and data binding\n"
                                          "ADVANCED: Templated report generation, automated weekly reports\n"
                                          "PRO: Branded client deliverables, automated report pipelines"},
                     });
}

QWidget* DocsScreen::page_node_editor() {
    return make_page("NODE EDITOR", "Visual workflow automation with drag-and-drop nodes",
                     {
                         {"OVERVIEW", "The Node Editor provides a visual programming environment for building "
                                      "data processing and trading workflows by connecting nodes."},
                         {"KEY FEATURES", "■  Drag-and-drop node placement\n"
                                          "■  Data source nodes — Market data, news, economic indicators\n"
                                          "■  Processing nodes — Filters, transformations, calculations\n"
                                          "■  Action nodes — Alerts, trades, report generation\n"
                                          "■  Workflow persistence and sharing\n"
                                          "■  Real-time execution monitoring"},
                         {"REAL-WORLD USAGE", "■  Alert systems: Price crosses MA → send notification\n"
                                              "■  Data pipelines: Fetch → filter → analyze → report\n"
                                              "■  Trading automation: Signal → validation → order → monitor\n"
                                              "■  Research workflows: Data collection → processing → visualization"},
                         {"SKILL LEVELS", "BEGINNER: Connect a data source to a simple alert node\n"
                                          "INTERMEDIATE: Multi-step workflows with conditional branching\n"
                                          "ADVANCED: Complex trading workflows with risk checks\n"
                                          "PRO: Production automation pipelines, multi-feed aggregation"},
                     });
}

QWidget* DocsScreen::page_code_editor() {
    return make_page("CODE EDITOR", "Built-in code editor for trading strategies and scripts",
                     {
                         {"OVERVIEW", "The Code Editor provides a built-in development environment for writing "
                                      "and testing Python trading strategies and analytics scripts."},
                         {"KEY FEATURES", "■  Syntax highlighting for Python\n"
                                          "■  Script execution with output panel\n"
                                          "■  Access to terminal data via APIs\n"
                                          "■  Template library for common patterns\n"
                                          "■  Integration with backtesting engine"},
                         {"REAL-WORLD USAGE", "■  Strategy development: Write and test Python strategies\n"
                                              "■  Custom analytics: Build bespoke analysis scripts\n"
                                              "■  Data processing: Transform and analyze raw data\n"
                                              "■  Automation: Create scheduled data processing jobs"},
                         {"SKILL LEVELS", "BEGINNER: Run example scripts, modify parameters\n"
                                          "INTERMEDIATE: Write simple strategies from templates\n"
                                          "ADVANCED: Custom analytics with pandas, numpy, scipy\n"
                                          "PRO: Full strategy development with custom backtesting integration"},
                     });
}

QWidget* DocsScreen::page_excel() {
    return make_page("EXCEL", "Spreadsheet interface for financial analysis",
                     {
                         {"OVERVIEW", "The Excel screen provides a spreadsheet-style interface for financial "
                                      "analysis with formula support, data import/export, and charting."},
                         {"KEY FEATURES", "■  Spreadsheet grid with formula support\n"
                                          "■  Data import from terminal screens\n"
                                          "■  Financial functions (NPV, IRR, XIRR, etc.)\n"
                                          "■  Chart creation from spreadsheet data\n"
                                          "■  CSV import/export compatibility"},
                         {"REAL-WORLD USAGE", "■  Financial modeling: Build DCF models in a familiar interface\n"
                                              "■  Data analysis: Import market data for custom calculations\n"
                                              "■  Reporting: Create tabular reports with formulas\n"
                                              "■  Comparison: Side-by-side metric comparison tables"},
                         {"SKILL LEVELS", "BEGINNER: Enter data, use basic formulas (SUM, AVG)\n"
                                          "INTERMEDIATE: Financial functions, data import from terminal\n"
                                          "ADVANCED: Complex financial models with linked cells\n"
                                          "PRO: Automated model updates with live data feeds"},
                     });
}

QWidget* DocsScreen::page_notes() {
    return make_page("NOTES", "Persistent note-taking for research and trade journals",
                     {
                         {"OVERVIEW", "The Notes screen provides a persistent note-taking environment for maintaining "
                                      "research notes, trade journals, and analysis records."},
                         {"KEY FEATURES", "■  Rich text editing\n"
                                          "■  Multiple note organization\n"
                                          "■  Persistent storage between sessions\n"
                                          "■  Search across all notes\n"
                                          "■  Timestamp tracking for each note"},
                         {"REAL-WORLD USAGE", "■  Trade journal: Record entry/exit rationale for every trade\n"
                                              "■  Research notes: Document investment theses and analysis\n"
                                              "■  Meeting notes: Record calls with management or analysts\n"
                                              "■  Watchlist notes: Add context to symbols you're tracking"},
                         {"SKILL LEVELS", "BEGINNER: Create your first trade journal entry\n"
                                          "INTERMEDIATE: Organized note system by category (trades, research, ideas)\n"
                                          "ADVANCED: Systematic trade journaling with performance review\n"
                                          "PRO: Integrated research workflow with notes linked to positions"},
                     });
}

QWidget* DocsScreen::page_mcp_servers() {
    return make_page("MCP SERVERS", "Model Context Protocol server management",
                     {
                         {"OVERVIEW", "The MCP Servers screen manages Model Context Protocol servers that extend "
                                      "AI agent capabilities with external tool access and data sources."},
                         {"KEY FEATURES", "■  Server registration and management\n"
                                          "■  Tool discovery and configuration\n"
                                          "■  Connection status monitoring\n"
                                          "■  Custom server integration\n"
                                          "■  Security and access control"},
                         {"REAL-WORLD USAGE", "■  Extend AI agents with custom data sources\n"
                                              "■  Connect to proprietary APIs and databases\n"
                                              "■  Add specialized tools for domain-specific analysis\n"
                                              "■  Build custom research pipelines with external tools"},
                         {"SKILL LEVELS", "BEGINNER: Browse available MCP servers, understand the concept\n"
                                          "INTERMEDIATE: Connect pre-configured MCP servers to agents\n"
                                          "ADVANCED: Deploy custom MCP servers for proprietary data\n"
                                          "PRO: Build production MCP infrastructure, multi-server orchestration"},
                     });
}

QWidget* DocsScreen::page_data_mapping() {
    return make_page("DATA MAPPING", "Map and configure external data source connections",
                     {
                         {"OVERVIEW", "The Data Mapping screen lets you configure connections between external data "
                                      "sources and terminal screens, mapping fields and transforming data formats."},
                         {"KEY FEATURES", "■  Data source configuration\n"
                                          "■  Field mapping and transformation\n"
                                          "■  Schema validation\n"
                                          "■  Connection testing\n"
                                          "■  Scheduled refresh configuration"},
                         {"REAL-WORLD USAGE", "■  Connect proprietary data feeds to terminal widgets\n"
                                              "■  Map custom API responses to standard formats\n"
                                              "■  Integrate internal databases with terminal screens\n"
                                              "■  Configure automated data refresh schedules"},
                         {"SKILL LEVELS", "BEGINNER: Browse configured data sources\n"
                                          "INTERMEDIATE: Add new data source connections\n"
                                          "ADVANCED: Custom field mapping and transformation logic\n"
                                          "PRO: Complex multi-source data pipelines with validation"},
                     });
}

// ============================================================================
// Community
// ============================================================================

QWidget* DocsScreen::page_settings() {
    return make_page("SETTINGS", "Application configuration and preferences",
                     {
                         {"OVERVIEW", "The Settings screen provides comprehensive application configuration "
                                      "including display preferences, data sources, LLM configuration, MCP servers, "
                                      "and integration settings."},
                         {"KEY FEATURES", "■  Display preferences — Theme, font size, layout options\n"
                                          "■  Data configuration — API keys, refresh intervals, cache settings\n"
                                          "■  LLM Configuration — Model provider, API key, parameters\n"
                                          "■  MCP Servers — Server management and configuration\n"
                                          "■  Notification preferences\n"
                                          "■  Keyboard shortcut customization"},
                         {"SECTIONS", "■  General — Language, timezone, display density\n"
                                      "■  Trading — Default exchange, order confirmations, risk limits\n"
                                      "■  Data — API keys for market data providers\n"
                                      "■  AI — LLM provider, model selection, temperature\n"
                                      "■  MCP — Server endpoints and tool access\n"
                                      "■  Notifications — Alert channels and thresholds"},
                         {"SKILL LEVELS", "BEGINNER: Set your timezone and preferred market\n"
                                          "INTERMEDIATE: Configure API keys for data providers and LLM\n"
                                          "ADVANCED: Fine-tune LLM parameters, set up MCP servers\n"
                                          "PRO: Full infrastructure configuration, custom integrations"},
                     });
}

QWidget* DocsScreen::page_profile() {
    return make_page("PROFILE", "User account, subscription, and usage tracking",
                     {
                         {"OVERVIEW", "The Profile screen displays your account information, subscription status, "
                                      "usage statistics, and billing history."},
                         {"KEY FEATURES", "■  Account details — Name, email, registration date\n"
                                          "■  Subscription status — Current plan, expiry, features\n"
                                          "■  Usage statistics — API calls, data consumed, trades placed\n"
                                          "■  Credit balance — Remaining AI/compute credits\n"
                                          "■  Billing history — Past invoices and payments"},
                         {"REAL-WORLD USAGE", "■  Monitor your subscription and credit usage\n"
                                              "■  Upgrade or change your plan\n"
                                              "■  Track API usage to stay within limits\n"
                                              "■  Download billing records for expense tracking"},
                         {"SKILL LEVELS", "BEGINNER: Check your account status and plan\n"
                                          "INTERMEDIATE: Monitor credit usage, plan API call budget\n"
                                          "ADVANCED: Optimize usage patterns for cost efficiency\n"
                                          "PRO: Enterprise account management, team usage tracking"},
                     });
}

// ============================================================================
// Sidebar
// ============================================================================

} // namespace fincept::screens
