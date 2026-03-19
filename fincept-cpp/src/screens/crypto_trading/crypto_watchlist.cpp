#include "crypto_trading_screen.h"
#include "crypto_internal.h"
#include "ui/theme.h"
#include "trading/exchange_service.h"
#include "core/logger.h"
#include <imgui.h>
#include <algorithm>

namespace fincept::crypto {

using namespace theme::colors;

void CryptoTradingScreen::render_watchlist(float w, float h) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##crypto_watchlist", ImVec2(w, h), true);

    // Header
    ImGui::TextColored(ACCENT, "WATCHLIST");
    ImGui::SameLine();

    int loaded = 0;
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        for (auto& w : watchlist_) if (w.has_data) loaded++;
    }
    ImGui::TextColored(TEXT_DIM, "(%d/%d)", loaded, (int)watchlist_.size());

    if (wl_fetching_) {
        ImGui::SameLine();
        ImGui::TextColored(WARNING, "[*]");
    }

    // Search / Filter field
    ImGui::PushItemWidth(-1);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);
    bool search_changed = ImGui::InputTextWithHint("##wl_filter", "Search symbol...",
                                                    watchlist_filter_, sizeof(watchlist_filter_));
    ImGui::PopStyleColor();
    ImGui::PopItemWidth();

    std::string filter_str(watchlist_filter_);
    for (auto& c : filter_str) c = (char)std::toupper((unsigned char)c);

    // Fetch exchange markets for search (lazy, cached per exchange)
    if (search_changed && filter_str.length() >= 2 && !search_fetching_) {
        // Load markets if not cached for this exchange
        if (search_cache_exchange_ != exchange_id_ || !search_markets_loaded_) {
            search_fetching_ = true;
            std::string ex_id = exchange_id_;
            launch_async(std::thread([this, ex_id]() {
                try {
                    auto markets = trading::ExchangeService::instance().fetch_markets("spot");
                    LOG_INFO(TAG, "Loaded %d markets for search from %s",
                             (int)markets.size(), ex_id.c_str());
                    std::lock_guard<std::mutex> lock(data_mutex_);
                    all_markets_ = std::move(markets);
                    search_cache_exchange_ = ex_id;
                    search_markets_loaded_ = true;
                } catch (const std::exception& e) {
                    LOG_ERROR(TAG, "Failed to load markets for search: %s", e.what());
                }
                search_fetching_ = false;
            }));
        }
    }

    // Build search results from cached markets
    bool has_search = filter_str.length() >= 2 && search_markets_loaded_;
    std::vector<trading::MarketInfo> filtered_markets;
    if (has_search) {
        std::lock_guard<std::mutex> lock(data_mutex_);
        int count = 0;
        for (const auto& m : all_markets_) {
            if (!m.active) continue;
            std::string sym_upper = m.symbol;
            for (auto& c : sym_upper) c = (char)std::toupper((unsigned char)c);
            if (sym_upper.find(filter_str) != std::string::npos) {
                // Skip if already in watchlist
                bool in_wl = false;
                for (const auto& w : watchlist_) {
                    if (w.symbol == m.symbol) { in_wl = true; break; }
                }
                if (!in_wl) {
                    filtered_markets.push_back(m);
                    if (++count >= 20) break; // limit results
                }
            }
        }
    }

    ImGui::Separator();

    // Scrollable list
    ImGui::BeginChild("##wl_scroll", ImVec2(0, 0), false);

    char buf[64];
    std::lock_guard<std::mutex> lock(data_mutex_);

    // --- Watchlist entries ---
    for (int i = 0; i < (int)watchlist_.size(); i++) {
        auto& entry = watchlist_[i];

        // Filter check
        if (!filter_str.empty()) {
            std::string sym_upper = entry.symbol;
            for (auto& c : sym_upper) c = (char)std::toupper((unsigned char)c);
            if (sym_upper.find(filter_str) == std::string::npos) continue;
        }

        bool is_selected = (entry.symbol == selected_symbol_);
        ImGui::PushID(i);

        ImGui::PushStyleColor(ImGuiCol_Header, BG_WIDGET);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, BG_HOVER);

        if (ImGui::Selectable("##wl_sel", is_selected, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, 22))) {
            watchlist_selected_ = i;
            switch_symbol(entry.symbol);
        }

        ImGui::PopStyleColor(2);

        // Draw content over the selectable
        ImGui::SameLine(4);

        // Symbol name (truncate base only)
        std::string base = entry.symbol;
        auto slash = base.find('/');
        if (slash != std::string::npos) base = base.substr(0, slash);
        ImGui::TextColored(TEXT_PRIMARY, "%s", base.c_str());

        if (entry.has_data) {
            // Price on right
            if (entry.price > 1.0)
                std::snprintf(buf, sizeof(buf), "%.2f", entry.price);
            else if (entry.price > 0.01)
                std::snprintf(buf, sizeof(buf), "%.4f", entry.price);
            else
                std::snprintf(buf, sizeof(buf), "%.6f", entry.price);

            ImVec4 col = entry.change_pct >= 0 ? MARKET_GREEN : MARKET_RED;

            // Change % next to symbol
            ImGui::SameLine(0, 4);
            char pct_buf[16];
            std::snprintf(pct_buf, sizeof(pct_buf), "%.1f%%", entry.change_pct);
            ImGui::TextColored(col, "%s", pct_buf);

            // Price on far right
            float price_w = ImGui::CalcTextSize(buf).x;
            float avail = ImGui::GetContentRegionAvail().x;
            if (avail > price_w + 4) {
                ImGui::SameLine(w - price_w - 16);
                ImGui::TextColored(col, "%s", buf);
            }
        } else {
            float avail = ImGui::GetContentRegionAvail().x;
            if (avail > 20) {
                ImGui::SameLine(w - 30);
                ImGui::TextColored(TEXT_DIM, "...");
            }
        }

        ImGui::PopID();
    }

    // --- Exchange search results (symbols not in watchlist) ---
    if (!filtered_markets.empty()) {
        ImGui::Separator();
        ImGui::TextColored(TEXT_DIM, "Exchange results:");
        ImGui::Spacing();

        for (int i = 0; i < (int)filtered_markets.size(); i++) {
            const auto& m = filtered_markets[i];
            ImGui::PushID(10000 + i);

            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.1f, 0.15f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, BG_HOVER);

            if (ImGui::Selectable("##search_sel", false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, 22))) {
                // Add to watchlist
                WatchlistEntry new_entry;
                new_entry.symbol = m.symbol;
                watchlist_.push_back(new_entry);
                watchlist_selected_ = (int)watchlist_.size() - 1;
                LOG_INFO(TAG, "Added %s to watchlist from search", m.symbol.c_str());

                // Clear search field
                watchlist_filter_[0] = '\0';

                // Defer symbol switch — we hold data_mutex_ here,
                // switch_symbol() will run in the render loop next frame
                pending_symbol_switch_ = m.symbol;
            }

            ImGui::PopStyleColor(2);

            ImGui::SameLine(4);

            // Show base/quote
            std::string base = m.base;
            ImGui::TextColored(ACCENT, "%s", base.c_str());
            ImGui::SameLine(0, 2);
            ImGui::TextColored(TEXT_DIM, "/%s", m.quote.c_str());

            // Type on right
            float type_w = ImGui::CalcTextSize(m.type.c_str()).x;
            float avail = ImGui::GetContentRegionAvail().x;
            if (avail > type_w + 4) {
                ImGui::SameLine(w - type_w - 16);
                ImGui::TextColored(TEXT_DIM, "%s", m.type.c_str());
            }

            ImGui::PopID();
        }
    } else if (filter_str.length() >= 2 && search_fetching_) {
        ImGui::Separator();
        ImGui::TextColored(TEXT_DIM, "Searching...");
    } else if (filter_str.length() >= 2 && search_markets_loaded_ && filtered_markets.empty()) {
        // Check if all matches are in watchlist already
        bool any_watchlist_match = false;
        for (const auto& w : watchlist_) {
            std::string sym_upper = w.symbol;
            for (auto& c : sym_upper) c = (char)std::toupper((unsigned char)c);
            if (sym_upper.find(filter_str) != std::string::npos) {
                any_watchlist_match = true;
                break;
            }
        }
        if (!any_watchlist_match) {
            ImGui::Separator();
            ImGui::TextColored(TEXT_DIM, "No results");
        }
    }

    ImGui::EndChild();
    ImGui::EndChild();
    ImGui::PopStyleColor();
}


} // namespace fincept::crypto
