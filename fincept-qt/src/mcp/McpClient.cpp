// McpClient.cpp — JSON-RPC 2.0 over stdio for external MCP servers (Qt port)

#include "mcp/McpClient.h"

#include "core/logging/Logger.h"

#include <QJsonDocument>
#include <QProcessEnvironment>

namespace fincept::mcp {

static constexpr const char* TAG = "McpClient";

McpClient::McpClient(const McpServerConfig& config, QObject* parent) : QObject(parent), config_(config) {}

McpClient::~McpClient() {
    stop();
}

// ============================================================================
// Lifecycle
// ============================================================================

Result<void> McpClient::start() {
    if (running_)
        return Result<void>::ok();

    process_ = new QProcess(this);

    // Merge environment
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    for (auto it = config_.env.constBegin(); it != config_.env.constEnd(); ++it)
        env.insert(it.key(), it.value());
    process_->setProcessEnvironment(env);

    // Wire signals — must be done before start() so we don't miss output
    connect(process_, &QProcess::readyReadStandardOutput, this, &McpClient::on_ready_read);
    connect(process_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &McpClient::on_finished);

    process_->start(config_.command, config_.args);
    if (!process_->waitForStarted(10000)) {
        LOG_ERROR(TAG, "Failed to start MCP server: " + config_.name);
        process_->deleteLater();
        process_ = nullptr;
        return Result<void>::err("Failed to start process: " + config_.name.toStdString());
    }

    running_ = true;
    LOG_INFO(TAG, "Started MCP server: " + config_.name);
    return Result<void>::ok();
}

void McpClient::stop() {
    if (!running_)
        return;
    running_ = false;

    // Wake any pending requests with an error
    {
        QMutexLocker lock(&rpc_mutex_);
        for (auto* req : pending_) {
            req->error = "Server stopped";
            req->completed = true;
        }
        rpc_cond_.wakeAll();
    }

    if (process_) {
        process_->terminate();
        if (!process_->waitForFinished(3000))
            process_->kill();
        process_->deleteLater();
        process_ = nullptr;
    }

    LOG_INFO(TAG, "Stopped MCP server: " + config_.name);
}

bool McpClient::is_running() const {
    return running_ && process_ && process_->state() == QProcess::Running;
}

// ============================================================================
// MCP Protocol Methods
// ============================================================================

Result<QJsonObject> McpClient::initialize() {
    QJsonObject params;
    params["protocolVersion"] = "2024-11-05";

    QJsonObject client_info;
    client_info["name"] = "FinceptTerminal";
    client_info["version"] = "4.0.0";
    params["clientInfo"] = client_info;

    QJsonObject capabilities;
    params["capabilities"] = capabilities;

    return send_request("initialize", params, 120000); // 120s for slow servers
}

Result<std::vector<ExternalTool>> McpClient::list_tools() {
    auto result = send_request("tools/list", {});
    if (result.is_err())
        return Result<std::vector<ExternalTool>>::err(result.error());

    const QJsonObject& data = result.value();
    QJsonArray tools_json = data["tools"].toArray();

    std::vector<ExternalTool> tools;
    tools.reserve(static_cast<std::size_t>(tools_json.size()));

    for (const auto& item : tools_json) {
        QJsonObject t = item.toObject();
        ExternalTool et;
        et.server_id = config_.id;
        et.server_name = config_.name;
        et.name = t["name"].toString();
        et.description = t["description"].toString();
        et.input_schema = t["inputSchema"].toObject();
        tools.push_back(std::move(et));
    }

    return Result<std::vector<ExternalTool>>::ok(std::move(tools));
}

Result<QJsonObject> McpClient::call_tool(const QString& name, const QJsonObject& args) {
    QJsonObject params;
    params["name"] = name;
    params["arguments"] = args;
    return send_request("tools/call", params, 30000);
}

Result<void> McpClient::ping() {
    auto r = send_request("ping", {}, 5000);
    if (r.is_err())
        return Result<void>::err(r.error());
    return Result<void>::ok();
}

// ============================================================================
// JSON-RPC internals
// ============================================================================

Result<QJsonObject> McpClient::send_request(const QString& method, const QJsonObject& params, int timeout_ms) {
    if (!is_running())
        return Result<QJsonObject>::err("Server not running: " + config_.name.toStdString());

    int id;
    PendingRequest pending;

    {
        QMutexLocker lock(&rpc_mutex_);
        id = next_id_++;
        pending_[id] = &pending;
    }

    // Build JSON-RPC 2.0 request
    QJsonObject req;
    req["jsonrpc"] = "2.0";
    req["id"] = id;
    req["method"] = method;
    if (!params.isEmpty())
        req["params"] = params;

    QByteArray line = QJsonDocument(req).toJson(QJsonDocument::Compact) + "\n";
    process_->write(line);

    // Wait for response
    {
        QMutexLocker lock(&rpc_mutex_);
        if (!pending.completed) {
            rpc_cond_.wait(&rpc_mutex_, static_cast<unsigned long>(timeout_ms));
        }
        pending_.remove(id);

        if (!pending.error.isEmpty())
            return Result<QJsonObject>::err(pending.error.toStdString());
        if (!pending.completed)
            return Result<QJsonObject>::err("Timeout waiting for: " + method.toStdString());
    }

    return Result<QJsonObject>::ok(pending.response);
}

void McpClient::handle_line(const QByteArray& line) {
    if (line.trimmed().isEmpty())
        return;

    QJsonDocument doc = QJsonDocument::fromJson(line);
    if (!doc.isObject())
        return;

    QJsonObject obj = doc.object();
    if (!obj.contains("id"))
        return; // Notification — ignored for now

    int id = obj["id"].toInt(-1);
    if (id < 0)
        return;

    QMutexLocker lock(&rpc_mutex_);
    PendingRequest* req = pending_.value(id, nullptr);
    if (!req)
        return;

    if (obj.contains("error")) {
        req->error = obj["error"].toObject()["message"].toString("RPC error");
    } else {
        req->response = obj["result"].toObject();
    }
    req->completed = true;
    rpc_cond_.wakeAll();
}

void McpClient::on_ready_read() {
    while (process_ && process_->canReadLine()) {
        QByteArray line = process_->readLine();
        handle_line(line);
    }
}

void McpClient::on_finished(int exit_code, QProcess::ExitStatus) {
    running_ = false;
    LOG_WARN(TAG, QString("MCP server '%1' exited with code %2").arg(config_.name).arg(exit_code));

    // Wake pending requests
    QMutexLocker lock(&rpc_mutex_);
    for (auto* req : pending_) {
        req->error = "Server process exited";
        req->completed = true;
    }
    rpc_cond_.wakeAll();
}

} // namespace fincept::mcp
