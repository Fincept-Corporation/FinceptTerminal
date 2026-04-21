// McpManager.cpp — External MCP server lifecycle management (Qt port)

#include "mcp/McpManager.h"

#include "core/logging/Logger.h"
#include "storage/repositories/McpServerRepository.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

namespace fincept::mcp {

static constexpr const char* TAG = "McpManager";

McpManager::McpManager() {
    health_timer_ = new QTimer(this);
    health_timer_->setSingleShot(false);
    connect(health_timer_, &QTimer::timeout, this, &McpManager::do_health_check);
}

McpManager& McpManager::instance() {
    static McpManager s;
    return s;
}

// ============================================================================
// Configuration
// ============================================================================

void McpManager::initialize() {
    QMutexLocker lock(&mutex_);
    configs_.clear();

    auto result = McpServerRepository::instance().list_all();
    if (result.is_err()) {
        LOG_WARN(TAG, "Failed to load MCP server configs: " + QString::fromStdString(result.error()));
        return;
    }

    for (const auto& srv : result.value()) {
        McpServerConfig cfg;
        cfg.id = srv.id;
        cfg.name = srv.name;
        cfg.description = srv.description;
        cfg.command = srv.command;
        cfg.category = srv.category;
        cfg.enabled = srv.enabled;
        cfg.auto_start = srv.auto_start;
        cfg.status = ServerStatus::Stopped;

        // Parse args: Try JSON array first (supports spaces), fallback to legacy space-split
        bool args_parsed = false;
        if (srv.args.trimmed().startsWith('[')) {
            QJsonDocument doc = QJsonDocument::fromJson(srv.args.toUtf8());
            if (doc.isArray()) {
                QJsonArray arr = doc.array();
                for (const auto& val : arr) {
                    if (val.isString())
                        cfg.args.append(val.toString());
                }
                args_parsed = true;
            }
        }

        if (!args_parsed) {
            cfg.args = srv.args.split(' ', Qt::SkipEmptyParts);
        }

        // Parse env: Try JSON object first, fallback to legacy "KEY=VAL KEY2=VAL2" format
        bool env_parsed = false;
        if (srv.env.trimmed().startsWith('{')) {
            QJsonDocument doc = QJsonDocument::fromJson(srv.env.toUtf8());
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                for (auto it = obj.begin(); it != obj.end(); ++it) {
                    cfg.env[it.key()] = it.value().toString();
                }
                env_parsed = true;
            }
        }

        if (!env_parsed && !srv.env.isEmpty()) {
            for (const auto& pair : srv.env.split(' ', Qt::SkipEmptyParts)) {
                int eq = pair.indexOf('=');
                if (eq > 0)
                    cfg.env[pair.left(eq)] = pair.mid(eq + 1);
            }
        }

        configs_.insert(cfg.id, cfg);
    }

    LOG_INFO(TAG, QString("Loaded %1 MCP server configs").arg(configs_.size()));
    emit servers_changed();
}

std::vector<McpServerConfig> McpManager::get_servers() const {
    QMutexLocker lock(&mutex_);
    std::vector<McpServerConfig> result;
    result.reserve(static_cast<std::size_t>(configs_.size()));
    for (const auto& cfg : configs_)
        result.push_back(cfg);
    return result;
}

Result<void> McpManager::save_server(const McpServerConfig& config) {
    McpServer srv;
    srv.id = config.id.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : config.id;
    srv.name = config.name;
    srv.description = config.description;
    srv.command = config.command;
    srv.category = config.category;
    srv.enabled = config.enabled;
    srv.auto_start = config.auto_start;
    srv.status = "stopped";

    // Store args and env as JSON strings to correctly handle spaces and special chars
    QJsonArray argsArr;
    for (const auto& arg : config.args)
        argsArr.append(arg);
    srv.args = QJsonDocument(argsArr).toJson(QJsonDocument::Compact);

    QJsonObject envObj;
    for (auto it = config.env.constBegin(); it != config.env.constEnd(); ++it)
        envObj[it.key()] = it.value();
    srv.env = QJsonDocument(envObj).toJson(QJsonDocument::Compact);

    auto r = McpServerRepository::instance().save(srv);
    if (r.is_err())
        return r;

    {
        QMutexLocker lock(&mutex_);
        McpServerConfig cfg = config;
        cfg.id = srv.id;
        configs_.insert(cfg.id, cfg);
    }

    emit servers_changed();
    return Result<void>::ok();
}

Result<void> McpManager::remove_server(const QString& id) {
    stop_server(id);

    auto r = McpServerRepository::instance().remove(id);
    if (r.is_err())
        return r;

    {
        QMutexLocker lock(&mutex_);
        configs_.remove(id);
        tool_cache_.remove(id);
    }

    emit servers_changed();
    return Result<void>::ok();
}

// ============================================================================
// Server Lifecycle
// ============================================================================

Result<void> McpManager::start_server(const QString& id) {
    McpServerConfig cfg;
    {
        QMutexLocker lock(&mutex_);
        if (!configs_.contains(id))
            return Result<void>::err("Unknown server: " + id.toStdString());
        // Already running — nothing to do
        if (clients_.contains(id) && clients_[id]->is_running())
            return Result<void>::ok();
        // Guard against concurrent start attempts (auto-start thread + user click).
        // If status is Starting, another caller is already starting this server.
        if (configs_[id].status == ServerStatus::Starting)
            return Result<void>::ok();
        configs_[id].status = ServerStatus::Starting;
        cfg = configs_[id];
    }
    // mutex released — slow operations below don't block other threads

    auto client = std::make_shared<McpClient>(cfg);

    auto start_result = client->start();
    if (start_result.is_err()) {
        QMutexLocker lock(&mutex_);
        if (configs_.contains(id))
            configs_[id].status = ServerStatus::Error;
        McpServerRepository::instance().set_status(id, "error");
        emit servers_changed();
        return start_result;
    }

    auto init_result = client->initialize();
    if (init_result.is_err()) {
        client->stop();
        QMutexLocker lock(&mutex_);
        if (configs_.contains(id))
            configs_[id].status = ServerStatus::Error;
        McpServerRepository::instance().set_status(id, "error");
        emit servers_changed();
        return Result<void>::err("Handshake failed for " + cfg.name.toStdString() + ": " + init_result.error());
    }

    // Re-acquire mutex only to update state
    {
        QMutexLocker lock(&mutex_);
        McpServerRepository::instance().set_status(id, "running");
        configs_[id].status = ServerStatus::Running;
        clients_.insert(id, std::move(client));
    }

    refresh_tools_for(id);

    emit servers_changed();
    LOG_INFO(TAG, "MCP server started: " + cfg.name);
    return Result<void>::ok();
}

Result<void> McpManager::stop_server(const QString& id) {
    QMutexLocker lock(&mutex_);

    if (!clients_.contains(id))
        return Result<void>::ok();

    clients_[id]->stop();
    clients_.remove(id);
    tool_cache_.remove(id);

    if (configs_.contains(id))
        configs_[id].status = ServerStatus::Stopped;

    McpServerRepository::instance().set_status(id, "stopped");
    lock.unlock();

    emit servers_changed();
    LOG_INFO(TAG, "MCP server stopped: " + id);
    return Result<void>::ok();
}

Result<void> McpManager::restart_server(const QString& id) {
    stop_server(id);
    return start_server(id);
}

void McpManager::start_auto_servers() {
    QMutexLocker lock(&mutex_);
    QStringList to_start;
    for (const auto& cfg : configs_) {
        if (cfg.auto_start && cfg.enabled)
            to_start.append(cfg.id);
    }
    lock.unlock();

    for (const auto& id : to_start) {
        auto r = start_server(id);
        if (r.is_err())
            LOG_WARN(TAG, "Auto-start failed for " + id + ": " + QString::fromStdString(r.error()));
    }
}

void McpManager::stop_all() {
    QMutexLocker lock(&mutex_);
    QStringList ids = clients_.keys();
    lock.unlock();

    for (const auto& id : ids)
        stop_server(id);
}

void McpManager::shutdown() {
    stop_health_check();
    stop_all();
}

// ============================================================================
// Health Check
// ============================================================================

void McpManager::start_health_check(int interval_seconds) {
    health_timer_->setInterval(interval_seconds * 1000);
    health_timer_->start();
}

void McpManager::stop_health_check() {
    if (health_timer_->isActive())
        health_timer_->stop();
}

void McpManager::do_health_check() {
    QMutexLocker lock(&mutex_);
    QStringList running_ids = clients_.keys();
    lock.unlock();

    for (const auto& id : running_ids) {
        QMutexLocker l(&mutex_);
        if (!clients_.contains(id))
            continue;
        McpClient* client = clients_[id].get();
        l.unlock();

        auto r = client->ping();
        if (r.is_err()) {
            LOG_WARN(TAG, "Health check failed for " + id + ": " + QString::fromStdString(r.error()));

            int attempts = restart_attempts_.value(id, 0);
            if (attempts < MAX_RESTART_ATTEMPTS) {
                restart_attempts_[id] = attempts + 1;
                LOG_INFO(TAG, QString("Restarting server %1 (attempt %2/%3)")
                                  .arg(id)
                                  .arg(attempts + 1)
                                  .arg(MAX_RESTART_ATTEMPTS));
                restart_server(id);
            } else {
                LOG_ERROR(TAG, "Server " + id + " exceeded max restart attempts — giving up");
                McpServerRepository::instance().set_status(id, "error");
            }
        } else {
            restart_attempts_.remove(id); // reset on success
        }
    }
}

// ============================================================================
// Tool Aggregation
// ============================================================================

std::vector<ExternalTool> McpManager::get_all_external_tools() {
    QMutexLocker lock(&mutex_);
    std::vector<ExternalTool> all;
    for (const auto& tools : tool_cache_)
        all.insert(all.end(), tools.begin(), tools.end());
    return all;
}

Result<QJsonObject> McpManager::call_external_tool(const QString& server_id, const QString& tool_name,
                                                   const QJsonObject& args) {
    QMutexLocker lock(&mutex_);
    McpClient* client = get_client(server_id);
    if (!client)
        return Result<QJsonObject>::err("Server not running: " + server_id.toStdString());
    lock.unlock();

    return client->call_tool(tool_name, args);
}

McpClient* McpManager::get_client(const QString& id) const {
    auto it = clients_.find(id);
    if (it == clients_.end())
        return nullptr;
    return it->get();
}

QStringList McpManager::get_logs(const QString& id) const {
    QMutexLocker lock(&mutex_);
    auto it = clients_.find(id);
    if (it == clients_.end())
        return {};
    return it->get()->get_logs();
}

void McpManager::refresh_tools_for(const QString& id) {
    QMutexLocker lock(&mutex_);
    McpClient* client = get_client(id);
    if (!client)
        return;
    lock.unlock();

    auto result = client->list_tools();
    if (result.is_err()) {
        LOG_WARN(TAG, "Failed to list tools for " + id + ": " + QString::fromStdString(result.error()));
        return;
    }

    QMutexLocker l2(&mutex_);
    tool_cache_[id] = result.value();
    LOG_INFO(TAG, QString("Cached %1 tools from server %2").arg(result.value().size()).arg(id));
}

} // namespace fincept::mcp
