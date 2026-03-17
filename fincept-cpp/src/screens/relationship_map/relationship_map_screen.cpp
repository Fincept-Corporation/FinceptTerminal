// Relationship Map — Multi-layer corporate intelligence graph visualization
// Port of RelationshipMapTab.tsx — radial node graph drawn on ImGui canvas
// Data from SEC EDGAR + yfinance via Python bridge

#include "relationship_map_screen.h"
#include "ui/yoga_helpers.h"
#include "ui/theme.h"
#include "python/python_runner.h"
#include "core/logger.h"
#include <imgui.h>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <chrono>

namespace fincept::relationship_map {

using namespace theme::colors;

// ============================================================================
// Constants
// ============================================================================

static constexpr float NODE_RADIUS    = 28.0f;
static constexpr float COMPANY_RADIUS = 40.0f;
static constexpr float PI = 3.14159265358979323846f;

// Ring radii for radial layout
static constexpr float RING_INSIDERS       = 160.0f;
static constexpr float RING_INSTITUTIONAL  = 280.0f;
static constexpr float RING_FUND_FAMILIES  = 380.0f;
static constexpr float RING_PEERS          = 460.0f;
static constexpr float RING_EVENTS         = 340.0f;
static constexpr float RING_SUPPLY_CHAIN   = 420.0f;
static constexpr float RING_SEGMENTS       = 200.0f;
static constexpr float RING_DEBT_HOLDERS   = 500.0f;

// ============================================================================
// Helpers
// ============================================================================

ImVec4 RelationshipMapScreen::category_color(NodeCategory cat) const {
    switch (cat) {
        case NodeCategory::Company:       return ACCENT;
        case NodeCategory::Peer:          return INFO;
        case NodeCategory::Institutional: return SUCCESS;
        case NodeCategory::FundFamily:    return ImVec4(0.62f, 0.31f, 0.87f, 1.0f);
        case NodeCategory::Insider:       return ImVec4(0.0f, 0.90f, 1.0f, 1.0f);
        case NodeCategory::Event:         return ERROR_RED;
        case NodeCategory::SupplyChain:   return WARNING;
        case NodeCategory::Segment:       return ACCENT_DIM;
        case NodeCategory::DebtHolder:    return ImVec4(1.0f, 0.42f, 0.42f, 1.0f);
        case NodeCategory::Metrics:       return TEXT_DIM;
    }
    return TEXT_DIM;
}

const char* RelationshipMapScreen::category_name(NodeCategory cat) const {
    switch (cat) {
        case NodeCategory::Company:       return "Company";
        case NodeCategory::Peer:          return "Peer";
        case NodeCategory::Institutional: return "Institutional";
        case NodeCategory::FundFamily:    return "Fund Family";
        case NodeCategory::Insider:       return "Insider";
        case NodeCategory::Event:         return "Event";
        case NodeCategory::SupplyChain:   return "Supply Chain";
        case NodeCategory::Segment:       return "Segment";
        case NodeCategory::DebtHolder:    return "Debt Holder";
        case NodeCategory::Metrics:       return "Metrics";
    }
    return "Unknown";
}

const GraphNode* RelationshipMapScreen::find_node(const std::string& id) const {
    for (auto& n : nodes_) {
        if (n.id == id) return &n;
    }
    return nullptr;
}

bool RelationshipMapScreen::should_show_node(const GraphNode& node) const {
    switch (node.category) {
        case NodeCategory::Company:       return true;
        case NodeCategory::Institutional: return filters_.show_institutional;
        case NodeCategory::FundFamily:    return filters_.show_fund_families;
        case NodeCategory::Insider:       return filters_.show_insiders;
        case NodeCategory::Peer:          return filters_.show_peers;
        case NodeCategory::Event:         return filters_.show_events;
        case NodeCategory::SupplyChain:   return filters_.show_supply_chain;
        case NodeCategory::Segment:       return filters_.show_segments;
        case NodeCategory::DebtHolder:    return filters_.show_debt_holders;
        case NodeCategory::Metrics:       return true;
    }
    return true;
}

// ============================================================================
// Init
// ============================================================================

void RelationshipMapScreen::init() {
    // Nothing needed — user triggers load via ticker input
}

// ============================================================================
// Async
// ============================================================================

bool RelationshipMapScreen::is_async_busy() const {
    return async_task_.valid() &&
           async_task_.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready;
}

void RelationshipMapScreen::check_async() {
    if (async_task_.valid() &&
        async_task_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        try { async_task_.get(); } catch (...) {}
    }
}

// ============================================================================
// Data fetching — calls Python edgar/yfinance scripts
// ============================================================================

void RelationshipMapScreen::load_relationship_data(const std::string& ticker) {
    if (is_async_busy()) return;
    status_ = AsyncStatus::Loading;
    loading_ = {"company", 10, "Fetching company data for " + ticker + "..."};
    error_.clear();
    nodes_.clear();
    edges_.clear();
    selected_node_id_.clear();
    company_name_.clear();

    std::string t = ticker;
    async_task_ = std::async(std::launch::async, [this, t]() {
        // Use the relationship_map.py script which aggregates all sources
        auto result = python::execute("relationship_map.py", {t});
        if (!result.success || result.output.empty()) {
            // Fallback: try yfinance_data.py for basic info
            auto yf_result = python::execute("yfinance_data.py", {"info", t});
            if (!yf_result.success || yf_result.output.empty()) {
                status_ = AsyncStatus::Error;
                error_ = result.error.empty() ? "Failed to load data for " + t : result.error;
                loading_ = {"error", 0, error_};
                return;
            }
            try {
                auto j = nlohmann::json::parse(yf_result.output);
                build_graph(j);
            } catch (const std::exception& ex) {
                status_ = AsyncStatus::Error;
                error_ = std::string("Parse error: ") + ex.what();
                loading_ = {"error", 0, error_};
            }
            return;
        }
        try {
            auto j = nlohmann::json::parse(result.output);
            loading_ = {"ownership", 50, "Building graph..."};
            build_graph(j);
        } catch (const std::exception& ex) {
            status_ = AsyncStatus::Error;
            error_ = std::string("Parse error: ") + ex.what();
            loading_ = {"error", 0, error_};
        }
    });
}

// ============================================================================
// Graph building from JSON response
// ============================================================================

void RelationshipMapScreen::build_graph(const nlohmann::json& data) {
    nlohmann::json payload = data;
    if (data.contains("data")) payload = data["data"];

    // Company node
    if (payload.contains("company")) {
        add_company_node(payload["company"]);
    } else {
        // Minimal company from yfinance response
        nlohmann::json comp;
        comp["ticker"] = loaded_ticker_;
        comp["name"] = payload.value("company_name", loaded_ticker_);
        comp["sector"] = payload.value("sector", "N/A");
        comp["industry"] = payload.value("industry", "N/A");
        comp["market_cap"] = payload.value("market_cap", 0);
        comp["current_price"] = payload.value("current_price", 0.0);
        add_company_node(comp);
    }

    // Relationship layers
    if (payload.contains("institutional_holders"))
        add_institutional_nodes(payload["institutional_holders"]);
    if (payload.contains("fund_families"))
        add_fund_family_nodes(payload["fund_families"]);
    if (payload.contains("insider_holders"))
        add_insider_nodes(payload["insider_holders"]);
    if (payload.contains("peers"))
        add_peer_nodes(payload["peers"]);
    if (payload.contains("corporate_events"))
        add_event_nodes(payload["corporate_events"]);
    if (payload.contains("supply_chain"))
        add_supply_chain_nodes(payload["supply_chain"]);
    if (payload.contains("segments"))
        add_segment_nodes(payload["segments"]);
    if (payload.contains("debt_holders"))
        add_debt_holder_nodes(payload["debt_holders"]);

    layout_radial();
    status_ = AsyncStatus::Success;
    loading_ = {"complete", 100, "Complete"};
}

// ============================================================================
// Node builders
// ============================================================================

void RelationshipMapScreen::add_company_node(const nlohmann::json& company) {
    GraphNode n;
    n.id = "company";
    n.label = company.value("name", company.value("ticker", "Company"));
    n.subtitle = company.value("sector", "") + " | " + company.value("industry", "");
    n.category = NodeCategory::Company;
    n.color = category_color(NodeCategory::Company);
    n.data = company;
    company_name_ = n.label;
    nodes_.push_back(std::move(n));
}

void RelationshipMapScreen::add_institutional_nodes(const nlohmann::json& holders) {
    if (!holders.is_array()) return;
    int count = 0;
    for (auto& h : holders) {
        if (count >= 15) break;
        GraphNode n;
        n.id = "inst_" + std::to_string(count);
        n.label = h.value("name", "Unknown");
        float pct = h.value("percentage", 0.0f);
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.1f%%", pct);
        n.subtitle = buf;
        n.category = NodeCategory::Institutional;
        n.color = category_color(NodeCategory::Institutional);
        n.data = h;
        nodes_.push_back(std::move(n));

        GraphEdge e;
        e.from_id = "company";
        e.to_id = "inst_" + std::to_string(count);
        e.label = buf;
        e.color = category_color(NodeCategory::Institutional);
        e.thickness = std::max(1.0f, pct * 0.3f);
        edges_.push_back(std::move(e));
        count++;
    }
}

void RelationshipMapScreen::add_fund_family_nodes(const nlohmann::json& families) {
    if (!families.is_array()) return;
    int count = 0;
    for (auto& f : families) {
        if (count >= 10) break;
        GraphNode n;
        n.id = "fund_" + std::to_string(count);
        n.label = f.value("name", "Unknown");
        float pct = f.value("total_percentage", 0.0f);
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.1f%%", pct);
        n.subtitle = buf;
        n.category = NodeCategory::FundFamily;
        n.color = category_color(NodeCategory::FundFamily);
        n.data = f;
        nodes_.push_back(std::move(n));

        GraphEdge e;
        e.from_id = "company";
        e.to_id = n.id;
        e.color = category_color(NodeCategory::FundFamily);
        edges_.push_back(std::move(e));
        count++;
    }
}

void RelationshipMapScreen::add_insider_nodes(const nlohmann::json& insiders) {
    if (!insiders.is_array()) return;
    int count = 0;
    for (auto& ins : insiders) {
        if (count >= 15) break;
        GraphNode n;
        n.id = "insider_" + std::to_string(count);
        n.label = ins.value("name", "Unknown");
        n.subtitle = ins.value("title", "Insider");
        n.category = NodeCategory::Insider;
        n.color = category_color(NodeCategory::Insider);
        n.data = ins;
        nodes_.push_back(std::move(n));

        GraphEdge e;
        e.from_id = "company";
        e.to_id = n.id;
        e.color = category_color(NodeCategory::Insider);
        edges_.push_back(std::move(e));
        count++;
    }
}

void RelationshipMapScreen::add_peer_nodes(const nlohmann::json& peers) {
    if (!peers.is_array()) return;
    int count = 0;
    for (auto& p : peers) {
        if (count >= 12) break;
        GraphNode n;
        n.id = "peer_" + std::to_string(count);
        n.label = p.value("ticker", p.value("name", "Peer"));
        n.subtitle = p.value("sector", "");
        n.category = NodeCategory::Peer;
        n.color = category_color(NodeCategory::Peer);
        n.data = p;
        nodes_.push_back(std::move(n));

        GraphEdge e;
        e.from_id = "company";
        e.to_id = n.id;
        e.color = category_color(NodeCategory::Peer);
        e.dashed = true;
        edges_.push_back(std::move(e));
        count++;
    }
}

void RelationshipMapScreen::add_event_nodes(const nlohmann::json& events) {
    if (!events.is_array()) return;
    int count = 0;
    for (auto& ev : events) {
        if (count >= 10) break;
        GraphNode n;
        n.id = "event_" + std::to_string(count);
        n.label = ev.value("form", "8-K");
        n.subtitle = ev.value("filing_date", "");
        n.category = NodeCategory::Event;
        n.color = category_color(NodeCategory::Event);
        n.data = ev;
        nodes_.push_back(std::move(n));

        GraphEdge e;
        e.from_id = "company";
        e.to_id = n.id;
        e.color = category_color(NodeCategory::Event);
        e.dashed = true;
        e.thickness = 1.0f;
        edges_.push_back(std::move(e));
        count++;
    }
}

void RelationshipMapScreen::add_supply_chain_nodes(const nlohmann::json& chain) {
    if (!chain.is_array()) return;
    int count = 0;
    for (auto& sc : chain) {
        if (count >= 10) break;
        GraphNode n;
        n.id = "sc_" + std::to_string(count);
        n.label = sc.value("name", "Unknown");
        n.subtitle = sc.value("relationship", "partner");
        n.category = NodeCategory::SupplyChain;
        n.color = category_color(NodeCategory::SupplyChain);
        n.data = sc;
        nodes_.push_back(std::move(n));

        GraphEdge e;
        e.from_id = "company";
        e.to_id = n.id;
        e.color = category_color(NodeCategory::SupplyChain);
        edges_.push_back(std::move(e));
        count++;
    }
}

void RelationshipMapScreen::add_segment_nodes(const nlohmann::json& segments) {
    if (!segments.is_array()) return;
    int count = 0;
    for (auto& seg : segments) {
        if (count >= 8) break;
        GraphNode n;
        n.id = "seg_" + std::to_string(count);
        n.label = seg.value("name", "Segment");
        n.subtitle = seg.value("type", "");
        n.category = NodeCategory::Segment;
        n.color = category_color(NodeCategory::Segment);
        n.data = seg;
        nodes_.push_back(std::move(n));

        GraphEdge e;
        e.from_id = "company";
        e.to_id = n.id;
        e.color = category_color(NodeCategory::Segment);
        e.thickness = 1.0f;
        edges_.push_back(std::move(e));
        count++;
    }
}

void RelationshipMapScreen::add_debt_holder_nodes(const nlohmann::json& holders) {
    if (!holders.is_array()) return;
    int count = 0;
    for (auto& dh : holders) {
        if (count >= 8) break;
        GraphNode n;
        n.id = "debt_" + std::to_string(count);
        n.label = dh.value("name", "Unknown");
        n.subtitle = dh.value("type", "lender");
        n.category = NodeCategory::DebtHolder;
        n.color = category_color(NodeCategory::DebtHolder);
        n.data = dh;
        nodes_.push_back(std::move(n));

        GraphEdge e;
        e.from_id = "company";
        e.to_id = n.id;
        e.color = category_color(NodeCategory::DebtHolder);
        edges_.push_back(std::move(e));
        count++;
    }
}

// ============================================================================
// Radial layout — arrange nodes in concentric rings by category
// ============================================================================

void RelationshipMapScreen::layout_radial() {
    // Group nodes by category for ring placement
    std::map<NodeCategory, std::vector<int>> by_cat;
    for (int i = 0; i < (int)nodes_.size(); i++) {
        by_cat[nodes_[i].category].push_back(i);
    }

    // Company at center
    for (int i : by_cat[NodeCategory::Company]) {
        nodes_[i].pos = {0, 0};
    }

    auto place_ring = [&](NodeCategory cat, float radius) {
        auto& idxs = by_cat[cat];
        int n = (int)idxs.size();
        if (n == 0) return;
        float angle_step = 2.0f * PI / (float)n;
        // Offset angle per category to avoid overlap
        float base_angle = (float)cat * 0.7f;
        for (int i = 0; i < n; i++) {
            float angle = base_angle + angle_step * (float)i;
            nodes_[idxs[i]].pos = {
                radius * std::cos(angle),
                radius * std::sin(angle)
            };
        }
    };

    place_ring(NodeCategory::Segment,       RING_SEGMENTS);
    place_ring(NodeCategory::Insider,       RING_INSIDERS);
    place_ring(NodeCategory::Institutional, RING_INSTITUTIONAL);
    place_ring(NodeCategory::Event,         RING_EVENTS);
    place_ring(NodeCategory::FundFamily,    RING_FUND_FAMILIES);
    place_ring(NodeCategory::SupplyChain,   RING_SUPPLY_CHAIN);
    place_ring(NodeCategory::Peer,          RING_PEERS);
    place_ring(NodeCategory::DebtHolder,    RING_DEBT_HOLDERS);
}

// ============================================================================
// Main render
// ============================================================================

void RelationshipMapScreen::render() {
    if (!initialized_) { init(); initialized_ = true; }
    check_async();

    ui::ScreenFrame frame("##relmap_screen", ImVec2(0, 0), BG_DARK);
    if (!frame.begin()) { frame.end(); return; }

    float w = frame.width();
    float h = frame.height();

    render_header(w);
    render_toolbar(w);

    float canvas_h = h - 94.0f; // header + toolbar + status
    float detail_w = show_detail_panel_ ? 300.0f : 0.0f;
    float canvas_w = w - detail_w;

    ImGui::BeginGroup();
    {
        render_canvas(canvas_w, canvas_h);
        if (show_detail_panel_) {
            ImGui::SameLine(0, 2);
            render_detail_panel(canvas_h);
        }
    }
    ImGui::EndGroup();

    if (status_ == AsyncStatus::Loading)
        render_loading_overlay(w, h);

    render_status_bar(w);
    frame.end();
}

// ============================================================================
// Header
// ============================================================================

void RelationshipMapScreen::render_header(float w) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##rm_header", ImVec2(w, 32), false);

    ImGui::SetCursorPos(ImVec2(8, 6));
    ImGui::TextColored(ACCENT, "Relationship Map");
    ImGui::SameLine();
    if (!company_name_.empty()) {
        ImGui::TextColored(TEXT_PRIMARY, "| %s", company_name_.c_str());
    }
    ImGui::SameLine();
    ImGui::TextColored(TEXT_DIM, "| Corporate Intelligence Graph");

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Toolbar
// ============================================================================

void RelationshipMapScreen::render_toolbar(float w) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARKEST);
    ImGui::BeginChild("##rm_toolbar", ImVec2(w, 32), false);

    ImGui::SetCursorPos(ImVec2(4, 4));

    // Ticker input
    ImGui::Text("Ticker:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80);
    bool enter = ImGui::InputText("##rm_ticker", ticker_buf_, sizeof(ticker_buf_),
                                   ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    if ((ImGui::SmallButton("Load") || enter) && !is_async_busy() && std::strlen(ticker_buf_) > 0) {
        loaded_ticker_ = ticker_buf_;
        // Uppercase
        for (auto& c : loaded_ticker_)
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        load_relationship_data(loaded_ticker_);
    }

    ImGui::SameLine(0, 20);

    // Filter toggle
    if (ImGui::SmallButton(show_filter_panel_ ? "Filters [x]" : "Filters")) {
        show_filter_panel_ = !show_filter_panel_;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton(show_legend_ ? "Legend [x]" : "Legend")) {
        show_legend_ = !show_legend_;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton(show_detail_panel_ ? "Detail [x]" : "Detail")) {
        show_detail_panel_ = !show_detail_panel_;
    }

    // Zoom controls
    ImGui::SameLine(w - 200);
    if (ImGui::SmallButton("-")) canvas_zoom_ = std::max(0.2f, canvas_zoom_ - 0.1f);
    ImGui::SameLine();
    ImGui::Text("%.0f%%", canvas_zoom_ * 100);
    ImGui::SameLine();
    if (ImGui::SmallButton("+")) canvas_zoom_ = std::min(3.0f, canvas_zoom_ + 0.1f);
    ImGui::SameLine();
    if (ImGui::SmallButton("Reset")) { canvas_zoom_ = 1.0f; canvas_offset_ = {0, 0}; }

    ImGui::EndChild();
    ImGui::PopStyleColor();

    // Filter panel popup
    if (show_filter_panel_) render_filter_panel();
}

// ============================================================================
// Canvas — draw graph nodes and edges
// ============================================================================

void RelationshipMapScreen::render_canvas(float w, float h) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARKEST);
    ImGui::BeginChild("##rm_canvas", ImVec2(w, h), false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    ImVec2 canvas_size(w, h);
    ImVec2 center(canvas_pos.x + w * 0.5f + canvas_offset_.x,
                  canvas_pos.y + h * 0.5f + canvas_offset_.y);

    // Handle panning
    if (ImGui::IsWindowHovered()) {
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle) ||
            (ImGui::IsMouseDragging(ImGuiMouseButton_Left) && ImGui::GetIO().KeyAlt)) {
            ImVec2 delta = ImGui::GetIO().MouseDelta;
            canvas_offset_.x += delta.x;
            canvas_offset_.y += delta.y;
        }
        // Scroll to zoom
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f) {
            canvas_zoom_ = std::clamp(canvas_zoom_ + wheel * 0.1f, 0.2f, 3.0f);
        }
    }

    if (nodes_.empty()) {
        ImGui::SetCursorPos(ImVec2(w / 2 - 140, h / 2));
        ImGui::TextColored(TEXT_DIM, "Enter a ticker above and click Load to visualize");
        ImGui::EndChild();
        ImGui::PopStyleColor();
        return;
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Draw edges first (behind nodes)
    for (auto& edge : edges_) {
        auto* from = find_node(edge.from_id);
        auto* to   = find_node(edge.to_id);
        if (!from || !to) continue;
        if (!should_show_node(*from) || !should_show_node(*to)) continue;

        ImVec2 p1(center.x + from->pos.x * canvas_zoom_, center.y + from->pos.y * canvas_zoom_);
        ImVec2 p2(center.x + to->pos.x * canvas_zoom_,   center.y + to->pos.y * canvas_zoom_);
        ImU32 col = ImGui::ColorConvertFloat4ToU32(
            ImVec4(edge.color.x, edge.color.y, edge.color.z, 0.4f));
        dl->AddLine(p1, p2, col, edge.thickness * canvas_zoom_);
    }

    // Draw nodes
    for (auto& node : nodes_) {
        if (!should_show_node(node)) continue;
        render_node(node, center);
    }

    // Legend overlay
    if (show_legend_) render_legend(w, h);

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Render a single node
// ============================================================================

void RelationshipMapScreen::render_node(const GraphNode& node, ImVec2 canvas_origin) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float zoom = canvas_zoom_;

    ImVec2 pos(canvas_origin.x + node.pos.x * zoom,
               canvas_origin.y + node.pos.y * zoom);
    float r = (node.category == NodeCategory::Company ? COMPANY_RADIUS : NODE_RADIUS) * zoom;
    bool selected = (node.id == selected_node_id_);

    // Background circle
    ImU32 bg_col = ImGui::ColorConvertFloat4ToU32(
        ImVec4(node.color.x * 0.2f, node.color.y * 0.2f, node.color.z * 0.2f, 0.8f));
    ImU32 border_col = ImGui::ColorConvertFloat4ToU32(
        selected ? ImVec4(1, 1, 1, 1) : node.color);
    dl->AddCircleFilled(pos, r, bg_col, 32);
    dl->AddCircle(pos, r, border_col, 32, selected ? 3.0f : 1.5f);

    // Label text (truncated to fit)
    std::string display = node.label;
    if (display.size() > 12 && zoom < 1.5f) display = display.substr(0, 10) + "..";
    ImVec2 text_size = ImGui::CalcTextSize(display.c_str());
    dl->AddText(ImVec2(pos.x - text_size.x * 0.5f, pos.y - text_size.y * 0.5f),
                ImGui::ColorConvertFloat4ToU32(TEXT_PRIMARY), display.c_str());

    // Subtitle below
    if (!node.subtitle.empty() && zoom >= 0.7f) {
        std::string sub = node.subtitle;
        if (sub.size() > 16) sub = sub.substr(0, 14) + "..";
        ImVec2 sub_size = ImGui::CalcTextSize(sub.c_str());
        dl->AddText(ImVec2(pos.x - sub_size.x * 0.5f, pos.y + r + 2),
                    ImGui::ColorConvertFloat4ToU32(TEXT_DIM), sub.c_str());
    }

    // Hit testing for click
    ImVec2 mouse = ImGui::GetIO().MousePos;
    float dx = mouse.x - pos.x;
    float dy = mouse.y - pos.y;
    if (dx * dx + dy * dy <= r * r) {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            selected_node_id_ = node.id;
            show_detail_panel_ = true;
        }
    }
}

// ============================================================================
// Filter panel
// ============================================================================

void RelationshipMapScreen::render_filter_panel() {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_WIDGET);
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f - 150, 100));
    ImGui::BeginChild("##rm_filters", ImVec2(300, 260), true);

    ImGui::TextColored(ACCENT, "Graph Filters");
    ImGui::Separator();

    ImGui::Checkbox("Institutional Holders", &filters_.show_institutional);
    ImGui::Checkbox("Fund Families",         &filters_.show_fund_families);
    ImGui::Checkbox("Insiders",              &filters_.show_insiders);
    ImGui::Checkbox("Peer Companies",        &filters_.show_peers);
    ImGui::Checkbox("Corporate Events",      &filters_.show_events);
    ImGui::Checkbox("Supply Chain",          &filters_.show_supply_chain);
    ImGui::Checkbox("Segments",              &filters_.show_segments);
    ImGui::Checkbox("Debt Holders",          &filters_.show_debt_holders);

    ImGui::Separator();
    ImGui::Text("Min Ownership %%:");
    ImGui::SetNextItemWidth(200);
    ImGui::SliderFloat("##min_own", &filters_.min_ownership, 0.0f, 20.0f, "%.1f%%");

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Detail panel (right side)
// ============================================================================

void RelationshipMapScreen::render_detail_panel(float h) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##rm_detail", ImVec2(298, h), true);

    if (selected_node_id_.empty()) {
        ImGui::TextColored(TEXT_DIM, "Click a node to see details");
        ImGui::EndChild();
        ImGui::PopStyleColor();
        return;
    }

    const GraphNode* node = find_node(selected_node_id_);
    if (!node) {
        ImGui::TextColored(TEXT_DIM, "Node not found");
        ImGui::EndChild();
        ImGui::PopStyleColor();
        return;
    }

    ImGui::TextColored(node->color, "%s", category_name(node->category));
    ImGui::TextColored(TEXT_PRIMARY, "%s", node->label.c_str());
    if (!node->subtitle.empty())
        ImGui::TextColored(TEXT_SECONDARY, "%s", node->subtitle.c_str());
    ImGui::Separator();

    // Render JSON data as key-value pairs
    if (node->data.is_object()) {
        for (auto it = node->data.begin(); it != node->data.end(); ++it) {
            const auto& key = it.key();
            const auto& val = it.value();

            ImGui::TextColored(TEXT_DIM, "%s:", key.c_str());
            ImGui::SameLine();

            if (val.is_number_float()) {
                double v = val.get<double>();
                if (std::abs(v) >= 1e9)
                    ImGui::TextColored(TEXT_PRIMARY, "$%.2fB", v / 1e9);
                else if (std::abs(v) >= 1e6)
                    ImGui::TextColored(TEXT_PRIMARY, "$%.2fM", v / 1e6);
                else
                    ImGui::TextColored(TEXT_PRIMARY, "%.4f", v);
            } else if (val.is_number_integer()) {
                int64_t v = val.get<int64_t>();
                if (std::abs(v) >= 1000000000LL)
                    ImGui::TextColored(TEXT_PRIMARY, "%lldB", (long long)(v / 1000000000LL));
                else if (std::abs(v) >= 1000000LL)
                    ImGui::TextColored(TEXT_PRIMARY, "%lldM", (long long)(v / 1000000LL));
                else
                    ImGui::TextColored(TEXT_PRIMARY, "%lld", (long long)v);
            } else if (val.is_string()) {
                ImGui::TextWrapped("%s", val.get<std::string>().c_str());
            } else if (val.is_boolean()) {
                ImGui::TextColored(val.get<bool>() ? SUCCESS : ERROR_RED,
                                   "%s", val.get<bool>() ? "Yes" : "No");
            } else if (val.is_null()) {
                ImGui::TextColored(TEXT_DISABLED, "N/A");
            } else {
                ImGui::TextColored(TEXT_DIM, "%s", val.dump().c_str());
            }
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Legend overlay
// ============================================================================

void RelationshipMapScreen::render_legend(float w, float h) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    // Position at bottom-left of canvas
    float lx = canvas_pos.x + 10;
    float ly = canvas_pos.y + h - 180;

    dl->AddRectFilled(ImVec2(lx, ly), ImVec2(lx + 160, ly + 170),
                      ImGui::ColorConvertFloat4ToU32(ImVec4(0.05f, 0.05f, 0.06f, 0.85f)), 4.0f);

    float y = ly + 6;
    NodeCategory cats[] = {
        NodeCategory::Company, NodeCategory::Institutional, NodeCategory::FundFamily,
        NodeCategory::Insider, NodeCategory::Peer, NodeCategory::Event,
        NodeCategory::SupplyChain, NodeCategory::Segment, NodeCategory::DebtHolder
    };
    for (auto cat : cats) {
        ImU32 col = ImGui::ColorConvertFloat4ToU32(category_color(cat));
        dl->AddCircleFilled(ImVec2(lx + 14, y + 7), 5, col);
        dl->AddText(ImVec2(lx + 26, y), ImGui::ColorConvertFloat4ToU32(TEXT_SECONDARY),
                    category_name(cat));
        y += 18;
    }
}

// ============================================================================
// Loading overlay
// ============================================================================

void RelationshipMapScreen::render_loading_overlay(float w, float h) {
    ImVec2 vp = ImGui::GetMainViewport()->Pos;
    ImDrawList* dl = ImGui::GetForegroundDrawList();

    // Semi-transparent overlay
    dl->AddRectFilled(ImVec2(vp.x, vp.y), ImVec2(vp.x + w, vp.y + h),
                      ImGui::ColorConvertFloat4ToU32(ImVec4(0, 0, 0, 0.5f)));

    // Loading box
    float bx = vp.x + w / 2 - 150;
    float by = vp.y + h / 2 - 40;
    dl->AddRectFilled(ImVec2(bx, by), ImVec2(bx + 300, by + 80),
                      ImGui::ColorConvertFloat4ToU32(BG_WIDGET), 6.0f);
    dl->AddText(ImVec2(bx + 20, by + 10),
                ImGui::ColorConvertFloat4ToU32(ACCENT),
                loading_.message.c_str());

    // Progress bar
    float bar_w = 260.0f * (loading_.progress / 100.0f);
    dl->AddRectFilled(ImVec2(bx + 20, by + 50), ImVec2(bx + 20 + 260, by + 58),
                      ImGui::ColorConvertFloat4ToU32(BG_INPUT), 3.0f);
    dl->AddRectFilled(ImVec2(bx + 20, by + 50), ImVec2(bx + 20 + bar_w, by + 58),
                      ImGui::ColorConvertFloat4ToU32(ACCENT), 3.0f);
}

// ============================================================================
// Status bar
// ============================================================================

void RelationshipMapScreen::render_status_bar(float w) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARKEST);
    ImGui::BeginChild("##rm_status", ImVec2(w, 24), false);
    ImGui::SetCursorPos(ImVec2(8, 4));

    if (is_async_busy()) {
        ImGui::TextColored(WARNING, "Loading: %s", loading_.message.c_str());
    } else if (status_ == AsyncStatus::Error) {
        ImGui::TextColored(ERROR_RED, "Error: %s", error_.c_str());
    } else if (status_ == AsyncStatus::Success) {
        ImGui::TextColored(SUCCESS, "Ready");
        ImGui::SameLine();
        ImGui::TextColored(TEXT_DIM, "| %d nodes | %d edges",
            (int)nodes_.size(), (int)edges_.size());
    } else {
        ImGui::TextColored(TEXT_DIM, "Ready — enter a ticker to begin");
    }

    ImGui::SameLine(w - 200);
    ImGui::TextColored(TEXT_DIM, "Pan: Alt+Drag | Zoom: Scroll");

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

} // namespace fincept::relationship_map
