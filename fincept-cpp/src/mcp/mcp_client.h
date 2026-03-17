#pragma once
// MCPClient — JSON-RPC 2.0 client over stdio for external MCP servers
// Spawns a child process (npx, uvx, node, etc.) and communicates via stdin/stdout.
// Each MCPClient instance manages one external server connection.

#include "mcp_types.h"
#include "../core/result.h"
#include <thread>
#include <atomic>
#include <condition_variable>
#include <future>
#include <queue>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fincept::mcp {

using core::Result;
using core::Error;

// ============================================================================
// MCPClient — one-per-server stdio transport
// ============================================================================

class MCPClient {
public:
    explicit MCPClient(const MCPServerConfig& config);
    ~MCPClient();

    MCPClient(const MCPClient&) = delete;
    MCPClient& operator=(const MCPClient&) = delete;

    // Lifecycle
    Result<void> start();
    void stop();
    bool is_running() const { return running_.load(); }

    // MCP Protocol
    Result<json> initialize();
    Result<std::vector<ExternalTool>> list_tools();
    Result<json> call_tool(const std::string& tool_name, const json& args);
    Result<void> ping();

    // Server info
    const MCPServerConfig& config() const { return config_; }
    const std::string& server_id() const { return config_.id; }

private:
    MCPServerConfig config_;
    std::atomic<bool> running_{false};
    int next_id_ = 1;

    // JSON-RPC request/response
    Result<json> send_request(const std::string& method, const json& params = json::object(),
                              int timeout_ms = 30000);
    bool write_message(const std::string& msg);
    Result<std::string> read_message(int timeout_ms);

    // Reader thread
    std::thread reader_thread_;
    void reader_loop();

    // Pending requests
    struct PendingRequest {
        std::promise<json> promise;
    };
    std::mutex pending_mutex_;
    std::unordered_map<int, std::shared_ptr<PendingRequest>> pending_requests_;

    // Incoming message queue (for notifications)
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::queue<json> notification_queue_;

    // Process handles
#ifdef _WIN32
    HANDLE child_stdin_write_ = INVALID_HANDLE_VALUE;
    HANDLE child_stdout_read_ = INVALID_HANDLE_VALUE;
    HANDLE process_handle_ = INVALID_HANDLE_VALUE;
    HANDLE thread_handle_ = INVALID_HANDLE_VALUE;
#else
    int child_stdin_fd_ = -1;
    int child_stdout_fd_ = -1;
    pid_t child_pid_ = -1;
#endif

    Result<void> spawn_process();
    void kill_process();

    // Resolve command (handle npx, uvx, bunx)
    std::string resolve_command() const;
};

} // namespace fincept::mcp
