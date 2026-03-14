#pragma once
// M&A Analytics Screen — Bloomberg-grade corporate finance terminal
// 3-panel layout: left sidebar (modules) | center content | right info panel
// 8 modules: Valuation, Merger, Deals, Startup, Fairness, Industry, Advanced, Comparison

#include "mna_constants.h"
#include "mna_data.h"

// Forward declare panel classes
namespace fincept::mna {
    class ValuationPanel;
    class MergerPanel;
    class DealDbPanel;
    class StartupPanel;
    class FairnessPanel;
    class IndustryPanel;
    class AdvancedPanel;
    class ComparisonPanel;
}

#include "panels/valuation_panel.h"
#include "panels/merger_panel.h"
#include "panels/deal_db_panel.h"
#include "panels/startup_panel.h"
#include "panels/fairness_panel.h"
#include "panels/industry_panel.h"
#include "panels/advanced_panel.h"
#include "panels/comparison_panel.h"

namespace fincept::mna {

class MNAScreen {
public:
    void render();

private:
    bool initialized_ = false;
    int active_module_ = 0;
    bool left_panel_open_ = true;
    bool right_panel_open_ = true;

    // Shared data layer
    MNAData data_;

    // Panel instances
    ValuationPanel  valuation_panel_;
    MergerPanel     merger_panel_;
    DealDbPanel     deal_db_panel_;
    StartupPanel    startup_panel_;
    FairnessPanel   fairness_panel_;
    IndustryPanel   industry_panel_;
    AdvancedPanel   advanced_panel_;
    ComparisonPanel comparison_panel_;

    // Status
    std::string status_;
    double status_time_ = 0;

    void init();
    void render_nav_bar(float height);
    void render_left_sidebar(float width, float height);
    void render_center_content(float width, float height);
    void render_right_info(float width, float height);
    void render_status_bar(float height);
};

} // namespace fincept::mna
