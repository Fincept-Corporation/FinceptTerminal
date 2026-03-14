// Bloomberg Terminal Layout Demo — Yoga + ImGui
// Complex multi-panel layout with real-time data simulation

#include <yoga/YGNode.h>
#include <yoga/YGNodeStyle.h>
#include <yoga/YGNodeLayout.h>
#include <yoga/YGEnums.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>

#include <cstdio>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

// ============================================================================
// Bloomberg color palette
// ============================================================================
namespace colors {
    constexpr ImVec4 BG_DARKEST   = {0.047f, 0.047f, 0.051f, 1.0f};
    constexpr ImVec4 BG_DARK      = {0.075f, 0.075f, 0.082f, 1.0f};
    constexpr ImVec4 BG_PANEL     = {0.098f, 0.098f, 0.106f, 1.0f};
    constexpr ImVec4 BG_HEADER    = {0.067f, 0.067f, 0.075f, 1.0f};
    constexpr ImVec4 ORANGE       = {1.00f, 0.45f, 0.05f, 1.0f};
    constexpr ImVec4 GREEN        = {0.00f, 0.85f, 0.45f, 1.0f};
    constexpr ImVec4 RED          = {0.95f, 0.25f, 0.20f, 1.0f};
    constexpr ImVec4 CYAN         = {0.00f, 0.75f, 0.95f, 1.0f};
    constexpr ImVec4 YELLOW       = {0.95f, 0.85f, 0.15f, 1.0f};
    constexpr ImVec4 PURPLE       = {0.60f, 0.40f, 0.95f, 1.0f};
    constexpr ImVec4 WHITE        = {0.90f, 0.90f, 0.92f, 1.0f};
    constexpr ImVec4 DIM          = {0.40f, 0.40f, 0.43f, 1.0f};
    constexpr ImVec4 DIMMER       = {0.28f, 0.28f, 0.30f, 1.0f};
    constexpr ImVec4 BORDER       = {0.18f, 0.18f, 0.20f, 1.0f};
}

static ImU32 col32(ImVec4 c, float a = -1.0f) {
    if (a < 0) a = c.w;
    return IM_COL32((int)(c.x*255),(int)(c.y*255),(int)(c.z*255),(int)(a*255));
}

// ============================================================================
// Fake market data
// ============================================================================
struct Ticker {
    const char* symbol;
    const char* name;
    float price;
    float change_pct;
    float volume_m; // millions
};

static Ticker indices[] = {
    {"SPX",    "S&P 500",      5842.31f,  0.47f, 3821.4f},
    {"INDU",   "Dow Jones",   43274.56f,  0.31f, 892.1f},
    {"CCMP",   "Nasdaq",      18512.73f,  0.68f, 5234.7f},
    {"RTY",    "Russell 2000",  2287.14f, -0.23f, 1456.2f},
    {"UKX",    "FTSE 100",     8412.30f,  0.15f, 723.8f},
    {"DAX",    "DAX",         19876.45f,  0.52f, 612.3f},
    {"NKY",    "Nikkei 225",  38742.18f, -0.34f, 1893.5f},
    {"HSI",    "Hang Seng",   20134.67f,  1.12f, 2145.6f},
};

static Ticker currencies[] = {
    {"EUR/USD", "Euro",         1.0847f,  0.12f, 0},
    {"GBP/USD", "British Pound",1.2734f, -0.08f, 0},
    {"USD/JPY", "Japanese Yen", 153.42f,  0.23f, 0},
    {"USD/CHF", "Swiss Franc",  0.8812f, -0.15f, 0},
    {"AUD/USD", "Aussie Dollar",0.6523f,  0.31f, 0},
    {"USD/CAD", "Canadian Dollar",1.3678f, 0.07f, 0},
};

static Ticker commodities[] = {
    {"GC1",  "Gold",         2687.40f,  0.82f, 234.5f},
    {"SI1",  "Silver",         31.24f,  1.23f, 89.2f},
    {"CL1",  "Crude Oil",      71.83f, -0.45f, 567.8f},
    {"NG1",  "Natural Gas",     2.87f,  2.14f, 345.1f},
    {"HG1",  "Copper",          4.12f,  0.67f, 123.4f},
    {"W 1",  "Wheat",         587.25f, -0.92f, 78.3f},
};

struct OrderEntry {
    const char* time;
    const char* symbol;
    const char* side;
    int qty;
    float price;
    const char* status;
};

static OrderEntry orders[] = {
    {"14:32:18", "AAPL",  "BUY",  500,  198.45f, "FILLED"},
    {"14:31:05", "MSFT",  "SELL", 200,  425.12f, "FILLED"},
    {"14:30:42", "GOOGL", "BUY",  100, 176.83f, "PARTIAL"},
    {"14:29:15", "AMZN",  "BUY",  300,  195.67f, "FILLED"},
    {"14:28:03", "NVDA",  "SELL", 150,  875.23f, "WORKING"},
    {"14:27:51", "META",  "BUY",  400,  542.18f, "FILLED"},
    {"14:26:30", "TSLA",  "SELL", 250,  245.90f, "FILLED"},
    {"14:25:12", "JPM",   "BUY",  600,  198.34f, "WORKING"},
};

struct NewsItem {
    const char* time;
    const char* source;
    const char* headline;
    ImVec4 tag_color;
};

static NewsItem news[] = {
    {"14:32", "BBG",   "Fed Minutes Show Officials Divided on Rate Path",            colors::ORANGE},
    {"14:28", "RTRS",  "China PMI Beats Expectations, Manufacturing Expands",        colors::GREEN},
    {"14:25", "BBG",   "Treasury Yields Rise as Inflation Data Comes in Hot",        colors::RED},
    {"14:22", "CNBC",  "NVIDIA Announces Next-Gen AI Chip Architecture",             colors::CYAN},
    {"14:18", "FT",    "European Banks Face New Capital Requirements",               colors::YELLOW},
    {"14:15", "BBG",   "Oil Prices Dip on Surprise Inventory Build",                 colors::RED},
    {"14:12", "WSJ",   "Apple Vision Pro Sales Exceed Analyst Forecasts",            colors::GREEN},
    {"14:08", "RTRS",  "BOJ Governor Signals Potential Policy Shift in Q2",          colors::PURPLE},
    {"14:05", "BBG",   "Gold Hits Record as Central Banks Accelerate Buying",        colors::YELLOW},
    {"14:02", "FT",    "UK GDP Growth Surprises to the Upside at 0.6%",             colors::GREEN},
};

// Simulated price chart data
static float chart_data[120];
static bool chart_initialized = false;

static void init_chart() {
    if (chart_initialized) return;
    float base = 5800.0f;
    for (int i = 0; i < 120; i++) {
        base += sinf(i * 0.15f) * 8.0f + cosf(i * 0.07f) * 12.0f
              + sinf(i * 0.3f) * 3.0f + (float)(i % 7 - 3) * 1.5f;
        chart_data[i] = base;
    }
    chart_initialized = true;
}

// ============================================================================
// Panel rendering functions
// ============================================================================

static void panel_header(const char* title, ImVec4 accent = colors::ORANGE) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;

    // Header background
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + 22), col32(colors::BG_HEADER));
    // Accent left bar
    dl->AddRectFilled(p, ImVec2(p.x + 3, p.y + 22), col32(accent));
    // Title
    dl->AddText(ImVec2(p.x + 10, p.y + 3), col32(accent), title);

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 26);
}

static void render_ticker_table(Ticker* tickers, int count) {
    // Header row
    ImGui::TextColored(colors::DIM, "%-8s %12s %8s %10s", "SYMBOL", "PRICE", "CHG%", "VOL(M)");
    ImGui::Separator();

    for (int i = 0; i < count; i++) {
        Ticker& t = tickers[i];
        float anim_t = (float)ImGui::GetTime();
        // Subtle price animation
        float jitter = sinf(anim_t * (1.5f + i * 0.3f) + i * 2.0f) * t.price * 0.0002f;
        float display_price = t.price + jitter;
        float display_chg = t.change_pct + sinf(anim_t * 0.7f + i) * 0.02f;

        ImVec4 chg_col = display_chg >= 0 ? colors::GREEN : colors::RED;
        const char* arrow = display_chg >= 0 ? "+" : "";

        ImGui::TextColored(colors::WHITE, "%-8s", t.symbol);
        ImGui::SameLine(100);
        ImGui::TextColored(colors::WHITE, "%12.2f", display_price);
        ImGui::SameLine(210);
        ImGui::TextColored(chg_col, "%s%.2f%%", arrow, display_chg);
        if (t.volume_m > 0) {
            ImGui::SameLine(300);
            ImGui::TextColored(colors::DIM, "%8.1f", t.volume_m);
        }
    }
}

static void render_chart(float w, float h) {
    init_chart();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 origin = ImGui::GetCursorScreenPos();
    float anim_t = (float)ImGui::GetTime();

    // Chart area background
    dl->AddRectFilled(origin, ImVec2(origin.x + w, origin.y + h),
                      col32(colors::BG_DARK));

    // Find min/max for scaling
    float min_v = 99999, max_v = -99999;
    for (int i = 0; i < 120; i++) {
        float v = chart_data[i] + sinf(anim_t * 0.5f + i * 0.1f) * 3.0f;
        if (v < min_v) min_v = v;
        if (v > max_v) max_v = v;
    }
    float range = max_v - min_v;
    if (range < 1) range = 1;

    float pad_x = 4, pad_y = 8;
    float chart_w = w - pad_x * 2;
    float chart_h = h - pad_y * 2;

    // Horizontal grid lines
    for (int g = 0; g <= 4; g++) {
        float gy = origin.y + pad_y + chart_h * (1.0f - g / 4.0f);
        dl->AddLine(ImVec2(origin.x + pad_x, gy),
                    ImVec2(origin.x + w - pad_x, gy),
                    col32(colors::DIMMER, 0.3f), 1.0f);
        float val = min_v + range * (g / 4.0f);
        char buf[32];
        snprintf(buf, sizeof(buf), "%.0f", val);
        dl->AddText(ImVec2(origin.x + pad_x + 2, gy - 12), col32(colors::DIMMER), buf);
    }

    // Price line
    ImVec2 prev;
    for (int i = 0; i < 120; i++) {
        float v = chart_data[i] + sinf(anim_t * 0.5f + i * 0.1f) * 3.0f;
        float nx = origin.x + pad_x + (i / 119.0f) * chart_w;
        float ny = origin.y + pad_y + chart_h * (1.0f - (v - min_v) / range);
        ImVec2 cur(nx, ny);
        if (i > 0) {
            dl->AddLine(prev, cur, col32(colors::CYAN), 1.5f);
        }
        prev = cur;
    }

    // Fill under the line
    for (int i = 1; i < 120; i++) {
        float v0 = chart_data[i-1] + sinf(anim_t * 0.5f + (i-1) * 0.1f) * 3.0f;
        float v1 = chart_data[i]   + sinf(anim_t * 0.5f + i * 0.1f) * 3.0f;
        float x0 = origin.x + pad_x + ((i-1) / 119.0f) * chart_w;
        float x1 = origin.x + pad_x + (i / 119.0f) * chart_w;
        float y0 = origin.y + pad_y + chart_h * (1.0f - (v0 - min_v) / range);
        float y1 = origin.y + pad_y + chart_h * (1.0f - (v1 - min_v) / range);
        float bottom = origin.y + h - pad_y;
        dl->AddQuadFilled(ImVec2(x0, y0), ImVec2(x1, y1),
                          ImVec2(x1, bottom), ImVec2(x0, bottom),
                          col32(colors::CYAN, 0.06f));
    }

    // Current price label
    float last_price = chart_data[119] + sinf(anim_t * 0.5f + 119 * 0.1f) * 3.0f;
    char price_buf[32];
    snprintf(price_buf, sizeof(price_buf), "%.2f", last_price);
    float last_y = origin.y + pad_y + chart_h * (1.0f - (last_price - min_v) / range);
    dl->AddRectFilled(ImVec2(origin.x + w - 58, last_y - 8),
                      ImVec2(origin.x + w - 2, last_y + 8),
                      col32(colors::CYAN, 0.9f), 2.0f);
    dl->AddText(ImVec2(origin.x + w - 55, last_y - 6), col32(colors::BG_DARKEST), price_buf);

    ImGui::Dummy(ImVec2(w, h));
}

static void render_orders() {
    ImGui::TextColored(colors::DIM, "%-8s %-6s %-5s %5s %10s  %-8s",
                       "TIME", "SYM", "SIDE", "QTY", "PRICE", "STATUS");
    ImGui::Separator();

    for (auto& o : orders) {
        ImVec4 side_col = (strcmp(o.side, "BUY") == 0) ? colors::GREEN : colors::RED;
        ImVec4 status_col = colors::DIM;
        if (strcmp(o.status, "FILLED") == 0) status_col = colors::GREEN;
        else if (strcmp(o.status, "WORKING") == 0) status_col = colors::YELLOW;
        else if (strcmp(o.status, "PARTIAL") == 0) status_col = colors::ORANGE;

        ImGui::TextColored(colors::DIMMER, "%s", o.time);
        ImGui::SameLine(80);
        ImGui::TextColored(colors::WHITE, "%-6s", o.symbol);
        ImGui::SameLine(140);
        ImGui::TextColored(side_col, "%-5s", o.side);
        ImGui::SameLine(190);
        ImGui::TextColored(colors::WHITE, "%5d", o.qty);
        ImGui::SameLine(240);
        ImGui::TextColored(colors::WHITE, "%10.2f", o.price);
        ImGui::SameLine(340);
        ImGui::TextColored(status_col, "%s", o.status);
    }
}

static void render_news() {
    for (auto& n : news) {
        ImGui::TextColored(colors::DIMMER, "%s", n.time);
        ImGui::SameLine(50);
        ImGui::TextColored(n.tag_color, "[%s]", n.source);
        ImGui::SameLine(100);
        ImGui::TextColored(colors::WHITE, "%s", n.headline);
    }
}

static void render_portfolio() {
    struct Position {
        const char* symbol;
        int shares;
        float avg_cost;
        float current;
    };
    Position positions[] = {
        {"AAPL",  1500, 178.23f, 198.45f},
        {"MSFT",   800, 398.50f, 425.12f},
        {"GOOGL", 1200, 165.40f, 176.83f},
        {"AMZN",   600, 182.75f, 195.67f},
        {"NVDA",   400, 812.00f, 875.23f},
        {"META",   900, 495.30f, 542.18f},
        {"TSLA",   350, 238.60f, 245.90f},
    };

    ImGui::TextColored(colors::DIM, "%-6s %6s %10s %10s %10s  %8s",
                       "SYM", "SHARES", "AVG COST", "CURRENT", "P/L", "P/L%");
    ImGui::Separator();

    float total_pl = 0;
    for (auto& p : positions) {
        float pl = (p.current - p.avg_cost) * p.shares;
        float pl_pct = ((p.current / p.avg_cost) - 1.0f) * 100.0f;
        total_pl += pl;
        ImVec4 pl_col = pl >= 0 ? colors::GREEN : colors::RED;

        ImGui::TextColored(colors::WHITE, "%-6s", p.symbol);
        ImGui::SameLine(65);
        ImGui::TextColored(colors::WHITE, "%6d", p.shares);
        ImGui::SameLine(120);
        ImGui::TextColored(colors::DIM, "%10.2f", p.avg_cost);
        ImGui::SameLine(200);
        ImGui::TextColored(colors::WHITE, "%10.2f", p.current);
        ImGui::SameLine(280);
        ImGui::TextColored(pl_col, "%+10.0f", pl);
        ImGui::SameLine(360);
        ImGui::TextColored(pl_col, "%+7.2f%%", pl_pct);
    }

    ImGui::Separator();
    ImVec4 total_col = total_pl >= 0 ? colors::GREEN : colors::RED;
    ImGui::TextColored(colors::ORANGE, "TOTAL");
    ImGui::SameLine(280);
    ImGui::TextColored(total_col, "%+10.0f", total_pl);
}

static void render_heatmap(float w, float h) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 origin = ImGui::GetCursorScreenPos();
    float anim_t = (float)ImGui::GetTime();

    struct Sector {
        const char* name;
        float weight; // relative area
        float change;
    };
    Sector sectors[] = {
        {"Tech",       0.28f,  0.72f},
        {"Health",     0.14f, -0.31f},
        {"Finance",    0.13f,  0.45f},
        {"Consumer",   0.11f,  0.18f},
        {"Industrial", 0.10f, -0.12f},
        {"Energy",     0.08f, -0.67f},
        {"Materials",  0.06f,  0.33f},
        {"Utilities",  0.05f,  0.08f},
        {"Real Est",   0.05f, -0.45f},
    };
    int n = sizeof(sectors) / sizeof(sectors[0]);

    // Simple treemap: lay out in rows
    float x = origin.x + 2, y = origin.y + 2;
    float usable_w = w - 4, usable_h = h - 4;
    float remaining_w = usable_w;
    float row_h = usable_h / 3.0f;
    int row = 0;
    float row_weight = 0;

    // Pre-compute row weights
    float row_weights[3] = {0};
    int row_assign[9];
    float cum = 0;
    for (int i = 0; i < n; i++) {
        cum += sectors[i].weight;
        if (cum <= 0.38f) { row_assign[i] = 0; row_weights[0] += sectors[i].weight; }
        else if (cum <= 0.70f) { row_assign[i] = 1; row_weights[1] += sectors[i].weight; }
        else { row_assign[i] = 2; row_weights[2] += sectors[i].weight; }
    }

    float row_x[3] = {origin.x + 2, origin.x + 2, origin.x + 2};
    for (int i = 0; i < n; i++) {
        int r = row_assign[i];
        float ry = origin.y + 2 + r * row_h;
        float cell_w = (sectors[i].weight / row_weights[r]) * usable_w;
        float cx = row_x[r];

        // Color based on change
        float chg = sectors[i].change + sinf(anim_t * 0.8f + i * 1.5f) * 0.05f;
        ImVec4 bg;
        if (chg >= 0) {
            float intensity = fminf(chg / 1.0f, 1.0f);
            bg = ImVec4(0.0f, 0.15f + intensity * 0.25f, 0.05f + intensity * 0.1f, 1.0f);
        } else {
            float intensity = fminf(-chg / 1.0f, 1.0f);
            bg = ImVec4(0.15f + intensity * 0.25f, 0.03f, 0.03f, 1.0f);
        }

        dl->AddRectFilled(ImVec2(cx, ry), ImVec2(cx + cell_w - 2, ry + row_h - 2),
                          col32(bg), 2.0f);
        dl->AddRect(ImVec2(cx, ry), ImVec2(cx + cell_w - 2, ry + row_h - 2),
                    col32(colors::BORDER), 2.0f);

        // Label
        if (cell_w > 40) {
            ImVec4 text_col = chg >= 0 ? colors::GREEN : colors::RED;
            float tx = cx + 5;
            float ty = ry + 4;
            dl->AddText(ImVec2(tx, ty), col32(colors::WHITE), sectors[i].name);
            char buf[16];
            snprintf(buf, sizeof(buf), "%+.2f%%", chg);
            dl->AddText(ImVec2(tx, ty + 16), col32(text_col), buf);
        }

        row_x[r] += cell_w;
    }

    ImGui::Dummy(ImVec2(w, h));
}

// ============================================================================
// Status bar ticker animation
// ============================================================================
static void render_status_bar(float w, float h) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 origin = ImGui::GetCursorScreenPos();
    float anim_t = (float)ImGui::GetTime();

    dl->AddRectFilled(origin, ImVec2(origin.x + w, origin.y + h),
                      col32(colors::BG_HEADER));

    // Scrolling ticker
    float scroll_speed = 40.0f;
    float ticker_x = origin.x + 8 - fmodf(anim_t * scroll_speed, 1800.0f);
    float ty = origin.y + 3;

    for (auto& t : indices) {
        if (ticker_x > origin.x + w + 200) { ticker_x += 180; continue; }
        if (ticker_x > origin.x - 200) {
            float jitter = sinf(anim_t * 2.0f + ticker_x * 0.01f) * 0.03f;
            float chg = t.change_pct + jitter;
            ImVec4 chg_col = chg >= 0 ? colors::GREEN : colors::RED;

            dl->AddText(ImVec2(ticker_x, ty), col32(colors::ORANGE), t.symbol);
            char buf[32];
            snprintf(buf, sizeof(buf), "%.2f", t.price);
            dl->AddText(ImVec2(ticker_x + 45, ty), col32(colors::WHITE), buf);
            snprintf(buf, sizeof(buf), "%+.2f%%", chg);
            dl->AddText(ImVec2(ticker_x + 115, ty), col32(chg_col), buf);
        }
        ticker_x += 180;
    }
    // Repeat for seamless scroll
    for (auto& t : indices) {
        if (ticker_x > origin.x + w + 200) break;
        if (ticker_x > origin.x - 200) {
            float chg = t.change_pct;
            ImVec4 chg_col = chg >= 0 ? colors::GREEN : colors::RED;
            dl->AddText(ImVec2(ticker_x, ty), col32(colors::ORANGE), t.symbol);
            char buf[32];
            snprintf(buf, sizeof(buf), "%.2f", t.price);
            dl->AddText(ImVec2(ticker_x + 45, ty), col32(colors::WHITE), buf);
            snprintf(buf, sizeof(buf), "%+.2f%%", chg);
            dl->AddText(ImVec2(ticker_x + 115, ty), col32(chg_col), buf);
        }
        ticker_x += 180;
    }

    // Right side: clock
    float time_val = fmodf(anim_t, 86400.0f) + 52200.0f; // start at ~14:30
    int hh = ((int)(time_val / 3600)) % 24;
    int mm = ((int)(time_val / 60)) % 60;
    int ss = ((int)time_val) % 60;
    char clock[32];
    snprintf(clock, sizeof(clock), "NYC  %02d:%02d:%02d", hh, mm, ss);
    float clock_w = ImGui::CalcTextSize(clock).x;
    dl->AddText(ImVec2(origin.x + w - clock_w - 12, ty), col32(colors::ORANGE), clock);

    ImGui::Dummy(ImVec2(w, h));
}

// ============================================================================
// Main
// ============================================================================
int main() {
    if (!glfwInit()) return 1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1600, 950,
        "Bloomberg Terminal — Yoga Layout Demo", nullptr, nullptr);
    if (!window) { glfwTerminate(); return 1; }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0;
    style.ChildRounding = 2.0f;
    style.FrameRounding = 2.0f;
    style.WindowBorderSize = 0;
    style.ChildBorderSize = 1.0f;
    style.WindowPadding = ImVec2(0, 0);
    style.Colors[ImGuiCol_WindowBg]  = colors::BG_DARKEST;
    style.Colors[ImGuiCol_ChildBg]   = colors::BG_PANEL;
    style.Colors[ImGuiCol_Border]    = colors::BORDER;
    style.Colors[ImGuiCol_Separator] = colors::BORDER;

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGuiViewport* vp = ImGui::GetMainViewport();
        float screen_w = vp->WorkSize.x;
        float screen_h = vp->WorkSize.y;

        // ==================================================================
        // YOGA LAYOUT — Full Bloomberg terminal structure
        // ==================================================================
        // Outer: column  [ticker_bar | main_body | status_bar]
        // main_body: row [left_col | center_col | right_col]
        // left_col: column [indices | currencies | commodities]
        // center_col: column [chart | order_book]
        // right_col: column [portfolio | heatmap | news]
        // ==================================================================

        YGNodeRef root = YGNodeNew();
        YGNodeStyleSetFlexDirection(root, YGFlexDirectionColumn);
        YGNodeStyleSetWidth(root, screen_w);
        YGNodeStyleSetHeight(root, screen_h);

        // Ticker bar (top)
        YGNodeRef ticker_bar = YGNodeNew();
        YGNodeStyleSetHeight(ticker_bar, 22);
        YGNodeInsertChild(root, ticker_bar, 0);

        // Main body (fills remaining)
        YGNodeRef body = YGNodeNew();
        YGNodeStyleSetFlexGrow(body, 1);
        YGNodeStyleSetFlexDirection(body, YGFlexDirectionRow);
        YGNodeStyleSetPadding(body, YGEdgeAll, 2);
        YGNodeStyleSetGap(body, YGGutterAll, 2);
        YGNodeInsertChild(root, body, 1);

        // Status bar (bottom)
        YGNodeRef status_bar = YGNodeNew();
        YGNodeStyleSetHeight(status_bar, 22);
        YGNodeInsertChild(root, status_bar, 2);

        // --- Left column: 20%, min 280 ---
        YGNodeRef left_col = YGNodeNew();
        YGNodeStyleSetWidthPercent(left_col, 20);
        YGNodeStyleSetMinWidth(left_col, 280);
        YGNodeStyleSetFlexDirection(left_col, YGFlexDirectionColumn);
        YGNodeStyleSetGap(left_col, YGGutterAll, 2);
        YGNodeInsertChild(body, left_col, 0);

        YGNodeRef indices_panel = YGNodeNew();
        YGNodeStyleSetFlexGrow(indices_panel, 1);
        YGNodeInsertChild(left_col, indices_panel, 0);

        YGNodeRef fx_panel = YGNodeNew();
        YGNodeStyleSetFlexGrow(fx_panel, 0.8f);
        YGNodeInsertChild(left_col, fx_panel, 1);

        YGNodeRef comm_panel = YGNodeNew();
        YGNodeStyleSetFlexGrow(comm_panel, 0.8f);
        YGNodeInsertChild(left_col, comm_panel, 2);

        // --- Center column: flex grow ---
        YGNodeRef center_col = YGNodeNew();
        YGNodeStyleSetFlexGrow(center_col, 1);
        YGNodeStyleSetMinWidth(center_col, 300);
        YGNodeStyleSetFlexDirection(center_col, YGFlexDirectionColumn);
        YGNodeStyleSetGap(center_col, YGGutterAll, 2);
        YGNodeInsertChild(body, center_col, 1);

        YGNodeRef chart_panel = YGNodeNew();
        YGNodeStyleSetFlexGrow(chart_panel, 1.2f);
        YGNodeInsertChild(center_col, chart_panel, 0);

        YGNodeRef orders_panel = YGNodeNew();
        YGNodeStyleSetFlexGrow(orders_panel, 1);
        YGNodeInsertChild(center_col, orders_panel, 1);

        // --- Right column: 28%, min 320 ---
        YGNodeRef right_col = YGNodeNew();
        YGNodeStyleSetWidthPercent(right_col, 28);
        YGNodeStyleSetMinWidth(right_col, 320);
        YGNodeStyleSetFlexDirection(right_col, YGFlexDirectionColumn);
        YGNodeStyleSetGap(right_col, YGGutterAll, 2);
        YGNodeInsertChild(body, right_col, 2);

        YGNodeRef portfolio_panel = YGNodeNew();
        YGNodeStyleSetFlexGrow(portfolio_panel, 1);
        YGNodeInsertChild(right_col, portfolio_panel, 0);

        YGNodeRef heatmap_panel = YGNodeNew();
        YGNodeStyleSetHeightPercent(heatmap_panel, 30);
        YGNodeStyleSetMinHeight(heatmap_panel, 120);
        YGNodeInsertChild(right_col, heatmap_panel, 1);

        YGNodeRef news_panel = YGNodeNew();
        YGNodeStyleSetFlexGrow(news_panel, 1);
        YGNodeInsertChild(right_col, news_panel, 2);

        // --- CALCULATE ---
        YGNodeCalculateLayout(root, screen_w, screen_h, YGDirectionLTR);

        // ==================================================================
        // Read computed positions
        // ==================================================================
        auto get_rect = [&](YGNodeRef node, YGNodeRef parent, YGNodeRef grandparent = nullptr) {
            float x = YGNodeLayoutGetLeft(node);
            float y = YGNodeLayoutGetTop(node);
            if (parent) {
                x += YGNodeLayoutGetLeft(parent);
                y += YGNodeLayoutGetTop(parent);
            }
            if (grandparent) {
                x += YGNodeLayoutGetLeft(grandparent);
                y += YGNodeLayoutGetTop(grandparent);
            }
            float w = YGNodeLayoutGetWidth(node);
            float h = YGNodeLayoutGetHeight(node);
            return ImVec4(vp->WorkPos.x + x, vp->WorkPos.y + y, w, h);
        };

        // Ticker bar rect
        ImVec4 r_ticker = get_rect(ticker_bar, nullptr);
        // Status bar rect
        ImVec4 r_status = get_rect(status_bar, nullptr);
        // Left panels
        ImVec4 r_indices = get_rect(indices_panel, left_col, body);
        ImVec4 r_fx      = get_rect(fx_panel, left_col, body);
        ImVec4 r_comm    = get_rect(comm_panel, left_col, body);
        // Center panels
        ImVec4 r_chart   = get_rect(chart_panel, center_col, body);
        ImVec4 r_orders  = get_rect(orders_panel, center_col, body);
        // Right panels
        ImVec4 r_port    = get_rect(portfolio_panel, right_col, body);
        ImVec4 r_heat    = get_rect(heatmap_panel, right_col, body);
        ImVec4 r_news    = get_rect(news_panel, right_col, body);

        YGNodeFreeRecursive(root);

        // ==================================================================
        // RENDER — fullscreen host window
        // ==================================================================
        ImGui::SetNextWindowPos(vp->WorkPos);
        ImGui::SetNextWindowSize(vp->WorkSize);
        ImGui::Begin("##bloomberg", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoDocking);

        // --- Scrolling ticker bar ---
        ImGui::SetCursorScreenPos(ImVec2(r_ticker.x, r_ticker.y));
        render_status_bar(r_ticker.z, r_ticker.w);

        // --- Left: Indices ---
        ImGui::SetCursorScreenPos(ImVec2(r_indices.x, r_indices.y));
        if (ImGui::BeginChild("##indices", ImVec2(r_indices.z, r_indices.w), ImGuiChildFlags_Borders)) {
            panel_header("GLOBAL INDICES", colors::CYAN);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 2));
            render_ticker_table(indices, 8);
            ImGui::PopStyleVar();
        }
        ImGui::EndChild();

        // --- Left: FX ---
        ImGui::SetCursorScreenPos(ImVec2(r_fx.x, r_fx.y));
        if (ImGui::BeginChild("##fx", ImVec2(r_fx.z, r_fx.w), ImGuiChildFlags_Borders)) {
            panel_header("CURRENCIES", colors::YELLOW);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 2));
            render_ticker_table(currencies, 6);
            ImGui::PopStyleVar();
        }
        ImGui::EndChild();

        // --- Left: Commodities ---
        ImGui::SetCursorScreenPos(ImVec2(r_comm.x, r_comm.y));
        if (ImGui::BeginChild("##comm", ImVec2(r_comm.z, r_comm.w), ImGuiChildFlags_Borders)) {
            panel_header("COMMODITIES", colors::ORANGE);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 2));
            render_ticker_table(commodities, 6);
            ImGui::PopStyleVar();
        }
        ImGui::EndChild();

        // --- Center: Chart ---
        ImGui::SetCursorScreenPos(ImVec2(r_chart.x, r_chart.y));
        if (ImGui::BeginChild("##chart", ImVec2(r_chart.z, r_chart.w), ImGuiChildFlags_Borders)) {
            panel_header("S&P 500 — INTRADAY", colors::CYAN);
            float cw = ImGui::GetContentRegionAvail().x;
            float ch = ImGui::GetContentRegionAvail().y;
            render_chart(cw, ch);
        }
        ImGui::EndChild();

        // --- Center: Orders ---
        ImGui::SetCursorScreenPos(ImVec2(r_orders.x, r_orders.y));
        if (ImGui::BeginChild("##orders", ImVec2(r_orders.z, r_orders.w), ImGuiChildFlags_Borders)) {
            panel_header("ORDER BOOK", colors::GREEN);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 2));
            render_orders();
            ImGui::PopStyleVar();
        }
        ImGui::EndChild();

        // --- Right: Portfolio ---
        ImGui::SetCursorScreenPos(ImVec2(r_port.x, r_port.y));
        if (ImGui::BeginChild("##portfolio", ImVec2(r_port.z, r_port.w), ImGuiChildFlags_Borders)) {
            panel_header("PORTFOLIO", colors::GREEN);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 2));
            render_portfolio();
            ImGui::PopStyleVar();
        }
        ImGui::EndChild();

        // --- Right: Heatmap ---
        ImGui::SetCursorScreenPos(ImVec2(r_heat.x, r_heat.y));
        if (ImGui::BeginChild("##heatmap", ImVec2(r_heat.z, r_heat.w), ImGuiChildFlags_Borders)) {
            panel_header("SECTOR HEATMAP", colors::PURPLE);
            float hw = ImGui::GetContentRegionAvail().x;
            float hh = ImGui::GetContentRegionAvail().y;
            render_heatmap(hw, hh);
        }
        ImGui::EndChild();

        // --- Right: News ---
        ImGui::SetCursorScreenPos(ImVec2(r_news.x, r_news.y));
        if (ImGui::BeginChild("##news", ImVec2(r_news.z, r_news.w), ImGuiChildFlags_Borders)) {
            panel_header("NEWS WIRE", colors::ORANGE);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 3));
            render_news();
            ImGui::PopStyleVar();
        }
        ImGui::EndChild();

        // --- Bottom status bar ---
        ImGui::SetCursorScreenPos(ImVec2(r_status.x, r_status.y));
        {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddRectFilled(ImVec2(r_status.x, r_status.y),
                              ImVec2(r_status.x + r_status.z, r_status.y + r_status.w),
                              col32(colors::BG_HEADER));
            // Accent top border
            dl->AddRectFilled(ImVec2(r_status.x, r_status.y),
                              ImVec2(r_status.x + r_status.z, r_status.y + 1),
                              col32(colors::ORANGE, 0.3f));

            dl->AddText(ImVec2(r_status.x + 8, r_status.y + 4),
                        col32(colors::ORANGE), "FINCEPT TERMINAL");
            dl->AddText(ImVec2(r_status.x + 160, r_status.y + 4),
                        col32(colors::DIM), "YOGA LAYOUT DEMO");
            dl->AddText(ImVec2(r_status.x + 320, r_status.y + 4),
                        col32(colors::DIMMER), "All data simulated | Resize window to test responsive layout");

            char fps[32];
            snprintf(fps, sizeof(fps), "%.0f FPS", ImGui::GetIO().Framerate);
            float fps_w = ImGui::CalcTextSize(fps).x;
            dl->AddText(ImVec2(r_status.x + r_status.z - fps_w - 12, r_status.y + 4),
                        col32(colors::DIM), fps);
        }

        ImGui::End();

        // Render
        ImGui::Render();
        int dw, dh;
        glfwGetFramebufferSize(window, &dw, &dh);
        glViewport(0, 0, dw, dh);
        glClearColor(colors::BG_DARKEST.x, colors::BG_DARKEST.y, colors::BG_DARKEST.z, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
