#include "services/alpha_arena/ModelDispatcher.h"

#include "core/logging/Logger.h"
#include "python/PythonRunner.h"
#include "services/alpha_arena/AlphaArenaJson.h"
#include "services/alpha_arena/ContextBuilder.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QPointer>
#include <QStandardPaths>
#include <QUuid>

namespace fincept::services::alpha_arena {

namespace {

QString sha256_hex(const QString& s) {
    return QString::fromLatin1(
        QCryptographicHash::hash(s.toUtf8(), QCryptographicHash::Sha256).toHex());
}

/// Where to write the per-tick request files. Subprocess deletes after read.
QString tmp_request_dir() {
    QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) +
                  QStringLiteral("/fincept-aa");
    QDir().mkpath(dir);
    return dir;
}

/// Build a name unique-per-run-but-Qt-LocalServer-safe.
QString new_pipe_name(const QString& comp_id, int seq) {
    return QStringLiteral("fincept-aa-%1-%2-%3")
        .arg(comp_id.left(8))
        .arg(seq)
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces).left(8));
}

QJsonArray actions_to_json_array(const QVector<ProposedAction>& actions) {
    QJsonArray arr;
    for (const auto& a : actions) arr.append(action_to_json(a));
    return arr;
}

QString market_snapshot_json(const QVector<MarketSnapshot>& markets) {
    QJsonArray arr;
    for (const auto& m : markets) {
        QJsonObject o;
        o["coin"] = m.coin;
        o["mid"] = m.mid;
        o["bid"] = m.bid;
        o["ask"] = m.ask;
        o["funding_rate"] = m.funding_rate;
        // Bars omitted from this snapshot — they live in the prompt rows
        // via the prompt sha256. Keep the snapshot light for the audit log.
        arr.append(o);
    }
    return QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

} // namespace

ModelDispatcher::ModelDispatcher(QObject* parent)
    : QObject(parent), secrets_(new SecretRedemptionServer(this)) {}

void ModelDispatcher::configure(const QString& competition_id,
                                CompetitionMode mode,
                                const QVector<DispatchAgent>& agents) {
    competition_id_ = competition_id;
    mode_ = mode;
    agents_ = agents;
    LOG_INFO("AlphaArena.Dispatch",
             QString("configured comp=%1 mode=%2 agents=%3")
                 .arg(competition_id, mode_to_wire(mode))
                 .arg(agents.size()));
}

void ModelDispatcher::set_stub_response_provider(StubResponseProvider p) {
    stub_provider_ = std::move(p);
}

void ModelDispatcher::dispatch_tick(Tick t,
                                    TickContext ctx,
                                    QHash<QString, AgentAccountState> per_agent_state,
                                    std::optional<SituationalContext> situational) {
    if (competition_id_.isEmpty() || agents_.isEmpty()) {
        LOG_WARN("AlphaArena.Dispatch", "tick fired with no configured competition");
        emit tick_dispatch_complete(t.seq);
        return;
    }

    // 1. Build & persist the system prompt (idempotent — prompt content-addressed).
    const QString system_prompt = build_system_prompt(mode_);
    const QString sys_sha = sha256_hex(system_prompt);
    AlphaArenaRepo::instance().upsert_prompt(sys_sha, system_prompt);

    // 2. Persist the tick row.
    const auto tick_r = AlphaArenaRepo::instance().insert_tick(
        competition_id_, t.seq, t.utc_ms, sys_sha, market_snapshot_json(ctx.markets));
    if (tick_r.is_err()) {
        LOG_ERROR("AlphaArena.Dispatch",
                  QString("insert_tick failed: %1").arg(QString::fromStdString(tick_r.error())));
        emit tick_dispatch_complete(t.seq);
        return;
    }
    const QString tick_id = tick_r.value();
    tick_ids_.insert(t.seq, tick_id);

    // 3. Open a per-tick SecretRedemptionServer.
    const QString pipe_name = new_pipe_name(competition_id_, t.seq);
    if (!secrets_->start(pipe_name)) {
        LOG_ERROR("AlphaArena.Dispatch", "failed to start secret IPC server — aborting tick");
        AlphaArenaRepo::instance().mark_tick_skipped(tick_id);
        emit tick_dispatch_complete(t.seq);
        return;
    }

    // 4. Per-agent: build user prompt, write request file, spawn subprocess.
    outstanding_.insert(t.seq, agents_.size());

    for (const auto& agent : agents_) {
        // Per-agent state — fall back to a zero-equity stub if the engine
        // hasn't supplied one (e.g. seed cycle).
        AgentAccountState state;
        if (auto it = per_agent_state.constFind(agent.agent_id); it != per_agent_state.constEnd()) {
            state = it.value();
        }

        const QString user_prompt = build_user_prompt(ctx, state, mode_, situational);
        const QString user_sha = sha256_hex(user_prompt);
        AlphaArenaRepo::instance().upsert_prompt(user_sha, user_prompt);

        // Test-only short-circuit: if a stub provider is installed, build the
        // decision directly off its return value and emit synchronously.
        // Skips the request-file + subprocess path entirely.
        if (stub_provider_) {
            const QString raw = stub_provider_(agent.agent_id, system_prompt, user_prompt);
            AlphaArenaRepo::DecisionInsert d;
            d.tick_id = tick_id;
            d.agent_id = agent.agent_id;
            d.user_prompt_sha256 = user_sha;
            d.raw_response = raw;
            d.risk_verdict_json = QStringLiteral("[]");
            ModelResponse parsed = parse_model_response(raw);
            if (parsed.parse_error.has_value()) d.parse_error = *parsed.parse_error;
            d.parsed_actions_json = QString::fromUtf8(
                QJsonDocument(actions_to_json_array(parsed.actions))
                    .toJson(QJsonDocument::Compact));
            const auto dr = AlphaArenaRepo::instance().insert_decision(d);
            const QString dec_id = dr.is_ok() ? dr.value() : QString();
            QJsonObject p;
            p[QStringLiteral("decision_id")] = dec_id;
            p[QStringLiteral("latency_ms")] = 0;
            p[QStringLiteral("parse_error")] = d.parse_error;
            AlphaArenaRepo::instance().append_event(competition_id_, agent.agent_id,
                                                    QStringLiteral("decision"), p);
            emit decision_received(dec_id, agent.agent_id, parsed.actions, d.parse_error);
            outstanding_[t.seq]--;
            if (outstanding_[t.seq] == 0) {
                close_secret_server_when_done();
                AlphaArenaRepo::instance().close_tick(tick_id);
                outstanding_.remove(t.seq);
                tick_ids_.remove(t.seq);
                emit tick_dispatch_complete(t.seq);
            }
            continue;
        }

        // Write request file. Notice: API key is NOT in this file. Only the
        // secret_handle (SecureStorage key) is — and the subprocess fetches
        // the plaintext via the IPC server.
        QJsonObject model_obj;
        model_obj["provider"] = agent.provider;
        model_obj["id"] = agent.model_id;
        if (!agent.base_url.isEmpty()) model_obj["base_url"] = agent.base_url;

        QJsonObject req;
        req["system_prompt"] = system_prompt;
        req["user_prompt"] = user_prompt;
        req["model"] = model_obj;
        req["temperature"] = 0.1;
        req["max_output_tokens"] = 4096;
        req["secret_handle"] = agent.api_secret_handle;
        req["secret_pipe"] = pipe_name;

        const QString req_path = tmp_request_dir() +
            QStringLiteral("/req-%1-%2.json")
                .arg(t.seq)
                .arg(QUuid::createUuid().toString(QUuid::WithoutBraces).left(8));
        QFile f(req_path);
        if (!f.open(QIODevice::WriteOnly)) {
            LOG_ERROR("AlphaArena.Dispatch",
                      QString("could not write request file %1").arg(req_path));
            // Persist a parse-error decision so the audit row exists.
            AlphaArenaRepo::DecisionInsert d;
            d.tick_id = tick_id;
            d.agent_id = agent.agent_id;
            d.user_prompt_sha256 = user_sha;
            d.parse_error = QStringLiteral("could not write request file");
            const auto dr = AlphaArenaRepo::instance().insert_decision(d);
            const QString dec_id = dr.is_ok() ? dr.value() : QString();
            emit decision_received(dec_id, agent.agent_id, {}, d.parse_error);
            outstanding_[t.seq]--;
            if (outstanding_[t.seq] == 0) close_secret_server_when_done();
            continue;
        }
        f.write(QJsonDocument(req).toJson(QJsonDocument::Compact));
        f.close();

        // Subprocess. We capture *only* the data we need — no `this` raw —
        // and the engine guarantees ModelDispatcher outlives any tick.
        QPointer<ModelDispatcher> self = this;
        const QString agent_id = agent.agent_id;
        const QString display_name = agent.display_name;
        const int seq = t.seq;
        const QString user_sha_capture = user_sha;
        const QString comp_id = competition_id_;

        fincept::python::PythonRunner::instance().run(
            QStringLiteral("alpha_arena/llm_call.py"),
            {req_path},
            [self, tick_id, agent_id, display_name, seq, user_sha_capture, comp_id]
            (const fincept::python::PythonResult& result) {
                if (!self) return;

                AlphaArenaRepo::DecisionInsert d;
                d.tick_id = tick_id;
                d.agent_id = agent_id;
                d.user_prompt_sha256 = user_sha_capture;

                ModelResponse parsed;
                if (!result.success) {
                    d.parse_error = result.error.isEmpty()
                                        ? QStringLiteral("subprocess failed")
                                        : result.error;
                } else {
                    const QString out_json = fincept::python::extract_json(result.output);
                    auto doc = QJsonDocument::fromJson(out_json.toUtf8());
                    if (!doc.isObject()) {
                        d.parse_error = QStringLiteral("subprocess returned non-JSON");
                    } else {
                        const auto obj = doc.object();
                        if (obj.contains("error")) {
                            d.parse_error =
                                QStringLiteral("subprocess: ") + obj.value("error").toString();
                        } else {
                            d.raw_response = obj.value("response_text").toString();
                            d.latency_ms = obj.value("latency_ms").toInt();
                            d.tokens_in = obj.value("tokens_in").toInt();
                            d.tokens_out = obj.value("tokens_out").toInt();
                            d.cost_usd = obj.value("cost_usd").toDouble();
                            parsed = parse_model_response(d.raw_response);
                            if (parsed.parse_error.has_value()) {
                                d.parse_error = *parsed.parse_error;
                            }
                            d.parsed_actions_json = QString::fromUtf8(
                                QJsonDocument(actions_to_json_array(parsed.actions))
                                    .toJson(QJsonDocument::Compact));
                        }
                    }
                }

                const auto dr = AlphaArenaRepo::instance().insert_decision(d);
                const QString dec_id = dr.is_ok() ? dr.value() : QString();

                // Telemetry event for RiskPanel — captures latency, parse status,
                // token usage. Always written, regardless of parse success.
                {
                    QJsonObject p;
                    p[QStringLiteral("decision_id")] = dec_id;
                    p[QStringLiteral("latency_ms")] = d.latency_ms;
                    p[QStringLiteral("tokens_in")] = d.tokens_in;
                    p[QStringLiteral("tokens_out")] = d.tokens_out;
                    p[QStringLiteral("parse_error")] = d.parse_error;
                    AlphaArenaRepo::instance().append_event(comp_id, agent_id,
                                                            QStringLiteral("decision"), p);
                }

                emit self->decision_received(dec_id, agent_id, parsed.actions, d.parse_error);

                self->outstanding_[seq]--;
                if (self->outstanding_[seq] == 0) {
                    self->close_secret_server_when_done();
                    AlphaArenaRepo::instance().close_tick(tick_id);
                    self->outstanding_.remove(seq);
                    self->tick_ids_.remove(seq);
                    emit self->tick_dispatch_complete(seq);
                }
            });
    }
}

void ModelDispatcher::close_secret_server_when_done() {
    secrets_->stop();
}

} // namespace fincept::services::alpha_arena
