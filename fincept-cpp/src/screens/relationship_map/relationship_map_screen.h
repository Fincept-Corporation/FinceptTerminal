#pragma once
// Relationship Map — Multi-layer corporate intelligence visualization
// Port of RelationshipMapTab.tsx — radial graph of company relationships:
// institutional holders, insiders, peer companies, events, supply chain,
// segments, debt holders, fund families — with filters + detail panel

#include <imgui.h>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <future>
#include <nlohmann/json.hpp>

namespace fincept::relationship_map {

// ============================================================================
// Node types
// ============================================================================

enum class NodeCategory {
    Company, Peer, Institutional, FundFamily, Insider,
    Event, SupplyChain, Segment, DebtHolder, Metrics
};

struct GraphNode {
    std::string id;
    std::string label;
    std::string subtitle;
    NodeCategory category = NodeCategory::Company;
    ImVec2 pos  = {0, 0};
    ImVec4 color = {1, 1, 1, 1};
    // Extra data (JSON blob for detail panel)
    nlohmann::json data;
};

struct GraphEdge {
    std::string from_id;
    std::string to_id;
    std::string label;
    ImVec4 color = {0.4f, 0.4f, 0.4f, 1.0f};
    float thickness = 1.5f;
    bool dashed = false;
};

// ============================================================================
// Filter state
// ============================================================================

struct FilterState {
    bool show_institutional = true;
    bool show_insiders      = true;
    bool show_peers         = true;
    bool show_events        = true;
    bool show_fund_families = true;
    bool show_supply_chain  = true;
    bool show_segments      = true;
    bool show_debt_holders  = true;
    float min_ownership     = 0.0f;  // 0-100%
};

// ============================================================================
// Loading state
// ============================================================================

struct LoadingPhase {
    std::string phase;     // idle, company, ownership, peers, complete, error
    int progress = 0;      // 0-100
    std::string message;
};

// ============================================================================
// Screen
// ============================================================================

class RelationshipMapScreen {
public:
    void render();

private:
    bool initialized_ = false;
    void init();

    // --- Ticker input ---
    char ticker_buf_[32] = "AAPL";
    std::string loaded_ticker_;

    // --- Graph data ---
    std::vector<GraphNode> nodes_;
    std::vector<GraphEdge> edges_;
    std::string company_name_;

    // --- Camera/pan ---
    ImVec2 canvas_offset_ = {0, 0};
    float canvas_zoom_    = 1.0f;
    bool dragging_canvas_ = false;
    ImVec2 drag_start_    = {0, 0};

    // --- Selected node ---
    std::string selected_node_id_;
    bool show_detail_panel_ = false;

    // --- Filters ---
    FilterState filters_;
    bool show_filter_panel_ = false;
    bool show_legend_       = true;

    // --- Loading ---
    LoadingPhase loading_;
    enum class AsyncStatus { Idle, Loading, Success, Error };
    AsyncStatus status_ = AsyncStatus::Idle;
    std::string error_;

    // --- Async ---
    std::future<void> async_task_;
    bool is_async_busy() const;
    void check_async();

    // --- Data fetching ---
    void load_relationship_data(const std::string& ticker);
    void build_graph(const nlohmann::json& data);
    void layout_radial();

    // --- Graph node builders ---
    void add_company_node(const nlohmann::json& company);
    void add_institutional_nodes(const nlohmann::json& holders);
    void add_fund_family_nodes(const nlohmann::json& families);
    void add_insider_nodes(const nlohmann::json& insiders);
    void add_peer_nodes(const nlohmann::json& peers);
    void add_event_nodes(const nlohmann::json& events);
    void add_supply_chain_nodes(const nlohmann::json& chain);
    void add_segment_nodes(const nlohmann::json& segments);
    void add_debt_holder_nodes(const nlohmann::json& holders);

    // --- Render sections ---
    void render_header(float w);
    void render_toolbar(float w);
    void render_canvas(float w, float h);
    void render_node(const GraphNode& node, ImVec2 canvas_origin);
    void render_edge(const GraphEdge& edge, ImVec2 canvas_origin);
    void render_filter_panel();
    void render_detail_panel(float h);
    void render_legend(float w, float h);
    void render_loading_overlay(float w, float h);
    void render_status_bar(float w);

    // --- Helpers ---
    ImVec4 category_color(NodeCategory cat) const;
    const char* category_name(NodeCategory cat) const;
    const GraphNode* find_node(const std::string& id) const;
    bool should_show_node(const GraphNode& node) const;
};

} // namespace fincept::relationship_map
