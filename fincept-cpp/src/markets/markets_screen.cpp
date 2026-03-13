#include "markets_screen.h"
#include "../theme/bloomberg_theme.h"
#include <imgui.h>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <cmath>

namespace fincept::markets {

using namespace theme::colors;

// ─── Formatting helpers ─────────────────────────────────────────────────────
static const char* fmt_change(char* buf, size_t sz, double v) {
    std::snprintf(buf, sz, "%s%.2f", v >= 0 ? "+" : "", v);
    return buf;
}

static const char* fmt_pct(char* buf, size_t sz, double v) {
    std::snprintf(buf, sz, "%s%.2f%%", v >= 0 ? "+" : "", v);
    return buf;
}

// ─────────────────────────────────────────────────────────────────────────────
void MarketsScreen::init() {
    global_markets_ = default_global_markets();
    regional_markets_ = default_regional_markets();
    refresh_all();
    initialized_ = true;
}

void MarketsScreen::refresh_all() {
    data_.fetch_all(global_markets_);
    data_.fetch_all(regional_markets_);
    last_refresh_ = time(nullptr);
}

// ─────────────────────────────────────────────────────────────────────────────
void MarketsScreen::render() {
    if (!initialized_) init();

    ImGuiViewport* vp = ImGui::GetMainViewport();
    float tab_h = ImGui::GetFrameHeight() + 4;
    float footer_h = ImGui::GetFrameHeight();

    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x, vp->WorkPos.y + tab_h));
    ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, vp->WorkSize.y - tab_h - footer_h));

    ImGui::PushStyleColor(ImGuiCol_WindowBg, BG_DARK);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    ImGui::Begin("##markets_screen", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus);

    render_header();
    render_controls();

    // Scrollable content area
    ImGui::BeginChild("##markets_content", ImVec2(0, 0), false);
    render_panels();
    ImGui::EndChild();

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    // Auto-update timer
    if (auto_update_) {
        update_timer_ += ImGui::GetIO().DeltaTime;
        if (update_timer_ >= update_interval_) {
            update_timer_ = 0;
            refresh_all();
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void MarketsScreen::render_header() {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##mkt_header", ImVec2(0, 32), true);

    ImGui::TextColored(ACCENT, "FINCEPT");
    ImGui::SameLine(0, 8);
    ImGui::TextColored(TEXT_PRIMARY, "MARKET TERMINAL");
    ImGui::SameLine(0, 8);
    ImGui::TextColored(MARKET_GREEN, "LIVE");

    ImGui::SameLine(0, 16);
    ImGui::TextColored(BORDER_BRIGHT, "|");
    ImGui::SameLine(0, 16);

    // Timestamp
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    char tb[32];
    std::strftime(tb, sizeof(tb), "%Y-%m-%d %H:%M:%S", t);
    ImGui::TextColored(TEXT_DIM, "%s", tb);

    // Right side: last refresh info
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 200);
    if (last_refresh_ > 0) {
        int elapsed = (int)difftime(now, last_refresh_);
        if (elapsed < 60)
            ImGui::TextColored(TEXT_DIM, "Updated %ds ago", elapsed);
        else
            ImGui::TextColored(TEXT_DIM, "Updated %dm ago", elapsed / 60);
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ─────────────────────────────────────────────────────────────────────────────
void MarketsScreen::render_controls() {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##mkt_controls", ImVec2(0, 30), true);

    // Refresh button
    if (theme::AccentButton("REFRESH ALL")) {
        refresh_all();
    }

    ImGui::SameLine(0, 12);

    // Auto-update toggle
    ImVec4 auto_col = auto_update_ ? MARKET_GREEN : TEXT_DIM;
    ImGui::PushStyleColor(ImGuiCol_Button, auto_update_ ? ImVec4(0.0f, 0.5f, 0.2f, 1.0f) : BG_WIDGET);
    ImGui::PushStyleColor(ImGuiCol_Text, auto_update_ ? TEXT_PRIMARY : TEXT_DIM);
    char auto_label[32];
    std::snprintf(auto_label, sizeof(auto_label), "AUTO %s", auto_update_ ? "ON" : "OFF");
    if (ImGui::Button(auto_label)) {
        auto_update_ = !auto_update_;
        update_timer_ = 0;
    }
    ImGui::PopStyleColor(2);

    ImGui::SameLine(0, 8);

    // Interval selector
    static const char* intervals[] = {"10 min", "15 min", "30 min", "1 hour"};
    static const float interval_vals[] = {600.0f, 900.0f, 1800.0f, 3600.0f};

    ImGui::PushItemWidth(80);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);
    ImGui::PushStyleColor(ImGuiCol_Text, TEXT_PRIMARY);
    if (ImGui::Combo("##interval", &interval_index_, intervals, 4)) {
        update_interval_ = interval_vals[interval_index_];
        update_timer_ = 0;
    }
    ImGui::PopStyleColor(2);
    ImGui::PopItemWidth();

    ImGui::SameLine(0, 16);
    ImGui::TextColored(BORDER_BRIGHT, "|");
    ImGui::SameLine(0, 16);

    // Count panels and symbols
    int total_panels = (int)global_markets_.size() + (int)regional_markets_.size();
    int total_symbols = 0;
    for (auto& m : global_markets_) total_symbols += (int)m.symbols.size();
    for (auto& m : regional_markets_) total_symbols += (int)m.symbols.size();
    ImGui::TextColored(TEXT_DIM, "%d panels | %d symbols", total_panels, total_symbols);

    // Loading indicator
    bool any_loading = false;
    for (auto& m : global_markets_) if (data_.is_loading(m.title)) any_loading = true;
    for (auto& m : regional_markets_) if (data_.is_loading(m.title)) any_loading = true;
    if (any_loading) {
        ImGui::SameLine(0, 12);
        ImGui::TextColored(WARNING, "[LOADING...]");
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ─────────────────────────────────────────────────────────────────────────────
void MarketsScreen::render_panels() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));

    // ── Global Markets ──
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, ACCENT);
    ImGui::SetWindowFontScale(1.1f);
    ImGui::TextUnformatted("  GLOBAL MARKETS");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();
    ImGui::PushStyleColor(ImGuiCol_Separator, BORDER_DIM);
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Spacing();

    // Render global panels in a grid (3 columns)
    float avail_w = ImGui::GetContentRegionAvail().x;
    float panel_w = (avail_w - 24) / 3.0f;
    if (panel_w < 280) panel_w = (avail_w - 16) / 2.0f;
    if (panel_w < 280) panel_w = avail_w - 8;

    int col = 0;
    int max_cols = (int)((avail_w + 8) / (panel_w + 8));
    if (max_cols < 1) max_cols = 1;

    for (size_t i = 0; i < global_markets_.size(); i++) {
        if (col > 0) ImGui::SameLine(0, 8);
        ImGui::BeginChild(("##gp_" + std::to_string(i)).c_str(),
            ImVec2(panel_w, 280), true);
        render_panel(global_markets_[i]);
        ImGui::EndChild();

        col++;
        if (col >= max_cols) col = 0;
    }

    ImGui::Spacing(); ImGui::Spacing();

    // ── Regional Markets ──
    ImGui::PushStyleColor(ImGuiCol_Text, ACCENT);
    ImGui::SetWindowFontScale(1.1f);
    ImGui::TextUnformatted("  REGIONAL MARKETS - LIVE DATA");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();
    ImGui::PushStyleColor(ImGuiCol_Separator, BORDER_DIM);
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Spacing();

    col = 0;
    for (size_t i = 0; i < regional_markets_.size(); i++) {
        if (col > 0) ImGui::SameLine(0, 8);
        ImGui::BeginChild(("##rp_" + std::to_string(i)).c_str(),
            ImVec2(panel_w, 300), true);
        render_panel(regional_markets_[i]);
        ImGui::EndChild();

        col++;
        if (col >= max_cols) col = 0;
    }

    ImGui::Spacing();
    ImGui::PopStyleVar();
}

// ─────────────────────────────────────────────────────────────────────────────
void MarketsScreen::render_panel(const MarketCategory& cat) {
    std::lock_guard<std::mutex> lock(data_.mutex());

    bool loading = data_.is_loading(cat.title);
    auto& quotes = data_.quotes(cat.title);
    auto& err = data_.error(cat.title);

    // ── Panel Header ──
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARKEST);
    ImGui::BeginChild(("##ph_" + cat.title).c_str(), ImVec2(0, 24), true);

    ImGui::TextColored(ACCENT, "%s", cat.title.c_str());
    if (cat.show_name) {
        ImGui::SameLine(0, 8);
        ImGui::TextColored(MARKET_GREEN, "LIVE");
    }

    if (loading) {
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 40);
        ImGui::TextColored(WARNING, "[...]");
    }

    // Refresh single panel
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 12);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
    if (ImGui::SmallButton(("R##" + cat.title).c_str())) {
        data_.fetch_category(cat.title, cat.symbols);
    }
    ImGui::PopStyleColor(2);

    ImGui::EndChild();
    ImGui::PopStyleColor();

    // ── Error banner ──
    if (!err.empty()) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.3f, 0.05f, 0.05f, 1.0f));
        ImGui::BeginChild(("##err_" + cat.title).c_str(), ImVec2(0, 18), false);
        ImGui::TextColored(MARKET_RED, " %s", err.c_str());
        ImGui::EndChild();
        ImGui::PopStyleColor();
    }

    // ── Table ──
    ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
                            ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY;

    int num_cols = cat.show_name ? 5 : 6;
    if (ImGui::BeginTable(cat.title.c_str(), num_cols, flags)) {
        // Header
        ImGui::TableSetupColumn("SYMBOL", ImGuiTableColumnFlags_WidthStretch, cat.show_name ? 1.2f : 1.5f);
        if (cat.show_name)
            ImGui::TableSetupColumn("NAME", ImGuiTableColumnFlags_WidthStretch, 2.0f);
        ImGui::TableSetupColumn("PRICE", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("CHG", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("%CHG", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        if (!cat.show_name) {
            ImGui::TableSetupColumn("HIGH", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("LOW", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        }
        ImGui::TableSetupScrollFreeze(0, 1);

        // Style header row
        ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, BG_DARKEST);
        ImGui::TableHeadersRow();
        ImGui::PopStyleColor();

        if (loading && quotes.empty()) {
            // Loading skeleton
            for (int i = 0; i < (int)cat.symbols.size() && i < 12; i++) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextColored(TEXT_DIM, "...");
            }
        } else if (quotes.empty()) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextColored(TEXT_DIM, "No data");
        } else {
            char buf[32];
            for (auto& q : quotes) {
                ImGui::TableNextRow();

                // Symbol
                ImGui::TableSetColumnIndex(0);
                ImGui::TextColored(TEXT_PRIMARY, "%s", q.symbol.c_str());

                int col_idx = 1;

                // Name (regional only)
                if (cat.show_name) {
                    ImGui::TableSetColumnIndex(col_idx++);
                    auto it = cat.names.find(q.symbol);
                    const char* name = (it != cat.names.end()) ? it->second.c_str() : q.symbol.c_str();
                    ImGui::TextColored(TEXT_SECONDARY, "%s", name);
                }

                // Price
                ImGui::TableSetColumnIndex(col_idx++);
                std::snprintf(buf, sizeof(buf), "%.2f", q.price);
                ImGui::TextColored(TEXT_PRIMARY, "%s", buf);

                // Change
                ImGui::TableSetColumnIndex(col_idx++);
                ImVec4 chg_col = q.change >= 0 ? MARKET_GREEN : MARKET_RED;
                fmt_change(buf, sizeof(buf), q.change);
                ImGui::TextColored(chg_col, "%s", buf);

                // %Change
                ImGui::TableSetColumnIndex(col_idx++);
                fmt_pct(buf, sizeof(buf), q.change_percent);
                ImGui::TextColored(chg_col, "%s", buf);

                // High/Low (global only)
                if (!cat.show_name) {
                    ImGui::TableSetColumnIndex(col_idx++);
                    if (q.high > 0) {
                        std::snprintf(buf, sizeof(buf), "%.2f", q.high);
                        ImGui::TextColored(TEXT_DIM, "%s", buf);
                    } else {
                        ImGui::TextColored(TEXT_DIM, "-");
                    }

                    ImGui::TableSetColumnIndex(col_idx++);
                    if (q.low > 0) {
                        std::snprintf(buf, sizeof(buf), "%.2f", q.low);
                        ImGui::TextColored(TEXT_DIM, "%s", buf);
                    } else {
                        ImGui::TextColored(TEXT_DIM, "-");
                    }
                }
            }
        }

        ImGui::EndTable();
    }
}

} // namespace fincept::markets
