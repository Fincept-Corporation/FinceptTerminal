// ReportBuilderTemplates.cpp — body of ReportBuilderService::apply_template().
//
// Kept in a separate translation unit because the template catalog is
// ~600 lines of literal data and would dominate the service file.

#include "core/report/ReportDocument.h"
#include "services/report_builder/ReportBuilderService.h"

#include <QDateTime>

namespace fincept::services {

namespace rep = report;

void apply_template_to_service(ReportBuilderService* svc, const QString& name) {
    // Build the new document offline, then atomically swap it onto the
    // service via replace_document(). This avoids a flurry of fine-grained
    // signals during template application — the screen just gets one
    // document_changed and re-renders once.

    rep::ReportDocument doc;
    doc.metadata.date = QDateTime::currentDateTime().toString("yyyy-MM-dd");

    auto add = [&](const QString& type, const QString& content = {}, const QMap<QString, QString>& cfg = {}) {
        rep::ReportComponent c;
        c.id = doc.allocate_id();
        c.type = type;
        c.content = content;
        c.config = cfg;
        doc.components.append(c);
    };

    // ── General Purpose ───────────────────────────────────────────────────────

    if (name == "Blank Report") {
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
            {{"chart_type", "line"}, {"title", "Portfolio Value"},
             {"data", "10000,10500,9800,11200,12000,11500,13000"}});
        add("heading", "Asset Allocation");
        add("chart", {},
            {{"chart_type", "pie"}, {"title", "Allocation"},
             {"data", "40,30,20,10"}, {"labels", "Equities,Fixed Income,Crypto,Cash"}});
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
            {{"chart_type", "bar"}, {"title", "Monthly Dividend Income"},
             {"data", "200,180,220,210,195,230,215,240,200,220,210,250"},
             {"labels", "Jan,Feb,Mar,Apr,May,Jun,Jul,Aug,Sep,Oct,Nov,Dec"}});
        add("heading", "Yield by Holding");
        add("chart", {},
            {{"chart_type", "pie"}, {"title", "Income Contribution"},
             {"data", "30,25,20,15,10"}, {"labels", "VZ,T,PFE,KO,MO"}});
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
            {{"chart_type", "bar"}, {"title", "Sector Returns Today"},
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
            {{"chart_type", "line"}, {"title", "Cumulative P&L"},
             {"data", "0,150,-80,320,210,480,390,620"}});
        add("heading", "Statistics");
        add("table", {}, {{"rows", "7"}, {"cols", "2"}});
        add("heading", "Win/Loss Breakdown");
        add("chart", {},
            {{"chart_type", "pie"}, {"title", "Win vs Loss"},
             {"data", "62,38"}, {"labels", "Wins,Losses"}});
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
            {{"chart_type", "bar"}, {"title", "Revenue & EPS Growth"},
             {"data", "100,115,130,148,169"}, {"labels", "FY20,FY21,FY22,FY23,FY24"}});
        add("heading", "Valuation");
        add("table", {}, {{"rows", "5"}, {"cols", "4"}});
        add("chart", {},
            {{"chart_type", "bar"}, {"title", "Valuation vs Peers"},
             {"data", "22,18,25,19,28"}, {"labels", "AAPL,MSFT,GOOGL,META,AMZN"}});
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
            {{"chart_type", "bar"}, {"title", "Revenue by Segment"},
             {"data", "42,18,10,8,6"}, {"labels", "iPhone,Services,Mac,iPad,Wearables"}});
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
            {{"chart_type", "bar"}, {"title", "Synergy Build-Up ($M)"},
             {"data", "50,120,200,250"}, {"labels", "Yr1,Yr2,Yr3,Yr4"}});
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
            {{"chart_type", "bar"}, {"title", "Market Size ($B)"},
             {"data", "800,900,1020,1160,1320"}, {"labels", "2020,2021,2022,2023,2024E"}});
        add("heading", "Key Players");
        add("table", {}, {{"rows", "8"}, {"cols", "4"}});
        add("heading", "Competitive Dynamics");
        add("text", "Porter's five forces or similar competitive analysis framework.");
        add("heading", "Structural Trends");
        add("list", "Trend 1\nTrend 2\nTrend 3\nTrend 4");
        add("heading", "Valuation Comparison");
        add("chart", {},
            {{"chart_type", "bar"}, {"title", "P/E Ratio by Company"},
             {"data", "22,19,25,17,30"}, {"labels", "Co1,Co2,Co3,Co4,Co5"}});
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
            {{"chart_type", "line"}, {"title", "GDP Growth Rate (%)"},
             {"data", "2.1,2.3,1.8,2.5,2.2,1.9,2.4"},
             {"labels", "Q1 23,Q2 23,Q3 23,Q4 23,Q1 24,Q2 24,Q3 24"}});
        add("heading", "Inflation");
        add("chart", {},
            {{"chart_type", "line"}, {"title", "CPI YoY (%)"},
             {"data", "6.0,5.2,4.3,3.7,3.2,2.9,2.6"},
             {"labels", "Q1 23,Q2 23,Q3 23,Q4 23,Q1 24,Q2 24,Q3 24"}});
        add("heading", "Labour Market");
        add("table", {}, {{"rows", "4"}, {"cols", "3"}});
        add("heading", "Central Bank Policy");
        add("table", {}, {{"rows", "6"}, {"cols", "4"}});
        add("heading", "Key Risks to Outlook");
        add("list", "Geopolitical escalation\nCredit tightening\nChina slowdown\nEnergy price shock\nElection uncertainty");
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
            {{"chart_type", "line"}, {"title", "GDP Growth vs Inflation"},
             {"data", "3.0,3.4,2.8,1.9,2.5"}, {"labels", "2020,2021,2022,2023,2024E"}});
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
            {{"chart_type", "line"}, {"title", "Policy Rate (%)"},
             {"data", "0.25,0.5,1.0,2.0,3.5,4.5,5.25,5.25,5.0"},
             {"labels", "Jan22,Mar22,Jun22,Sep22,Dec22,Mar23,Jun23,Sep23,Dec23"}});
        add("heading", "Forward Guidance Summary");
        add("text", "Summarise the forward guidance from major central banks.");
        add("heading", "Balance Sheet");
        add("chart", {},
            {{"chart_type", "bar"}, {"title", "Balance Sheet Size ($T)"},
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
            {{"chart_type", "pie"}, {"title", "Token Distribution"},
             {"data", "20,15,30,25,10"}, {"labels", "Team,Investors,Community,Treasury,Public"}});
        add("heading", "On-Chain Metrics");
        add("table", {}, {{"rows", "6"}, {"cols", "3"}});
        add("heading", "Price History");
        add("chart", {},
            {{"chart_type", "line"}, {"title", "Price (USD)"},
             {"data", "16000,20000,25000,30000,28000,42000,38000,52000"}});
        add("heading", "Competitive Landscape");
        add("text", "Key competitors and differentiation.");
        add("heading", "Risks");
        add("list", "Regulatory risk\nSmart contract bugs\nCompetitor displacement\nLiquidity risk\nTeam/execution risk");
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
            {{"chart_type", "line"}, {"title", "Total Value Locked ($M)"},
             {"data", "500,800,1200,900,1500,2000,1800,2400"}});
        add("heading", "Revenue & Fees");
        add("chart", {},
            {{"chart_type", "bar"}, {"title", "Monthly Protocol Revenue ($K)"},
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
            {{"chart_type", "pie"}, {"title", "Portfolio Allocation"},
             {"data", "50,25,15,10"}, {"labels", "BTC,ETH,SOL,Other"}});
        add("heading", "Performance");
        add("chart", {},
            {{"chart_type", "line"}, {"title", "Portfolio Value (USD)"},
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
            {{"chart_type", "line"}, {"title", "Yield vs Benchmark"},
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
            {{"chart_type", "line"}, {"title", "US Treasury Yield Curve"},
             {"data", "5.3,5.1,4.9,4.7,4.6,4.5,4.4,4.3"},
             {"labels", "3M,6M,1Y,2Y,3Y,5Y,10Y,30Y"}});
        add("heading", "Key Spreads");
        add("table", {}, {{"rows", "5"}, {"cols", "3"}});
        add("heading", "Historical Comparison");
        add("chart", {},
            {{"chart_type", "bar"}, {"title", "10Y-2Y Spread (bps)"},
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
            {{"chart_type", "line"}, {"title", "Strategy vs Benchmark"},
             {"data", "100,108,115,112,124,131,128,142,150"},
             {"labels", "Jan,Feb,Mar,Apr,May,Jun,Jul,Aug,Sep"}});
        add("heading", "Drawdown Analysis");
        add("chart", {},
            {{"chart_type", "bar"}, {"title", "Monthly Drawdowns (%)"},
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
            {{"chart_type", "bar"}, {"title", "VaR by Confidence Level"},
             {"data", "1.2,1.8,2.5,3.1"}, {"labels", "90%,95%,99%,99.9%"}});
        add("heading", "Stress Test Results");
        add("table", {}, {{"rows", "7"}, {"cols", "3"}});
        add("heading", "Correlation Matrix");
        add("text", "Paste or describe key pairwise correlations between holdings.");
        add("heading", "Concentration Risk");
        add("chart", {},
            {{"chart_type", "pie"}, {"title", "Position Concentration"},
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
            {{"chart_type", "bar"}, {"title", "Revenue ($M)"},
             {"data", "42,45,48,44,51,53,50,55"},
             {"labels", "Q1 22,Q2 22,Q3 22,Q4 22,Q1 23,Q2 23,Q3 23,Q4 23"}});
        add("heading", "Cost Breakdown");
        add("chart", {},
            {{"chart_type", "pie"}, {"title", "Cost Structure"},
             {"data", "40,25,20,15"}, {"labels", "COGS,R&D,S&M,G&A"}});
        add("heading", "Department Performance");
        add("table", {}, {{"rows", "6"}, {"cols", "4"}});
        add("heading", "Targets vs Actuals");
        add("chart", {},
            {{"chart_type", "bar"}, {"title", "Target vs Actual"},
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
            {{"chart_type", "bar"}, {"title", "Revenue & Net Income"},
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

    svc->replace_document(doc);
}

} // namespace fincept::services
