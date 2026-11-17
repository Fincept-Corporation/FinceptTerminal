// ReportBuilderTemplates.cpp — the built-in template catalogue and builder
// (see ReportBuilderTemplates.h).
//
// Kept in a separate translation unit because the template catalog is
// ~700 lines of literal data and would dominate the service file.

#include "services/report_builder/ReportBuilderTemplates.h"

#include "core/report/ReportBinding.h"
#include "core/report/ReportBlocks.h"
#include "core/report/ReportDocument.h"

#include <QCoreApplication>
#include <QDateTime>

namespace fincept::services::report_templates {

namespace rep = report;

namespace {

// Same translation context and source text the template dialog used, so existing
// translations keep applying after the catalogue moved here.
QString rbt_tr(const char* text) {
    return QCoreApplication::translate("fincept::screens::ReportBuilderScreen", text);
}

// A prose slot: empty text with guidance shown on the canvas only (exports leave it out).
rep::ReportComponent rbt_slot(const QString& guidance) {
    rep::ReportComponent c;
    c.type = QStringLiteral("text");
    c.config.insert(QStringLiteral("placeholder"), guidance);
    return c;
}

void rbt_add(rep::ReportDocument& doc, rep::ReportComponent c) {
    c.id = doc.allocate_id();
    doc.components.append(c);
}

// ── Live-data templates: bound blocks + guidance, no invented numbers ─────────

void rbt_equity_note(rep::ReportDocument& doc, const QString& symbol_in) {
    namespace b = rep::blocks;
    const QString sym = symbol_in.trimmed().toUpper();
    doc.metadata.title = sym.isEmpty() ? QStringLiteral("Equity Research Note")
                                       : sym + QStringLiteral(" — Equity Research Note");
    rbt_add(doc, b::heading(QStringLiteral("Snapshot"), 1));
    rbt_add(doc, b::bound(rep::quote_binding(sym)));
    rbt_add(doc, b::bound(rep::info_binding(sym), QStringLiteral("Key statistics")));
    rbt_add(doc, b::heading(QStringLiteral("Price performance"), 1));
    rbt_add(doc, b::bound(rep::history_binding({sym}, QStringLiteral("1y"), QStringLiteral("1d")),
                          sym.isEmpty() ? QStringLiteral("Price, 1 year") : sym + QStringLiteral(" — 1 year")));
    rbt_add(doc, rbt_slot(QStringLiteral("What moved the stock this year: results, guidance, sector moves.")));
    rbt_add(doc, b::heading(QStringLiteral("Financials"), 1));
    rbt_add(doc, b::bound(rep::financials_binding(sym, QStringLiteral("income"), QStringLiteral("annual"), 4),
                          QStringLiteral("Income statement (annual)")));
    rbt_add(doc, b::bound(rep::financials_binding(sym, QStringLiteral("cashflow"), QStringLiteral("annual"), 4),
                          QStringLiteral("Cash flow (annual)")));
    rbt_add(doc, b::heading(QStringLiteral("Peer comparison"), 1));
    rbt_add(doc, b::bound(rep::peers_binding(sym, 5), QStringLiteral("Peers")));
    rbt_add(doc, b::heading(QStringLiteral("Recent news"), 1));
    rbt_add(doc, b::bound(rep::news_binding(sym, 8), QStringLiteral("Headlines")));
    rbt_add(doc, b::heading(QStringLiteral("Investment thesis"), 1));
    rbt_add(doc, rbt_slot(QStringLiteral("Your thesis: what the market is missing, and what would prove you right.")));
    rbt_add(doc, b::heading(QStringLiteral("Valuation"), 1));
    rbt_add(doc, rbt_slot(QStringLiteral("Method, key assumptions and the resulting value range.")));
    rbt_add(doc, b::heading(QStringLiteral("Risks"), 1));
    rbt_add(doc, rbt_slot(QStringLiteral("What would make the thesis wrong — and how you would notice.")));
    rbt_add(doc, b::caption(QStringLiteral("For research purposes only; not investment advice. Market data from Yahoo "
                                          "Finance via Fincept; each block shows when its data was taken.")));
}

void rbt_portfolio_review(rep::ReportDocument& doc, const QString& portfolio_id) {
    namespace b = rep::blocks;
    doc.metadata.title = QStringLiteral("Portfolio Review");
    rbt_add(doc, b::heading(QStringLiteral("Summary"), 1));
    rbt_add(doc, b::bound(rep::portfolio_binding(portfolio_id, QStringLiteral("summary")), QStringLiteral("Performance")));
    rbt_add(doc, rbt_slot(QStringLiteral("How the period went in two sentences: return, what drove it.")));
    rbt_add(doc, b::heading(QStringLiteral("Allocation"), 1));
    rbt_add(doc, b::bound(rep::portfolio_binding(portfolio_id, QStringLiteral("allocation")), QStringLiteral("Allocation by holding")));
    rbt_add(doc, b::heading(QStringLiteral("Holdings"), 1));
    rbt_add(doc, b::bound(rep::portfolio_binding(portfolio_id, QStringLiteral("holdings")), QStringLiteral("Holdings")));
    rbt_add(doc, b::heading(QStringLiteral("Commentary"), 1));
    rbt_add(doc, rbt_slot(QStringLiteral("Contributors and detractors, risk, changes made in the period.")));
    rbt_add(doc, b::heading(QStringLiteral("Next steps"), 1));
    rbt_add(doc, rbt_slot(QStringLiteral("Planned trades, rebalancing and what you are watching.")));
    rbt_add(doc, b::caption(QStringLiteral("Values in the portfolio's base currency; holdings priced from the latest "
                                          "quotes. Each block shows when its data was taken.")));
}

void rbt_weekly_wrap(rep::ReportDocument& doc) {
    namespace b = rep::blocks;
    doc.metadata.title = QStringLiteral("Weekly Market Wrap — week of %1")
                             .arg(QDate::currentDate().addDays(1 - QDate::currentDate().dayOfWeek()).toString(
                                 QStringLiteral("d MMM yyyy")));
    auto universe = [](const QString& u, const QString& sort, int limit) {
        rep::Binding bd = rep::quotes_binding({}, sort, limit);
        bd.params.insert(QStringLiteral("universe"), u);
        return bd;
    };
    rbt_add(doc, b::heading(QStringLiteral("Major indices"), 1));
    rbt_add(doc, b::bound(universe(QStringLiteral("indices"), {}, 0), QStringLiteral("Indices")));
    rbt_add(doc, b::bound(rep::history_binding({QStringLiteral("^GSPC"), QStringLiteral("^IXIC"), QStringLiteral("^STOXX50E")},
                                               QStringLiteral("1mo"), QStringLiteral("1d"), QStringLiteral("rebased")),
                          QStringLiteral("S&P 500, Nasdaq, Euro Stoxx 50 — 1 month, rebased to 100")));
    rbt_add(doc, b::heading(QStringLiteral("Rates, currencies and commodities"), 1));
    rbt_add(doc, b::bound(universe(QStringLiteral("macro"), {}, 0), QStringLiteral("Cross-asset")));
    rbt_add(doc, b::heading(QStringLiteral("Movers"), 1));
    rbt_add(doc, b::bound(universe(QStringLiteral("movers"), QStringLiteral("abs_change_desc"), 10),
                          QStringLiteral("Largest moves today")));
    rbt_add(doc, b::heading(QStringLiteral("Your watchlist"), 1));
    rbt_add(doc, b::bound(rep::watchlist_binding(QString(), QStringLiteral("change_desc")), QStringLiteral("Watchlist")));
    rbt_add(doc, b::heading(QStringLiteral("Headlines"), 1));
    rbt_add(doc, b::bound(rep::headlines_binding(10), QStringLiteral("Headlines")));
    rbt_add(doc, b::heading(QStringLiteral("Economic calendar"), 1));
    // Anchored on the day of each refresh (date ""), not on {{date}}: the free feed lists the
    // current week only, so a wrap regenerated every week keeps showing that week's releases
    // instead of failing on the date it was first created with.
    rbt_add(doc, b::bound(rep::econ_calendar_binding(QString(), QStringLiteral("week"), QStringLiteral("high")),
                          QStringLiteral("This week's high-impact releases")));
    rbt_add(doc, b::heading(QStringLiteral("What to watch next week"), 1));
    rbt_add(doc, rbt_slot(QStringLiteral("Data releases, central-bank meetings, earnings you are watching.")));
    rbt_add(doc, b::caption(QStringLiteral("Market data from Yahoo Finance via Fincept; headlines from the News tab's "
                                          "feeds; economic calendar from Forex Factory's free weekly export (forecast "
                                          "and previous values, no actuals). Each block shows when its data was taken.")));
}

// The quarter whose results come next: the last completed calendar quarter ("Q3 2026").
QString rbt_last_quarter() {
    const QDate d = QDate::currentDate();
    int q = (d.month() - 1) / 3; // 0..3 = the current quarter
    int y = d.year();
    if (q == 0) {
        q = 4;
        --y;
    }
    return QStringLiteral("Q%1 %2").arg(q).arg(y);
}

rep::ReportComponent rbt_cover(const QString& subtitle, const QString& disclaimer) {
    rep::ReportComponent c;
    c.type = QStringLiteral("cover");
    c.config.insert(QStringLiteral("subtitle"), subtitle);
    if (!disclaimer.isEmpty())
        c.config.insert(QStringLiteral("disclaimer"), disclaimer);
    return c;
}

rep::ReportComponent rbt_chart(const rep::Binding& bind, const QString& title, const QMap<QString, QString>& extra = {}) {
    rep::ReportComponent c = rep::blocks::bound(bind, title);
    for (auto it = extra.cbegin(); it != extra.cend(); ++it)
        c.config.insert(it.key(), it.value());
    return c;
}

rep::Binding rbt_fin_chart(const QString& sym, const QString& period, int n, const QStringList& items) {
    rep::Binding b = rep::financials_binding(sym, QStringLiteral("income"), period, n);
    b.view = QString::fromLatin1(rep::view::kChart);
    b.params.insert(QStringLiteral("items"), rep::to_json_array(items));
    return b;
}

// Earnings preview / review. Every data block follows the {{symbol}} variable, so one
// report re-targets by changing the variable; {{period}} names the quarter in the text.
void rbt_earnings(rep::ReportDocument& doc, const QString& symbol_in, bool review) {
    namespace b = rep::blocks;
    const QString sym = symbol_in.trimmed().toUpper();
    const QString v = QStringLiteral("{{symbol}}");
    doc.metadata.variables.insert(QStringLiteral("symbol"), sym);
    doc.metadata.variables.insert(QStringLiteral("period"), rbt_last_quarter());
    doc.metadata.title = review ? QStringLiteral("{{symbol}} — Earnings review") : QStringLiteral("{{symbol}} — Earnings preview");
    doc.metadata.number_headings = true;
    rbt_add(doc, rbt_cover(review ? QStringLiteral("{{period}} results: what happened")
                                  : QStringLiteral("{{period}} results: what to expect"),
                           QStringLiteral("For research purposes only; not investment advice. Market data from Yahoo "
                                          "Finance via Fincept, each block dated.")));
    rbt_add(doc, b::heading(QStringLiteral("Snapshot"), 1));
    rbt_add(doc, b::bound(rep::quote_binding(v)));
    rbt_add(doc, b::bound(rep::info_binding(v), QStringLiteral("Key statistics (incl. next earnings date)")));
    if (review) {
        rbt_add(doc, b::heading(QStringLiteral("Results vs expectations"), 1));
        rbt_add(doc, rbt_slot(QStringLiteral("Revenue, margin and EPS against what the market expected; the one number "
                                             "that mattered.")));
        rbt_add(doc, b::heading(QStringLiteral("Guidance and management commentary"), 1));
        rbt_add(doc, rbt_slot(QStringLiteral("What changed in the outlook, and how credible it is.")));
        rbt_add(doc, b::heading(QStringLiteral("Price reaction"), 1));
        rbt_add(doc, rbt_chart(rep::history_binding({v}, QStringLiteral("1mo"), QStringLiteral("1d"), QStringLiteral("price"),
                                                    QStringLiteral("candle")),
                               QStringLiteral("{{symbol}} — 1 month around the print"),
                               {{QStringLiteral("annot_last"), QStringLiteral("1")}}));
    } else {
        rbt_add(doc, b::heading(QStringLiteral("Price into the print"), 1));
        rbt_add(doc, rbt_chart(rep::history_binding({v}, QStringLiteral("3mo"), QStringLiteral("1d"), QStringLiteral("price"),
                                                    QStringLiteral("candle")),
                               QStringLiteral("{{symbol}} — 3 months, daily"),
                               {{QStringLiteral("annot_last"), QStringLiteral("1")},
                                {QStringLiteral("annot_minmax"), QStringLiteral("1")}}));
    }
    rbt_add(doc, b::heading(QStringLiteral("Last eight quarters"), 1));
    rbt_add(doc, b::bound(rep::financials_binding(v, QStringLiteral("income"), QStringLiteral("quarterly"), 8),
                          QStringLiteral("Income statement (quarterly)")));
    rbt_add(doc, rbt_chart(rbt_fin_chart(v, QStringLiteral("quarterly"), 8,
                                         {QStringLiteral("Total Revenue"), QStringLiteral("Net Income")}),
                           QStringLiteral("Revenue and net income by quarter")));
    rbt_add(doc, rbt_chart(rbt_fin_chart(v, QStringLiteral("quarterly"), 8, {QStringLiteral("Diluted EPS")}),
                           QStringLiteral("Diluted EPS by quarter")));
    rbt_add(doc, b::heading(review ? QStringLiteral("What changes in the thesis") : QStringLiteral("What to watch"), 1));
    rbt_add(doc, rbt_slot(review ? QStringLiteral("What the quarter proved or disproved, and what you do now.")
                                 : QStringLiteral("The three numbers or remarks that will move the stock, and where you "
                                                  "stand against consensus.")));
    rbt_add(doc, b::heading(QStringLiteral("Peers"), 1));
    rbt_add(doc, b::bound(rep::peers_binding(v, 5), QStringLiteral("Peer comparison")));
    rbt_add(doc, b::heading(QStringLiteral("Recent news"), 1));
    rbt_add(doc, b::bound(rep::news_binding(v, 8), QStringLiteral("Headlines")));
    rbt_add(doc, b::caption(QStringLiteral("Consensus estimates are not included: no free source provides them. Each data "
                                          "block shows when its data was taken.")));
}

// M&A teaser: target snapshot, financial summary and trading comparables from live data;
// the valuation comes from M&A Analytics through its "Add to report" button (a snapshot the
// user adds — this template never reaches into that tab).
void rbt_ma_teaser(rep::ReportDocument& doc, const QString& symbol_in) {
    namespace b = rep::blocks;
    const QString sym = symbol_in.trimmed().toUpper();
    const QString v = QStringLiteral("{{symbol}}");
    doc.metadata.variables.insert(QStringLiteral("symbol"), sym);
    doc.metadata.title = QStringLiteral("{{symbol}} — Transaction teaser");
    doc.metadata.watermark = QStringLiteral("CONFIDENTIAL");
    doc.metadata.confidentiality = QStringLiteral("Strictly private and confidential — not for distribution");
    doc.metadata.number_headings = true;
    rbt_add(doc, rbt_cover(QStringLiteral("Strictly private and confidential"),
                           QStringLiteral("This document is for discussion only and is not an offer. Market data from "
                                          "Yahoo Finance via Fincept, each block dated.")));
    rbt_add(doc, b::heading(QStringLiteral("Opportunity"), 1));
    rbt_add(doc, rbt_slot(QStringLiteral("The transaction in three sentences: what, why now, for whom.")));
    rbt_add(doc, b::heading(QStringLiteral("Target snapshot"), 1));
    rbt_add(doc, b::bound(rep::quote_binding(v)));
    rbt_add(doc, b::bound(rep::info_binding(v), QStringLiteral("Key statistics")));
    rbt_add(doc, b::heading(QStringLiteral("Financial summary"), 1));
    rbt_add(doc, b::bound(rep::financials_binding(v, QStringLiteral("income"), QStringLiteral("annual"), 4),
                          QStringLiteral("Income statement (annual)")));
    rbt_add(doc, rbt_chart(rbt_fin_chart(v, QStringLiteral("annual"), 4, {QStringLiteral("Total Revenue"), QStringLiteral("EBITDA")}),
                           QStringLiteral("Revenue and EBITDA")));
    rbt_add(doc, b::bound(rep::financials_binding(v, QStringLiteral("cashflow"), QStringLiteral("annual"), 4),
                          QStringLiteral("Cash flow (annual)")));
    rbt_add(doc, b::heading(QStringLiteral("Trading comparables"), 1));
    rbt_add(doc, b::bound(rep::peers_binding(v, 6), QStringLiteral("Comparable companies")));
    rbt_add(doc, b::heading(QStringLiteral("Valuation"), 1));
    {
        rep::ReportComponent c;
        c.type = QStringLiteral("callout");
        c.config.insert(QStringLiteral("style"), QStringLiteral("info"));
        c.config.insert(QStringLiteral("heading"), QStringLiteral("From M&A Analytics"));
        c.content = QStringLiteral("Open M&A Analytics on this target and use its Add to Report button on the football "
                                   "field and the valuation summary — they arrive here as dated snapshots. Delete this "
                                   "note afterwards.");
        rbt_add(doc, c);
    }
    rbt_add(doc, b::heading(QStringLiteral("Investment highlights"), 1));
    rbt_add(doc, rbt_slot(QStringLiteral("Four or five reasons a buyer should care, each backed by a number above.")));
    rbt_add(doc, b::heading(QStringLiteral("Process and next steps"), 1));
    rbt_add(doc, rbt_slot(QStringLiteral("Timetable, information available, contacts.")));
}

} // namespace

QVector<BuiltinTemplate> catalogue() {
    const QString live = rbt_tr("Live Data");
    const QString general = rbt_tr("General Purpose");
    const QString retail = rbt_tr("Retail Investor");
    const QString trader = rbt_tr("Trader");
    const QString inst = rbt_tr("Institutional / Analyst");
    const QString macro = rbt_tr("Economist / Macro");
    const QString crypto = rbt_tr("Crypto / Digital Assets");
    const QString fixed = rbt_tr("Fixed Income");
    const QString quant = rbt_tr("Quant / Risk");
    const QString corp = rbt_tr("Corporate / Business");
    return {
        {"Equity Research Note", live,
         rbt_tr("Quote, key statistics, 1-year chart, income and cash-flow statements, peers and news for one "
                "symbol — filled from live data, with slots for your thesis."),
         "symbol", true},
        {"Portfolio Review (Live)", live,
         rbt_tr("Performance KPIs, allocation donut and holdings table of one of your portfolios — filled from the "
                "Portfolio tab's data."),
         "portfolio", true},
        {"Weekly Market Wrap", live,
         rbt_tr("Indices, rates / FX / commodities, the day's movers, your watchlist, a 1-month index chart, "
                "headlines and this week's economic calendar — filled from live data."),
         "", true},
        {"Earnings Preview", live,
         rbt_tr("Cover, snapshot, 3-month candles, eight quarters of revenue / net income / EPS (table and bars), "
                "peers and news for one symbol, with slots for what to watch."),
         "symbol", true},
        {"Earnings Review", live,
         rbt_tr("Cover, snapshot, results and guidance slots, the price reaction, eight quarters of financials, "
                "peers and news for one symbol."),
         "symbol", true},
        {"M&A Teaser", live,
         rbt_tr("Confidential teaser for a target: snapshot, financial summary, trading comparables; the valuation "
                "comes from M&A Analytics with its Add to Report button."),
         "symbol", true},
        {"Blank Report", general, rbt_tr("Empty document with title and date — start from scratch."), "", false},
        {"Meeting Notes", general, rbt_tr("Agenda, attendees, discussion points, action items, next steps."), "", false},
        {"Investment Memo", general, rbt_tr("Executive summary, thesis, opportunity, risks, recommendation."), "", false},
        {"Stock Research", retail, rbt_tr("Company overview, financials, valuation, thesis, and risks."), "", false},
        {"Portfolio Review", retail, rbt_tr("Holdings, performance, allocation pie, risk metrics, notes."), "", false},
        {"Watchlist Report", retail, rbt_tr("Tracked tickers with price, change, target, and notes columns."), "", false},
        {"Dividend Income Report", retail, rbt_tr("Dividend stocks table, yield chart, income projection, calendar."), "",
         false},
        {"Daily Market Brief", trader, rbt_tr("Indices snapshot, top movers, news highlights, watchlist."), "", false},
        {"Trade Journal", trader, rbt_tr("Trade log table, P&L chart, win rate stats, lessons learned."), "", false},
        {"Technical Analysis", trader, rbt_tr("Price chart placeholder, indicators table, S/R levels, setup."), "", false},
        {"Pre-Market Checklist", trader, rbt_tr("Macro events, key levels, planned trades, risk per trade."), "", false},
        {"Equity Research Report", inst, rbt_tr("Full buy-side template: overview, financials, DCF, comparables."), "",
         false},
        {"Earnings Review", inst, rbt_tr("Revenue/EPS vs estimates, guidance, segment breakdown, outlook."), "", false},
        {"M&A Deal Summary", inst, rbt_tr("Deal overview, rationale, comparables, synergies, valuation."), "", false},
        {"Sector Deep Dive", inst, rbt_tr("Sector overview, sub-industries, key players, trends, risks."), "", false},
        {"Macro Economic Summary", macro, rbt_tr("GDP, inflation, unemployment, central bank policy, outlook."), "", false},
        {"Country Risk Report", macro, rbt_tr("Political, economic, financial, market risk tables and charts."), "", false},
        {"Central Bank Monitor", macro, rbt_tr("Rate decisions, forward guidance, balance sheet, implications."), "", false},
        {"Crypto Research Report", crypto, rbt_tr("Project overview, tokenomics, on-chain metrics, price chart."), "",
         false},
        {"DeFi Protocol Analysis", crypto, rbt_tr("Protocol overview, TVL, revenue, risks, token valuation."), "", false},
        {"Crypto Portfolio Review", crypto, rbt_tr("Holdings, allocation, cost basis table, P&L, risk exposure."), "",
         false},
        {"Bond Research Report", fixed, rbt_tr("Issuer overview, credit profile, yield analysis, recommendation."), "",
         false},
        {"Yield Curve Analysis", fixed, rbt_tr("Curve chart, spread table, duration/convexity, macro drivers."), "", false},
        {"Quant Strategy Report", quant, rbt_tr("Strategy description, backtest results, stats, drawdown chart."), "",
         false},
        {"Risk Management Report", quant, rbt_tr("VaR, CVaR, stress tests, correlation matrix, recommendations."), "",
         false},
        {"Business Performance", corp, rbt_tr("KPI dashboard, revenue chart, cost breakdown, targets vs actual."), "",
         false},
        {"Project Status Report", corp, rbt_tr("Objectives, milestones table, status, risks, next actions."), "", false},
        {"Financial Statement", corp, rbt_tr("Income statement, balance sheet, cash flow tables."), "", false},
    };
}

bool is_builtin(const QString& name) {
    for (const BuiltinTemplate& t : catalogue())
        if (t.name == name)
            return true;
    return false;
}

rep::ReportDocument build(const QString& name, const rep::ReportTheme& theme, const QVariantMap& params) {
    rep::ReportDocument doc;
    doc.metadata.date = QDateTime::currentDateTime().toString("yyyy-MM-dd");
    // The theme is a look the user picked, not template content: a fresh ReportDocument
    // carries Light Professional, so applying any template used to silently reset a
    // Dark Corporate / Midnight Blue report back to white.
    doc.theme = theme;

    auto add = [&](const QString& type, const QString& content = {}, const QMap<QString, QString>& cfg = {}) {
        rep::ReportComponent c;
        c.id = doc.allocate_id();
        c.type = type;
        c.content = content;
        c.config = cfg;
        doc.components.append(c);
    };

    // ── Live data ─────────────────────────────────────────────────────────────

    if (name == "Equity Research Note") {
        rbt_equity_note(doc, params.value(QStringLiteral("symbol")).toString());
    } else if (name == "Portfolio Review (Live)") {
        rbt_portfolio_review(doc, params.value(QStringLiteral("portfolio_id")).toString());
    } else if (name == "Weekly Market Wrap") {
        rbt_weekly_wrap(doc);
    } else if (name == "Earnings Preview") {
        rbt_earnings(doc, params.value(QStringLiteral("symbol")).toString(), /*review=*/false);
    } else if (name == "Earnings Review") {
        rbt_earnings(doc, params.value(QStringLiteral("symbol")).toString(), /*review=*/true);
    } else if (name == "M&A Teaser") {
        rbt_ma_teaser(doc, params.value(QStringLiteral("symbol")).toString());

    // ── General Purpose ───────────────────────────────────────────────────────

    } else if (name == "Blank Report") {
        doc.metadata.title = "Untitled Report";
        add("heading", "Title");
        add("text", "Start writing here...");

    } else if (name == "Meeting Notes") {
        doc.metadata.title = "Meeting Notes";
        add("toc");
        add("heading", "Meeting Details");
        add("table", {}, {{"rows", "4"}, {"cols", "2"}});
        add("heading", "Attendees");
        add("list", "Name — Role\nName — Role\nName — Role");
        add("heading", "Agenda");
        add("list", "Item 1\nItem 2\nItem 3");
        add("divider");
        add("heading", "Discussion Points");
        add("text", "Summarise key discussion points here.");
        add("heading", "Decisions Made");
        add("list", "Decision 1\nDecision 2");
        add("heading", "Action Items");
        add("table", {}, {{"rows", "5"}, {"cols", "3"}});
        add("heading", "Next Meeting");
        add("text", "Date / time / agenda for next meeting.");

    } else if (name == "Investment Memo") {
        doc.metadata.title = "Investment Memo";
        add("toc");
        add("heading", "Executive Summary");
        add("text", "One-paragraph summary of the investment opportunity and recommendation.");
        add("heading", "Opportunity");
        add("text", "Describe the investment opportunity, market context, and why now.");
        add("heading", "Investment Thesis");
        add("list", "Thesis point 1\nThesis point 2\nThesis point 3");
        add("heading", "Financial Snapshot");
        add("table", {}, {{"rows", "5"}, {"cols", "3"}});
        add("heading", "Valuation");
        add("chart", {},
            {{"chart_type", "bar"},
             {"title", "Valuation Scenarios"},
             {"data", "80,100,130"},
             {"labels", "Bear,Base,Bull"}});
        add("heading", "Key Risks");
        add("list", "Risk 1\nRisk 2\nRisk 3");
        add("heading", "Recommendation");
        add("text", "State your recommendation and conviction level.");
        add("divider");
        add("quote", "This memo is for internal use only. Not investment advice.");

    } else if (name == "Stock Research") {
        doc.metadata.title = "Stock Research";
        add("toc");
        add("heading", "Company Overview");
        add("text", "Describe the company, its business model, products/services, and competitive position.");
        add("market_data", {}, {{"symbol", "AAPL"}});
        add("heading", "Financial Highlights");
        add("table", {}, {{"rows", "6"}, {"cols", "4"}});
        add("heading", "Earnings History");
        add("chart", {},
            {{"chart_type", "bar"},
             {"title", "Revenue vs Net Income"},
             {"data", "120,130,145,160"},
             {"labels", "FY21,FY22,FY23,FY24"}});
        add("heading", "Valuation Metrics");
        add("table", {}, {{"rows", "5"}, {"cols", "3"}});
        add("heading", "Investment Thesis");
        add("text", "Why you are considering this stock.");
        add("heading", "Risks");
        add("list", "Regulatory\nCompetition\nMacro headwinds\nExecution risk");
        add("heading", "Price Target");
        add("text", "Your price target and timeline.");
        add("divider");
        add("quote", "For personal research purposes only. Not financial advice.");

    } else if (name == "Portfolio Review") {
        doc.metadata.title = "Portfolio Review";
        add("toc");
        add("heading", "Holdings Summary");
        add("table", {}, {{"rows", "8"}, {"cols", "6"}});
        add("heading", "Portfolio Performance");
        add("chart", {},
            {{"chart_type", "line"},
             {"title", "Portfolio Value"},
             {"data", "10000,10500,9800,11200,12000,11500,13000"}});
        add("heading", "Asset Allocation");
        add("chart", {},
            {{"chart_type", "pie"},
             {"title", "Allocation"},
             {"data", "40,30,20,10"},
             {"labels", "Equities,Fixed Income,Crypto,Cash"}});
        add("heading", "Top Performers");
        add("table", {}, {{"rows", "4"}, {"cols", "3"}});
        add("heading", "Underperformers");
        add("table", {}, {{"rows", "4"}, {"cols", "3"}});
        add("heading", "Risk Metrics");
        add("table", {}, {{"rows", "5"}, {"cols", "2"}});
        add("heading", "Notes & Next Steps");
        add("text", "Commentary on portfolio health and planned changes.");

    } else if (name == "Watchlist Report") {
        doc.metadata.title = "Watchlist Report";
        add("heading", "Watchlist Snapshot");
        add("market_data", {}, {{"symbol", "AAPL"}});
        add("market_data", {}, {{"symbol", "MSFT"}});
        add("market_data", {}, {{"symbol", "GOOGL"}});
        add("market_data", {}, {{"symbol", "AMZN"}});
        add("market_data", {}, {{"symbol", "TSLA"}});
        add("divider");
        add("heading", "Watchlist Detail");
        add("table", {}, {{"rows", "8"}, {"cols", "6"}});
        add("heading", "Observations");
        add("text", "Key observations and setup notes for tracked names.");

    } else if (name == "Dividend Income Report") {
        doc.metadata.title = "Dividend Income Report";
        add("toc");
        add("heading", "Dividend Holdings");
        add("table", {}, {{"rows", "8"}, {"cols", "6"}});
        add("heading", "Annual Income Projection");
        add("chart", {},
            {{"chart_type", "bar"},
             {"title", "Monthly Dividend Income"},
             {"data", "200,180,220,210,195,230,215,240,200,220,210,250"},
             {"labels", "Jan,Feb,Mar,Apr,May,Jun,Jul,Aug,Sep,Oct,Nov,Dec"}});
        add("heading", "Yield by Holding");
        add("chart", {},
            {{"chart_type", "pie"},
             {"title", "Income Contribution"},
             {"data", "30,25,20,15,10"},
             {"labels", "VZ,T,PFE,KO,MO"}});
        add("heading", "Dividend Calendar");
        add("table", {}, {{"rows", "8"}, {"cols", "4"}});
        add("heading", "Growth Targets");
        add("text", "Target yield, DRIP strategy, and reinvestment notes.");

    } else if (name == "Daily Market Brief") {
        doc.metadata.title = "Daily Market Brief";
        add("heading", "Market Overview");
        add("market_data", {}, {{"symbol", "^GSPC"}});
        add("market_data", {}, {{"symbol", "^DJI"}});
        add("market_data", {}, {{"symbol", "^IXIC"}});
        add("market_data", {}, {{"symbol", "^VIX"}});
        add("market_data", {}, {{"symbol", "DX-Y.NYB"}});
        add("divider");
        add("heading", "Sector Performance");
        add("chart", {},
            {{"chart_type", "bar"},
             {"title", "Sector Returns Today"},
             {"data", "0.5,-0.3,1.2,-0.8,0.4,0.9,-0.2,1.1,0.3,-0.5,0.7"},
             {"labels", "XLK,XLF,XLV,XLE,XLI,XLY,XLP,XLC,XLB,XLU,XLRE"}});
        add("heading", "Top Movers");
        add("table", {}, {{"rows", "7"}, {"cols", "4"}});
        add("heading", "Key Events & News");
        add("text", "Enter today's key market events, earnings, and macro news.");
        add("heading", "Watchlist Levels");
        add("table", {}, {{"rows", "6"}, {"cols", "5"}});
        add("heading", "Commentary");
        add("text", "Market tone, breadth observations, and outlook.");

    } else if (name == "Trade Journal") {
        doc.metadata.title = "Trade Journal";
        add("toc");
        add("heading", "Trade Log");
        add("table", {}, {{"rows", "10"}, {"cols", "8"}});
        add("heading", "P&L Summary");
        add("chart", {},
            {{"chart_type", "line"}, {"title", "Cumulative P&L"}, {"data", "0,150,-80,320,210,480,390,620"}});
        add("heading", "Statistics");
        add("table", {}, {{"rows", "7"}, {"cols", "2"}});
        add("heading", "Win/Loss Breakdown");
        add("chart", {},
            {{"chart_type", "pie"}, {"title", "Win vs Loss"}, {"data", "62,38"}, {"labels", "Wins,Losses"}});
        add("heading", "Best Trades");
        add("table", {}, {{"rows", "4"}, {"cols", "5"}});
        add("heading", "Worst Trades");
        add("table", {}, {{"rows", "4"}, {"cols", "5"}});
        add("heading", "Lessons Learned");
        add("text", "What worked, what didn't, and adjustments for next session.");
        add("heading", "Rules Checklist");
        add("list", "Followed entry rules\nRespected stop loss\nDid not revenge trade\nPositioned sized "
                    "correctly\nLogged all trades");

    } else if (name == "Technical Analysis") {
        doc.metadata.title = "Technical Analysis";
        add("heading", "Instrument");
        add("market_data", {}, {{"symbol", "AAPL"}});
        add("heading", "Price Chart");
        add("image", {}, {{"width", "700"}, {"caption", "Daily chart — add screenshot here"}, {"align", "center"}});
        add("heading", "Key Levels");
        add("table", {}, {{"rows", "6"}, {"cols", "3"}});
        add("heading", "Indicators");
        add("table", {}, {{"rows", "6"}, {"cols", "3"}});
        add("heading", "Pattern / Setup");
        add("text", "Describe the chart pattern or setup and the rationale.");
        add("heading", "Trade Plan");
        add("table", {}, {{"rows", "4"}, {"cols", "3"}});
        add("heading", "Bias");
        add("quote", "Bullish / Bearish / Neutral — state your directional bias and timeframe.");

    } else if (name == "Pre-Market Checklist") {
        doc.metadata.title = "Pre-Market Checklist";
        add("heading", "Overnight Summary");
        add("market_data", {}, {{"symbol", "ES=F"}});
        add("market_data", {}, {{"symbol", "NQ=F"}});
        add("market_data", {}, {{"symbol", "CL=F"}});
        add("market_data", {}, {{"symbol", "GC=F"}});
        add("divider");
        add("heading", "Economic Calendar");
        add("table", {}, {{"rows", "5"}, {"cols", "4"}});
        add("heading", "Earnings Today");
        add("table", {}, {{"rows", "5"}, {"cols", "3"}});
        add("heading", "Key Levels to Watch");
        add("table", {}, {{"rows", "5"}, {"cols", "4"}});
        add("heading", "Planned Trades");
        add("table", {}, {{"rows", "4"}, {"cols", "5"}});
        add("heading", "Risk Parameters");
        add("table", {}, {{"rows", "4"}, {"cols", "2"}});
        add("heading", "Mental Checklist");
        add("list",
            "Reviewed overnight news\nIdentified key levels\nSet max daily loss\nNo FOMO trades\nLog every trade");

    } else if (name == "Equity Research Report") {
        doc.metadata.title = "Equity Research Report";
        add("toc");
        add("heading", "Company Overview");
        add("text", "Business description, history, products/services, and competitive moat.");
        add("market_data", {}, {{"symbol", "AAPL"}});
        add("heading", "Industry & Market");
        add("text", "Industry dynamics, total addressable market, and competitive landscape.");
        add("heading", "Financial Analysis");
        add("table", {}, {{"rows", "8"}, {"cols", "5"}});
        add("chart", {},
            {{"chart_type", "bar"},
             {"title", "Revenue & EPS Growth"},
             {"data", "100,115,130,148,169"},
             {"labels", "FY20,FY21,FY22,FY23,FY24"}});
        add("heading", "Valuation");
        add("table", {}, {{"rows", "5"}, {"cols", "4"}});
        add("chart", {},
            {{"chart_type", "bar"},
             {"title", "Valuation vs Peers"},
             {"data", "22,18,25,19,28"},
             {"labels", "AAPL,MSFT,GOOGL,META,AMZN"}});
        add("heading", "Investment Thesis");
        add("list", "Catalyst 1\nCatalyst 2\nCatalyst 3");
        add("heading", "Risks");
        add("list", "Regulatory risk\nCompetitive pressure\nMacro sensitivity\nExecution risk");
        add("heading", "Rating & Price Target");
        add("table", {}, {{"rows", "3"}, {"cols", "3"}});
        add("divider");
        add("quote", "This report is for informational purposes only. Not investment advice.");

    } else if (name == "Earnings Review") {
        doc.metadata.title = "Earnings Review";
        add("market_data", {}, {{"symbol", "AAPL"}});
        add("heading", "Results vs Estimates");
        add("table", {}, {{"rows", "5"}, {"cols", "4"}});
        add("heading", "Revenue Breakdown");
        add("chart", {},
            {{"chart_type", "bar"},
             {"title", "Revenue by Segment"},
             {"data", "42,18,10,8,6"},
             {"labels", "iPhone,Services,Mac,iPad,Wearables"}});
        add("heading", "Margin Analysis");
        add("table", {}, {{"rows", "4"}, {"cols", "3"}});
        add("heading", "Guidance");
        add("table", {}, {{"rows", "4"}, {"cols", "4"}});
        add("heading", "Management Commentary");
        add("text", "Key quotes and commentary from the earnings call.");
        add("heading", "Our Take");
        add("text", "Analysis of results relative to expectations and impact on thesis.");
        add("heading", "Revised Estimates");
        add("table", {}, {{"rows", "4"}, {"cols", "3"}});

    } else if (name == "M&A Deal Summary") {
        doc.metadata.title = "M&A Deal Summary";
        add("toc");
        add("heading", "Deal Overview");
        add("table", {}, {{"rows", "6"}, {"cols", "2"}});
        add("heading", "Strategic Rationale");
        add("list", "Strategic fit\nMarket expansion\nSynergy opportunity\nDefensive rationale");
        add("heading", "Comparable Transactions");
        add("table", {}, {{"rows", "7"}, {"cols", "5"}});
        add("heading", "Synergy Analysis");
        add("chart", {},
            {{"chart_type", "bar"},
             {"title", "Synergy Build-Up ($M)"},
             {"data", "50,120,200,250"},
             {"labels", "Yr1,Yr2,Yr3,Yr4"}});
        add("heading", "Valuation Bridge");
        add("table", {}, {{"rows", "5"}, {"cols", "3"}});
        add("heading", "Key Risks");
        add("list", "Integration risk\nRegulatory clearance\nCulture clash\nPremium paid\nFinancing risk");
        add("heading", "Timeline");
        add("table", {}, {{"rows", "5"}, {"cols", "3"}});
        add("quote", "Deal subject to regulatory approval and shareholder vote.");

    } else if (name == "Sector Deep Dive") {
        doc.metadata.title = "Sector Deep Dive";
        add("toc");
        add("heading", "Sector Overview");
        add("text", "Define the sector, key sub-industries, and economic significance.");
        add("heading", "Market Size & Growth");
        add("chart", {},
            {{"chart_type", "bar"},
             {"title", "Market Size ($B)"},
             {"data", "800,900,1020,1160,1320"},
             {"labels", "2020,2021,2022,2023,2024E"}});
        add("heading", "Key Players");
        add("table", {}, {{"rows", "8"}, {"cols", "4"}});
        add("heading", "Competitive Dynamics");
        add("text", "Porter's five forces or similar competitive analysis framework.");
        add("heading", "Structural Trends");
        add("list", "Trend 1\nTrend 2\nTrend 3\nTrend 4");
        add("heading", "Valuation Comparison");
        add("chart", {},
            {{"chart_type", "bar"},
             {"title", "P/E Ratio by Company"},
             {"data", "22,19,25,17,30"},
             {"labels", "Co1,Co2,Co3,Co4,Co5"}});
        add("heading", "Risks & Headwinds");
        add("list", "Regulatory pressure\nTechnology disruption\nCyclicality\nGeopolitical exposure");
        add("heading", "Investment Opportunities");
        add("text", "Best positioned names and sub-themes to play.");

    } else if (name == "Macro Economic Summary") {
        doc.metadata.title = "Macro Economic Summary";
        add("toc");
        add("heading", "Global Snapshot");
        add("table", {}, {{"rows", "6"}, {"cols", "4"}});
        add("heading", "GDP Growth");
        add("chart", {},
            {{"chart_type", "line"},
             {"title", "GDP Growth Rate (%)"},
             {"data", "2.1,2.3,1.8,2.5,2.2,1.9,2.4"},
             {"labels", "Q1 23,Q2 23,Q3 23,Q4 23,Q1 24,Q2 24,Q3 24"}});
        add("heading", "Inflation");
        add("chart", {},
            {{"chart_type", "line"},
             {"title", "CPI YoY (%)"},
             {"data", "6.0,5.2,4.3,3.7,3.2,2.9,2.6"},
             {"labels", "Q1 23,Q2 23,Q3 23,Q4 23,Q1 24,Q2 24,Q3 24"}});
        add("heading", "Labour Market");
        add("table", {}, {{"rows", "4"}, {"cols", "3"}});
        add("heading", "Central Bank Policy");
        add("table", {}, {{"rows", "6"}, {"cols", "4"}});
        add("heading", "Key Risks to Outlook");
        add("list",
            "Geopolitical escalation\nCredit tightening\nChina slowdown\nEnergy price shock\nElection uncertainty");
        add("heading", "Outlook");
        add("text", "Macro outlook summary and base-case scenario for the next 6–12 months.");

    } else if (name == "Country Risk Report") {
        doc.metadata.title = "Country Risk Report";
        add("toc");
        add("heading", "Country Overview");
        add("table", {}, {{"rows", "6"}, {"cols", "2"}});
        add("heading", "Political Risk");
        add("text", "Government stability, rule of law, corruption index, and geopolitical exposure.");
        add("heading", "Economic Risk");
        add("table", {}, {{"rows", "6"}, {"cols", "3"}});
        add("chart", {},
            {{"chart_type", "line"},
             {"title", "GDP Growth vs Inflation"},
             {"data", "3.0,3.4,2.8,1.9,2.5"},
             {"labels", "2020,2021,2022,2023,2024E"}});
        add("heading", "Financial Risk");
        add("table", {}, {{"rows", "5"}, {"cols", "3"}});
        add("heading", "Market Risk");
        add("table", {}, {{"rows", "4"}, {"cols", "3"}});
        add("heading", "Overall Risk Rating");
        add("table", {}, {{"rows", "5"}, {"cols", "3"}});
        add("heading", "Outlook");
        add("text", "Summary of country risk outlook and key watchpoints.");

    } else if (name == "Central Bank Monitor") {
        doc.metadata.title = "Central Bank Monitor";
        add("heading", "Rate Decisions");
        add("table", {}, {{"rows", "7"}, {"cols", "4"}});
        add("heading", "Rate Path");
        add("chart", {},
            {{"chart_type", "line"},
             {"title", "Policy Rate (%)"},
             {"data", "0.25,0.5,1.0,2.0,3.5,4.5,5.25,5.25,5.0"},
             {"labels", "Jan22,Mar22,Jun22,Sep22,Dec22,Mar23,Jun23,Sep23,Dec23"}});
        add("heading", "Forward Guidance Summary");
        add("text", "Summarise the forward guidance from major central banks.");
        add("heading", "Balance Sheet");
        add("chart", {},
            {{"chart_type", "bar"},
             {"title", "Balance Sheet Size ($T)"},
             {"data", "8.9,8.5,8.1,7.8,7.4"},
             {"labels", "Q4 22,Q1 23,Q2 23,Q3 23,Q4 23"}});
        add("heading", "Market Implications");
        add("text", "Impact on equities, bonds, FX, and commodities.");
        add("heading", "Key Upcoming Events");
        add("table", {}, {{"rows", "5"}, {"cols", "3"}});

    } else if (name == "Crypto Research Report") {
        doc.metadata.title = "Crypto Research Report";
        add("toc");
        add("heading", "Project Overview");
        add("text", "Describe the project, its purpose, technology, and value proposition.");
        add("market_data", {}, {{"symbol", "BTC-USD"}});
        add("heading", "Tokenomics");
        add("table", {}, {{"rows", "7"}, {"cols", "2"}});
        add("chart", {},
            {{"chart_type", "pie"},
             {"title", "Token Distribution"},
             {"data", "20,15,30,25,10"},
             {"labels", "Team,Investors,Community,Treasury,Public"}});
        add("heading", "On-Chain Metrics");
        add("table", {}, {{"rows", "6"}, {"cols", "3"}});
        add("heading", "Price History");
        add("chart", {},
            {{"chart_type", "line"},
             {"title", "Price (USD)"},
             {"data", "16000,20000,25000,30000,28000,42000,38000,52000"}});
        add("heading", "Competitive Landscape");
        add("text", "Key competitors and differentiation.");
        add("heading", "Risks");
        add("list",
            "Regulatory risk\nSmart contract bugs\nCompetitor displacement\nLiquidity risk\nTeam/execution risk");
        add("heading", "Investment Thesis");
        add("text", "Thesis, catalysts, and price targets.");
        add("quote", "Crypto assets are highly volatile. This is not financial advice.");

    } else if (name == "DeFi Protocol Analysis") {
        doc.metadata.title = "DeFi Protocol Analysis";
        add("toc");
        add("heading", "Protocol Overview");
        add("text", "What the protocol does, how it works, and what problem it solves.");
        add("heading", "Key Metrics");
        add("table", {}, {{"rows", "7"}, {"cols", "3"}});
        add("heading", "TVL History");
        add("chart", {},
            {{"chart_type", "line"},
             {"title", "Total Value Locked ($M)"},
             {"data", "500,800,1200,900,1500,2000,1800,2400"}});
        add("heading", "Revenue & Fees");
        add("chart", {},
            {{"chart_type", "bar"},
             {"title", "Monthly Protocol Revenue ($K)"},
             {"data", "120,150,180,140,200,230,210,250"}});
        add("heading", "Governance & Token");
        add("text", "Token utility, governance rights, staking mechanics, and emission schedule.");
        add("heading", "Risks");
        add("list", "Smart contract exploit\nOracle manipulation\nRegulatory action\nLiquidity "
                    "fragmentation\nFork/governance risk");
        add("heading", "Valuation");
        add("text", "P/S, P/TVL, or other relevant DeFi valuation metrics and comparables.");

    } else if (name == "Crypto Portfolio Review") {
        doc.metadata.title = "Crypto Portfolio Review";
        add("heading", "Holdings");
        add("table", {}, {{"rows", "8"}, {"cols", "6"}});
        add("heading", "Current Prices");
        add("market_data", {}, {{"symbol", "BTC-USD"}});
        add("market_data", {}, {{"symbol", "ETH-USD"}});
        add("market_data", {}, {{"symbol", "SOL-USD"}});
        add("heading", "Allocation");
        add("chart", {},
            {{"chart_type", "pie"},
             {"title", "Portfolio Allocation"},
             {"data", "50,25,15,10"},
             {"labels", "BTC,ETH,SOL,Other"}});
        add("heading", "Performance");
        add("chart", {},
            {{"chart_type", "line"},
             {"title", "Portfolio Value (USD)"},
             {"data", "10000,9500,12000,11000,15000,13000,18000"}});
        add("heading", "Notes");
        add("text", "Rebalancing notes, conviction levels, and next moves.");

    } else if (name == "Bond Research Report") {
        doc.metadata.title = "Bond Research Report";
        add("toc");
        add("heading", "Issuer Overview");
        add("text", "Issuer description, business profile, and credit history.");
        add("heading", "Bond Details");
        add("table", {}, {{"rows", "8"}, {"cols", "2"}});
        add("heading", "Credit Profile");
        add("table", {}, {{"rows", "5"}, {"cols", "4"}});
        add("heading", "Yield Analysis");
        add("chart", {},
            {{"chart_type", "line"},
             {"title", "Yield vs Benchmark"},
             {"data", "3.0,3.2,3.5,3.8,4.1,4.3,4.0"},
             {"labels", "Jan,Feb,Mar,Apr,May,Jun,Jul"}});
        add("heading", "Duration & Convexity");
        add("table", {}, {{"rows", "4"}, {"cols", "2"}});
        add("heading", "Comparable Bonds");
        add("table", {}, {{"rows", "5"}, {"cols", "5"}});
        add("heading", "Recommendation");
        add("text", "Buy / Hold / Sell with rationale and target spread.");

    } else if (name == "Yield Curve Analysis") {
        doc.metadata.title = "Yield Curve Analysis";
        add("heading", "Yield Curve Shape");
        add("chart", {},
            {{"chart_type", "line"},
             {"title", "US Treasury Yield Curve"},
             {"data", "5.3,5.1,4.9,4.7,4.6,4.5,4.4,4.3"},
             {"labels", "3M,6M,1Y,2Y,3Y,5Y,10Y,30Y"}});
        add("heading", "Key Spreads");
        add("table", {}, {{"rows", "5"}, {"cols", "3"}});
        add("heading", "Historical Comparison");
        add("chart", {},
            {{"chart_type", "bar"},
             {"title", "10Y-2Y Spread (bps)"},
             {"data", "-80,-60,-40,-20,5,15,30"},
             {"labels", "Oct23,Nov23,Dec23,Jan24,Feb24,Mar24,Apr24"}});
        add("heading", "Duration & Convexity");
        add("table", {}, {{"rows", "4"}, {"cols", "3"}});
        add("heading", "Macro Drivers");
        add("text", "Explain the key macro factors driving current curve shape.");
        add("heading", "Implications");
        add("list",
            "Recession signal interpretation\nFed policy implications\nSector rotation impact\nCurrency effects");

    } else if (name == "Quant Strategy Report") {
        doc.metadata.title = "Quant Strategy Report";
        add("toc");
        add("heading", "Strategy Description");
        add("text", "Describe the strategy, signal generation logic, universe, and execution rules.");
        add("heading", "Backtest Parameters");
        add("table", {}, {{"rows", "6"}, {"cols", "2"}});
        add("heading", "Performance Summary");
        add("table", {}, {{"rows", "8"}, {"cols", "2"}});
        add("heading", "Equity Curve");
        add("chart", {},
            {{"chart_type", "line"},
             {"title", "Strategy vs Benchmark"},
             {"data", "100,108,115,112,124,131,128,142,150"},
             {"labels", "Jan,Feb,Mar,Apr,May,Jun,Jul,Aug,Sep"}});
        add("heading", "Drawdown Analysis");
        add("chart", {},
            {{"chart_type", "bar"},
             {"title", "Monthly Drawdowns (%)"},
             {"data", "0,-2.1,-0.5,-3.2,-1.1,0,-1.8,-0.4,0"}});
        add("heading", "Factor Exposure");
        add("table", {}, {{"rows", "6"}, {"cols", "3"}});
        add("heading", "Stress Tests");
        add("table", {}, {{"rows", "5"}, {"cols", "3"}});
        add("heading", "Limitations & Risks");
        add("list",
            "Overfitting risk\nTransaction cost sensitivity\nRegime change\nCapacity constraints\nData snooping bias");

    } else if (name == "Risk Management Report") {
        doc.metadata.title = "Risk Management Report";
        add("toc");
        add("heading", "Portfolio Risk Summary");
        add("table", {}, {{"rows", "6"}, {"cols", "2"}});
        add("heading", "VaR Analysis");
        add("chart", {},
            {{"chart_type", "bar"},
             {"title", "VaR by Confidence Level"},
             {"data", "1.2,1.8,2.5,3.1"},
             {"labels", "90%,95%,99%,99.9%"}});
        add("heading", "Stress Test Results");
        add("table", {}, {{"rows", "7"}, {"cols", "3"}});
        add("heading", "Correlation Matrix");
        add("text", "Paste or describe key pairwise correlations between holdings.");
        add("heading", "Concentration Risk");
        add("chart", {},
            {{"chart_type", "pie"},
             {"title", "Position Concentration"},
             {"data", "25,20,18,15,12,10"},
             {"labels", "Pos1,Pos2,Pos3,Pos4,Pos5,Other"}});
        add("heading", "Liquidity Risk");
        add("table", {}, {{"rows", "5"}, {"cols", "3"}});
        add("heading", "Recommendations");
        add("list", "Reduce concentration in top 3 positions\nAdd hedges for macro exposure\nReview stop-loss "
                    "levels\nRebalance sector exposure");

    } else if (name == "Business Performance") {
        doc.metadata.title = "Business Performance Report";
        add("toc");
        add("heading", "Executive Summary");
        add("text", "One-paragraph summary of business performance this period.");
        add("heading", "Key Performance Indicators");
        add("table", {}, {{"rows", "8"}, {"cols", "4"}});
        add("heading", "Revenue");
        add("chart", {},
            {{"chart_type", "bar"},
             {"title", "Revenue ($M)"},
             {"data", "42,45,48,44,51,53,50,55"},
             {"labels", "Q1 22,Q2 22,Q3 22,Q4 22,Q1 23,Q2 23,Q3 23,Q4 23"}});
        add("heading", "Cost Breakdown");
        add("chart", {},
            {{"chart_type", "pie"},
             {"title", "Cost Structure"},
             {"data", "40,25,20,15"},
             {"labels", "COGS,R&D,S&M,G&A"}});
        add("heading", "Department Performance");
        add("table", {}, {{"rows", "6"}, {"cols", "4"}});
        add("heading", "Targets vs Actuals");
        add("chart", {},
            {{"chart_type", "bar"},
             {"title", "Target vs Actual"},
             {"data", "100,95,100,102,100,88"},
             {"labels", "Jan,Feb,Mar,Apr,May,Jun"}});
        add("heading", "Issues & Risks");
        add("list", "Issue 1\nIssue 2\nIssue 3");
        add("heading", "Next Period Targets");
        add("table", {}, {{"rows", "5"}, {"cols", "3"}});

    } else if (name == "Project Status Report") {
        doc.metadata.title = "Project Status Report";
        add("heading", "Project Overview");
        add("table", {}, {{"rows", "5"}, {"cols", "2"}});
        add("heading", "Overall Status");
        add("table", {}, {{"rows", "4"}, {"cols", "3"}});
        add("heading", "Milestones");
        add("table", {}, {{"rows", "7"}, {"cols", "4"}});
        add("heading", "Progress This Period");
        add("text", "Summarise completed work and progress against plan.");
        add("heading", "Issues & Blockers");
        add("table", {}, {{"rows", "5"}, {"cols", "4"}});
        add("heading", "Risks");
        add("table", {}, {{"rows", "5"}, {"cols", "4"}});
        add("heading", "Next Steps");
        add("list", "Action 1 — Owner — Due date\nAction 2 — Owner — Due date\nAction 3 — Owner — Due date");

    } else if (name == "Financial Statement") {
        doc.metadata.title = "Financial Statement";
        add("toc");
        add("heading", "Income Statement");
        add("table", {}, {{"rows", "12"}, {"cols", "4"}});
        add("chart", {},
            {{"chart_type", "bar"},
             {"title", "Revenue & Net Income"},
             {"data", "400,450,500,420,480,530"},
             {"labels", "H1 22,H2 22,H1 23,H2 23,H1 24,H2 24"}});
        add("page_break");
        add("heading", "Balance Sheet");
        add("table", {}, {{"rows", "14"}, {"cols", "3"}});
        add("page_break");
        add("heading", "Cash Flow Statement");
        add("table", {}, {{"rows", "12"}, {"cols", "3"}});
        add("heading", "Key Ratios");
        add("table", {}, {{"rows", "8"}, {"cols", "3"}});
        add("divider");
        add("quote", "Prepared in accordance with applicable accounting standards.");
    }

    // Every chart above carries made-up placeholder numbers. Flag them so the canvas
    // (and the PDF/HTML export) labels them SAMPLE DATA until the user replaces the
    // data — they used to print as if they were the company's real figures.
    for (auto& c : doc.components) {
        if (c.type == QLatin1String("chart") && !c.config.value("data").isEmpty())
            c.config["sample"] = "1";
    }
    return doc;
}

} // namespace fincept::services::report_templates
