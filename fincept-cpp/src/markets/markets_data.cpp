#include "markets_data.h"
#include <nlohmann/json.hpp>
#include <thread>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#endif

using json = nlohmann::json;

namespace fincept::markets {

const std::string MarketsData::empty_string_;
const std::vector<MarketQuote> MarketsData::empty_quotes_;

static const char* PYTHON_EXE = "C:/ProgramData/miniconda3/python.exe";
static const char* YFINANCE_SCRIPT =
    "C:/windowsdisk/finceptTerminal/fincept-terminal-desktop/"
    "src-tauri/resources/scripts/yfinance_data.py";

// Run python and capture stdout (same as research_data.cpp)
static std::string run_python(const std::string& args) {
    std::string cmd = std::string("\"") + PYTHON_EXE + "\" \""
                    + YFINANCE_SCRIPT + "\" " + args;

#ifdef _WIN32
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hRead = nullptr, hWrite = nullptr;
    CreatePipe(&hRead, &hWrite, &sa, 0);
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    si.dwFlags |= STARTF_USESTDHANDLES;

    PROCESS_INFORMATION pi{};
    std::string cmd_line = cmd;

    BOOL ok = CreateProcessA(
        nullptr, cmd_line.data(), nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi
    );
    CloseHandle(hWrite);

    if (!ok) {
        CloseHandle(hRead);
        return "";
    }

    std::string output;
    char buffer[4096];
    DWORD bytesRead;
    while (ReadFile(hRead, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        output += buffer;
    }
    CloseHandle(hRead);
    WaitForSingleObject(pi.hProcess, 60000);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    // Extract last JSON line
    std::string json_line;
    std::istringstream stream(output);
    std::string line;
    while (std::getline(stream, line)) {
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        char first = line[start];
        if (first == '{' || first == '[') json_line = line;
    }
    return json_line;
#else
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";
    std::string output;
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe)) output += buffer;
    pclose(pipe);

    std::string json_line;
    std::istringstream stream(output);
    std::string line;
    while (std::getline(stream, line)) {
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        char first = line[start];
        if (first == '{' || first == '[') json_line = line;
    }
    return json_line;
#endif
}

bool MarketsData::is_loading(const std::string& category) const {
    auto it = categories_.find(category);
    if (it == categories_.end()) return false;
    return it->second.loading.load();
}

bool MarketsData::has_data(const std::string& category) const {
    auto it = categories_.find(category);
    if (it == categories_.end()) return false;
    return it->second.has_data;
}

const std::string& MarketsData::error(const std::string& category) const {
    auto it = categories_.find(category);
    if (it == categories_.end()) return empty_string_;
    return it->second.error;
}

const std::vector<MarketQuote>& MarketsData::quotes(const std::string& category) const {
    auto it = categories_.find(category);
    if (it == categories_.end()) return empty_quotes_;
    return it->second.quotes;
}

void MarketsData::fetch_category(const std::string& category, const std::vector<std::string>& symbols) {
    auto& state = categories_[category];
    if (state.loading.load()) return;
    state.loading = true;
    state.error.clear();

    // Build symbol args: "batch_quotes SYM1 SYM2 SYM3..."
    std::string args = "batch_quotes";
    for (auto& s : symbols) args += " " + s;

    std::string cat = category;
    std::thread([this, cat, args]() {
        std::string out = run_python(args);
        if (!out.empty()) {
            std::lock_guard<std::mutex> lock(mutex_);
            parse_quotes(cat, out);
        } else {
            std::lock_guard<std::mutex> lock(mutex_);
            categories_[cat].error = "Failed to fetch data";
        }
        categories_[cat].has_data = true;
        categories_[cat].loading = false;
    }).detach();
}

void MarketsData::fetch_all(const std::vector<MarketCategory>& categories) {
    for (auto& cat : categories) {
        fetch_category(cat.title, cat.symbols);
    }
}

void MarketsData::parse_quotes(const std::string& category, const std::string& json_str) {
    auto& state = categories_[category];
    state.quotes.clear();
    try {
        auto j = json::parse(json_str);
        if (j.is_array()) {
            for (auto& item : j) {
                MarketQuote q;
                q.symbol = item.value("symbol", "");
                q.price = item.value("price", 0.0);
                q.change = item.value("change", 0.0);
                q.change_percent = item.value("change_percent", 0.0);
                q.high = item.value("high", 0.0);
                q.low = item.value("low", 0.0);
                q.open = item.value("open", 0.0);
                q.previous_close = item.value("previous_close", 0.0);
                q.volume = item.value("volume", 0.0);
                if (!q.symbol.empty())
                    state.quotes.push_back(q);
            }
        }
    } catch (...) {
        state.error = "Parse error";
    }
}

} // namespace fincept::markets
