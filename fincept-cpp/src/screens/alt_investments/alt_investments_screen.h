#pragma once
// Alternative Investments — CFA-Level analytics for 10 asset categories
// Port of AlternativeInvestmentsTab.tsx — 27 analyzers across bonds,
// real estate, hedge funds, commodities, PE/VC, annuities, structured
// products, TIPS, strategies, crypto

#include <imgui.h>
#include <string>
#include <vector>
#include <future>
#include <nlohmann/json.hpp>

namespace fincept::alt_investments {

// ============================================================================
// Category & Analyzer definitions
// ============================================================================

struct Analyzer {
    std::string id;
    std::string name;
    std::string description;
};

struct Category {
    std::string id;
    std::string name;
    std::string short_name;
    std::string description;
    std::string group;     // "Fixed Income", "Real Assets", "Alternatives", "Structured"
    ImVec4      color;
    int         analyzer_count;
    std::vector<Analyzer> analyzers;
};

// ============================================================================
// Input field definition (dynamic form)
// ============================================================================

struct InputField {
    std::string key;
    std::string label;
    enum class Type { Float, Int, Text, Combo } type = Type::Float;
    float default_val  = 0.0f;
    float min_val      = 0.0f;
    float max_val      = 1e9f;
    std::vector<std::string> combo_options;  // for Combo type
};

// ============================================================================
// Screen
// ============================================================================

class AltInvestmentsScreen {
public:
    void render();

private:
    bool initialized_ = false;
    void init();

    // --- Categories ---
    std::vector<Category> categories_;
    int active_category_idx_ = 0;
    int active_analyzer_idx_ = 0;

    // --- Left panel ---
    bool left_collapsed_ = false;

    // --- Input form (dynamic per analyzer) ---
    std::vector<InputField> current_fields_;
    std::map<std::string, float>       float_values_;
    std::map<std::string, int>         int_values_;
    std::map<std::string, std::string> text_values_;
    std::map<std::string, int>         combo_values_;
    void build_fields_for_analyzer(const std::string& analyzer_id);

    // --- Analysis result ---
    enum class AsyncStatus { Idle, Loading, Success, Error };
    AsyncStatus analysis_status_ = AsyncStatus::Idle;
    nlohmann::json result_;
    std::string error_;

    // --- Async ---
    std::future<void> async_task_;
    bool is_async_busy() const;
    void check_async();

    // --- Actions ---
    void run_analysis();
    nlohmann::json build_params() const;

    // --- Render sections ---
    void render_header(float w);
    void render_sidebar(float w, float h);
    void render_center_panel(float w, float h);
    void render_analyzer_selector(float w);
    void render_input_form(float w, float h);
    void render_result_panel(float w, float h);
    void render_verdict(float w);
    void render_metrics(float w);
    void render_status_bar(float w);
};

} // namespace fincept::alt_investments
