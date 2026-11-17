// TaskRunStore.cpp — parameterized SQL for the agent_task_* tables (v059) and
// agent_run_artefacts / agent_run_templates plus the added columns (v063).

#include "services/agent_tasks/TaskRunStore.h"

#include "core/logging/Logger.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace fincept::agent_tasks {

namespace task_run_store_detail {

constexpr const char* kTaskRunStoreTag = "TaskRunStore";

// v059 columns, then v063's (active_ms … credits_unreported).
constexpr const char* kRunColumns =
    "id, title, prompt, engine, plan_first, mode, template_id, session_uuid, remote_task_id, profile_id, "
    "context_json, status, current_step, plan_json, max_credits, max_steps, max_minutes, credits_spent, "
    "tokens_used, turns, final_text, error, origin, parent_run_id, schedule_id, created_at, started_at, "
    "finished_at, updated_at, active_ms, active_since, credits_unreported";
constexpr int kRunColumnCount = 32;

constexpr const char* kStepColumns = "run_id, idx, title, instruction, deliverable, status, summary, attempts, credits, "
                                     "message_uuid, started_at, finished_at";
constexpr int kStepColumnCount = 12;

constexpr const char* kEventColumns = "seq, ts, step_idx, kind, title, body, data_json, is_error, ref_id";

// agent_run_artefacts (v063) — v059's agent_task_artefacts cascades with its run
// and cannot keep a pinned artefact; its rows were moved here.
constexpr const char* kArtefactColumns = "id, run_id, step_idx, kind, title, mime, stored_name, inline_json, bytes, "
                                         "created_at, run_title, source_json, pinned, updated_at";
constexpr int kArtefactColumnCount = 14;
constexpr int kTaskRunMaxArtefactsLoaded = 200000;

// v059 columns, then v063's (time_zone … last_error).
constexpr const char* kScheduleColumns =
    "id, name, template_id, prompt, engine, mode, request_json, max_credits, kind, interval_minutes, daily_time, "
    "weekdays_mask, enabled, paused_for_review, last_run_at, next_run_at, created_at, updated_at, time_zone, once_at, "
    "catch_up_minutes, last_run_id, last_status, last_error";
constexpr int kScheduleColumnCount = 24;

/// SQLite's historical bound-parameter limit is 999 — chunk IN (...) lists well below it.
constexpr int kTaskRunDeleteChunk = 400;
constexpr int kTaskRunMaxEventsLoaded = 20000;

QVariant trs_ms_or_null(qint64 ms) {
    return ms > 0 ? QVariant(ms) : QVariant();
}

QVariant trs_str_or_null(const QString& s) {
    return s.isEmpty() ? QVariant() : QVariant(s);
}

/// Bind value for a NOT NULL text column: Qt binds a null QString as SQL NULL,
/// and `DEFAULT ''` only applies when the column is omitted.
QVariant trs_text(const QString& s) {
    return s.isNull() ? QVariant(QStringLiteral("")) : QVariant(s);
}

QString trs_json_text(const QJsonObject& o) {
    return o.isEmpty() ? QStringLiteral("{}") : QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

QString trs_json_text(const QJsonArray& a) {
    return a.isEmpty() ? QString(QStringLiteral("")) : QString::fromUtf8(QJsonDocument(a).toJson(QJsonDocument::Compact));
}

QJsonObject trs_parse_object(const QString& s) {
    if (s.isEmpty())
        return {};
    return QJsonDocument::fromJson(s.toUtf8()).object();
}

QJsonArray trs_parse_array(const QString& s) {
    if (s.isEmpty())
        return {};
    return QJsonDocument::fromJson(s.toUtf8()).array();
}

/// inline_json holds any JSON value, wrapped as {"value": …} so scalars survive.
QString trs_inline_text(const QJsonValue& v) {
    if (v.isUndefined() || v.isNull())
        return QStringLiteral("");
    QJsonObject wrap;
    wrap.insert(QStringLiteral("value"), v);
    return QString::fromUtf8(QJsonDocument(wrap).toJson(QJsonDocument::Compact));
}

QJsonValue trs_inline_value(const QString& s) {
    if (s.isEmpty())
        return QJsonValue(QJsonValue::Undefined);
    return QJsonDocument::fromJson(s.toUtf8()).object().value(QStringLiteral("value"));
}

QString trs_placeholders(int n) {
    QStringList p;
    p.reserve(n);
    for (int i = 0; i < n; ++i)
        p << QStringLiteral("?");
    return p.join(QStringLiteral(", "));
}

TaskRun trs_map_run(QSqlQuery& q) {
    TaskRun r;
    r.id = q.value(0).toString();
    r.title = q.value(1).toString();
    r.prompt = q.value(2).toString();
    r.engine = q.value(3).toString();
    r.plan_first = q.value(4).toInt() != 0;
    r.mode = q.value(5).toString();
    r.template_id = q.value(6).toString();
    r.session_uuid = q.value(7).toString();
    r.remote_task_id = q.value(8).toString();
    r.profile_id = q.value(9).toInt();
    r.context = trs_parse_object(q.value(10).toString());
    r.status = run_status_from_id(q.value(11).toString()).value_or(RunStatus::Failed);
    r.current_step = q.value(12).toInt();
    r.plan = trs_parse_array(q.value(13).toString());
    r.budget.max_credits = q.value(14).toDouble();
    r.budget.max_steps = q.value(15).toInt();
    r.budget.max_minutes = q.value(16).toInt();
    r.credits_spent = q.value(17).toDouble();
    r.tokens_used = q.value(18).toLongLong();
    r.turns = q.value(19).toInt();
    r.final_text = q.value(20).toString();
    r.error = q.value(21).toString();
    r.origin = q.value(22).toString();
    r.parent_run_id = q.value(23).toString();
    r.schedule_id = q.value(24).toString();
    r.created_at_ms = q.value(25).toLongLong();
    r.started_at_ms = q.value(26).isNull() ? 0 : q.value(26).toLongLong();
    r.finished_at_ms = q.value(27).isNull() ? 0 : q.value(27).toLongLong();
    r.updated_at_ms = q.value(28).toLongLong();
    r.active_ms = q.value(29).toLongLong();
    r.active_since_ms = q.value(30).toLongLong();
    r.credits_unreported = q.value(31).toInt();
    return r;
}

RunStep trs_map_step(QSqlQuery& q) {
    RunStep s;
    s.run_id = q.value(0).toString();
    s.idx = q.value(1).toInt();
    s.title = q.value(2).toString();
    s.instruction = q.value(3).toString();
    s.deliverable = q.value(4).toString();
    s.status = step_status_from_id(q.value(5).toString()).value_or(StepStatus::Pending);
    s.summary = q.value(6).toString();
    s.attempts = q.value(7).toInt();
    s.credits = q.value(8).toDouble();
    s.message_uuid = q.value(9).toString();
    s.started_at_ms = q.value(10).isNull() ? 0 : q.value(10).toLongLong();
    s.finished_at_ms = q.value(11).isNull() ? 0 : q.value(11).toLongLong();
    return s;
}

RunEvent trs_map_event(QSqlQuery& q) {
    RunEvent e;
    e.seq = q.value(0).toInt();
    e.ts_ms = q.value(1).toLongLong();
    e.step = q.value(2).toInt();
    e.kind = event_kind_from_id(q.value(3).toString()).value_or(RunEventKind::Note);
    e.title = q.value(4).toString();
    e.body = q.value(5).toString();
    e.data = trs_parse_object(q.value(6).toString());
    e.is_error = q.value(7).toInt() != 0;
    e.ref_id = q.value(8).toString();
    return e;
}

RunArtefact trs_map_artefact(QSqlQuery& q) {
    RunArtefact a;
    a.id = q.value(0).toString();
    a.run_id = q.value(1).toString();
    a.step_idx = q.value(2).toInt();
    a.kind = q.value(3).toString();
    a.title = q.value(4).toString();
    a.mime = q.value(5).toString();
    a.stored_name = q.value(6).toString();
    a.inline_json = trs_inline_value(q.value(7).toString());
    a.bytes = q.value(8).toLongLong();
    a.created_at_ms = q.value(9).toLongLong();
    a.run_title = q.value(10).toString();
    a.source = trs_parse_object(q.value(11).toString());
    a.pinned = q.value(12).toInt() != 0;
    return a;
}

RunSchedule trs_map_schedule(QSqlQuery& q) {
    RunSchedule s;
    s.id = q.value(0).toString();
    s.name = q.value(1).toString();
    s.template_id = q.value(2).toString();
    s.prompt = q.value(3).toString();
    s.engine = q.value(4).toString();
    s.mode = q.value(5).toString();
    s.request = run_request_from_json(trs_parse_object(q.value(6).toString()));
    s.max_credits = q.value(7).toDouble();
    s.kind = q.value(8).toString();
    s.interval_minutes = q.value(9).toInt();
    s.daily_time = q.value(10).toString();
    s.weekdays_mask = q.value(11).toInt();
    s.enabled = q.value(12).toInt() != 0;
    s.paused_for_review = q.value(13).toInt() != 0;
    s.last_run_at_ms = q.value(14).isNull() ? 0 : q.value(14).toLongLong();
    s.next_run_at_ms = q.value(15).isNull() ? 0 : q.value(15).toLongLong();
    s.created_at_ms = q.value(16).toLongLong();
    s.updated_at_ms = q.value(17).toLongLong();
    s.time_zone = q.value(18).toString();
    s.once_at_ms = q.value(19).toLongLong();
    s.catch_up_minutes = q.value(20).isNull() ? 720 : q.value(20).toInt();
    s.last_run_id = q.value(21).toString();
    s.last_status = q.value(22).toString();
    s.last_error = q.value(23).toString();
    return s;
}

QVariantList trs_step_params(const RunStep& s) {
    return {s.run_id,
            s.idx,
            trs_text(s.title),
            trs_text(s.instruction),
            trs_text(s.deliverable),
            step_status_id(s.status),
            trs_text(s.summary),
            s.attempts,
            s.credits,
            trs_text(s.message_uuid),
            trs_ms_or_null(s.started_at_ms),
            trs_ms_or_null(s.finished_at_ms)};
}

} // namespace task_run_store_detail

using namespace task_run_store_detail;

TaskRunStore& TaskRunStore::instance() {
    static TaskRunStore inst;
    return inst;
}

// ── Runs ─────────────────────────────────────────────────────────────────────

Result<QVector<TaskRun>> TaskRunStore::load_runs(int limit) const {
    return query_list(QStringLiteral("SELECT %1 FROM agent_task_runs ORDER BY created_at DESC LIMIT ?")
                          .arg(QLatin1String(kRunColumns)),
                      {qMax(1, limit)}, trs_map_run);
}

Result<void> TaskRunStore::upsert_run(const TaskRun& r) const {
    const QString sql =
        QStringLiteral("INSERT INTO agent_task_runs (%1) VALUES (%2) "
                       "ON CONFLICT(id) DO UPDATE SET title=excluded.title, prompt=excluded.prompt, "
                       "engine=excluded.engine, plan_first=excluded.plan_first, mode=excluded.mode, "
                       "template_id=excluded.template_id, session_uuid=excluded.session_uuid, "
                       "remote_task_id=excluded.remote_task_id, profile_id=excluded.profile_id, "
                       "context_json=excluded.context_json, status=excluded.status, "
                       "current_step=excluded.current_step, plan_json=excluded.plan_json, "
                       "max_credits=excluded.max_credits, max_steps=excluded.max_steps, "
                       "max_minutes=excluded.max_minutes, credits_spent=excluded.credits_spent, "
                       "tokens_used=excluded.tokens_used, turns=excluded.turns, final_text=excluded.final_text, "
                       "error=excluded.error, origin=excluded.origin, parent_run_id=excluded.parent_run_id, "
                       "schedule_id=excluded.schedule_id, started_at=excluded.started_at, "
                       "finished_at=excluded.finished_at, updated_at=excluded.updated_at, "
                       "active_ms=excluded.active_ms, active_since=excluded.active_since, "
                       "credits_unreported=excluded.credits_unreported")
            .arg(QLatin1String(kRunColumns), trs_placeholders(kRunColumnCount));
    return exec_write(sql, {r.id,
                            trs_text(r.title),
                            trs_text(r.prompt),
                            trs_text(r.engine),
                            r.plan_first ? 1 : 0,
                            trs_text(r.mode),
                            trs_text(r.template_id),
                            trs_text(r.session_uuid),
                            trs_text(r.remote_task_id),
                            r.profile_id,
                            trs_json_text(r.context),
                            run_status_id(r.status),
                            r.current_step,
                            trs_json_text(r.plan),
                            r.budget.max_credits,
                            r.budget.max_steps,
                            r.budget.max_minutes,
                            r.credits_spent,
                            r.tokens_used,
                            r.turns,
                            trs_text(r.final_text),
                            trs_text(r.error),
                            r.origin.isEmpty() ? QStringLiteral("user") : r.origin,
                            trs_str_or_null(r.parent_run_id),
                            trs_str_or_null(r.schedule_id),
                            r.created_at_ms,
                            trs_ms_or_null(r.started_at_ms),
                            trs_ms_or_null(r.finished_at_ms),
                            r.updated_at_ms > 0 ? r.updated_at_ms : r.created_at_ms,
                            qMax<qint64>(0, r.active_ms),
                            qMax<qint64>(0, r.active_since_ms),
                            qMax(0, r.credits_unreported)});
}

Result<void> TaskRunStore::delete_runs(const QStringList& ids) const {
    if (ids.isEmpty())
        return Result<void>::ok();
    auto tx = db().begin_transaction();
    if (tx.is_err())
        return tx;
    for (qsizetype off = 0; off < ids.size(); off += kTaskRunDeleteChunk) {
        const QStringList chunk = ids.mid(off, kTaskRunDeleteChunk);
        QVariantList params;
        params.reserve(chunk.size());
        for (const auto& id : chunk)
            params << id;
        const QString ph = trs_placeholders(static_cast<int>(chunk.size()));
        // Explicit child deletes as well as the FK cascade: correct even on a
        // connection where foreign_keys did not get switched on.
        const QStringList statements = {
            QStringLiteral("DELETE FROM agent_task_events WHERE run_id IN (%1)").arg(ph),
            QStringLiteral("DELETE FROM agent_task_steps WHERE run_id IN (%1)").arg(ph),
            QStringLiteral("DELETE FROM agent_task_artefacts WHERE run_id IN (%1)").arg(ph),
            QStringLiteral("DELETE FROM agent_task_runs WHERE id IN (%1)").arg(ph),
        };
        for (const QString& sql : statements) {
            auto r = db().execute(sql, params);
            if (r.is_err()) {
                db().rollback();
                return Result<void>::err(r.error());
            }
        }
    }
    return db().commit();
}

Result<int> TaskRunStore::prune_runs(int keep_newest) const {
    const QVariantList params{qMax(0, keep_newest)};
    const QString victims =
        QStringLiteral("SELECT id FROM agent_task_runs ORDER BY created_at DESC LIMIT -1 OFFSET ?");
    auto tx = db().begin_transaction();
    if (tx.is_err())
        return Result<int>::err(tx.error());
    for (const char* child : {"agent_task_events", "agent_task_steps", "agent_task_artefacts"}) {
        auto r = db().execute(QStringLiteral("DELETE FROM %1 WHERE run_id IN (%2)").arg(QLatin1String(child), victims),
                              params);
        if (r.is_err()) {
            db().rollback();
            return Result<int>::err(r.error());
        }
    }
    auto r2 = db().execute(QStringLiteral("DELETE FROM agent_task_runs WHERE id IN (%1)").arg(victims), params);
    if (r2.is_err()) {
        db().rollback();
        return Result<int>::err(r2.error());
    }
    const int deleted = r2.value().numRowsAffected();
    auto c = db().commit();
    if (c.is_err())
        return Result<int>::err(c.error());
    return Result<int>::ok(qMax(0, deleted));
}

Result<int> TaskRunStore::mark_unfinished_interrupted(qint64 before_ms, qint64 now_ms, const QString& note) const {
    // The working clock stops where the run was last recorded (the old updated_at —
    // SET expressions read the row as it was), not at this restart.
    auto r = db().execute(QStringLiteral("UPDATE agent_task_runs SET status = 'interrupted', finished_at = ?, "
                                         "updated_at = ?, error = CASE WHEN error = '' THEN ? ELSE error END, "
                                         "active_ms = active_ms + CASE WHEN active_since > 0 AND updated_at > "
                                         "active_since THEN updated_at - active_since ELSE 0 END, active_since = 0 "
                                         "WHERE status IN ('queued', 'planning', 'running', 'waiting_approval') "
                                         "AND created_at < ?"),
                          {now_ms, now_ms, note, before_ms});
    if (r.is_err())
        return Result<int>::err(r.error());
    const int marked = qMax(0, r.value().numRowsAffected());
    // A step caught mid-flight runs again on resume: show it as pending, not running.
    auto s = db().execute(QStringLiteral("UPDATE agent_task_steps SET status = 'pending' WHERE status = 'running' "
                                         "AND run_id IN (SELECT id FROM agent_task_runs WHERE status = 'interrupted')"));
    if (s.is_err())
        LOG_WARN(kTaskRunStoreTag, QStringLiteral("Resetting interrupted steps failed: %1")
                                       .arg(QString::fromStdString(s.error())));
    return Result<int>::ok(marked);
}

// ── Steps ────────────────────────────────────────────────────────────────────

Result<QVector<RunStep>> TaskRunStore::load_steps(int run_limit) const {
    return query_list_as<RunStep>(
        QStringLiteral("SELECT %1 FROM agent_task_steps WHERE run_id IN (SELECT id FROM agent_task_runs ORDER BY "
                       "created_at DESC LIMIT ?) ORDER BY run_id, idx")
            .arg(QLatin1String(kStepColumns)),
        {qMax(1, run_limit)}, trs_map_step);
}

Result<void> TaskRunStore::replace_steps(const QString& run_id, const QVector<RunStep>& steps) const {
    QSqlDatabase conn = db().connection();
    if (!conn.isOpen())
        return Result<void>::err("DB connection unavailable on this thread");
    auto tx = db().begin_transaction();
    if (tx.is_err())
        return tx;
    auto del = db().execute(QStringLiteral("DELETE FROM agent_task_steps WHERE run_id = ?"), {run_id});
    if (del.is_err()) {
        db().rollback();
        return Result<void>::err(del.error());
    }
    QSqlQuery q(conn);
    if (!q.prepare(QStringLiteral("INSERT INTO agent_task_steps (%1) VALUES (%2)")
                       .arg(QLatin1String(kStepColumns), trs_placeholders(kStepColumnCount)))) {
        const std::string err = q.lastError().text().toStdString();
        db().rollback();
        return Result<void>::err("prepare failed: " + err);
    }
    for (const RunStep& s : steps) {
        const QVariantList params = trs_step_params(s);
        for (int i = 0; i < params.size(); ++i)
            q.bindValue(i, params.at(i));
        if (!q.exec()) {
            const std::string err = q.lastError().text().toStdString();
            db().rollback();
            return Result<void>::err(err);
        }
    }
    return db().commit();
}

Result<void> TaskRunStore::upsert_step(const RunStep& s) const {
    const QString sql =
        QStringLiteral("INSERT INTO agent_task_steps (%1) VALUES (%2) ON CONFLICT(run_id, idx) DO UPDATE SET "
                       "title=excluded.title, instruction=excluded.instruction, deliverable=excluded.deliverable, "
                       "status=excluded.status, summary=excluded.summary, attempts=excluded.attempts, "
                       "credits=excluded.credits, message_uuid=excluded.message_uuid, "
                       "started_at=excluded.started_at, finished_at=excluded.finished_at")
            .arg(QLatin1String(kStepColumns), trs_placeholders(kStepColumnCount));
    return exec_write(sql, trs_step_params(s));
}

// ── Events ───────────────────────────────────────────────────────────────────

Result<int> TaskRunStore::insert_events(const QVector<RunEventRow>& rows) const {
    if (rows.isEmpty())
        return Result<int>::ok(0);
    QSqlDatabase conn = db().connection();
    if (!conn.isOpen())
        return Result<int>::err("DB connection unavailable on this thread");
    auto tx = db().begin_transaction();
    if (tx.is_err())
        return Result<int>::err(tx.error());

    QSqlQuery q(conn);
    if (!q.prepare(QStringLiteral("INSERT INTO agent_task_events (run_id, %1) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)")
                       .arg(QLatin1String(kEventColumns)))) {
        const std::string err = q.lastError().text().toStdString();
        db().rollback();
        return Result<int>::err("prepare failed: " + err);
    }
    int written = 0;
    int failed = 0;
    for (const auto& row : rows) {
        const RunEvent& e = row.second;
        q.bindValue(0, row.first);
        q.bindValue(1, e.seq);
        q.bindValue(2, e.ts_ms);
        q.bindValue(3, e.step);
        q.bindValue(4, event_kind_id(e.kind));
        q.bindValue(5, trs_text(e.title));
        q.bindValue(6, trs_text(e.body));
        q.bindValue(7, e.data.isEmpty() ? QStringLiteral("") : trs_json_text(e.data));
        q.bindValue(8, e.is_error ? 1 : 0);
        q.bindValue(9, trs_text(e.ref_id));
        if (q.exec())
            ++written;
        else
            ++failed;
    }
    if (failed > 0)
        LOG_WARN(kTaskRunStoreTag, QStringLiteral("Saving run events skipped rows skipped=%1 total=%2 reason=\"run deleted or constraint\"")
                                       .arg(failed)
                                       .arg(rows.size()));
    auto c = db().commit();
    if (c.is_err()) {
        db().rollback();
        return Result<int>::err(c.error());
    }
    return Result<int>::ok(written);
}

Result<QVector<RunEvent>> TaskRunStore::load_events(const QString& run_id, int last_n) const {
    if (last_n > 0) {
        return query_list_as<RunEvent>(
            QStringLiteral("SELECT %1 FROM (SELECT %1 FROM agent_task_events WHERE run_id = ? ORDER BY seq DESC "
                           "LIMIT ?) ORDER BY seq ASC")
                .arg(QLatin1String(kEventColumns)),
            {run_id, qMin(last_n, kTaskRunMaxEventsLoaded)}, trs_map_event);
    }
    return query_list_as<RunEvent>(
        QStringLiteral("SELECT %1 FROM agent_task_events WHERE run_id = ? ORDER BY seq ASC LIMIT ?")
            .arg(QLatin1String(kEventColumns)),
        {run_id, kTaskRunMaxEventsLoaded}, trs_map_event);
}

Result<QHash<QString, int>> TaskRunStore::load_max_event_seq(int run_limit) const {
    auto r = db().execute(QStringLiteral("SELECT run_id, MAX(seq) FROM agent_task_events WHERE run_id IN (SELECT id "
                                         "FROM agent_task_runs ORDER BY created_at DESC LIMIT ?) GROUP BY run_id"),
                          {qMax(1, run_limit)});
    if (r.is_err())
        return Result<QHash<QString, int>>::err(r.error());
    QHash<QString, int> out;
    auto& q = r.value();
    while (q.next())
        out.insert(q.value(0).toString(), q.value(1).toInt());
    return Result<QHash<QString, int>>::ok(std::move(out));
}

// ── Artefacts ────────────────────────────────────────────────────────────────

Result<QVector<RunArtefact>> TaskRunStore::load_artefacts(int run_limit) const {
    // Every row: the retention policy bounds the table, and pinned artefacts of
    // deleted / pruned runs must load too (as must orphans, for the sweep).
    Q_UNUSED(run_limit)
    return query_list_as<RunArtefact>(
        QStringLiteral("SELECT %1 FROM agent_run_artefacts ORDER BY created_at ASC LIMIT ?").arg(QLatin1String(kArtefactColumns)),
        {kTaskRunMaxArtefactsLoaded}, trs_map_artefact);
}

Result<void> TaskRunStore::upsert_artefact(const RunArtefact& a) const {
    const QString sql =
        QStringLiteral("INSERT INTO agent_run_artefacts (%1) VALUES (%2) ON CONFLICT(id) DO UPDATE SET "
                       "step_idx=excluded.step_idx, kind=excluded.kind, title=excluded.title, mime=excluded.mime, "
                       "stored_name=excluded.stored_name, inline_json=excluded.inline_json, bytes=excluded.bytes, "
                       "run_title=excluded.run_title, source_json=excluded.source_json, pinned=excluded.pinned, "
                       "updated_at=excluded.updated_at")
            .arg(QLatin1String(kArtefactColumns), trs_placeholders(kArtefactColumnCount));
    return exec_write(sql, {a.id, a.run_id, a.step_idx, trs_text(a.kind), trs_text(a.title), trs_text(a.mime),
                            trs_text(a.stored_name), trs_inline_text(a.inline_json), a.bytes, a.created_at_ms,
                            trs_text(a.run_title), trs_json_text(a.source), a.pinned ? 1 : 0,
                            QDateTime::currentMSecsSinceEpoch()});
}

Result<void> TaskRunStore::delete_artefact(const QString& id) const {
    return exec_write(QStringLiteral("DELETE FROM agent_run_artefacts WHERE id = ?"), {id});
}

Result<void> TaskRunStore::delete_artefacts(const QStringList& ids) const {
    if (ids.isEmpty())
        return Result<void>::ok();
    auto tx = db().begin_transaction();
    if (tx.is_err())
        return tx;
    for (qsizetype off = 0; off < ids.size(); off += kTaskRunDeleteChunk) {
        const QStringList chunk = ids.mid(off, kTaskRunDeleteChunk);
        QVariantList params;
        params.reserve(chunk.size());
        for (const auto& id : chunk)
            params << id;
        auto r = db().execute(QStringLiteral("DELETE FROM agent_run_artefacts WHERE id IN (%1)")
                                  .arg(trs_placeholders(static_cast<int>(chunk.size()))),
                              params);
        if (r.is_err()) {
            db().rollback();
            return Result<void>::err(r.error());
        }
    }
    return db().commit();
}

// ── Templates ────────────────────────────────────────────────────────────────

Result<QVector<StoredRunTemplate>> TaskRunStore::load_templates() const {
    return query_list_as<StoredRunTemplate>(
        QStringLiteral("SELECT id, title, template_json, created_at, updated_at FROM agent_run_templates "
                       "ORDER BY title COLLATE NOCASE ASC"),
        {}, [](QSqlQuery& q) {
            StoredRunTemplate t;
            t.id = q.value(0).toString();
            t.title = q.value(1).toString();
            t.json = trs_parse_object(q.value(2).toString());
            t.created_at_ms = q.value(3).toLongLong();
            t.updated_at_ms = q.value(4).toLongLong();
            return t;
        });
}

Result<void> TaskRunStore::upsert_template(const StoredRunTemplate& t) const {
    return exec_write(QStringLiteral("INSERT INTO agent_run_templates (id, title, template_json, created_at, updated_at) "
                                     "VALUES (?, ?, ?, ?, ?) ON CONFLICT(id) DO UPDATE SET title=excluded.title, "
                                     "template_json=excluded.template_json, updated_at=excluded.updated_at"),
                      {t.id, trs_text(t.title), trs_json_text(t.json), t.created_at_ms,
                       t.updated_at_ms > 0 ? t.updated_at_ms : t.created_at_ms});
}

Result<void> TaskRunStore::delete_template(const QString& id) const {
    return exec_write(QStringLiteral("DELETE FROM agent_run_templates WHERE id = ?"), {id});
}

// ── Schedules ────────────────────────────────────────────────────────────────

Result<QVector<RunSchedule>> TaskRunStore::load_schedules() const {
    return query_list_as<RunSchedule>(QStringLiteral("SELECT %1 FROM agent_task_schedules ORDER BY created_at ASC")
                                          .arg(QLatin1String(kScheduleColumns)),
                                      {}, trs_map_schedule);
}

Result<void> TaskRunStore::upsert_schedule(const RunSchedule& s) const {
    const QString sql =
        QStringLiteral("INSERT INTO agent_task_schedules (%1) VALUES (%2) ON CONFLICT(id) DO UPDATE SET "
                       "name=excluded.name, template_id=excluded.template_id, prompt=excluded.prompt, "
                       "engine=excluded.engine, mode=excluded.mode, request_json=excluded.request_json, "
                       "max_credits=excluded.max_credits, kind=excluded.kind, "
                       "interval_minutes=excluded.interval_minutes, daily_time=excluded.daily_time, "
                       "weekdays_mask=excluded.weekdays_mask, enabled=excluded.enabled, "
                       "paused_for_review=excluded.paused_for_review, last_run_at=excluded.last_run_at, "
                       "next_run_at=excluded.next_run_at, updated_at=excluded.updated_at, "
                       "time_zone=excluded.time_zone, once_at=excluded.once_at, "
                       "catch_up_minutes=excluded.catch_up_minutes, last_run_id=excluded.last_run_id, "
                       "last_status=excluded.last_status, last_error=excluded.last_error")
            .arg(QLatin1String(kScheduleColumns), trs_placeholders(kScheduleColumnCount));
    return exec_write(sql, {s.id,
                            trs_text(s.name),
                            trs_text(s.template_id),
                            trs_text(s.prompt),
                            trs_text(s.engine),
                            trs_text(s.mode),
                            trs_json_text(run_request_to_json(s.request)),
                            s.max_credits,
                            trs_text(s.kind),
                            s.interval_minutes,
                            trs_text(s.daily_time),
                            s.weekdays_mask,
                            s.enabled ? 1 : 0,
                            s.paused_for_review ? 1 : 0,
                            trs_ms_or_null(s.last_run_at_ms),
                            trs_ms_or_null(s.next_run_at_ms),
                            s.created_at_ms,
                            s.updated_at_ms,
                            trs_text(s.time_zone),
                            qMax<qint64>(0, s.once_at_ms),
                            qMax(0, s.catch_up_minutes),
                            trs_text(s.last_run_id),
                            trs_text(s.last_status),
                            trs_text(s.last_error.left(2000))});
}

Result<void> TaskRunStore::delete_schedule(const QString& id) const {
    return exec_write(QStringLiteral("DELETE FROM agent_task_schedules WHERE id = ?"), {id});
}

} // namespace fincept::agent_tasks
