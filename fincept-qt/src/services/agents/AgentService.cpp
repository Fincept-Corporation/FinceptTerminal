// src/services/agents/AgentService.cpp
#include "services/agents/AgentService.h"

#include "ai_chat/LlmService.h"
#include "auth/AuthManager.h"
#include "core/logging/Logger.h"
#include "python/PythonRunner.h"
#include "storage/cache/CacheManager.h"
#include "storage/repositories/LlmConfigRepository.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <QProcess>
#include <QUuid>

#include <memory>

namespace fincept::services {

// ── Singleton ────────────────────────────────────────────────────────────────

AgentService& AgentService::instance() {
    static AgentService inst;
    return inst;
}

AgentService::AgentService(QObject* parent) : QObject(parent) {}

// ── Cache helpers ────────────────────────────────────────────────────────────

void AgentService::clear_cache() {
    fincept::CacheManager::instance().clear_category("agents");
    LOG_INFO("AgentService", "Cache cleared");
}

QVector<AgentInfo> AgentService::cached_agents() const {
    const QVariant cv = fincept::CacheManager::instance().get("agents:list");
    if (cv.isNull())
        return {};
    const QJsonObject root = QJsonDocument::fromJson(cv.toString().toUtf8()).object();
    QVector<AgentInfo> agents;
    for (const auto& v : root["agents"].toArray()) {
        const QJsonObject o = v.toObject();
        AgentInfo info;
        info.id = o["id"].toString();
        info.name = o["name"].toString();
        info.description = o["description"].toString();
        info.category = o["category"].toString();
        info.provider = o["provider"].toString();
        info.version = o["version"].toString();
        info.config = o["config"].toObject();
        for (const auto& c : o["capabilities"].toArray())
            info.capabilities.append(c.toString());
        agents.append(info);
    }
    return agents;
}

int AgentService::cached_agent_count() const {
    const QVariant cv = fincept::CacheManager::instance().get("agents:list");
    if (cv.isNull())
        return 0;
    return QJsonDocument::fromJson(cv.toString().toUtf8()).object()["agents"].toArray().size();
}

// ── API key builder ──────────────────────────────────────────────────────────

QJsonObject AgentService::build_api_keys() const {
    // Python's _get_api_key() looks up keys by env-var name (e.g. "ANTHROPIC_API_KEY"),
    // not by lowercase provider name. Send both forms so the lookup always succeeds.
    static const QMap<QString, QString> kEnvVarNames = {
        {"anthropic", "ANTHROPIC_API_KEY"},
        {"openai", "OPENAI_API_KEY"},
        {"google", "GOOGLE_API_KEY"},
        {"gemini", "GOOGLE_API_KEY"},
        {"groq", "GROQ_API_KEY"},
        {"deepseek", "DEEPSEEK_API_KEY"},
        {"financial_datasets", "FINANCIAL_DATASETS_API_KEY"},
        {"tavily", "TAVILY_API_KEY"},
        {"mistral", "MISTRAL_API_KEY"},
        {"cohere", "COHERE_API_KEY"},
        {"xai", "XAI_API_KEY"},
    };

    QJsonObject keys;
    auto providers = LlmConfigRepository::instance().list_providers();
    if (providers.is_ok()) {
        for (const auto& p : providers.value()) {
            if (p.api_key.isEmpty())
                continue;
            const QString lower = p.provider.toLower();
            keys[lower] = p.api_key; // lowercase form
            const QString env_name = kEnvVarNames.value(lower);
            if (!env_name.isEmpty())
                keys[env_name] = p.api_key; // env-var form Python expects
            // Provider aliases: send canonical form so Python ModelsRegistry resolves correctly
            // e.g. "gemini" -> also set "google" so key lookup works after alias normalisation
            static const QMap<QString, QString> kProviderAliases = {
                {"gemini", "google"},
                {"claude", "anthropic"},
            };
            const QString alias = kProviderAliases.value(lower);
            if (!alias.isEmpty())
                keys[alias] = p.api_key;
            // Also ship base_url so super_agent/_llm_classify can use the right
            // endpoint (e.g. MiniMax OpenAI-compat behind "anthropic" provider).
            if (!p.base_url.isEmpty()) {
                keys[lower + "_base_url"] = p.base_url;
                keys[lower.toUpper() + "_BASE_URL"] = p.base_url;
            }
        }
    }

    // Always include Fincept session API key (from login) so agents can use
    // the fincept provider even if user hasn't manually configured it in Settings.
    if (!keys.contains("fincept")) {
        const auto& session = auth::AuthManager::instance().session();
        if (!session.api_key.isEmpty()) {
            keys["fincept"] = session.api_key;
            keys["FINCEPT_API_KEY"] = session.api_key;
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
                active_llm["provider"] = llm.active_provider();
                active_llm["model_id"] = llm.active_model();
                active_llm["api_key"] = llm.active_api_key();
                active_llm["base_url"] = llm.active_base_url();
                active_llm["temperature"] = llm.active_temperature();
                active_llm["max_tokens"] = llm.active_max_tokens();
                payload["active_llm"] = active_llm;
            }
        }
    }

    // Resolve user_id for per-persona SQLite isolation on the Python side.
    // Priority: params["user_id"] (caller override) > config["user_id"] > session-derived.
    // Session: user_info.id > 0 → QString::number(id); id == 0 (guest/unauth) → "guest".
    QJsonObject enriched_params = params;
    if (!enriched_params.contains("user_id") || enriched_params["user_id"].toString().isEmpty()) {
        QString uid;
        if (config.contains("user_id") && !config["user_id"].toString().isEmpty()) {
            uid = config["user_id"].toString();
        } else {
            const auto& session = auth::AuthManager::instance().session();
            uid = session.user_info.id > 0 ? QString::number(session.user_info.id)
                                           : QStringLiteral("guest");
        }
        enriched_params["user_id"] = uid;
    }

    if (!enriched_params.isEmpty())
        payload["params"] = enriched_params;
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
    // Share the standard Python env + cwd + Windows console suppression with
    // PythonRunner so every finagent spawn sees the same FINCEPT_DATA_DIR,
    // FINAGENT_DATA_DIR, and PYTHONPATH.
    proc->setProcessEnvironment(py.build_python_env());
    proc->setWorkingDirectory(py.scripts_dir());
#ifdef _WIN32
    proc->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* cpa) {
        cpa->flags |= 0x08000000; // CREATE_NO_WINDOW
    });
#endif
    QPointer<AgentService> self = this;
    auto timer = std::make_shared<QElapsedTimer>();
    timer->start();

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [self, proc, action, on_result, timer](int exit_code, QProcess::ExitStatus) {
                int elapsed = timer->elapsed();

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
        const QVariant cv = fincept::CacheManager::instance().get("agents:list");
        if (!cv.isNull()) {
            LOG_DEBUG("AgentService", "agents: serving from cache");
            const QJsonObject root = QJsonDocument::fromJson(cv.toString().toUtf8()).object();
            QVector<AgentInfo> agents;
            for (const auto& v : root["agents"].toArray()) {
                const QJsonObject o = v.toObject();
                AgentInfo info;
                info.id = o["id"].toString();
                info.name = o["name"].toString();
                info.description = o["description"].toString();
                info.category = o["category"].toString();
                info.provider = o["provider"].toString();
                info.version = o["version"].toString();
                info.config = o["config"].toObject();
                for (const auto& c : o["capabilities"].toArray())
                    info.capabilities.append(c.toString());
                agents.append(info);
            }
            QVector<AgentCategory> categories;
            for (const auto& v : root["categories"].toArray()) {
                const QJsonObject o = v.toObject();
                categories.append({o["name"].toString(), o["count"].toInt()});
            }
            emit agents_discovered(agents, categories);
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

        QJsonArray agents_arr;
        for (const auto& a : agents) {
            QJsonObject o;
            o["id"] = a.id;
            o["name"] = a.name;
            o["description"] = a.description;
            o["category"] = a.category;
            o["provider"] = a.provider;
            o["version"] = a.version;
            o["config"] = a.config;
            QJsonArray caps;
            for (const auto& c : a.capabilities)
                caps.append(c);
            o["capabilities"] = caps;
            agents_arr.append(o);
        }
        QJsonArray cats_arr;
        for (const auto& c : categories) {
            QJsonObject o;
            o["name"] = c.name;
            o["count"] = c.count;
            cats_arr.append(o);
        }
        QJsonObject root;
        root["agents"] = agents_arr;
        root["categories"] = cats_arr;
        fincept::CacheManager::instance().put(
            "agents:list", QVariant(QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact))),
            kAgentCacheTtlSec, "agents");

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
    // Share the standard Python env + cwd + Windows console suppression with
    // PythonRunner so every finagent spawn sees the same FINCEPT_DATA_DIR,
    // FINAGENT_DATA_DIR, and PYTHONPATH.
    proc->setProcessEnvironment(py.build_python_env());
    proc->setWorkingDirectory(py.scripts_dir());
#ifdef _WIN32
    proc->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* cpa) {
        cpa->flags |= 0x08000000; // CREATE_NO_WINDOW
    });
#endif
    QPointer<AgentService> self = this;
    auto timer = std::make_shared<QElapsedTimer>();
    auto accumulated = std::make_shared<QString>();
    auto final_json_line = std::make_shared<QString>();
    auto done_emitted = std::make_shared<bool>(false);
    timer->start();

    // Read stdout line by line as process writes
    connect(proc, &QProcess::readyReadStandardOutput, this,
            [self, proc, accumulated, final_json_line, done_emitted, req_id]() {
                if (!self)
                    return;
                while (proc->canReadLine()) {
                    QString line = QString::fromUtf8(proc->readLine()).trimmed();
                    if (line.isEmpty())
                        continue;

                    // Python emits "TOKEN: {content}" (stream_print adds a space after colon)
                    auto extract = [](const QString& s, const QString& prefix) -> QString {
                        QString rest = s.mid(prefix.length());
                        // Strip one leading space if present
                        if (rest.startsWith(' '))
                            rest = rest.mid(1);
                        // Unescape \n -> newline
                        rest.replace("\\n", "\n").replace("\\\\", "\\");
                        return rest;
                    };

                    if (line.startsWith('{')) {
                        // Final JSON result line — capture it for the finished handler
                        *final_json_line = line;
                    } else if (line.startsWith("TOKEN:")) {
                        QString token = extract(line, "TOKEN:");
                        *accumulated += token;
                        emit self->agent_stream_token(req_id, token);
                    } else if (line.startsWith("THINKING:")) {
                        emit self->agent_stream_thinking(req_id, extract(line, "THINKING:"));
                    } else if (line.startsWith("TOOL:")) {
                        emit self->agent_stream_thinking(req_id, "Tool: " + extract(line, "TOOL:"));
                    } else if (line.startsWith("TOOL_RESULT:")) {
                        emit self->agent_stream_thinking(req_id, "Result: " + extract(line, "TOOL_RESULT:"));
                    } else if (line.startsWith("DONE:")) {
                        // DONE carries the full cleaned response — use it if accumulated is empty
                        QString done_content = extract(line, "DONE:");
                        if (accumulated->trimmed().isEmpty() && !done_content.isEmpty())
                            *accumulated = done_content;
                    } else if (line.startsWith("ERROR:")) {
                        if (!*done_emitted) {
                            *done_emitted = true;
                            AgentExecutionResult r;
                            r.request_id = req_id;
                            r.success = false;
                            r.error = extract(line, "ERROR:");
                            emit self->agent_stream_done(r);
                        }
                    }
                }
            });

    connect(
        proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
        [self, proc, accumulated, final_json_line, done_emitted, timer, req_id](int exit_code, QProcess::ExitStatus) {
            int elapsed = timer->elapsed();

            // Drain any remaining stdout
            QString remaining = QString::fromUtf8(proc->readAllStandardOutput());
            QString stderr_str = QString::fromUtf8(proc->readAllStandardError());
            proc->deleteLater();

            if (!self || *done_emitted)
                return;
            *done_emitted = true;

            // Scan remaining stdout for final JSON line (may not have been read yet)
            for (const QString& line : remaining.split('\n')) {
                QString t = line.trimmed();
                if (t.startsWith('{'))
                    *final_json_line = t;
            }

            AgentExecutionResult r;
            r.request_id = req_id;
            r.execution_time_ms = elapsed;

            QJsonDocument doc = QJsonDocument::fromJson(final_json_line->toUtf8());
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

            LOG_INFO("AgentService", QString("Streaming completed in %1ms").arg(elapsed));
            emit self->agent_stream_done(r);
        });

    connect(proc, &QProcess::errorOccurred, this,
            [self, proc, accumulated, final_json_line, done_emitted, timer, req_id](QProcess::ProcessError) {
                QString err = proc->errorString();
                proc->deleteLater();
                if (!self || *done_emitted)
                    return;
                *done_emitted = true;
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
    const QString req_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    LOG_INFO("AgentService", QString("Running team query [%1]: %2").arg(req_id.left(8), query.left(80)));

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
    params["team_config"] = team_config;

    // Promote coordinator model to active_llm so Python resolves it correctly
    QJsonObject coord_config;
    if (team_config.contains("model"))
        coord_config["model"] = team_config["model"];

    QJsonObject payload = build_payload("run_team", params, coord_config);
    QByteArray payload_bytes = QJsonDocument(payload).toJson(QJsonDocument::Compact);

    QString python_path = py.python_path();
    QString script_path = py.scripts_dir() + "/agents/finagent_core/main.py";

    auto* proc = new QProcess(this);
    // Share the standard Python env + cwd + Windows console suppression with
    // PythonRunner so every finagent spawn sees the same FINCEPT_DATA_DIR,
    // FINAGENT_DATA_DIR, and PYTHONPATH.
    proc->setProcessEnvironment(py.build_python_env());
    proc->setWorkingDirectory(py.scripts_dir());
#ifdef _WIN32
    proc->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* cpa) {
        cpa->flags |= 0x08000000; // CREATE_NO_WINDOW
    });
#endif
    QPointer<AgentService> self = this;
    auto timer = std::make_shared<QElapsedTimer>();
    auto accumulated = std::make_shared<QString>();
    auto final_json = std::make_shared<QString>();
    auto done_emitted = std::make_shared<bool>(false);
    timer->start();

    connect(proc, &QProcess::readyReadStandardOutput, this,
            [self, proc, accumulated, final_json, done_emitted, req_id]() {
                if (!self)
                    return;
                while (proc->canReadLine()) {
                    QString line = QString::fromUtf8(proc->readLine()).trimmed();
                    if (line.isEmpty())
                        continue;

                    auto extract = [](const QString& s, const QString& prefix) -> QString {
                        QString rest = s.mid(prefix.length());
                        if (rest.startsWith(' '))
                            rest = rest.mid(1);
                        rest.replace("\\n", "\n").replace("\\\\", "\\");
                        return rest;
                    };

                    if (line.startsWith('{')) {
                        *final_json = line;
                    } else if (line.startsWith("TOKEN:")) {
                        QString token = extract(line, "TOKEN:");
                        *accumulated += token;
                        emit self->agent_stream_token(req_id, token);
                    } else if (line.startsWith("THINKING:")) {
                        emit self->agent_stream_thinking(req_id, extract(line, "THINKING:"));
                    } else if (line.startsWith("TOOL:")) {
                        emit self->agent_stream_thinking(req_id, "Tool: " + extract(line, "TOOL:"));
                    } else if (line.startsWith("DONE:")) {
                        QString done_content = extract(line, "DONE:");
                        if (accumulated->trimmed().isEmpty() && !done_content.isEmpty())
                            *accumulated = done_content;
                    } else if (line.startsWith("ERROR:")) {
                        if (!*done_emitted) {
                            *done_emitted = true;
                            AgentExecutionResult r;
                            r.request_id = req_id;
                            r.success = false;
                            r.error = extract(line, "ERROR:");
                            emit self->agent_stream_done(r);
                        }
                    }
                }
            });

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [self, proc, accumulated, final_json, done_emitted, timer, req_id](int exit_code, QProcess::ExitStatus) {
                int elapsed = timer->elapsed();
                QString remaining = QString::fromUtf8(proc->readAllStandardOutput());
                QString stderr_str = QString::fromUtf8(proc->readAllStandardError());
                proc->deleteLater();

                if (!self || *done_emitted)
                    return;
                *done_emitted = true;

                for (const QString& line : remaining.split('\n')) {
                    QString t = line.trimmed();
                    if (t.startsWith('{'))
                        *final_json = t;
                }

                AgentExecutionResult r;
                r.request_id = req_id;
                r.execution_time_ms = elapsed;

                QJsonDocument doc = QJsonDocument::fromJson(final_json->toUtf8());
                if (!doc.isNull() && doc.object()["success"].toBool()) {
                    QString json_response = doc.object()["response"].toString();
                    r.success = true;
                    r.response = json_response.isEmpty() ? accumulated->trimmed() : json_response;
                } else if (!accumulated->trimmed().isEmpty()) {
                    r.success = true;
                    r.response = accumulated->trimmed();
                } else {
                    r.success = false;
                    r.error = exit_code != 0 ? stderr_str.left(500) : "No response from team";
                }

                LOG_INFO("AgentService", QString("Team completed in %1ms").arg(elapsed));
                emit self->agent_stream_done(r);
            });

    connect(proc, &QProcess::errorOccurred, this, [self, proc, done_emitted, timer, req_id](QProcess::ProcessError) {
        QString err = proc->errorString();
        proc->deleteLater();
        if (!self || *done_emitted)
            return;
        *done_emitted = true;
        AgentExecutionResult r;
        r.request_id = req_id;
        r.success = false;
        r.error = "Process error: " + err;
        emit self->agent_stream_done(r);
    });

    LOG_INFO("AgentService", QString("Starting team stream (%1 bytes payload, %2 members)")
                                 .arg(payload_bytes.size())
                                 .arg(team_config["members"].toArray().size()));
    proc->start(python_path, {script_path, "--stdin", "--stream"});
    proc->write(payload_bytes);
    proc->closeWriteChannel();
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

        // Python returns {"success": true, "plan": {...}} — unwrap the nested plan object
        const QJsonObject planObj = result.contains("plan") ? result["plan"].toObject() : result;

        ExecutionPlan plan;
        plan.request_id = req_id;
        plan.id = planObj["id"].toString();
        plan.name = planObj["name"].toString();
        plan.description = planObj["description"].toString();
        plan.status = planObj["status"].toString("pending");
        plan.is_complete = planObj["is_complete"].toBool();

        QJsonArray steps = planObj["steps"].toArray();
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
        const QVariant cv = fincept::CacheManager::instance().get("agents:sysinfo");
        if (!cv.isNull()) {
            const QJsonObject r = QJsonDocument::fromJson(cv.toString().toUtf8()).object();
            AgentSystemInfo info;
            info.version = r["version"].toString();
            info.framework = r["framework"].toString();
            info.capabilities = r["capabilities"].toObject();
            for (const auto& f : r["features"].toArray())
                info.features.append(f.toString());
            emit system_info_loaded(info);
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
        for (const auto& f : result["features"].toArray())
            info.features.append(f.toString());

        fincept::CacheManager::instance().put(
            "agents:sysinfo", QVariant(QString::fromUtf8(QJsonDocument(result).toJson(QJsonDocument::Compact))),
            kSysInfoCacheTtlSec, "agents");

        emit self->system_info_loaded(info);
    });
}

void AgentService::list_tools() {
    {
        const QVariant cv = fincept::CacheManager::instance().get("agents:tools");
        if (!cv.isNull()) {
            const QJsonObject r = QJsonDocument::fromJson(cv.toString().toUtf8()).object();
            AgentToolsInfo info;
            info.tools = r["tools"].toObject();
            info.total_count = r["total_count"].toInt();
            for (const auto& c : r["categories"].toArray())
                info.categories.append(c.toString());
            emit tools_loaded(info);
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
        info.total_count = result["total_count"].toInt();
        for (const auto& c : result["categories"].toArray())
            info.categories.append(c.toString());

        fincept::CacheManager::instance().put(
            "agents:tools", QVariant(QString::fromUtf8(QJsonDocument(result).toJson(QJsonDocument::Compact))),
            kToolsCacheTtlSec, "agents");

        emit self->tools_loaded(info);
    });
}

void AgentService::list_models() {
    {
        const QVariant cv = fincept::CacheManager::instance().get("agents:models");
        if (!cv.isNull()) {
            const QJsonObject r = QJsonDocument::fromJson(cv.toString().toUtf8()).object();
            AgentModelsInfo info;
            info.count = r["count"].toInt();
            for (const auto& p : r["providers"].toArray())
                info.providers.append(p.toString());
            emit models_loaded(info);
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
        info.count = result["count"].toInt();
        for (const auto& p : result["providers"].toArray())
            info.providers.append(p.toString());

        fincept::CacheManager::instance().put(
            "agents:models", QVariant(QString::fromUtf8(QJsonDocument(result).toJson(QJsonDocument::Compact))),
            kModelsCacheTtlSec, "agents");

        emit self->models_loaded(info);
    });
}

// ── Config CRUD ──────────────────────────────────────────────────────────────

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

QString AgentService::run_agent_structured(const QString& query, const QJsonObject& config,
                                           const QString& output_model) {
    const QString req_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    LOG_INFO("AgentService",
             QString("Running structured agent [%1] (%2): %3").arg(req_id.left(8), output_model, query.left(60)));
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

void AgentService::store_memory(const QString& content, const QString& memory_type,
                                const QJsonObject& metadata, const QString& agent_id) {
    QJsonObject params;
    params["content"] = content;
    params["memory_type"] = memory_type;
    if (!metadata.isEmpty())
        params["metadata"] = metadata;
    if (!agent_id.isEmpty())
        params["agent_id"] = agent_id;

    QPointer<AgentService> self = this;
    run_python_stdin("store_memory", params, {}, [self](bool ok, QJsonObject result) {
        if (!self)
            return;
        emit self->memory_stored(ok, ok ? "Memory stored" : result["error"].toString());
    });
}

void AgentService::recall_memories(const QString& query, const QString& memory_type, int limit,
                                   const QString& agent_id) {
    QJsonObject params;
    params["query"] = query;
    if (!memory_type.isEmpty())
        params["memory_type"] = memory_type;
    params["limit"] = limit;
    if (!agent_id.isEmpty())
        params["agent_id"] = agent_id;

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
    const QVariant cv = fincept::CacheManager::instance().get("agents:resp:" + key);
    if (cv.isNull())
        return false;
    out = QJsonDocument::fromJson(cv.toString().toUtf8()).object();
    return true;
}

void AgentService::set_cached_response(const QString& key, const QJsonObject& data) {
    fincept::CacheManager::instance().put(
        "agents:resp:" + key, QVariant(QString::fromUtf8(QJsonDocument(data).toJson(QJsonDocument::Compact))),
        kResponseCacheTtlSec, "agents");
}

void AgentService::clear_response_cache() {
    fincept::CacheManager::instance().remove_prefix("agents:resp:");
    LOG_INFO("AgentService", "Response cache cleared");
}

QJsonObject AgentService::get_cache_stats() const {
    QJsonObject stats;
    stats["agents_cached"] = cached_agent_count();
    stats["total_entries"] = fincept::CacheManager::instance().entry_count();
    return stats;
}

} // namespace fincept::services
