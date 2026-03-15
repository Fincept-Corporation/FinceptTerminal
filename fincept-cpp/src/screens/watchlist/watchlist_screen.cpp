#include "watchlist_screen.h"
#include "ui/yoga_helpers.h"
#include "ui/theme.h"
#include <imgui.h>
#include <cstring>
#include <cstdio>
#include <ctime>
#include <cmath>
#include <algorithm>

namespace fincept::watchlist {

// Bloomberg amber/orange palette overlaid on Fincept theme
namespace bb {
    constexpr ImVec4 AMBER       = {1.00f, 0.53f, 0.00f, 1.0f};  // #FF8800 — Bloomberg signature amber
    constexpr ImVec4 AMBER_DIM   = {0.80f, 0.42f, 0.00f, 1.0f};  // dimmed amber for labels
    constexpr ImVec4 AMBER_BG    = {1.00f, 0.53f, 0.00f, 0.06f};  // amber tint background
    constexpr ImVec4 WHITE       = {0.95f, 0.95f, 0.97f, 1.0f};  // bright data text
    constexpr ImVec4 WHITE_DIM   = {0.65f, 0.65f, 0.70f, 1.0f};  // secondary text
    constexpr ImVec4 GREEN       = {0.00f, 0.85f, 0.35f, 1.0f};  // positive change
    constexpr ImVec4 RED         = {0.95f, 0.22f, 0.22f, 1.0f};  // negative change
    constexpr ImVec4 YELLOW      = {1.00f, 0.85f, 0.00f, 1.0f};  // highlights/warnings
    constexpr ImVec4 CYAN        = {0.00f, 0.80f, 0.90f, 1.0f};  // info accent
    constexpr ImVec4 BG_BLACK    = {0.04f, 0.04f, 0.05f, 1.0f};  // near-black background
    constexpr ImVec4 BG_PANEL    = {0.07f, 0.07f, 0.08f, 1.0f};  // panel background
    constexpr ImVec4 BG_ROW_ALT  = {0.06f, 0.06f, 0.07f, 1.0f};  // alternating row
    constexpr ImVec4 BG_SELECTED = {0.15f, 0.10f, 0.00f, 1.0f};  // selected row (amber tint)
    constexpr ImVec4 BG_HEADER   = {0.10f, 0.10f, 0.12f, 1.0f};  // header bar bg
    constexpr ImVec4 BORDER      = {0.18f, 0.18f, 0.20f, 1.0f};  // thin grid lines
    constexpr ImVec4 BORDER_DIM  = {0.12f, 0.12f, 0.14f, 1.0f};  // subtle borders
    constexpr ImVec4 FUNC_KEY_BG = {0.12f, 0.12f, 0.15f, 1.0f};  // function key background
    constexpr ImVec4 FUNC_KEY_AC = {0.20f, 0.14f, 0.00f, 1.0f};  // active function key
}

// ============================================================================
// Bloomberg drawing helpers
// ============================================================================
void WatchlistScreen::bb_label(const char* label, const char* value, ImVec4 val_color) {
    ImGui::TextColored(bb::AMBER_DIM, "%-12s", label);
    ImGui::SameLine(100);
    ImGui::TextColored(val_color, "%s", value);
}

void WatchlistScreen::bb_separator_line(float width) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddLine(
        p, ImVec2(p.x + width, p.y),
        ImGui::ColorConvertFloat4ToU32(bb::BORDER), 1.0f);
    ImGui::Dummy(ImVec2(width, 1));
}

bool WatchlistScreen::bb_func_key(const char* label, bool active, float width) {
    if (width <= 0) width = ImGui::CalcTextSize(label).x + 16;

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 3));

    if (active) {
        ImGui::PushStyleColor(ImGuiCol_Button, bb::FUNC_KEY_AC);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, bb::AMBER_DIM);
        ImGui::PushStyleColor(ImGuiCol_Text, bb::AMBER);
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, bb::FUNC_KEY_BG);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, bb::BORDER);
        ImGui::PushStyleColor(ImGuiCol_Text, bb::WHITE_DIM);
    }

    bool clicked = ImGui::Button(label, ImVec2(width, 0));
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(2);
    return clicked;
}

// ============================================================================
// Init & data loading (unchanged logic, reused from original)
// ============================================================================
void WatchlistScreen::init() {
    if (initialized_) return;
    load_watchlists();
    initialized_ = true;
}

void WatchlistScreen::load_watchlists() {
    watchlists_.clear();
    auto wls = db::ops::get_watchlists();
    for (auto& wl : wls) {
        WatchlistWithCount wc;
        wc.id = wl.id;
        wc.name = wl.name;
        wc.description = wl.description.value_or("");
        wc.color = wl.color;
        wc.created_at = wl.created_at;
        wc.updated_at = wl.updated_at;
        auto stocks = db::ops::get_watchlist_stocks(wl.id);
        wc.stock_count = static_cast<int>(stocks.size());
        watchlists_.push_back(std::move(wc));
    }
    if (selected_watchlist_ < 0 && !watchlists_.empty()) {
        selected_watchlist_ = 0;
        load_stocks();
    }
    if (selected_watchlist_ >= static_cast<int>(watchlists_.size())) {
        selected_watchlist_ = watchlists_.empty() ? -1 : 0;
        if (selected_watchlist_ >= 0) load_stocks();
    }
}

void WatchlistScreen::load_stocks() {
    stocks_.clear();
    selected_stock_ = -1;
    if (selected_watchlist_ < 0 || selected_watchlist_ >= static_cast<int>(watchlists_.size()))
        return;
    auto& wl = watchlists_[selected_watchlist_];
    auto db_stocks = db::ops::get_watchlist_stocks(wl.id);
    for (auto& s : db_stocks) {
        WatchlistStockEntry entry;
        entry.id = s.id;
        entry.watchlist_id = s.watchlist_id;
        entry.symbol = s.symbol;
        entry.added_at = s.added_at;
        entry.notes = s.notes.value_or("");
        stocks_.push_back(std::move(entry));
    }
    refresh_quotes();
}

void WatchlistScreen::refresh_quotes() {
    if (stocks_.empty()) return;
    std::vector<std::string> symbols;
    symbols.reserve(stocks_.size());
    for (auto& s : stocks_) symbols.push_back(s.symbol);
    data_.fetch_quotes(symbols);
    refresh_timer_ = 0;
}

void WatchlistScreen::sort_stocks() {
    std::sort(stocks_.begin(), stocks_.end(),
        [this](const WatchlistStockEntry& a, const WatchlistStockEntry& b) {
            switch (sort_by_) {
                case SortCriteria::Ticker: return a.symbol < b.symbol;
                case SortCriteria::Change: return b.quote.change_percent < a.quote.change_percent;
                case SortCriteria::Volume: return b.quote.volume < a.quote.volume;
                case SortCriteria::Price:  return b.quote.price < a.quote.price;
            }
            return false;
        });
}

// ============================================================================
// Actions (unchanged logic)
// ============================================================================
void WatchlistScreen::create_watchlist() {
    std::string name(create_name_);
    if (name.empty()) return;
    std::string desc(create_desc_);
    const auto& colors = watchlist_colors();
    std::string color = colors[create_color_idx_ % colors.size()];
    db::ops::create_watchlist(name, color, desc);
    load_watchlists();
    if (!watchlists_.empty()) {
        selected_watchlist_ = static_cast<int>(watchlists_.size()) - 1;
        load_stocks();
    }
    std::memset(create_name_, 0, sizeof(create_name_));
    std::memset(create_desc_, 0, sizeof(create_desc_));
    create_color_idx_ = 0;
    show_create_modal_ = false;
    status_ = "CREATED: " + name;
    status_time_ = ImGui::GetTime();
}

void WatchlistScreen::delete_watchlist(const std::string& id) {
    db::ops::delete_watchlist(id);
    load_watchlists();
    status_ = "WATCHLIST DELETED";
    status_time_ = ImGui::GetTime();
}

void WatchlistScreen::add_stock() {
    if (selected_watchlist_ < 0 || selected_watchlist_ >= static_cast<int>(watchlists_.size()))
        return;
    std::string symbol(add_symbol_);
    if (symbol.empty()) return;
    for (auto& c : symbol) c = static_cast<char>(std::toupper(c));
    for (auto& s : stocks_) {
        if (s.symbol == symbol) {
            status_ = symbol + " DUPLICATE";
            status_time_ = ImGui::GetTime();
            return;
        }
    }
    auto& wl = watchlists_[selected_watchlist_];
    std::string notes(add_notes_);
    db::ops::add_watchlist_stock(wl.id, symbol, notes);
    load_stocks();
    load_watchlists();
    std::memset(add_symbol_, 0, sizeof(add_symbol_));
    std::memset(add_notes_, 0, sizeof(add_notes_));
    show_add_stock_ = false;
    status_ = "ADDED " + symbol;
    status_time_ = ImGui::GetTime();
}

void WatchlistScreen::remove_stock(const std::string& symbol) {
    if (selected_watchlist_ < 0 || selected_watchlist_ >= static_cast<int>(watchlists_.size()))
        return;
    auto& wl = watchlists_[selected_watchlist_];
    db::ops::remove_watchlist_stock(wl.id, symbol);
    if (selected_stock_ >= 0 && selected_stock_ < static_cast<int>(stocks_.size()) &&
        stocks_[selected_stock_].symbol == symbol) {
        selected_stock_ = -1;
    }
    load_stocks();
    load_watchlists();
    status_ = "REMOVED " + symbol;
    status_time_ = ImGui::GetTime();
}

// ============================================================================
// COMMAND BAR — Bloomberg top bar with WLIST<GO> branding
// ============================================================================
void WatchlistScreen::render_command_bar(float width) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, bb::BG_HEADER);
    ImGui::BeginChild("##bb_cmdbar", ImVec2(width, 28), ImGuiChildFlags_Borders);

    // WLIST<GO> branding
    ImGui::TextColored(bb::AMBER, "WLIST");
    ImGui::SameLine(0, 0);
    ImGui::TextColored(bb::YELLOW, "<GO>");

    ImGui::SameLine(0, 16);
    ImGui::TextColored(bb::BORDER, "|");
    ImGui::SameLine(0, 8);

    // Title
    ImGui::TextColored(bb::WHITE, "WATCHLIST MONITOR");

    ImGui::SameLine(0, 16);
    ImGui::TextColored(bb::BORDER, "|");
    ImGui::SameLine(0, 8);

    // Live status with blink
    bool loading = data_.is_loading();
    if (loading) {
        ImGui::TextColored(bb::YELLOW, "[*]");
        ImGui::SameLine(0, 4);
        ImGui::TextColored(bb::YELLOW, "UPDATING");
    } else {
        ImVec4 dot_color = blink_on_ ? bb::GREEN : ImVec4(0, 0, 0, 0);
        // Draw blinking dot
        ImVec2 p = ImGui::GetCursorScreenPos();
        float cy = p.y + ImGui::GetTextLineHeight() * 0.5f;
        ImGui::GetWindowDrawList()->AddCircleFilled(
            ImVec2(p.x + 4, cy), 3.0f,
            ImGui::ColorConvertFloat4ToU32(dot_color));
        ImGui::Dummy(ImVec2(12, 0));
        ImGui::SameLine(0, 4);
        ImGui::TextColored(bb::GREEN, "LIVE");
    }

    ImGui::SameLine(0, 16);
    ImGui::TextColored(bb::BORDER, "|");
    ImGui::SameLine(0, 8);

    // Security count
    ImGui::TextColored(bb::AMBER_DIM, "SEC:");
    ImGui::SameLine(0, 4);
    ImGui::TextColored(bb::WHITE, "%d", static_cast<int>(stocks_.size()));

    // Right side: timestamp + action buttons
    float right_x = width - 420;
    if (right_x > 300) {
        ImGui::SameLine(right_x);

        // Timestamp
        time_t now = time(nullptr);
        struct tm* t = localtime(&now);
        char datetime[32];
        std::strftime(datetime, sizeof(datetime), "%Y-%m-%d %H:%M:%S", t);
        ImGui::TextColored(bb::CYAN, "%s", datetime);

        ImGui::SameLine(0, 16);
    } else {
        ImGui::SameLine(width - 200);
    }

    // Action buttons
    bool has_wl = selected_watchlist_ >= 0 && selected_watchlist_ < static_cast<int>(watchlists_.size());
    if (has_wl) {
        if (bb_func_key("+SEC", false, 50)) show_add_stock_ = true;
        ImGui::SameLine(0, 2);
    }
    if (bb_func_key("RFSH", false, 50)) {
        if (has_wl) refresh_quotes();
    }
    ImGui::SameLine(0, 2);
    if (bb_func_key("+NEW", false, 50)) show_create_modal_ = true;

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// SIDEBAR — Watchlist navigation + market movers
// ============================================================================
void WatchlistScreen::render_sidebar(float width, float height) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, bb::BG_PANEL);
    ImGui::BeginChild("##bb_sidebar", ImVec2(width, height), ImGuiChildFlags_Borders);

    // Header
    ImGui::PushStyleColor(ImGuiCol_ChildBg, bb::BG_HEADER);
    ImGui::BeginChild("##sb_hdr", ImVec2(0, 22), ImGuiChildFlags_Borders);
    ImGui::TextColored(bb::AMBER, "LISTS");
    ImGui::SameLine(width - 50);
    ImGui::TextColored(bb::WHITE_DIM, "(%d)", static_cast<int>(watchlists_.size()));
    ImGui::EndChild();
    ImGui::PopStyleColor();

    // Watchlist items
    if (watchlists_.empty()) {
        ImGui::Spacing();
        ImGui::TextColored(bb::WHITE_DIM, " No watchlists.");
        ImGui::TextColored(bb::AMBER_DIM, " Press +NEW");
    } else {
        for (int i = 0; i < static_cast<int>(watchlists_.size()); i++) {
            auto& wl = watchlists_[i];
            bool is_sel = (i == selected_watchlist_);

            ImGui::PushID(i);

            // Row background
            ImVec2 row_start = ImGui::GetCursorScreenPos();
            float row_h = ImGui::GetTextLineHeightWithSpacing() + 4;

            if (is_sel) {
                ImGui::GetWindowDrawList()->AddRectFilled(
                    row_start,
                    ImVec2(row_start.x + width - 2, row_start.y + row_h),
                    ImGui::ColorConvertFloat4ToU32(bb::BG_SELECTED));
            }

            // Color indicator bar (3px left edge)
            ImVec4 wl_color = hex_to_color(wl.color);
            ImGui::GetWindowDrawList()->AddRectFilled(
                row_start,
                ImVec2(row_start.x + 3, row_start.y + row_h),
                ImGui::ColorConvertFloat4ToU32(wl_color));

            // Clickable row
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, bb::BORDER_DIM);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 2));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);

            char label[128];
            std::snprintf(label, sizeof(label), " %s%s##wl_%d",
                is_sel ? "> " : "  ", wl.name.c_str(), i);

            ImGui::PushStyleColor(ImGuiCol_Text, is_sel ? bb::AMBER : bb::WHITE);
            if (ImGui::Button(label, ImVec2(width - 38, 0))) {
                if (selected_watchlist_ != i) {
                    selected_watchlist_ = i;
                    load_stocks();
                }
            }
            ImGui::PopStyleColor();
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(2);

            // Stock count badge
            ImGui::SameLine(width - 50);
            ImGui::TextColored(bb::WHITE_DIM, "%d", wl.stock_count);

            // Delete
            ImGui::SameLine(width - 30);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(bb::RED.x, bb::RED.y, bb::RED.z, 0.3f));
            ImGui::PushStyleColor(ImGuiCol_Text, bb::BORDER);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);
            if (ImGui::SmallButton("x")) {
                delete_watchlist(wl.id);
            }
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);
            if (ImGui::IsItemHovered()) {
                ImGui::PushStyleColor(ImGuiCol_PopupBg, bb::BG_HEADER);
                ImGui::SetTooltip("DEL %s", wl.name.c_str());
                ImGui::PopStyleColor();
            }

            ImGui::PopID();
        }
    }

    // ── MARKET MOVERS ──
    if (!stocks_.empty() && data_.has_data()) {
        ImGui::Spacing();

        // Divider line
        bb_separator_line(width - 16);
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_ChildBg, bb::BG_BLACK);
        ImGui::BeginChild("##bb_movers", ImVec2(0, 0), ImGuiChildFlags_Borders);

        ImGui::TextColored(bb::AMBER, "MOVERS");
        ImGui::Spacing();

        // Collect and sort by change
        std::vector<const WatchlistStockEntry*> sorted;
        {
            std::lock_guard<std::mutex> lock(data_.mutex());
            for (auto& s : stocks_) {
                auto* q = data_.quote(s.symbol);
                if (q && q->valid) sorted.push_back(&s);
            }
            std::sort(sorted.begin(), sorted.end(),
                [this](const WatchlistStockEntry* a, const WatchlistStockEntry* b) {
                    auto* qa = data_.quote(a->symbol);
                    auto* qb = data_.quote(b->symbol);
                    return (qb ? qb->change_percent : 0) < (qa ? qa->change_percent : 0);
                });
        }

        // Top gainers
        int shown = 0;
        {
            std::lock_guard<std::mutex> lock(data_.mutex());
            for (auto* s : sorted) {
                if (shown >= 3) break;
                auto* q = data_.quote(s->symbol);
                if (!q || q->change_percent <= 0) break;

                // Green up arrow + symbol + change
                ImGui::TextColored(bb::GREEN, " %s", "\xe2\x96\xb2"); // ▲
                ImGui::SameLine(0, 4);
                ImGui::TextColored(bb::WHITE, "%-6s", s->symbol.c_str());
                ImGui::SameLine(width - 70);
                ImGui::TextColored(bb::GREEN, "+%.2f%%", q->change_percent);
                shown++;
            }
        }

        // Bottom losers
        shown = 0;
        {
            std::lock_guard<std::mutex> lock(data_.mutex());
            for (int i = static_cast<int>(sorted.size()) - 1; i >= 0 && shown < 2; i--) {
                auto* q = data_.quote(sorted[i]->symbol);
                if (!q || q->change_percent >= 0) break;

                ImGui::TextColored(bb::RED, " %s", "\xe2\x96\xbc"); // ▼
                ImGui::SameLine(0, 4);
                ImGui::TextColored(bb::WHITE, "%-6s", sorted[i]->symbol.c_str());
                ImGui::SameLine(width - 70);
                ImGui::TextColored(bb::RED, "%.2f%%", q->change_percent);
                shown++;
            }
        }

        ImGui::EndChild();
        ImGui::PopStyleColor();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// STOCK TABLE — Dense Bloomberg-style data grid
// ============================================================================
void WatchlistScreen::render_stock_table(float width, float height) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, bb::BG_BLACK);
    ImGui::BeginChild("##bb_table", ImVec2(width, height), ImGuiChildFlags_Borders);

    bool has_wl = selected_watchlist_ >= 0 && selected_watchlist_ < static_cast<int>(watchlists_.size());

    if (!has_wl) {
        float cy = height * 0.4f;
        ImGui::SetCursorPosY(cy);
        float tw = ImGui::CalcTextSize("SELECT A WATCHLIST").x;
        ImGui::SetCursorPosX((width - tw) * 0.5f);
        ImGui::TextColored(bb::WHITE_DIM, "SELECT A WATCHLIST");
        ImGui::EndChild();
        ImGui::PopStyleColor();
        return;
    }

    auto& wl = watchlists_[selected_watchlist_];

    // Table header bar with sort function keys
    ImGui::PushStyleColor(ImGuiCol_ChildBg, bb::BG_HEADER);
    ImGui::BeginChild("##tbl_hdr", ImVec2(0, 24), ImGuiChildFlags_Borders);

    ImGui::TextColored(bb::AMBER, "%s", wl.name.c_str());
    ImGui::SameLine(0, 8);
    ImGui::TextColored(bb::WHITE_DIM, "(%d)", static_cast<int>(stocks_.size()));

    // Sort function keys (right side)
    float btn_start = width - 260;
    if (btn_start > 150) {
        ImGui::SameLine(btn_start);
        static const SortCriteria opts[] = {
            SortCriteria::Ticker, SortCriteria::Change,
            SortCriteria::Volume, SortCriteria::Price
        };
        static const char* labels[] = {"F1 SYM", "F2 CHG", "F3 VOL", "F4 PX"};
        for (int i = 0; i < 4; i++) {
            if (bb_func_key(labels[i], sort_by_ == opts[i], 58)) {
                sort_by_ = opts[i];
                sort_stocks();
            }
            if (i < 3) ImGui::SameLine(0, 2);
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();

    // Update quotes into stock entries
    if (data_.has_data()) {
        std::lock_guard<std::mutex> lock(data_.mutex());
        for (auto& s : stocks_) {
            auto* q = data_.quote(s.symbol);
            if (q) s.quote = *q;
        }
        sort_stocks();
    }

    // Data table
    if (stocks_.empty()) {
        ImGui::Spacing();
        float tw = ImGui::CalcTextSize("NO SECURITIES").x;
        ImGui::SetCursorPosX((width - tw) * 0.5f);
        ImGui::TextColored(bb::WHITE_DIM, "NO SECURITIES");
        ImGui::Spacing();
        float tw2 = ImGui::CalcTextSize("Press +SEC to add").x;
        ImGui::SetCursorPosX((width - tw2) * 0.5f);
        ImGui::TextColored(bb::AMBER_DIM, "Press +SEC to add");
    } else {
        // Bloomberg-style dense table
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4, 1));
        ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, bb::BG_HEADER);
        ImGui::PushStyleColor(ImGuiCol_TableBorderStrong, bb::BORDER);
        ImGui::PushStyleColor(ImGuiCol_TableBorderLight, bb::BORDER_DIM);
        ImGui::PushStyleColor(ImGuiCol_TableRowBg, bb::BG_BLACK);
        ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, bb::BG_ROW_ALT);

        if (ImGui::BeginTable("##bb_stock_tbl", 7,
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable |
                ImGuiTableFlags_SizingFixedFit)) {

            ImGui::TableSetupColumn("SYMBOL",  ImGuiTableColumnFlags_WidthFixed, 80);
            ImGui::TableSetupColumn("LAST",    ImGuiTableColumnFlags_WidthFixed, 72);
            ImGui::TableSetupColumn("CHG",     ImGuiTableColumnFlags_WidthFixed, 64);
            ImGui::TableSetupColumn("CHG%",    ImGuiTableColumnFlags_WidthFixed, 60);
            ImGui::TableSetupColumn("VOLUME",  ImGuiTableColumnFlags_WidthFixed, 68);
            ImGui::TableSetupColumn("RANGE",   ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("##del",   ImGuiTableColumnFlags_WidthFixed, 20);

            // Custom header row styling
            ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
            const char* hdrs[] = {"SYMBOL", "LAST", "CHG", "CHG%", "VOLUME", "RANGE", ""};
            for (int col = 0; col < 7; col++) {
                ImGui::TableSetColumnIndex(col);
                ImGui::TextColored(bb::AMBER_DIM, "%s", hdrs[col]);
            }

            for (int i = 0; i < static_cast<int>(stocks_.size()); i++) {
                auto& stock = stocks_[i];
                bool is_sel = (i == selected_stock_);

                ImGui::TableNextRow();
                ImGui::PushID(i);

                // Selected row highlight
                if (is_sel) {
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                        ImGui::ColorConvertFloat4ToU32(bb::BG_SELECTED));
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1,
                        ImGui::ColorConvertFloat4ToU32(bb::BG_SELECTED));
                }

                // SYMBOL
                ImGui::TableNextColumn();
                ImGui::PushStyleColor(ImGuiCol_Header, bb::BG_SELECTED);
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, bb::BG_SELECTED);
                if (ImGui::Selectable(stock.symbol.c_str(), is_sel,
                        ImGuiSelectableFlags_SpanAllColumns)) {
                    selected_stock_ = i;
                }
                ImGui::PopStyleColor(2);

                // LAST
                ImGui::TableNextColumn();
                if (stock.quote.valid) {
                    ImGui::TextColored(bb::WHITE, "%.2f", stock.quote.price);
                } else {
                    ImGui::TextColored(bb::WHITE_DIM, "--");
                }

                // CHG
                ImGui::TableNextColumn();
                if (stock.quote.valid) {
                    ImVec4 cc = stock.quote.change >= 0 ? bb::GREEN : bb::RED;
                    ImGui::TextColored(cc, "%+.2f", stock.quote.change);
                } else {
                    ImGui::TextColored(bb::WHITE_DIM, "--");
                }

                // CHG%
                ImGui::TableNextColumn();
                if (stock.quote.valid) {
                    ImVec4 cc = stock.quote.change_percent >= 0 ? bb::GREEN : bb::RED;
                    // Change percent with background color bar
                    ImVec2 p = ImGui::GetCursorScreenPos();
                    float bar_w = std::min(std::abs((float)stock.quote.change_percent) * 8.0f, 55.0f);
                    ImGui::GetWindowDrawList()->AddRectFilled(
                        p, ImVec2(p.x + bar_w, p.y + ImGui::GetTextLineHeight()),
                        ImGui::ColorConvertFloat4ToU32(ImVec4(cc.x, cc.y, cc.z, 0.12f)));
                    ImGui::TextColored(cc, "%+.2f%%", stock.quote.change_percent);
                } else {
                    ImGui::TextColored(bb::WHITE_DIM, "--");
                }

                // VOLUME
                ImGui::TableNextColumn();
                if (stock.quote.valid && stock.quote.volume > 0) {
                    ImGui::TextColored(bb::YELLOW, "%s", format_volume(stock.quote.volume).c_str());
                } else {
                    ImGui::TextColored(bb::WHITE_DIM, "--");
                }

                // RANGE (mini inline bar)
                ImGui::TableNextColumn();
                if (stock.quote.valid && stock.quote.high > stock.quote.low && stock.quote.high > 0) {
                    ImVec2 p = ImGui::GetCursorScreenPos();
                    float cell_w = ImGui::GetContentRegionAvail().x;
                    float bar_h = 3.0f;
                    float bar_y = p.y + ImGui::GetTextLineHeight() * 0.5f - bar_h * 0.5f;

                    // Track background
                    ImGui::GetWindowDrawList()->AddRectFilled(
                        ImVec2(p.x, bar_y),
                        ImVec2(p.x + cell_w, bar_y + bar_h),
                        ImGui::ColorConvertFloat4ToU32(bb::BORDER_DIM));

                    // Gradient: red → green
                    float half = cell_w * 0.5f;
                    ImGui::GetWindowDrawList()->AddRectFilledMultiColor(
                        ImVec2(p.x, bar_y),
                        ImVec2(p.x + half, bar_y + bar_h),
                        ImGui::ColorConvertFloat4ToU32(ImVec4(bb::RED.x, bb::RED.y, bb::RED.z, 0.4f)),
                        ImGui::ColorConvertFloat4ToU32(ImVec4(bb::YELLOW.x, bb::YELLOW.y, bb::YELLOW.z, 0.3f)),
                        ImGui::ColorConvertFloat4ToU32(ImVec4(bb::YELLOW.x, bb::YELLOW.y, bb::YELLOW.z, 0.3f)),
                        ImGui::ColorConvertFloat4ToU32(ImVec4(bb::RED.x, bb::RED.y, bb::RED.z, 0.4f)));
                    ImGui::GetWindowDrawList()->AddRectFilledMultiColor(
                        ImVec2(p.x + half, bar_y),
                        ImVec2(p.x + cell_w, bar_y + bar_h),
                        ImGui::ColorConvertFloat4ToU32(ImVec4(bb::YELLOW.x, bb::YELLOW.y, bb::YELLOW.z, 0.3f)),
                        ImGui::ColorConvertFloat4ToU32(ImVec4(bb::GREEN.x, bb::GREEN.y, bb::GREEN.z, 0.4f)),
                        ImGui::ColorConvertFloat4ToU32(ImVec4(bb::GREEN.x, bb::GREEN.y, bb::GREEN.z, 0.4f)),
                        ImGui::ColorConvertFloat4ToU32(ImVec4(bb::YELLOW.x, bb::YELLOW.y, bb::YELLOW.z, 0.3f)));

                    // Price position dot
                    float range = (float)(stock.quote.high - stock.quote.low);
                    float pct = (float)((stock.quote.price - stock.quote.low) / range);
                    pct = std::max(0.0f, std::min(1.0f, pct));
                    float dot_x = p.x + pct * cell_w;
                    ImGui::GetWindowDrawList()->AddCircleFilled(
                        ImVec2(dot_x, bar_y + bar_h * 0.5f), 4.0f,
                        ImGui::ColorConvertFloat4ToU32(bb::AMBER));
                    ImGui::GetWindowDrawList()->AddCircle(
                        ImVec2(dot_x, bar_y + bar_h * 0.5f), 4.0f,
                        ImGui::ColorConvertFloat4ToU32(bb::WHITE), 0, 1.0f);

                    ImGui::Dummy(ImVec2(cell_w, ImGui::GetTextLineHeight()));
                } else {
                    ImGui::TextColored(bb::WHITE_DIM, "--");
                }

                // DELETE
                ImGui::TableNextColumn();
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(bb::RED.x, bb::RED.y, bb::RED.z, 0.2f));
                ImGui::PushStyleColor(ImGuiCol_Text, bb::BORDER);
                if (ImGui::SmallButton("x")) {
                    remove_stock(stock.symbol);
                }
                ImGui::PopStyleColor(3);

                ImGui::PopID();
            }
            ImGui::EndTable();
        }

        ImGui::PopStyleColor(5);
        ImGui::PopStyleVar();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// DETAIL PANEL — Bloomberg security detail view
// ============================================================================
void WatchlistScreen::render_detail_panel(float width, float height) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, bb::BG_PANEL);
    ImGui::BeginChild("##bb_detail", ImVec2(width, height), ImGuiChildFlags_Borders);

    if (selected_stock_ < 0 || selected_stock_ >= static_cast<int>(stocks_.size())) {
        float cy = height * 0.35f;
        ImGui::SetCursorPosY(cy);
        float tw = ImGui::CalcTextSize("SELECT SECURITY").x;
        ImGui::SetCursorPosX((width - tw) * 0.5f);
        ImGui::TextColored(bb::WHITE_DIM, "SELECT SECURITY");
        ImGui::EndChild();
        ImGui::PopStyleColor();
        return;
    }

    auto& stock = stocks_[selected_stock_];

    // Security header
    ImGui::PushStyleColor(ImGuiCol_ChildBg, bb::BG_HEADER);
    ImGui::BeginChild("##det_hdr", ImVec2(0, 22), ImGuiChildFlags_Borders);
    ImGui::TextColored(bb::AMBER, "DES");
    ImGui::SameLine(0, 8);
    ImGui::TextColored(bb::WHITE, "%s", stock.symbol.c_str());
    ImGui::SameLine(0, 4);
    ImGui::TextColored(bb::WHITE_DIM, "US Equity");
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Spacing();

    if (!stock.quote.valid) {
        // Error state
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled(
            p, ImVec2(p.x + width - 16, p.y + 20),
            ImGui::ColorConvertFloat4ToU32(ImVec4(bb::RED.x, bb::RED.y, bb::RED.z, 0.1f)));
        ImGui::TextColored(bb::RED, " NO MARKET DATA");
        ImGui::Spacing();
    } else {
        auto& q = stock.quote;
        ImVec4 chg_color = q.change_percent >= 0 ? bb::GREEN : bb::RED;

        // Big price display
        ImGui::SetWindowFontScale(1.8f);
        ImGui::TextColored(bb::WHITE, " %.2f", q.price);
        ImGui::SetWindowFontScale(1.0f);

        // Change badge with colored background
        ImGui::SameLine(0, 12);
        ImVec2 badge_pos = ImGui::GetCursorScreenPos();
        char chg_text[32];
        std::snprintf(chg_text, sizeof(chg_text), " %+.2f (%+.2f%%) ",
            q.change, q.change_percent);
        ImVec2 badge_sz = ImGui::CalcTextSize(chg_text);

        // Badge background
        ImGui::GetWindowDrawList()->AddRectFilled(
            badge_pos,
            ImVec2(badge_pos.x + badge_sz.x + 4, badge_pos.y + badge_sz.y + 2),
            ImGui::ColorConvertFloat4ToU32(ImVec4(chg_color.x, chg_color.y, chg_color.z, 0.2f)));
        ImGui::TextColored(chg_color, "%s", chg_text);

        ImGui::Spacing();

        // Horizontal divider
        bb_separator_line(width - 16);
        ImGui::Spacing();

        // Stats grid — Bloomberg style label:value pairs
        char buf[32];

        std::snprintf(buf, sizeof(buf), "%.2f", q.open);
        bb_label("OPEN", buf, bb::WHITE);

        std::snprintf(buf, sizeof(buf), "%.2f", q.previous_close);
        bb_label("PREV CLOSE", buf, bb::WHITE);

        std::snprintf(buf, sizeof(buf), "%.2f", q.high);
        bb_label("DAY HIGH", buf, bb::GREEN);

        std::snprintf(buf, sizeof(buf), "%.2f", q.low);
        bb_label("DAY LOW", buf, bb::RED);

        bb_label("VOLUME", format_volume(q.volume).c_str(), bb::YELLOW);

        // Day range calculation
        if (q.high > q.low && q.high > 0) {
            ImGui::Spacing();
            bb_separator_line(width - 16);
            ImGui::Spacing();

            ImGui::TextColored(bb::AMBER_DIM, "DAY RANGE");
            ImGui::Spacing();

            // Low label
            ImGui::TextColored(bb::RED, "%.2f", q.low);
            ImGui::SameLine(0, 8);

            // Range bar
            float bar_w = width - 120;
            if (bar_w < 60) bar_w = 60;
            ImVec2 bar_pos = ImGui::GetCursorScreenPos();
            float bar_h = 8.0f;

            // Track
            ImGui::GetWindowDrawList()->AddRectFilled(
                bar_pos,
                ImVec2(bar_pos.x + bar_w, bar_pos.y + bar_h),
                ImGui::ColorConvertFloat4ToU32(bb::BORDER_DIM));

            // Gradient fill (red -> amber -> green)
            float third = bar_w / 3.0f;
            ImGui::GetWindowDrawList()->AddRectFilledMultiColor(
                bar_pos,
                ImVec2(bar_pos.x + third, bar_pos.y + bar_h),
                ImGui::ColorConvertFloat4ToU32(ImVec4(bb::RED.x, bb::RED.y, bb::RED.z, 0.6f)),
                ImGui::ColorConvertFloat4ToU32(ImVec4(bb::AMBER.x, bb::AMBER.y, bb::AMBER.z, 0.5f)),
                ImGui::ColorConvertFloat4ToU32(ImVec4(bb::AMBER.x, bb::AMBER.y, bb::AMBER.z, 0.5f)),
                ImGui::ColorConvertFloat4ToU32(ImVec4(bb::RED.x, bb::RED.y, bb::RED.z, 0.6f)));
            ImGui::GetWindowDrawList()->AddRectFilledMultiColor(
                ImVec2(bar_pos.x + third, bar_pos.y),
                ImVec2(bar_pos.x + third * 2, bar_pos.y + bar_h),
                ImGui::ColorConvertFloat4ToU32(ImVec4(bb::AMBER.x, bb::AMBER.y, bb::AMBER.z, 0.5f)),
                ImGui::ColorConvertFloat4ToU32(ImVec4(bb::YELLOW.x, bb::YELLOW.y, bb::YELLOW.z, 0.5f)),
                ImGui::ColorConvertFloat4ToU32(ImVec4(bb::YELLOW.x, bb::YELLOW.y, bb::YELLOW.z, 0.5f)),
                ImGui::ColorConvertFloat4ToU32(ImVec4(bb::AMBER.x, bb::AMBER.y, bb::AMBER.z, 0.5f)));
            ImGui::GetWindowDrawList()->AddRectFilledMultiColor(
                ImVec2(bar_pos.x + third * 2, bar_pos.y),
                ImVec2(bar_pos.x + bar_w, bar_pos.y + bar_h),
                ImGui::ColorConvertFloat4ToU32(ImVec4(bb::YELLOW.x, bb::YELLOW.y, bb::YELLOW.z, 0.5f)),
                ImGui::ColorConvertFloat4ToU32(ImVec4(bb::GREEN.x, bb::GREEN.y, bb::GREEN.z, 0.6f)),
                ImGui::ColorConvertFloat4ToU32(ImVec4(bb::GREEN.x, bb::GREEN.y, bb::GREEN.z, 0.6f)),
                ImGui::ColorConvertFloat4ToU32(ImVec4(bb::YELLOW.x, bb::YELLOW.y, bb::YELLOW.z, 0.5f)));

            // Current price marker (diamond shape)
            float range = (float)(q.high - q.low);
            float pct = (float)((q.price - q.low) / range);
            pct = std::max(0.0f, std::min(1.0f, pct));
            float marker_x = bar_pos.x + pct * bar_w;
            float marker_y = bar_pos.y + bar_h * 0.5f;

            // Diamond marker
            ImVec2 diamond[4] = {
                {marker_x, marker_y - 6},
                {marker_x + 5, marker_y},
                {marker_x, marker_y + 6},
                {marker_x - 5, marker_y}
            };
            ImGui::GetWindowDrawList()->AddConvexPolyFilled(diamond, 4,
                ImGui::ColorConvertFloat4ToU32(bb::AMBER));
            ImGui::GetWindowDrawList()->AddPolyline(diamond, 4,
                ImGui::ColorConvertFloat4ToU32(bb::WHITE), ImDrawFlags_Closed, 1.0f);

            ImGui::Dummy(ImVec2(bar_w, bar_h + 8));

            // High label
            ImGui::SameLine(0, 8);
            ImGui::TextColored(bb::GREEN, "%.2f", q.high);
        }

        // Price position percentage
        if (q.high > q.low) {
            float range = (float)(q.high - q.low);
            float pct = (float)((q.price - q.low) / range) * 100.0f;
            char pos_buf[32];
            std::snprintf(pos_buf, sizeof(pos_buf), "%.0f%%", pct);
            bb_label("POSITION", pos_buf, bb::CYAN);
        }
    }

    // Notes section
    if (!stock.notes.empty()) {
        ImGui::Spacing();
        bb_separator_line(width - 16);
        ImGui::Spacing();

        ImGui::TextColored(bb::AMBER, "NOTES");

        ImGui::PushStyleColor(ImGuiCol_ChildBg, bb::BG_BLACK);
        ImGui::BeginChild("##bb_notes", ImVec2(0, 60), ImGuiChildFlags_Borders);
        ImGui::TextColored(bb::WHITE_DIM, "%s", stock.notes.c_str());
        ImGui::EndChild();
        ImGui::PopStyleColor();
    }

    // Added date
    ImGui::Spacing();
    ImGui::TextColored(bb::BORDER, "ADDED %s", stock.added_at.c_str());

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// FOOTER BAR — Bloomberg status line
// ============================================================================
void WatchlistScreen::render_footer_bar(float width) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, bb::BG_HEADER);
    ImGui::BeginChild("##bb_footer", ImVec2(width, ImGui::GetFrameHeight()), ImGuiChildFlags_Borders);

    // Left: branding
    ImGui::TextColored(bb::AMBER, "WLIST");
    ImGui::SameLine(0, 8);
    ImGui::TextColored(bb::BORDER, "|");
    ImGui::SameLine(0, 8);

    // Stats
    ImGui::TextColored(bb::AMBER_DIM, "LISTS");
    ImGui::SameLine(0, 4);
    ImGui::TextColored(bb::WHITE, "%d", static_cast<int>(watchlists_.size()));
    ImGui::SameLine(0, 12);

    ImGui::TextColored(bb::AMBER_DIM, "SEC");
    ImGui::SameLine(0, 4);
    ImGui::TextColored(bb::WHITE, "%d", static_cast<int>(stocks_.size()));
    ImGui::SameLine(0, 12);

    ImGui::TextColored(bb::AMBER_DIM, "STATUS");
    ImGui::SameLine(0, 4);
    if (data_.is_loading()) {
        ImGui::TextColored(bb::YELLOW, "LOADING");
    } else {
        ImGui::TextColored(bb::GREEN, "CONNECTED");
    }

    // Right: status message (fades after 5s)
    if (!status_.empty()) {
        double elapsed = ImGui::GetTime() - status_time_;
        if (elapsed < 5.0) {
            // Flash effect for first second
            bool show = true;
            if (elapsed < 1.0) {
                show = (int)(elapsed * 6) % 2 == 0;
            }
            if (show) {
                float msg_w = ImGui::CalcTextSize(status_.c_str()).x + 24;
                ImGui::SameLine(width - msg_w);
                ImGui::TextColored(bb::YELLOW, "%s", status_.c_str());
            }
        } else {
            status_.clear();
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// CREATE WATCHLIST MODAL — Bloomberg-styled dark modal
// ============================================================================
void WatchlistScreen::render_create_modal() {
    if (!show_create_modal_) return;

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(400, 300));

    ImGui::PushStyleColor(ImGuiCol_WindowBg, bb::BG_PANEL);
    ImGui::PushStyleColor(ImGuiCol_TitleBg, bb::BG_HEADER);
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, bb::BG_HEADER);
    ImGui::PushStyleColor(ImGuiCol_Border, bb::AMBER_DIM);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);

    if (ImGui::Begin("NEW WATCHLIST##bb_modal", &show_create_modal_,
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoDocking)) {

        // Name
        ImGui::TextColored(bb::AMBER, "NAME *");
        ImGui::PushStyleColor(ImGuiCol_FrameBg, bb::BG_BLACK);
        ImGui::PushStyleColor(ImGuiCol_Text, bb::WHITE);
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##wl_name", "WATCHLIST NAME",
            create_name_, sizeof(create_name_));
        ImGui::PopStyleColor(2);

        ImGui::Spacing();

        // Description
        ImGui::TextColored(bb::AMBER, "DESCRIPTION");
        ImGui::PushStyleColor(ImGuiCol_FrameBg, bb::BG_BLACK);
        ImGui::PushStyleColor(ImGuiCol_Text, bb::WHITE);
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextMultiline("##wl_desc", create_desc_, sizeof(create_desc_),
            ImVec2(-1, 50));
        ImGui::PopStyleColor(2);

        ImGui::Spacing();

        // Color picker (horizontal swatches)
        ImGui::TextColored(bb::AMBER, "COLOR");
        const auto& colors = watchlist_colors();
        for (int i = 0; i < static_cast<int>(colors.size()); i++) {
            ImVec4 c = hex_to_color(colors[i]);
            bool selected = (i == create_color_idx_);

            ImGui::PushID(i);
            ImGui::PushStyleColor(ImGuiCol_Button, c);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                ImVec4(c.x * 0.7f, c.y * 0.7f, c.z * 0.7f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);

            float sz = selected ? 28.0f : 24.0f;
            if (ImGui::Button("##clr", ImVec2(sz, sz))) {
                create_color_idx_ = i;
            }
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);

            if (selected) {
                ImVec2 mn = ImGui::GetItemRectMin();
                ImVec2 mx = ImGui::GetItemRectMax();
                ImGui::GetWindowDrawList()->AddRect(mn, mx,
                    ImGui::ColorConvertFloat4ToU32(bb::WHITE), 0, 0, 2.0f);
            }

            ImGui::PopID();
            if (i < static_cast<int>(colors.size()) - 1) ImGui::SameLine(0, 4);
        }

        ImGui::Spacing();
        bb_separator_line(380);
        ImGui::Spacing();

        // Buttons
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 180);

        if (bb_func_key("CANCEL", false, 70)) {
            show_create_modal_ = false;
        }
        ImGui::SameLine(0, 8);

        bool can_create = create_name_[0] != '\0';
        if (!can_create) ImGui::BeginDisabled();

        ImGui::PushStyleColor(ImGuiCol_Button, bb::AMBER_DIM);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, bb::AMBER);
        ImGui::PushStyleColor(ImGuiCol_Text, bb::BG_BLACK);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);
        if (ImGui::Button("CREATE", ImVec2(70, 0))) {
            create_watchlist();
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);

        if (!can_create) ImGui::EndDisabled();
    }
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);
}

// ============================================================================
// ADD STOCK MODAL
// ============================================================================
void WatchlistScreen::render_add_stock_modal() {
    if (!show_add_stock_) return;

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(380, 240));

    ImGui::PushStyleColor(ImGuiCol_WindowBg, bb::BG_PANEL);
    ImGui::PushStyleColor(ImGuiCol_TitleBg, bb::BG_HEADER);
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, bb::BG_HEADER);
    ImGui::PushStyleColor(ImGuiCol_Border, bb::AMBER_DIM);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);

    if (ImGui::Begin("ADD SECURITY##bb_add", &show_add_stock_,
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoDocking)) {

        // Symbol input
        ImGui::TextColored(bb::AMBER, "TICKER *");
        ImGui::PushStyleColor(ImGuiCol_FrameBg, bb::BG_BLACK);
        ImGui::PushStyleColor(ImGuiCol_Text, bb::WHITE);
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##add_sym", "AAPL",
            add_symbol_, sizeof(add_symbol_),
            ImGuiInputTextFlags_CharsUppercase);
        ImGui::PopStyleColor(2);

        ImGui::TextColored(bb::WHITE_DIM, "Enter ticker symbol (AAPL, MSFT, GOOGL)");

        ImGui::Spacing();

        // Notes
        ImGui::TextColored(bb::AMBER, "NOTES");
        ImGui::PushStyleColor(ImGuiCol_FrameBg, bb::BG_BLACK);
        ImGui::PushStyleColor(ImGuiCol_Text, bb::WHITE);
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextMultiline("##add_notes", add_notes_, sizeof(add_notes_),
            ImVec2(-1, 50));
        ImGui::PopStyleColor(2);

        ImGui::Spacing();
        bb_separator_line(360);
        ImGui::Spacing();

        // Buttons
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 180);

        if (bb_func_key("CANCEL", false, 70)) {
            show_add_stock_ = false;
        }
        ImGui::SameLine(0, 8);

        bool can_add = add_symbol_[0] != '\0';
        if (!can_add) ImGui::BeginDisabled();

        ImGui::PushStyleColor(ImGuiCol_Button, bb::GREEN);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
            ImVec4(bb::GREEN.x * 0.8f, bb::GREEN.y * 0.8f, bb::GREEN.z * 0.8f, 1));
        ImGui::PushStyleColor(ImGuiCol_Text, bb::BG_BLACK);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);
        if (ImGui::Button("ADD", ImVec2(70, 0))) {
            add_stock();
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);

        if (!can_add) ImGui::EndDisabled();
    }
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);
}

// ============================================================================
// MAIN RENDER — Bloomberg 4-zone layout
// ============================================================================
void WatchlistScreen::render() {
    init();

    // Blink timer (1Hz pulse for live indicator)
    float dt = ImGui::GetIO().DeltaTime;
    blink_timer_ += dt;
    if (blink_timer_ >= 0.8f) {
        blink_on_ = !blink_on_;
        blink_timer_ = 0;
    }

    // Auto-refresh
    refresh_timer_ += dt;
    if (refresh_timer_ >= REFRESH_INTERVAL && !data_.is_loading()) {
        refresh_quotes();
    }

    // Full-screen Bloomberg frame
    ui::ScreenFrame frame("##watchlist", ImVec2(0, 0), bb::BG_BLACK);
    if (!frame.begin()) { frame.end(); return; }

    float total_w = frame.width();
    float total_h = frame.height();

    // ── ZONE 1: Command bar (top) ──
    float cmd_h = 28;
    render_command_bar(total_w);

    // ── ZONE 2: Three-panel content area ──
    float footer_h = ImGui::GetFrameHeight() + 2;
    float content_h = total_h - cmd_h - footer_h - 4;

    // Use yoga for responsive three-panel layout
    auto panels = ui::three_panel_layout(
        total_w, content_h,
        16, 22,          // left%, right%
        180, 240, 300,   // min widths
        2                // gap
    );

    render_sidebar(panels.left_w, panels.left_h);
    ImGui::SameLine(0, panels.gap);
    render_stock_table(panels.center_w, panels.center_h);
    if (panels.right_w > 0) {
        ImGui::SameLine(0, panels.gap);
        render_detail_panel(panels.right_w, panels.right_h);
    }

    // ── ZONE 3: Footer status bar ──
    render_footer_bar(total_w);

    frame.end();

    // Modals (rendered outside frame)
    render_create_modal();
    render_add_stock_modal();
}

} // namespace fincept::watchlist
