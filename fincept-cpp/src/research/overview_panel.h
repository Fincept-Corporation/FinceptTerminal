#pragma once
// Overview panel — Trading, Valuation, Chart, Analyst Targets, Profitability, Growth
// Mirrors OverviewTab.tsx: 4-column data grid + bottom company overview

#include "research_types.h"
#include "research_data.h"

namespace fincept::research {

class OverviewPanel {
public:
    void render(ResearchData& data, ChartPeriod& period);

private:
    void render_trading_valuation(const QuoteData& q, const StockInfo& si, float width);
    void render_chart(const std::vector<HistoricalDataPoint>& hist, ChartPeriod& period, float width, float height);
    void render_analyst_performance(const StockInfo& si, float width);
    void render_company_overview(const StockInfo& si);
};

} // namespace fincept::research
