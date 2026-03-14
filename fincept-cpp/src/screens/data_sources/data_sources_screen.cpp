// Data Sources Screen — Bloomberg Terminal-inspired connector gallery
// Dense, data-focused layout with category sidebar, command bar, table views

#include "data_sources_screen.h"
#include "ui/theme.h"
#include "ui/yoga_helpers.h"
#include "core/logger.h"
#include <imgui.h>
#include <cstring>
#include <cstdio>
#include <algorithm>
#include <chrono>
#include <nlohmann/json.hpp>

namespace fincept::data_sources {

using json = nlohmann::json;
using namespace theme::colors;

// Bloomberg-style colors
static constexpr ImVec4 BB_BG          = {0.055f, 0.055f, 0.060f, 1.0f};  // near black
static constexpr ImVec4 BB_PANEL       = {0.075f, 0.075f, 0.082f, 1.0f};  // panel bg
static constexpr ImVec4 BB_SIDEBAR     = {0.062f, 0.062f, 0.068f, 1.0f};  // sidebar bg
static constexpr ImVec4 BB_ROW_ALT     = {0.082f, 0.082f, 0.090f, 1.0f};  // alternate row
static constexpr ImVec4 BB_HEADER_BG   = {0.090f, 0.090f, 0.098f, 1.0f};  // table header
static constexpr ImVec4 BB_SELECTED    = {1.0f, 0.45f, 0.05f, 0.12f};     // selected row
static constexpr ImVec4 BB_CMD_BG      = {0.067f, 0.067f, 0.073f, 1.0f};  // command bar bg
static constexpr ImVec4 BB_AMBER       = {1.0f, 0.65f, 0.0f, 1.0f};       // bloomberg amber
static constexpr ImVec4 BB_GREEN       = {0.0f, 0.85f, 0.35f, 1.0f};      // connected
static constexpr ImVec4 BB_RED         = {1.0f, 0.25f, 0.20f, 1.0f};      // error
static constexpr ImVec4 BB_CYAN        = {0.0f, 0.75f, 0.85f, 1.0f};      // info highlight
static constexpr ImVec4 BB_WHITE       = {0.92f, 0.92f, 0.93f, 1.0f};     // primary text
static constexpr ImVec4 BB_GRAY        = {0.50f, 0.50f, 0.54f, 1.0f};     // secondary text
static constexpr ImVec4 BB_DIM         = {0.35f, 0.35f, 0.38f, 1.0f};     // dim text
static constexpr ImVec4 BB_BORDER      = {0.16f, 0.16f, 0.18f, 1.0f};     // subtle borders

// ============================================================================
// Emoji icon map — exact same emoji as the Tauri desktop app
// Requires IMGUI_USE_WCHAR32 + IMGUI_ENABLE_FREETYPE + seguiemj.ttf merge
// ============================================================================
static const char* get_source_symbol(const char* type) {
    struct Entry { const char* key; const char* emoji; };
    static const Entry map[] = {
        // Relational Databases
        {"postgresql",      "\xf0\x9f\x90\x98"},  // 🐘
        {"mysql",           "\xf0\x9f\x90\xac"},  // 🐬
        {"mariadb",         "\xf0\x9f\xa6\xad"},  // 🦭
        {"sqlite",          "\xf0\x9f\x92\xbe"},  // 💾
        {"oracle",          "\xf0\x9f\x94\xb4"},  // 🔴
        {"mssql",           "\xf0\x9f\xaa\x9f"},  // 🪟
        {"cockroachdb",     "\xf0\x9f\xaa\xb3"},  // 🪳
        {"snowflake",       "\xe2\x9d\x84\xef\xb8\x8f"},  // ❄️
        {"vertica",         "\xf0\x9f\x94\xb7"},  // 🔷
        // NoSQL
        {"mongodb",         "\xf0\x9f\x8d\x83"},  // 🍃
        {"redis",           "\xf0\x9f\x94\xb4"},  // 🔴
        {"cassandra",       "\xf0\x9f\x94\xb8"},  // 🔸
        {"couchdb",         "\xf0\x9f\x9b\x8b\xef\xb8\x8f"},  // 🛋️
        {"dynamodb",        "\xe2\x9a\xa1"},       // ⚡
        {"neo4j",           "\xf0\x9f\x95\xb8\xef\xb8\x8f"},  // 🕸️
        {"arangodb",        "\xf0\x9f\xa5\x91"},  // 🥑
        {"memcached",       "\xf0\x9f\xa7\xa0"},  // 🧠
        {"rethinkdb",       "\xf0\x9f\x94\x84"},  // 🔄
        {"firestore",       "\xf0\x9f\x94\xa5"},  // 🔥
        {"hbase",           "\xf0\x9f\x90\x9d"},  // 🐝
        {"scylladb",        "\xf0\x9f\xa6\x82"},  // 🦂
        {"orientdb",        "\xf0\x9f\x8c\x90"},  // 🌐
        {"cosmosdb",        "\xf0\x9f\x8c\x8d"},  // 🌍
        {"etcd",            "\xf0\x9f\x94\x90"},  // 🔐
        // Time Series
        {"influxdb",        "\xf0\x9f\x93\x88"},  // 📈
        {"questdb",         "\xf0\x9f\x93\x8a"},  // 📊
        {"timescaledb",     "\xf0\x9f\x93\x89"},  // 📉
        {"prometheus",      "\xf0\x9f\x94\xa5"},  // 🔥
        {"victoriametrics", "\xf0\x9f\x93\x8a"},  // 📊
        {"opentsdb",        "\xf0\x9f\x93\x88"},  // 📈
        {"kdb",             "\xf0\x9f\x8e\xaf"},  // 🎯
        {"clickhouse",      "\xf0\x9f\x96\xb1\xef\xb8\x8f"},  // 🖱️
        // File
        {"csv",             "\xf0\x9f\x93\x84"},  // 📄
        {"excel",           "\xf0\x9f\x93\x8a"},  // 📊
        {"json",            "\xf0\x9f\x93\x8b"},  // 📋
        {"parquet",         "\xf0\x9f\x93\xa6"},  // 📦
        {"xml",             "\xf0\x9f\x93\x9d"},  // 📝
        {"avro",            "\xf0\x9f\x94\xb7"},  // 🔷
        {"orc",             "\xf0\x9f\x9f\xa6"},  // 🟦
        {"feather",         "\xf0\x9f\xaa\xb6"},  // 🪶
        {"ftp",             "\xf0\x9f\x93\x81"},  // 📁
        {"sftp",            "\xf0\x9f\x94\x92"},  // 🔒
        // API
        {"rest",            "\xf0\x9f\x8c\x90"},  // 🌐
        {"graphql",         "\xe2\x97\x86"},       // ◆
        {"grpc",            "\xf0\x9f\x94\x8c"},  // 🔌
        {"soap",            "\xf0\x9f\xa7\xbc"},  // 🧼
        {"odata",           "\xf0\x9f\x94\x97"},  // 🔗
        {"msgraph",         "\xf0\x9f\x94\x97"},  // 🔗
        // Streaming
        {"websocket",       "\xf0\x9f\x94\x8c"},  // 🔌
        {"mqtt",            "\xf0\x9f\x93\xa1"},  // 📡
        {"kafka",           "\xf0\x9f\x8c\x8a"},  // 🌊
        {"rabbitmq",        "\xf0\x9f\x90\xb0"},  // 🐰
        {"nats",            "\xf0\x9f\x93\xae"},  // 📮
        // Cloud
        {"s3",              "\xe2\x98\x81\xef\xb8\x8f"},  // ☁️
        {"gcp-storage",     "\xe2\x98\x81\xef\xb8\x8f"},  // ☁️
        {"azure-blob",      "\xe2\x98\x81\xef\xb8\x8f"},  // ☁️
        {"do-spaces",       "\xf0\x9f\x8c\x8a"},  // 🌊
        {"minio",           "\xf0\x9f\x97\x84\xef\xb8\x8f"},  // 🗄️
        {"backblaze-b2",    "\xf0\x9f\x92\xbe"},  // 💾
        {"wasabi",          "\xf0\x9f\xa5\xac"},  // 🥬
        {"cloudflare-r2",   "\xf0\x9f\x94\xb6"},  // 🔶
        {"oracle-cloud-storage", "\xf0\x9f\x94\xb4"},  // 🔴
        {"ibm-cloud-storage",    "\xf0\x9f\x94\xb5"},  // 🔵
        // Market Data
        {"yahoo-finance",   "\xf0\x9f\x93\x88"},  // 📈
        {"alpha-vantage",   "\xf0\x9f\x93\x8a"},  // 📊
        {"finnhub",         "\xf0\x9f\x93\x89"},  // 📉
        {"iex-cloud",       "\xf0\x9f\x92\xb9"},  // 💹
        {"twelve-data",     "\xf0\x9f\x93\x8a"},  // 📊
        {"quandl",          "\xf0\x9f\x93\x9a"},  // 📚
        {"binance",         "\xe2\x82\xbf"},       // ₿
        {"coinbase",        "\xf0\x9f\xaa\x99"},  // 🪙
        {"kraken",          "\xf0\x9f\x90\x99"},  // 🐙
        {"coinmarketcap",   "\xf0\x9f\x92\xb0"},  // 💰
        {"coingecko",       "\xf0\x9f\xa6\x8e"},  // 🦎
        {"eod-historical",  "\xf0\x9f\x93\x9c"},  // 📜
        {"tiingo",          "\xf0\x9f\x94\x94"},  // 🔔
        {"intrinio",        "\xf0\x9f\x8f\xa2"},  // 🏢
        {"fincept-data",    "\xf0\x9f\x93\xb0"},  // 📰
        {"reuters",         "\xf0\x9f\x93\xa1"},  // 📡
        {"marketstack",     "\xf0\x9f\x8f\xa6"},  // 🏦
        {"finage",          "\xf0\x9f\x92\xb5"},  // 💵
        {"tradier",         "\xf0\x9f\x8e\xaf"},  // 🎯
        // Search & Analytics
        {"elasticsearch",   "\xf0\x9f\x94\x8d"},  // 🔍
        {"opensearch",      "\xf0\x9f\x94\x8e"},  // 🔎
        {"solr",            "\xe2\x98\x80\xef\xb8\x8f"},  // ☀️
        {"algolia",         "\xf0\x9f\x94\xb7"},  // 🔷
        {"meilisearch",     "\xf0\x9f\x94\x8d"},  // 🔍
        // Data Warehouses
        {"bigquery",        "\xf0\x9f\x93\x8a"},  // 📊
        {"redshift",        "\xf0\x9f\x94\xb4"},  // 🔴
        {"databricks",      "\xf0\x9f\xa7\xb1"},  // 🧱
        {"azure-synapse",   "\xf0\x9f\x94\xb7"},  // 🔷
    };

    std::string t(type);
    for (auto& e : map) {
        if (t == e.key) return e.emoji;
    }
    return "\xf0\x9f\x94\xb7";  // 🔷 default
}

// Category stats helper
struct CatStats { int total; int connected; };

static CatStats count_category(const std::vector<DataSourceDef>& defs,
                                const std::vector<db::DataSource>& conns,
                                int cat_filter) {
    CatStats s{0, 0};
    for (auto& d : defs) {
        if (cat_filter >= 0 && static_cast<int>(d.category) != cat_filter) continue;
        s.total++;
        for (auto& c : conns) {
            if (c.type == std::string(d.type)) { s.connected++; break; }
        }
    }
    return s;
}

// ============================================================================
// Init
// ============================================================================
void DataSourcesScreen::init() {
    if (initialized_) return;
    all_defs_ = get_all_data_source_defs();
    load_connections();
    initialized_ = true;
    LOG_INFO("DataSources", "Initialized with %zu sources", all_defs_.size());
}

void DataSourcesScreen::load_connections() {
    connections_ = db::ops::get_all_data_sources();
}

// ============================================================================
// Main render — Bloomberg-style three-panel layout
// ============================================================================
void DataSourcesScreen::render() {
    init();

    // Poll async test futures
    if (testing_ && test_future_.valid()) {
        if (test_future_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
            test_result_ = test_future_.get();
            testing_ = false;
        }
    }
    if (!testing_conn_id_.empty() && conn_test_future_.valid()) {
        if (conn_test_future_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
            conn_test_results_[testing_conn_id_] = conn_test_future_.get();
            testing_conn_id_.clear();
        }
    }

    ui::ScreenFrame frame("##data_sources_screen", ImVec2(0, 0), BB_BG);
    if (!frame.begin()) { frame.end(); return; }

    float W = frame.width();
    float H = frame.height();

    // Vertical stack: command bar (32) + orange line (2) + body (flex) + status bar (22)
    auto vstack = ui::vstack_layout(W, H, {32, 2, -1, 22});

    // ── Command bar ──
    render_command_bar(W, vstack.heights[0]);

    // ── Orange accent line ──
    {
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled(
            p, ImVec2(p.x + W, p.y + 2),
            IM_COL32(255, 140, 0, 255));
        ImGui::Dummy(ImVec2(W, 2));
    }

    // ── Body: sidebar + main table ──
    float body_h = vstack.heights[2];
    float sidebar_w = frame.is_compact() ? 0.0f : (frame.is_medium() ? 180.0f : 220.0f);

    ImGui::BeginChild("##ds_body", ImVec2(W, body_h), false, ImGuiWindowFlags_NoScrollbar);
    {
        if (sidebar_w > 0) {
            render_sidebar(sidebar_w, body_h);
            ImGui::SameLine(0, 0);
        }

        float main_w = W - sidebar_w;
        ImGui::BeginChild("##ds_main", ImVec2(main_w, body_h), false);
        {
            if (view_ == 0) {
                render_gallery_table(main_w, body_h);
            } else {
                render_connections_table(main_w, body_h);
            }
        }
        ImGui::EndChild();
    }
    ImGui::EndChild();

    // ── Status bar ──
    render_status_bar(W, vstack.heights[3]);

    frame.end();

    // Modal overlay
    if (show_modal_) render_config_modal();
}

// ============================================================================
// Command Bar — Bloomberg-style top bar with search + actions
// ============================================================================
void DataSourcesScreen::render_command_bar(float w, float h) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BB_CMD_BG);
    ImGui::BeginChild("##ds_cmdbar", ImVec2(w, h), false, ImGuiWindowFlags_NoScrollbar);

    float pad = 6.0f;
    ImGui::SetCursorPos(ImVec2(pad, (h - 20) * 0.5f));

    // Title
    ImGui::TextColored(BB_AMBER, "DATA SOURCES");
    ImGui::SameLine(0, 12);

    // Separator
    ImVec2 sp = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddRectFilled(
        ImVec2(sp.x, sp.y - 2), ImVec2(sp.x + 1, sp.y + 16),
        IM_COL32(80, 80, 88, 255));
    ImGui::Dummy(ImVec2(8, 0));
    ImGui::SameLine();

    // View toggle: GALLERY | CONNECTIONS
    auto view_btn = [&](const char* label, int idx, int count) {
        bool active = (view_ == idx);
        ImGui::PushStyleColor(ImGuiCol_Button, active ? ImVec4(BB_AMBER.x, BB_AMBER.y, BB_AMBER.z, 0.2f) : ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(BB_AMBER.x, BB_AMBER.y, BB_AMBER.z, 0.15f));
        ImGui::PushStyleColor(ImGuiCol_Text, active ? BB_AMBER : BB_GRAY);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 2));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);

        char buf[64];
        std::snprintf(buf, sizeof(buf), "%s (%d)", label, count);
        if (ImGui::Button(buf, ImVec2(0, 20))) view_ = idx;

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(3);
        ImGui::SameLine(0, 2);
    };

    view_btn("GALLERY", 0, (int)all_defs_.size());
    view_btn("CONNECTIONS", 1, (int)connections_.size());

    ImGui::SameLine(0, 16);

    // Separator
    sp = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddRectFilled(
        ImVec2(sp.x, sp.y - 2), ImVec2(sp.x + 1, sp.y + 16),
        IM_COL32(80, 80, 88, 255));
    ImGui::Dummy(ImVec2(8, 0));
    ImGui::SameLine();

    // Search
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.04f, 0.04f, 0.05f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, BB_BORDER);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 2));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::SetNextItemWidth(200);
    ImGui::InputTextWithHint("##ds_cmd", "SEARCH <GO>", search_buf_, sizeof(search_buf_));
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);

    // Right-side: Refresh + count
    float right_w = 120;
    ImGui::SameLine(w - right_w);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(BB_AMBER.x, BB_AMBER.y, BB_AMBER.z, 0.15f));
    ImGui::PushStyleColor(ImGuiCol_Text, BB_GRAY);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 2));
    if (ImGui::Button("REFRESH", ImVec2(0, 20))) {
        initialized_ = false;
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Sidebar — Bloomberg-style category panel with counts
// ============================================================================
void DataSourcesScreen::render_sidebar(float w, float h) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BB_SIDEBAR);
    ImGui::PushStyleColor(ImGuiCol_Border, BB_BORDER);
    ImGui::BeginChild("##ds_sidebar", ImVec2(w, h), true, ImGuiWindowFlags_NoScrollbar);

    float pad = 8.0f;
    ImGui::SetCursorPos(ImVec2(pad, 8));
    ImGui::TextColored(BB_DIM, "CATEGORIES");
    ImGui::Spacing();

    // Draw each category as a dense row
    struct CatEntry { int val; const char* label; const char* code; };
    static const CatEntry cats[] = {
        {-1, "ALL SOURCES",    "ALL"},
        { 0, "DATABASE",       "DB "},
        { 1, "API",            "API"},
        { 2, "FILE",           "FIL"},
        { 3, "STREAMING",      "STR"},
        { 4, "CLOUD",          "CLD"},
        { 5, "TIMESERIES",     "TSR"},
        { 6, "MARKET DATA",    "MKT"},
    };

    for (auto& cat : cats) {
        bool active = (selected_category_ == cat.val);
        CatStats stats = count_category(all_defs_, connections_, cat.val);

        ImVec2 cursor = ImGui::GetCursorScreenPos();
        float row_h = 26.0f;
        float row_w = w - pad * 2;

        // Highlight bg for selected
        if (active) {
            ImGui::GetWindowDrawList()->AddRectFilled(
                ImVec2(cursor.x - 2, cursor.y),
                ImVec2(cursor.x + row_w + 2, cursor.y + row_h),
                ImGui::ColorConvertFloat4ToU32(BB_SELECTED));
            // Left accent bar
            ImGui::GetWindowDrawList()->AddRectFilled(
                ImVec2(cursor.x - 2, cursor.y),
                ImVec2(cursor.x + 2, cursor.y + row_h),
                IM_COL32(255, 140, 0, 255));
        }

        ImGui::SetCursorPosX(pad + 6);

        // Code badge
        ImGui::TextColored(active ? BB_AMBER : BB_DIM, "%s", cat.code);
        ImGui::SameLine(0, 8);

        // Label
        ImGui::TextColored(active ? BB_WHITE : BB_GRAY, "%s", cat.label);

        // Right-aligned counts
        char count_str[32];
        std::snprintf(count_str, sizeof(count_str), "%d/%d", stats.connected, stats.total);
        float count_w = ImGui::CalcTextSize(count_str).x;
        ImGui::SameLine(w - pad - count_w - 8);
        ImGui::TextColored(stats.connected > 0 ? BB_GREEN : BB_DIM, "%s", count_str);

        // Invisible button for click detection
        ImGui::SetCursorScreenPos(cursor);
        char btn_id[32];
        std::snprintf(btn_id, sizeof(btn_id), "##cat_%d", cat.val);
        if (ImGui::InvisibleButton(btn_id, ImVec2(row_w, row_h))) {
            selected_category_ = cat.val;
        }
    }

    // Bottom: summary
    ImGui::SetCursorPosY(h - 60);
    ImGui::SetCursorPosX(pad);
    ImGui::PushStyleColor(ImGuiCol_Separator, BB_BORDER);
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Spacing();
    ImGui::SetCursorPosX(pad);
    ImGui::TextColored(BB_DIM, "TOTAL");
    ImGui::SetCursorPosX(pad);
    ImGui::TextColored(BB_AMBER, "%zu", all_defs_.size());
    ImGui::SameLine(0, 4);
    ImGui::TextColored(BB_DIM, "sources");
    ImGui::SameLine(0, 8);
    ImGui::TextColored(BB_GREEN, "%zu", connections_.size());
    ImGui::SameLine(0, 4);
    ImGui::TextColored(BB_DIM, "active");

    ImGui::EndChild();
    ImGui::PopStyleColor(2);
}

// ============================================================================
// Gallery Table — Bloomberg-style dense table of data sources
// ============================================================================
void DataSourcesScreen::render_gallery_table(float w, float h) {
    std::string search_lower;
    for (char c : std::string(search_buf_)) search_lower += (char)std::tolower(c);

    // Build filtered list
    std::vector<const DataSourceDef*> filtered;
    for (auto& def : all_defs_) {
        if (selected_category_ >= 0 && static_cast<int>(def.category) != selected_category_) continue;
        if (!search_lower.empty()) {
            std::string name_l, desc_l;
            for (char c : std::string(def.name)) name_l += (char)std::tolower(c);
            for (char c : std::string(def.description)) desc_l += (char)std::tolower(c);
            if (name_l.find(search_lower) == std::string::npos &&
                desc_l.find(search_lower) == std::string::npos) continue;
        }
        filtered.push_back(&def);
    }

    ImGuiTableFlags flags =
        ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_Resizable |
        ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_PadOuterX;

    ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, BB_HEADER_BG);
    ImGui::PushStyleColor(ImGuiCol_TableBorderLight, BB_BORDER);
    ImGui::PushStyleColor(ImGuiCol_TableRowBg, BB_PANEL);
    ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, BB_ROW_ALT);
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(8, 4));

    if (ImGui::BeginTable("##ds_gallery_tbl", 7, flags, ImVec2(w, h))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("",         ImGuiTableColumnFlags_WidthFixed, 30);   // icon
        ImGui::TableSetupColumn("SOURCE",   ImGuiTableColumnFlags_WidthStretch, 3.0f);
        ImGui::TableSetupColumn("TYPE",     ImGuiTableColumnFlags_WidthFixed, 90);
        ImGui::TableSetupColumn("AUTH",     ImGuiTableColumnFlags_WidthFixed, 50);
        ImGui::TableSetupColumn("TEST",     ImGuiTableColumnFlags_WidthFixed, 50);
        ImGui::TableSetupColumn("STATUS",   ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("",         ImGuiTableColumnFlags_WidthFixed, 90);   // action

        // Header
        ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
        for (int col = 0; col < 7; col++) {
            ImGui::TableSetColumnIndex(col);
            const char* name = ImGui::TableGetColumnName(col);
            ImGui::PushStyleColor(ImGuiCol_Text, BB_AMBER);
            ImGui::TableHeader(name);
            ImGui::PopStyleColor();
        }

        // Rows
        int row_idx = 0;
        for (auto* def : filtered) {
            ImGui::TableNextRow();
            ImGui::PushID(row_idx);

            // Check if this source has a connection
            bool has_conn = false;
            for (auto& c : connections_) {
                if (c.type == std::string(def->type)) { has_conn = true; break; }
            }

            // Icon — Unicode symbol from lookup map
            ImGui::TableSetColumnIndex(0);
            ImGui::PushStyleColor(ImGuiCol_Text, BB_CYAN);
            ImGui::Text("%s", get_source_symbol(def->type));
            ImGui::PopStyleColor();

            // Source name + description
            ImGui::TableSetColumnIndex(1);
            ImGui::TextColored(BB_WHITE, "%s", def->name);
            ImGui::SameLine(0, 8);
            // Truncate description
            std::string desc = def->description;
            if (desc.size() > 60) desc = desc.substr(0, 57) + "...";
            ImGui::TextColored(BB_DIM, "%s", desc.c_str());

            // Type badge
            ImGui::TableSetColumnIndex(2);
            ImVec4 type_col = BB_CYAN;
            if (def->category == Category::Database)   type_col = ImVec4(0.6f, 0.4f, 1.0f, 1.0f);
            if (def->category == Category::Streaming)  type_col = ImVec4(1.0f, 0.6f, 0.2f, 1.0f);
            if (def->category == Category::MarketData) type_col = BB_GREEN;
            if (def->category == Category::Cloud)      type_col = ImVec4(0.4f, 0.7f, 1.0f, 1.0f);
            ImGui::TextColored(type_col, "%s", category_label(def->category));

            // Auth required
            ImGui::TableSetColumnIndex(3);
            if (def->requires_auth) {
                ImGui::TextColored(BB_AMBER, "KEY");
            } else {
                ImGui::TextColored(BB_DIM, "--");
            }

            // Testable
            ImGui::TableSetColumnIndex(4);
            if (def->testable) {
                ImGui::TextColored(BB_GREEN, "YES");
            } else {
                ImGui::TextColored(BB_DIM, "--");
            }

            // Status
            ImGui::TableSetColumnIndex(5);
            if (has_conn) {
                ImGui::TextColored(BB_GREEN, "CONNECTED");
            } else {
                ImGui::TextColored(BB_DIM, "AVAILABLE");
            }

            // Action button
            ImGui::TableSetColumnIndex(6);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(BB_AMBER.x, BB_AMBER.y, BB_AMBER.z, 0.15f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(BB_AMBER.x, BB_AMBER.y, BB_AMBER.z, 0.3f));
            ImGui::PushStyleColor(ImGuiCol_Text, BB_AMBER);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 1));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);
            char cfg_id[64];
            std::snprintf(cfg_id, sizeof(cfg_id), "CONNECT##gc_%d", row_idx);
            if (ImGui::SmallButton(cfg_id)) {
                open_config(*def);
            }
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(3);

            ImGui::PopID();
            row_idx++;
        }

        ImGui::EndTable();
    }

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);
}

// ============================================================================
// Connections Table — Bloomberg-style table of active connections
// ============================================================================
void DataSourcesScreen::render_connections_table(float w, float h) {
    if (connections_.empty()) {
        ImGui::SetCursorPos(ImVec2(w * 0.5f - 120, h * 0.4f));
        ImGui::TextColored(BB_DIM, "NO ACTIVE CONNECTIONS");
        ImGui::SetCursorPosX(w * 0.5f - 80);
        ImGui::Spacing();
        ImGui::SetCursorPosX(w * 0.5f - 80);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(BB_AMBER.x, BB_AMBER.y, BB_AMBER.z, 0.15f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(BB_AMBER.x, BB_AMBER.y, BB_AMBER.z, 0.3f));
        ImGui::PushStyleColor(ImGuiCol_Text, BB_AMBER);
        if (ImGui::Button("BROWSE SOURCES", ImVec2(160, 24))) view_ = 0;
        ImGui::PopStyleColor(3);
        return;
    }

    ImGuiTableFlags flags =
        ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_Resizable |
        ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_PadOuterX;

    ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, BB_HEADER_BG);
    ImGui::PushStyleColor(ImGuiCol_TableBorderLight, BB_BORDER);
    ImGui::PushStyleColor(ImGuiCol_TableRowBg, BB_PANEL);
    ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, BB_ROW_ALT);
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(8, 5));

    if (ImGui::BeginTable("##ds_conn_tbl", 9, flags, ImVec2(w, h))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("",         ImGuiTableColumnFlags_WidthFixed, 20);   // status dot
        ImGui::TableSetupColumn("",         ImGuiTableColumnFlags_WidthFixed, 24);   // icon
        ImGui::TableSetupColumn("NAME",     ImGuiTableColumnFlags_WidthStretch, 2.5f);
        ImGui::TableSetupColumn("PROVIDER", ImGuiTableColumnFlags_WidthStretch, 2.0f);
        ImGui::TableSetupColumn("TYPE",     ImGuiTableColumnFlags_WidthFixed, 90);
        ImGui::TableSetupColumn("STATE",    ImGuiTableColumnFlags_WidthFixed, 70);
        ImGui::TableSetupColumn("LATENCY",  ImGuiTableColumnFlags_WidthFixed, 70);
        ImGui::TableSetupColumn("CREATED",  ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("ACTIONS",  ImGuiTableColumnFlags_WidthFixed, 160);

        // Header
        ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
        for (int col = 0; col < 9; col++) {
            ImGui::TableSetColumnIndex(col);
            ImGui::PushStyleColor(ImGuiCol_Text, BB_AMBER);
            ImGui::TableHeader(ImGui::TableGetColumnName(col));
            ImGui::PopStyleColor();
        }

        int row = 0;
        for (auto& conn : connections_) {
            ImGui::TableNextRow();
            ImGui::PushID(row);

            // Status dot
            ImGui::TableSetColumnIndex(0);
            ImVec4 dot_col = conn.enabled ? BB_GREEN : BB_RED;
            ImVec2 dot_p = ImGui::GetCursorScreenPos();
            dot_p.x += 8;
            dot_p.y += 7;
            ImGui::GetWindowDrawList()->AddCircleFilled(dot_p, 4.0f,
                ImGui::ColorConvertFloat4ToU32(dot_col));
            ImGui::Dummy(ImVec2(20, 14));

            // Icon symbol
            ImGui::TableSetColumnIndex(1);
            ImGui::TextColored(BB_CYAN, "%s", get_source_symbol(conn.type.c_str()));

            // Name
            ImGui::TableSetColumnIndex(2);
            ImGui::TextColored(BB_WHITE, "%s", conn.display_name.c_str());

            // Provider
            ImGui::TableSetColumnIndex(3);
            ImGui::TextColored(BB_CYAN, "%s", conn.provider.c_str());

            // Type
            ImGui::TableSetColumnIndex(4);
            ImGui::TextColored(BB_GRAY, "%s", conn.type.c_str());

            // State
            ImGui::TableSetColumnIndex(5);
            if (conn.enabled) {
                ImGui::TextColored(BB_GREEN, "ACTIVE");
            } else {
                ImGui::TextColored(BB_RED, "INACTIVE");
            }

            // Latency / Test result
            ImGui::TableSetColumnIndex(6);
            auto tr_it = conn_test_results_.find(conn.id);
            if (testing_conn_id_ == conn.id) {
                ImGui::TextColored(BB_AMBER, "...");
            } else if (tr_it != conn_test_results_.end()) {
                auto& tr = tr_it->second;
                ImGui::TextColored(tr.success ? BB_GREEN : BB_RED, "%.0fms", tr.elapsed_ms);
            } else {
                ImGui::TextColored(BB_DIM, "--");
            }

            // Created
            ImGui::TableSetColumnIndex(7);
            std::string date = conn.created_at;
            if (date.size() > 10) date = date.substr(0, 10);
            ImGui::TextColored(BB_DIM, "%s", date.c_str());

            // Actions
            ImGui::TableSetColumnIndex(8);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 1));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);

            // Test
            bool is_testing_this = (testing_conn_id_ == conn.id);
            if (!is_testing_this) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(BB_GREEN.x, BB_GREEN.y, BB_GREEN.z, 0.15f));
                ImGui::PushStyleColor(ImGuiCol_Text, BB_GREEN);
                char test_id[64];
                std::snprintf(test_id, sizeof(test_id), "TEST##ct_%d", row);
                if (ImGui::SmallButton(test_id)) {
                    std::map<std::string, std::string> cfg;
                    try {
                        auto j = json::parse(conn.config);
                        for (auto& [k, v] : j.items())
                            cfg[k] = v.is_string() ? v.get<std::string>() : v.dump();
                    } catch (...) {}
                    testing_conn_id_ = conn.id;
                    conn_test_future_ = adapters::AdapterRegistry::instance().test_connection_async(conn.type, cfg);
                }
                ImGui::PopStyleColor(3);
            } else {
                ImGui::TextColored(BB_AMBER, "...");
            }
            ImGui::SameLine(0, 4);

            // Edit
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(BB_CYAN.x, BB_CYAN.y, BB_CYAN.z, 0.15f));
            ImGui::PushStyleColor(ImGuiCol_Text, BB_CYAN);
            char edit_id[64];
            std::snprintf(edit_id, sizeof(edit_id), "EDIT##ce_%d", row);
            if (ImGui::SmallButton(edit_id)) {
                for (auto& def : all_defs_) {
                    if (std::string(def.type) == conn.type) {
                        editing_id_ = conn.id;
                        modal_def_ = &def;
                        show_modal_ = true;
                        form_data_.clear();
                        form_data_["name"] = conn.display_name;
                        try {
                            auto cfg = json::parse(conn.config);
                            for (auto& [k, v] : cfg.items())
                                form_data_[k] = v.is_string() ? v.get<std::string>() : v.dump();
                        } catch (...) {}
                        break;
                    }
                }
            }
            ImGui::PopStyleColor(3);
            ImGui::SameLine(0, 4);

            // Delete
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(BB_RED.x, BB_RED.y, BB_RED.z, 0.15f));
            ImGui::PushStyleColor(ImGuiCol_Text, BB_RED);
            char del_id[64];
            std::snprintf(del_id, sizeof(del_id), "DEL##cd_%d", row);
            if (ImGui::SmallButton(del_id)) {
                delete_connection(conn.id);
            }
            ImGui::PopStyleColor(3);

            ImGui::PopStyleVar(2);
            ImGui::PopID();
            row++;
        }

        ImGui::EndTable();
    }

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);
}

// ============================================================================
// Status Bar — Bloomberg-style bottom ticker bar
// ============================================================================
void DataSourcesScreen::render_status_bar(float w, float h) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.04f, 0.04f, 0.045f, 1.0f));
    ImGui::BeginChild("##ds_status", ImVec2(w, h), false, ImGuiWindowFlags_NoScrollbar);

    ImGui::SetCursorPos(ImVec2(8, (h - 14) * 0.5f));

    // Left: counts
    CatStats total = count_category(all_defs_, connections_, -1);
    ImGui::TextColored(BB_DIM, "SOURCES:");
    ImGui::SameLine(0, 4);
    ImGui::TextColored(BB_AMBER, "%d", total.total);
    ImGui::SameLine(0, 12);

    ImGui::TextColored(BB_DIM, "CONNECTED:");
    ImGui::SameLine(0, 4);
    ImGui::TextColored(BB_GREEN, "%d", total.connected);
    ImGui::SameLine(0, 12);

    ImGui::TextColored(BB_DIM, "AVAILABLE:");
    ImGui::SameLine(0, 4);
    ImGui::TextColored(BB_CYAN, "%d", total.total - total.connected);
    ImGui::SameLine(0, 16);

    ImGui::TextColored(BB_BORDER, "|");
    ImGui::SameLine(0, 16);

    ImGui::TextColored(BB_DIM, "VIEW:");
    ImGui::SameLine(0, 4);
    ImGui::TextColored(BB_AMBER, "%s", view_ == 0 ? "GALLERY" : "CONNECTIONS");

    if (selected_category_ >= 0) {
        ImGui::SameLine(0, 16);
        ImGui::TextColored(BB_DIM, "FILTER:");
        ImGui::SameLine(0, 4);
        ImGui::TextColored(BB_CYAN, "%s", category_label(static_cast<Category>(selected_category_)));
    }

    // Right: timestamp
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    char ts[32];
    std::strftime(ts, sizeof(ts), "%H:%M:%S", t);
    float ts_w = ImGui::CalcTextSize(ts).x + 12;
    ImGui::SameLine(w - ts_w);
    ImGui::TextColored(BB_DIM, "%s", ts);

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Config modal — Bloomberg-style form overlay
// ============================================================================
void DataSourcesScreen::open_config(const DataSourceDef& def) {
    modal_def_ = &def;
    editing_id_.clear();
    save_error_.clear();
    form_data_.clear();
    form_data_["name"] = std::string("My ") + def.name;
    for (auto& field : def.fields) {
        if (field.default_value && field.default_value[0] != '\0') {
            form_data_[field.name] = field.default_value;
        }
    }
    show_modal_ = true;
}

void DataSourcesScreen::render_config_modal() {
    if (!modal_def_) return;

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImVec2 center = ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f,
                           vp->WorkPos.y + vp->WorkSize.y * 0.5f);
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(480, 0));

    ImGui::PushStyleColor(ImGuiCol_WindowBg, BB_PANEL);
    ImGui::PushStyleColor(ImGuiCol_Border, BB_AMBER);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 12));

    bool open = true;
    char title[128];
    std::snprintf(title, sizeof(title), "%s %s###ds_cfg_modal",
        editing_id_.empty() ? "CONFIGURE" : "EDIT", modal_def_->name);

    if (ImGui::Begin(title, &open,
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_AlwaysAutoResize)) {

        // Header
        ImGui::TextColored(BB_CYAN, "%s", get_source_symbol(modal_def_->type));
        ImGui::SameLine(0, 8);
        ImGui::TextColored(BB_WHITE, "%s", modal_def_->name);
        ImGui::SameLine(0, 8);
        ImGui::TextColored(BB_DIM, "- %s", category_label(modal_def_->category));

        ImGui::Spacing();
        ImGui::TextColored(BB_GRAY, "%s", modal_def_->description);

        ImGui::Spacing();
        ImVec2 sep_p = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled(
            sep_p, ImVec2(sep_p.x + 448, sep_p.y + 1),
            IM_COL32(255, 140, 0, 100));
        ImGui::Dummy(ImVec2(0, 4));

        // Error
        if (!save_error_.empty()) {
            ImGui::TextColored(BB_RED, "%s", save_error_.c_str());
            ImGui::Spacing();
        }

        // Connection name
        ImGui::TextColored(BB_AMBER, "CONNECTION NAME");
        auto& name_val = form_data_["name"];
        char name_buf[128];
        std::strncpy(name_buf, name_val.c_str(), sizeof(name_buf) - 1);
        name_buf[sizeof(name_buf) - 1] = '\0';
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.04f, 0.04f, 0.05f, 1.0f));
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("##ds_conn_name", name_buf, sizeof(name_buf))) {
            name_val = name_buf;
        }
        ImGui::PopStyleColor();
        ImGui::Spacing();

        // Dynamic fields
        for (auto& field : modal_def_->fields) {
            render_field(field, std::string(modal_def_->id));
        }

        ImGui::Spacing();
        sep_p = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled(
            sep_p, ImVec2(sep_p.x + 448, sep_p.y + 1),
            IM_COL32(255, 140, 0, 100));
        ImGui::Dummy(ImVec2(0, 6));

        // Test result
        if (test_result_.has_value()) {
            auto& r = test_result_.value();
            ImVec4 col = r.success ? BB_GREEN : BB_RED;
            ImGui::TextColored(col, "%s", r.success ? "CONNECTION OK" : "CONNECTION FAILED");
            ImGui::SameLine(0, 8);
            ImGui::TextColored(BB_DIM, "%s", r.message.c_str());
            if (r.elapsed_ms > 0) {
                ImGui::SameLine(0, 8);
                ImGui::TextColored(BB_DIM, "(%.0fms)", r.elapsed_ms);
            }
            ImGui::Spacing();
        }

        // Buttons
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);

        // Test
        if (testing_) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.25f, 0.05f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, BB_AMBER);
            ImGui::Button("TESTING...", ImVec2(120, 26));
            ImGui::PopStyleColor(2);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(BB_GREEN.x * 0.2f, BB_GREEN.y * 0.2f, BB_GREEN.z * 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(BB_GREEN.x * 0.3f, BB_GREEN.y * 0.3f, BB_GREEN.z * 0.3f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, BB_GREEN);
            if (ImGui::Button("TEST", ImVec2(120, 26))) {
                std::map<std::string, std::string> cfg;
                for (auto& [k, v] : form_data_) {
                    if (k != "name") cfg[k] = v;
                }
                test_result_.reset();
                testing_ = true;
                test_future_ = adapters::AdapterRegistry::instance().test_connection_async(
                    std::string(modal_def_->type), cfg);
            }
            ImGui::PopStyleColor(3);
        }
        ImGui::SameLine(0, 6);

        // Save
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(BB_AMBER.x * 0.3f, BB_AMBER.y * 0.3f, 0, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(BB_AMBER.x * 0.45f, BB_AMBER.y * 0.45f, 0, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, BB_AMBER);
        if (ImGui::Button(editing_id_.empty() ? "SAVE" : "UPDATE", ImVec2(120, 26))) {
            save_connection();
        }
        ImGui::PopStyleColor(3);
        ImGui::SameLine(0, 6);

        // Cancel
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.12f, 0.13f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.18f, 0.20f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, BB_GRAY);
        if (ImGui::Button("CANCEL", ImVec2(80, 26))) {
            show_modal_ = false;
            modal_def_ = nullptr;
            test_result_.reset();
            testing_ = false;
        }
        ImGui::PopStyleColor(3);

        ImGui::PopStyleVar();
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);

    if (!open) {
        show_modal_ = false;
        modal_def_ = nullptr;
        test_result_.reset();
        testing_ = false;
    }
}

void DataSourcesScreen::render_field(const FieldDef& field, const std::string& /*ds_id*/) {
    char label[128];
    std::snprintf(label, sizeof(label), "%s%s", field.label, field.required ? " *" : "");
    ImGui::TextColored(BB_AMBER, "%s", label);

    auto& val = form_data_[field.name];
    if (val.empty() && field.default_value && field.default_value[0] != '\0') {
        val = field.default_value;
    }

    char input_id[128];
    std::snprintf(input_id, sizeof(input_id), "##dsf_%s", field.name);

    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.04f, 0.04f, 0.05f, 1.0f));
    ImGui::SetNextItemWidth(-1);

    switch (field.type) {
        case FieldType::Text:
        case FieldType::Url:
        case FieldType::Number: {
            char buf[256];
            std::strncpy(buf, val.c_str(), sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';
            if (ImGui::InputTextWithHint(input_id, field.placeholder, buf, sizeof(buf))) {
                val = buf;
            }
            break;
        }
        case FieldType::Password: {
            char buf[256];
            std::strncpy(buf, val.c_str(), sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';
            if (ImGui::InputText(input_id, buf, sizeof(buf), ImGuiInputTextFlags_Password)) {
                val = buf;
            }
            break;
        }
        case FieldType::Textarea: {
            char buf[1024];
            std::strncpy(buf, val.c_str(), sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';
            if (ImGui::InputTextMultiline(input_id, buf, sizeof(buf), ImVec2(-1, 60))) {
                val = buf;
            }
            break;
        }
        case FieldType::Select: {
            if (ImGui::BeginCombo(input_id, val.c_str())) {
                for (auto& opt : field.options) {
                    bool selected = (val == opt.value);
                    if (ImGui::Selectable(opt.label, selected)) {
                        val = opt.value;
                    }
                }
                ImGui::EndCombo();
            }
            break;
        }
        case FieldType::Checkbox: {
            bool checked = (val == "true" || val == "1");
            if (ImGui::Checkbox(input_id, &checked)) {
                val = checked ? "true" : "false";
            }
            break;
        }
    }

    ImGui::PopStyleColor();
    ImGui::Spacing();
}

// ============================================================================
// Save / Delete
// ============================================================================
void DataSourcesScreen::save_connection() {
    if (!modal_def_) return;

    auto name_it = form_data_.find("name");
    if (name_it == form_data_.end() || name_it->second.empty()) {
        save_error_ = "Connection name is required";
        return;
    }

    json config;
    for (auto& [k, v] : form_data_) {
        if (k == "name") continue;
        config[k] = v;
    }

    db::DataSource ds;
    if (editing_id_.empty()) {
        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
        ds.id = "conn_" + std::to_string(ms);
    } else {
        ds.id = editing_id_;
    }
    ds.alias = std::string(modal_def_->type);
    ds.display_name = name_it->second;
    ds.description = std::string(modal_def_->description);
    ds.type = std::string(modal_def_->type);
    ds.provider = std::string(modal_def_->name);
    ds.category = category_id(modal_def_->category);
    ds.config = config.dump();
    ds.enabled = true;

    db::ops::save_data_source(ds);
    load_connections();

    show_modal_ = false;
    modal_def_ = nullptr;
    editing_id_.clear();
    save_error_.clear();
    view_ = 1;

    LOG_INFO("DataSources", "Saved connection: %s", ds.display_name.c_str());
}

void DataSourcesScreen::delete_connection(const std::string& id) {
    db::ops::delete_data_source(id);
    load_connections();
    LOG_INFO("DataSources", "Deleted connection: %s", id.c_str());
}

} // namespace fincept::data_sources
