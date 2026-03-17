#pragma once
// MCP Servers Screen — Manage Model Context Protocol server integrations
// Port of MCP Tab (index.tsx) — marketplace, installed servers, tools management
// Uses existing fincept::mcp::MCPManager for server lifecycle

#include <imgui.h>
#include <string>
#include <vector>

namespace fincept::mcp_servers {

// ============================================================================
// Marketplace server definition (static catalog)
// ============================================================================

struct MarketplaceServer {
    std::string id;
    std::string name;
    std::string description;
    std::string category;    // data, web, productivity, dev
    std::string icon;        // emoji
    std::string command;
    std::vector<std::string> args;
    bool requires_config;
    std::vector<std::string> tools;
    std::string documentation;
};

// ============================================================================
// Screen
// ============================================================================

class MCPServersScreen {
public:
    void render();

private:
    bool initialized_ = false;
    void init();

    // --- Views ---
    enum class View { Marketplace, Installed, Tools };
    View active_view_ = View::Marketplace;

    // --- Marketplace ---
    std::vector<MarketplaceServer> marketplace_;
    char search_buf_[128] = {};
    int category_filter_ = 0; // 0=all, 1=data, 2=web, 3=productivity, 4=dev

    // --- Add server modal ---
    bool show_add_modal_ = false;
    char add_id_[64]      = {};
    char add_name_[128]   = {};
    char add_cmd_[256]    = {};
    char add_args_[512]   = {};
    char add_desc_[256]   = {};
    int  add_category_    = 0;
    bool add_auto_start_  = false;
    char add_env_key_[64] = {};
    char add_env_val_[256] = {};
    std::vector<std::pair<std::string, std::string>> add_env_pairs_;

    // --- Status ---
    std::string status_msg_;
    std::string error_msg_;

    // --- Actions ---
    void refresh_servers();
    void install_marketplace_server(const MarketplaceServer& server);
    void start_server(const std::string& id);
    void stop_server(const std::string& id);
    void remove_server(const std::string& id);
    void add_custom_server();

    // --- Render sections ---
    void render_header(float w);
    void render_view_tabs(float w);
    void render_marketplace_view(float w, float h);
    void render_installed_view(float w, float h);
    void render_tools_view(float w, float h);
    void render_add_modal();
    void render_status_bar(float w);
};

} // namespace fincept::mcp_servers
