// MCP Servers Screen — Manage Model Context Protocol server integrations
// Port of MCP Tab — marketplace browse, installed server management,
// tool aggregation view. Uses fincept::mcp::MCPManager for lifecycle.

#include "mcp_servers_screen.h"
#include "ui/yoga_helpers.h"
#include "ui/theme.h"
#include "mcp/mcp_manager.h"
#include "mcp/mcp_service.h"
#include "core/logger.h"
#include <imgui.h>
#include <algorithm>
#include <cstring>
#include <sstream>

namespace fincept::mcp_servers {

using namespace theme::colors;

// ============================================================================
// Marketplace catalog
// ============================================================================

static std::vector<MarketplaceServer> build_marketplace() {
    return {
        {"postgres", "PostgreSQL", "Connect to PostgreSQL databases for SQL queries and analysis",
         "data", "E", "bunx", {"-y", "@modelcontextprotocol/server-postgres"},
         true, {"query", "describe_table", "list_tables", "explain_query", "get_indexes"},
         "https://github.com/modelcontextprotocol/servers/tree/main/src/postgres"},

        {"questdb", "QuestDB", "Connect to QuestDB time-series databases for fast analytics",
         "data", "Q", "bunx", {"-y", "@questdb/mcp-server"},
         true, {"query", "list_tables", "describe_table", "insert_data", "get_metrics"},
         "https://questdb.io/docs/third-party-tools/mcp/"},

        {"wikipedia", "Wikipedia", "Search and retrieve Wikipedia articles with summaries and key facts",
         "web", "W", "bunx", {"-y", "wikipedia-mcp"},
         false, {"search_wikipedia", "get_article", "get_summary", "get_sections", "extract_key_facts"},
         "https://github.com/Rudra-ravi/wikipedia-mcp"},

        {"defeatbeta-api", "DefeatBeta API", "Open-source Yahoo Finance alternative with 70+ financial tools",
         "data", "$", "uvx", {"--refresh", "git+https://github.com/defeat-beta/defeatbeta-api.git#subdirectory=mcp"},
         false, {"get_stock_profile", "get_stock_price", "get_stock_news", "get_stock_sec_filings",
                 "get_stock_earning_call_transcript", "get_quarterly_income_statement"},
         "https://github.com/defeat-beta/defeatbeta-api/blob/main/mcp/README.md"},
    };
}

static const char* CATEGORY_LABELS[] = {"All", "Data", "Web", "Productivity", "Dev"};
static const char* CATEGORY_IDS[]    = {"all", "data", "web", "productivity", "dev"};
static constexpr int CATEGORY_COUNT  = 5;

// ============================================================================
// Init
// ============================================================================

void MCPServersScreen::init() {
    marketplace_ = build_marketplace();
    refresh_servers();
}

// ============================================================================
// Server actions — delegate to MCPManager
// ============================================================================

void MCPServersScreen::refresh_servers() {
    status_msg_ = "Refreshed server list";
    error_msg_.clear();
}

void MCPServersScreen::install_marketplace_server(const MarketplaceServer& server) {
    mcp::MCPServerConfig config;
    config.id          = server.id;
    config.name        = server.name;
    config.description = server.description;
    config.command     = server.command;
    config.args        = server.args;
    config.category    = server.category;
    config.enabled     = true;
    config.auto_start  = false;

    auto result = mcp::MCPManager::instance().save_server(config);
    if (result) {
        status_msg_ = "Installed: " + server.name;
        error_msg_.clear();
    } else {
        error_msg_ = "Failed to install: " + server.name;
    }
}

void MCPServersScreen::start_server(const std::string& id) {
    auto result = mcp::MCPManager::instance().start_server(id);
    if (result) {
        status_msg_ = "Started: " + id;
        error_msg_.clear();
    } else {
        error_msg_ = "Failed to start: " + id;
    }
}

void MCPServersScreen::stop_server(const std::string& id) {
    auto result = mcp::MCPManager::instance().stop_server(id);
    if (result) {
        status_msg_ = "Stopped: " + id;
    } else {
        error_msg_ = "Failed to stop: " + id;
    }
}

void MCPServersScreen::remove_server(const std::string& id) {
    auto result = mcp::MCPManager::instance().remove_server(id);
    if (result) {
        status_msg_ = "Removed: " + id;
    } else {
        error_msg_ = "Failed to remove: " + id;
    }
}

void MCPServersScreen::add_custom_server() {
    if (std::strlen(add_id_) == 0 || std::strlen(add_cmd_) == 0) {
        error_msg_ = "Server ID and command are required";
        return;
    }

    mcp::MCPServerConfig config;
    config.id          = add_id_;
    config.name        = std::strlen(add_name_) > 0 ? add_name_ : add_id_;
    config.description = add_desc_;
    config.command     = add_cmd_;
    config.category    = CATEGORY_IDS[std::clamp(add_category_, 0, CATEGORY_COUNT - 1)];
    config.auto_start  = add_auto_start_;
    config.enabled     = true;

    // Parse args (space-separated)
    std::string args_str(add_args_);
    std::istringstream iss(args_str);
    std::string arg;
    while (iss >> arg) config.args.push_back(arg);

    // Add env vars
    for (auto& kv : add_env_pairs_)
        config.env[kv.first] = kv.second;

    auto result = mcp::MCPManager::instance().save_server(config);
    if (result) {
        status_msg_ = "Added server: " + config.name;
        show_add_modal_ = false;
        // Reset form
        std::memset(add_id_, 0, sizeof(add_id_));
        std::memset(add_name_, 0, sizeof(add_name_));
        std::memset(add_cmd_, 0, sizeof(add_cmd_));
        std::memset(add_args_, 0, sizeof(add_args_));
        std::memset(add_desc_, 0, sizeof(add_desc_));
        add_env_pairs_.clear();
        add_auto_start_ = false;
        error_msg_.clear();
    } else {
        error_msg_ = "Failed to add server";
    }
}

// ============================================================================
// Main render
// ============================================================================

void MCPServersScreen::render() {
    if (!initialized_) { init(); initialized_ = true; }

    ui::ScreenFrame frame("##mcp_screen", ImVec2(0, 0), BG_DARK);
    if (!frame.begin()) { frame.end(); return; }

    float w = frame.width();
    float h = frame.height();

    render_header(w);
    render_view_tabs(w);

    float content_h = h - 88.0f; // header + tabs + status

    switch (active_view_) {
        case View::Marketplace: render_marketplace_view(w, content_h); break;
        case View::Installed:   render_installed_view(w, content_h); break;
        case View::Tools:       render_tools_view(w, content_h); break;
    }

    if (show_add_modal_) render_add_modal();

    render_status_bar(w);
    frame.end();
}

// ============================================================================
// Header
// ============================================================================

void MCPServersScreen::render_header(float w) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##mcp_header", ImVec2(w, 32), false);

    ImGui::SetCursorPos(ImVec2(8, 6));
    ImGui::TextColored(ACCENT, "MCP Servers");
    ImGui::SameLine();
    ImGui::TextColored(TEXT_DIM, "| Model Context Protocol Integrations");

    // Installed count
    auto servers = mcp::MCPManager::instance().get_servers();
    int running = 0;
    for (auto& s : servers) {
        if (mcp::MCPManager::instance().get_server_status(s.id) == mcp::ServerStatus::Running)
            running++;
    }
    ImGui::SameLine(w - 200);
    ImGui::TextColored(SUCCESS, "%d running", running);
    ImGui::SameLine();
    ImGui::TextColored(TEXT_DIM, "/ %d configured", (int)servers.size());

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// View tabs
// ============================================================================

void MCPServersScreen::render_view_tabs(float w) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARKEST);
    ImGui::BeginChild("##mcp_tabs", ImVec2(w, 28), false);
    ImGui::SetCursorPos(ImVec2(4, 3));

    struct Tab { const char* label; View view; };
    Tab tabs[] = {
        {"Marketplace", View::Marketplace},
        {"Installed",   View::Installed},
        {"Tools",       View::Tools},
    };

    for (auto& t : tabs) {
        bool active = (active_view_ == t.view);
        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Button, ACCENT_BG);
            ImGui::PushStyleColor(ImGuiCol_Text, ACCENT);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, BG_WIDGET);
            ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
        }
        if (ImGui::SmallButton(t.label)) active_view_ = t.view;
        ImGui::PopStyleColor(2);
        ImGui::SameLine(0, 6);
    }

    // Add server button
    ImGui::SameLine(w - 120);
    ImGui::PushStyleColor(ImGuiCol_Button, ACCENT);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1));
    if (ImGui::SmallButton("+ Add Server")) show_add_modal_ = true;
    ImGui::PopStyleColor(2);

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Marketplace view
// ============================================================================

void MCPServersScreen::render_marketplace_view(float w, float h) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##mcp_market", ImVec2(w, h), false);

    // Search + category filter
    ImGui::SetNextItemWidth(w * 0.4f);
    ImGui::InputTextWithHint("##mcp_search", "Search servers...", search_buf_, sizeof(search_buf_));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120);
    ImGui::Combo("##mcp_cat", &category_filter_, CATEGORY_LABELS, CATEGORY_COUNT);
    ImGui::Separator();

    std::string search_lower(search_buf_);
    for (auto& c : search_lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    // Check what's already installed
    auto installed = mcp::MCPManager::instance().get_servers();

    // Server cards
    for (auto& server : marketplace_) {
        // Category filter
        if (category_filter_ > 0) {
            if (server.category != CATEGORY_IDS[category_filter_]) continue;
        }
        // Search filter
        if (!search_lower.empty()) {
            std::string name_lower = server.name;
            for (auto& c : name_lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            std::string desc_lower = server.description;
            for (auto& c : desc_lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (name_lower.find(search_lower) == std::string::npos &&
                desc_lower.find(search_lower) == std::string::npos)
                continue;
        }

        bool is_installed = false;
        for (auto& s : installed) {
            if (s.id == server.id) { is_installed = true; break; }
        }

        // Card
        ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_WIDGET);
        ImGui::BeginChild(("##mp_" + server.id).c_str(), ImVec2(w - 24, 80), true);

        ImGui::TextColored(TEXT_PRIMARY, "%s %s", server.icon.c_str(), server.name.c_str());
        ImGui::SameLine(w - 140);
        if (is_installed) {
            ImGui::TextColored(SUCCESS, "Installed");
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ACCENT);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1));
            if (ImGui::SmallButton(("Install##" + server.id).c_str())) {
                install_marketplace_server(server);
            }
            ImGui::PopStyleColor(2);
        }

        ImGui::TextColored(TEXT_SECONDARY, "%s", server.description.c_str());

        // Tools list
        ImGui::TextColored(TEXT_DIM, "Tools: ");
        ImGui::SameLine();
        int shown = 0;
        for (auto& tool : server.tools) {
            if (shown >= 4) { ImGui::SameLine(); ImGui::TextColored(TEXT_DISABLED, "+%d more", (int)server.tools.size() - shown); break; }
            ImGui::SameLine();
            ImGui::TextColored(INFO, "%s", tool.c_str());
            shown++;
        }

        ImGui::TextColored(TEXT_DISABLED, "Category: %s | Command: %s", server.category.c_str(), server.command.c_str());

        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Installed view
// ============================================================================

void MCPServersScreen::render_installed_view(float w, float h) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##mcp_installed", ImVec2(w, h), false);

    auto servers = mcp::MCPManager::instance().get_servers();

    if (servers.empty()) {
        ImGui::SetCursorPos(ImVec2(w / 2 - 120, h / 2));
        ImGui::TextColored(TEXT_DIM, "No servers configured. Browse the Marketplace to add one.");
        ImGui::EndChild();
        ImGui::PopStyleColor();
        return;
    }

    // Refresh button
    if (ImGui::SmallButton("Refresh All")) refresh_servers();
    ImGui::SameLine();
    ImGui::TextColored(TEXT_DIM, "%d servers configured", (int)servers.size());
    ImGui::Separator();

    for (auto& config : servers) {
        auto status = mcp::MCPManager::instance().get_server_status(config.id);
        bool is_running = (status == mcp::ServerStatus::Running);
        bool is_error   = (status == mcp::ServerStatus::Error);

        ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_WIDGET);
        ImGui::BeginChild(("##srv_" + config.id).c_str(), ImVec2(w - 24, 72), true);

        // Status indicator + name
        ImVec4 status_col = is_running ? SUCCESS : (is_error ? ERROR_RED : TEXT_DIM);
        ImGui::TextColored(status_col, "[%s]", mcp::server_status_str(status));
        ImGui::SameLine();
        ImGui::TextColored(TEXT_PRIMARY, "%s", config.name.c_str());

        // Action buttons on right
        ImGui::SameLine(w - 220);
        if (is_running) {
            if (ImGui::SmallButton(("Stop##" + config.id).c_str()))
                stop_server(config.id);
            ImGui::SameLine();
            if (ImGui::SmallButton(("Restart##" + config.id).c_str())) {
                stop_server(config.id);
                start_server(config.id);
            }
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, SUCCESS);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1));
            if (ImGui::SmallButton(("Start##" + config.id).c_str()))
                start_server(config.id);
            ImGui::PopStyleColor(2);
        }
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ERROR_RED);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
        if (ImGui::SmallButton(("Remove##" + config.id).c_str()))
            remove_server(config.id);
        ImGui::PopStyleColor(2);

        // Details
        ImGui::TextColored(TEXT_SECONDARY, "%s", config.description.c_str());
        ImGui::TextColored(TEXT_DIM, "Command: %s | Category: %s | Auto-start: %s",
            config.command.c_str(), config.category.c_str(),
            config.auto_start ? "Yes" : "No");

        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Tools view
// ============================================================================

void MCPServersScreen::render_tools_view(float w, float h) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##mcp_tools", ImVec2(w, h), false);

    // All tools (internal + external) from MCP service
    auto all_tools = mcp::MCPService::instance().get_all_tools();

    int internal_count = 0, external_count = 0;
    for (auto& t : all_tools) {
        if (t.is_internal) internal_count++; else external_count++;
    }

    ImGui::TextColored(ACCENT, "Internal Tools (%d)", internal_count);
    ImGui::Separator();

    for (auto& tool : all_tools) {
        if (!tool.is_internal) continue;
        ImGui::PushID(tool.name.c_str());
        ImGui::TextColored(SUCCESS, "[INT]");
        ImGui::SameLine();
        ImGui::TextColored(TEXT_PRIMARY, "%s", tool.name.c_str());

        ImGui::TextColored(TEXT_SECONDARY, "  %s", tool.description.c_str());
        ImGui::PopID();
    }

    ImGui::Spacing();
    ImGui::TextColored(ACCENT, "External Tools (%d)", external_count);
    ImGui::Separator();

    if (external_count == 0) {
        ImGui::TextColored(TEXT_DIM, "No external tools available. Start an MCP server to see its tools.");
    } else {
        for (auto& tool : all_tools) {
            if (tool.is_internal) continue;
            ImGui::PushID((tool.server_id + tool.name).c_str());
            ImGui::TextColored(INFO, "[%s]", tool.server_name.c_str());
            ImGui::SameLine();
            ImGui::TextColored(TEXT_PRIMARY, "%s", tool.name.c_str());

            ImGui::TextColored(TEXT_SECONDARY, "  %s", tool.description.c_str());
            ImGui::PopID();
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Add server modal
// ============================================================================

void MCPServersScreen::render_add_modal() {
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f - 220,
                                    ImGui::GetIO().DisplaySize.y * 0.5f - 200));
    ImGui::SetNextWindowSize(ImVec2(440, 400));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, BG_WIDGET);

    if (ImGui::Begin("Add Custom MCP Server", &show_add_modal_,
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse)) {
        float field_w = 400.0f;

        ImGui::Text("Server ID *");
        ImGui::SetNextItemWidth(field_w);
        ImGui::InputTextWithHint("##add_id", "e.g., my-server", add_id_, sizeof(add_id_));

        ImGui::Text("Display Name");
        ImGui::SetNextItemWidth(field_w);
        ImGui::InputText("##add_name", add_name_, sizeof(add_name_));

        ImGui::Text("Command *");
        ImGui::SetNextItemWidth(field_w);
        ImGui::InputTextWithHint("##add_cmd", "e.g., npx, uvx, node", add_cmd_, sizeof(add_cmd_));

        ImGui::Text("Arguments (space-separated)");
        ImGui::SetNextItemWidth(field_w);
        ImGui::InputTextWithHint("##add_args", "e.g., -y @scope/server", add_args_, sizeof(add_args_));

        ImGui::Text("Description");
        ImGui::SetNextItemWidth(field_w);
        ImGui::InputText("##add_desc", add_desc_, sizeof(add_desc_));

        ImGui::Text("Category");
        ImGui::SetNextItemWidth(120);
        ImGui::Combo("##add_cat", &add_category_, CATEGORY_LABELS, CATEGORY_COUNT);
        ImGui::SameLine();
        ImGui::Checkbox("Auto-start", &add_auto_start_);

        // Env vars
        ImGui::Separator();
        ImGui::Text("Environment Variables");
        ImGui::SetNextItemWidth(120);
        ImGui::InputTextWithHint("##env_key", "Key", add_env_key_, sizeof(add_env_key_));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(200);
        ImGui::InputTextWithHint("##env_val", "Value", add_env_val_, sizeof(add_env_val_));
        ImGui::SameLine();
        if (ImGui::SmallButton("+ Add") && std::strlen(add_env_key_) > 0) {
            add_env_pairs_.emplace_back(add_env_key_, add_env_val_);
            std::memset(add_env_key_, 0, sizeof(add_env_key_));
            std::memset(add_env_val_, 0, sizeof(add_env_val_));
        }

        for (int i = 0; i < (int)add_env_pairs_.size(); i++) {
            ImGui::TextColored(TEXT_DIM, "  %s = %s",
                add_env_pairs_[i].first.c_str(), add_env_pairs_[i].second.c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton(("x##env" + std::to_string(i)).c_str())) {
                add_env_pairs_.erase(add_env_pairs_.begin() + i);
                i--;
            }
        }

        ImGui::Spacing();
        ImGui::Separator();

        // Action buttons
        ImGui::PushStyleColor(ImGuiCol_Button, ACCENT);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1));
        if (ImGui::Button("Add Server", ImVec2(120, 28))) {
            add_custom_server();
        }
        ImGui::PopStyleColor(2);

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(80, 28))) {
            show_add_modal_ = false;
        }

        if (!error_msg_.empty()) {
            ImGui::TextColored(ERROR_RED, "%s", error_msg_.c_str());
        }
    }
    ImGui::End();
    ImGui::PopStyleColor();
}

// ============================================================================
// Status bar
// ============================================================================

void MCPServersScreen::render_status_bar(float w) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARKEST);
    ImGui::BeginChild("##mcp_status", ImVec2(w, 24), false);
    ImGui::SetCursorPos(ImVec2(8, 4));

    if (!error_msg_.empty()) {
        ImGui::TextColored(ERROR_RED, "%s", error_msg_.c_str());
    } else if (!status_msg_.empty()) {
        ImGui::TextColored(SUCCESS, "%s", status_msg_.c_str());
    } else {
        ImGui::TextColored(TEXT_DIM, "MCP Server Management");
    }

    auto all_servers = mcp::MCPManager::instance().get_servers();
    auto all_unified = mcp::MCPService::instance().get_all_tools();
    int ext_count = 0;
    for (auto& t : all_unified) { if (!t.is_internal) ext_count++; }
    ImGui::SameLine(w - 300);
    ImGui::TextColored(TEXT_DIM, "%d servers | %d external tools | MCP Protocol v1.0",
        (int)all_servers.size(), ext_count);

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

} // namespace fincept::mcp_servers
