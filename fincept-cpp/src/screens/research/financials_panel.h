#pragma once
// Financials panel — Income Statement, Balance Sheet, Cash Flow
// Mirrors FinancialsTab.tsx: table-based display of financial statements

#include "research_types.h"
#include "research_data.h"

namespace fincept::research {

class FinancialsPanel {
public:
    void render(ResearchData& data);

private:
    int active_statement_ = 0; // 0=income, 1=balance, 2=cashflow

    void render_statement_table(const FinancialStatement& stmt, const char* title);
};

} // namespace fincept::research
