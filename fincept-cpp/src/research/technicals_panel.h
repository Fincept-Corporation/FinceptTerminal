#pragma once
// Technicals panel — Technical indicators with signal summary
// Mirrors TechnicalsTab.tsx: Overall rating, key indicators, category sections

#include "research_types.h"
#include "research_data.h"

namespace fincept::research {

class TechnicalsPanel {
public:
    void render(ResearchData& data);

private:
    bool show_trend_     = true;
    bool show_momentum_  = true;
    bool show_volatility_= false;
};

} // namespace fincept::research
