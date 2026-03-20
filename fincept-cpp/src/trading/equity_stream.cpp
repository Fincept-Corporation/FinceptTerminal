// EquityStreamManager — spawns broker_ws_bridge.py subprocess and streams ticks

#include "equity_stream.h"
#include "python/python_runner.h"
#include "core/logger.h"
#include <nlohmann/json.hpp>
#include <sstream>

#ifndef _WIN32
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#endif

namespace fincept::trading {

using json = nlohmann::json;
static const char* ESTAG = "EquityStream";

EquityStreamManager& EquityStreamManager::instance() {
    static EquityStreamManager s;
    return s;
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

void EquityStreamManager::start(const std::string& broker_adapter,
                                 const BrokerCredentials& creds,
                                 const std::vector<EquitySymbol>& symbols) {
    stop();  // clean up any previous stream

    if (broker_adapter.empty()) {
        LOG_INFO(ESTAG, "No WS adapter for this broker — REST polling only");
        return;
    }
    if (symbols.empty()) {
        LOG_INFO(ESTAG, "No symbols to stream");
        return;
    }

    running_   = true;
    connected_ = false;

    spawn_subprocess(broker_adapter, creds, symbols);

    reader_thread_ = std::thread([this]() {
        LOG_INFO(ESTAG, "Reader thread started");
        reader_loop();
        LOG_INFO(ESTAG, "Reader thread exited");
    });
}

void EquityStreamManager::stop() {
    if (!running_) return;
    running_   = false;
    connected_ = false;

    kill_subprocess();

    if (reader_thread_.joinable())
        reader_thread_.join();

    LOG_INFO(ESTAG, "Stream stopped");
}

bool EquityStreamManager::get_tick(const std::string& symbol, EquityTick& out) const {
    std::lock_guard<std::mutex> lk(mutex_);
    auto it = tick_cache_.find(symbol);
    if (it == tick_cache_.end()) return false;
    out = it->second;
    return true;
}

int EquityStreamManager::on_tick(TickCallback cb) {
    std::lock_guard<std::mutex> lk(mutex_);
    int id = next_cb_id_++;
    tick_cbs_[id] = std::move(cb);
    return id;
}

void EquityStreamManager::remove_tick_callback(int id) {
    std::lock_guard<std::mutex> lk(mutex_);
    tick_cbs_.erase(id);
}

int EquityStreamManager::on_status(StatusCallback cb) {
    std::lock_guard<std::mutex> lk(mutex_);
    int id = next_cb_id_++;
    status_cbs_[id] = std::move(cb);
    return id;
}

void EquityStreamManager::remove_status_callback(int id) {
    std::lock_guard<std::mutex> lk(mutex_);
    status_cbs_.erase(id);
}

// ─────────────────────────────────────────────────────────────────────────────
// Subprocess management
// ─────────────────────────────────────────────────────────────────────────────

void EquityStreamManager::spawn_subprocess(const std::string& broker,
                                            const BrokerCredentials& creds,
                                            const std::vector<EquitySymbol>& symbols) {
    auto python = python::resolve_python_path("exchange/broker_ws_bridge.py");
    if (python.empty()) {
        LOG_ERROR(ESTAG, "Python not found");
        running_ = false;
        return;
    }
    auto script = python::resolve_script_path("exchange/broker_ws_bridge.py");
    if (script.empty()) {
        LOG_ERROR(ESTAG, "broker_ws_bridge.py not found");
        running_ = false;
        return;
    }

    // Build --symbols args: SYMBOL:EXCHANGE (token unknown at this point → omit)
    std::string sym_args;
    for (const auto& s : symbols)
        sym_args += " " + s.symbol + ":" + s.exchange;

    // Pull credentials
    const std::string& api_key      = creds.api_key;
    const std::string& access_token = creds.access_token.empty() ? creds.api_secret : creds.access_token;
    const std::string& user_id      = creds.user_id.empty() ? api_key : creds.user_id;

    // Extract feed_token from additional_data (AngelOne stores it there)
    std::string feed_token;
    if (!creds.additional_data.empty()) {
        try {
            auto j = json::parse(creds.additional_data);
            feed_token = j.value("feed_token", "");
        } catch (...) {}
    }
    std::string feed_token_arg = feed_token.empty() ? "" : (" --feed-token " + feed_token);

    LOG_INFO(ESTAG, "Starting equity WS bridge: broker=%s symbols=%zu",
             broker.c_str(), symbols.size());

#ifdef _WIN32
    // "python -u -B broker_ws_bridge.py <broker> <api_key> <access_token> <user_id> --symbols SYM:EXCH ..."
    std::string cmd = "\"" + python.string() + "\" -u -B \""
        + script.string() + "\" "
        + broker + " "
        + api_key + " "
        + access_token + " "
        + user_id
        + feed_token_arg
        + " --symbols" + sym_args;

    SECURITY_ATTRIBUTES sa{};
    sa.nLength        = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hRead = nullptr, hWrite = nullptr;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) {
        LOG_ERROR(ESTAG, "Failed to create pipe");
        running_ = false;
        return;
    }
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si{};
    si.cb          = sizeof(si);
    si.hStdOutput  = hWrite;
    si.hStdError   = hWrite;
    si.dwFlags    |= STARTF_USESTDHANDLES;

    PROCESS_INFORMATION pi{};
    BOOL ok = CreateProcessA(nullptr, cmd.data(), nullptr, nullptr,
                              TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(hWrite);

    if (!ok) {
        LOG_ERROR(ESTAG, "Failed to spawn broker_ws_bridge.py");
        CloseHandle(hRead);
        running_ = false;
        return;
    }

    proc_handle_ = pi.hProcess;
    stdout_rd_   = hRead;
    CloseHandle(pi.hThread);
    LOG_INFO(ESTAG, "Bridge subprocess spawned");

#else
    int pipefd[2];
    if (pipe(pipefd) < 0) {
        LOG_ERROR(ESTAG, "Failed to create pipe");
        running_ = false;
        return;
    }

    pid_t pid = fork();
    if (pid < 0) {
        LOG_ERROR(ESTAG, "fork() failed");
        close(pipefd[0]); close(pipefd[1]);
        running_ = false;
        return;
    }

    if (pid == 0) {
        // Child
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);

        std::vector<const char*> argv;
        argv.push_back(python.c_str());
        argv.push_back("-u"); argv.push_back("-B");
        argv.push_back(script.c_str());
        argv.push_back(broker.c_str());
        argv.push_back(api_key.c_str());
        argv.push_back(access_token.c_str());
        argv.push_back(user_id.c_str());
        argv.push_back("--symbols");
        std::vector<std::string> sym_strs;
        for (const auto& s : symbols) {
            sym_strs.push_back(s.symbol + ":" + s.exchange);
            argv.push_back(sym_strs.back().c_str());
        }
        argv.push_back(nullptr);
        execvp(python.c_str(), const_cast<char* const*>(argv.data()));
        _exit(1);
    }

    close(pipefd[1]);
    proc_pid_  = pid;
    stdout_fd_ = pipefd[0];
    LOG_INFO(ESTAG, "Bridge subprocess spawned (PID=%d)", pid);
#endif
}

void EquityStreamManager::kill_subprocess() {
#ifdef _WIN32
    if (proc_handle_) {
        TerminateProcess((HANDLE)proc_handle_, 0);
        WaitForSingleObject((HANDLE)proc_handle_, 3000);
        CloseHandle((HANDLE)proc_handle_);
        proc_handle_ = nullptr;
    }
    if (stdout_rd_) {
        CloseHandle((HANDLE)stdout_rd_);
        stdout_rd_ = nullptr;
    }
#else
    if (proc_pid_ > 0) {
        kill(proc_pid_, SIGTERM);
        int st; waitpid(proc_pid_, &st, WNOHANG);
        proc_pid_ = -1;
    }
    if (stdout_fd_ >= 0) {
        close(stdout_fd_);
        stdout_fd_ = -1;
    }
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
// Reader loop — reads stdout line by line, parses JSON
// ─────────────────────────────────────────────────────────────────────────────

void EquityStreamManager::reader_loop() {
    std::string buf;

#ifdef _WIN32
    HANDLE rd = (HANDLE)stdout_rd_;
    char ch;
    DWORD n;

    while (running_) {
        if (!ReadFile(rd, &ch, 1, &n, nullptr) || n == 0) {
            LOG_INFO(ESTAG, "Bridge pipe closed");
            break;
        }
        if (ch == '\n') {
            if (!buf.empty()) handle_message(buf);
            buf.clear();
        } else if (ch != '\r') {
            buf += ch;
        }
    }
#else
    int fd = stdout_fd_;
    char ch;

    while (running_) {
        ssize_t n = read(fd, &ch, 1);
        if (n <= 0) {
            LOG_INFO(ESTAG, "Bridge pipe closed");
            break;
        }
        if (ch == '\n') {
            if (!buf.empty()) handle_message(buf);
            buf.clear();
        } else if (ch != '\r') {
            buf += ch;
        }
    }
#endif

    connected_ = false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Message handler
// ─────────────────────────────────────────────────────────────────────────────

void EquityStreamManager::handle_message(const std::string& line) {
    if (line.empty() || line[0] != '{') {
        LOG_DEBUG(ESTAG, "Bridge: %s", line.c_str());
        return;
    }

    json msg;
    try {
        msg = json::parse(line);
    } catch (...) {
        LOG_ERROR(ESTAG, "JSON parse error: %.120s", line.c_str());
        return;
    }

    const std::string type = msg.value("type", "");

    // ── status ──────────────────────────────────────────────────────────────
    if (type == "status") {
        bool conn = msg.value("connected", false);
        std::string message = msg.value("message", "");
        connected_ = conn;
        LOG_INFO(ESTAG, "Bridge status: connected=%s msg=%s",
                 conn ? "true" : "false", message.c_str());

        std::vector<StatusCallback> cbs;
        {
            std::lock_guard<std::mutex> lk(mutex_);
            for (auto& [_, cb] : status_cbs_) cbs.push_back(cb);
        }
        for (auto& cb : cbs) try { cb(conn, message); } catch (...) {}
        return;
    }

    // ── error ────────────────────────────────────────────────────────────────
    if (type == "error") {
        LOG_ERROR(ESTAG, "Bridge error: %s", msg.value("message", "?").c_str());
        return;
    }

    // ── tick ─────────────────────────────────────────────────────────────────
    if (type == "tick") {
        EquityTick t;
        t.symbol    = msg.value("symbol",   "");
        t.exchange  = msg.value("exchange", "");
        t.ltp       = msg.value("ltp",       0.0);
        t.open      = msg.value("open",      0.0);
        t.high      = msg.value("high",      0.0);
        t.low       = msg.value("low",       0.0);
        t.close     = msg.value("close",     0.0);
        t.volume    = msg.value("volume",    0.0);
        t.bid       = msg.value("bid",       0.0);
        t.ask       = msg.value("ask",       0.0);
        t.timestamp = msg.value("timestamp", int64_t{0});

        if (t.symbol.empty() || t.ltp <= 0.0) return;

        std::vector<TickCallback> cbs;
        {
            std::lock_guard<std::mutex> lk(mutex_);
            tick_cache_[t.symbol] = t;
            last_tick_time_ = std::chrono::steady_clock::now();
            for (auto& [_, cb] : tick_cbs_) cbs.push_back(cb);
        }
        for (auto& cb : cbs) try { cb(t); } catch (...) {}
        return;
    }

    LOG_DEBUG(ESTAG, "Bridge unknown message type: %s", type.c_str());
}

} // namespace fincept::trading
