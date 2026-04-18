// McpClient.cpp — JSON-RPC 2.0 over stdio for external MCP servers (Qt port)

#include "mcp/McpClient.h"

#include "core/logging/Logger.h"
#include "python/PythonSetupManager.h"

#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonDocument>
#include <QMetaObject>
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

    // Create a dedicated worker thread with its own event loop so that
    // QProcess signals are delivered there. start() is always called from a
    // background thread (never the UI thread), so blocking here is safe.
    worker_thread_ = new QThread;
    worker_thread_->setObjectName("mcp-" + config_.id);

    process_ = new QProcess; // no parent — we'll move it to worker thread

    // Merge environment
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    for (auto it = config_.env.constBegin(); it != config_.env.constEnd(); ++it)
        env.insert(it.key(), it.value());
    process_->setProcessEnvironment(env);

    // Wire signals — they'll fire on worker_thread_ once process_ is moved
    connect(process_, &QProcess::readyReadStandardOutput, this, &McpClient::on_ready_read, Qt::DirectConnection);
    connect(
        process_, &QProcess::readyReadStandardError, this,
        [this]() {
            while (process_ && process_->canReadLine()) {
                const QString line = QString::fromUtf8(process_->readLine()).trimmed();
                if (!line.isEmpty())
                    append_log("[stderr] " + line);
            }
        },
        Qt::DirectConnection);
    connect(process_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &McpClient::on_finished,
            Qt::DirectConnection);

    // Route uvx through the app's bundled uv
    QString command = config_.command;
    QStringList args = config_.args;
    if (command == "uvx") {
        const QString uv = python::PythonSetupManager::instance().uv_path();
        if (QFileInfo::exists(uv)) {
            command = uv;
            args.prepend("run");
            args.prepend("tool"); // becomes: uv tool run <package> <args>
            LOG_INFO(TAG, "Routing uvx through bundled uv: " + uv);
        }
    }

#ifdef _WIN32
    process_->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* cpa) {
        cpa->flags |= 0x08000000; // CREATE_NO_WINDOW
    });
#endif

    // Move QProcess to worker thread and start it there
    process_->moveToThread(worker_thread_);
    worker_thread_->start();

    // BlockingQueuedConnection is safe here — start() is always called from a
    // background thread (McpService auto-start thread), never from the UI thread.
    bool started_ok = false;
    QMetaObject::invokeMethod(
        process_,
        [this, command, args, &started_ok]() {
            process_->start(command, args);
            started_ok = process_->waitForStarted(60000); // 60s for first-time uv downloads
        },
        Qt::BlockingQueuedConnection);

    if (!started_ok) {
        const QString err = process_->errorString();
        LOG_ERROR(TAG, "Failed to start MCP server: " + config_.name + " — " + err);
        worker_thread_->quit();
        worker_thread_->wait(3000);
        cleanup_process();
        delete worker_thread_;
        worker_thread_ = nullptr;
        return Result<void>::err("Failed to start: " + config_.name.toStdString() + " — " + err.toStdString());
    }

    running_ = true;
    LOG_INFO(TAG, "Started MCP server process: " + config_.name);
    return Result<void>::ok();
}

void McpClient::cleanup_process() {
    if (process_) {
        delete process_;
        process_ = nullptr;
    }
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
        // Ask the worker thread to terminate the process, then quit its event loop.
        // We use QueuedConnection so the worker thread handles terminate() in its
        // own event loop — no BlockingQueuedConnection deadlock risk.
        // After quit(), wait() drains the thread and the process dies with it.
        QMetaObject::invokeMethod(process_, [this]() { process_->terminate(); }, Qt::QueuedConnection);
    }

    if (worker_thread_) {
        worker_thread_->quit();
        // Wait up to 4s for the worker to drain terminate() and exit cleanly.
        // If it hangs, kill the process directly before giving up.
        if (!worker_thread_->wait(4000) && process_) {
            process_->kill();
            worker_thread_->wait(1000);
        }
        delete worker_thread_;
        worker_thread_ = nullptr;
    }

    cleanup_process();
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
    client_info["version"] = "4.0.2";
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
    // 120s timeout for tool execution — external tools like database queries
    // (Postgres, MySQL, etc.) can take significantly longer than 30s,
    // especially for schema introspection or large result sets.
    return send_request("tools/call", params, 120000);
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

    LOG_INFO(TAG, "RPC → " + method + " (id=" + QString::number(id) + ")");

    QByteArray line = QJsonDocument(req).toJson(QJsonDocument::Compact) + "\n";

    // Write to process on its worker thread
    QMetaObject::invokeMethod(process_, [this, line]() { process_->write(line); }, Qt::QueuedConnection);

    // Wait for response via QWaitCondition. QProcess lives on its own
    // worker thread with an event loop, so readyRead signals fire there
    // and handle_line() wakes us up via rpc_cond_.
    QElapsedTimer timer;
    timer.start();

    {
        QMutexLocker lock(&rpc_mutex_);
        if (!pending.completed) {
            rpc_cond_.wait(&rpc_mutex_, static_cast<unsigned long>(timeout_ms));
        }
    }

    {
        QMutexLocker lock(&rpc_mutex_);
        pending_.remove(id);
    }

    if (!pending.error.isEmpty())
        return Result<QJsonObject>::err(pending.error.toStdString());
    if (!pending.completed)
        return Result<QJsonObject>::err("Timeout waiting for: " + method.toStdString());

    LOG_INFO(TAG, "RPC ← " + method + " OK (" + QString::number(timer.elapsed()) + "ms)");
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
        append_log("[stdout] " + QString::fromUtf8(line).trimmed());
        handle_line(line);
    }
}

QStringList McpClient::get_logs() const {
    QMutexLocker lock(&log_mutex_);
    return log_lines_;
}

void McpClient::append_log(const QString& line) {
    QMutexLocker lock(&log_mutex_);
    log_lines_.append(line);
    if (log_lines_.size() > MAX_LOG_LINES)
        log_lines_.removeFirst();
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
