#pragma once
// Equity Research Screen — Bloomberg-style multi-tab research terminal
// Mirrors EquityResearchTab.tsx: Symbol search, ticker bar, sub-tabs
// Sub-tabs: Overview, Financials, Analysis, Technicals, News

#include "research_types.h"
#include "research_data.h"
#include "overview_panel.h"
#include "financials_panel.h"
#include "analysis_panel.h"
#include "technicals_panel.h"
#include "news_panel.h"

namespace fincept::research {

class ResearchScreen {
public:
    void render();

private:
    bool initialized_ = false;

    // Data layer
    ResearchData data_;

    // UI state
    char search_buf_[32] = "AAPL";
    std::string current_symbol_ = "AAPL";
    ChartPeriod chart_period_ = ChartPeriod::M6;
    int active_tab_ = 0; // 0=Overview, 1=Financials, 2=Analysis, 3=Technicals, 4=News

    // Sub-panels
    OverviewPanel   overview_;
    FinancialsPanel financials_;
    AnalysisPanel   analysis_;
    TechnicalsPanel technicals_;
    ResearchNewsPanel news_;

    // Init
    void init();

    // Section renderers
    void render_header();
    void render_ticker_bar();
    void render_sub_tabs();
    void render_content();
    void render_footer();

    // Actions
    void do_search();
};

} // namespace fincept::research
