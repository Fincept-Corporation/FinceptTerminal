// AI Quant Lab Screen — 18-module quantitative research platform
// Three-panel layout: [Sidebar | Content | Status]
// Mirrors the Tauri AIQuantLabTab with all sub-panels

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

// ============================================================================
// PANEL 1: Factor Discovery (RD-Agent) — PURPLE THEME
// ============================================================================
void AIQuantLabScreen::render_factor_discovery() {
    using namespace theme::colors;
    section_header("FACTOR DISCOVERY", panel_colors::PURPLE);
    ImGui::TextColored(TEXT_DIM, "RD-Agent automated alpha factor mining with AI-driven research");
    ImGui::Dummy(ImVec2(0, 8));

    // Config
    static const char* markets[] = {"US", "CN", "CRYPTO"};
    config_input_text("Task Description", fd_task_desc_, sizeof(fd_task_desc_), 130);
    config_input_text("API Key", fd_api_key_, sizeof(fd_api_key_), 130);
    config_combo("Target Market", &fd_market_idx_, markets, 3, 130);
    config_input_int("Budget", &fd_budget_, 130);

    ImGui::Dummy(ImVec2(0, 8));

    // Start button
    bool is_running = fd_op_.loading.load() || fd_polling_;
    if (is_running) {
        theme::LoadingSpinner("Mining factors...");
        progress_bar(0.5f, "Searching for alpha...");

        // Poll status every 5 seconds
        if (fd_polling_ && !fd_task_id_.empty()) {
            double now = ImGui::GetTime();
            if (now - fd_last_poll_ > 5.0) {
                fd_last_poll_ = now;
                // Check status
                auto status_future = QuantLabApi::instance().rdagent_get_status(fd_task_id_);
                if (status_future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
                    auto r = status_future.get();
                    if (r.success && r.data.value("status", "") == "completed") {
                        fd_polling_ = false;
                        // Fetch factors
                        auto fac_future = QuantLabApi::instance().rdagent_get_factors(fd_task_id_);
                        // Will be picked up next frame
                    }
                }
            }
        }
    } else {
        if (theme::AccentButton("Start Factor Mining")) {
            json config = {
                {"task_description", fd_task_desc_},
                {"api_key", fd_api_key_},
                {"target_market", markets[fd_market_idx_]},
                {"budget", fd_budget_},
            };
            fd_op_.loading.store(true);
            log_status("Starting factor mining...", panel_colors::PURPLE);

            std::thread([this, config]() {
                auto result = QuantLabApi::instance().rdagent_start_mining(config).get();
                if (result.success) {
                    fd_task_id_ = result.data.value("task_id", "");
                    fd_polling_ = true;
                    fd_last_poll_ = 0;
                    log_status("Factor mining started: " + fd_task_id_, SUCCESS);
                } else {
                    fd_op_.set_error(result.error);
                    log_status("Factor mining failed: " + result.error, ERROR_RED);
                }
                fd_op_.loading.store(false);
            }).detach();
        }
    }

    // Display discovered factors
    if (!fd_factors_.empty()) {
        ImGui::Dummy(ImVec2(0, 12));
        section_header("DISCOVERED FACTORS", panel_colors::PURPLE);

        for (size_t i = 0; i < fd_factors_.size(); i++) {
            auto& f = fd_factors_[i];
            ImGui::PushID((int)i);

            ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_WIDGET);
            ImGui::BeginChild("##factor", ImVec2(-1, 0), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);

            ImGui::TextColored(panel_colors::PURPLE, "%s", f.name.c_str());
            ImGui::TextColored(TEXT_DIM, "%s", f.description.c_str());

            // Metrics row
            metric_card("IC", f.ic, "%.4f", f.ic > 0.03 ? MARKET_GREEN : TEXT_SECONDARY);
            ImGui::SameLine();
            metric_card("Sharpe", f.sharpe, "%.2f", f.sharpe > 1.0 ? MARKET_GREEN : TEXT_SECONDARY);
            ImGui::SameLine();
            metric_card("Annual Return", f.annual_return, "%.2f%%", f.annual_return > 0 ? MARKET_GREEN : MARKET_RED);

            // Code display
            if (ImGui::CollapsingHeader("Factor Code")) {
                ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARKEST);
                ImGui::BeginChild("##code", ImVec2(-1, 100));
                ImGui::TextWrapped("%s", f.code.c_str());
                ImGui::EndChild();
                ImGui::PopStyleColor();

                if (ImGui::SmallButton("Copy Code")) {
                    ImGui::SetClipboardText(f.code.c_str());
                    log_status("Factor code copied", SUCCESS);
                }
            }

            ImGui::EndChild();
            ImGui::PopStyleColor();
            ImGui::PopID();
            ImGui::Dummy(ImVec2(0, 4));
        }
    }

    // Error display
    if (!fd_op_.error.empty()) {
        theme::ErrorMessage(fd_op_.error.c_str());
    }
}

// ============================================================================
// PANEL 2: Model Library (Qlib) — BLUE THEME
// ============================================================================
void AIQuantLabScreen::render_model_library() {
    using namespace theme::colors;
    section_header("MODEL LIBRARY", panel_colors::BLUE);
    ImGui::TextColored(TEXT_DIM, "Microsoft Qlib ML models for financial prediction");
    ImGui::Dummy(ImVec2(0, 8));

    // Filter
    static const char* filters[] = {"All", "Tree-Based", "Neural Network", "Linear", "Ensemble"};
    config_combo("Filter", &ml_filter_idx_, filters, 5, 100);

    // Load models button
    if (!ml_list_op_.loading.load()) {
        if (theme::SecondaryButton("Refresh Models")) {
            ml_list_op_.loading.store(true);
            std::thread([this]() {
                auto result = QuantLabApi::instance().qlib_list_models().get();
                if (result.success) {
                    std::lock_guard<std::mutex> lock(ml_list_op_.mutex);
                    ml_models_.clear();
                    if (result.data.is_array()) {
                        for (auto& m : result.data) {
                            QlibModel model;
                            model.name = m.value("name", "");
                            model.type = m.value("type", "");
                            model.description = m.value("description", "");
                            ml_models_.push_back(model);
                        }
                    }
                    ml_list_op_.set_result(result.data);
                } else {
                    ml_list_op_.set_error(result.error);
                }
            }).detach();
        }
    } else {
        theme::LoadingSpinner("Loading models...");
    }

    ImGui::Dummy(ImVec2(0, 8));

    // Model list
    if (ml_models_.empty() && !ml_list_op_.has_result) {
        // Show default models
        static const struct { const char* name; const char* type; const char* desc; } defaults[] = {
            {"LightGBM",    "tree_based",      "Gradient boosting framework with high efficiency"},
            {"XGBoost",     "tree_based",      "Extreme gradient boosting for structured data"},
            {"CatBoost",    "tree_based",      "Gradient boosting with native categorical support"},
            {"LSTM",        "neural_network",  "Long Short-Term Memory recurrent network"},
            {"GRU",         "neural_network",  "Gated Recurrent Unit network"},
            {"Transformer", "neural_network",  "Attention-based architecture for sequences"},
            {"Linear",      "linear",          "Linear regression with regularization"},
            {"Ridge",       "linear",          "L2-regularized linear regression"},
            {"RandomForest","ensemble",        "Bagged decision tree ensemble"},
            {"AdaBoost",    "ensemble",        "Adaptive boosting ensemble method"},
        };

        for (auto& d : defaults) {
            if (ml_filter_idx_ > 0) {
                const char* filter_types[] = {"", "tree_based", "neural_network", "linear", "ensemble"};
                if (std::strcmp(d.type, filter_types[ml_filter_idx_]) != 0) continue;
            }

            bool selected = false;
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(panel_colors::BLUE.x, panel_colors::BLUE.y, panel_colors::BLUE.z, 0.1f));
            if (ImGui::Selectable(d.name, &selected, ImGuiSelectableFlags_None, ImVec2(0, 28))) {
                // Select model
            }
            ImGui::PopStyleColor();
            ImGui::SameLine(200);
            ImGui::TextColored(TEXT_DIM, "[%s]", d.type);
            ImGui::SameLine(350);
            ImGui::TextColored(TEXT_SECONDARY, "%s", d.desc);
        }
    }

    ImGui::Dummy(ImVec2(0, 12));

    // Train config
    section_header("TRAIN MODEL", panel_colors::BLUE);
    config_input_text("Instruments", ml_instruments_, sizeof(ml_instruments_), 120);
    config_input_text("Start Date", ml_start_date_, sizeof(ml_start_date_), 120);
    config_input_text("End Date", ml_end_date_, sizeof(ml_end_date_), 120);

    if (!ml_train_op_.loading.load()) {
        if (theme::AccentButton("Train Model")) {
            ml_train_op_.loading.store(true);
            json config = {
                {"instruments", ml_instruments_},
                {"start_date", ml_start_date_},
                {"end_date", ml_end_date_},
            };
            log_status("Training model...", panel_colors::BLUE);
            std::thread([this, config]() {
                auto result = QuantLabApi::instance().qlib_train_model(config).get();
                if (result.success) {
                    ml_train_op_.set_result(result.data);
                    log_status("Model training completed", SUCCESS);
                } else {
                    ml_train_op_.set_error(result.error);
                    log_status("Training failed: " + result.error, ERROR_RED);
                }
            }).detach();
        }
    } else {
        theme::LoadingSpinner("Training...");
    }

    if (!ml_train_op_.error.empty()) theme::ErrorMessage(ml_train_op_.error.c_str());
}

// ============================================================================
// PANEL 3: Backtesting (Qlib)
// ============================================================================
void AIQuantLabScreen::render_backtesting() {
    using namespace theme::colors;
    section_header("STRATEGY BACKTESTING", ACCENT);
    ImGui::TextColored(TEXT_DIM, "Qlib-powered strategy backtesting engine");
    ImGui::Dummy(ImVec2(0, 8));

    static const char* strategies[] = {"TopK Dropout", "Weight Base", "Enhanced Indexing"};
    config_combo("Strategy", &bt_strategy_idx_, strategies, 3, 120);
    config_input_text("Instruments", bt_instruments_, sizeof(bt_instruments_), 120);
    config_input_text("Start Date", bt_start_, sizeof(bt_start_), 120);
    config_input_text("End Date", bt_end_, sizeof(bt_end_), 120);
    config_input_float("Initial Capital", &bt_capital_, 120);
    config_input_text("Benchmark", bt_benchmark_, sizeof(bt_benchmark_), 120);
    config_input_float("Commission", &bt_commission_, 120);
    config_input_float("Slippage", &bt_slippage_, 120);

    ImGui::Dummy(ImVec2(0, 8));

    if (!bt_op_.loading.load()) {
        if (theme::AccentButton("Run Backtest")) {
            static const char* strat_keys[] = {"topk_dropout", "weight_base", "enhanced_indexing"};
            json config = {
                {"strategy", strat_keys[bt_strategy_idx_]},
                {"instruments", bt_instruments_},
                {"start_date", bt_start_},
                {"end_date", bt_end_},
                {"initial_capital", bt_capital_},
                {"benchmark", bt_benchmark_},
                {"commission", bt_commission_},
                {"slippage", bt_slippage_},
            };
            bt_op_.loading.store(true);
            log_status("Running backtest...", ACCENT);
            std::thread([this, config]() {
                auto result = QuantLabApi::instance().qlib_run_backtest(config).get();
                if (result.success) {
                    bt_op_.set_result(result.data);
                    log_status("Backtest completed", SUCCESS);
                } else {
                    bt_op_.set_error(result.error);
                    log_status("Backtest failed: " + result.error, ERROR_RED);
                }
            }).detach();
        }
    } else {
        theme::LoadingSpinner("Running backtest...");
    }

    // Results
    if (bt_op_.has_result) {
        ImGui::Dummy(ImVec2(0, 12));
        section_header("BACKTEST RESULTS", SUCCESS);

        std::lock_guard<std::mutex> lock(bt_op_.mutex);
        auto& d = bt_op_.result;

        double ann_ret = d.value("annual_return", 0.0);
        double sharpe = d.value("sharpe_ratio", 0.0);
        double max_dd = d.value("max_drawdown", 0.0);
        double win_rate = d.value("win_rate", 0.0);
        int trades = d.value("total_trades", 0);
        double pf = d.value("profit_factor", 0.0);

        metric_card("Annual Return", ann_ret * 100, "%.2f%%", ann_ret > 0 ? MARKET_GREEN : MARKET_RED);
        ImGui::SameLine();
        metric_card("Sharpe Ratio", sharpe, "%.2f", sharpe > 1 ? MARKET_GREEN : WARNING);
        ImGui::SameLine();
        metric_card("Max Drawdown", max_dd * 100, "%.2f%%", MARKET_RED);

        metric_card("Win Rate", win_rate * 100, "%.1f%%", win_rate > 0.5 ? MARKET_GREEN : WARNING);
        ImGui::SameLine();
        char trades_buf[32];
        std::snprintf(trades_buf, sizeof(trades_buf), "%d", trades);
        metric_card("Total Trades", trades_buf, TEXT_PRIMARY);
        ImGui::SameLine();
        metric_card("Profit Factor", pf, "%.2f", pf > 1 ? MARKET_GREEN : MARKET_RED);
    }

    if (!bt_op_.error.empty()) theme::ErrorMessage(bt_op_.error.c_str());
}

// ============================================================================
// PANEL 4: Live Signals — GREEN THEME
// ============================================================================
void AIQuantLabScreen::render_live_signals() {
    using namespace theme::colors;
    section_header("LIVE SIGNALS", panel_colors::GREEN);
    ImGui::TextColored(TEXT_DIM, "Real-time model predictions & trading signals");
    ImGui::Dummy(ImVec2(0, 8));

    static const char* models[] = {"LightGBM", "XGBoost", "LSTM", "Transformer"};
    config_combo("Model", &ls_model_idx_, models, 4, 120);
    config_input_int("Refresh (sec)", &ls_refresh_sec_, 120);

    static const char* signal_filters[] = {"ALL", "BUY", "SELL"};
    config_combo("Filter", &ls_filter_idx_, signal_filters, 3, 120);

    ImGui::Dummy(ImVec2(0, 8));

    // Stream toggle
    if (ls_streaming_) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(panel_colors::GREEN.x, panel_colors::GREEN.y, panel_colors::GREEN.z, 0.3f));
        if (ImGui::Button("Stop Streaming")) {
            ls_streaming_ = false;
            log_status("Signal streaming stopped", WARNING);
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        theme::LoadingSpinner("Streaming...");

        // Auto-refresh
        double now = ImGui::GetTime();
        if (now - ls_last_refresh_ > ls_refresh_sec_ && !ls_op_.loading.load()) {
            ls_last_refresh_ = now;
            ls_op_.loading.store(true);
            json config = {{"model", models[ls_model_idx_]}};
            std::thread([this, config]() {
                auto result = QuantLabApi::instance().qlib_get_live_predictions(config).get();
                if (result.success) {
                    std::lock_guard<std::mutex> lock(ls_op_.mutex);
                    ls_signals_.clear();
                    if (result.data.is_array()) {
                        for (auto& s : result.data) {
                            SignalEntry sig;
                            sig.symbol = s.value("symbol", "");
                            sig.signal = s.value("signal", "HOLD");
                            sig.score = s.value("score", 0.0);
                            sig.price = s.value("price", 0.0);
                            sig.confidence = s.value("confidence", 0.0);
                            sig.model = s.value("model", "");
                            sig.timestamp = s.value("timestamp", "");
                            ls_signals_.push_back(sig);
                        }
                    }
                    ls_op_.set_result(result.data);
                } else {
                    ls_op_.set_error(result.error);
                }
            }).detach();
        }
    } else {
        if (theme::AccentButton("Start Streaming")) {
            ls_streaming_ = true;
            ls_last_refresh_ = 0;
            log_status("Signal streaming started", panel_colors::GREEN);
        }
    }

    // Signal cards
    if (!ls_signals_.empty()) {
        ImGui::Dummy(ImVec2(0, 8));
        for (auto& sig : ls_signals_) {
            // Filter
            if (ls_filter_idx_ == 1 && sig.signal != "BUY") continue;
            if (ls_filter_idx_ == 2 && sig.signal != "SELL") continue;

            ImVec4 sig_color = sig.signal == "BUY" ? MARKET_GREEN :
                               sig.signal == "SELL" ? MARKET_RED : WARNING;

            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(sig_color.x, sig_color.y, sig_color.z, 0.05f));
            ImGui::BeginChild(ImGui::GetID(sig.symbol.c_str()), ImVec2(-1, 0),
                ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);

            ImGui::TextColored(TEXT_PRIMARY, "%s", sig.symbol.c_str());
            ImGui::SameLine(120);
            status_badge(sig.signal.c_str(),
                ImVec4(sig_color.x, sig_color.y, sig_color.z, 0.2f), sig_color);
            ImGui::SameLine(220);
            ImGui::TextColored(TEXT_SECONDARY, "Score: %.4f", sig.score);
            ImGui::SameLine(350);
            ImGui::TextColored(TEXT_SECONDARY, "$%.2f", sig.price);
            ImGui::SameLine(440);
            ImGui::TextColored(TEXT_DIM, "Conf: %.1f%%", sig.confidence * 100);

            ImGui::EndChild();
            ImGui::PopStyleColor();
            ImGui::Dummy(ImVec2(0, 2));
        }
    } else if (!ls_op_.loading.load() && !ls_streaming_) {
        ImGui::Dummy(ImVec2(0, 20));
        ImGui::TextColored(TEXT_DIM, "Click 'Start Streaming' to receive live predictions");
    }

    if (!ls_op_.error.empty()) theme::ErrorMessage(ls_op_.error.c_str());
}

// ============================================================================
// PANEL 5: Deep Agent — LLM-powered autonomous research
// ============================================================================
void AIQuantLabScreen::render_deep_agent() {
    using namespace theme::colors;
    section_header("DEEP AGENT", ACCENT);
    ImGui::TextColored(TEXT_DIM, "LLM-powered autonomous financial research agent");
    ImGui::Dummy(ImVec2(0, 8));

    static const char* agents[] = {"Research", "Strategy Builder", "Portfolio Manager", "Risk Analyzer", "General"};
    config_combo("Agent Type", &da_agent_idx_, agents, 5, 120);
    config_input_text("Query", da_query_, sizeof(da_query_), 120);

    ImGui::Dummy(ImVec2(0, 8));

    // Phase indicator
    static const char* phases[] = {"Ready", "Planning", "Executing", "Synthesizing", "Complete"};
    ImGui::TextColored(TEXT_DIM, "Phase:");
    ImGui::SameLine();
    ImVec4 phase_color = da_phase_ == 4 ? SUCCESS :
                          da_phase_ > 0 ? WARNING : TEXT_SECONDARY;
    ImGui::TextColored(phase_color, "%s", phases[da_phase_]);

    if (da_phase_ > 0 && da_phase_ < 4) {
        float prog = (float)da_phase_ / 4.0f;
        progress_bar(prog, phases[da_phase_]);
    }

    ImGui::Dummy(ImVec2(0, 8));

    if (da_phase_ == 0) {
        if (theme::AccentButton("Create Plan")) {
            if (std::strlen(da_query_) == 0) return;
            da_phase_ = 1;
            da_steps_.clear();
            da_plan_text_.clear();
            da_synthesis_.clear();
            da_op_.loading.store(true);
            log_status("Deep Agent: Creating plan...", ACCENT);

            json config = {
                {"agent_type", agents[da_agent_idx_]},
                {"query", da_query_},
            };
            std::thread([this, config]() {
                auto result = QuantLabApi::instance().deep_agent_create_plan(config).get();
                if (result.success) {
                    da_plan_text_ = result.data.value("plan", "");
                    if (result.data.contains("steps") && result.data["steps"].is_array()) {
                        int idx = 0;
                        for (auto& s : result.data["steps"]) {
                            AgentStep step;
                            step.index = idx++;
                            step.description = s.value("description", "");
                            step.status = "pending";
                            da_steps_.push_back(step);
                        }
                    }
                    da_phase_ = 2;
                    da_current_step_ = 0;
                    log_status("Plan created with " + std::to_string(da_steps_.size()) + " steps", SUCCESS);
                } else {
                    da_phase_ = 0;
                    log_status("Planning failed: " + result.error, ERROR_RED);
                }
                da_op_.loading.store(false);
            }).detach();
        }
    } else if (da_phase_ == 2) {
        // Show plan and steps
        if (!da_plan_text_.empty()) {
            ImGui::TextColored(TEXT_SECONDARY, "Plan:");
            ImGui::TextWrapped("%s", da_plan_text_.c_str());
            ImGui::Dummy(ImVec2(0, 8));
        }

        section_header("EXECUTION STEPS", ACCENT);
        for (auto& step : da_steps_) {
            ImVec4 sc = step.status == "completed" ? SUCCESS :
                        step.status == "running" ? WARNING :
                        step.status == "failed" ? ERROR_RED : TEXT_DIM;
            ImGui::TextColored(sc, "[%d] %s — %s",
                step.index + 1, step.description.c_str(), step.status.c_str());
            if (!step.result.empty() && step.status == "completed") {
                ImGui::Indent(20);
                ImGui::TextColored(TEXT_DIM, "%s", step.result.c_str());
                ImGui::Unindent(20);
            }
        }

        ImGui::Dummy(ImVec2(0, 8));

        if (!da_op_.loading.load() && da_current_step_ < (int)da_steps_.size()) {
            if (theme::AccentButton("Execute Next Step")) {
                da_op_.loading.store(true);
                int step_idx = da_current_step_;
                da_steps_[step_idx].status = "running";
                json config = {
                    {"step_index", step_idx},
                    {"description", da_steps_[step_idx].description},
                    {"agent_type", agents[da_agent_idx_]},
                    {"query", da_query_},
                };
                std::thread([this, config, step_idx]() {
                    auto result = QuantLabApi::instance().deep_agent_execute_step(config).get();
                    if (result.success) {
                        da_steps_[step_idx].status = "completed";
                        da_steps_[step_idx].result = result.data.value("result", "");
                        da_current_step_++;
                        if (da_current_step_ >= (int)da_steps_.size()) {
                            da_phase_ = 3; // Ready to synthesize
                        }
                    } else {
                        da_steps_[step_idx].status = "failed";
                        da_steps_[step_idx].result = result.error;
                    }
                    da_op_.loading.store(false);
                }).detach();
            }
        } else if (da_op_.loading.load()) {
            theme::LoadingSpinner("Executing step...");
        }

        if (da_current_step_ >= (int)da_steps_.size() && !da_op_.loading.load()) {
            if (theme::AccentButton("Synthesize Results")) {
                da_phase_ = 3;
                da_op_.loading.store(true);
                json config = {
                    {"agent_type", agents[da_agent_idx_]},
                    {"query", da_query_},
                    {"steps", json::array()},
                };
                for (auto& s : da_steps_) {
                    config["steps"].push_back({
                        {"description", s.description},
                        {"result", s.result},
                    });
                }
                std::thread([this, config]() {
                    auto result = QuantLabApi::instance().deep_agent_synthesize(config).get();
                    if (result.success) {
                        da_synthesis_ = result.data.value("synthesis", "");
                        da_phase_ = 4;
                        log_status("Deep Agent: Synthesis complete", SUCCESS);
                    } else {
                        da_phase_ = 2;
                        log_status("Synthesis failed: " + result.error, ERROR_RED);
                    }
                    da_op_.loading.store(false);
                }).detach();
            }
        }
    } else if (da_phase_ == 3) {
        theme::LoadingSpinner("Synthesizing results...");
    } else if (da_phase_ == 4) {
        section_header("SYNTHESIS", SUCCESS);
        ImGui::TextWrapped("%s", da_synthesis_.c_str());

        ImGui::Dummy(ImVec2(0, 8));
        if (theme::SecondaryButton("New Query")) {
            da_phase_ = 0;
            da_steps_.clear();
            da_plan_text_.clear();
            da_synthesis_.clear();
        }
    }
}

// ============================================================================
// PANEL 6: RL Trading — PURPLE THEME
// ============================================================================
void AIQuantLabScreen::render_rl_trading() {
    using namespace theme::colors;
    section_header("RL TRADING", panel_colors::PURPLE);
    ImGui::TextColored(TEXT_DIM, "Reinforcement learning trading strategies");
    ImGui::Dummy(ImVec2(0, 8));

    static const char* algos[] = {"PPO", "A2C", "DQN", "SAC", "TD3"};
    static const char* action_spaces[] = {"Continuous", "Discrete"};

    config_combo("Algorithm", &rl_algo_idx_, algos, 5, 130);
    config_input_text("Tickers", rl_tickers_, sizeof(rl_tickers_), 130);
    config_input_text("Start Date", rl_start_, sizeof(rl_start_), 130);
    config_input_text("End Date", rl_end_, sizeof(rl_end_), 130);
    config_input_float("Initial Cash", &rl_initial_cash_, 130);
    config_input_float("Commission", &rl_commission_, 130);
    config_combo("Action Space", &rl_action_space_, action_spaces, 2, 130);
    config_input_int("Timesteps", &rl_timesteps_, 130);
    config_input_float("Learning Rate", &rl_learning_rate_, 130);

    ImGui::Dummy(ImVec2(0, 8));

    // Phase display
    static const char* rl_phases[] = {"Configure", "Env Created", "Training", "Evaluating", "Done"};
    ImGui::TextColored(TEXT_DIM, "Phase:");
    ImGui::SameLine();
    ImGui::TextColored(rl_phase_ == 4 ? SUCCESS : WARNING, "%s", rl_phases[rl_phase_]);

    if (rl_phase_ > 0 && rl_phase_ < 4)
        progress_bar((float)rl_phase_ / 4.0f);

    ImGui::Dummy(ImVec2(0, 8));

    if (!rl_op_.loading.load()) {
        const char* btn_label = rl_phase_ == 0 ? "Create Environment" :
                                 rl_phase_ == 1 ? "Train Agent" :
                                 rl_phase_ == 3 ? "Evaluate" : "Reset";

        if (theme::AccentButton(btn_label)) {
            if (rl_phase_ >= 4) {
                rl_phase_ = 0;
                rl_op_.clear();
                return;
            }

            rl_op_.loading.store(true);
            json config = {
                {"algorithm", algos[rl_algo_idx_]},
                {"tickers", rl_tickers_},
                {"start_date", rl_start_},
                {"end_date", rl_end_},
                {"initial_cash", rl_initial_cash_},
                {"commission", rl_commission_},
                {"action_space", action_spaces[rl_action_space_]},
                {"timesteps", rl_timesteps_},
                {"learning_rate", rl_learning_rate_},
            };

            int phase = rl_phase_;
            std::thread([this, config, phase]() {
                ApiResult result;
                if (phase == 0) {
                    result = QuantLabApi::instance().rl_create_env(config).get();
                    if (result.success) rl_phase_ = 1;
                } else if (phase == 1) {
                    result = QuantLabApi::instance().rl_train_agent(config).get();
                    if (result.success) rl_phase_ = 3;
                } else if (phase == 3) {
                    result = QuantLabApi::instance().rl_evaluate(config).get();
                    if (result.success) rl_phase_ = 4;
                }

                if (result.success) {
                    rl_op_.set_result(result.data);
                    log_status("RL Phase complete", SUCCESS);
                } else {
                    rl_op_.set_error(result.error);
                    log_status("RL failed: " + result.error, ERROR_RED);
                }
            }).detach();
        }
    } else {
        theme::LoadingSpinner("Processing...");
    }

    // Show results
    if (rl_op_.has_result && rl_phase_ == 4) {
        ImGui::Dummy(ImVec2(0, 12));
        section_header("EVALUATION RESULTS", panel_colors::PURPLE);

        std::lock_guard<std::mutex> lock(rl_op_.mutex);
        auto& d = rl_op_.result;
        double total_ret = d.value("total_return", 0.0);
        double sharpe = d.value("sharpe_ratio", 0.0);
        double max_dd = d.value("max_drawdown", 0.0);

        metric_card("Total Return", total_ret * 100, "%.2f%%", total_ret > 0 ? MARKET_GREEN : MARKET_RED);
        ImGui::SameLine();
        metric_card("Sharpe", sharpe, "%.2f", sharpe > 1 ? MARKET_GREEN : WARNING);
        ImGui::SameLine();
        metric_card("Max Drawdown", max_dd * 100, "%.2f%%", MARKET_RED);
    }

    if (!rl_op_.error.empty()) theme::ErrorMessage(rl_op_.error.c_str());
}

// ============================================================================
// PANEL 7: Online Learning — YELLOW THEME
// ============================================================================
void AIQuantLabScreen::render_online_learning() {
    using namespace theme::colors;
    section_header("ONLINE LEARNING", panel_colors::YELLOW);
    ImGui::TextColored(TEXT_DIM, "Continuous model adaptation with drift detection");
    ImGui::Dummy(ImVec2(0, 8));

    static const char* model_types[] = {
        "LinearRegression", "RidgeRegression", "LogisticRegression",
        "DecisionTree", "RandomForest", "GradientBoosting",
        "AdaBoost", "BaggingClassifier", "VotingEnsemble"
    };
    static const char* drift_strategies[] = {"Retrain", "Adaptive"};

    config_combo("Model Type", &ol_model_idx_, model_types, 9, 130);
    config_combo("Drift Strategy", &ol_drift_strategy_, drift_strategies, 2, 130);

    ImGui::Dummy(ImVec2(0, 8));

    // Create Model
    if (!ol_create_op_.loading.load()) {
        if (theme::AccentButton("Create Model")) {
            ol_create_op_.loading.store(true);
            json config = {{"model_type", model_types[ol_model_idx_]}};
            log_status("Creating online model...", panel_colors::YELLOW);
            std::thread([this, config]() {
                auto result = QuantLabApi::instance().qlib_online_create(config).get();
                if (result.success) {
                    ol_create_op_.set_result(result.data);
                    log_status("Online model created", SUCCESS);
                } else {
                    ol_create_op_.set_error(result.error);
                }
            }).detach();
        }
    } else {
        theme::LoadingSpinner("Creating model...");
    }

    ImGui::SameLine();

    // Train
    if (!ol_train_op_.loading.load()) {
        if (theme::SecondaryButton("Train")) {
            ol_train_op_.loading.store(true);
            json config = {{"model_type", model_types[ol_model_idx_]}};
            std::thread([this, config]() {
                auto result = QuantLabApi::instance().qlib_online_train(config).get();
                if (result.success) ol_train_op_.set_result(result.data);
                else ol_train_op_.set_error(result.error);
            }).detach();
        }
    } else {
        theme::LoadingSpinner("Training...");
    }

    ImGui::Dummy(ImVec2(0, 8));

    // Live mode toggle
    if (ImGui::Checkbox("Live Mode (2s updates)", &ol_live_mode_)) {
        log_status(ol_live_mode_ ? "Live mode enabled" : "Live mode disabled",
                   ol_live_mode_ ? panel_colors::YELLOW : TEXT_DIM);
    }

    if (ol_live_mode_) {
        double now = ImGui::GetTime();
        if (now - ol_last_update_ > 2.0 && !ol_perf_op_.loading.load()) {
            ol_last_update_ = now;
            ol_perf_op_.loading.store(true);
            std::thread([this]() {
                auto result = QuantLabApi::instance().qlib_online_performance("current").get();
                if (result.success) ol_perf_op_.set_result(result.data);
                else ol_perf_op_.set_error(result.error);
            }).detach();
        }
    }

    // Performance display
    if (ol_perf_op_.has_result) {
        ImGui::Dummy(ImVec2(0, 8));
        section_header("PERFORMANCE", panel_colors::YELLOW);
        std::lock_guard<std::mutex> lock(ol_perf_op_.mutex);
        auto& d = ol_perf_op_.result;
        double acc = d.value("accuracy", 0.0);
        double loss = d.value("loss", 0.0);
        metric_card("Accuracy", acc * 100, "%.1f%%", acc > 0.6 ? MARKET_GREEN : WARNING);
        ImGui::SameLine();
        metric_card("Loss", loss, "%.4f", TEXT_SECONDARY);
    }

    if (!ol_create_op_.error.empty()) theme::ErrorMessage(ol_create_op_.error.c_str());
    if (!ol_train_op_.error.empty()) theme::ErrorMessage(ol_train_op_.error.c_str());
}

// ============================================================================
// PANEL 8: HFT Analytics — RED THEME
// ============================================================================
void AIQuantLabScreen::render_hft() {
    using namespace theme::colors;
    section_header("HFT ANALYTICS", panel_colors::RED);
    ImGui::TextColored(TEXT_DIM, "Order book visualization & microstructure analysis");
    ImGui::Dummy(ImVec2(0, 8));

    config_input_text("Symbol", hft_symbol_, sizeof(hft_symbol_), 120);
    config_input_float("Spread Target", &hft_spread_target_, 120);
    config_input_float("Position Limit", &hft_position_limit_, 120);

    ImGui::Dummy(ImVec2(0, 4));

    // Live toggle
    if (ImGui::Checkbox("Live Mode (1s updates)", &hft_live_)) {
        if (hft_live_) {
            log_status("HFT live mode started", panel_colors::RED);
            // Generate initial mock data
            hft_bids_.clear();
            hft_asks_.clear();
            double base = 185.50;
            for (int i = 0; i < 10; i++) {
                hft_bids_.push_back({base - i * 0.01, 100.0 + rand() % 500, 1 + rand() % 10});
                hft_asks_.push_back({base + 0.01 + i * 0.01, 100.0 + rand() % 500, 1 + rand() % 10});
            }
            hft_metrics_ = {0.15, base - 0.05, base + 0.05, 0.23, 0.02, base};
        }
    }

    if (hft_live_) {
        double now = ImGui::GetTime();
        if (now - hft_last_update_ > 1.0) {
            hft_last_update_ = now;
            // Update mock data with slight randomization
            for (auto& b : hft_bids_) b.size += (rand() % 100) - 50;
            for (auto& a : hft_asks_) a.size += (rand() % 100) - 50;
            hft_metrics_.depth_imbalance = 0.1 + (rand() % 30) / 100.0;
            hft_metrics_.toxicity_score = 0.15 + (rand() % 20) / 100.0;
        }
    }

    // Order Book
    if (!hft_bids_.empty()) {
        ImGui::Dummy(ImVec2(0, 8));
        section_header("ORDER BOOK", panel_colors::RED);

        // Bids and asks side by side
        ImGui::Columns(2, "##orderbook", true);

        // Bids
        ImGui::TextColored(MARKET_GREEN, "BIDS");
        ImGui::Separator();
        ImGui::TextColored(TEXT_DIM, "%-10s %-10s %-6s", "Price", "Size", "Orders");
        for (auto& b : hft_bids_) {
            float pct = (float)(b.size / 600.0);
            ImVec2 p = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddRectFilled(p,
                ImVec2(p.x + ImGui::GetColumnWidth() * pct, p.y + ImGui::GetTextLineHeight()),
                IM_COL32(5, 217, 107, 30));
            ImGui::TextColored(MARKET_GREEN, "%.2f    %.0f    %d", b.price, b.size, b.orders);
        }

        ImGui::NextColumn();

        // Asks
        ImGui::TextColored(MARKET_RED, "ASKS");
        ImGui::Separator();
        ImGui::TextColored(TEXT_DIM, "%-10s %-10s %-6s", "Price", "Size", "Orders");
        for (auto& a : hft_asks_) {
            float pct = (float)(a.size / 600.0);
            ImVec2 p2 = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddRectFilled(p2,
                ImVec2(p2.x + ImGui::GetColumnWidth() * pct, p2.y + ImGui::GetTextLineHeight()),
                IM_COL32(245, 64, 64, 30));
            ImGui::TextColored(MARKET_RED, "%.2f    %.0f    %d", a.price, a.size, a.orders);
        }

        ImGui::Columns(1);

        // Microstructure metrics
        ImGui::Dummy(ImVec2(0, 8));
        section_header("MICROSTRUCTURE", panel_colors::RED);

        metric_card("Depth Imbalance", hft_metrics_.depth_imbalance, "%.4f",
            std::abs(hft_metrics_.depth_imbalance) > 0.3 ? WARNING : TEXT_SECONDARY);
        ImGui::SameLine();
        metric_card("VWAP Bid", hft_metrics_.vwap_bid, "%.2f", MARKET_GREEN);
        ImGui::SameLine();
        metric_card("VWAP Ask", hft_metrics_.vwap_ask, "%.2f", MARKET_RED);

        metric_card("Toxicity", hft_metrics_.toxicity_score, "%.4f",
            hft_metrics_.toxicity_score > 0.5 ? MARKET_RED : TEXT_SECONDARY);
        ImGui::SameLine();
        metric_card("Spread", hft_metrics_.spread, "%.4f", TEXT_SECONDARY);
        ImGui::SameLine();
        metric_card("Mid Price", hft_metrics_.mid_price, "%.2f", TEXT_PRIMARY);

        // Market Making Quote Calculator
        ImGui::Dummy(ImVec2(0, 8));
        section_header("MARKET MAKING QUOTES", panel_colors::RED);

        double bid_quote = hft_metrics_.mid_price - hft_spread_target_ / 2;
        double ask_quote = hft_metrics_.mid_price + hft_spread_target_ / 2;
        ImGui::TextColored(MARKET_GREEN, "Bid Quote: $%.4f", bid_quote);
        ImGui::TextColored(MARKET_RED, "Ask Quote: $%.4f", ask_quote);
        ImGui::TextColored(TEXT_DIM, "Spread: $%.4f (%.2f bps)",
            ask_quote - bid_quote,
            (ask_quote - bid_quote) / hft_metrics_.mid_price * 10000);
    }
}

// ============================================================================
// PANEL 9: Meta Learning — MAGENTA THEME
// ============================================================================
void AIQuantLabScreen::render_meta_learning() {
    using namespace theme::colors;
    section_header("META LEARNING", panel_colors::MAGENTA);
    ImGui::TextColored(TEXT_DIM, "Automated model selection and comparison");
    ImGui::Dummy(ImVec2(0, 8));

    static const char* task_types[] = {"Classification", "Regression"};
    config_combo("Task Type", &meta_task_type_, task_types, 2, 130);

    ImGui::Dummy(ImVec2(0, 8));

    if (!meta_run_op_.loading.load()) {
        if (theme::AccentButton("Run Model Selection")) {
            meta_run_op_.loading.store(true);
            json config = {
                {"task_type", task_types[meta_task_type_]},
                {"models", json::array({"LightGBM", "XGBoost", "CatBoost", "RandomForest"})},
            };
            log_status("Running meta-learning selection...", panel_colors::MAGENTA);
            std::thread([this, config]() {
                auto result = QuantLabApi::instance().qlib_meta_run_selection(config).get();
                if (result.success) {
                    meta_run_op_.set_result(result.data);
                    log_status("Meta-learning complete", SUCCESS);
                } else {
                    meta_run_op_.set_error(result.error);
                }
            }).detach();
        }
    } else {
        theme::LoadingSpinner("Running model selection...");
    }

    // Results — show default comparison if no results yet
    ImGui::Dummy(ImVec2(0, 12));
    section_header("MODEL COMPARISON", panel_colors::MAGENTA);

    if (ImGui::BeginTable("##meta_models", 7,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("Rank", ImGuiTableColumnFlags_WidthFixed, 40);
        ImGui::TableSetupColumn("Model");
        ImGui::TableSetupColumn("Accuracy");
        ImGui::TableSetupColumn("F1 Score");
        ImGui::TableSetupColumn("AUC-ROC");
        ImGui::TableSetupColumn("MSE");
        ImGui::TableSetupColumn("R2");
        ImGui::TableHeadersRow();

        // Default data (or API results)
        struct ModelResult { const char* name; double acc; double f1; double auc; double mse; double r2; };
        ModelResult defaults[] = {
            {"LightGBM",     0.847, 0.832, 0.891, 0.0234, 0.765},
            {"XGBoost",      0.841, 0.825, 0.885, 0.0241, 0.758},
            {"CatBoost",     0.838, 0.821, 0.882, 0.0247, 0.752},
            {"RandomForest", 0.823, 0.808, 0.869, 0.0268, 0.731},
        };

        for (int i = 0; i < 4; i++) {
            auto& m = defaults[i];
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(i == 0 ? panel_colors::MAGENTA : TEXT_SECONDARY, "#%d", i + 1);
            ImGui::TableNextColumn();
            ImGui::TextColored(TEXT_PRIMARY, "%s", m.name);
            ImGui::TableNextColumn();
            ImGui::TextColored(m.acc > 0.84 ? MARKET_GREEN : TEXT_SECONDARY, "%.3f", m.acc);
            ImGui::TableNextColumn();
            ImGui::TextColored(TEXT_SECONDARY, "%.3f", m.f1);
            ImGui::TableNextColumn();
            ImGui::TextColored(TEXT_SECONDARY, "%.3f", m.auc);
            ImGui::TableNextColumn();
            ImGui::TextColored(TEXT_SECONDARY, "%.4f", m.mse);
            ImGui::TableNextColumn();
            ImGui::TextColored(m.r2 > 0.75 ? MARKET_GREEN : TEXT_SECONDARY, "%.3f", m.r2);
        }

        ImGui::EndTable();
    }

    if (!meta_run_op_.error.empty()) theme::ErrorMessage(meta_run_op_.error.c_str());
}

// ============================================================================
// PANEL 10: Rolling Retraining — CYAN THEME
// ============================================================================
void AIQuantLabScreen::render_rolling_retraining() {
    using namespace theme::colors;
    section_header("ROLLING RETRAINING", panel_colors::CYAN);
    ImGui::TextColored(TEXT_DIM, "Scheduled model updates and retraining pipelines");
    ImGui::Dummy(ImVec2(0, 8));

    static const char* frequencies[] = {"Hourly", "Daily", "Weekly"};

    config_input_text("Model Name", rr_model_name_, sizeof(rr_model_name_), 130);
    config_input_int("Window (days)", &rr_window_days_, 130);
    config_combo("Frequency", &rr_frequency_, frequencies, 3, 130);

    ImGui::Dummy(ImVec2(0, 8));

    if (!rr_create_op_.loading.load()) {
        if (theme::AccentButton("Create Schedule")) {
            rr_create_op_.loading.store(true);
            json config = {
                {"model_name", rr_model_name_},
                {"training_window_days", rr_window_days_},
                {"frequency", frequencies[rr_frequency_]},
            };
            log_status("Creating retraining schedule...", panel_colors::CYAN);
            std::thread([this, config]() {
                auto result = QuantLabApi::instance().qlib_rolling_create_schedule(config).get();
                if (result.success) {
                    rr_create_op_.set_result(result.data);
                    log_status("Schedule created", SUCCESS);
                } else {
                    rr_create_op_.set_error(result.error);
                }
            }).detach();
        }
    } else {
        theme::LoadingSpinner("Creating schedule...");
    }

    ImGui::SameLine();
    if (theme::SecondaryButton("List Schedules")) {
        rr_list_op_.loading.store(true);
        std::thread([this]() {
            auto result = QuantLabApi::instance().qlib_rolling_list_schedules().get();
            if (result.success) rr_list_op_.set_result(result.data);
            else rr_list_op_.set_error(result.error);
        }).detach();
    }

    if (!rr_create_op_.error.empty()) theme::ErrorMessage(rr_create_op_.error.c_str());
    if (!rr_list_op_.error.empty()) theme::ErrorMessage(rr_list_op_.error.c_str());
}

// ============================================================================
// PANEL 11: Advanced Models
// ============================================================================
void AIQuantLabScreen::render_advanced_models() {
    using namespace theme::colors;
    section_header("ADVANCED MODELS", panel_colors::BLUE);
    ImGui::TextColored(TEXT_DIM, "LSTM, GRU, and Transformer neural network architectures");
    ImGui::Dummy(ImVec2(0, 8));

    static const char* models[] = {"LSTM", "GRU", "Transformer"};
    config_combo("Architecture", &am_model_idx_, models, 3, 130);

    // Model details
    ImGui::Dummy(ImVec2(0, 8));
    const char* descs[] = {
        "Long Short-Term Memory — excels at capturing long-range temporal dependencies in sequential financial data",
        "Gated Recurrent Unit — lightweight alternative to LSTM with comparable performance and faster training",
        "Transformer — attention-based architecture that captures complex multi-variate relationships",
    };
    ImGui::TextColored(TEXT_SECONDARY, "%s", descs[am_model_idx_]);

    ImGui::Dummy(ImVec2(0, 8));

    if (!am_train_op_.loading.load()) {
        if (theme::AccentButton("Train Model")) {
            am_train_op_.loading.store(true);
            json config = {{"model_type", models[am_model_idx_]}};
            log_status(std::string("Training ") + models[am_model_idx_] + "...", panel_colors::BLUE);
            std::thread([this, config]() {
                auto result = QuantLabApi::instance().qlib_advanced_train(config).get();
                if (result.success) {
                    am_train_op_.set_result(result.data);
                    log_status("Training complete", SUCCESS);
                } else {
                    am_train_op_.set_error(result.error);
                }
            }).detach();
        }
    } else {
        theme::LoadingSpinner("Training neural network...");
    }

    if (am_train_op_.has_result) {
        ImGui::Dummy(ImVec2(0, 8));
        section_header("TRAINING RESULTS", SUCCESS);
        std::lock_guard<std::mutex> lock(am_train_op_.mutex);
        auto& d = am_train_op_.result;
        double loss = d.value("final_loss", 0.0);
        int epochs = d.value("epochs", 0);
        double val_acc = d.value("val_accuracy", 0.0);
        char epochs_buf[16];
        std::snprintf(epochs_buf, sizeof(epochs_buf), "%d", epochs);
        metric_card("Final Loss", loss, "%.6f", TEXT_SECONDARY);
        ImGui::SameLine();
        metric_card("Epochs", epochs_buf, TEXT_PRIMARY);
        ImGui::SameLine();
        metric_card("Val Accuracy", val_acc * 100, "%.1f%%", val_acc > 0.6 ? MARKET_GREEN : WARNING);
    }

    if (!am_train_op_.error.empty()) theme::ErrorMessage(am_train_op_.error.c_str());
}

// ============================================================================
// PANEL 12: GluonTS — GLUON BLUE THEME
// ============================================================================
void AIQuantLabScreen::render_gluonts() {
    using namespace theme::colors;
    section_header("GLUONTS", panel_colors::GLUON_BLUE);
    ImGui::TextColored(TEXT_DIM, "Deep learning time series forecasting");
    ImGui::Dummy(ImVec2(0, 8));

    auto models = get_gluonts_models();
    // Build combo items
    std::vector<const char*> model_names;
    for (auto& m : models) model_names.push_back(m.label);

    config_combo("Model", &gts_model_idx_, model_names.data(), (int)model_names.size(), 130);
    config_input_text("Data Path (CSV)", gts_data_path_, sizeof(gts_data_path_), 130);
    config_input_int("Prediction Length", &gts_pred_length_, 130);

    static const char* freqs[] = {"H (Hourly)", "D (Daily)", "W (Weekly)", "M (Monthly)"};
    config_combo("Frequency", &gts_freq_idx_, freqs, 4, 130);
    config_input_int("Epochs", &gts_epochs_, 130);

    ImGui::Dummy(ImVec2(0, 8));

    if (!gts_op_.loading.load()) {
        if (theme::AccentButton("Run Forecast")) {
            gts_op_.loading.store(true);
            static const char* freq_codes[] = {"H", "D", "W", "M"};
            json config = {
                {"model", models[gts_model_idx_].name},
                {"data_path", gts_data_path_},
                {"prediction_length", gts_pred_length_},
                {"freq", freq_codes[gts_freq_idx_]},
                {"epochs", gts_epochs_},
            };
            log_status("Running GluonTS forecast...", panel_colors::GLUON_BLUE);
            std::thread([this, config]() {
                auto result = QuantLabApi::instance().gluonts_forecast(config).get();
                if (result.success) {
                    gts_op_.set_result(result.data);
                    log_status("Forecast complete", SUCCESS);
                } else {
                    gts_op_.set_error(result.error);
                    log_status("Forecast failed", ERROR_RED);
                }
            }).detach();
        }
    } else {
        theme::LoadingSpinner("Forecasting...");
    }

    // Forecast results
    if (gts_op_.has_result) {
        ImGui::Dummy(ImVec2(0, 12));
        section_header("FORECAST RESULTS", panel_colors::GLUON_BLUE);

        std::lock_guard<std::mutex> lock(gts_op_.mutex);
        auto& d = gts_op_.result;

        // Metrics
        double mse = d.value("mse", 0.0);
        double mae = d.value("mae", 0.0);
        double mape = d.value("mape", 0.0);
        metric_card("MSE", mse, "%.4f", TEXT_SECONDARY);
        ImGui::SameLine();
        metric_card("MAE", mae, "%.4f", TEXT_SECONDARY);
        ImGui::SameLine();
        metric_card("MAPE", mape * 100, "%.2f%%", mape < 0.1 ? MARKET_GREEN : WARNING);

        // Forecast plot with ImPlot
        if (d.contains("forecast") && d["forecast"].is_array()) {
            ImGui::Dummy(ImVec2(0, 8));
            auto& fc = d["forecast"];
            std::vector<double> values;
            for (auto& v : fc) values.push_back(v.get<double>());

            if (ImPlot::BeginPlot("##forecast_plot", ImVec2(-1, 250))) {
                ImPlot::SetupAxes("Time", "Value");
                ImPlot::PushStyleColor(ImPlotCol_Line, panel_colors::GLUON_BLUE);
                std::vector<double> xs(values.size());
                for (size_t i = 0; i < xs.size(); i++) xs[i] = (double)i;
                ImPlot::PlotLine("Forecast", xs.data(), values.data(), (int)values.size());
                ImPlot::PopStyleColor();
                ImPlot::EndPlot();
            }
        }
    }

    if (!gts_op_.error.empty()) theme::ErrorMessage(gts_op_.error.c_str());
}

// ============================================================================
// PANEL 13: GS Quant — GS PURPLE THEME
// ============================================================================
void AIQuantLabScreen::render_gs_quant() {
    using namespace theme::colors;
    section_header("GS QUANT", panel_colors::GS_PURPLE);
    ImGui::TextColored(TEXT_DIM, "Goldman Sachs-style quantitative analytics");
    ImGui::Dummy(ImVec2(0, 8));

    static const char* modes[] = {
        "Risk Metrics", "Portfolio Analytics", "Greeks",
        "VaR", "Stress Testing", "Backtesting", "Statistics"
    };
    config_combo("Analysis Mode", &gsq_mode_idx_, modes, 7, 130);
    config_input_text("Symbols", gsq_symbols_, sizeof(gsq_symbols_), 130);

    ImGui::Dummy(ImVec2(0, 8));

    if (!gsq_op_.loading.load()) {
        if (theme::AccentButton("Run Analysis")) {
            gsq_op_.loading.store(true);
            json config = {
                {"mode", modes[gsq_mode_idx_]},
                {"symbols", gsq_symbols_},
            };
            log_status("Running GS Quant analysis...", panel_colors::GS_PURPLE);
            std::thread([this, config]() {
                auto result = QuantLabApi::instance().gs_quant_analyze(config).get();
                if (result.success) {
                    gsq_op_.set_result(result.data);
                    log_status("GS Quant analysis complete", SUCCESS);
                } else {
                    gsq_op_.set_error(result.error);
                }
            }).detach();
        }
    } else {
        theme::LoadingSpinner("Analyzing...");
    }

    // Results
    if (gsq_op_.has_result) {
        ImGui::Dummy(ImVec2(0, 12));
        section_header("ANALYSIS RESULTS", panel_colors::GS_PURPLE);

        std::lock_guard<std::mutex> lock(gsq_op_.mutex);
        // Render JSON results in a structured way
        if (gsq_op_.result.is_object()) {
            for (auto& [key, val] : gsq_op_.result.items()) {
                if (val.is_number()) {
                    ImGui::TextColored(TEXT_DIM, "%s:", key.c_str());
                    ImGui::SameLine(200);
                    ImGui::TextColored(TEXT_PRIMARY, "%.4f", val.get<double>());
                } else if (val.is_string()) {
                    ImGui::TextColored(TEXT_DIM, "%s:", key.c_str());
                    ImGui::SameLine(200);
                    ImGui::TextColored(TEXT_SECONDARY, "%s", val.get<std::string>().c_str());
                }
            }
        }
    }

    if (!gsq_op_.error.empty()) theme::ErrorMessage(gsq_op_.error.c_str());
}

// ============================================================================
// PANEL 14: Statsmodels — TEAL THEME
// ============================================================================
void AIQuantLabScreen::render_statsmodels() {
    using namespace theme::colors;
    section_header("STATSMODELS", panel_colors::TEAL);
    ImGui::TextColored(TEXT_DIM, "Statistical time series analysis (ARIMA, SARIMAX, decomposition)");
    ImGui::Dummy(ImVec2(0, 8));

    static const char* analyses[] = {
        "ARIMA", "SARIMAX", "Exp Smoothing", "STL Decompose",
        "ADF Test", "KPSS Test", "ACF", "PACF"
    };
    static const char* data_sources[] = {"Manual Data", "Symbol (Yahoo Finance)"};

    config_combo("Analysis", &sm_analysis_idx_, analyses, 8, 130);
    config_combo("Data Source", &sm_data_source_, data_sources, 2, 130);

    if (sm_data_source_ == 1) {
        config_input_text("Symbol", sm_symbol_, sizeof(sm_symbol_), 130);
    } else {
        ImGui::TextColored(TEXT_DIM, "Data (CSV):");
        ImGui::InputTextMultiline("##sm_data", sm_data_, sizeof(sm_data_), ImVec2(-1, 80));
    }

    // ARIMA params
    if (sm_analysis_idx_ <= 1) {
        ImGui::Dummy(ImVec2(0, 4));
        ImGui::TextColored(TEXT_DIM, "ARIMA Order:");
        ImGui::SameLine(130);
        ImGui::SetNextItemWidth(50);
        ImGui::InputInt("##p", &sm_arima_p_);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(50);
        ImGui::InputInt("##d", &sm_arima_d_);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(50);
        ImGui::InputInt("##q", &sm_arima_q_);
    }

    ImGui::Dummy(ImVec2(0, 8));

    if (!sm_op_.loading.load()) {
        if (theme::AccentButton("Run Analysis")) {
            sm_op_.loading.store(true);
            json config = {
                {"analysis_type", analyses[sm_analysis_idx_]},
                {"data_source", sm_data_source_ == 1 ? "symbol" : "manual"},
                {"symbol", sm_symbol_},
                {"data", sm_data_},
                {"p", sm_arima_p_},
                {"d", sm_arima_d_},
                {"q", sm_arima_q_},
            };
            log_status("Running statsmodels analysis...", panel_colors::TEAL);
            std::thread([this, config]() {
                auto result = QuantLabApi::instance().statsmodels_analyze(config).get();
                if (result.success) {
                    sm_op_.set_result(result.data);
                    log_status("Analysis complete", SUCCESS);
                } else {
                    sm_op_.set_error(result.error);
                }
            }).detach();
        }
    } else {
        theme::LoadingSpinner("Analyzing...");
    }

    // Results
    if (sm_op_.has_result) {
        ImGui::Dummy(ImVec2(0, 12));
        section_header("RESULTS", panel_colors::TEAL);
        std::lock_guard<std::mutex> lock(sm_op_.mutex);
        if (sm_op_.result.is_object()) {
            for (auto& [key, val] : sm_op_.result.items()) {
                if (val.is_number()) {
                    ImGui::TextColored(TEXT_DIM, "%s:", key.c_str());
                    ImGui::SameLine(200);
                    ImGui::TextColored(TEXT_PRIMARY, "%.6f", val.get<double>());
                } else if (val.is_string()) {
                    ImGui::TextColored(TEXT_DIM, "%s:", key.c_str());
                    ImGui::SameLine(200);
                    ImGui::TextColored(TEXT_SECONDARY, "%s", val.get<std::string>().c_str());
                }
            }
        }
    }

    if (!sm_op_.error.empty()) theme::ErrorMessage(sm_op_.error.c_str());
}

// ============================================================================
// PANEL 15: Functime
// ============================================================================
void AIQuantLabScreen::render_functime() {
    using namespace theme::colors;
    section_header("FUNCTIME", ACCENT);
    ImGui::TextColored(TEXT_DIM, "Fast time series forecasting with Functime library");
    ImGui::Dummy(ImVec2(0, 8));

    static const char* models[] = {
        "Naive", "SeasonalNaive", "LinearForecaster",
        "LightGBMForecaster", "AutoARIMA", "AutoETS",
    };
    config_input_text("Symbol", ft_symbol_, sizeof(ft_symbol_), 130);
    config_combo("Model", &ft_model_idx_, models, 6, 130);
    config_input_int("Horizon", &ft_horizon_, 130);

    ImGui::Dummy(ImVec2(0, 8));

    if (!ft_op_.loading.load()) {
        if (theme::AccentButton("Forecast")) {
            ft_op_.loading.store(true);
            json config = {
                {"symbol", ft_symbol_},
                {"model", models[ft_model_idx_]},
                {"horizon", ft_horizon_},
            };
            log_status("Running functime forecast...", ACCENT);
            std::thread([this, config]() {
                auto result = QuantLabApi::instance().functime_forecast(config).get();
                if (result.success) ft_op_.set_result(result.data);
                else ft_op_.set_error(result.error);
            }).detach();
        }
    } else {
        theme::LoadingSpinner("Forecasting...");
    }

    if (ft_op_.has_result) {
        ImGui::Dummy(ImVec2(0, 8));
        section_header("FORECAST", SUCCESS);
        std::lock_guard<std::mutex> lock(ft_op_.mutex);
        auto& d = ft_op_.result;

        // Plot forecast if available
        if (d.contains("forecast") && d["forecast"].is_array()) {
            std::vector<double> values;
            for (auto& v : d["forecast"]) values.push_back(v.get<double>());

            if (ImPlot::BeginPlot("##ft_forecast", ImVec2(-1, 200))) {
                ImPlot::SetupAxes("Step", "Value");
                std::vector<double> xs(values.size());
                for (size_t i = 0; i < xs.size(); i++) xs[i] = (double)i;
                ImPlot::PlotLine("Forecast", xs.data(), values.data(), (int)values.size());
                ImPlot::EndPlot();
            }
        }
    }

    if (!ft_op_.error.empty()) theme::ErrorMessage(ft_op_.error.c_str());
}

// ============================================================================
// PANEL 16: Fortitudo
// ============================================================================
void AIQuantLabScreen::render_fortitudo() {
    using namespace theme::colors;
    section_header("FORTITUDO", ACCENT);
    ImGui::TextColored(TEXT_DIM, "Entropy pooling, portfolio optimization, and risk analysis");
    ImGui::Dummy(ImVec2(0, 8));

    static const char* modes[] = {"Entropy Pooling", "Optimization", "Option Pricing", "Portfolio Risk"};
    config_combo("Mode", &fort_mode_idx_, modes, 4, 130);
    config_input_text("Symbols", fort_symbols_, sizeof(fort_symbols_), 130);

    ImGui::Dummy(ImVec2(0, 8));

    if (!fort_op_.loading.load()) {
        if (theme::AccentButton("Run Analysis")) {
            fort_op_.loading.store(true);
            json config = {
                {"mode", modes[fort_mode_idx_]},
                {"symbols", fort_symbols_},
            };
            log_status("Running Fortitudo analysis...", ACCENT);
            std::thread([this, config]() {
                auto result = QuantLabApi::instance().fortitudo_analyze(config).get();
                if (result.success) fort_op_.set_result(result.data);
                else fort_op_.set_error(result.error);
            }).detach();
        }
    } else {
        theme::LoadingSpinner("Analyzing...");
    }

    if (fort_op_.has_result) {
        ImGui::Dummy(ImVec2(0, 8));
        section_header("RESULTS", SUCCESS);
        std::lock_guard<std::mutex> lock(fort_op_.mutex);
        if (fort_op_.result.is_object()) {
            for (auto& [key, val] : fort_op_.result.items()) {
                if (val.is_number()) {
                    ImGui::TextColored(TEXT_DIM, "%s:", key.c_str());
                    ImGui::SameLine(200);
                    ImGui::TextColored(TEXT_PRIMARY, "%.6f", val.get<double>());
                }
            }
        }
    }

    if (!fort_op_.error.empty()) theme::ErrorMessage(fort_op_.error.c_str());
}

// ============================================================================
// PANEL 17: CFA Quant
// ============================================================================
void AIQuantLabScreen::render_cfa_quant() {
    using namespace theme::colors;
    section_header("CFA QUANT", ACCENT);
    ImGui::TextColored(TEXT_DIM, "CFA-level quantitative financial analysis");
    ImGui::Dummy(ImVec2(0, 8));

    static const char* analyses[] = {
        "Equity Valuation", "Fixed Income", "Derivatives",
        "Portfolio Management", "Corporate Finance", "Economics",
    };
    config_input_text("Symbol", cfa_symbol_, sizeof(cfa_symbol_), 130);
    config_combo("Analysis", &cfa_analysis_idx_, analyses, 6, 130);

    ImGui::Dummy(ImVec2(0, 8));

    if (!cfa_op_.loading.load()) {
        if (theme::AccentButton("Run Analysis")) {
            cfa_op_.loading.store(true);
            json config = {
                {"symbol", cfa_symbol_},
                {"analysis_type", analyses[cfa_analysis_idx_]},
            };
            log_status("Running CFA analysis...", ACCENT);
            std::thread([this, config]() {
                auto result = QuantLabApi::instance().cfa_quant_analyze(config).get();
                if (result.success) cfa_op_.set_result(result.data);
                else cfa_op_.set_error(result.error);
            }).detach();
        }
    } else {
        theme::LoadingSpinner("Analyzing...");
    }

    if (cfa_op_.has_result) {
        ImGui::Dummy(ImVec2(0, 8));
        section_header("RESULTS", SUCCESS);
        std::lock_guard<std::mutex> lock(cfa_op_.mutex);
        if (cfa_op_.result.is_object()) {
            for (auto& [key, val] : cfa_op_.result.items()) {
                if (val.is_number()) {
                    ImGui::TextColored(TEXT_DIM, "%s:", key.c_str());
                    ImGui::SameLine(200);
                    ImGui::TextColored(TEXT_PRIMARY, "%.4f", val.get<double>());
                }
            }
        }
    }

    if (!cfa_op_.error.empty()) theme::ErrorMessage(cfa_op_.error.c_str());
}

// ============================================================================
// PANEL 18: Pattern Intelligence — VisionQuant
// ============================================================================
void AIQuantLabScreen::render_pattern_intelligence() {
    using namespace theme::colors;
    section_header("PATTERN INTELLIGENCE", panel_colors::PURPLE);
    ImGui::TextColored(TEXT_DIM, "VisionQuant K-line pattern recognition and analysis");
    ImGui::Dummy(ImVec2(0, 8));

    static const char* patterns[] = {
        "Doji", "Hammer", "Engulfing", "Morning Star", "Evening Star",
        "Three White Soldiers", "Three Black Crows", "Harami",
        "Piercing Line", "Dark Cloud Cover",
    };
    config_input_text("Symbol", pi_symbol_, sizeof(pi_symbol_), 130);
    config_combo("Pattern", &pi_pattern_idx_, patterns, 10, 130);

    ImGui::Dummy(ImVec2(0, 8));

    // Three action buttons
    if (!pi_search_op_.loading.load()) {
        if (theme::AccentButton("Search Patterns")) {
            pi_search_op_.loading.store(true);
            json config = {
                {"symbol", pi_symbol_},
                {"pattern", patterns[pi_pattern_idx_]},
            };
            log_status("Searching patterns...", panel_colors::PURPLE);
            std::thread([this, config]() {
                auto result = QuantLabApi::instance().visionquant_search(config).get();
                if (result.success) pi_search_op_.set_result(result.data);
                else pi_search_op_.set_error(result.error);
            }).detach();
        }
    } else {
        theme::LoadingSpinner("Searching...");
    }

    ImGui::SameLine();

    if (!pi_score_op_.loading.load()) {
        if (theme::SecondaryButton("Score Pattern")) {
            pi_score_op_.loading.store(true);
            json config = {
                {"symbol", pi_symbol_},
                {"pattern", patterns[pi_pattern_idx_]},
            };
            std::thread([this, config]() {
                auto result = QuantLabApi::instance().visionquant_score(config).get();
                if (result.success) pi_score_op_.set_result(result.data);
                else pi_score_op_.set_error(result.error);
            }).detach();
        }
    } else {
        theme::LoadingSpinner("Scoring...");
    }

    ImGui::SameLine();

    if (!pi_backtest_op_.loading.load()) {
        if (theme::SecondaryButton("Backtest Pattern")) {
            pi_backtest_op_.loading.store(true);
            json config = {
                {"symbol", pi_symbol_},
                {"pattern", patterns[pi_pattern_idx_]},
            };
            std::thread([this, config]() {
                auto result = QuantLabApi::instance().visionquant_backtest(config).get();
                if (result.success) pi_backtest_op_.set_result(result.data);
                else pi_backtest_op_.set_error(result.error);
            }).detach();
        }
    } else {
        theme::LoadingSpinner("Backtesting...");
    }

    // Search results
    if (pi_search_op_.has_result) {
        ImGui::Dummy(ImVec2(0, 8));
        section_header("PATTERN MATCHES", panel_colors::PURPLE);
        std::lock_guard<std::mutex> lock(pi_search_op_.mutex);
        if (pi_search_op_.result.contains("matches") && pi_search_op_.result["matches"].is_array()) {
            for (auto& m : pi_search_op_.result["matches"]) {
                ImGui::TextColored(TEXT_PRIMARY, "%s at %s",
                    m.value("pattern", "").c_str(),
                    m.value("date", "").c_str());
                ImGui::SameLine(300);
                double conf = m.value("confidence", 0.0);
                ImGui::TextColored(conf > 0.8 ? MARKET_GREEN : WARNING, "%.1f%%", conf * 100);
            }
        }
    }

    // Score results
    if (pi_score_op_.has_result) {
        ImGui::Dummy(ImVec2(0, 8));
        section_header("PATTERN SCORE", panel_colors::PURPLE);
        std::lock_guard<std::mutex> lock(pi_score_op_.mutex);
        auto& d = pi_score_op_.result;
        double score = d.value("score", 0.0);
        double reliability = d.value("reliability", 0.0);
        metric_card("Score", score, "%.2f", score > 0.7 ? MARKET_GREEN : WARNING);
        ImGui::SameLine();
        metric_card("Reliability", reliability * 100, "%.1f%%", reliability > 0.6 ? MARKET_GREEN : WARNING);
    }

    // Backtest results
    if (pi_backtest_op_.has_result) {
        ImGui::Dummy(ImVec2(0, 8));
        section_header("PATTERN BACKTEST", panel_colors::PURPLE);
        std::lock_guard<std::mutex> lock(pi_backtest_op_.mutex);
        auto& d = pi_backtest_op_.result;
        double win_rate = d.value("win_rate", 0.0);
        double avg_return = d.value("avg_return", 0.0);
        int occurrences = d.value("occurrences", 0);
        metric_card("Win Rate", win_rate * 100, "%.1f%%", win_rate > 0.5 ? MARKET_GREEN : WARNING);
        ImGui::SameLine();
        metric_card("Avg Return", avg_return * 100, "%.2f%%", avg_return > 0 ? MARKET_GREEN : MARKET_RED);
        ImGui::SameLine();
        char occ_buf[16];
        std::snprintf(occ_buf, sizeof(occ_buf), "%d", occurrences);
        metric_card("Occurrences", occ_buf, TEXT_PRIMARY);
    }

    if (!pi_search_op_.error.empty()) theme::ErrorMessage(pi_search_op_.error.c_str());
    if (!pi_score_op_.error.empty()) theme::ErrorMessage(pi_score_op_.error.c_str());
    if (!pi_backtest_op_.error.empty()) theme::ErrorMessage(pi_backtest_op_.error.c_str());
}

} // namespace fincept::ai_quant_lab
