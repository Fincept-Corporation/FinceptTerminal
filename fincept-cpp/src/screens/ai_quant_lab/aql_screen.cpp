#include "ai_quant_lab_screen.h"
#include "ui/theme.h"
#include "core/logger.h"
#include <imgui.h>
#include <implot.h>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <ctime>

namespace fincept::ai_quant_lab {


// ============================================================================
// Module theme colors (per-panel accent)
// ============================================================================
namespace panel_colors {
    constexpr ImVec4 PURPLE      = {0.55f, 0.26f, 0.87f, 1.0f};  // Factor Discovery, RL Trading
    constexpr ImVec4 BLUE        = {0.18f, 0.53f, 0.96f, 1.0f};  // Model Library
    constexpr ImVec4 GREEN       = {0.05f, 0.85f, 0.42f, 1.0f};  // Live Signals
    constexpr ImVec4 CYAN        = {0.00f, 0.79f, 0.89f, 1.0f};  // Rolling Retraining
    constexpr ImVec4 RED         = {0.96f, 0.25f, 0.25f, 1.0f};  // HFT
    constexpr ImVec4 MAGENTA     = {0.87f, 0.26f, 0.55f, 1.0f};  // Meta Learning
    constexpr ImVec4 YELLOW      = {1.0f, 0.78f, 0.08f, 1.0f};   // Online Learning
    constexpr ImVec4 TEAL        = {0.00f, 0.79f, 0.65f, 1.0f};  // Statsmodels
    constexpr ImVec4 GLUON_BLUE  = {0.00f, 0.63f, 0.89f, 1.0f};  // GluonTS
    constexpr ImVec4 GS_PURPLE   = {0.44f, 0.26f, 0.76f, 1.0f};  // GS Quant
    constexpr ImVec4 ORANGE      = {1.0f, 0.45f, 0.05f, 1.0f};   // Default / Accent
}

// ============================================================================
// Init
// ============================================================================
void AIQuantLabScreen::init() {
    if (initialized_) return;
    modules_ = get_modules();
    expanded_categories_ = {ModuleCategory::Core, ModuleCategory::AIML};
    initialized_ = true;
    log_status("AI Quant Lab initialized", theme::colors::SUCCESS);
    LOG_INFO("AIQuantLab", "Initialized with %zu modules", modules_.size());
}

void AIQuantLabScreen::log_status(const std::string& msg, ImVec4 color) {
    status_log_.push_back({ImGui::GetTime(), msg, color});
    if (status_log_.size() > 100) status_log_.erase(status_log_.begin());
}

// ============================================================================
// Main render
// ============================================================================
void AIQuantLabScreen::render() {
    init();
    using namespace theme::colors;

    ImGuiViewport* vp = ImGui::GetMainViewport();
    float tab_bar_h = ImGui::GetFrameHeight() + 4;
    float footer_h = ImGui::GetFrameHeight();

    float x = vp->WorkPos.x;
    float y = vp->WorkPos.y + tab_bar_h;
    float w = vp->WorkSize.x;
    float h = vp->WorkSize.y - tab_bar_h - footer_h;

    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::SetNextWindowSize(ImVec2(w, h));

    ImGui::PushStyleColor(ImGuiCol_WindowBg, BG_DARK);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    ImGui::Begin("##ai_quant_lab", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoDocking);

    float sidebar_w = left_panel_visible_ ? 220.0f : 0.0f;
    float status_w = right_panel_visible_ ? 260.0f : 0.0f;
    float content_w = w - sidebar_w - status_w - 4;

    // Left sidebar
    if (left_panel_visible_) {
        ImGui::BeginChild("##aql_sidebar", ImVec2(sidebar_w, h), true);
        render_sidebar(h);
        ImGui::EndChild();
        ImGui::SameLine();
    }

    // Center content
    ImGui::BeginChild("##aql_content", ImVec2(content_w, h), false);
    render_content(content_w, h);
    ImGui::EndChild();

    // Right status panel
    if (right_panel_visible_) {
        ImGui::SameLine();
        ImGui::BeginChild("##aql_status", ImVec2(status_w, h), true);
        render_status_panel(h);
        ImGui::EndChild();
    }

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

// ============================================================================
// Sidebar — Module navigation grouped by category
// ============================================================================
void AIQuantLabScreen::render_sidebar(float /*height*/) {
    using namespace theme::colors;

    // Header
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARKEST);
    ImGui::BeginChild("##aql_header", ImVec2(-1, 50), false);
    ImGui::SetCursorPos(ImVec2(12, 8));
    ImGui::TextColored(panel_colors::ORANGE, "AI QUANT LAB");
    ImGui::SetCursorPos(ImVec2(12, 28));
    ImGui::TextColored(TEXT_DIM, "18 MODULES | QUANT RESEARCH");
    ImGui::EndChild();
    ImGui::PopStyleColor();

    // Accent line
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddRectFilled(p,
        ImVec2(p.x + ImGui::GetContentRegionAvail().x, p.y + 2),
        IM_COL32(255, 115, 13, 255));
    ImGui::Dummy(ImVec2(0, 4));

    // Toggle buttons
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 2));
    if (ImGui::SmallButton(right_panel_visible_ ? "Hide Status >>" : "<< Show Status")) {
        right_panel_visible_ = !right_panel_visible_;
    }
    ImGui::PopStyleVar();
    ImGui::Dummy(ImVec2(0, 4));

    // Category groups
    ModuleCategory categories[] = {
        ModuleCategory::Core, ModuleCategory::AIML,
        ModuleCategory::Advanced, ModuleCategory::Analytics
    };

    for (auto cat : categories) {
        bool expanded = expanded_categories_.count(cat) > 0;
        ImVec4 cat_color;
        switch (cat) {
            case ModuleCategory::Core:     cat_color = panel_colors::BLUE; break;
            case ModuleCategory::AIML:     cat_color = panel_colors::PURPLE; break;
            case ModuleCategory::Advanced: cat_color = panel_colors::RED; break;
            case ModuleCategory::Analytics: cat_color = panel_colors::TEAL; break;
        }

        char cat_label[64];
        std::snprintf(cat_label, sizeof(cat_label), "%s %s",
            expanded ? "v" : ">", category_label(cat));

        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(cat_color.x, cat_color.y, cat_color.z, 0.08f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, BG_HOVER);

        if (ImGui::Selectable(cat_label, false, ImGuiSelectableFlags_None, ImVec2(0, 22))) {
            if (expanded) expanded_categories_.erase(cat);
            else expanded_categories_.insert(cat);
        }
        ImGui::PopStyleColor(2);

        if (!expanded) continue;

        ImGui::Indent(12);
        for (auto& mod : modules_) {
            if (mod.category != cat) continue;
            bool is_active = (mod.view == active_view_);

            if (is_active) {
                ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(cat_color.x, cat_color.y, cat_color.z, 0.15f));
                ImGui::PushStyleColor(ImGuiCol_Text, cat_color);
            } else {
                ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_Text, TEXT_SECONDARY);
            }

            if (ImGui::Selectable(mod.label, is_active, ImGuiSelectableFlags_None, ImVec2(0, 20))) {
                active_view_ = mod.view;
                log_status(std::string("Switched to ") + mod.label, cat_color);
            }
            ImGui::PopStyleColor(2);

            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", mod.description);
            }
        }
        ImGui::Unindent(12);
        ImGui::Dummy(ImVec2(0, 2));
    }
}

// ============================================================================
// Content — dispatches to the active panel
// ============================================================================
void AIQuantLabScreen::render_content(float width, float height) {
    using namespace theme::colors;
    (void)width; (void)height;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 8));

    // Panel toggle for left sidebar
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 2));
    if (ImGui::SmallButton(left_panel_visible_ ? "<< Hide" : ">> Show")) {
        left_panel_visible_ = !left_panel_visible_;
    }
    ImGui::SameLine();

    // Current module title
    for (auto& mod : modules_) {
        if (mod.view == active_view_) {
            ImGui::TextColored(ACCENT, "%s", mod.label);
            ImGui::SameLine();
            ImGui::TextColored(TEXT_DIM, "- %s", mod.description);
            break;
        }
    }
    ImGui::PopStyleVar();
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 4));

    // Dispatch to panel
    switch (active_view_) {
        case ViewMode::FactorDiscovery:    render_factor_discovery(); break;
        case ViewMode::ModelLibrary:       render_model_library(); break;
        case ViewMode::Backtesting:        render_backtesting(); break;
        case ViewMode::LiveSignals:        render_live_signals(); break;
        case ViewMode::Functime:           render_functime(); break;
        case ViewMode::Fortitudo:          render_fortitudo(); break;
        case ViewMode::Statsmodels:        render_statsmodels(); break;
        case ViewMode::CFAQuant:           render_cfa_quant(); break;
        case ViewMode::DeepAgent:          render_deep_agent(); break;
        case ViewMode::RLTrading:          render_rl_trading(); break;
        case ViewMode::OnlineLearning:     render_online_learning(); break;
        case ViewMode::HFT:               render_hft(); break;
        case ViewMode::MetaLearning:       render_meta_learning(); break;
        case ViewMode::RollingRetraining:  render_rolling_retraining(); break;
        case ViewMode::AdvancedModels:     render_advanced_models(); break;
        case ViewMode::GluonTS:            render_gluonts(); break;
        case ViewMode::GSQuant:            render_gs_quant(); break;
        case ViewMode::PatternIntelligence: render_pattern_intelligence(); break;
    }

    ImGui::PopStyleVar();
}

// ============================================================================
// Status Panel (Right) — System status + activity log
// ============================================================================
void AIQuantLabScreen::render_status_panel(float /*height*/) {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARKEST);
    ImGui::BeginChild("##status_header", ImVec2(-1, 40), false);
    ImGui::SetCursorPos(ImVec2(8, 8));
    ImGui::TextColored(TEXT_DIM, "SYSTEM STATUS");
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddRectFilled(p,
        ImVec2(p.x + ImGui::GetContentRegionAvail().x, p.y + 1),
        IM_COL32(88, 88, 97, 255));
    ImGui::Dummy(ImVec2(0, 4));

    // Service status indicators
    theme::StatusIndicator("Qlib Engine", true);
    theme::StatusIndicator("RD-Agent", true);
    theme::StatusIndicator("Deep Agent", true);
    theme::StatusIndicator("Python Runtime", true);
    ImGui::Dummy(ImVec2(0, 8));

    // Active module
    ImGui::TextColored(TEXT_DIM, "Active Module:");
    for (auto& mod : modules_) {
        if (mod.view == active_view_) {
            ImGui::TextColored(ACCENT, "  %s", mod.label);
            break;
        }
    }
    ImGui::Dummy(ImVec2(0, 8));

    // Activity log
    ImGui::TextColored(TEXT_DIM, "ACTIVITY LOG");
    ImGui::Separator();

    ImGui::BeginChild("##log_scroll", ImVec2(-1, -1), false);
    for (int i = (int)status_log_.size() - 1; i >= 0; i--) {
        auto& entry = status_log_[i];
        double age = ImGui::GetTime() - entry.time;
        char time_buf[16];
        if (age < 60) std::snprintf(time_buf, sizeof(time_buf), "%.0fs ago", age);
        else if (age < 3600) std::snprintf(time_buf, sizeof(time_buf), "%.0fm ago", age / 60);
        else std::snprintf(time_buf, sizeof(time_buf), "%.0fh ago", age / 3600);

        ImGui::TextColored(TEXT_DISABLED, "%s", time_buf);
        ImGui::SameLine();
        ImGui::TextColored(entry.color, "%s", entry.msg.c_str());
    }
    ImGui::EndChild();
}

// ============================================================================
// UI Helpers
// ============================================================================
void AIQuantLabScreen::metric_card(const char* label, const char* value, ImVec4 color) {
    using namespace theme::colors;
    ImGui::BeginGroup();
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_WIDGET);
    ImGui::BeginChild(ImGui::GetID(label), ImVec2(140, 56), true);
    ImGui::TextColored(TEXT_DIM, "%s", label);
    ImGui::TextColored(color, "%s", value);
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::EndGroup();
}

void AIQuantLabScreen::metric_card(const char* label, double value, const char* fmt, ImVec4 color) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), fmt, value);
    metric_card(label, buf, color);
}

void AIQuantLabScreen::progress_bar(float fraction, const char* label) {
    using namespace theme::colors;
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ACCENT);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);
    if (label)
        ImGui::ProgressBar(fraction, ImVec2(-1, 18), label);
    else
        ImGui::ProgressBar(fraction, ImVec2(-1, 18));
    ImGui::PopStyleColor(2);
}

bool AIQuantLabScreen::config_input_text(const char* label, char* buf, int buf_size, float width) {
    ImGui::TextColored(theme::colors::TEXT_DIM, "%s", label);
    ImGui::SameLine(width);
    char id[128];
    std::snprintf(id, sizeof(id), "##%s", label);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    return ImGui::InputText(id, buf, buf_size);
}

bool AIQuantLabScreen::config_input_float(const char* label, float* val, float width) {
    ImGui::TextColored(theme::colors::TEXT_DIM, "%s", label);
    ImGui::SameLine(width);
    char id[128];
    std::snprintf(id, sizeof(id), "##%s", label);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    return ImGui::InputFloat(id, val, 0, 0, "%.4f");
}

bool AIQuantLabScreen::config_input_int(const char* label, int* val, float width) {
    ImGui::TextColored(theme::colors::TEXT_DIM, "%s", label);
    ImGui::SameLine(width);
    char id[128];
    std::snprintf(id, sizeof(id), "##%s", label);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    return ImGui::InputInt(id, val);
}

bool AIQuantLabScreen::config_combo(const char* label, int* current, const char* const* items, int count, float width) {
    ImGui::TextColored(theme::colors::TEXT_DIM, "%s", label);
    ImGui::SameLine(width);
    char id[128];
    std::snprintf(id, sizeof(id), "##%s", label);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    return ImGui::Combo(id, current, items, count);
}

void AIQuantLabScreen::section_header(const char* title, ImVec4 color) {
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::SeparatorText(title);
    ImGui::PopStyleColor();
}

void AIQuantLabScreen::status_badge(const char* text, ImVec4 bg_color, ImVec4 text_color) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImVec2 sz = ImGui::CalcTextSize(text);
    ImGui::GetWindowDrawList()->AddRectFilled(p,
        ImVec2(p.x + sz.x + 12, p.y + sz.y + 6),
        ImGui::ColorConvertFloat4ToU32(bg_color), 3.0f);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 6);
    ImGui::TextColored(text_color, "%s", text);
    ImGui::Dummy(ImVec2(sz.x + 16, sz.y + 8));
}


} // namespace fincept::ai_quant_lab
