#pragma once
// News panel — Symbol-specific news articles
// Mirrors NewsTab.tsx: article list with headline, publisher, date, description

#include "research_types.h"
#include "research_data.h"

namespace fincept::research {

class ResearchNewsPanel {
public:
    void render(ResearchData& data, const std::string& symbol);

private:
    int selected_ = -1;
};

} // namespace fincept::research
