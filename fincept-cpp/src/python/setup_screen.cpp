// First-run setup screen — ImGui progress UI for Python installation

#include "setup_screen.h"
#include "setup_manager.h"
#include "ui/yoga_helpers.h"
#include <imgui.h>

namespace fincept::python {

// Fincept-style colors (using theme where possible)
static const ImVec4 COL_ORANGE  = theme::colors::ACCENT;
static const ImVec4 COL_GREEN   = theme::colors::SUCCESS;
static const ImVec4 COL_RED     = theme::colors::ERROR_RED;
static const ImVec4 COL_WHITE   = theme::colors::TEXT_PRIMARY;
static const ImVec4 COL_MUTED   = theme::colors::TEXT_DIM;

bool SetupScreen::needs_setup() {
    if (!checked_) {
        checked_ = true;
        auto status = SetupManager::instance().check_status();
        setup_needed_ = status.needs_setup;
    }
    return setup_needed_;
}

bool SetupScreen::render() {
    auto& mgr = SetupManager::instance();

    // Already done
    if (setup_done_ || mgr.is_complete()) {
        setup_done_ = true;
        if (setup_thread_.joinable()) setup_thread_.join();
        return true;
    }

    // Responsive centered panel
    ui::CenteredFrame centered("##setup_panel", 520.0f, theme::colors::BG_PANEL);
    centered.begin();

    // Title
    float content_w = ImGui::GetContentRegionAvail().x;

    ImGui::PushFont(nullptr); // Use default font
    float title_w = ImGui::CalcTextSize("FINCEPT TERMINAL").x;
    ImGui::SetCursorPosX((content_w - title_w) / 2.0f);
    ImGui::TextColored(COL_ORANGE, "FINCEPT TERMINAL");
    ImGui::Spacing();

    float subtitle_w = ImGui::CalcTextSize("Python Environment Setup").x;
    ImGui::SetCursorPosX((content_w - subtitle_w) / 2.0f);
    ImGui::TextColored(COL_WHITE, "Python Environment Setup");
    ImGui::PopFont();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (!setup_started_ && !mgr.is_running()) {
        // Not started yet — show info and buttons
        ImGui::TextColored(COL_MUTED, "Fincept Terminal requires Python for advanced analytics,");
        ImGui::TextColored(COL_MUTED, "data fetching, and AI features.");
        ImGui::Spacing();
        ImGui::TextColored(COL_MUTED, "This will download and install:");
        ImGui::Spacing();

        ImGui::TextColored(COL_WHITE, "  1.  Python 3.12 runtime (~15 MB download)");
        ImGui::TextColored(COL_WHITE, "  2.  UV package manager");
        ImGui::TextColored(COL_WHITE, "  3.  NumPy 1.x environment (backtesting, vectorbt)");
        ImGui::TextColored(COL_WHITE, "  4.  NumPy 2.x environment (yfinance, AI/ML, trading)");

        ImGui::Spacing();
        ImGui::TextColored(COL_MUTED, "Total disk space: ~1.5 GB");
        ImGui::TextColored(COL_MUTED, "This may take 5-15 minutes depending on your connection.");

        ImGui::Spacing();
        ImGui::Spacing();

        // Buttons
        float btn_w = 200.0f;
        float btn_h = 36.0f;
        float spacing = 20.0f;
        float total_btn_w = btn_w * 2 + spacing;
        ImGui::SetCursorPosX((content_w - total_btn_w) / 2.0f);

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.00f, 0.50f, 0.25f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.00f, 0.60f, 0.30f, 1.0f));
        if (ImGui::Button("Install Python", ImVec2(btn_w, btn_h))) {
            setup_started_ = true;
            setup_thread_ = std::thread([]() {
                SetupManager::instance().run_setup();
            });
        }
        ImGui::PopStyleColor(2);

        ImGui::SameLine(0, spacing);

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.25f, 0.30f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.35f, 0.40f, 1.0f));
        if (ImGui::Button("Skip for Now", ImVec2(btn_w, btn_h))) {
            mgr.skip_setup();
            setup_done_ = true;
        }
        ImGui::PopStyleColor(2);

        ImGui::Spacing();
        ImGui::TextColored(COL_MUTED, "  (You can install later from Settings)");
    } else {
        // Setup in progress — show progress
        auto progress = mgr.get_progress();

        ImGui::Spacing();

        // Step indicator
        struct StepInfo { const char* key; const char* label; };
        StepInfo steps[] = {
            {"python",   "Python Runtime"},
            {"uv",       "UV Package Manager"},
            {"venv",     "Virtual Environments"},
            {"packages", "Python Packages"},
            {"complete", "Complete"},
        };

        for (auto& step : steps) {
            bool is_current = (progress.step == step.key);
            bool is_done = false;

            // Determine if step is done
            if (progress.step == "complete") {
                is_done = true;
            } else {
                // Check ordering
                int current_idx = -1, step_idx = -1;
                for (int i = 0; i < 5; i++) {
                    if (progress.step == steps[i].key) current_idx = i;
                    if (std::string(step.key) == steps[i].key) step_idx = i;
                }
                is_done = (step_idx < current_idx);
            }

            if (is_done) {
                ImGui::TextColored(COL_GREEN, "  [OK]");
            } else if (is_current) {
                ImGui::TextColored(COL_ORANGE, "  [..]");
            } else {
                ImGui::TextColored(COL_MUTED, "  [  ]");
            }
            ImGui::SameLine();

            ImVec4 col = is_current ? COL_WHITE : (is_done ? COL_GREEN : COL_MUTED);
            ImGui::TextColored(col, "%s", step.label);
        }

        ImGui::Spacing();
        ImGui::Spacing();

        // Progress bar
        float bar_progress = progress.progress / 100.0f;
        ImVec4 bar_col = progress.is_error ? COL_RED : COL_ORANGE;
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, bar_col);
        ImGui::ProgressBar(bar_progress, ImVec2(-1, 20), "");
        ImGui::PopStyleColor();

        ImGui::Spacing();

        // Status message
        ImVec4 msg_col = progress.is_error ? COL_RED : COL_MUTED;
        ImGui::TextColored(msg_col, "%s", progress.message.c_str());

        // Error — show retry button
        if (progress.is_error && !mgr.is_running()) {
            ImGui::Spacing();
            ImGui::Spacing();

            float btn_w = 150.0f;
            ImGui::SetCursorPosX((content_w - btn_w * 2 - 20) / 2.0f);

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.80f, 0.30f, 0.00f, 1.0f));
            if (ImGui::Button("Retry", ImVec2(btn_w, 32))) {
                if (setup_thread_.joinable()) setup_thread_.join();
                setup_thread_ = std::thread([]() {
                    SetupManager::instance().run_setup();
                });
            }
            ImGui::PopStyleColor();

            ImGui::SameLine(0, 20);

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.25f, 0.30f, 1.0f));
            if (ImGui::Button("Skip", ImVec2(btn_w, 32))) {
                mgr.skip_setup();
                setup_done_ = true;
            }
            ImGui::PopStyleColor();
        }

        // Complete
        if (mgr.is_complete() && !progress.is_error) {
            ImGui::Spacing();
            float btn_w = 200.0f;
            ImGui::SetCursorPosX((content_w - btn_w) / 2.0f);

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.00f, 0.50f, 0.25f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.00f, 0.60f, 0.30f, 1.0f));
            if (ImGui::Button("Continue", ImVec2(btn_w, 36))) {
                setup_done_ = true;
            }
            ImGui::PopStyleColor(2);
        }
    }

    centered.end();

    return setup_done_;
}

} // namespace fincept::python
