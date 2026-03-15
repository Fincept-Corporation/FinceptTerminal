#include "algo_service.h"
#include "storage/database.h"
#include "python/python_runner.h"
#include "storage/cache_service.h"
#include "trading/unified_trading.h"
#include "core/logger.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#include <tlhelp32.h>
#else
#include <signal.h>
#endif

namespace fincept::algo {

using json = nlohmann::json;

AlgoService& AlgoService::instance() {
    static AlgoService svc;
    return svc;
}

// ============================================================================
// Path Helpers
// ============================================================================

std::string AlgoService::get_scripts_dir() const {
    auto exe_dir = python::get_exe_dir();
    // Check: exe_dir/scripts/algo_trading/
    auto path = exe_dir / "scripts" / "algo_trading";
    if (std::filesystem::exists(path)) return path.string();

    // Fallback: CWD/scripts/algo_trading/
    path = std::filesystem::current_path() / "scripts" / "algo_trading";
    if (std::filesystem::exists(path)) return path.string();

    return "";
}

std::string AlgoService::get_db_path() const {
    auto dir = python::get_install_dir();
    return (dir / "fincept.db").string();
}

// ============================================================================
// Process Management (cross-platform)
// ============================================================================

bool AlgoService::is_pid_alive(int pid) const {
    if (pid <= 0) return false;
#ifdef _WIN32
    HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)pid);
    if (!proc) return false;
    DWORD exit_code = 0;
    GetExitCodeProcess(proc, &exit_code);
    CloseHandle(proc);
    return exit_code == STILL_ACTIVE;
#else
    return kill(pid, 0) == 0;
#endif
}

bool AlgoService::verify_pid_is_algo_runner(int pid) const {
    if (pid <= 0) return false;
#ifdef _WIN32
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;
    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(pe);
    bool found = false;
    if (Process32First(snap, &pe)) {
        do {
            if ((int)pe.th32ProcessID == pid) {
                std::string name = pe.szExeFile;
                // Check if it's a python process
                found = (name.find("python") != std::string::npos ||
                         name.find("Python") != std::string::npos);
                break;
            }
        } while (Process32Next(snap, &pe));
    }
    CloseHandle(snap);
    return found;
#else
    std::string cmdline_path = "/proc/" + std::to_string(pid) + "/cmdline";
    std::ifstream f(cmdline_path);
    if (!f.is_open()) return false;
    std::string content((std::istreambuf_iterator<char>(f)), {});
    return content.find("algo_live_runner") != std::string::npos;
#endif
}

bool AlgoService::safe_kill_process(int pid) const {
    if (!verify_pid_is_algo_runner(pid)) {
        LOG_WARN("AlgoService", "PID %d is not an algo runner, skipping kill", pid);
        return false;
    }
#ifdef _WIN32
    HANDLE proc = OpenProcess(PROCESS_TERMINATE, FALSE, (DWORD)pid);
    if (!proc) return false;
    bool ok = TerminateProcess(proc, 1) != 0;
    CloseHandle(proc);
    return ok;
#else
    return kill(pid, SIGTERM) == 0;
#endif
}

// ============================================================================
// Deploy Strategy
// ============================================================================

std::string AlgoService::deploy_strategy(const std::string& strategy_id,
                                           const std::string& symbol,
                                           const std::string& mode,
                                           const std::string& provider,
                                           const std::string& timeframe,
                                           double quantity,
                                           const std::string& params_json) {
    std::lock_guard lock(mutex_);

    // Validate quantity
    if (quantity <= 0) {
        return json({{"success", false}, {"error", "Quantity must be > 0"}}).dump();
    }

    // Rate limit
    if (running_deployment_count() >= MAX_CONCURRENT) {
        return json({{"success", false},
                     {"error", "Max " + std::to_string(MAX_CONCURRENT) + " concurrent deployments"}}).dump();
    }

    // Get strategy
    auto strategy = db::ops::get_algo_strategy(strategy_id);
    if (!strategy) {
        return json({{"success", false}, {"error", "Strategy not found"}}).dump();
    }

    // Create deployment record
    std::string deploy_id = "algo-" + db::generate_uuid();
    AlgoDeployment dep;
    dep.id = deploy_id;
    dep.strategy_id = strategy_id;
    dep.strategy_name = strategy->name;
    dep.mode = mode;
    dep.status = DeploymentStatus::Starting;
    dep.config = params_json;
    db::ops::save_algo_deployment(dep);

    // Create initial metrics row
    AlgoMetrics metrics;
    metrics.deployment_id = deploy_id;
    db::ops::save_algo_metrics(metrics);

    // Find the runner script
    std::string scripts_dir = get_scripts_dir();
    std::string runner_path = scripts_dir + "/algo_live_runner.py";
    if (scripts_dir.empty() || !std::filesystem::exists(runner_path)) {
        db::ops::update_algo_deployment_status(deploy_id, "error", "Runner script not found");
        return json({{"success", false}, {"error", "algo_live_runner.py not found"}}).dump();
    }

    std::string db_path = get_db_path();
    std::string prov = provider.empty() ? "fyers" : provider;

    // Spawn the Python runner process
    LOG_INFO("AlgoService", "Deploying %s: %s mode=%s tf=%s qty=%.2f",
             deploy_id.c_str(), symbol.c_str(), mode.c_str(), timeframe.c_str(), quantity);

    auto python_path = python::resolve_python_path("algo_live_runner");

#ifdef _WIN32
    // Build command line
    std::string cmdline = "\"" + python_path.string() + "\" "
        "\"" + runner_path + "\" "
        "--deploy-id " + deploy_id + " "
        "--strategy-id " + strategy_id + " "
        "--symbol " + symbol + " "
        "--provider " + prov + " "
        "--mode " + mode + " "
        "--timeframe " + timeframe + " "
        "--quantity " + std::to_string(quantity) + " "
        "--db \"" + db_path + "\"";

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};

    BOOL ok = CreateProcessA(
        nullptr, const_cast<char*>(cmdline.c_str()),
        nullptr, nullptr, FALSE,
        CREATE_NO_WINDOW | DETACHED_PROCESS,
        nullptr, nullptr, &si, &pi
    );

    if (!ok) {
        db::ops::update_algo_deployment_status(deploy_id, "error", "Failed to spawn Python runner");
        return json({{"success", false}, {"error", "CreateProcess failed"}}).dump();
    }

    int pid = (int)pi.dwProcessId;
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
#else
    // Unix: fork+exec
    std::vector<std::string> args = {
        runner_path,
        "--deploy-id", deploy_id,
        "--strategy-id", strategy_id,
        "--symbol", symbol,
        "--provider", prov,
        "--mode", mode,
        "--timeframe", timeframe,
        "--quantity", std::to_string(quantity),
        "--db", db_path
    };

    pid_t child = fork();
    if (child < 0) {
        db::ops::update_algo_deployment_status(deploy_id, "error", "Fork failed");
        return json({{"success", false}, {"error", "fork() failed"}}).dump();
    }
    if (child == 0) {
        // Child process
        std::vector<const char*> argv;
        argv.push_back(python_path.c_str());
        for (auto& a : args) argv.push_back(a.c_str());
        argv.push_back(nullptr);
        execvp(python_path.c_str(), const_cast<char* const*>(argv.data()));
        _exit(1);
    }
    int pid = (int)child;
#endif

    LOG_INFO("AlgoService", "Python runner spawned PID=%d for %s", pid, deploy_id.c_str());

    // Update deployment with PID and running status
    dep.pid = pid;
    dep.status = DeploymentStatus::Running;
    db::ops::update_algo_deployment_status(deploy_id, "running");
    // Update PID separately
    {
        auto& db = db::Database::instance();
        std::lock_guard db_lock(db.mutex());
        auto stmt = db.prepare("UPDATE algo_deployments SET pid = ?1 WHERE id = ?2");
        stmt.bind_int(1, pid);
        stmt.bind_text(2, deploy_id);
        stmt.execute();
    }

    // Spawn background reaper thread to detect when the runner exits
    std::string reaper_id = deploy_id;
    int reaper_pid = pid;
    std::thread([reaper_id, reaper_pid]() {
        // Poll every 2 seconds to check if PID is still alive
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(2));
#ifdef _WIN32
            HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)reaper_pid);
            if (!proc) break;
            DWORD code = 0;
            GetExitCodeProcess(proc, &code);
            CloseHandle(proc);
            if (code != STILL_ACTIVE) break;
#else
            if (kill(reaper_pid, 0) != 0) break;
#endif
        }
        // Process exited — update DB if still marked as running
        auto dep = db::ops::get_algo_deployment(reaper_id);
        if (dep && dep->status == DeploymentStatus::Running) {
            db::ops::update_algo_deployment_status(reaper_id, "stopped", "Runner process exited");
            LOG_INFO("AlgoReaper", "Deployment %s runner exited", reaper_id.c_str());
        }
    }).detach();

    // Auto-start order bridge for live mode
    if (mode == "live") {
        trading::UnifiedTrading::instance().start_order_bridge();
    }

    return json({{"success", true}, {"deployment_id", deploy_id}, {"pid", pid}}).dump();
}

// ============================================================================
// Stop Deployment
// ============================================================================

bool AlgoService::stop_deployment(const std::string& deployment_id) {
    auto dep = db::ops::get_algo_deployment(deployment_id);
    if (!dep) return false;

    // Kill the process
    if (dep->pid > 0) {
        safe_kill_process(dep->pid);
    }

    db::ops::update_algo_deployment_status(deployment_id, "stopped");
    LOG_INFO("AlgoService", "Stopped deployment %s", deployment_id.c_str());
    return true;
}

int AlgoService::stop_all_deployments() {
    auto deployments = db::ops::list_algo_deployments();
    int count = 0;
    for (auto& d : deployments) {
        if (d.status == DeploymentStatus::Running || d.status == DeploymentStatus::Starting) {
            if (stop_deployment(d.id)) count++;
        }
    }
    // Stop order bridge too
    trading::UnifiedTrading::instance().stop_order_bridge();
    return count;
}

void AlgoService::delete_deployment(const std::string& deployment_id) {
    // Stop first if running
    auto dep = db::ops::get_algo_deployment(deployment_id);
    if (dep && (dep->status == DeploymentStatus::Running || dep->status == DeploymentStatus::Starting)) {
        stop_deployment(deployment_id);
    }
    db::ops::delete_algo_deployment(deployment_id);
}

// ============================================================================
// Backtest
// ============================================================================

BacktestResult AlgoService::run_backtest(const std::string& strategy_id,
                                           const std::string& symbol,
                                           const std::string& start_date,
                                           const std::string& end_date,
                                           double capital,
                                           const std::string& provider) {
    BacktestResult result;

    auto strategy = db::ops::get_algo_strategy(strategy_id);
    if (!strategy) {
        result.error = "Strategy not found";
        return result;
    }

    std::vector<std::string> args = {
        "--strategy-id", strategy_id,
        "--symbol", symbol,
        "--start", start_date,
        "--end", end_date,
        "--capital", std::to_string(capital),
        "--provider", provider,
        "--entry-conditions", strategy->entry_conditions,
        "--exit-conditions", strategy->exit_conditions,
        "--timeframe", strategy->timeframe,
        "--stop-loss", std::to_string(strategy->stop_loss),
        "--take-profit", std::to_string(strategy->take_profit),
        "--db", get_db_path()
    };

    auto py_result = python::execute("algo_trading/backtest_engine.py", args);
    if (!py_result.success) {
        result.error = py_result.error;
        return result;
    }

    // Parse JSON result
    try {
        auto j = json::parse(py_result.output);
        result.success = j.value("success", false);
        if (!result.success) {
            result.error = j.value("error", "Backtest failed");
            return result;
        }

        auto m = j.value("metrics", json::object());
        result.metrics.total_return      = m.value("total_return", 0.0);
        result.metrics.total_return_pct  = m.value("total_return_pct", 0.0);
        result.metrics.max_drawdown     = m.value("max_drawdown", 0.0);
        result.metrics.sharpe_ratio     = m.value("sharpe_ratio", 0.0);
        result.metrics.win_rate         = m.value("win_rate", 0.0);
        result.metrics.total_trades     = m.value("total_trades", 0);
        result.metrics.winning_trades   = m.value("winning_trades", 0);
        result.metrics.losing_trades    = m.value("losing_trades", 0);
        result.metrics.avg_win          = m.value("avg_win", 0.0);
        result.metrics.avg_loss         = m.value("avg_loss", 0.0);
        result.metrics.profit_factor    = m.value("profit_factor", 0.0);

        for (auto& t : j.value("trades", json::array())) {
            BacktestTrade bt;
            bt.symbol      = t.value("symbol", "");
            bt.side        = t.value("side", "");
            bt.entry_price = t.value("entry_price", 0.0);
            bt.exit_price  = t.value("exit_price", 0.0);
            bt.quantity    = t.value("quantity", 0.0);
            bt.pnl         = t.value("pnl", 0.0);
            bt.pnl_pct     = t.value("pnl_pct", 0.0);
            bt.entry_time  = t.value("entry_time", "");
            bt.exit_time   = t.value("exit_time", "");
            result.trades.push_back(bt);
        }

        for (auto& e : j.value("equity_curve", json::array())) {
            EquityPoint ep;
            ep.date     = e.value("date", "");
            ep.equity   = e.value("equity", 0.0);
            ep.drawdown = e.value("drawdown", 0.0);
            result.equity_curve.push_back(ep);
        }
    } catch (const std::exception& e) {
        result.error = std::string("Failed to parse backtest result: ") + e.what();
    }

    return result;
}

BacktestResult AlgoService::run_python_backtest(const std::string& code,
                                                  const std::string& symbol,
                                                  const std::string& start_date,
                                                  const std::string& end_date,
                                                  double capital) {
    BacktestResult result;

    std::vector<std::string> args = {
        "--symbol", symbol,
        "--start", start_date,
        "--end", end_date,
        "--capital", std::to_string(capital),
        "--db", get_db_path()
    };

    auto py_result = python::execute_with_stdin("algo_trading/python_backtest_engine.py", args, code);
    if (!py_result.success) {
        result.error = py_result.error;
        return result;
    }

    try {
        auto j = json::parse(py_result.output);
        result.success = j.value("success", false);
        result.error = j.value("error", "");
        // Parse metrics same as above (reuse pattern)
        auto m = j.value("metrics", json::object());
        result.metrics.total_return     = m.value("total_return", 0.0);
        result.metrics.total_return_pct = m.value("total_return_pct", 0.0);
        result.metrics.max_drawdown    = m.value("max_drawdown", 0.0);
        result.metrics.sharpe_ratio    = m.value("sharpe_ratio", 0.0);
        result.metrics.win_rate        = m.value("win_rate", 0.0);
        result.metrics.total_trades    = m.value("total_trades", 0);
    } catch (const std::exception& e) {
        result.error = std::string("Parse error: ") + e.what();
    }

    return result;
}

// ============================================================================
// Scanner
// ============================================================================

std::vector<ScanResult> AlgoService::run_scan(const std::string& conditions_json,
                                                const std::vector<std::string>& symbols,
                                                const std::string& timeframe,
                                                const std::string& provider) {
    std::vector<ScanResult> results;

    // Build symbols CSV
    std::string syms;
    for (size_t i = 0; i < symbols.size(); i++) {
        if (i > 0) syms += ",";
        syms += symbols[i];
    }

    std::vector<std::string> args = {
        "--conditions", conditions_json,
        "--symbols", syms,
        "--timeframe", timeframe,
        "--provider", provider,
        "--db", get_db_path()
    };

    auto& cache = CacheService::instance();
    std::string scan_key = CacheService::make_key("market-quotes", "algo-scan",
        syms.substr(0, std::min((size_t)48, syms.size())) + "_" + timeframe);

    std::string scan_out;
    auto cached_scan = cache.get(scan_key);
    if (cached_scan && !cached_scan->empty()) { scan_out = *cached_scan; }
    else {
        auto py_result = python::execute("algo_trading/scanner_engine.py", args);
        if (!py_result.success) return results;
        scan_out = py_result.output;
        cache.set(scan_key, scan_out, "market-quotes", CacheTTL::FIVE_MIN);
    }

    try {
        auto j = json::parse(scan_out);
        for (auto& r : j.value("results", json::array())) {
            ScanResult sr;
            sr.symbol      = r.value("symbol", "");
            sr.entry_match = r.value("entry_match", false);
            sr.exit_match  = r.value("exit_match", false);
            sr.last_price  = r.value("last_price", 0.0);
            sr.volume      = r.value("volume", 0.0);
            sr.details     = r.value("details", json::object()).dump();
            results.push_back(sr);
        }
    } catch (...) {}

    return results;
}

// ============================================================================
// Condition Evaluation
// ============================================================================

std::string AlgoService::evaluate_conditions(const std::string& conditions_json,
                                               const std::string& symbol,
                                               const std::string& timeframe) {
    std::vector<std::string> args = {
        "--conditions", conditions_json,
        "--symbol", symbol,
        "--timeframe", timeframe,
        "--db", get_db_path()
    };

    auto py_result = python::execute("algo_trading/condition_evaluator.py", args);
    return py_result.success ? py_result.output : json({{"error", py_result.error}}).dump();
}

// ============================================================================
// Deployment Count
// ============================================================================

int AlgoService::running_deployment_count() const {
    auto deployments = db::ops::list_algo_deployments();
    int count = 0;
    for (auto& d : deployments) {
        if (d.status == DeploymentStatus::Running || d.status == DeploymentStatus::Starting)
            count++;
    }
    return count;
}

// ============================================================================
// Python Strategy Library
// ============================================================================

std::string AlgoService::get_strategies_dir() const {
    // strategies/ sits next to algo_trading/ under scripts/
    auto base = std::filesystem::path(get_scripts_dir()).parent_path() / "strategies";
    return base.string();
}

std::vector<PythonStrategy> AlgoService::list_python_strategies() const {
    std::vector<PythonStrategy> result;
    auto dir = get_strategies_dir();
    auto registry_path = dir + "/_registry.py";

    std::ifstream f(registry_path);
    if (!f.is_open()) return result;

    std::string content((std::istreambuf_iterator<char>(f)),
                          std::istreambuf_iterator<char>());
    f.close();

    // Parse STRATEGY_REGISTRY entries: "FCT-XXXXX": {"name": "...", "category": "...", "path": "..."}
    std::string marker = "STRATEGY_REGISTRY = {";
    auto start = content.find(marker);
    if (start == std::string::npos) return result;

    std::string dict = content.substr(start + marker.size());

    size_t pos = 0;
    while (pos < dict.size()) {
        // Find "FCT-
        auto id_start = dict.find("\"FCT-", pos);
        if (id_start == std::string::npos) break;
        id_start += 1; // skip opening quote

        auto id_end = dict.find("\"", id_start);
        if (id_end == std::string::npos) break;
        std::string id = dict.substr(id_start, id_end - id_start);

        // Find the { ... } for this entry
        auto brace_start = dict.find("{", id_end);
        if (brace_start == std::string::npos) break;
        auto brace_end = dict.find("}", brace_start);
        if (brace_end == std::string::npos) break;

        std::string entry = dict.substr(brace_start, brace_end - brace_start + 1);

        // Extract fields with simple parsing
        auto extract = [&](const char* field) -> std::string {
            std::string key = std::string("\"") + field + "\"";
            auto kpos = entry.find(key);
            if (kpos == std::string::npos) {
                // Try single quotes
                key = std::string("'") + field + "'";
                kpos = entry.find(key);
                if (kpos == std::string::npos) return "";
            }
            auto colon = entry.find(":", kpos + key.size());
            if (colon == std::string::npos) return "";
            // Find the value (quoted string)
            auto val_start = entry.find_first_of("\"'", colon + 1);
            if (val_start == std::string::npos) return "";
            char quote = entry[val_start];
            auto val_end = entry.find(quote, val_start + 1);
            if (val_end == std::string::npos) return "";
            return entry.substr(val_start + 1, val_end - val_start - 1);
        };

        PythonStrategy ps;
        ps.id = id;
        ps.name = extract("name");
        ps.category = extract("category");
        ps.filename = extract("path");
        result.push_back(ps);

        pos = brace_end + 1;
    }

    return result;
}

std::vector<std::string> AlgoService::get_strategy_categories() const {
    auto strategies = list_python_strategies();
    std::vector<std::string> cats;
    for (auto& s : strategies) {
        bool found = false;
        for (auto& c : cats) {
            if (c == s.category) { found = true; break; }
        }
        if (!found && !s.category.empty()) cats.push_back(s.category);
    }
    std::sort(cats.begin(), cats.end());
    return cats;
}

std::string AlgoService::get_strategy_code(const std::string& strategy_id) const {
    auto strategies = list_python_strategies();
    std::string filename;
    for (auto& s : strategies) {
        if (s.id == strategy_id) { filename = s.filename; break; }
    }
    if (filename.empty()) return "# Strategy not found: " + strategy_id;

    auto path = get_strategies_dir() + "/" + filename;
    std::ifstream f(path);
    if (!f.is_open()) return "# File not found: " + path;

    std::string code((std::istreambuf_iterator<char>(f)),
                       std::istreambuf_iterator<char>());
    return code;
}

bool AlgoService::validate_python_syntax(const std::string& code, std::string& error_out) const {
    // Write code to temp file, then validate with python -c "import ast; ast.parse(open(...).read())"
    auto tmp = std::filesystem::temp_directory_path() / "fct_validate.py";
    {
        std::ofstream f(tmp);
        f << code;
    }

    std::string cmd = "python -c \"import ast; ast.parse(open('" +
                      tmp.string() + "').read())\" 2>&1";

    // Use popen to capture stderr
#ifdef _WIN32
    FILE* pipe = _popen(cmd.c_str(), "r");
#else
    FILE* pipe = popen(cmd.c_str(), "r");
#endif
    if (!pipe) {
        error_out = "Failed to run Python";
        return false;
    }

    char buf[256];
    std::string output;
    while (fgets(buf, sizeof(buf), pipe)) output += buf;
#ifdef _WIN32
    int ret = _pclose(pipe);
#else
    int ret = pclose(pipe);
#endif

    std::filesystem::remove(tmp);

    if (ret == 0) return true;
    error_out = output;
    return false;
}

std::vector<StrategyParameter> AlgoService::extract_strategy_parameters(const std::string& code) const {
    std::vector<StrategyParameter> params;

    // Look for patterns: UPPER_CASE_NAME = number
    // Simple line-by-line scan
    std::istringstream stream(code);
    std::string line;
    while (std::getline(stream, line)) {
        // Skip comments and empty lines
        auto trimmed = line;
        while (!trimmed.empty() && (trimmed[0] == ' ' || trimmed[0] == '\t'))
            trimmed.erase(0, 1);
        if (trimmed.empty() || trimmed[0] == '#') continue;

        // Match: UPPER_NAME = 123 or UPPER_NAME = 12.5
        size_t eq = trimmed.find('=');
        if (eq == std::string::npos || eq == 0) continue;
        // Make sure it's not ==
        if (eq + 1 < trimmed.size() && trimmed[eq + 1] == '=') continue;

        std::string name = trimmed.substr(0, eq);
        // trim trailing spaces
        while (!name.empty() && name.back() == ' ') name.pop_back();
        // Check if it's all uppercase + underscore
        bool all_upper = true;
        for (char c : name) {
            if (!((c >= 'A' && c <= 'Z') || c == '_' || (c >= '0' && c <= '9'))) {
                all_upper = false;
                break;
            }
        }
        if (!all_upper || name.empty()) continue;

        std::string val = trimmed.substr(eq + 1);
        while (!val.empty() && val[0] == ' ') val.erase(0, 1);
        // Strip trailing comment
        auto hash = val.find('#');
        if (hash != std::string::npos) val = val.substr(0, hash);
        while (!val.empty() && val.back() == ' ') val.pop_back();

        // Check if value is numeric
        bool is_float = false;
        bool is_numeric = !val.empty();
        for (char c : val) {
            if (c == '.') { is_float = true; continue; }
            if (c == '-' && &c == &val[0]) continue;
            if (c < '0' || c > '9') { is_numeric = false; break; }
        }
        if (!is_numeric) continue;

        StrategyParameter p;
        p.name = name;
        p.type = is_float ? "float" : "int";
        p.default_value = val;
        params.push_back(p);
    }

    return params;
}

} // namespace fincept::algo
