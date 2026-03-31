// src/services/agents/AgentService.cpp
#include "services/agents/AgentService.h"

#include "ai_chat/LlmService.h"
#include "auth/AuthManager.h"
#include "core/logging/Logger.h"
#include "python/PythonRunner.h"
#include "storage/repositories/LlmConfigRepository.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <QProcess>
#include <QUuid>

namespace fincept::services {

// ── Singleton ────────────────────────────────────────────────────────────────

AgentService& AgentService::instance() {
    static AgentService inst;
    return inst;
}

AgentService::AgentService(QObject* parent) : QObject(parent) {}

// ── Cache helpers ────────────────────────────────────────────────────────────

bool AgentService::is_cache_fresh(qint64 fetched_at, int ttl_ms) const {
    return fetched_at > 0 && (QDateTime::currentMSecsSinceEpoch() - fetched_at) < ttl_ms;
}

void AgentService::clear_cache() {
    QMutexLocker lock(&cache_mutex_);
    agents_cache_ = {};
    tools_cache_ = {};
    models_cache_ = {};
    sysinfo_cache_ = {};
    LOG_INFO("AgentService", "Cache cleared");
}

QVector<AgentInfo> AgentService::cached_agents() const {
    QMutexLocker lock(&cache_mutex_);
    return agents_cache_.data;
}

int AgentService::cached_agent_count() const {
    QMutexLocker lock(&cache_mutex_);
    return agents_cache_.data.size();
}

// ── API key builder ──────────────────────────────────────────────────────────

QJsonObject AgentService::build_api_keys() const {
    // Python's _get_api_key() looks up keys by env-var name (e.g. "ANTHROPIC_API_KEY"),
    // not by lowercase provider name. Send both forms so the lookup always succeeds.
    static const QMap<QString, QString> kEnvVarNames = {
        {"anthropic",         "ANTHROPIC_API_KEY"},
        {"openai",            "OPENAI_API_KEY"},
        {"google",            "GOOGLE_API_KEY"},
        {"gemini",            "GOOGLE_API_KEY"},
        {"groq",              "GROQ_API_KEY"},
        {"deepseek",          "DEEPSEEK_API_KEY"},
        {"financial_datasets","FINANCIAL_DATASETS_API_KEY"},
        {"tavily",            "TAVILY_API_KEY"},
        {"mistral",           "MISTRAL_API_KEY"},
        {"cohere",            "COHERE_API_KEY"},
        {"xai",               "XAI_API_KEY"},
    };

    QJsonObject keys;
    auto providers = LlmConfigRepository::instance().list_providers();
    if (providers.is_ok()) {
        for (const auto& p : providers.value()) {
            if (p.api_key.isEmpty())
                continue;
            const QString lower = p.provider.toLower();
            keys[lower] = p.api_key;                          // lowercase form
            const QString env_name = kEnvVarNames.value(lower);
            if (!env_name.isEmpty())
                keys[env_name] = p.api_key;                   // env-var form Python expects
        }
    }

    // Always include Fincept session API key (from login) so agents can use
    // the fincept provider even if user hasn't manually configured it in Settings.
    if (!keys.contains("fincept")) {
        const auto& session = auth::AuthManager::instance().session();
        if (!session.api_key.isEmpty()) {
            keys["fincept"]          = session.api_key;
            keys["FINCEPT_API_KEY"]  = session.api_key;
        }
    }

    return keys;
}

// ── Payload builder ──────────────────────────────────────────────────────────

QJsonObject AgentService::build_payload(const QString& action, const QJsonObject& params,
                                        const QJsonObject& config) const {
    QJsonObject payload;
    payload["action"] = action;
    payload["api_keys"] = build_api_keys();

    // Inject the resolved LLM config as active_llm so Python can build the exact
    // model instance without re-resolving credentials.
    // Priority: config["model"] (per-agent resolved profile) > global active LLM.
    // This means: if the caller already embedded a resolved profile in config["model"]
    // (as build_config_from_editor() now does), Python gets the right per-agent creds.
    {
        if (config.contains("model") && !config["model"].toObject()["provider"].toString().isEmpty()) {
            // Use the per-agent resolved profile already embedded in the config
            payload["active_llm"] = config["model"].toObject();
        } else {
            auto& llm = ai_chat::LlmService::instance();
            if (llm.is_configured()) {
                QJsonObject active_llm;
                active_llm["provider"]    = llm.active_provider();
                active_llm["model_id"]    = llm.active_model();
                active_llm["api_key"]     = llm.active_api_key();
                active_llm["base_url"]    = llm.active_base_url();
                active_llm["temperature"] = llm.active_temperature();
                active_llm["max_tokens"]  = llm.active_max_tokens();
                payload["active_llm"] = active_llm;
            }
        }
    }

    if (!params.isEmpty())
        payload["params"] = params;
    if (!config.isEmpty())
        payload["config"] = config;
    return payload;
}

// ── Python lightweight runner (via PythonRunner args) ─────────────────────────

void AgentService::run_python_light(const QString& action, const QJsonObject& params,
                                    std::function<void(bool, QJsonObject)> on_result) {
    QJsonObject payload = build_payload(action, params);
    QString payload_str = QString::fromUtf8(QJsonDocument(payload).toJson(QJsonDocument::Compact));

    QPointer<AgentService> self = this;
    python::PythonRunner::instance().run(
        "agents/finagent_core/main.py", {payload_str}, [self, action, on_result](python::PythonResult pr) {
            if (!self)
                return;
            if (!pr.success) {
                LOG_ERROR("AgentService", QString("%1 failed: %2").arg(action, pr.error.left(200)));
                on_result(false, QJsonObject{{"error", pr.error}});
                return;
            }
            QJsonDocument doc = QJsonDocument::fromJson(pr.output.toUtf8());
            if (doc.isNull()) {
                LOG_ERROR("AgentService", QString("%1: invalid JSON response").arg(action));
                on_result(false, QJsonObject{{"error", "Invalid JSON response from Python"}});
                return;
            }
            on_result(true, doc.object());
        });
}

// ── Python stdin runner (for large payloads) ─────────────────────────────────

void AgentService::run_python_stdin(const QString& action, const QJsonObject& params, const QJsonObject& config,
                                    std::function<void(bool, QJsonObject)> on_result) {
    auto& py = python::PythonRunner::instance();
    if (!py.is_available()) {
        on_result(false, QJsonObject{{"error", "Python not available"}});
        return;
    }

    QJsonObject payload = build_payload(action, params, config);
    QByteArray payload_bytes = QJsonDocument(payload).toJson(QJsonDocument::Compact);

    QString python_path = py.python_path();
    QString script_path = py.scripts_dir() + "/agents/finagent_core/main.py";

    // Spawn QProcess directly for stdin writing (P4 exception like ExchangeService)
    auto* proc = new QProcess(this);
    QPointer<AgentService> self = this;
    QElapsedTimer* timer = new QElapsedTimer;
    timer->start();

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [self, proc, action, on_result, timer](int exit_code, QProcess::ExitStatus) {
                int elapsed = timer->elapsed();
                delete timer;

                QString stdout_str = QString::fromUtf8(proc->readAllStandardOutput());
                QString stderr_str = QString::fromUtf8(proc->readAllStandardError());
                proc->deleteLater();

                if (!self)
                    return;

                // Extract JSON from output
                QString json_str = python::extract_json(stdout_str);
                if (json_str.isEmpty() && exit_code == 0) {
                    json_str = stdout_str.trimmed();
                }

                QJsonDocument doc = QJsonDocument::fromJson(json_str.toUtf8());
                if (exit_code != 0 || doc.isNull()) {
                    LOG_ERROR("AgentService", QString("%1 failed (exit=%2, %3ms): %4")
                                                  .arg(action)
                                                  .arg(exit_code)
                                                  .arg(elapsed)
                                                  .arg(stderr_str.left(200)));
                    QString err = stderr_str.isEmpty() ? "Agent execution failed" : stderr_str.left(500);
                    on_result(false, QJsonObject{{"error", err}});
                    return;
                }

                LOG_INFO("AgentService", QString("%1 completed in %2ms").arg(action).arg(elapsed));
                QJsonObject result = doc.object();
                result["execution_time_ms"] = elapsed;
                on_result(true, result);
            });

    connect(proc, &QProcess::errorOccurred, this, [self, proc, action, on_result, timer](QProcess::ProcessError) {
        delete timer;
        QString err = proc->errorString();
        proc->deleteLater();
        if (!self)
            return;
        LOG_ERROR("AgentService", QString("%1 process error: %2").arg(action, err));
        on_result(false, QJsonObject{{"error", "Process error: " + err}});
    });

    LOG_INFO("AgentService", QString("Running %1 via stdin (%2 bytes)").arg(action).arg(payload_bytes.size()));
    proc->start(python_path, {script_path, "--stdin"});
    proc->write(payload_bytes);
    proc->closeWriteChannel();
}

// ── Agent discovery ──────────────────────────────────────────────────────────

void AgentService::discover_agents() {
    {
        QMutexLocker lock(&cache_mutex_);
        if (is_cache_fresh(agents_cache_.fetched_at, kAgentCacheTtlMs)) {
            LOG_DEBUG("AgentService", "agents: serving from cache");
            emit agents_discovered(agents_cache_.data, agents_cache_.categories);
            return;
        }
    }

    LOG_INFO("AgentService", "Discovering agents from Python");
    QPointer<AgentService> self = this;

    run_python_light("discover_agents", {}, [self](bool ok, QJsonObject result) {
        if (!self)
            return;
        if (!ok) {
            emit self->error_occurred("discover_agents", result["error"].toString());
            return;
        }

        QVector<AgentInfo> agents;
        QMap<QString, int> cat_counts;

        QJsonArray arr = result["agents"].toArray();
        if (arr.isEmpty()) {
            // Try alternative response format
            arr = result["data"].toArray();
        }

        for (const auto& v : arr) {
            QJsonObject o = v.toObject();
            AgentInfo info;
            info.id = o["id"].toString();
            info.name = o["name"].toString();
            info.description = o["description"].toString();
            info.category = o["category"].toString("general");
            info.provider = o["provider"].toString();
            info.version = o["version"].toString("1.0.0");
            info.config = o["config"].toObject();

            QJsonArray caps = o["capabilities"].toArray();
            for (const auto& c : caps)
                info.capabilities.append(c.toString());

            agents.append(info);
            cat_counts[info.category]++;
        }

        // Also merge DB-saved custom agents
        auto db_agents = AgentConfigRepository::instance().list_all();
        if (db_agents.is_ok()) {
            for (const auto& dba : db_agents.value()) {
                // Skip if already discovered by Python
                bool found = false;
                for (const auto& a : agents) {
                    if (a.id == dba.id) {
                        found = true;
                        break;
                    }
                }
                if (found)
                    continue;

                AgentInfo info;
                info.id = dba.id;
                info.name = dba.name;
                info.description = dba.description;
                info.category = dba.category;
                info.provider = "custom";
                info.version = "1.0.0";

                QJsonDocument doc = QJsonDocument::fromJson(dba.config_json.toUtf8());
                if (!doc.isNull())
                    info.config = doc.object();

                agents.append(info);
                cat_counts[info.category]++;
            }
        }

        QVector<AgentCategory> categories;
        for (auto it = cat_counts.begin(); it != cat_counts.end(); ++it) {
            categories.append({it.key(), it.value()});
        }

        {
            QMutexLocker lock(&self->cache_mutex_);
            self->agents_cache_.data = agents;
            self->agents_cache_.categories = categories;
            self->agents_cache_.fetched_at = QDateTime::currentMSecsSinceEpoch();
        }

        LOG_INFO("AgentService",
                 QString("Discovered %1 agents in %2 categories").arg(agents.size()).arg(categories.size()));
        emit self->agents_discovered(agents, categories);
    });
}

// ── Agent execution ──────────────────────────────────────────────────────────

QString AgentService::run_agent(const QString& query, const QJsonObject& config) {
    const QString req_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    LOG_INFO("AgentService", QString("Running agent query [%1]: %2").arg(req_id.left(8), query.left(80)));

    QJsonObject params;
    params["query"] = query;

    QPointer<AgentService> self = this;
    run_python_stdin("run", params, config, [self, req_id](bool ok, QJsonObject result) {
        if (!self)
            return;
        AgentExecutionResult r;
        r.request_id = req_id;
        r.success = ok && result["success"].toBool(ok);
        r.execution_time_ms = result["execution_time_ms"].toInt();

        if (r.success) {
            if (result.contains("response"))
                r.response = result["response"].toString();
            else if (result.contains("result"))
                r.response = result["result"].toString();
            else if (result.contains("data"))
                r.response = QJsonDocument(result["data"].toObject()).toJson(QJsonDocument::Indented);
            else
                r.response = QJsonDocument(result).toJson(QJsonDocument::Indented);
        } else {
            r.error = result["error"].toString("Agent execution failed");
        }

        emit self->agent_result(r);
    });
    return req_id;
}

// ── Streaming agent execution ─────────────────────────────────────────────────

QString AgentService::run_agent_streaming(const QString& query, const QJsonObject& config) {
    const QString req_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    LOG_INFO("AgentService", QString("Streaming agent query [%1]: %2").arg(req_id.left(8), query.left(80)));

    auto& py = python::PythonRunner::instance();
    if (!py.is_available()) {
        AgentExecutionResult r;
        r.request_id = req_id;
        r.success = false;
        r.error = "Python not available";
        emit agent_stream_done(r);
        return req_id;
    }

    QJsonObject params;
    params["query"] = query;

    QJsonObject payload = build_payload("run", params, config);
    QByteArray payload_bytes = QJsonDocument(payload).toJson(QJsonDocument::Compact);

    QString python_path = py.python_path();
    QString script_path = py.scripts_dir() + "/agents/finagent_core/main.py";

    auto* proc = new QProcess(this);
    QPointer<AgentService> self = this;
    QElapsedTimer* timer = new QElapsedTimer;
    timer->start();

    // Accumulate full response from TOKEN chunks
    QString* accumulated = new QString;

    // Read stdout line by line as process writes
    connect(proc, &QProcess::readyReadStandardOutput, this, [self, proc, accumulated, req_id]() {
        if (!self) return;
        while (proc->canReadLine()) {
            QString line = QString::fromUtf8(proc->readLine()).trimmed();
            if (line.isEmpty()) continue;

            if (line.startsWith("TOKEN: ")) {
                QString token = line.mid(7);
                *accumulated += token + " ";
                emit self->agent_stream_token(req_id, token);
            } else if (line.startsWith("THINKING: ")) {
                emit self->agent_stream_thinking(req_id, line.mid(10));
            } else if (line.startsWith("TOOL: ")) {
                emit self->agent_stream_thinking(req_id, "Tool: " + line.mid(6));
            } else if (line.startsWith("ERROR: ")) {
                AgentExecutionResult r;
                r.request_id = req_id;
                r.success = false;
                r.error = line.mid(7);
                emit self->agent_stream_done(r);
            }
            // DONE line and final JSON are handled in finished signal
        }
    });

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [self, proc, accumulated, timer, req_id](int exit_code, QProcess::ExitStatus) {
                int elapsed = timer->elapsed();
                delete timer;

                // Drain any remaining stdout (final JSON result line)
                QString remaining = QString::fromUtf8(proc->readAllStandardOutput());
                QString stderr_str = QString::fromUtf8(proc->readAllStandardError());
                proc->deleteLater();

                if (!self) { delete accumulated; return; }

                // Find the final JSON line (last non-empty line)
                QString final_json;
                for (const QString& line : remaining.split('\n')) {
                    QString t = line.trimmed();
                    if (t.startsWith('{')) final_json = t;
                }

                AgentExecutionResult r;
                r.request_id = req_id;
                r.execution_time_ms = elapsed;

                QJsonDocument doc = QJsonDocument::fromJson(final_json.toUtf8());
                if (!doc.isNull() && doc.object()["success"].toBool()) {
                    QString json_response = doc.object()["response"].toString();
                    r.success = true;
                    r.response = json_response.isEmpty() ? accumulated->trimmed() : json_response;
                } else if (!accumulated->trimmed().isEmpty()) {
                    r.success = true;
                    r.response = accumulated->trimmed();
                } else {
                    r.success = false;
                    r.error = exit_code != 0 ? stderr_str.left(500) : "No response received";
                }

                delete accumulated;
                LOG_INFO("AgentService", QString("Streaming completed in %1ms").arg(elapsed));
                emit self->agent_stream_done(r);
            });

    connect(proc, &QProcess::errorOccurred, this, [self, proc, accumulated, timer, req_id](QProcess::ProcessError) {
        delete timer;
        QString err = proc->errorString();
        proc->deleteLater();
        delete accumulated;
        if (!self) return;
        AgentExecutionResult r;
        r.request_id = req_id;
        r.success = false;
        r.error = "Process error: " + err;
        emit self->agent_stream_done(r);
    });

    LOG_INFO("AgentService", QString("Starting streaming agent (%1 bytes payload)").arg(payload_bytes.size()));
    proc->start(python_path, {script_path, "--stdin", "--stream"});
    proc->write(payload_bytes);
    proc->closeWriteChannel();
    return req_id;
}

// ── Query routing ────────────────────────────────────────────────────────────

QString AgentService::route_query(const QString& query) {
    LOG_INFO("AgentService", QString("Routing query: %1").arg(query.left(80)));

    QJsonObject params;
    params["query"] = query;

    const QString req_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QPointer<AgentService> self = this;
    run_python_stdin("route_query", params, {}, [self, req_id](bool ok, QJsonObject result) {
        if (!self)
            return;
        RoutingResult r;
        r.request_id = req_id;
        r.success = ok && result["success"].toBool(ok);
        r.agent_id = result["agent_id"].toString();
        r.intent = result["intent"].toString();
        r.confidence = result["confidence"].toDouble();
        r.config = result["config"].toObject();

        QJsonArray kw = result["matched_keywords"].toArray();
        for (const auto& k : kw)
            r.matched_keywords.append(k.toString());

        emit self->routing_result(r);
    });
    return req_id;
}

// ── Team execution ───────────────────────────────────────────────────────────

QString AgentService::run_team(const QString& query, const QJsonObject& team_config) {
    LOG_INFO("AgentService", QString("Running team query: %1").arg(query.left(80)));

    QJsonObject params;
    params["query"] = query;
    params["team_config"] = team_config;

    // Pass coordinator model as config so build_payload promotes it to active_llm.
    // This ensures Python receives the resolved coordinator profile credentials.
    QJsonObject coord_as_config;
    if (team_config.contains("model"))
        coord_as_config["model"] = team_config["model"];

    const QString req_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QPointer<AgentService> self = this;
    run_python_stdin("run_team", params, coord_as_config, [self, req_id](bool ok, QJsonObject result) {
        if (!self)
            return;
        AgentExecutionResult r;
        r.request_id = req_id;
        r.success = ok && result["success"].toBool(ok);
        r.execution_time_ms = result["execution_time_ms"].toInt();
        r.response = result.contains("response") ? result["response"].toString()
                                                 : QJsonDocument(result).toJson(QJsonDocument::Indented);
        r.error = result["error"].toString();
        emit self->agent_result(r);
    });
    return req_id;
}

// ── Workflow execution ───────────────────────────────────────────────────────

QString AgentService::run_workflow(const QString& workflow_type, const QJsonObject& params) {
    const QString req_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    LOG_INFO("AgentService", QString("Running workflow [%1]: %2").arg(req_id.left(8), workflow_type));

    QJsonObject p = params;
    p["workflow_type"] = workflow_type;

    QPointer<AgentService> self = this;
    run_python_stdin(workflow_type, p, {}, [self, req_id](bool ok, QJsonObject result) {
        if (!self)
            return;
        AgentExecutionResult r;
        r.request_id = req_id;
        r.success = ok && result["success"].toBool(ok);
        r.execution_time_ms = result["execution_time_ms"].toInt();
        r.response = result.contains("response") ? result["response"].toString()
                                                 : QJsonDocument(result).toJson(QJsonDocument::Indented);
        r.error = result["error"].toString();
        emit self->agent_result(r);
    });
    return req_id;
}

// ── Planner ──────────────────────────────────────────────────────────────────

QString AgentService::create_plan(const QString& query, const QJsonObject& config) {
    LOG_INFO("AgentService", QString("Creating plan for: %1").arg(query.left(80)));

    const QString req_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QJsonObject params;
    params["query"] = query;
    if (!config.isEmpty())
        for (auto it = config.begin(); it != config.end(); ++it)
            params[it.key()] = it.value();

    QPointer<AgentService> self = this;
    run_python_stdin("generate_dynamic_plan", params, {}, [self, req_id](bool ok, QJsonObject result) {
        if (!self)
            return;
        if (!ok) {
            emit self->error_occurred("create_plan", result["error"].toString());
            return;
        }

        ExecutionPlan plan;
        plan.request_id = req_id;
        plan.id = result["id"].toString();
        plan.name = result["name"].toString();
        plan.description = result["description"].toString();
        plan.status = result["status"].toString("pending");
        plan.is_complete = result["is_complete"].toBool();

        QJsonArray steps = result["steps"].toArray();
        for (const auto& sv : steps) {
            QJsonObject so = sv.toObject();
            PlanStep step;
            step.id = so["id"].toString();
            step.name = so["name"].toString();
            step.step_type = so["step_type"].toString();
            step.config = so["config"].toObject();
            step.status = so["status"].toString("pending");
            QJsonArray deps = so["dependencies"].toArray();
            for (const auto& d : deps)
                step.dependencies.append(d.toString());
            plan.steps.append(step);
        }

        emit self->plan_created(plan);
    });
    return req_id;
}

QString AgentService::execute_plan(const QJsonObject& plan, const QJsonObject& config) {
    LOG_INFO("AgentService", "Executing plan");

    const QString req_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QJsonObject params;
    params["plan"] = plan;
    if (!config.isEmpty())
        for (auto it = config.begin(); it != config.end(); ++it)
            params[it.key()] = it.value();

    QPointer<AgentService> self = this;
    run_python_stdin("execute_plan", params, {}, [self, req_id](bool ok, QJsonObject result) {
        if (!self)
            return;
        if (!ok) {
            emit self->error_occurred("execute_plan", result["error"].toString());
            return;
        }

        ExecutionPlan plan;
        plan.request_id = req_id;
        plan.id = result["id"].toString();
        plan.status = result["status"].toString();
        plan.is_complete = result["is_complete"].toBool();
        plan.has_failed = result["has_failed"].toBool();

        QJsonArray steps = result["steps"].toArray();
        for (const auto& sv : steps) {
            QJsonObject so = sv.toObject();
            PlanStep step;
            step.id = so["id"].toString();
            step.name = so["name"].toString();
            step.status = so["status"].toString();
            step.result = so["result"].toString();
            step.error = so["error"].toString();
            plan.steps.append(step);
        }

        emit self->plan_executed(plan);
    });
    return req_id;
}

// ── System info ──────────────────────────────────────────────────────────────

void AgentService::get_system_info() {
    {
        QMutexLocker lock(&cache_mutex_);
        if (is_cache_fresh(sysinfo_cache_.fetched_at, kSysInfoCacheTtlMs)) {
            emit system_info_loaded(sysinfo_cache_.data);
            return;
        }
    }

    QPointer<AgentService> self = this;
    run_python_light("system_info", {}, [self](bool ok, QJsonObject result) {
        if (!self)
            return;
        if (!ok) {
            emit self->error_occurred("system_info", result["error"].toString());
            return;
        }

        AgentSystemInfo info;
        info.version = result["version"].toString();
        info.framework = result["framework"].toString();
        info.capabilities = result["capabilities"].toObject();

        QJsonArray feats = result["features"].toArray();
        for (const auto& f : feats)
            info.features.append(f.toString());

        {
            QMutexLocker lock(&self->cache_mutex_);
            self->sysinfo_cache_.data = info;
            self->sysinfo_cache_.fetched_at = QDateTime::currentMSecsSinceEpoch();
        }

        emit self->system_info_loaded(info);
    });
}

void AgentService::list_tools() {
    {
        QMutexLocker lock(&cache_mutex_);
        if (is_cache_fresh(tools_cache_.fetched_at, kToolsCacheTtlMs)) {
            emit tools_loaded(tools_cache_.data);
            return;
        }
    }

    QPointer<AgentService> self = this;
    run_python_light("list_tools", {}, [self](bool ok, QJsonObject result) {
        if (!self)
            return;
        if (!ok) {
            emit self->error_occurred("list_tools", result["error"].toString());
            return;
        }

        AgentToolsInfo info;
        info.tools = result["tools"].toObject();
        QJsonArray cats = result["categories"].toArray();
        for (const auto& c : cats)
            info.categories.append(c.toString());
        info.total_count = result["total_count"].toInt();

        {
            QMutexLocker lock(&self->cache_mutex_);
            self->tools_cache_.data = info;
            self->tools_cache_.fetched_at = QDateTime::currentMSecsSinceEpoch();
        }

        emit self->tools_loaded(info);
    });
}

void AgentService::list_models() {
    {
        QMutexLocker lock(&cache_mutex_);
        if (is_cache_fresh(models_cache_.fetched_at, kModelsCacheTtlMs)) {
            emit models_loaded(models_cache_.data);
            return;
        }
    }

    QPointer<AgentService> self = this;
    run_python_light("list_models", {}, [self](bool ok, QJsonObject result) {
        if (!self)
            return;
        if (!ok) {
            emit self->error_occurred("list_models", result["error"].toString());
            return;
        }

        AgentModelsInfo info;
        QJsonArray provs = result["providers"].toArray();
        for (const auto& p : provs)
            info.providers.append(p.toString());
        info.count = result["count"].toInt();

        {
            QMutexLocker lock(&self->cache_mutex_);
            self->models_cache_.data = info;
            self->models_cache_.fetched_at = QDateTime::currentMSecsSinceEpoch();
        }

        emit self->models_loaded(info);
    });
}

// ── Config CRUD ──────────────────────────────────────────────────────────────

void AgentService::load_configs(const QString& category) {
    auto& repo = AgentConfigRepository::instance();
    auto result = category.isEmpty() ? repo.list_all() : repo.list_by_category(category);

    if (result.is_ok()) {
        emit configs_loaded(result.value());
    } else {
        emit error_occurred("load_configs", QString::fromStdString(result.error()));
    }
}

void AgentService::save_config(const AgentConfig& config) {
    auto result = AgentConfigRepository::instance().save(config);
    if (result.is_ok()) {
        LOG_INFO("AgentService", QString("Saved agent config: %1").arg(config.name));
        emit config_saved();
    } else {
        emit error_occurred("save_config", QString::fromStdString(result.error()));
    }
}

void AgentService::delete_config(const QString& id) {
    auto result = AgentConfigRepository::instance().remove(id);
    if (result.is_ok()) {
        LOG_INFO("AgentService", QString("Deleted agent config: %1").arg(id));
        emit config_deleted();
    } else {
        emit error_occurred("delete_config", QString::fromStdString(result.error()));
    }
}

void AgentService::set_active_config(const QString& id) {
    auto result = AgentConfigRepository::instance().set_active(id);
    if (result.is_ok()) {
        LOG_INFO("AgentService", QString("Set active agent: %1").arg(id));
    } else {
        emit error_occurred("set_active_config", QString::fromStdString(result.error()));
    }
}

// ── Structured output ────────────────────────────────────────────────────────

QString AgentService::run_agent_structured(const QString& query, const QJsonObject& config, const QString& output_model) {
    const QString req_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    LOG_INFO("AgentService", QString("Running structured agent [%1] (%2): %3").arg(req_id.left(8), output_model, query.left(60)));
    QJsonObject params;
    params["query"] = query;
    params["output_model"] = output_model;

    QPointer<AgentService> self = this;
    run_python_stdin("run_structured", params, config, [self, req_id](bool ok, QJsonObject result) {
        if (!self)
            return;
        AgentExecutionResult r;
        r.request_id = req_id;
        r.success = ok && result["success"].toBool(ok);
        r.execution_time_ms = result["execution_time_ms"].toInt();
        r.response = result.contains("response") ? result["response"].toString()
                                                 : QJsonDocument(result).toJson(QJsonDocument::Indented);
        r.error = result["error"].toString();
        emit self->agent_result(r);
    });
    return req_id;
}

// ── Routed query execution ───────────────────────────────────────────────────

void AgentService::execute_routed_query(const QString& query, const QJsonObject& config, const QString& session_id) {
    LOG_INFO("AgentService", QString("Execute routed query: %1").arg(query.left(60)));
    QJsonObject params;
    params["query"] = query;
    if (!session_id.isEmpty())
        params["session_id"] = session_id;

    QPointer<AgentService> self = this;
    run_python_stdin("execute_query", params, config, [self](bool ok, QJsonObject result) {
        if (!self)
            return;
        AgentExecutionResult r;
        r.success = ok && result["success"].toBool(ok);
        r.execution_time_ms = result["execution_time_ms"].toInt();
        r.response = result.contains("response") ? result["response"].toString()
                                                 : QJsonDocument(result).toJson(QJsonDocument::Indented);
        r.error = result["error"].toString();
        emit self->agent_result(r);
    });
}

// ── Multi-query ──────────────────────────────────────────────────────────────

void AgentService::execute_multi_query(const QString& query, bool aggregate) {
    LOG_INFO("AgentService", QString("Multi-query: %1").arg(query.left(60)));
    QJsonObject params;
    params["query"] = query;
    params["aggregate"] = aggregate;

    QPointer<AgentService> self = this;
    run_python_stdin("execute_multi_query", params, {}, [self](bool ok, QJsonObject result) {
        if (!self)
            return;
        if (ok)
            emit self->multi_query_result(result);
        else
            emit self->error_occurred("multi_query", result["error"].toString());
    });
}

// ── Financial workflows (specialized) ────────────────────────────────────────

void AgentService::run_stock_analysis(const QString& symbol, const QJsonObject& config) {
    LOG_INFO("AgentService", QString("Stock analysis: %1").arg(symbol));
    QJsonObject params;
    params["symbol"] = symbol;

    QPointer<AgentService> self = this;
    run_python_stdin("stock_analysis", params, config, [self](bool ok, QJsonObject result) {
        if (!self)
            return;
        AgentExecutionResult r;
        r.success = ok && result["success"].toBool(ok);
        r.execution_time_ms = result["execution_time_ms"].toInt();
        r.response = result.contains("response") ? result["response"].toString()
                                                 : QJsonDocument(result).toJson(QJsonDocument::Indented);
        r.error = result["error"].toString();
        emit self->agent_result(r);
    });
}

void AgentService::run_portfolio_rebalancing(const QJsonObject& portfolio_data) {
    LOG_INFO("AgentService", "Portfolio rebalancing");
    QJsonObject params;
    if (!portfolio_data.isEmpty())
        params["portfolio_data"] = portfolio_data;

    QPointer<AgentService> self = this;
    run_python_stdin("portfolio_rebal", params, {}, [self](bool ok, QJsonObject result) {
        if (!self)
            return;
        AgentExecutionResult r;
        r.success = ok && result["success"].toBool(ok);
        r.execution_time_ms = result["execution_time_ms"].toInt();
        r.response = result.contains("response") ? result["response"].toString()
                                                 : QJsonDocument(result).toJson(QJsonDocument::Indented);
        r.error = result["error"].toString();
        emit self->agent_result(r);
    });
}

void AgentService::run_risk_assessment(const QJsonObject& portfolio_data) {
    LOG_INFO("AgentService", "Risk assessment");
    QJsonObject params;
    if (!portfolio_data.isEmpty())
        params["portfolio_data"] = portfolio_data;

    QPointer<AgentService> self = this;
    run_python_stdin("risk_assessment", params, {}, [self](bool ok, QJsonObject result) {
        if (!self)
            return;
        AgentExecutionResult r;
        r.success = ok && result["success"].toBool(ok);
        r.execution_time_ms = result["execution_time_ms"].toInt();
        r.response = result.contains("response") ? result["response"].toString()
                                                 : QJsonDocument(result).toJson(QJsonDocument::Indented);
        r.error = result["error"].toString();
        emit self->agent_result(r);
    });
}

void AgentService::run_portfolio_analysis(const QString& analysis_type, const QJsonObject& portfolio_summary) {
    LOG_INFO("AgentService", QString("Portfolio analysis: %1").arg(analysis_type));
    QJsonObject params;
    params["analysis_type"] = analysis_type;
    if (!portfolio_summary.isEmpty())
        params["portfolio_summary"] = portfolio_summary;

    QPointer<AgentService> self = this;
    run_python_stdin("run", params, {}, [self](bool ok, QJsonObject result) {
        if (!self)
            return;
        AgentExecutionResult r;
        r.success = ok && result["success"].toBool(ok);
        r.execution_time_ms = result["execution_time_ms"].toInt();
        r.response = result.contains("response") ? result["response"].toString()
                                                 : QJsonDocument(result).toJson(QJsonDocument::Indented);
        r.error = result["error"].toString();
        emit self->agent_result(r);
    });
}

// ── Plan variants ────────────────────────────────────────────────────────────

QString AgentService::create_stock_analysis_plan(const QString& symbol, const QJsonObject& config) {
    LOG_INFO("AgentService", QString("Stock analysis plan: %1").arg(symbol));
    const QString req_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QJsonObject params;
    params["symbol"] = symbol;

    QPointer<AgentService> self = this;
    run_python_stdin("create_stock_plan", params, {}, [self, req_id](bool ok, QJsonObject result) {
        if (!self)
            return;
        if (!ok) {
            emit self->error_occurred("create_stock_plan", result["error"].toString());
            return;
        }
        ExecutionPlan plan;
        plan.request_id = req_id;
        plan.id = result["id"].toString();
        plan.name = result["name"].toString();
        plan.status = result["status"].toString("pending");
        QJsonArray steps = result["steps"].toArray();
        for (const auto& sv : steps) {
            QJsonObject so = sv.toObject();
            PlanStep step;
            step.id = so["id"].toString();
            step.name = so["name"].toString();
            step.step_type = so["step_type"].toString();
            step.config = so["config"].toObject();
            step.status = "pending";
            plan.steps.append(step);
        }
        emit self->plan_created(plan);
    });
    return req_id;
}

QString AgentService::create_portfolio_plan(const QJsonObject& goals, const QJsonObject& config) {
    LOG_INFO("AgentService", "Portfolio plan");
    const QString req_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QJsonObject params;
    if (!goals.isEmpty())
        params["goals"] = goals;

    QPointer<AgentService> self = this;
    run_python_stdin("create_portfolio_plan", params, {}, [self, req_id](bool ok, QJsonObject result) {
        if (!self)
            return;
        if (!ok) {
            emit self->error_occurred("create_portfolio_plan", result["error"].toString());
            return;
        }
        ExecutionPlan plan;
        plan.request_id = req_id;
        plan.id = result["id"].toString();
        plan.name = result["name"].toString();
        plan.status = result["status"].toString("pending");
        QJsonArray steps = result["steps"].toArray();
        for (const auto& sv : steps) {
            QJsonObject so = sv.toObject();
            PlanStep step;
            step.id = so["id"].toString();
            step.name = so["name"].toString();
            step.step_type = so["step_type"].toString();
            step.config = so["config"].toObject();
            step.status = "pending";
            plan.steps.append(step);
        }
        emit self->plan_created(plan);
    });
    return req_id;
}

// ── Memory & Knowledge ───────────────────────────────────────────────────────

void AgentService::store_memory(const QString& content, const QString& memory_type, const QJsonObject& metadata) {
    QJsonObject params;
    params["content"] = content;
    params["memory_type"] = memory_type;
    if (!metadata.isEmpty())
        params["metadata"] = metadata;

    QPointer<AgentService> self = this;
    run_python_stdin("store_memory", params, {}, [self](bool ok, QJsonObject result) {
        if (!self)
            return;
        emit self->memory_stored(ok, ok ? "Memory stored" : result["error"].toString());
    });
}

void AgentService::recall_memories(const QString& query, const QString& memory_type, int limit) {
    QJsonObject params;
    params["query"] = query;
    if (!memory_type.isEmpty())
        params["memory_type"] = memory_type;
    params["limit"] = limit;

    QPointer<AgentService> self = this;
    run_python_stdin("recall_memories", params, {}, [self](bool ok, QJsonObject result) {
        if (!self)
            return;
        if (ok)
            emit self->memories_recalled(result["memories"].toArray());
        else
            emit self->error_occurred("recall_memories", result["error"].toString());
    });
}

void AgentService::search_knowledge(const QString& query, int limit) {
    QJsonObject params;
    params["query"] = query;
    params["limit"] = limit;

    QPointer<AgentService> self = this;
    run_python_stdin("search_knowledge", params, {}, [self](bool ok, QJsonObject result) {
        if (!self)
            return;
        if (ok)
            emit self->knowledge_results(result["results"].toArray());
        else
            emit self->error_occurred("search_knowledge", result["error"].toString());
    });
}

void AgentService::save_memory_repo(const QString& content, const QString& agent_id, const QJsonObject& options) {
    QJsonObject params;
    params["content"] = content;
    if (!agent_id.isEmpty())
        params["agent_id"] = agent_id;
    if (!options.isEmpty())
        params["options"] = options;

    QPointer<AgentService> self = this;
    run_python_stdin("save_memory", params, {}, [self](bool ok, QJsonObject result) {
        if (!self)
            return;
        emit self->memory_stored(ok, ok ? "Memory saved to repository" : result["error"].toString());
    });
}

void AgentService::search_memories_repo(const QString& query, const QString& agent_id, int limit) {
    QJsonObject params;
    params["query"] = query;
    if (!agent_id.isEmpty())
        params["agent_id"] = agent_id;
    params["limit"] = limit;

    QPointer<AgentService> self = this;
    run_python_stdin("search_memories", params, {}, [self](bool ok, QJsonObject result) {
        if (!self)
            return;
        if (ok)
            emit self->memories_recalled(result["memories"].toArray());
        else
            emit self->error_occurred("search_memories", result["error"].toString());
    });
}

// ── Session management ───────────────────────────────────────────────────────

void AgentService::save_session(const QJsonObject& session_data) {
    QJsonObject params = session_data;
    QPointer<AgentService> self = this;
    run_python_stdin("save_session", params, {}, [self](bool ok, QJsonObject result) {
        if (!self)
            return;
        emit self->session_saved(ok && result["success"].toBool(ok));
    });
}

void AgentService::get_session(const QString& session_id) {
    QJsonObject params;
    params["session_id"] = session_id;

    QPointer<AgentService> self = this;
    run_python_stdin("get_session", params, {}, [self](bool ok, QJsonObject result) {
        if (!self)
            return;
        if (ok)
            emit self->session_loaded(result);
        else
            emit self->error_occurred("get_session", result["error"].toString());
    });
}

void AgentService::add_session_message(const QString& session_id, const QString& role, const QString& content) {
    QJsonObject params;
    params["session_id"] = session_id;
    params["role"] = role;
    params["content"] = content;

    QPointer<AgentService> self = this;
    run_python_stdin("add_message", params, {}, [self](bool ok, QJsonObject result) {
        if (!self)
            return;
        emit self->session_saved(ok && result["success"].toBool(ok));
    });
}

// ── Paper trading ────────────────────────────────────────────────────────────

void AgentService::paper_execute_trade(const QString& portfolio_id, const QString& symbol, const QString& action,
                                       double quantity, double price) {
    LOG_INFO("AgentService", QString("Paper trade: %1 %2 %3").arg(action, symbol).arg(quantity));
    QJsonObject params;
    params["portfolio_id"] = portfolio_id;
    params["symbol"] = symbol;
    params["action"] = action;
    params["quantity"] = quantity;
    params["price"] = price;

    QPointer<AgentService> self = this;
    run_python_stdin("paper_execute_trade", params, {}, [self](bool ok, QJsonObject result) {
        if (!self)
            return;
        if (ok)
            emit self->trade_executed(result);
        else
            emit self->error_occurred("paper_trade", result["error"].toString());
    });
}

void AgentService::paper_get_portfolio(const QString& portfolio_id) {
    QJsonObject params;
    params["portfolio_id"] = portfolio_id;

    QPointer<AgentService> self = this;
    run_python_stdin("paper_get_portfolio", params, {}, [self](bool ok, QJsonObject result) {
        if (!self)
            return;
        if (ok)
            emit self->trade_executed(result);
        else
            emit self->error_occurred("paper_portfolio", result["error"].toString());
    });
}

void AgentService::paper_get_positions(const QString& portfolio_id) {
    QJsonObject params;
    params["portfolio_id"] = portfolio_id;

    QPointer<AgentService> self = this;
    run_python_stdin("paper_get_positions", params, {}, [self](bool ok, QJsonObject result) {
        if (!self)
            return;
        if (ok)
            emit self->trade_executed(result);
        else
            emit self->error_occurred("paper_positions", result["error"].toString());
    });
}

// ── Trade decisions ──────────────────────────────────────────────────────────

void AgentService::save_trade_decision(const QJsonObject& decision) {
    QPointer<AgentService> self = this;
    run_python_stdin("save_trade_decision", decision, {}, [self](bool ok, QJsonObject result) {
        if (!self)
            return;
        if (!ok)
            emit self->error_occurred("save_trade_decision", result["error"].toString());
    });
}

void AgentService::get_trade_decisions(const QString& competition_id, const QString& model_name) {
    QJsonObject params;
    if (!competition_id.isEmpty())
        params["competition_id"] = competition_id;
    if (!model_name.isEmpty())
        params["model_name"] = model_name;

    QPointer<AgentService> self = this;
    run_python_stdin("get_decisions", params, {}, [self](bool ok, QJsonObject result) {
        if (!self)
            return;
        if (ok)
            emit self->trade_decisions_loaded(result["decisions"].toArray());
        else
            emit self->error_occurred("get_decisions", result["error"].toString());
    });
}

// ── LRU Response Cache ───────────────────────────────────────────────────────

QString AgentService::make_cache_key(const QString& action, const QJsonObject& params) const {
    return action + "|" + QString::fromUtf8(QJsonDocument(params).toJson(QJsonDocument::Compact));
}

bool AgentService::get_cached_response(const QString& key, QJsonObject& out) const {
    QMutexLocker lock(&cache_mutex_);
    auto it = response_cache_.find(key);
    if (it == response_cache_.end())
        return false;
    if (!is_cache_fresh(it->fetched_at, kResponseCacheTtlMs))
        return false;
    out = it->data;
    return true;
}

void AgentService::set_cached_response(const QString& key, const QJsonObject& data) {
    QMutexLocker lock(&cache_mutex_);
    // Evict oldest if at capacity
    if (response_cache_.size() >= kResponseCacheMaxSize) {
        qint64 oldest_time = std::numeric_limits<qint64>::max();
        QString oldest_key;
        for (auto it = response_cache_.begin(); it != response_cache_.end(); ++it) {
            if (it->fetched_at < oldest_time) {
                oldest_time = it->fetched_at;
                oldest_key = it.key();
            }
        }
        if (!oldest_key.isEmpty())
            response_cache_.remove(oldest_key);
    }
    response_cache_[key] = {data, QDateTime::currentMSecsSinceEpoch()};
}

void AgentService::clear_response_cache() {
    QMutexLocker lock(&cache_mutex_);
    response_cache_.clear();
    LOG_INFO("AgentService", "Response cache cleared");
}

QJsonObject AgentService::get_cache_stats() const {
    QMutexLocker lock(&cache_mutex_);
    QJsonObject stats;
    stats["agents_cached"] = agents_cache_.data.size();
    stats["tools_cached"] = tools_cache_.data.total_count;
    stats["models_cached"] = models_cache_.data.count;
    stats["response_cache_size"] = response_cache_.size();
    stats["response_cache_max"] = kResponseCacheMaxSize;
    return stats;
}

} // namespace fincept::services
