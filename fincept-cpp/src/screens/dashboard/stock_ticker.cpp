#include "stock_ticker.h"
#include "core/logger.h"
#include "ui/theme.h"
#include <imgui.h>
#include <nlohmann/json.hpp>
#include <cstdio>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <sstream>
#include <array>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

namespace fincept::dashboard {

using json = nlohmann::json;

// Tickers to display — major indices, stocks, crypto, commodities
static const std::vector<std::pair<std::string, std::string>> TICKER_SYMBOLS = {
    {"^GSPC",    "S&P 500"},
    {"^DJI",     "DOW"},
    {"^IXIC",    "NASDAQ"},
    {"^FTSE",    "FTSE 100"},
    {"^N225",    "NIKKEI"},
    {"AAPL",     "AAPL"},
    {"MSFT",     "MSFT"},
    {"GOOGL",    "GOOGL"},
    {"AMZN",     "AMZN"},
    {"NVDA",     "NVDA"},
    {"TSLA",     "TSLA"},
    {"META",     "META"},
    {"BTC-USD",  "BTC"},
    {"ETH-USD",  "ETH"},
    {"GC=F",     "GOLD"},
    {"CL=F",     "CRUDE"},
    {"^TNX",     "US10Y"},
    {"EURUSD=X", "EUR/USD"},
    {"^VIX",     "VIX"},
    {"^NSEI",    "NIFTY"},
};

// Find the yfinance_data.py script path
static std::string find_script_path() {
    // Try paths relative to the exe
#ifdef _WIN32
    char exe_buf[MAX_PATH];
    GetModuleFileNameA(nullptr, exe_buf, MAX_PATH);
    std::filesystem::path exe_dir = std::filesystem::path(exe_buf).parent_path();
#elif defined(__APPLE__)
    // macOS: use _NSGetExecutablePath or /proc/self/exe fallback
    char exe_buf[4096];
    uint32_t bufsize = sizeof(exe_buf);
    std::filesystem::path exe_dir;
    if (_NSGetExecutablePath(exe_buf, &bufsize) == 0) {
        exe_dir = std::filesystem::path(exe_buf).parent_path();
    } else {
        exe_dir = std::filesystem::current_path();
    }
#else
    // Linux: /proc/self/exe
    std::filesystem::path exe_dir;
    try {
        exe_dir = std::filesystem::read_symlink("/proc/self/exe").parent_path();
    } catch (...) {
        exe_dir = std::filesystem::current_path();
    }
#endif

    // Search candidates
    std::vector<std::string> candidates = {
        // Relative to exe (build/Release/ on Windows, build/ on Linux/macOS)
        (exe_dir / ".." / ".." / ".." / "fincept-terminal-desktop" / "src-tauri" / "resources" / "scripts" / "yfinance_data.py").string(),
        (exe_dir / ".." / ".." / "fincept-terminal-desktop" / "src-tauri" / "resources" / "scripts" / "yfinance_data.py").string(),
        // From project root
        (exe_dir / ".." / ".." / ".." / ".." / "fincept-terminal-desktop" / "src-tauri" / "resources" / "scripts" / "yfinance_data.py").string(),
    };

    for (auto& path : candidates) {
        if (std::filesystem::exists(path)) {
            auto canonical = std::filesystem::canonical(path).string();
            LOG_DEBUG("Ticker", "Found script: %s", canonical.c_str());
            return canonical;
        }
    }

    LOG_ERROR("Ticker", "yfinance_data.py not found!");
    return "";
}

// Run python script and capture stdout
static std::string run_python(const std::string& script_path, const std::string& args) {
#ifdef _WIN32
    const char* python_cmd = "python";
    const char* null_dev = "NUL";
#else
    const char* python_cmd = "python3";
    const char* null_dev = "/dev/null";
#endif

    LOG_DEBUG("Ticker", "Running: %s \"%s\" %s", python_cmd, script_path.c_str(), args.c_str());

#ifdef _WIN32
    // Use CreateProcess with pipes for cleaner capture
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    HANDLE hReadPipe, hWritePipe;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        LOG_ERROR("Ticker", "CreatePipe failed");
        return "";
    }
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    // Also redirect stderr to NUL to suppress yfinance warnings
    HANDLE hNul = CreateFileA("NUL", GENERIC_WRITE, FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.hStdOutput = hWritePipe;
    si.hStdError = hNul;
    si.dwFlags = STARTF_USESTDHANDLES;

    PROCESS_INFORMATION pi = {};
    std::string full_cmd = std::string(python_cmd) + " \"" + script_path + "\" " + args;
    char cmd_buf[4096];
    strncpy_s(cmd_buf, full_cmd.c_str(), sizeof(cmd_buf) - 1);

    if (!CreateProcessA(nullptr, cmd_buf, nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        LOG_ERROR("Ticker", "CreateProcess failed (error %lu)", GetLastError());
        CloseHandle(hReadPipe);
        CloseHandle(hWritePipe);
        if (hNul != INVALID_HANDLE_VALUE) CloseHandle(hNul);
        return "";
    }

    CloseHandle(hWritePipe);
    if (hNul != INVALID_HANDLE_VALUE) CloseHandle(hNul);

    // Read output
    std::string output;
    char buffer[4096];
    DWORD bytesRead;
    while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        output += buffer;
    }

    WaitForSingleObject(pi.hProcess, 60000); // 60s timeout
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hReadPipe);

    return output;
#else
    std::string cmd = std::string(python_cmd) + " \"" + script_path + "\" " + args + " 2>" + null_dev;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";

    std::string output;
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        output += buffer;
    }
    pclose(pipe);
    return output;
#endif
}

StockTicker& StockTicker::instance() {
    static StockTicker ticker;
    return ticker;
}

void StockTicker::initialize() {
    if (running_) return;

    // Initialize with placeholder data
    {
        std::lock_guard<std::mutex> lock(mutex_);
        items_.clear();
        for (auto& [sym, name] : TICKER_SYMBOLS) {
            TickerItem item;
            item.symbol = sym;
            item.name = name;
            items_.push_back(item);
        }
    }

    running_ = true;
    fetch_thread_ = std::thread(&StockTicker::fetch_loop, this);
    LOG_INFO("Ticker", "Initialized with %zu symbols", TICKER_SYMBOLS.size());
}

void StockTicker::shutdown() {
    running_ = false;
    if (fetch_thread_.joinable()) {
        fetch_thread_.join();
    }
}

void StockTicker::fetch_prices() {
    std::string script = find_script_path();
    if (script.empty()) {
        LOG_WARN("Ticker", "Cannot fetch — script not found");
        return;
    }

    // Build args: batch_quotes SYM1 SYM2 SYM3 ...
    std::string args = "batch_quotes";
    for (auto& [sym, name] : TICKER_SYMBOLS) {
        args += " " + sym;
    }

    std::string output = run_python(script, args);

    if (output.empty()) {
        LOG_WARN("Ticker", "No output from python script");
        return;
    }

    // Find the JSON in the output (last line starting with [ or {)
    std::string json_str;
    std::istringstream stream(output);
    std::string line;
    while (std::getline(stream, line)) {
        // Trim whitespace
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        char first = line[start];
        if (first == '[' || first == '{') {
            json_str = line.substr(start);
        }
    }

    if (json_str.empty()) {
        LOG_WARN("Ticker", "No JSON found in output. Raw: %.200s", output.c_str());
        return;
    }

    try {
        auto data = json::parse(json_str);

        if (data.is_array()) {
            std::lock_guard<std::mutex> lock(mutex_);
            int matched = 0;
            for (auto& entry : data) {
                std::string sym = entry.value("symbol", "");
                for (auto& item : items_) {
                    if (item.symbol == sym) {
                        item.price = entry.value("price", 0.0f);
                        item.change = entry.value("change", 0.0f);
                        item.change_pct = entry.value("change_percent", 0.0f);
                        item.valid = item.price > 0;
                        matched++;
                        break;
                    }
                }
            }
            fetched_ = true;
            LOG_INFO("Ticker", "Loaded %d/%zu prices from yfinance", matched, TICKER_SYMBOLS.size());
        } else if (data.contains("error")) {
            LOG_ERROR("Ticker", "Script error: %s", data["error"].get<std::string>().c_str());
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Ticker", "JSON parse error: %s, raw: %.200s", e.what(), json_str.c_str());
    }
}

void StockTicker::fetch_loop() {
    // Initial fetch immediately
    fetch_prices();

    while (running_) {
        // Refresh every 60 seconds (yfinance has rate limits)
        for (int i = 0; i < 60 && running_; i++) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        if (running_) {
            fetch_prices();
        }
    }
}

void StockTicker::render(float bar_height) {
    ImGuiIO& io = ImGui::GetIO();
    float window_width = io.DisplaySize.x;

    // Position just below the main menu bar using viewport WorkPos
    ImGuiViewport* vp = ImGui::GetMainViewport();
    float ticker_y = vp->WorkPos.y;
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x, ticker_y));
    ImGui::SetNextWindowSize(ImVec2(window_width, bar_height));

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.02f, 0.02f, 0.05f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, theme::colors::BORDER_DIM);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 2));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);

    ImGui::Begin("##stock_ticker", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus);

    // Calculate total content width
    std::lock_guard<std::mutex> lock(mutex_);

    float spacing = 32.0f;
    float total_width = 0.0f;

    // Pre-calculate widths
    struct RenderItem {
        std::string text;
        float width;
        ImVec4 change_color;
        std::string change_text;
        float change_width;
    };
    std::vector<RenderItem> render_items;

    for (auto& item : items_) {
        RenderItem ri;

        if (item.valid) {
            // Format price based on magnitude
            char price_buf[32];
            if (item.price >= 10000.0f) {
                std::snprintf(price_buf, sizeof(price_buf), "%.0f", item.price);
            } else if (item.price >= 100.0f) {
                std::snprintf(price_buf, sizeof(price_buf), "%.1f", item.price);
            } else if (item.price >= 1.0f) {
                std::snprintf(price_buf, sizeof(price_buf), "%.2f", item.price);
            } else {
                std::snprintf(price_buf, sizeof(price_buf), "%.4f", item.price);
            }

            ri.text = item.name + "  " + price_buf;

            char change_buf[32];
            std::snprintf(change_buf, sizeof(change_buf), "%s%.2f%%",
                item.change_pct >= 0 ? "+" : "", item.change_pct);
            ri.change_text = change_buf;

            ri.change_color = item.change_pct >= 0 ? theme::colors::SUCCESS : theme::colors::ERROR_RED;
        } else {
            ri.text = item.name + "  ---";
            ri.change_text = "";
            ri.change_color = theme::colors::TEXT_DISABLED;
        }

        ri.width = ImGui::CalcTextSize(ri.text.c_str()).x;
        ri.change_width = ri.change_text.empty() ? 0 : ImGui::CalcTextSize(ri.change_text.c_str()).x;

        float item_width = ri.width + (ri.change_width > 0 ? ri.change_width + 8.0f : 0) + spacing;
        total_width += item_width;
        render_items.push_back(ri);
    }

    if (total_width < 1.0f) total_width = 1.0f;

    // Scroll speed — pixels per second
    float scroll_speed = 40.0f;
    scroll_offset_ += scroll_speed * io.DeltaTime;

    // Wrap around when we've scrolled past the full content
    if (scroll_offset_ >= total_width) {
        scroll_offset_ -= total_width;
    }

    // Render two copies for seamless looping
    float y_center = (bar_height - ImGui::GetTextLineHeight()) * 0.5f;

    for (int copy = 0; copy < 2; copy++) {
        float x = -scroll_offset_ + copy * total_width;

        for (auto& ri : render_items) {
            float item_end = x + ri.width + ri.change_width + 8.0f + spacing;

            // Only render if visible (fully on screen)
            if (item_end > 0 && x < window_width && x >= -ri.width) {
                float draw_x = std::max(x, 0.0f);

                // Symbol + Price
                if (x >= 0) {
                    ImGui::SetCursorPos(ImVec2(draw_x, y_center));
                    ImGui::TextColored(theme::colors::TEXT_PRIMARY, "%s", ri.text.c_str());
                }

                // Change percentage
                float chg_x = x + ri.width + 8.0f;
                if (!ri.change_text.empty() && chg_x >= 0 && chg_x < window_width) {
                    ImGui::SetCursorPos(ImVec2(chg_x, y_center));
                    ImGui::TextColored(ri.change_color, "%s", ri.change_text.c_str());
                }

                // Separator
                float sep_x = x + ri.width + (ri.change_width > 0 ? ri.change_width + 8.0f : 0) + spacing * 0.5f - 4.0f;
                if (sep_x >= 0 && sep_x < window_width) {
                    ImGui::SetCursorPos(ImVec2(sep_x, y_center));
                    ImGui::TextColored(theme::colors::BORDER_DIM, "|");
                }
            }

            x += ri.width + (ri.change_width > 0 ? ri.change_width + 8.0f : 0) + spacing;
        }
    }

    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

} // namespace fincept::dashboard
