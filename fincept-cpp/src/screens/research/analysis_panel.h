#pragma once
// Analysis panel — Financial Health, Enterprise Value, Revenue & Profits, Key Ratios
// Mirrors AnalysisTab.tsx: 2x2 grid of financial metric cards

#include "research_types.h"
#include "research_data.h"

namespace fincept::research {

class AnalysisPanel {
public:
    void render(ResearchData& data);
};

} // namespace fincept::research
