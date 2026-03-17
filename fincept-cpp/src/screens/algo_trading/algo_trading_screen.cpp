#include "algo_trading_screen.h"
#include "algo_service.h"
#include "storage/database.h"
#include "ui/yoga_helpers.h"
#include "core/notification.h"
#include <thread>

namespace fincept::algo {

void AlgoTradingScreen::init() {
    try {
        load_strategies();
        load_deployments();
        refresh_running_count();
    } catch (const std::exception& e) {
        status_msg_ = std::string("Init error: ") + e.what();
    }
    initialized_ = true;
}

void AlgoTradingScreen::render() {
    if (!initialized_) init();

    ui::ScreenFrame frame("##algo_trading_screen", ImVec2(0, 0),
                          ImVec4(0.05f, 0.05f, 0.07f, 1.0f));
    if (!frame.begin()) { frame.end(); return; }

    const float w = frame.width();
    const float h = frame.height();

    // Yoga vstack: nav(40) | content(flex) | status(28)
    auto vstack = ui::vstack_layout(w, h, {40.0f, -1, 28.0f});

    render_top_nav(w);

    const float content_h = vstack.heights[1];

    // Content area
    ImGui::BeginChild("##algo_content", ImVec2(w, content_h), false);
    switch (active_view_) {
        case SubView::Builder:    render_builder(w, content_h);    break;
        case SubView::Strategies: render_strategies(w, content_h); break;
        case SubView::Library:    render_library(w, content_h);    break;
        case SubView::Scanner:    render_scanner(w, content_h);    break;
        case SubView::Dashboard:  render_dashboard(w, content_h);  break;
    }
    ImGui::EndChild();

    render_status_bar(w);

    frame.end();
}

void AlgoTradingScreen::render_top_nav(float w) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.08f, 0.12f, 1.0f));
    ImGui::BeginChild("##algo_nav", ImVec2(w, 40.0f), false);

    struct NavItem { const char* label; SubView view; };
    static const NavItem items[] = {
        {"Strategy Builder",  SubView::Builder},
        {"My Strategies",     SubView::Strategies},
        {"Strategy Library",  SubView::Library},
        {"Scanner",           SubView::Scanner},
        {"Dashboard",         SubView::Dashboard},
    };

    ImGui::SetCursorPosY(6.0f);
    ImGui::SetCursorPosX(10.0f);

    for (const auto& item : items) {
        bool selected = (active_view_ == item.view);
        if (selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.40f, 0.80f, 1.0f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.15f, 0.20f, 1.0f));
        }
        if (ImGui::Button(item.label, ImVec2(0, 28))) {
            active_view_ = item.view;
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
    }

    // Running deployments indicator + emergency stop
    if (running_count_ > 0) {
        ImGui::SameLine(w - 250.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 1.0f, 0.2f, 1.0f));
        ImGui::Text("%d Running", running_count_);
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.1f, 0.1f, 1.0f));
        if (ImGui::SmallButton("STOP ALL")) stop_all_deployments();
        ImGui::PopStyleColor();
    }

    // Backtest indicator
    if (backtest_running_) {
        ImGui::SameLine(running_count_ > 0 ? 0 : w - 180.0f);
        ImGui::TextColored(ImVec4(0.4f, 0.7f, 1, 1), "Backtesting...");
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void AlgoTradingScreen::render_status_bar(float w) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.06f, 0.06f, 0.10f, 1.0f));
    ImGui::BeginChild("##algo_status", ImVec2(w, 28.0f), false);
    ImGui::SetCursorPos(ImVec2(10.0f, 6.0f));
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                       "Algo Trading | Strategies: %d | Deployments: %d | Running: %d",
                       (int)strategies_.size(), (int)deployments_.size(), running_count_);
    if (!status_msg_.empty()) {
        ImGui::SameLine(w - 400.0f);
        ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "%s", status_msg_.c_str());
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Builder sub-view
// ============================================================================

void AlgoTradingScreen::render_builder(float w, float h) {
    ImGui::Columns(2, "##builder_cols", true);
    ImGui::SetColumnWidth(0, w * 0.55f);

    // Left: Condition editor
    ImGui::BeginChild("##builder_left", ImVec2(0, h), false);

    ImGui::Text("Strategy Name");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##strat_name", strategy_name_, sizeof(strategy_name_));

    ImGui::Text("Symbols (comma-separated)");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##strat_symbols", symbols_buf_, sizeof(symbols_buf_));

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Entry Conditions");
    render_condition_editor("entry", entry_conditions_, entry_logic_idx_);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Exit Conditions");
    render_condition_editor("exit", exit_conditions_, exit_logic_idx_);

    ImGui::EndChild();

    // Right: Parameters
    ImGui::NextColumn();
    ImGui::BeginChild("##builder_right", ImVec2(0, h), false);

    ImGui::Text("Parameters");
    ImGui::Separator();

    ImGui::Text("Timeframe");
    auto& tfs = get_timeframes();
    if (ImGui::BeginCombo("##timeframe", tfs[timeframe_idx_].label)) {
        for (int i = 0; i < (int)tfs.size(); i++) {
            if (ImGui::Selectable(tfs[i].label, i == timeframe_idx_))
                timeframe_idx_ = i;
        }
        ImGui::EndCombo();
    }

    ImGui::Text("Provider");
    auto& provs = get_broker_providers();
    if (ImGui::BeginCombo("##provider", provs[provider_idx_].second)) {
        for (int i = 0; i < (int)provs.size(); i++) {
            if (ImGui::Selectable(provs[i].second, i == provider_idx_))
                provider_idx_ = i;
        }
        ImGui::EndCombo();
    }

    ImGui::Text("Stop Loss (%%)");
    ImGui::SliderFloat("##sl", &stop_loss_, 0, 20, "%.1f%%");
    ImGui::Text("Take Profit (%%)");
    ImGui::SliderFloat("##tp", &take_profit_, 0, 50, "%.1f%%");
    ImGui::Text("Trailing Stop (%%)");
    ImGui::SliderFloat("##ts", &trailing_stop_, 0, 20, "%.1f%%");
    ImGui::Text("Position Size");
    ImGui::InputFloat("##possize", &position_size_, 0.1f, 1.0f, "%.1f");
    ImGui::Text("Max Positions");
    ImGui::InputInt("##maxpos", &max_positions_);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Description");
    ImGui::InputTextMultiline("##strat_desc", strategy_desc_, sizeof(strategy_desc_),
                               ImVec2(-1, 80));

    ImGui::Spacing();
    if (ImGui::Button("Save Strategy", ImVec2(-1, 36))) {
        save_strategy();
    }
    if (!editing_strategy_id_.empty()) {
        if (ImGui::Button("Cancel Edit", ImVec2(-1, 28))) {
            editing_strategy_id_.clear();
            strategy_name_[0] = '\0';
            strategy_desc_[0] = '\0';
            symbols_buf_[0] = '\0';
            entry_conditions_.clear();
            exit_conditions_.clear();
        }
    }

    ImGui::EndChild();
    ImGui::Columns(1);
}

void AlgoTradingScreen::render_condition_editor(const char* label,
                                                 std::vector<ConditionItem>& conditions,
                                                 int& logic_idx) {
    ImGui::PushID(label);

    // Logic selector
    const char* logic_options[] = {"AND", "OR"};
    ImGui::Text("Logic:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80);
    ImGui::Combo("##logic", &logic_idx, logic_options, 2);

    ImGui::SameLine();
    if (ImGui::Button("+ Add Condition")) {
        ConditionItem c;
        c.indicator = "close";
        c.operator_type = ">";
        c.field = "value";
        conditions.push_back(c);
    }

    // Render each condition
    int remove_idx = -1;
    for (int i = 0; i < (int)conditions.size(); i++) {
        ImGui::PushID(i);
        bool remove = false;
        render_condition_row(conditions[i], i, remove);
        if (remove) remove_idx = i;
        ImGui::PopID();
    }

    if (remove_idx >= 0) {
        conditions.erase(conditions.begin() + remove_idx);
    }

    ImGui::PopID();
}

void AlgoTradingScreen::render_condition_row(ConditionItem& cond, int /*idx*/, bool& remove) {
    auto& indicators = get_indicators();
    auto& operators = get_operators();

    ImGui::BeginGroup();

    // Indicator selector
    ImGui::SetNextItemWidth(140);
    if (ImGui::BeginCombo("##ind", cond.indicator.c_str())) {
        for (auto& cat : get_indicator_categories()) {
            ImGui::TextDisabled("%s", cat.second);
            for (auto& ind : indicators) {
                if (std::string(ind.category) == cat.first) {
                    if (ImGui::Selectable(ind.name, cond.indicator == ind.id)) {
                        cond.indicator = ind.id;
                        if (ind.has_period) cond.period = ind.default_period;
                    }
                }
            }
            ImGui::Separator();
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();

    // Period input (if applicable)
    bool has_period = false;
    for (auto& ind : indicators) {
        if (ind.id == cond.indicator) { has_period = ind.has_period; break; }
    }
    if (has_period) {
        ImGui::SetNextItemWidth(60);
        ImGui::InputInt("##period", &cond.period, 0, 0);
        ImGui::SameLine();
    }

    // Operator selector
    ImGui::SetNextItemWidth(120);
    if (ImGui::BeginCombo("##op", cond.operator_type.c_str())) {
        for (auto& op : operators) {
            if (ImGui::Selectable(op.label, cond.operator_type == op.id))
                cond.operator_type = op.id;
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();

    // Value input
    ImGui::SetNextItemWidth(100);
    char vbuf[64];
    snprintf(vbuf, sizeof(vbuf), "%s", cond.value.c_str());
    if (ImGui::InputText("##val", vbuf, sizeof(vbuf))) {
        cond.value = vbuf;
    }

    // Second value for "between"
    for (auto& op : operators) {
        if (op.id == cond.operator_type && op.needs_value2) {
            ImGui::SameLine();
            ImGui::Text("and");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(100);
            char v2buf[64];
            snprintf(v2buf, sizeof(v2buf), "%s", cond.value2.c_str());
            if (ImGui::InputText("##val2", v2buf, sizeof(v2buf))) {
                cond.value2 = v2buf;
            }
            break;
        }
    }

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
    if (ImGui::Button("X")) remove = true;
    ImGui::PopStyleColor();

    ImGui::EndGroup();
}

// ============================================================================
// Strategies sub-view
// ============================================================================

void AlgoTradingScreen::render_strategies(float w, float h) {
    ImGui::BeginChild("##strategies_view", ImVec2(w, h), false);

    if (ImGui::Button("Refresh")) load_strategies();
    ImGui::SameLine();
    ImGui::Text("(%d strategies)", (int)strategies_.size());

    ImGui::Separator();

    // Table of strategies
    if (ImGui::BeginTable("##strat_table", 7,
                           ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                           ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Symbols");
        ImGui::TableSetupColumn("Timeframe");
        ImGui::TableSetupColumn("Provider");
        ImGui::TableSetupColumn("SL/TP");
        ImGui::TableSetupColumn("Status");
        ImGui::TableSetupColumn("Actions");
        ImGui::TableHeadersRow();

        std::lock_guard lock(data_mutex_);
        std::string pending_delete_strategy;
        for (auto& s : strategies_) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("%s", s.name.c_str());
            ImGui::TableNextColumn(); ImGui::TextWrapped("%s", s.symbols.c_str());
            ImGui::TableNextColumn(); ImGui::Text("%s", s.timeframe.c_str());
            ImGui::TableNextColumn(); ImGui::Text("%s", s.provider.c_str());
            ImGui::TableNextColumn(); ImGui::Text("%.1f/%.1f", s.stop_loss, s.take_profit);
            ImGui::TableNextColumn();
            ImGui::TextColored(s.is_active ? ImVec4(0.2f,1,0.2f,1) : ImVec4(0.5f,0.5f,0.5f,1),
                               s.is_active ? "Active" : "Inactive");
            ImGui::TableNextColumn();
            ImGui::PushID(s.id.c_str());
            if (ImGui::SmallButton("Edit")) edit_strategy(s);
            ImGui::SameLine();
            if (ImGui::SmallButton("Paper")) deploy_strategy(s.id, "paper");
            ImGui::SameLine();
            if (ImGui::SmallButton("Live")) deploy_strategy(s.id, "live");
            ImGui::SameLine();
            if (ImGui::SmallButton("Backtest")) run_backtest(s.id);
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f,0.1f,0.1f,1));
            if (ImGui::SmallButton("Del")) pending_delete_strategy = s.id;
            ImGui::PopStyleColor();
            ImGui::PopID();
        }

        ImGui::EndTable();
        if (!pending_delete_strategy.empty()) {
            delete_strategy(pending_delete_strategy);
        }
    }

    // Deployments section
    ImGui::Spacing();
    ImGui::Text("Active Deployments");
    ImGui::Separator();

    if (ImGui::BeginTable("##deploy_table", 6,
                           ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                           ImGuiTableFlags_ScrollY, ImVec2(0, 200))) {
        ImGui::TableSetupColumn("Strategy");
        ImGui::TableSetupColumn("Mode");
        ImGui::TableSetupColumn("Status");
        ImGui::TableSetupColumn("PID");
        ImGui::TableSetupColumn("Started");
        ImGui::TableSetupColumn("Actions");
        ImGui::TableHeadersRow();

        std::lock_guard lock(data_mutex_);
        std::string pending_delete_deploy;
        for (auto& d : deployments_) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("%s", d.strategy_name.c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(d.mode == "live" ? ImVec4(1,0.3f,0.3f,1) : ImVec4(0.3f,0.7f,1,1),
                               "%s", d.mode.c_str());
            ImGui::TableNextColumn();
            auto sc = deployment_status_str(d.status);
            ImVec4 col = d.status == DeploymentStatus::Running ? ImVec4(0.2f,1,0.2f,1)
                       : d.status == DeploymentStatus::Error   ? ImVec4(1,0.2f,0.2f,1)
                       : ImVec4(0.5f,0.5f,0.5f,1);
            ImGui::TextColored(col, "%s", sc);
            ImGui::TableNextColumn(); ImGui::Text("%d", d.pid);
            ImGui::TableNextColumn(); ImGui::Text("%s", d.started_at.c_str());
            ImGui::TableNextColumn();
            ImGui::PushID(d.id.c_str());
            if (d.status == DeploymentStatus::Running) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f,0.2f,0.1f,1));
                if (ImGui::SmallButton("Stop")) stop_deployment(d.id);
                ImGui::PopStyleColor();
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f,0.1f,0.1f,1));
                if (ImGui::SmallButton("Delete")) pending_delete_deploy = d.id;
                ImGui::PopStyleColor();
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
        if (!pending_delete_deploy.empty()) {
            delete_deployment_action(pending_delete_deploy);
        }
    }

    // Backtest results (shown inline after running a backtest)
    render_backtest_results();

    ImGui::EndChild();
}

// ============================================================================
// Library sub-view (stub — full impl in Part 4)
// ============================================================================

void AlgoTradingScreen::render_library(float w, float h) {
    if (!library_loaded_) load_library();

    // Three-column layout: categories | strategy list | code preview
    float cat_w = 160.0f;
    float list_w = 280.0f;
    float code_w = w - cat_w - list_w - 20.0f;

    // --- Left: Category sidebar ---
    ImGui::BeginChild("##lib_cats", ImVec2(cat_w, h), true);
    ImGui::Text("Categories");
    ImGui::Separator();

    // "All" category
    bool all_sel = (library_category_idx_ == 0);
    if (ImGui::Selectable("All", all_sel))
        library_category_idx_ = 0;
    if (all_sel) {
        ImGui::SameLine(cat_w - 40);
        ImGui::TextDisabled("%d", (int)python_strategies_.size());
    }

    for (int i = 0; i < (int)library_categories_.size(); i++) {
        bool sel = (library_category_idx_ == i + 1);
        // Count strategies in this category
        int count = 0;
        for (auto& s : python_strategies_)
            if (s.category == library_categories_[i]) count++;

        if (ImGui::Selectable(library_categories_[i].c_str(), sel))
            library_category_idx_ = i + 1;
        if (sel || true) {
            ImGui::SameLine(cat_w - 40);
            ImGui::TextDisabled("%d", count);
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Custom Strategies");
    ImGui::Separator();
    for (auto& cs : custom_strategies_) {
        ImGui::PushID(cs.id.c_str());
        if (ImGui::Selectable(cs.name.c_str(), selected_strategy_id_ == cs.id)) {
            selected_strategy_id_ = cs.id;
            code_preview_ = cs.code;
            editing_code_ = false;
        }
        ImGui::PopID();
    }

    ImGui::EndChild();

    ImGui::SameLine();

    // --- Middle: Strategy list ---
    ImGui::BeginChild("##lib_list", ImVec2(list_w, h), true);

    // Search bar
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##lib_search", "Search strategies...",
                              library_search_, sizeof(library_search_));

    ImGui::Separator();

    std::string search_q = library_search_;
    // Convert to lowercase for matching
    for (auto& c : search_q) c = (char)tolower(c);

    std::string active_cat;
    if (library_category_idx_ > 0 && library_category_idx_ <= (int)library_categories_.size())
        active_cat = library_categories_[library_category_idx_ - 1];

    int shown = 0;
    for (auto& ps : python_strategies_) {
        // Category filter
        if (!active_cat.empty() && ps.category != active_cat) continue;

        // Search filter
        if (!search_q.empty()) {
            std::string name_lower = ps.name;
            for (auto& c : name_lower) c = (char)tolower(c);
            std::string id_lower = ps.id;
            for (auto& c : id_lower) c = (char)tolower(c);
            if (name_lower.find(search_q) == std::string::npos &&
                id_lower.find(search_q) == std::string::npos)
                continue;
        }

        bool sel = (selected_strategy_id_ == ps.id);
        ImGui::PushID(ps.id.c_str());

        // Format: name + category badge
        if (ImGui::Selectable("##sel", sel, 0, ImVec2(0, 36))) {
            selected_strategy_id_ = ps.id;
            editing_code_ = false;
            // Load code in background
            code_preview_ = "Loading...";
            loading_code_ = true;
            std::string sid = ps.id;
            std::thread([this, sid]() {
                auto code = AlgoService::instance().get_strategy_code(sid);
                code_preview_ = code;
                loading_code_ = false;
            }).detach();
        }
        // Draw name and category on the selectable
        ImGui::SameLine(4);
        ImGui::BeginGroup();
        // Insert spaces before capitals for readability
        std::string display_name = ps.name;
        std::string readable;
        for (size_t i = 0; i < display_name.size(); i++) {
            if (i > 0 && display_name[i] >= 'A' && display_name[i] <= 'Z' &&
                display_name[i-1] >= 'a' && display_name[i-1] <= 'z')
                readable += ' ';
            readable += display_name[i];
        }
        ImGui::Text("%s", readable.c_str());
        ImGui::TextColored(ImVec4(0.4f, 0.6f, 0.8f, 1.0f), "%s | %s",
                           ps.category.c_str(), ps.id.c_str());
        ImGui::EndGroup();

        ImGui::PopID();
        shown++;
    }

    if (shown == 0) {
        ImGui::TextDisabled("No strategies match the filter");
    }

    ImGui::EndChild();

    ImGui::SameLine();

    // --- Right: Code preview / editor ---
    ImGui::BeginChild("##lib_code", ImVec2(code_w, h), true);

    if (selected_strategy_id_.empty()) {
        ImGui::TextDisabled("Select a strategy to view its code");
    } else if (loading_code_) {
        ImGui::Text("Loading code...");
    } else {
        // Toolbar
        if (!editing_code_) {
            if (ImGui::Button("Edit / Clone")) {
                editing_code_ = true;
                snprintf(custom_code_, sizeof(custom_code_), "%s", code_preview_.c_str());
                // Pre-fill name from strategy
                for (auto& ps : python_strategies_) {
                    if (ps.id == selected_strategy_id_) {
                        snprintf(custom_name_, sizeof(custom_name_), "%s_custom", ps.name.c_str());
                        break;
                    }
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Backtest")) {
                // Run python backtest with this code
                status_msg_ = "Running Python backtest...";
                backtest_running_ = true;
                std::string code = code_preview_;
                std::thread([this, code]() {
                    try {
                        auto result = AlgoService::instance().run_python_backtest(
                            code, "NSE:NIFTY50", "2024-01-01", "2024-12-31", 100000.0);
                        std::lock_guard lock(data_mutex_);
                        backtest_result_ = result;
                        status_msg_ = "Python backtest complete";
                        core::notify::send("Backtest Complete",
                            "Strategy backtest finished successfully",
                            core::NotifyLevel::Success);
                    } catch (const std::exception& e) {
                        status_msg_ = std::string("Backtest failed: ") + e.what();
                        core::notify::send("Backtest Failed", e.what(), core::NotifyLevel::Error);
                    }
                    backtest_running_ = false;
                }).detach();
            }
            ImGui::SameLine();
            if (ImGui::Button("Copy Code")) {
                ImGui::SetClipboardText(code_preview_.c_str());
                status_msg_ = "Code copied to clipboard";
            }

            ImGui::Separator();

            // Read-only code display
            ImGui::InputTextMultiline("##code_ro", const_cast<char*>(code_preview_.c_str()),
                                       code_preview_.size() + 1,
                                       ImVec2(-1, h - 80),
                                       ImGuiInputTextFlags_ReadOnly);
        } else {
            // Edit mode
            ImGui::Text("Name:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(300);
            ImGui::InputText("##clone_name", custom_name_, sizeof(custom_name_));

            ImGui::SameLine();
            if (validating_syntax_.load()) {
                ImGui::Button("Validating...");
            } else if (ImGui::Button("Validate Syntax")) {
                validating_syntax_.store(true);
                std::string code_copy(custom_code_);
                std::thread([this, code_copy]() {
                    std::string err;
                    bool valid = AlgoService::instance().validate_python_syntax(code_copy, err);
                    {
                        std::lock_guard<std::mutex> lock(data_mutex_);
                        syntax_error_ = valid ? "" : err;
                        status_msg_ = valid ? "Syntax OK" : "Syntax error";
                    }
                    validating_syntax_.store(false);
                }).detach();
            }
            ImGui::SameLine();
            if (ImGui::Button("Save as Custom")) {
                CustomPythonStrategy cs;
                cs.id = db::generate_uuid();
                cs.name = custom_name_;
                cs.description = "Cloned from " + selected_strategy_id_;
                cs.code = custom_code_;
                cs.category = "custom";
                db::ops::save_custom_python_strategy(cs);
                status_msg_ = "Custom strategy saved: " + cs.name;
                editing_code_ = false;
                // Reload customs
                custom_strategies_ = db::ops::list_custom_python_strategies();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                editing_code_ = false;
                syntax_error_.clear();
            }

            if (!syntax_error_.empty()) {
                ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "%s", syntax_error_.c_str());
            }

            ImGui::Separator();

            ImGui::InputTextMultiline("##code_edit", custom_code_, sizeof(custom_code_),
                                       ImVec2(-1, h - 100));
        }
    }

    ImGui::EndChild();
}

// ============================================================================
// Scanner sub-view
// ============================================================================

void AlgoTradingScreen::render_scanner(float w, float h) {
    ImGui::BeginChild("##scanner_view", ImVec2(w, h), false);
    ImGui::Text("Market Scanner");
    ImGui::Separator();

    // --- Controls row ---
    ImGui::Text("Symbols (comma-separated)");
    ImGui::SetNextItemWidth(w * 0.4f);
    ImGui::InputText("##scan_symbols", scan_symbols_, sizeof(scan_symbols_));

    ImGui::SameLine();
    ImGui::Text("Timeframe:");
    ImGui::SameLine();
    auto& tfs = get_timeframes();
    ImGui::SetNextItemWidth(100);
    if (ImGui::BeginCombo("##scan_tf", tfs[scan_tf_idx_].label)) {
        for (int i = 0; i < (int)tfs.size(); i++) {
            if (ImGui::Selectable(tfs[i].label, i == scan_tf_idx_))
                scan_tf_idx_ = i;
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    ImGui::Text("Provider:");
    ImGui::SameLine();
    auto& provs = get_broker_providers();
    ImGui::SetNextItemWidth(120);
    if (ImGui::BeginCombo("##scan_prov", provs[scan_prov_idx_].second)) {
        for (int i = 0; i < (int)provs.size(); i++) {
            if (ImGui::Selectable(provs[i].second, i == scan_prov_idx_))
                scan_prov_idx_ = i;
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button,
        scanning_ ? ImVec4(0.5f, 0.5f, 0.5f, 1.0f) : ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
    if (ImGui::Button(scanning_ ? "Scanning..." : "Run Scan", ImVec2(120, 0))) {
        if (!scanning_) run_scan();
    }
    ImGui::PopStyleColor();

    // --- Active conditions summary ---
    ImGui::Spacing();
    if (entry_conditions_.empty()) {
        ImGui::TextColored(ImVec4(1, 0.6f, 0.2f, 1),
            "No entry conditions defined. Go to Builder tab to add conditions.");
    } else {
        ImGui::TextColored(ImVec4(0.4f, 0.7f, 1, 1),
            "Scanning with %d entry condition(s) [%s]",
            (int)entry_conditions_.size(),
            entry_logic_idx_ == 0 ? "AND" : "OR");
        // Show conditions as a compact summary
        ImGui::SameLine();
        ImGui::TextDisabled("(");
        for (int i = 0; i < (int)entry_conditions_.size(); i++) {
            if (i > 0) { ImGui::SameLine(); ImGui::TextDisabled(entry_logic_idx_ == 0 ? "&&" : "||"); }
            ImGui::SameLine();
            auto& c = entry_conditions_[i];
            ImGui::Text("%s %s %s", c.indicator.c_str(), c.operator_type.c_str(), c.value.c_str());
        }
        ImGui::SameLine();
        ImGui::TextDisabled(")");
    }

    ImGui::Spacing();
    ImGui::Separator();

    // --- Results table ---
    if (scan_results_.empty() && !scanning_) {
        ImGui::TextDisabled("No scan results yet. Enter symbols and click Run Scan.");
    } else {
        ImGui::Text("Results: %d symbols", (int)scan_results_.size());
        ImGui::Spacing();

        if (ImGui::BeginTable("##scan_results", 6,
                               ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                               ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY,
                               ImVec2(0, h - 140))) {
            ImGui::TableSetupColumn("Symbol", ImGuiTableColumnFlags_WidthFixed, 120);
            ImGui::TableSetupColumn("Entry Match", ImGuiTableColumnFlags_WidthFixed, 90);
            ImGui::TableSetupColumn("Exit Match", ImGuiTableColumnFlags_WidthFixed, 90);
            ImGui::TableSetupColumn("Price", ImGuiTableColumnFlags_WidthFixed, 100);
            ImGui::TableSetupColumn("Volume", ImGuiTableColumnFlags_WidthFixed, 100);
            ImGui::TableSetupColumn("Details");
            ImGui::TableHeadersRow();

            for (auto& r : scan_results_) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("%s", r.symbol.c_str());
                ImGui::TableNextColumn();
                ImGui::TextColored(r.entry_match ? ImVec4(0.2f,1,0.2f,1) : ImVec4(0.5f,0.5f,0.5f,1),
                                   r.entry_match ? "YES" : "NO");
                ImGui::TableNextColumn();
                ImGui::TextColored(r.exit_match ? ImVec4(1,0.3f,0.3f,1) : ImVec4(0.5f,0.5f,0.5f,1),
                                   r.exit_match ? "YES" : "NO");
                ImGui::TableNextColumn(); ImGui::Text("%.2f", r.last_price);
                ImGui::TableNextColumn(); ImGui::Text("%.0f", r.volume);
                ImGui::TableNextColumn();
                if (!r.details.empty() && r.details != "{}") {
                    ImGui::TextWrapped("%s", r.details.c_str());
                } else {
                    ImGui::TextDisabled("-");
                }
            }
            ImGui::EndTable();
        }
    }

    ImGui::EndChild();
}

// ============================================================================
// Dashboard sub-view
// ============================================================================

void AlgoTradingScreen::render_dashboard(float w, float h) {
    ImGui::BeginChild("##dashboard_view", ImVec2(w, h), false);

    // Toolbar
    if (ImGui::Button("Refresh Dashboard")) {
        load_deployments();
        load_dashboard_data();
    }
    ImGui::SameLine();
    if (running_count_ > 0) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.1f, 0.1f, 1));
        if (ImGui::Button("STOP ALL")) stop_all_deployments();
        ImGui::PopStyleColor();
    }

    ImGui::Spacing();

    // Summary cards
    render_dashboard_summary();

    ImGui::Spacing();
    ImGui::Text("Deployment Metrics");
    ImGui::Separator();

    if (ImGui::BeginTable("##metrics_table", 8,
                           ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                           ImGuiTableFlags_ScrollY, ImVec2(0, h * 0.4f))) {
        ImGui::TableSetupColumn("Deployment");
        ImGui::TableSetupColumn("Total Trades");
        ImGui::TableSetupColumn("Win Rate");
        ImGui::TableSetupColumn("Total PnL");
        ImGui::TableSetupColumn("Max DD");
        ImGui::TableSetupColumn("Sharpe");
        ImGui::TableSetupColumn("Avg Win");
        ImGui::TableSetupColumn("Avg Loss");
        ImGui::TableHeadersRow();

        std::lock_guard lock(data_mutex_);
        for (auto& m : dashboard_metrics_) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("%s", m.deployment_id.c_str());
            ImGui::TableNextColumn(); ImGui::Text("%d", m.total_trades);
            ImGui::TableNextColumn(); ImGui::Text("%.1f%%", m.win_rate * 100);
            ImGui::TableNextColumn();
            ImGui::TextColored(m.total_pnl >= 0 ? ImVec4(0.2f,1,0.2f,1) : ImVec4(1,0.2f,0.2f,1),
                               "%.2f", m.total_pnl);
            ImGui::TableNextColumn(); ImGui::Text("%.2f%%", m.max_drawdown * 100);
            ImGui::TableNextColumn(); ImGui::Text("%.2f", m.sharpe_ratio);
            ImGui::TableNextColumn(); ImGui::Text("%.2f", m.avg_win);
            ImGui::TableNextColumn(); ImGui::Text("%.2f", m.avg_loss);
        }
        ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::Text("Recent Trades");
    ImGui::Separator();

    if (ImGui::BeginTable("##trades_table", 7,
                           ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                           ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupColumn("Symbol");
        ImGui::TableSetupColumn("Side");
        ImGui::TableSetupColumn("Qty");
        ImGui::TableSetupColumn("Price");
        ImGui::TableSetupColumn("PnL");
        ImGui::TableSetupColumn("Status");
        ImGui::TableSetupColumn("Time");
        ImGui::TableHeadersRow();

        std::lock_guard lock(data_mutex_);
        for (auto& t : recent_trades_) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("%s", t.symbol.c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(t.side == "buy" ? ImVec4(0.2f,1,0.2f,1) : ImVec4(1,0.3f,0.3f,1),
                               "%s", t.side.c_str());
            ImGui::TableNextColumn(); ImGui::Text("%.4f", t.quantity);
            ImGui::TableNextColumn(); ImGui::Text("%.2f", t.price);
            ImGui::TableNextColumn();
            ImGui::TextColored(t.pnl >= 0 ? ImVec4(0.2f,1,0.2f,1) : ImVec4(1,0.2f,0.2f,1),
                               "%.2f", t.pnl);
            ImGui::TableNextColumn(); ImGui::Text("%s", t.status.c_str());
            ImGui::TableNextColumn(); ImGui::Text("%s", t.executed_at.c_str());
        }
        ImGui::EndTable();
    }

    ImGui::EndChild();
}

// ============================================================================
// Actions (DB-backed — using placeholder implementations, full in Part 3)
// ============================================================================

void AlgoTradingScreen::save_strategy() {
    if (strategy_name_[0] == '\0') {
        status_msg_ = "Error: Strategy name required";
        return;
    }

    AlgoStrategy s;
    s.id = editing_strategy_id_.empty() ? db::generate_uuid() : editing_strategy_id_;
    s.name = strategy_name_;
    s.description = strategy_desc_;
    s.timeframe = get_timeframes()[timeframe_idx_].id;
    s.provider = get_broker_providers()[provider_idx_].first;
    s.stop_loss = stop_loss_;
    s.take_profit = take_profit_;
    s.trailing_stop = trailing_stop_;
    s.position_size = position_size_;
    s.max_positions = max_positions_;
    s.entry_logic = entry_logic_idx_ == 0 ? "AND" : "OR";
    s.exit_logic = exit_logic_idx_ == 0 ? "AND" : "OR";

    // Serialize symbols
    json sym_arr = json::array();
    std::string syms = symbols_buf_;
    size_t pos = 0;
    while ((pos = syms.find(',')) != std::string::npos) {
        auto tok = syms.substr(0, pos);
        if (!tok.empty()) sym_arr.push_back(tok);
        syms.erase(0, pos + 1);
    }
    if (!syms.empty()) sym_arr.push_back(syms);
    s.symbols = sym_arr.dump();

    // Serialize conditions
    json entry_j = json::array();
    for (auto& c : entry_conditions_) { json cj; to_json(cj, c); entry_j.push_back(cj); }
    s.entry_conditions = entry_j.dump();

    json exit_j = json::array();
    for (auto& c : exit_conditions_) { json cj; to_json(cj, c); exit_j.push_back(cj); }
    s.exit_conditions = exit_j.dump();

    db::ops::save_algo_strategy(s);
    status_msg_ = "Strategy saved: " + s.name;

    // Reset form
    editing_strategy_id_.clear();
    strategy_name_[0] = '\0';
    strategy_desc_[0] = '\0';
    symbols_buf_[0] = '\0';
    entry_conditions_.clear();
    exit_conditions_.clear();
    stop_loss_ = take_profit_ = trailing_stop_ = 0;
    position_size_ = 1.0f;
    max_positions_ = 1;

    load_strategies();
}

void AlgoTradingScreen::load_strategies() {
    std::lock_guard lock(data_mutex_);
    strategies_ = db::ops::list_algo_strategies();
    strategies_loaded_ = true;
}

void AlgoTradingScreen::delete_strategy(const std::string& id) {
    db::ops::delete_algo_strategy(id);
    status_msg_ = "Strategy deleted";
    load_strategies();
}

void AlgoTradingScreen::edit_strategy(const AlgoStrategy& s) {
    editing_strategy_id_ = s.id;
    snprintf(strategy_name_, sizeof(strategy_name_), "%s", s.name.c_str());
    snprintf(strategy_desc_, sizeof(strategy_desc_), "%s", s.description.c_str());

    // Parse symbols back
    try {
        auto arr = json::parse(s.symbols);
        std::string syms;
        for (auto& el : arr) {
            if (!syms.empty()) syms += ",";
            syms += el.get<std::string>();
        }
        snprintf(symbols_buf_, sizeof(symbols_buf_), "%s", syms.c_str());
    } catch (...) {
        snprintf(symbols_buf_, sizeof(symbols_buf_), "%s", s.symbols.c_str());
    }

    // Parse conditions back
    entry_conditions_.clear();
    exit_conditions_.clear();
    try {
        auto entry_j = json::parse(s.entry_conditions);
        for (auto& cj : entry_j) { ConditionItem c; from_json(cj, c); entry_conditions_.push_back(c); }
    } catch (...) {}
    try {
        auto exit_j = json::parse(s.exit_conditions);
        for (auto& cj : exit_j) { ConditionItem c; from_json(cj, c); exit_conditions_.push_back(c); }
    } catch (...) {}

    entry_logic_idx_ = s.entry_logic == "OR" ? 1 : 0;
    exit_logic_idx_ = s.exit_logic == "OR" ? 1 : 0;
    stop_loss_ = (float)s.stop_loss;
    take_profit_ = (float)s.take_profit;
    trailing_stop_ = (float)s.trailing_stop;
    position_size_ = (float)s.position_size;
    max_positions_ = s.max_positions;

    // Find timeframe & provider indices
    auto& tfs = get_timeframes();
    for (int i = 0; i < (int)tfs.size(); i++)
        if (s.timeframe == tfs[i].id) { timeframe_idx_ = i; break; }
    auto& provs = get_broker_providers();
    for (int i = 0; i < (int)provs.size(); i++)
        if (s.provider == provs[i].first) { provider_idx_ = i; break; }

    active_view_ = SubView::Builder;
    status_msg_ = "Editing: " + s.name;
}

void AlgoTradingScreen::deploy_strategy(const std::string& strategy_id,
                                         const std::string& mode) {
    // Find strategy
    AlgoStrategy* strat = nullptr;
    for (auto& s : strategies_) {
        if (s.id == strategy_id) { strat = &s; break; }
    }
    if (!strat) {
        status_msg_ = "Error: Strategy not found";
        return;
    }

    status_msg_ = "Deploying " + strat->name + " in " + mode + " mode...";

    // Parse first symbol from JSON array
    std::string symbol;
    try {
        auto arr = json::parse(strat->symbols);
        if (!arr.empty()) symbol = arr[0].get<std::string>();
    } catch (...) {
        symbol = strat->symbols;
    }

    // Launch deployment in background thread to avoid UI freeze
    std::string sid = strategy_id;
    std::string tf = strat->timeframe;
    std::string prov = strat->provider;
    double qty = strat->position_size;
    std::thread([this, sid, symbol, mode, prov, tf, qty]() {
        try {
            auto dep_id = AlgoService::instance().deploy_strategy(
                sid, symbol, mode, prov, tf, qty);
            status_msg_ = "Deployed: " + dep_id;
            core::notify::send("Strategy Deployed",
                "Deployment started: " + dep_id.substr(0, 8),
                core::NotifyLevel::Success);
        } catch (const std::exception& e) {
            status_msg_ = std::string("Deploy failed: ") + e.what();
            core::notify::send("Deploy Failed", e.what(), core::NotifyLevel::Error);
        }
        load_deployments();
    }).detach();
}

void AlgoTradingScreen::stop_deployment(const std::string& deployment_id) {
    status_msg_ = "Stopping deployment...";
    std::string did = deployment_id;
    std::thread([this, did]() {
        bool ok = AlgoService::instance().stop_deployment(did);
        status_msg_ = ok ? "Deployment stopped" : "Failed to stop deployment";
        if (ok) {
            core::notify::send("Deployment Stopped",
                "Strategy deployment halted", core::NotifyLevel::Warning);
        }
        load_deployments();
    }).detach();
}

void AlgoTradingScreen::load_deployments() {
    std::lock_guard lock(data_mutex_);
    deployments_ = db::ops::list_algo_deployments();
    deployments_loaded_ = true;
    refresh_running_count();
}

void AlgoTradingScreen::load_library() {
    python_strategies_ = AlgoService::instance().list_python_strategies();
    library_categories_ = AlgoService::instance().get_strategy_categories();
    custom_strategies_ = db::ops::list_custom_python_strategies();
    library_loaded_ = true;
}

void AlgoTradingScreen::run_scan() {
    if (entry_conditions_.empty()) {
        status_msg_ = "Error: Add entry conditions in Builder first";
        return;
    }

    // Parse symbols from scan input
    std::vector<std::string> symbols;
    std::string syms = scan_symbols_;
    size_t pos = 0;
    while ((pos = syms.find(',')) != std::string::npos) {
        auto tok = syms.substr(0, pos);
        // trim whitespace
        while (!tok.empty() && tok.front() == ' ') tok.erase(0, 1);
        while (!tok.empty() && tok.back() == ' ') tok.pop_back();
        if (!tok.empty()) symbols.push_back(tok);
        syms.erase(0, pos + 1);
    }
    while (!syms.empty() && syms.front() == ' ') syms.erase(0, 1);
    while (!syms.empty() && syms.back() == ' ') syms.pop_back();
    if (!syms.empty()) symbols.push_back(syms);

    if (symbols.empty()) {
        status_msg_ = "Error: Enter symbols to scan";
        return;
    }

    // Build conditions JSON from current entry conditions
    json conds = json::array();
    for (auto& c : entry_conditions_) { json cj; to_json(cj, c); conds.push_back(cj); }

    std::string tf = get_timeframes()[scan_tf_idx_].id;
    std::string prov = get_broker_providers()[scan_prov_idx_].first;
    std::string conds_str = conds.dump();

    status_msg_ = "Scanning " + std::to_string(symbols.size()) + " symbols...";
    scanning_ = true;

    std::thread([this, conds_str, symbols, tf, prov]() {
        try {
            auto results = AlgoService::instance().run_scan(conds_str, symbols, tf, prov);
            std::lock_guard lock(data_mutex_);
            scan_results_ = results;
            status_msg_ = "Scan complete: " + std::to_string(results.size()) + " results";
        } catch (const std::exception& e) {
            status_msg_ = std::string("Scan failed: ") + e.what();
        }
        scanning_ = false;
    }).detach();
}

void AlgoTradingScreen::run_backtest(const std::string& strategy_id) {
    // Find strategy
    AlgoStrategy* strat = nullptr;
    for (auto& s : strategies_) {
        if (s.id == strategy_id) { strat = &s; break; }
    }
    if (!strat) {
        status_msg_ = "Error: Strategy not found";
        return;
    }

    status_msg_ = "Running backtest for " + strat->name + "...";
    backtest_running_ = true;

    std::string sid = strategy_id;
    std::string symbol;
    try {
        auto arr = json::parse(strat->symbols);
        if (!arr.empty()) symbol = arr[0].get<std::string>();
    } catch (...) {
        symbol = strat->symbols;
    }
    std::string prov = strat->provider;

    std::thread([this, sid, symbol, prov]() {
        try {
            auto result = AlgoService::instance().run_backtest(
                sid, symbol,
                "2024-01-01", "2024-12-31",  // default date range
                100000.0, prov);

            std::lock_guard lock(data_mutex_);
            backtest_result_ = result;
            status_msg_ = "Backtest complete: " + std::to_string(result.metrics.total_trades) + " trades";
        } catch (const std::exception& e) {
            status_msg_ = std::string("Backtest failed: ") + e.what();
        }
        backtest_running_ = false;
    }).detach();
}

void AlgoTradingScreen::load_dashboard_data() {
    // Load metrics for all deployments
    std::lock_guard lock(data_mutex_);
    dashboard_metrics_.clear();
    recent_trades_.clear();
    for (auto& d : deployments_) {
        auto m = db::ops::get_algo_metrics(d.id);
        if (m) dashboard_metrics_.push_back(*m);
        auto trades = db::ops::get_algo_trades(d.id, 20);
        recent_trades_.insert(recent_trades_.end(), trades.begin(), trades.end());
    }
}

void AlgoTradingScreen::refresh_running_count() {
    running_count_ = 0;
    for (auto& d : deployments_) {
        if (d.status == DeploymentStatus::Running) running_count_++;
    }
}

void AlgoTradingScreen::stop_all_deployments() {
    status_msg_ = "Stopping all deployments...";
    std::thread([this]() {
        int stopped = AlgoService::instance().stop_all_deployments();
        status_msg_ = "Stopped " + std::to_string(stopped) + " deployment(s)";
        load_deployments();
    }).detach();
}

void AlgoTradingScreen::delete_deployment_action(const std::string& deployment_id) {
    std::string did = deployment_id;
    std::thread([this, did]() {
        AlgoService::instance().delete_deployment(did);
        status_msg_ = "Deployment deleted";
        load_deployments();
    }).detach();
}

// ============================================================================
// Backtest Results Panel
// ============================================================================

void AlgoTradingScreen::render_backtest_results() {
    if (!backtest_result_.success && backtest_result_.error.empty() && !backtest_running_)
        return;  // No results to show

    ImGui::Spacing();
    ImGui::Separator();

    if (backtest_running_) {
        ImGui::TextColored(ImVec4(0.4f, 0.7f, 1, 1), "Backtest running...");
        return;
    }

    if (!backtest_result_.error.empty()) {
        ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "Backtest Error: %s",
                           backtest_result_.error.c_str());
        return;
    }

    ImGui::Text("Backtest Results");

    // Summary cards row
    auto& m = backtest_result_.metrics;
    float card_w = 130.0f;

    auto draw_card = [](const char* label, const char* value, ImVec4 color) {
        ImGui::BeginGroup();
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.10f, 0.15f, 1.0f));
        ImGui::BeginChild(label, ImVec2(125, 52), true);
        ImGui::TextDisabled("%s", label);
        ImGui::TextColored(color, "%s", value);
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::EndGroup();
    };

    char buf[64];

    snprintf(buf, sizeof(buf), "%.2f%%", m.total_return_pct);
    draw_card("Total Return", buf, m.total_return_pct >= 0 ? ImVec4(0.2f,1,0.2f,1) : ImVec4(1,0.2f,0.2f,1));
    ImGui::SameLine();

    snprintf(buf, sizeof(buf), "%d", m.total_trades);
    draw_card("Trades", buf, ImVec4(0.8f, 0.8f, 1, 1));
    ImGui::SameLine();

    snprintf(buf, sizeof(buf), "%.1f%%", m.win_rate * 100);
    draw_card("Win Rate", buf, m.win_rate >= 0.5 ? ImVec4(0.2f,1,0.2f,1) : ImVec4(1,0.6f,0.2f,1));
    ImGui::SameLine();

    snprintf(buf, sizeof(buf), "%.2f", m.sharpe_ratio);
    draw_card("Sharpe", buf, m.sharpe_ratio >= 1.0 ? ImVec4(0.2f,1,0.2f,1) : ImVec4(1,0.6f,0.2f,1));
    ImGui::SameLine();

    snprintf(buf, sizeof(buf), "%.2f%%", m.max_drawdown * 100);
    draw_card("Max DD", buf, ImVec4(1, 0.4f, 0.4f, 1));

    // Trades table
    if (!backtest_result_.trades.empty()) {
        ImGui::Spacing();
        ImGui::Text("Backtest Trades (%d)", (int)backtest_result_.trades.size());

        if (ImGui::BeginTable("##bt_trades", 7,
                               ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                               ImGuiTableFlags_ScrollY, ImVec2(0, 200))) {
            ImGui::TableSetupColumn("Symbol");
            ImGui::TableSetupColumn("Side");
            ImGui::TableSetupColumn("Entry");
            ImGui::TableSetupColumn("Exit");
            ImGui::TableSetupColumn("Qty");
            ImGui::TableSetupColumn("PnL");
            ImGui::TableSetupColumn("PnL %");
            ImGui::TableHeadersRow();

            for (auto& t : backtest_result_.trades) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("%s", t.symbol.c_str());
                ImGui::TableNextColumn();
                ImGui::TextColored(t.side == "buy" ? ImVec4(0.2f,1,0.2f,1) : ImVec4(1,0.3f,0.3f,1),
                                   "%s", t.side.c_str());
                ImGui::TableNextColumn(); ImGui::Text("%.2f", t.entry_price);
                ImGui::TableNextColumn(); ImGui::Text("%.2f", t.exit_price);
                ImGui::TableNextColumn(); ImGui::Text("%.2f", t.quantity);
                ImGui::TableNextColumn();
                ImGui::TextColored(t.pnl >= 0 ? ImVec4(0.2f,1,0.2f,1) : ImVec4(1,0.2f,0.2f,1),
                                   "%.2f", t.pnl);
                ImGui::TableNextColumn();
                ImGui::TextColored(t.pnl_pct >= 0 ? ImVec4(0.2f,1,0.2f,1) : ImVec4(1,0.2f,0.2f,1),
                                   "%.2f%%", t.pnl_pct);
            }
            ImGui::EndTable();
        }
    }

    if (ImGui::Button("Clear Results")) {
        backtest_result_ = BacktestResult{};
    }
}

// ============================================================================
// Dashboard Summary Cards
// ============================================================================

void AlgoTradingScreen::render_dashboard_summary() {
    // Compute aggregate stats
    double total_pnl = 0;
    int total_trades = 0;
    double total_wins = 0;
    int metrics_count = 0;

    for (auto& m : dashboard_metrics_) {
        total_pnl += m.total_pnl;
        total_trades += m.total_trades;
        total_wins += m.win_rate;
        metrics_count++;
    }
    double avg_win_rate = metrics_count > 0 ? (total_wins / metrics_count) : 0;

    float card_h = 60.0f;

    auto summary_card = [card_h](const char* label, const char* value, ImVec4 val_col) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.10f, 0.16f, 1.0f));
        ImGui::BeginChild(label, ImVec2(160, card_h), true);
        ImGui::TextDisabled("%s", label);
        ImGui::SetCursorPosY(32.0f);
        ImGui::TextColored(val_col, "%s", value);
        ImGui::EndChild();
        ImGui::PopStyleColor();
    };

    char buf[64];

    snprintf(buf, sizeof(buf), "%d", running_count_);
    summary_card("Active Deployments", buf,
        running_count_ > 0 ? ImVec4(0.2f, 1, 0.2f, 1) : ImVec4(0.5f, 0.5f, 0.5f, 1));
    ImGui::SameLine();

    snprintf(buf, sizeof(buf), "%.2f", total_pnl);
    summary_card("Total PnL", buf,
        total_pnl >= 0 ? ImVec4(0.2f, 1, 0.2f, 1) : ImVec4(1, 0.2f, 0.2f, 1));
    ImGui::SameLine();

    snprintf(buf, sizeof(buf), "%d", total_trades);
    summary_card("Total Trades", buf, ImVec4(0.8f, 0.8f, 1, 1));
    ImGui::SameLine();

    snprintf(buf, sizeof(buf), "%.1f%%", avg_win_rate * 100);
    summary_card("Avg Win Rate", buf,
        avg_win_rate >= 0.5 ? ImVec4(0.2f, 1, 0.2f, 1) : ImVec4(1, 0.6f, 0.2f, 1));
    ImGui::SameLine();

    snprintf(buf, sizeof(buf), "%d", (int)deployments_.size());
    summary_card("Total Deployments", buf, ImVec4(0.6f, 0.7f, 1, 1));
}

} // namespace fincept::algo
