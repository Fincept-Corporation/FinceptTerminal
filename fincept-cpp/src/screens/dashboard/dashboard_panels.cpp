#include "dashboard_screen.h"
#include "dashboard_helpers.h"
#include <imgui.h>
#include <cmath>
#include <algorithm>

namespace fincept::dashboard {

// ============================================================================
// Market Pulse Panel (Right Sidebar)
// ============================================================================
void DashboardScreen::render_market_pulse(float width, float height) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.04f, 0.04f, 0.04f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, FC_BORDER);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::BeginChild("##market_pulse", ImVec2(width, height), ImGuiChildFlags_Borders);

    float pad = 8.0f;
    float inner_w = width - pad * 2;

    // Title
    ImGui::SetCursorPos(ImVec2(pad, 4));
    ImGui::TextColored(FC_ORANGE, "MARKET PULSE");
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Separator, FC_BORDER);
    ImGui::Separator();
    ImGui::PopStyleColor();

    // ---- Fear & Greed Gauge ----
    ImGui::Spacing();
    ImGui::SetCursorPosX(pad);
    ImGui::TextColored(FC_GRAY, "FEAR & GREED");

    auto pulse_quotes = DashboardData::instance().get_quotes(DataCategory::MarketPulse);
    float vix_val = 0;
    for (auto& q : pulse_quotes) {
        if (q.symbol == "^VIX") { vix_val = (float)q.price; break; }
    }

    float fear_greed = vix_val > 0 ?
        std::max(0.0f, std::min(100.0f, 100.0f - (vix_val - 10.0f) * 3.0f)) : 55.0f;

    // Gradient bar
    ImVec2 bar_start = ImGui::GetCursorScreenPos();
    bar_start.x += pad;
    float bar_w = inner_w;
    float bar_h = 6.0f;
    ImDrawList* dl = ImGui::GetWindowDrawList();

    for (int i = 0; i < (int)bar_w; i++) {
        float t = (float)i / bar_w;
        int r, g;
        if (t < 0.5f) { r = 255; g = (int)(t * 2 * 255); }
        else { r = (int)((1.0f - (t - 0.5f) * 2) * 255); g = 255; }
        dl->AddRectFilled(
            ImVec2(bar_start.x + i, bar_start.y),
            ImVec2(bar_start.x + i + 1, bar_start.y + bar_h),
            IM_COL32(r, g, 0, 180));
    }

    float ptr_x = bar_start.x + (fear_greed / 100.0f) * bar_w;
    dl->AddTriangleFilled(
        ImVec2(ptr_x - 3, bar_start.y + bar_h + 1),
        ImVec2(ptr_x + 3, bar_start.y + bar_h + 1),
        ImVec2(ptr_x, bar_start.y + bar_h - 2),
        IM_COL32(255, 255, 255, 220));

    ImGui::Dummy(ImVec2(bar_w, bar_h + 6));

    const char* fg_label = fear_greed <= 25 ? "FEAR" : fear_greed <= 50 ? "CAUTIOUS" :
                           fear_greed <= 75 ? "NEUTRAL" : "GREED";
    ImVec4 fg_color = fear_greed <= 25 ? FC_RED : fear_greed <= 50 ? FC_ORANGE :
                      fear_greed <= 75 ? FC_YELLOW : FC_GREEN;

    ImGui::SetCursorPosX(pad);
    ImGui::TextColored(fg_color, "%.0f %s", fear_greed, fg_label);

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Separator, FC_BORDER); ImGui::Separator(); ImGui::PopStyleColor();

    // ---- Global Snapshot ----
    ImGui::Spacing();
    ImGui::SetCursorPosX(pad);
    ImGui::TextColored(FC_GRAY, "GLOBAL SNAPSHOT");
    ImGui::Spacing();

    if (!pulse_quotes.empty()) {
        if (ImGui::BeginTable("##pulse_snap", 3, ImGuiTableFlags_SizingStretchProp,
                              ImVec2(inner_w, 0))) {
            ImGui::TableSetupColumn("Name", 0, 1.2f);
            ImGui::TableSetupColumn("Price", 0, 1.0f);
            ImGui::TableSetupColumn("Chg", 0, 0.9f);
            for (auto& q : pulse_quotes) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::TextColored(FC_WHITE, "%s", q.label.c_str());
                ImGui::TableNextColumn(); ImGui::Text("%s", fmt_price(q.price).c_str());
                ImGui::TableNextColumn(); ImGui::TextColored(chg_col(q.change_percent), "%+.2f%%", q.change_percent);
            }
            ImGui::EndTable();
        }
    } else {
        ImGui::SetCursorPosX(pad);
        ImGui::TextColored(FC_MUTED, "Loading...");
    }

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Separator, FC_BORDER); ImGui::Separator(); ImGui::PopStyleColor();

    // ---- Top Movers ----
    ImGui::Spacing();
    ImGui::SetCursorPosX(pad);
    ImGui::TextColored(FC_GRAY, "TOP MOVERS");
    ImGui::Spacing();

    auto movers = DashboardData::instance().get_quotes(DataCategory::TopMovers);
    if (!movers.empty()) {
        auto sorted = movers;
        std::sort(sorted.begin(), sorted.end(), [](const QuoteEntry& a, const QuoteEntry& b) {
            return std::abs(a.change_percent) > std::abs(b.change_percent);
        });
        int show = std::min(6, (int)sorted.size());

        if (ImGui::BeginTable("##pulse_movers", 3, ImGuiTableFlags_SizingStretchProp,
                              ImVec2(inner_w, 0))) {
            ImGui::TableSetupColumn("Dir", 0, 0.8f);
            ImGui::TableSetupColumn("Name", 0, 1.5f);
            ImGui::TableSetupColumn("Price", 0, 1.0f);
            for (int i = 0; i < show; i++) {
                auto& q = sorted[i];
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextColored(chg_col(q.change_percent), "%s%+.1f%%",
                    q.change_percent >= 0 ? "+" : "", q.change_percent);
                ImGui::TableNextColumn();
                ImGui::TextColored(FC_WHITE, "%s", q.label.c_str());
                ImGui::TableNextColumn();
                ImGui::TextColored(FC_GRAY, "$%s", fmt_price(q.price).c_str());
            }
            ImGui::EndTable();
        }
    } else {
        ImGui::SetCursorPosX(pad);
        ImGui::TextColored(FC_MUTED, "Loading...");
    }

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Separator, FC_BORDER); ImGui::Separator(); ImGui::PopStyleColor();

    // ---- Market Breadth ----
    ImGui::Spacing();
    ImGui::SetCursorPosX(pad);
    ImGui::TextColored(FC_GRAY, "MARKET BREADTH");
    ImGui::Spacing();

    auto indices = DashboardData::instance().get_quotes(DataCategory::Indices);
    if (!indices.empty()) {
        int advancing = 0, declining = 0;
        for (auto& q : indices) {
            if (q.change_percent > 0) advancing++;
            else if (q.change_percent < 0) declining++;
        }
        int total = (int)indices.size();

        ImGui::SetCursorPosX(pad);
        ImGui::TextColored(FC_GREEN, "Adv: %d", advancing);
        ImGui::SameLine(0, 8);
        ImGui::TextColored(FC_RED, "Dec: %d", declining);
        ImGui::SameLine(0, 8);
        ImGui::TextColored(FC_GRAY, "Unch: %d", total - advancing - declining);

        if (total > 0) {
            float adv_pct = (float)advancing / total;
            ImVec2 p = ImGui::GetCursorScreenPos();
            p.x += pad;
            float bw = inner_w;
            dl = ImGui::GetWindowDrawList();
            dl->AddRectFilled(ImVec2(p.x, p.y), ImVec2(p.x + bw * adv_pct, p.y + 5),
                ImGui::ColorConvertFloat4ToU32(FC_GREEN), 2.0f);
            dl->AddRectFilled(ImVec2(p.x + bw * adv_pct, p.y), ImVec2(p.x + bw, p.y + 5),
                ImGui::ColorConvertFloat4ToU32(FC_RED), 2.0f);
            ImGui::Dummy(ImVec2(bw, 8));
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
}

// ============================================================================
// Widget Grid — positions widgets using absolute cursor positions
// ============================================================================
void DashboardScreen::render_widget_grid(float width, float height) {
    float gap = 4.0f;

    int max_col = 0, max_row = 0;
    for (auto& w : layout_.widgets) {
        if (!w.visible) continue;
        max_col = std::max(max_col, w.col + w.col_span);
        max_row = std::max(max_row, w.row + w.row_span);
    }
    if (max_col == 0) max_col = layout_.grid_cols;
    if (max_row == 0) max_row = layout_.grid_rows;

    float cell_w = (width - (max_col + 1) * gap) / max_col;
    float cell_h = (height - (max_row + 1) * gap) / max_row;

    cell_w = std::max(100.0f, cell_w);
    cell_h = std::max(60.0f, cell_h);

    bool needs_scroll = (max_row * (cell_h + gap) + gap) > height;
    ImGuiWindowFlags flags = needs_scroll ? 0 : ImGuiWindowFlags_NoScrollbar;

    ImGui::BeginChild("##grid", ImVec2(width, height), false, flags);

    for (auto& w : layout_.widgets) {
        if (!w.visible) continue;

        float wx = gap + w.col * (cell_w + gap);
        float wy = gap + w.row * (cell_h + gap);
        float ww = w.col_span * cell_w + (w.col_span - 1) * gap;
        float wh = w.row_span * cell_h + (w.row_span - 1) * gap;

        ImGui::SetCursorPos(ImVec2(wx, wy));
        render_widget_frame(w.title.c_str(), accent_for(w.type), ww, wh, w.type);
    }

    ImGui::EndChild();
}

// ============================================================================
// Add Widget Modal
// ============================================================================
void DashboardScreen::render_add_widget_modal() {
    if (!show_add_widget_modal_) return;

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(420, 380), ImGuiCond_FirstUseEver);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.06f, 0.06f, 0.97f));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, FC_HEADER);

    if (ImGui::Begin("Add Widget##modal", &show_add_widget_modal_,
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking)) {

        ImGui::TextColored(FC_ORANGE, "Select widget to add:");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        for (int i = 0; i < static_cast<int>(WidgetType::Count); i++) {
            auto type = static_cast<WidgetType>(i);
            ImVec4 accent = accent_for(type);

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.08f, 0.08f, 0.08f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                ImVec4(accent.x * 0.3f, accent.y * 0.3f, accent.z * 0.3f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, accent);

            if (ImGui::Button(widget_type_name(type), ImVec2(-1, 26))) {
                add_widget(type);
                show_add_widget_modal_ = false;
            }
            ImGui::PopStyleColor(3);
        }
    }
    ImGui::End();
    ImGui::PopStyleColor(2);
}

// ============================================================================
// Template Picker Modal
// ============================================================================
void DashboardScreen::render_template_picker_modal() {
    if (!show_template_picker_) return;

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(500, 340), ImGuiCond_FirstUseEver);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.06f, 0.06f, 0.97f));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, FC_HEADER);

    if (ImGui::Begin("Dashboard Templates##modal", &show_template_picker_,
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking)) {

        ImGui::TextColored(FC_ORANGE, "Choose a layout:");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        for (auto& t : get_all_templates()) {
            bool is_current = (layout_.template_id == t.id);
            ImGui::PushStyleColor(ImGuiCol_Button,
                is_current ? ImVec4(0.15f, 0.08f, 0.0f, 1.0f) : ImVec4(0.08f, 0.08f, 0.08f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.12f, 0.0f, 1.0f));

            std::string label = std::string("[") + t.tag + "] " + t.name;
            if (is_current) label += "  (ACTIVE)";

            if (ImGui::Button(label.c_str(), ImVec2(-1, 0))) {
                apply_template(t.id);
                show_template_picker_ = false;
            }
            ImGui::PopStyleColor(2);

            ImGui::PushStyleColor(ImGuiCol_Text, FC_GRAY);
            ImGui::TextWrapped("  %s", t.description.c_str());
            ImGui::PopStyleColor();
            ImGui::Spacing();
        }
    }
    ImGui::End();
    ImGui::PopStyleColor(2);
}

// ============================================================================
// Status Bar
// ============================================================================
void DashboardScreen::render_status_bar(float width) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, FC_HEADER);
    ImGui::BeginChild("##dash_status", ImVec2(width, 20), false);

    ImGui::SetCursorPos(ImVec2(8, 3));

    ImGui::TextColored(FC_ORANGE, "v4.0.0");
    ImGui::SameLine(0, 8);
    ImGui::TextColored(FC_BORDER, "|");
    ImGui::SameLine(0, 8);
    ImGui::TextColored(FC_GRAY, "LAYOUT:");
    ImGui::SameLine(0, 3);
    ImGui::TextColored(FC_GREEN, "%s", layout_.template_id.c_str());
    ImGui::SameLine(0, 8);
    ImGui::TextColored(FC_BORDER, "|");
    ImGui::SameLine(0, 8);

    // Feed indicators
    struct FeedInfo { const char* label; DataCategory cat; };
    FeedInfo feeds[] = {
        {"EQ", DataCategory::Indices}, {"FX", DataCategory::Forex},
        {"CM", DataCategory::Commodities}, {"CR", DataCategory::Crypto},
        {"SC", DataCategory::SectorETFs}
    };
    for (int i = 0; i < 5; i++) {
        bool has = DashboardData::instance().has_data(feeds[i].cat);
        bool ld = DashboardData::instance().is_loading(feeds[i].cat);
        ImVec4 col = has ? FC_GREEN : (ld ? FC_YELLOW : FC_RED);
        ImGui::TextColored(col, "[*]%s", feeds[i].label);
        if (i < 4) ImGui::SameLine(0, 3);
    }

    // Right: status
    float right_x = width - 60;
    float cur = ImGui::GetCursorPosX();
    if (right_x > cur + 10) {
        ImGui::SameLine(right_x);
        bool ok = DashboardData::instance().has_data(DataCategory::Indices);
        ImGui::TextColored(ok ? FC_GREEN : FC_YELLOW, ok ? "READY" : "LOADING");
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

} // namespace fincept::dashboard
