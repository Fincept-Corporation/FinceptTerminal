#include "watchlist_screen.h"
#include "ui/yoga_helpers.h"
#include "ui/theme.h"
#include <imgui.h>
#include <cstring>
#include <cstdio>
#include <ctime>
#include <algorithm>

namespace fincept::watchlist {

// ============================================================================
// Init & data loading
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
        // Count stocks
        auto stocks = db::ops::get_watchlist_stocks(wl.id);
        wc.stock_count = static_cast<int>(stocks.size());
        watchlists_.push_back(std::move(wc));
    }

    // Auto-select first if nothing selected
    if (selected_watchlist_ < 0 && !watchlists_.empty()) {
        selected_watchlist_ = 0;
        load_stocks();
    }
    // Validate selection
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

    // Fetch quotes for loaded stocks
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
                case SortCriteria::Ticker:
                    return a.symbol < b.symbol;
                case SortCriteria::Change:
                    return b.quote.change_percent < a.quote.change_percent;
                case SortCriteria::Volume:
                    return b.quote.volume < a.quote.volume;
                case SortCriteria::Price:
                    return b.quote.price < a.quote.price;
            }
            return false;
        });
}

// ============================================================================
// Actions
// ============================================================================
void WatchlistScreen::create_watchlist() {
    std::string name(create_name_);
    if (name.empty()) return;

    std::string desc(create_desc_);
    const auto& colors = watchlist_colors();
    std::string color = colors[create_color_idx_ % colors.size()];

    db::ops::create_watchlist(name, color, desc);
    load_watchlists();

    // Select the new one (last added)
    if (!watchlists_.empty()) {
        selected_watchlist_ = static_cast<int>(watchlists_.size()) - 1;
        load_stocks();
    }

    std::memset(create_name_, 0, sizeof(create_name_));
    std::memset(create_desc_, 0, sizeof(create_desc_));
    create_color_idx_ = 0;
    show_create_modal_ = false;

    status_ = "Watchlist created: " + name;
    status_time_ = ImGui::GetTime();
}

void WatchlistScreen::delete_watchlist(const std::string& id) {
    db::ops::delete_watchlist(id);
    load_watchlists();

    status_ = "Watchlist deleted";
    status_time_ = ImGui::GetTime();
}

void WatchlistScreen::add_stock() {
    if (selected_watchlist_ < 0 || selected_watchlist_ >= static_cast<int>(watchlists_.size()))
        return;

    std::string symbol(add_symbol_);
    if (symbol.empty()) return;

    // Uppercase
    for (auto& c : symbol) c = static_cast<char>(std::toupper(c));

    // Check duplicate
    for (auto& s : stocks_) {
        if (s.symbol == symbol) {
            status_ = symbol + " already in this watchlist";
            status_time_ = ImGui::GetTime();
            return;
        }
    }

    auto& wl = watchlists_[selected_watchlist_];
    std::string notes(add_notes_);
    db::ops::add_watchlist_stock(wl.id, symbol, notes);

    load_stocks();
    load_watchlists(); // Update count

    std::memset(add_symbol_, 0, sizeof(add_symbol_));
    std::memset(add_notes_, 0, sizeof(add_notes_));
    show_add_stock_ = false;

    status_ = "Added " + symbol;
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

    status_ = "Removed " + symbol;
    status_time_ = ImGui::GetTime();
}

// ============================================================================
// Header bar — top navigation with status and action buttons
// ============================================================================
void WatchlistScreen::render_header_bar() {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##wl_header", ImVec2(0, 36), ImGuiChildFlags_Borders);

    // Left: title + status
    ImGui::TextColored(ACCENT, "WATCHLIST MONITOR");
    ImGui::SameLine(0, 12);
    ImGui::TextColored(BORDER_BRIGHT, "|");
    ImGui::SameLine(0, 12);

    // Status dot
    bool loading = data_.is_loading();
    ImVec4 dot_color = loading ? WARNING : MARKET_GREEN;
    ImGui::TextColored(dot_color, "[*]");
    ImGui::SameLine(0, 4);
    ImGui::TextColored(dot_color, "%s", loading ? "UPDATING" : "LIVE");

    ImGui::SameLine(0, 12);
    ImGui::TextColored(BORDER_BRIGHT, "|");
    ImGui::SameLine(0, 12);

    // Timestamp
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    char datetime[32];
    std::strftime(datetime, sizeof(datetime), "%Y-%m-%d %H:%M:%S", t);
    ImGui::TextColored(INFO, "%s", datetime);

    // Right: action buttons
    float right_x = ImGui::GetWindowWidth() - 340;
    ImGui::SameLine(right_x);

    bool has_wl = selected_watchlist_ >= 0 && selected_watchlist_ < static_cast<int>(watchlists_.size());

    if (has_wl) {
        if (theme::SecondaryButton("+ ADD STOCK")) {
            show_add_stock_ = true;
        }
        ImGui::SameLine();
    }

    if (theme::SecondaryButton("REFRESH")) {
        if (has_wl) refresh_quotes();
    }

    ImGui::SameLine();
    if (theme::AccentButton("+ NEW LIST")) {
        show_create_modal_ = true;
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Info bar — active watchlist summary
// ============================================================================
void WatchlistScreen::render_info_bar() {
    using namespace theme::colors;

    if (selected_watchlist_ < 0 || selected_watchlist_ >= static_cast<int>(watchlists_.size()))
        return;

    auto& wl = watchlists_[selected_watchlist_];

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##wl_info", ImVec2(0, 22), ImGuiChildFlags_Borders);

    ImGui::TextColored(TEXT_DIM, "ACTIVE LIST");
    ImGui::SameLine(0, 8);
    ImGui::TextColored(WARNING, "%s", wl.name.c_str());
    ImGui::SameLine(0, 16);
    ImGui::TextColored(BORDER_BRIGHT, "|");
    ImGui::SameLine(0, 16);
    ImGui::TextColored(TEXT_DIM, "SYMBOLS");
    ImGui::SameLine(0, 8);
    ImGui::TextColored(INFO, "%d", static_cast<int>(stocks_.size()));
    ImGui::SameLine(0, 16);
    ImGui::TextColored(BORDER_BRIGHT, "|");
    ImGui::SameLine(0, 16);
    ImGui::TextColored(TEXT_DIM, "SORT");
    ImGui::SameLine(0, 8);
    ImGui::TextColored(INFO, "%s", sort_label(sort_by_));

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Sidebar — watchlist list + market stats
// ============================================================================
void WatchlistScreen::render_sidebar(float width, float height) {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##wl_sidebar", ImVec2(width, height), ImGuiChildFlags_Borders);

    // Header
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARK);
    ImGui::BeginChild("##wl_sidebar_hdr", ImVec2(0, 32), ImGuiChildFlags_Borders);
    char hdr[64];
    std::snprintf(hdr, sizeof(hdr), "WATCHLISTS (%d)", static_cast<int>(watchlists_.size()));
    ImGui::TextColored(TEXT_DIM, "%s", hdr);
    ImGui::SameLine(width - 60);
    ImGui::PushStyleColor(ImGuiCol_Button, ACCENT);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ACCENT_HOVER);
    ImGui::PushStyleColor(ImGuiCol_Text, BG_DARKEST);
    if (ImGui::SmallButton("+ NEW")) {
        show_create_modal_ = true;
    }
    ImGui::PopStyleColor(3);
    ImGui::EndChild();
    ImGui::PopStyleColor();

    // Watchlist items
    if (watchlists_.empty()) {
        ImGui::Spacing();
        ImGui::TextColored(TEXT_DIM, "  No watchlists yet.");
        ImGui::TextColored(TEXT_DIM, "  Click + NEW to create one.");
    } else {
        for (int i = 0; i < static_cast<int>(watchlists_.size()); i++) {
            auto& wl = watchlists_[i];
            bool is_selected = (i == selected_watchlist_);

            ImGui::PushID(i);

            // Highlight selected
            if (is_selected) {
                ImGui::PushStyleColor(ImGuiCol_Button, ACCENT_BG);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ACCENT_BG);
                ImGui::PushStyleColor(ImGuiCol_Text, TEXT_PRIMARY);
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, BG_HOVER);
                ImGui::PushStyleColor(ImGuiCol_Text, TEXT_SECONDARY);
            }

            // Color dot + name
            ImVec4 wl_color = hex_to_color(wl.color);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 6));

            char label[128];
            std::snprintf(label, sizeof(label), "%s (%d)##wl_%d",
                wl.name.c_str(), wl.stock_count, i);

            if (ImGui::Button(label, ImVec2(width - 44, 0))) {
                if (selected_watchlist_ != i) {
                    selected_watchlist_ = i;
                    load_stocks();
                }
            }

            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);

            // Color indicator (drawn after button)
            ImVec2 btn_min = ImGui::GetItemRectMin();
            ImGui::GetWindowDrawList()->AddRectFilled(
                ImVec2(btn_min.x, btn_min.y),
                ImVec2(btn_min.x + 3, ImGui::GetItemRectMax().y),
                ImGui::ColorConvertFloat4ToU32(wl_color));

            // Delete button
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, BG_HOVER);
            ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
            if (ImGui::SmallButton("X")) {
                delete_watchlist(wl.id);
            }
            ImGui::PopStyleColor(3);
            if (ImGui::IsItemHovered()) {
                ImGui::PushStyleColor(ImGuiCol_Text, ERROR_RED);
                ImGui::SetTooltip("Delete %s", wl.name.c_str());
                ImGui::PopStyleColor();
            }

            ImGui::PopID();
        }
    }

    // Market movers section (bottom of sidebar)
    // Show top gainers/losers from current watchlist stocks
    if (!stocks_.empty() && data_.has_data()) {
        ImGui::Spacing();
        ImGui::Separator();

        ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARK);
        ImGui::BeginChild("##mkt_movers", ImVec2(0, 0), ImGuiChildFlags_Borders);
        ImGui::TextColored(TEXT_DIM, "MARKET MOVERS");
        ImGui::Separator();

        // Collect stocks with valid quotes, sort by change
        std::vector<const WatchlistStockEntry*> sorted;
        for (auto& s : stocks_) {
            std::lock_guard<std::mutex> lock(data_.mutex());
            auto* q = data_.quote(s.symbol);
            if (q && q->valid) sorted.push_back(&s);
        }

        // Sort by change_percent
        {
            std::lock_guard<std::mutex> lock(data_.mutex());
            std::sort(sorted.begin(), sorted.end(),
                [this](const WatchlistStockEntry* a, const WatchlistStockEntry* b) {
                    auto* qa = data_.quote(a->symbol);
                    auto* qb = data_.quote(b->symbol);
                    double ca = qa ? qa->change_percent : 0;
                    double cb = qb ? qb->change_percent : 0;
                    return cb < ca;
                });
        }

        // Top 3 gainers
        int shown = 0;
        {
            std::lock_guard<std::mutex> lock(data_.mutex());
            for (auto* s : sorted) {
                if (shown >= 3) break;
                auto* q = data_.quote(s->symbol);
                if (!q || q->change_percent <= 0) break;
                ImGui::TextColored(MARKET_GREEN, " + ");
                ImGui::SameLine();
                ImGui::TextColored(INFO, "%-6s", s->symbol.c_str());
                ImGui::SameLine(width - 80);
                ImGui::TextColored(MARKET_GREEN, "+%.2f%%", q->change_percent);
                shown++;
            }
        }

        // Bottom 2 losers
        shown = 0;
        {
            std::lock_guard<std::mutex> lock(data_.mutex());
            for (int i = static_cast<int>(sorted.size()) - 1; i >= 0 && shown < 2; i--) {
                auto* q = data_.quote(sorted[i]->symbol);
                if (!q || q->change_percent >= 0) break;
                ImGui::TextColored(MARKET_RED, " - ");
                ImGui::SameLine();
                ImGui::TextColored(INFO, "%-6s", sorted[i]->symbol.c_str());
                ImGui::SameLine(width - 80);
                ImGui::TextColored(MARKET_RED, "%.2f%%", q->change_percent);
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
// Stock list — table with sort buttons
// ============================================================================
void WatchlistScreen::render_stock_list(float width, float height) {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARK);
    ImGui::BeginChild("##wl_stocks", ImVec2(width, height), ImGuiChildFlags_Borders);

    bool has_wl = selected_watchlist_ >= 0 && selected_watchlist_ < static_cast<int>(watchlists_.size());

    if (!has_wl) {
        float cy = height * 0.4f;
        ImGui::SetCursorPosY(cy);
        float tw = ImGui::CalcTextSize("SELECT A WATCHLIST TO VIEW").x;
        ImGui::SetCursorPosX((width - tw) * 0.5f);
        ImGui::TextColored(TEXT_DIM, "SELECT A WATCHLIST TO VIEW");
        ImGui::EndChild();
        ImGui::PopStyleColor();
        return;
    }

    auto& wl = watchlists_[selected_watchlist_];

    // Header with sort buttons
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##stock_hdr", ImVec2(0, 28), ImGuiChildFlags_Borders);

    char title[64];
    std::snprintf(title, sizeof(title), "%s (%d)", wl.name.c_str(), static_cast<int>(stocks_.size()));
    ImGui::TextColored(TEXT_DIM, "%s", title);

    // Sort buttons (right side)
    static const SortCriteria sort_opts[] = {
        SortCriteria::Ticker, SortCriteria::Change,
        SortCriteria::Volume, SortCriteria::Price
    };

    float btn_start = width - 240;
    ImGui::SameLine(btn_start);

    for (int i = 0; i < 4; i++) {
        bool active = (sort_by_ == sort_opts[i]);
        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Button, ACCENT);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ACCENT_HOVER);
            ImGui::PushStyleColor(ImGuiCol_Text, BG_DARKEST);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, BG_WIDGET);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, BG_HOVER);
            ImGui::PushStyleColor(ImGuiCol_Text, TEXT_SECONDARY);
        }

        if (ImGui::SmallButton(sort_label(sort_opts[i]))) {
            sort_by_ = sort_opts[i];
            sort_stocks();
        }
        ImGui::PopStyleColor(3);
        if (i < 3) ImGui::SameLine(0, 2);
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

    // Stock table
    if (stocks_.empty()) {
        ImGui::Spacing();
        float tw = ImGui::CalcTextSize("NO STOCKS IN THIS WATCHLIST").x;
        ImGui::SetCursorPosX((width - tw) * 0.5f);
        ImGui::TextColored(TEXT_DIM, "NO STOCKS IN THIS WATCHLIST");
        ImGui::SetCursorPosX((width - ImGui::CalcTextSize("Click ADD STOCK to get started").x) * 0.5f);
        ImGui::TextColored(TEXT_DIM, "Click ADD STOCK to get started");
    } else {
        if (ImGui::BeginTable("##stock_table", 5,
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable)) {

            ImGui::TableSetupColumn("SYMBOL", ImGuiTableColumnFlags_WidthFixed, 90);
            ImGui::TableSetupColumn("PRICE", ImGuiTableColumnFlags_WidthFixed, 80);
            ImGui::TableSetupColumn("CHANGE", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("VOLUME", ImGuiTableColumnFlags_WidthFixed, 80);
            ImGui::TableSetupColumn("##act", ImGuiTableColumnFlags_WidthFixed, 30);
            ImGui::TableHeadersRow();

            for (int i = 0; i < static_cast<int>(stocks_.size()); i++) {
                auto& stock = stocks_[i];
                bool is_selected = (i == selected_stock_);

                ImGui::TableNextRow();
                ImGui::PushID(i);

                // Row click highlight
                if (is_selected) {
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                        ImGui::ColorConvertFloat4ToU32(ACCENT_BG));
                }

                // Symbol
                ImGui::TableNextColumn();
                if (ImGui::Selectable(stock.symbol.c_str(), is_selected,
                        ImGuiSelectableFlags_SpanAllColumns)) {
                    selected_stock_ = i;
                }

                // Price
                ImGui::TableNextColumn();
                if (stock.quote.valid) {
                    ImGui::TextColored(TEXT_PRIMARY, "$%.2f", stock.quote.price);
                } else {
                    ImGui::TextColored(TEXT_DIM, "--");
                }

                // Change
                ImGui::TableNextColumn();
                if (stock.quote.valid) {
                    ImVec4 chg_color = stock.quote.change_percent >= 0 ? MARKET_GREEN : MARKET_RED;
                    ImGui::TextColored(chg_color, "%s%.2f%%",
                        stock.quote.change_percent >= 0 ? "+" : "",
                        stock.quote.change_percent);
                    ImGui::SameLine(0, 8);
                    ImGui::TextColored(chg_color, "($%.2f)", stock.quote.change);
                } else {
                    ImGui::TextColored(TEXT_DIM, "--");
                }

                // Volume
                ImGui::TableNextColumn();
                if (stock.quote.valid && stock.quote.volume > 0) {
                    ImGui::TextColored(WARNING, "%s", format_volume(stock.quote.volume).c_str());
                } else {
                    ImGui::TextColored(TEXT_DIM, "--");
                }

                // Remove button
                ImGui::TableNextColumn();
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, BG_HOVER);
                ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
                if (ImGui::SmallButton("X")) {
                    remove_stock(stock.symbol);
                }
                ImGui::PopStyleColor(3);
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Remove %s", stock.symbol.c_str());
                }

                ImGui::PopID();
            }

            ImGui::EndTable();
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Stock detail — right panel showing selected stock info
// ============================================================================
void WatchlistScreen::render_stock_detail(float width, float height) {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##wl_detail", ImVec2(width, height), ImGuiChildFlags_Borders);

    if (selected_stock_ < 0 || selected_stock_ >= static_cast<int>(stocks_.size())) {
        float cy = height * 0.4f;
        ImGui::SetCursorPosY(cy);
        float tw = ImGui::CalcTextSize("SELECT A STOCK TO VIEW DETAILS").x;
        ImGui::SetCursorPosX((width - tw) * 0.5f);
        ImGui::TextColored(TEXT_DIM, "SELECT A STOCK TO VIEW DETAILS");
        ImGui::EndChild();
        ImGui::PopStyleColor();
        return;
    }

    auto& stock = stocks_[selected_stock_];

    // Header
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARK);
    ImGui::BeginChild("##detail_hdr", ImVec2(0, 24), ImGuiChildFlags_Borders);
    ImGui::TextColored(TEXT_DIM, "STOCK DETAIL");
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Spacing();

    // Symbol hero
    ImGui::TextColored(INFO, "%s", stock.symbol.c_str());

    if (!stock.quote.valid) {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(ERROR_RED.x, ERROR_RED.y, ERROR_RED.z, 0.1f));
        ImGui::BeginChild("##no_data", ImVec2(0, 28), ImGuiChildFlags_Borders);
        ImGui::TextColored(ERROR_RED, " FAILED TO LOAD MARKET DATA");
        ImGui::EndChild();
        ImGui::PopStyleColor();
    } else {
        auto& q = stock.quote;

        // Price + change
        ImGui::SetWindowFontScale(1.5f);
        ImGui::TextColored(TEXT_PRIMARY, "$%.2f", q.price);
        ImGui::SetWindowFontScale(1.0f);

        ImVec4 chg_color = q.change_percent >= 0 ? MARKET_GREEN : MARKET_RED;

        // Change badge
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled(
            p, ImVec2(p.x + 80, p.y + 18),
            ImGui::ColorConvertFloat4ToU32(ImVec4(chg_color.x, chg_color.y, chg_color.z, 0.15f)),
            2.0f);
        char chg_buf[32];
        std::snprintf(chg_buf, sizeof(chg_buf), " %s%.2f%%",
            q.change_percent >= 0 ? "+" : "", q.change_percent);
        ImGui::TextColored(chg_color, "%s", chg_buf);
        ImGui::SameLine(0, 8);
        ImGui::TextColored(chg_color, "($%.2f)", q.change);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Stats rows
        auto stat_row = [&](const char* label, const char* val, ImVec4 color) {
            ImGui::TextColored(TEXT_DIM, "%-12s", label);
            ImGui::SameLine(120);
            ImGui::TextColored(color, "%s", val);
        };

        char buf[32];
        std::snprintf(buf, sizeof(buf), "$%.2f", q.open);
        stat_row("OPEN", buf, TEXT_PRIMARY);
        std::snprintf(buf, sizeof(buf), "$%.2f", q.previous_close);
        stat_row("PREV CLOSE", buf, TEXT_PRIMARY);
        std::snprintf(buf, sizeof(buf), "$%.2f", q.high);
        stat_row("DAY HIGH", buf, MARKET_GREEN);
        std::snprintf(buf, sizeof(buf), "$%.2f", q.low);
        stat_row("DAY LOW", buf, MARKET_RED);
        stat_row("VOLUME", format_volume(q.volume).c_str(), WARNING);

        // Day range visual bar
        if (q.high > q.low && q.high > 0) {
            ImGui::Spacing();
            ImGui::TextColored(TEXT_DIM, "DAY RANGE");

            float bar_w = width - 24;
            ImVec2 bar_pos = ImGui::GetCursorScreenPos();
            float bar_h = 4.0f;

            // Background bar
            ImGui::GetWindowDrawList()->AddRectFilled(
                bar_pos,
                ImVec2(bar_pos.x + bar_w, bar_pos.y + bar_h),
                ImGui::ColorConvertFloat4ToU32(BORDER_DIM), 2.0f);

            // Gradient overlay
            float range = static_cast<float>(q.high - q.low);
            float pos_pct = static_cast<float>((q.price - q.low) / range);
            pos_pct = std::max(0.0f, std::min(1.0f, pos_pct));

            // Current price indicator
            float dot_x = bar_pos.x + pos_pct * bar_w;
            ImGui::GetWindowDrawList()->AddCircleFilled(
                ImVec2(dot_x, bar_pos.y + bar_h * 0.5f), 5.0f,
                ImGui::ColorConvertFloat4ToU32(ACCENT));

            ImGui::Dummy(ImVec2(bar_w, bar_h + 4));

            // Low / High labels
            ImGui::TextColored(MARKET_RED, "$%.2f", q.low);
            ImGui::SameLine(width - 80);
            ImGui::TextColored(MARKET_GREEN, "$%.2f", q.high);
        }
    }

    // Notes section
    if (!stock.notes.empty()) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextColored(WARNING, "NOTES");
        ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARKEST);
        ImGui::BeginChild("##stock_notes", ImVec2(0, 60), ImGuiChildFlags_Borders);
        ImGui::TextWrapped("%s", stock.notes.c_str());
        ImGui::EndChild();
        ImGui::PopStyleColor();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Status bar
// ============================================================================
void WatchlistScreen::render_status_bar() {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARKEST);
    ImGui::BeginChild("##wl_status", ImVec2(0, ImGui::GetFrameHeight()), ImGuiChildFlags_Borders);

    ImGui::TextColored(TEXT_DIM, "FINCEPT WATCHLIST");
    ImGui::SameLine(0, 16);
    ImGui::TextColored(BORDER_BRIGHT, "|");
    ImGui::SameLine(0, 16);

    ImGui::TextColored(TEXT_DIM, "LISTS:");
    ImGui::SameLine(0, 4);
    ImGui::TextColored(INFO, "%d", static_cast<int>(watchlists_.size()));
    ImGui::SameLine(0, 16);
    ImGui::TextColored(TEXT_DIM, "STOCKS:");
    ImGui::SameLine(0, 4);
    ImGui::TextColored(INFO, "%d", static_cast<int>(stocks_.size()));
    ImGui::SameLine(0, 16);
    ImGui::TextColored(TEXT_DIM, "STATUS:");
    ImGui::SameLine(0, 4);
    ImGui::TextColored(data_.is_loading() ? WARNING : MARKET_GREEN,
        "%s", data_.is_loading() ? "LOADING" : "CONNECTED");

    // Status message on right
    if (!status_.empty()) {
        double elapsed = ImGui::GetTime() - status_time_;
        if (elapsed < 5.0) {
            float msg_w = ImGui::CalcTextSize(status_.c_str()).x + 16;
            ImGui::SameLine(ImGui::GetWindowWidth() - msg_w);
            ImGui::TextColored(SUCCESS, "%s", status_.c_str());
        } else {
            status_.clear();
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Create watchlist modal
// ============================================================================
void WatchlistScreen::render_create_modal() {
    using namespace theme::colors;

    if (!show_create_modal_) return;

    // Center the modal
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(420, 320));

    ImGui::PushStyleColor(ImGuiCol_WindowBg, BG_PANEL);
    ImGui::PushStyleColor(ImGuiCol_TitleBg, BG_DARK);
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, BG_DARK);

    if (ImGui::Begin("CREATE NEW WATCHLIST##modal", &show_create_modal_,
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoDocking)) {

        // Name
        ImGui::TextColored(TEXT_DIM, "WATCHLIST NAME *");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##wl_name", "My Watchlist",
            create_name_, sizeof(create_name_));

        ImGui::Spacing();

        // Description
        ImGui::TextColored(TEXT_DIM, "DESCRIPTION");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextMultiline("##wl_desc", create_desc_, sizeof(create_desc_),
            ImVec2(-1, 60));

        ImGui::Spacing();

        // Color picker
        ImGui::TextColored(TEXT_DIM, "COLOR");
        const auto& colors = watchlist_colors();
        for (int i = 0; i < static_cast<int>(colors.size()); i++) {
            ImVec4 c = hex_to_color(colors[i]);
            bool selected = (i == create_color_idx_);

            ImGui::PushID(i);
            ImGui::PushStyleColor(ImGuiCol_Button, c);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                ImVec4(c.x * 0.8f, c.y * 0.8f, c.z * 0.8f, 1.0f));

            float size = selected ? 32.0f : 28.0f;
            if (ImGui::Button("##color", ImVec2(size, size))) {
                create_color_idx_ = i;
            }
            ImGui::PopStyleColor(2);

            if (selected) {
                ImVec2 mn = ImGui::GetItemRectMin();
                ImVec2 mx = ImGui::GetItemRectMax();
                ImGui::GetWindowDrawList()->AddRect(mn, mx,
                    ImGui::ColorConvertFloat4ToU32(TEXT_PRIMARY), 0, 0, 2.0f);
            }

            ImGui::PopID();
            if (i < static_cast<int>(colors.size()) - 1) ImGui::SameLine(0, 6);
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Buttons
        float btn_w = 100;
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - btn_w * 2 - 24);

        if (theme::SecondaryButton("Cancel")) {
            show_create_modal_ = false;
        }
        ImGui::SameLine();
        bool can_create = create_name_[0] != '\0';
        if (!can_create) ImGui::BeginDisabled();
        if (theme::AccentButton("Create")) {
            create_watchlist();
        }
        if (!can_create) ImGui::EndDisabled();
    }
    ImGui::End();
    ImGui::PopStyleColor(3);
}

// ============================================================================
// Add stock modal
// ============================================================================
void WatchlistScreen::render_add_stock_modal() {
    using namespace theme::colors;

    if (!show_add_stock_) return;

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(400, 260));

    ImGui::PushStyleColor(ImGuiCol_WindowBg, BG_PANEL);
    ImGui::PushStyleColor(ImGuiCol_TitleBg, BG_DARK);
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, BG_DARK);

    if (ImGui::Begin("ADD STOCK TO WATCHLIST##add_modal", &show_add_stock_,
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoDocking)) {

        // Symbol
        ImGui::TextColored(TEXT_DIM, "SYMBOL *");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputTextWithHint("##add_sym", "AAPL",
                add_symbol_, sizeof(add_symbol_),
                ImGuiInputTextFlags_CharsUppercase)) {
        }
        ImGui::TextColored(TEXT_DIM, "Enter stock ticker (e.g., AAPL, MSFT, GOOGL)");

        ImGui::Spacing();

        // Notes
        ImGui::TextColored(TEXT_DIM, "NOTES (OPTIONAL)");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextMultiline("##add_notes", add_notes_, sizeof(add_notes_),
            ImVec2(-1, 60));

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Buttons
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 220);
        if (theme::SecondaryButton("Cancel")) {
            show_add_stock_ = false;
        }
        ImGui::SameLine();

        bool can_add = add_symbol_[0] != '\0';
        if (!can_add) ImGui::BeginDisabled();
        ImGui::PushStyleColor(ImGuiCol_Button, MARKET_GREEN);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
            ImVec4(MARKET_GREEN.x * 0.8f, MARKET_GREEN.y * 0.8f, MARKET_GREEN.z * 0.8f, 1));
        ImGui::PushStyleColor(ImGuiCol_Text, BG_DARKEST);
        if (ImGui::Button("Add Stock")) {
            add_stock();
        }
        ImGui::PopStyleColor(3);
        if (!can_add) ImGui::EndDisabled();
    }
    ImGui::End();
    ImGui::PopStyleColor(3);
}

// ============================================================================
// Main render
// ============================================================================
void WatchlistScreen::render() {
    init();

    using namespace theme::colors;

    // Auto-refresh timer
    float dt = ImGui::GetIO().DeltaTime;
    refresh_timer_ += dt;
    if (refresh_timer_ >= REFRESH_INTERVAL && !data_.is_loading()) {
        refresh_quotes();
    }

    ui::ScreenFrame frame("##watchlist", ImVec2(4, 4), BG_DARKEST);
    if (!frame.begin()) { frame.end(); return; }

    // Header bar
    render_header_bar();

    // Info bar
    render_info_bar();

    // Three-panel layout
    float sidebar_w = 240.0f;
    float detail_w = 280.0f;
    float status_h = ImGui::GetFrameHeight() + 4;
    float header_h = 36 + 22 + 8; // header + info + spacing
    float content_h = frame.height() - header_h - status_h;
    float stock_list_w = frame.width() - sidebar_w - detail_w - 8;
    if (stock_list_w < 200) stock_list_w = 200;

    render_sidebar(sidebar_w, content_h);
    ImGui::SameLine(0, 4);
    render_stock_list(stock_list_w, content_h);
    ImGui::SameLine(0, 4);
    render_stock_detail(detail_w, content_h);

    // Status bar
    render_status_bar();

    frame.end();

    // Modals (rendered outside frame)
    render_create_modal();
    render_add_stock_modal();
}

} // namespace fincept::watchlist
