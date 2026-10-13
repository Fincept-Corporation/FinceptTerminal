// AgentWorkbenchTemplates.cpp — starter prompts + placeholder highlighting.

#include "screens/agent_workbench/AgentWorkbenchTemplates.h"

#include "ui/theme/Theme.h"

#include <QCoreApplication>
#include <QTextCharFormat>
#include <QTextDocument>

namespace fincept::screens {

QVector<AgentPromptTemplate> agent_prompt_templates() {
    QVector<AgentPromptTemplate> t;
    t.append({QCoreApplication::translate("AgentWorkbenchTemplates", "Morning brief for my watchlist"),
              QCoreApplication::translate("AgentWorkbenchTemplates", "Prepare my morning brief for <WATCHLIST NAME> (or my default watchlist).\n"
                  "For each symbol: last price, 1-day and 5-day change, pre-market move if available, and the most "
                  "important news or filing of the last 24 hours.\n"
                  "Then summarise the overall market tone (indices, rates, FX, commodities), the three biggest movers "
                  "and why, and today's scheduled catalysts (earnings, macro releases).\n"
                  "Finish with a short list of what to watch at the open."),
              false});
    t.append({QCoreApplication::translate("AgentWorkbenchTemplates", "Earnings preview: <TICKER>"),
              QCoreApplication::translate("AgentWorkbenchTemplates", "Earnings preview for <TICKER>.\n"
                  "Cover the report date and time, consensus EPS and revenue against the last four quarters' actuals, "
                  "guidance history, the options-implied move if available, the KPIs and segments to watch, recent "
                  "news and estimate revisions, and how the stock traded after each of the last four reports.\n"
                  "End with the bull case, the bear case and what would surprise the market."),
              false});
    t.append({QCoreApplication::translate("AgentWorkbenchTemplates", "10-K risk-factor diff vs last year: <TICKER>"),
              QCoreApplication::translate("AgentWorkbenchTemplates", "Compare the Risk Factors section (Item 1A) of <TICKER>'s latest 10-K with the previous year's "
                  "10-K.\n"
                  "List the risks that are new, removed or materially reworded, quote the key sentences, and rate each "
                  "change by its likely importance to investors.\n"
                  "Cite both filings' dates and accession numbers."),
              false});
    t.append({QCoreApplication::translate("AgentWorkbenchTemplates", "Portfolio risk & concentration review"),
              QCoreApplication::translate("AgentWorkbenchTemplates", "Review the risk of my portfolio <PORTFOLIO NAME>.\n"
                  "Show position weights, sector / country / currency concentration, the five largest contributors "
                  "to risk, beta and volatility against <BENCHMARK>, correlation clusters, and the drawdown over the "
                  "last three months.\n"
                  "Flag any single name above 10% or sector above 30%, and suggest concrete ways to reduce risk "
                  "without changing the overall thesis."),
              false});
    t.append({QCoreApplication::translate("AgentWorkbenchTemplates", "Macro dashboard: rates, inflation, FX — what changed this week"),
              QCoreApplication::translate("AgentWorkbenchTemplates", "Build a macro dashboard for <COUNTRY OR REGION>: policy rate and the market-implied path, 2y and "
                  "10y yields and the curve slope, the latest CPI / PCE and core inflation, PMIs, unemployment, and "
                  "the main FX crosses.\n"
                  "For each, show the latest value, the change over the past week and month, and the next release "
                  "date.\n"
                  "Summarise what changed this week and what it means for equities and bonds."),
              false});
    t.append({QCoreApplication::translate("AgentWorkbenchTemplates", "Screen for <CRITERIA> and rank"),
              QCoreApplication::translate("AgentWorkbenchTemplates", "Screen <UNIVERSE, e.g. S&P 500> for <CRITERIA, e.g. P/E below 15, revenue growth above 10%, net "
                  "debt / EBITDA below 2>.\n"
                  "Rank the matches by <RANKING METRIC>, show the top 15 in a table with the screening metrics, and "
                  "add one line on each company's investment case and main risk."),
              false});
    t.append({QCoreApplication::translate("AgentWorkbenchTemplates", "Build a DCF for <TICKER> and save it as a spreadsheet"),
              QCoreApplication::translate("AgentWorkbenchTemplates", "Build a discounted cash flow model for <TICKER>.\n"
                  "Use the last five years of financials from the terminal, project five years of revenue, margins, "
                  "capex and working capital with explicit assumptions, derive WACC from the terminal's market data, "
                  "and compute equity value per share with a sensitivity table for WACC and terminal growth.\n"
                  "Save the model as a spreadsheet (CSV or XLSX) in the working folder and summarise the result "
                  "against the current share price."),
              true});
    t.append({QCoreApplication::translate("AgentWorkbenchTemplates", "Summarise today's geopolitical risks for my holdings"),
              QCoreApplication::translate("AgentWorkbenchTemplates", "Summarise today's geopolitical risks that matter for my holdings in <PORTFOLIO NAME>.\n"
                  "Map each relevant event (conflicts, sanctions, elections, trade measures, shipping disruptions) to "
                  "the positions, sectors and regions it affects, estimate the direction and size of the impact, and "
                  "list hedges or signals to monitor."),
              false});
    return t;
}

const QRegularExpression& agent_placeholder_regex() {
    static const QRegularExpression rx(QStringLiteral("<[A-Z][^<>\\n]{0,80}>"));
    return rx;
}

AgentPlaceholderHighlighter::AgentPlaceholderHighlighter(QTextDocument* document) : QSyntaxHighlighter(document) {}

void AgentPlaceholderHighlighter::highlightBlock(const QString& text) {
    QTextCharFormat fmt;
    fmt.setForeground(QColor(ui::colors::AMBER()));
    fmt.setBackground(QColor(ui::colors::ACCENT_BG()));
    fmt.setFontWeight(QFont::Bold);
    auto it = agent_placeholder_regex().globalMatch(text);
    while (it.hasNext()) {
        const auto m = it.next();
        setFormat(static_cast<int>(m.capturedStart()), static_cast<int>(m.capturedLength()), fmt);
    }
}

} // namespace fincept::screens
