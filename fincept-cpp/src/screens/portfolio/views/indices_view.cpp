// Custom Indices View — Port of CustomIndexView.tsx
// Create, manage, and track custom stock indices

#include "indices_view.h"
#include "storage/database.h"
#include "python/python_runner.h"
#include "ui/theme.h"
#include "core/logger.h"
#include <imgui.h>
#include <nlohmann/json.hpp>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <algorithm>

namespace fincept::portfolio {

static const char* METHOD_LABELS[] = {
    "Equal Weighted", "Price Weighted", "Market Cap Weighted"
};
static const char* METHOD_KEYS[] = {
    "equal_weighted", "price_weighted", "market_cap_weighted"
};
static constexpr int NUM_METHODS = 3;

// ============================================================================
// Database setup
// ============================================================================

void IndicesView::ensure_tables() {
    auto& dbi = db::Database::instance();
    dbi.exec(R"(
        CREATE TABLE IF NOT EXISTS custom_indices (
            id TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            description TEXT DEFAULT '',
            method TEXT DEFAULT 'equal_weighted',
            base_value REAL DEFAULT 100.0,
            current_value REAL DEFAULT 100.0,
            prev_close REAL DEFAULT 100.0,
            currency TEXT DEFAULT 'USD',
            is_active INTEGER DEFAULT 1,
            created_at TEXT DEFAULT (datetime('now'))
        )
    )");
    dbi.exec(R"(
        CREATE TABLE IF NOT EXISTS index_constituents (
            id TEXT PRIMARY KEY,
            index_id TEXT NOT NULL,
            symbol TEXT NOT NULL,
            weight REAL DEFAULT 0.0,
            current_price REAL DEFAULT 0.0,
            prev_close REAL DEFAULT 0.0,
            FOREIGN KEY (index_id) REFERENCES custom_indices(id)
        )
    )");
}

// ============================================================================
// Main render
// ============================================================================

void IndicesView::render(const PortfolioSummary& /*summary*/) {
    if (needs_refresh_) {
        ensure_tables();
        refresh_data();
    }

    // Header with create button
    ImGui::TextColored(theme::colors::ACCENT, "Custom Indices");
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 120);
    if (theme::AccentButton("+ New Index")) {
        show_create_ = true;
        std::memset(new_name_, 0, sizeof(new_name_));
        std::memset(new_desc_, 0, sizeof(new_desc_));
        new_method_idx_ = 0;
        std::strncpy(new_base_, "100", sizeof(new_base_) - 1);
    }
    ImGui::Separator();

    if (indices_.empty()) {
        ImGui::TextColored(theme::colors::TEXT_DIM, "No custom indices. Click '+ New Index' to create one.");
    } else {
        // Two-panel layout
        float left_w = 250;
        ImGui::BeginChild("##idx_list", ImVec2(left_w, 0), ImGuiChildFlags_Borders);
        render_index_list();
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("##idx_detail", ImVec2(0, 0), ImGuiChildFlags_Borders);
        if (selected_idx_ >= 0 && selected_idx_ < static_cast<int>(indices_.size())) {
            render_index_detail();
        } else {
            ImGui::TextColored(theme::colors::TEXT_DIM, "Select an index from the list");
        }
        ImGui::EndChild();
    }

    render_create_modal();
    render_add_constituent_modal();
}

// ============================================================================
// Data operations
// ============================================================================

void IndicesView::refresh_data() {
    indices_.clear();
    auto& dbi = db::Database::instance();

    auto stmt = dbi.prepare("SELECT id, name, description, method, base_value, current_value, prev_close, currency, is_active, created_at FROM custom_indices ORDER BY created_at DESC");
    while (stmt.step()) {
        CustomIndex idx;
        idx.id = stmt.col_text(0);
        idx.name = stmt.col_text(1);
        idx.description = stmt.col_text(2);
        idx.method = stmt.col_text(3);
        idx.base_value = stmt.col_double(4);
        idx.current_value = stmt.col_double(5);
        idx.prev_close = stmt.col_double(6);
        idx.currency = stmt.col_text(7);
        idx.is_active = stmt.col_int(8) != 0;
        idx.created_at = stmt.col_text(9);
        indices_.push_back(idx);
    }

    // Load constituents for selected index
    if (selected_idx_ >= 0 && selected_idx_ < static_cast<int>(indices_.size())) {
        constituents_.clear();
        auto cstmt = dbi.prepare("SELECT id, index_id, symbol, weight, current_price, prev_close FROM index_constituents WHERE index_id = ?");
        cstmt.bind_text(1, indices_[selected_idx_].id);
        while (cstmt.step()) {
            IndexConstituent c;
            c.id = cstmt.col_text(0);
            c.index_id = cstmt.col_text(1);
            c.symbol = cstmt.col_text(2);
            c.weight = cstmt.col_double(3);
            c.current_price = cstmt.col_double(4);
            c.prev_close = cstmt.col_double(5);
            constituents_.push_back(c);
        }
    }

    needs_refresh_ = false;
}

void IndicesView::create_index() {
    auto& dbi = db::Database::instance();
    std::string id = db::generate_uuid();
    double base = std::atof(new_base_);
    if (base <= 0) base = 100.0;

    auto stmt = dbi.prepare("INSERT INTO custom_indices (id, name, description, method, base_value, current_value, prev_close) VALUES (?, ?, ?, ?, ?, ?, ?)");
    stmt.bind_text(1, id);
    stmt.bind_text(2, std::string(new_name_));
    stmt.bind_text(3, std::string(new_desc_));
    stmt.bind_text(4, std::string(METHOD_KEYS[new_method_idx_]));
    stmt.bind_double(5, base);
    stmt.bind_double(6, base);
    stmt.bind_double(7, base);
    stmt.execute();

    needs_refresh_ = true;
}

void IndicesView::delete_index(const std::string& id) {
    auto& dbi = db::Database::instance();
    auto stmt1 = dbi.prepare("DELETE FROM index_constituents WHERE index_id = ?");
    stmt1.bind_text(1, id);
    stmt1.execute();

    auto stmt2 = dbi.prepare("DELETE FROM custom_indices WHERE id = ?");
    stmt2.bind_text(1, id);
    stmt2.execute();

    selected_idx_ = -1;
    needs_refresh_ = true;
}

void IndicesView::add_constituent(const std::string& index_id) {
    auto& dbi = db::Database::instance();
    std::string id = db::generate_uuid();
    double weight = std::atof(add_weight_);

    auto stmt = dbi.prepare("INSERT INTO index_constituents (id, index_id, symbol, weight) VALUES (?, ?, ?, ?)");
    stmt.bind_text(1, id);
    stmt.bind_text(2, index_id);
    stmt.bind_text(3, std::string(add_symbol_));
    stmt.bind_double(4, weight);
    stmt.execute();

    needs_refresh_ = true;
}

void IndicesView::remove_constituent(const std::string& id) {
    auto& dbi = db::Database::instance();
    auto stmt = dbi.prepare("DELETE FROM index_constituents WHERE id = ?");
    stmt.bind_text(1, id);
    stmt.execute();

    needs_refresh_ = true;
}

void IndicesView::recalculate_index(const std::string& index_id) {
    if (constituents_.empty()) return;

    // Fetch current prices via Python
    try {
        nlohmann::json req;
        std::vector<std::string> symbols;
        for (const auto& c : constituents_)
            symbols.push_back(c.symbol);
        req["symbols"] = symbols;

        auto result = python::execute_with_stdin(
            "Analytics/portfolioManagement/fetch_quotes.py", {}, req.dump());

        if (result.success && !result.output.empty()) {
            auto j = nlohmann::json::parse(result.output, nullptr, false);
            if (!j.is_discarded() && j.is_object()) {
                auto& dbi = db::Database::instance();
                for (auto& c : constituents_) {
                    if (j.contains(c.symbol) && j[c.symbol].is_object()) {
                        double price = j[c.symbol].value("price", c.current_price);
                        double prev = j[c.symbol].value("prev_close", c.prev_close);
                        auto ustmt = dbi.prepare("UPDATE index_constituents SET current_price = ?, prev_close = ? WHERE id = ?");
                        ustmt.bind_double(1, price);
                        ustmt.bind_double(2, prev);
                        ustmt.bind_text(3, c.id);
                        ustmt.execute();
                        c.current_price = price;
                        c.prev_close = prev;
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR("IndicesView", "Price fetch failed: %s", e.what());
    }

    // Calculate index value based on method
    double index_value = 0;
    auto it = std::find_if(indices_.begin(), indices_.end(),
        [&](const CustomIndex& idx) { return idx.id == index_id; });
    if (it == indices_.end()) return;

    if (it->method == "equal_weighted") {
        double sum = 0;
        int count = 0;
        for (const auto& c : constituents_) {
            if (c.prev_close > 0) {
                sum += c.current_price / c.prev_close;
                count++;
            }
        }
        index_value = count > 0 ? it->prev_close * (sum / count) : it->base_value;
    } else if (it->method == "price_weighted") {
        double sum = 0;
        for (const auto& c : constituents_)
            sum += c.current_price;
        double divisor = static_cast<double>(constituents_.size());
        index_value = divisor > 0 ? sum / divisor * (it->base_value / 100.0) : it->base_value;
    } else {
        // Market cap weighted
        double total_weight = 0;
        for (const auto& c : constituents_)
            total_weight += c.weight;
        if (total_weight <= 0) total_weight = static_cast<double>(constituents_.size());

        double weighted_return = 0;
        for (const auto& c : constituents_) {
            double w = total_weight > 0 ? c.weight / total_weight : 1.0 / static_cast<double>(constituents_.size());
            if (c.prev_close > 0)
                weighted_return += w * (c.current_price / c.prev_close);
        }
        index_value = it->prev_close * weighted_return;
    }

    // Update in database
    auto& dbi = db::Database::instance();
    auto ustmt = dbi.prepare("UPDATE custom_indices SET current_value = ? WHERE id = ?");
    ustmt.bind_double(1, index_value);
    ustmt.bind_text(2, index_id);
    ustmt.execute();

    it->current_value = index_value;
}

// ============================================================================
// UI Rendering
// ============================================================================

void IndicesView::render_index_list() {
    theme::SectionHeader("Indices");
    for (int i = 0; i < static_cast<int>(indices_.size()); i++) {
        const auto& idx = indices_[i];
        bool selected = (selected_idx_ == i);

        if (selected) ImGui::PushStyleColor(ImGuiCol_Header, theme::colors::ACCENT_BG);

        double change_pct = idx.prev_close > 0
            ? ((idx.current_value - idx.prev_close) / idx.prev_close) * 100.0 : 0.0;

        char label[256];
        std::snprintf(label, sizeof(label), "%s\n  %.2f  %+.2f%%", idx.name.c_str(), idx.current_value, change_pct);

        if (ImGui::Selectable(label, selected, ImGuiSelectableFlags_None, ImVec2(0, 36))) {
            selected_idx_ = i;
            needs_refresh_ = true;
        }

        if (selected) ImGui::PopStyleColor();
    }
}

void IndicesView::render_index_detail() {
    const auto& idx = indices_[selected_idx_];
    double change = idx.current_value - idx.prev_close;
    double change_pct = idx.prev_close > 0 ? (change / idx.prev_close) * 100.0 : 0.0;
    ImVec4 col = change >= 0 ? theme::colors::MARKET_GREEN : theme::colors::MARKET_RED;

    ImGui::TextColored(theme::colors::ACCENT, "%s", idx.name.c_str());
    ImGui::SameLine();
    ImGui::TextColored(theme::colors::TEXT_DIM, "[%s]", idx.method.c_str());

    ImGui::TextColored(col, "%.2f  %+.2f (%+.2f%%)", idx.current_value, change, change_pct);

    if (!idx.description.empty())
        ImGui::TextColored(theme::colors::TEXT_DIM, "%s", idx.description.c_str());

    ImGui::Spacing();
    if (theme::AccentButton("Refresh Prices")) {
        recalculate_index(idx.id);
    }
    ImGui::SameLine();
    if (theme::SecondaryButton("Add Symbol")) {
        show_add_ = true;
        std::memset(add_symbol_, 0, sizeof(add_symbol_));
        std::memset(add_weight_, 0, sizeof(add_weight_));
    }
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0, 0, 1));
    if (ImGui::Button("Delete Index")) {
        delete_index(idx.id);
    }
    ImGui::PopStyleColor();

    ImGui::Spacing();
    theme::SectionHeader("Constituents");

    if (constituents_.empty()) {
        ImGui::TextColored(theme::colors::TEXT_DIM, "No constituents. Click 'Add Symbol' to add.");
    } else if (ImGui::BeginTable("##constituents", 6,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("Symbol", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("Price", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("Prev Close", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("Change", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("Weight", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 30);
        ImGui::TableHeadersRow();

        for (const auto& c : constituents_) {
            double chg = c.current_price - c.prev_close;
            double chg_pct = c.prev_close > 0 ? (chg / c.prev_close) * 100.0 : 0.0;
            ImVec4 cc = chg >= 0 ? theme::colors::MARKET_GREEN : theme::colors::MARKET_RED;

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextColored(theme::colors::ACCENT, "%s", c.symbol.c_str());
            ImGui::TableNextColumn(); ImGui::Text("$%.2f", c.current_price);
            ImGui::TableNextColumn(); ImGui::Text("$%.2f", c.prev_close);
            ImGui::TableNextColumn(); ImGui::TextColored(cc, "%+.2f%%", chg_pct);
            ImGui::TableNextColumn(); ImGui::Text("%.1f", c.weight);
            ImGui::TableNextColumn();
            ImGui::PushID(c.id.c_str());
            if (ImGui::SmallButton("X")) {
                remove_constituent(c.id);
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
}

void IndicesView::render_create_modal() {
    if (!show_create_) return;
    ImGui::OpenPopup("Create Custom Index");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(400, 280));

    if (ImGui::BeginPopupModal("Create Custom Index", &show_create_)) {
        ImGui::Text("Index Name"); ImGui::SameLine(120);
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##idx_name", new_name_, sizeof(new_name_));

        ImGui::Text("Description"); ImGui::SameLine(120);
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##idx_desc", new_desc_, sizeof(new_desc_));

        ImGui::Text("Method"); ImGui::SameLine(120);
        ImGui::SetNextItemWidth(-1);
        ImGui::Combo("##idx_method", &new_method_idx_, METHOD_LABELS, NUM_METHODS);

        ImGui::Text("Base Value"); ImGui::SameLine(120);
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##idx_base", new_base_, sizeof(new_base_));

        ImGui::Spacing();
        if (theme::AccentButton("Create") && std::strlen(new_name_) > 0) {
            create_index();
            show_create_ = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            show_create_ = false;
        }
        ImGui::EndPopup();
    }
}

void IndicesView::render_add_constituent_modal() {
    if (!show_add_) return;
    if (selected_idx_ < 0 || selected_idx_ >= static_cast<int>(indices_.size())) {
        show_add_ = false;
        return;
    }

    ImGui::OpenPopup("Add Constituent");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(300, 160));

    if (ImGui::BeginPopupModal("Add Constituent", &show_add_)) {
        ImGui::Text("Symbol"); ImGui::SameLine(80);
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##csym", add_symbol_, sizeof(add_symbol_));

        ImGui::Text("Weight"); ImGui::SameLine(80);
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##cwgt", add_weight_, sizeof(add_weight_));

        ImGui::Spacing();
        if (theme::AccentButton("Add") && std::strlen(add_symbol_) > 0) {
            add_constituent(indices_[selected_idx_].id);
            show_add_ = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            show_add_ = false;
        }
        ImGui::EndPopup();
    }
}

} // namespace fincept::portfolio
