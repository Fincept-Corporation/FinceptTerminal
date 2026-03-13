#include "research_screen.h"
#include "../theme/bloomberg_theme.h"
#include <imgui.h>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <cmath>
#include <cctype>

namespace fincept::research {

using namespace theme::colors;

// ─── Formatting helpers (local) ─────────────────────────────────────────────
static void fmt_number(char* buf, size_t sz, double v, int decimals = 2) {
    if (v == 0) { std::snprintf(buf, sz, "N/A"); return; }
    std::snprintf(buf, sz, "%.*f", decimals, v);
}

// ─────────────────────────────────────────────────────────────────────────────
void ResearchScreen::init() {
    std::strncpy(search_buf_, "AAPL", sizeof(search_buf_));
    current_symbol_ = "AAPL";
    data_.fetch(current_symbol_, chart_period_);
    initialized_ = true;
}

void ResearchScreen::do_search() {
    // Normalize to uppercase
    for (char* p = search_buf_; *p; p++) *p = (char)std::toupper(*p);
    std::string sym(search_buf_);

    // Trim whitespace
    while (!sym.empty() && sym.back() == ' ') sym.pop_back();
    while (!sym.empty() && sym.front() == ' ') sym.erase(sym.begin());

    if (sym.empty()) return;
    if (sym == current_symbol_ && data_.has_data()) return;

    current_symbol_ = sym;
    data_.fetch(current_symbol_, chart_period_);

    if (active_tab_ == 4) { // News tab
        data_.fetch_news(current_symbol_);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void ResearchScreen::render() {
    if (!initialized_) init();

    ImGuiViewport* vp = ImGui::GetMainViewport();
    float tab_h = ImGui::GetFrameHeight() + 4;
    float footer_h = ImGui::GetFrameHeight();

    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x, vp->WorkPos.y + tab_h));
    ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, vp->WorkSize.y - tab_h - footer_h));

    ImGui::PushStyleColor(ImGuiCol_WindowBg, BG_DARK);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    ImGui::Begin("##research_screen", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus);

    render_header();
    render_ticker_bar();
    render_sub_tabs();
    render_content();

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

// ─────────────────────────────────────────────────────────────────────────────
// Header: [RESEARCH icon] | [Symbol Search] [GO] [REFRESH] | [Sub-tabs] | [Status]
// ─────────────────────────────────────────────────────────────────────────────
void ResearchScreen::render_header() {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##res_header", ImVec2(0, 32), true);

    // Title
    ImGui::TextColored(ACCENT, "RESEARCH");
    ImGui::SameLine(0, 12);

    // Separator
    ImGui::TextColored(BORDER_BRIGHT, "|");
    ImGui::SameLine(0, 12);

    // Symbol search
    ImGui::PushItemWidth(120);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);
    ImGui::PushStyleColor(ImGuiCol_Text, TEXT_PRIMARY);
    if (ImGui::InputText("##sym_search", search_buf_, sizeof(search_buf_),
                          ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CharsUppercase)) {
        do_search();
    }
    ImGui::PopStyleColor(2);
    ImGui::PopItemWidth();

    ImGui::SameLine(0, 4);
    if (theme::AccentButton("GO")) {
        do_search();
    }

    ImGui::SameLine(0, 4);
    if (theme::SecondaryButton("REFRESH")) {
        data_.fetch(current_symbol_, chart_period_);
    }

    ImGui::SameLine(0, 12);
    ImGui::TextColored(BORDER_BRIGHT, "|");
    ImGui::SameLine(0, 12);

    // Status
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 200);

    // Current time
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    char tb[16];
    std::strftime(tb, sizeof(tb), "%H:%M:%S", t);
    ImGui::TextColored(TEXT_DIM, "%s", tb);

    ImGui::SameLine(0, 12);
    ImGui::TextColored(BORDER_BRIGHT, "|");
    ImGui::SameLine(0, 12);

    ImGui::TextColored(INFO, "%s", current_symbol_.c_str());

    ImGui::SameLine(0, 8);
    if (data_.is_loading()) {
        ImGui::TextColored(WARNING, "[LOADING]");
    } else {
        ImGui::TextColored(SUCCESS, "[LIVE]");
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ─────────────────────────────────────────────────────────────────────────────
// Ticker bar: SYMBOL | Company Name | Sector | Price | Change
// ─────────────────────────────────────────────────────────────────────────────
void ResearchScreen::render_ticker_bar() {
    std::lock_guard<std::mutex> lock(data_.mutex());
    auto& si = data_.stock_info();
    auto& q  = data_.quote_data();

    double price = q.price > 0 ? q.price : si.current_price;
    double change = q.change;
    double change_pct = q.change_percent;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##ticker_bar", ImVec2(0, 44), true);

    // Left: Symbol + Company
    ImGui::SetWindowFontScale(1.3f);
    ImGui::TextColored(ACCENT, "%s", current_symbol_.c_str());
    ImGui::SetWindowFontScale(1.0f);

    ImGui::SameLine(0, 12);
    ImGui::TextColored(TEXT_PRIMARY, "%s",
        si.company_name.empty() ? "Loading..." : si.company_name.c_str());

    if (!si.sector.empty()) {
        ImGui::SameLine(0, 12);
        ImGui::PushStyleColor(ImGuiCol_Text, INFO);
        ImGui::TextUnformatted(si.sector.c_str());
        ImGui::PopStyleColor();
    }

    if (!si.recommendation_key.empty()) {
        ImGui::SameLine(0, 12);
        ImVec4 rec_col = WARNING;
        if (si.recommendation_key == "buy" || si.recommendation_key == "strong_buy")
            rec_col = MARKET_GREEN;
        else if (si.recommendation_key == "sell" || si.recommendation_key == "strong_sell")
            rec_col = MARKET_RED;
        std::string rec = si.recommendation_key;
        for (auto& c : rec) c = (char)std::toupper(c);
        for (auto& c : rec) if (c == '_') c = ' ';
        ImGui::PushStyleColor(ImGuiCol_Text, rec_col);
        ImGui::TextUnformatted(rec.c_str());
        ImGui::PopStyleColor();
    }

    // Second line: exchange, industry, country
    if (!si.industry.empty()) {
        ImGui::TextColored(TEXT_DIM, "%s%s%s%s%s",
            si.exchange.empty() ? "" : si.exchange.c_str(),
            si.exchange.empty() ? "" : " | ",
            si.industry.c_str(),
            si.country.empty() ? "" : " | ",
            si.country.c_str());
    }

    // Right: Price
    if (price > 0) {
        float right_x = ImGui::GetContentRegionAvail().x;
        ImGui::SameLine(right_x - 160);

        char price_buf[32];
        std::snprintf(price_buf, sizeof(price_buf), "$%.2f", price);
        ImGui::SetWindowFontScale(1.4f);
        ImGui::TextColored(TEXT_PRIMARY, "%s", price_buf);
        ImGui::SetWindowFontScale(1.0f);

        ImGui::SameLine(0, 8);
        ImVec4 chg_col = change >= 0 ? MARKET_GREEN : MARKET_RED;
        char chg_buf[32];
        std::snprintf(chg_buf, sizeof(chg_buf), "%s$%.2f (%.2f%%)",
            change >= 0 ? "+" : "", change, change_pct);
        ImGui::TextColored(chg_col, "%s", chg_buf);
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ─────────────────────────────────────────────────────────────────────────────
// Sub-tabs: OVERVIEW | FINANCIALS | ANALYSIS | TECHNICALS | NEWS
// ─────────────────────────────────────────────────────────────────────────────
void ResearchScreen::render_sub_tabs() {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARKEST);
    ImGui::BeginChild("##res_tabs", ImVec2(0, 28), true);

    static const char* tab_labels[] = {
        "OVERVIEW", "FINANCIALS", "ANALYSIS", "TECHNICALS", "NEWS"
    };
    int num_tabs = 5;

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 3));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 0));

    for (int i = 0; i < num_tabs; i++) {
        bool active = (i == active_tab_);

        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Button, ACCENT);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ACCENT_HOVER);
            ImGui::PushStyleColor(ImGuiCol_Text, BG_DARKEST);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, BG_HOVER);
            ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
        }

        if (ImGui::Button(tab_labels[i])) {
            active_tab_ = i;
            // Fetch news when switching to news tab
            if (i == 4 && data_.news().empty()) {
                data_.fetch_news(current_symbol_);
            }
        }

        ImGui::PopStyleColor(3);
        if (i < num_tabs - 1) ImGui::SameLine(0, 2);
    }

    ImGui::PopStyleVar(2);
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ─────────────────────────────────────────────────────────────────────────────
// Main content area — routes to active sub-panel
// ─────────────────────────────────────────────────────────────────────────────
void ResearchScreen::render_content() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, 6));
    ImGui::BeginChild("##res_content", ImVec2(0, 0), false,
        ImGuiWindowFlags_NoScrollbar);

    if (data_.is_loading() && !data_.has_data()) {
        // Full loading state
        ImVec2 avail = ImGui::GetContentRegionAvail();
        ImGui::SetCursorPos(ImVec2(avail.x * 0.35f, avail.y * 0.4f));
        theme::LoadingSpinner("Fetching stock data...");
        ImGui::SetCursorPosX(avail.x * 0.35f);
        ImGui::TextColored(TEXT_DIM, "Loading data for %s", current_symbol_.c_str());
    } else {
        switch (active_tab_) {
            case 0: overview_.render(data_, chart_period_); break;
            case 1: financials_.render(data_); break;
            case 2: analysis_.render(data_); break;
            case 3: technicals_.render(data_); break;
            case 4: news_.render(data_, current_symbol_); break;
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
}

} // namespace fincept::research
