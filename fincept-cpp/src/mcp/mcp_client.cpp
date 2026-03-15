// MCPClient — JSON-RPC 2.0 over stdio implementation
// Spawns external MCP servers and communicates via stdin/stdout pipes.

#include "mcp_client.h"
#include "../core/logger.h"
#include <sstream>
#include <chrono>
#include <cstdlib>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#endif

namespace fincept::mcp {

static constexpr const char* TAG_CLIENT = "MCPClient";

MCPClient::MCPClient(const MCPServerConfig& config) : config_(config) {}

MCPClient::~MCPClient() {
    stop();
}

// ============================================================================
// Lifecycle
// ============================================================================

Result<void> MCPClient::start() {
    if (running_.load()) return {};

    auto result = spawn_process();
    if (result.failed()) return result;

    running_ = true;

    // Start reader thread
    reader_thread_ = std::thread(&MCPClient::reader_loop, this);

    LOG_INFO(TAG_CLIENT, "Started MCP server: %s (%s)", config_.name.c_str(), config_.id.c_str());
    return {};
}

void MCPClient::stop() {
    if (!running_.load()) return;

    running_ = false;

    // Cancel all pending requests
    {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        for (auto& [id, req] : pending_requests_) {
            try {
                req->promise.set_value(json{{"error", "Server shutting down"}});
            } catch (...) {}
        }
        pending_requests_.clear();
    }

    // Wake up any waiting threads
    queue_cv_.notify_all();

    kill_process();

    if (reader_thread_.joinable()) {
        reader_thread_.join();
    }

    LOG_INFO(TAG_CLIENT, "Stopped MCP server: %s", config_.name.c_str());
}

// ============================================================================
// MCP Protocol
// ============================================================================

Result<json> MCPClient::initialize() {
    json params = {
        {"protocolVersion", "2024-11-05"},
        {"capabilities", {
            {"roots", {{"listChanged", false}}}
        }},
        {"clientInfo", {
            {"name", "fincept-terminal"},
            {"version", "1.0.0"}
        }}
    };

    auto result = send_request("initialize", params, 120000); // 120s timeout for init
    if (result.failed()) return result;

    // Send initialized notification (no response expected)
    json notif = {
        {"jsonrpc", "2.0"},
        {"method", "notifications/initialized"}
    };
    write_message(notif.dump());

    return result;
}

Result<std::vector<ExternalTool>> MCPClient::list_tools() {
    auto result = send_request("tools/list");
    if (result.failed()) return Error(result.error().message);

    std::vector<ExternalTool> tools;
    try {
        auto& tools_arr = result.value()["tools"];
        for (auto& t : tools_arr) {
            ExternalTool tool;
            tool.server_id = config_.id;
            tool.server_name = config_.name;
            tool.name = t.value("name", "");
            tool.description = t.value("description", "");
            tool.input_schema = t.value("inputSchema", json::object());
            tools.push_back(std::move(tool));
        }
    } catch (const std::exception& e) {
        return Error(std::string("Failed to parse tools list: ") + e.what());
    }

    return tools;
}

Result<json> MCPClient::call_tool(const std::string& tool_name, const json& args) {
    json params = {
        {"name", tool_name},
        {"arguments", args}
    };
    return send_request("tools/call", params);
}

Result<void> MCPClient::ping() {
    auto result = send_request("ping", json::object(), 5000);
    if (result.failed()) return Error(result.error().message);
    return {};
}

// ============================================================================
// JSON-RPC Transport
// ============================================================================

Result<json> MCPClient::send_request(const std::string& method, const json& params, int timeout_ms) {
    if (!running_.load()) return Error("Server not running");

    int id = next_id_++;

    json request = {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"method", method},
        {"params", params}
    };

    // Register pending request
    auto pending = std::make_shared<PendingRequest>();
    auto future = pending->promise.get_future();

    {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        pending_requests_[id] = pending;
    }

    // Send the request
    std::string msg = request.dump();
    if (!write_message(msg)) {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        pending_requests_.erase(id);
        return Error("Failed to write to server stdin");
    }

    LOG_DEBUG(TAG_CLIENT, "[%s] -> %s (id=%d)", config_.id.c_str(), method.c_str(), id);

    // Wait for response with timeout
    auto status = future.wait_for(std::chrono::milliseconds(timeout_ms));
    if (status == std::future_status::timeout) {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        pending_requests_.erase(id);
        return Error("Request timed out: " + method);
    }

    json response = future.get();

    // Check for JSON-RPC error
    if (response.contains("error")) {
        auto& err = response["error"];
        std::string msg_str = err.value("message", "Unknown error");
        int code = err.value("code", -1);
        return Error(msg_str, code);
    }

    return response.value("result", json::object());
}

bool MCPClient::write_message(const std::string& msg) {
    // MCP uses newline-delimited JSON
    std::string line = msg + "\n";

#ifdef _WIN32
    if (child_stdin_write_ == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    return WriteFile(child_stdin_write_, line.c_str(), (DWORD)line.size(), &written, NULL) != 0;
#else
    if (child_stdin_fd_ < 0) return false;
    ssize_t written = ::write(child_stdin_fd_, line.c_str(), line.size());
    return written == (ssize_t)line.size();
#endif
}

// ============================================================================
// Reader Thread — reads stdout, dispatches responses
// ============================================================================

void MCPClient::reader_loop() {
    std::string buffer;
    char chunk[4096];

    while (running_.load()) {
#ifdef _WIN32
        DWORD bytes_read = 0;
        BOOL ok = ReadFile(child_stdout_read_, chunk, sizeof(chunk) - 1, &bytes_read, NULL);
        if (!ok || bytes_read == 0) {
            if (running_.load()) {
                LOG_WARN(TAG_CLIENT, "[%s] stdout read failed or EOF", config_.id.c_str());
            }
            break;
        }
        chunk[bytes_read] = '\0';
#else
        ssize_t bytes_read = ::read(child_stdout_fd_, chunk, sizeof(chunk) - 1);
        if (bytes_read <= 0) {
            if (running_.load()) {
                LOG_WARN(TAG_CLIENT, "[%s] stdout read failed or EOF", config_.id.c_str());
            }
            break;
        }
        chunk[bytes_read] = '\0';
#endif

        buffer.append(chunk, bytes_read);

        // Process complete lines (newline-delimited JSON)
        size_t pos;
        while ((pos = buffer.find('\n')) != std::string::npos) {
            std::string line = buffer.substr(0, pos);
            buffer.erase(0, pos + 1);

            if (line.empty() || line[0] != '{') continue;

            try {
                json msg = json::parse(line);

                if (msg.contains("id") && !msg["id"].is_null()) {
                    // Response to a request
                    int id = msg["id"].get<int>();
                    std::lock_guard<std::mutex> lock(pending_mutex_);
                    auto it = pending_requests_.find(id);
                    if (it != pending_requests_.end()) {
                        try {
                            it->second->promise.set_value(msg);
                        } catch (...) {}
                        pending_requests_.erase(it);
                    }
                } else {
                    // Notification
                    std::lock_guard<std::mutex> lock(queue_mutex_);
                    notification_queue_.push(msg);
                    queue_cv_.notify_one();
                }
            } catch (const json::parse_error& e) {
                LOG_DEBUG(TAG_CLIENT, "[%s] Non-JSON line: %s", config_.id.c_str(), line.substr(0, 100).c_str());
            }
        }
    }

    running_ = false;
}

// ============================================================================
// Process Management
// ============================================================================

std::string MCPClient::resolve_command() const {
    const auto& cmd = config_.command;

#ifdef _WIN32
    // On Windows, resolve npx/bunx/uvx to .cmd variants
    if (cmd == "npx" || cmd == "bunx" || cmd == "uvx" || cmd == "node" || cmd == "python" || cmd == "python3") {
        // Try cmd.exe wrapper — Windows needs this for .cmd scripts
        return cmd;
    }
#endif

    return cmd;
}

Result<void> MCPClient::spawn_process() {
    std::string cmd = resolve_command();
    LOG_INFO(TAG_CLIENT, "Spawning: %s %s", cmd.c_str(),
             config_.args.empty() ? "" : config_.args[0].c_str());

#ifdef _WIN32
    // Create pipes for stdin/stdout
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    HANDLE stdin_read = INVALID_HANDLE_VALUE, stdin_write = INVALID_HANDLE_VALUE;
    HANDLE stdout_read = INVALID_HANDLE_VALUE, stdout_write = INVALID_HANDLE_VALUE;

    if (!CreatePipe(&stdin_read, &stdin_write, &sa, 0))
        return Error("Failed to create stdin pipe");
    if (!CreatePipe(&stdout_read, &stdout_write, &sa, 0)) {
        CloseHandle(stdin_read); CloseHandle(stdin_write);
        return Error("Failed to create stdout pipe");
    }

    // Ensure our ends are not inherited
    SetHandleInformation(stdin_write, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0);

    // Build command line
    std::string cmdline = cmd;
    for (const auto& arg : config_.args) {
        cmdline += " " + arg;
    }

    // Set up environment block if needed
    std::vector<char> env_block;
    if (!config_.env.empty()) {
        // Get current environment
        LPCH current_env = GetEnvironmentStrings();
        if (current_env) {
            LPCH p = current_env;
            while (*p) {
                std::string var(p);
                env_block.insert(env_block.end(), var.begin(), var.end());
                env_block.push_back('\0');
                p += var.size() + 1;
            }
            FreeEnvironmentStrings(current_env);
        }
        // Add custom env vars
        for (const auto& [key, val] : config_.env) {
            std::string entry = key + "=" + val;
            env_block.insert(env_block.end(), entry.begin(), entry.end());
            env_block.push_back('\0');
        }
        env_block.push_back('\0'); // Double null terminator
    }

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.hStdInput = stdin_read;
    si.hStdOutput = stdout_write;
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    si.dwFlags |= STARTF_USESTDHANDLES;

    PROCESS_INFORMATION pi = {};

    // CREATE_NO_WINDOW to avoid flashing console
    DWORD flags = CREATE_NO_WINDOW;

    BOOL success = CreateProcessA(
        NULL,
        const_cast<char*>(cmdline.c_str()),
        NULL, NULL, TRUE, flags,
        env_block.empty() ? NULL : env_block.data(),
        NULL,
        &si, &pi
    );

    // Close child-side pipe handles
    CloseHandle(stdin_read);
    CloseHandle(stdout_write);

    if (!success) {
        CloseHandle(stdin_write);
        CloseHandle(stdout_read);
        DWORD err = GetLastError();
        return Error("Failed to spawn process: error code " + std::to_string(err));
    }

    child_stdin_write_ = stdin_write;
    child_stdout_read_ = stdout_read;
    process_handle_ = pi.hProcess;
    thread_handle_ = pi.hThread;

    return {};

#else
    // Unix: fork + exec with pipes
    int stdin_pipe[2], stdout_pipe[2];

    if (pipe(stdin_pipe) < 0) return Error("Failed to create stdin pipe");
    if (pipe(stdout_pipe) < 0) {
        close(stdin_pipe[0]); close(stdin_pipe[1]);
        return Error("Failed to create stdout pipe");
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(stdin_pipe[0]); close(stdin_pipe[1]);
        close(stdout_pipe[0]); close(stdout_pipe[1]);
        return Error("Failed to fork");
    }

    if (pid == 0) {
        // Child process
        dup2(stdin_pipe[0], STDIN_FILENO);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        close(stdin_pipe[0]); close(stdin_pipe[1]);
        close(stdout_pipe[0]); close(stdout_pipe[1]);

        // Set environment variables
        for (const auto& [key, val] : config_.env) {
            setenv(key.c_str(), val.c_str(), 1);
        }

        // Build argv
        std::vector<const char*> argv;
        argv.push_back(cmd.c_str());
        for (const auto& arg : config_.args) {
            argv.push_back(arg.c_str());
        }
        argv.push_back(nullptr);

        execvp(cmd.c_str(), const_cast<char* const*>(argv.data()));
        _exit(127);
    }

    // Parent
    close(stdin_pipe[0]);
    close(stdout_pipe[1]);

    child_stdin_fd_ = stdin_pipe[1];
    child_stdout_fd_ = stdout_pipe[0];
    child_pid_ = pid;

    return {};
#endif
}

void MCPClient::kill_process() {
#ifdef _WIN32
    if (child_stdin_write_ != INVALID_HANDLE_VALUE) {
        CloseHandle(child_stdin_write_);
        child_stdin_write_ = INVALID_HANDLE_VALUE;
    }
    if (child_stdout_read_ != INVALID_HANDLE_VALUE) {
        CloseHandle(child_stdout_read_);
        child_stdout_read_ = INVALID_HANDLE_VALUE;
    }
    if (process_handle_ != INVALID_HANDLE_VALUE) {
        TerminateProcess(process_handle_, 0);
        WaitForSingleObject(process_handle_, 5000);
        CloseHandle(process_handle_);
        process_handle_ = INVALID_HANDLE_VALUE;
    }
    if (thread_handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(thread_handle_);
        thread_handle_ = INVALID_HANDLE_VALUE;
    }
#else
    if (child_stdin_fd_ >= 0) { close(child_stdin_fd_); child_stdin_fd_ = -1; }
    if (child_stdout_fd_ >= 0) { close(child_stdout_fd_); child_stdout_fd_ = -1; }
    if (child_pid_ > 0) {
        kill(child_pid_, SIGTERM);
        int status;
        // Wait up to 5s
        for (int i = 0; i < 50; ++i) {
            if (waitpid(child_pid_, &status, WNOHANG) != 0) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        // Force kill if still alive
        kill(child_pid_, SIGKILL);
        waitpid(child_pid_, &status, 0);
        child_pid_ = -1;
    }
#endif
}

} // namespace fincept::mcp
