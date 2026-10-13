// CodingAgentRepository.cpp — parameterized SQL for coding_agent_* tables (v052).

#include "services/coding_agents/CodingAgentRepository.h"

#include "core/logging/Logger.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace fincept::services::coding_agents {

namespace coding_agent_repo_detail {

constexpr const char* kCodingAgentRepoTag = "CodingAgentRepo";

constexpr const char* kRunColumns =
    "id, title, agent, prompt, model, mode, workdir, workdir_auto, use_fincept_tools, max_budget_usd, status, "
    "created_at, started_at, finished_at, session_id, exit_code, cost_usd, input_tokens, output_tokens, final_text, "
    "error, parent_run_id, schedule_id, origin";

constexpr const char* kEventColumns = "seq, ts, kind, title, body, raw_json, is_error, ref_id";

constexpr const char* kScheduleColumns =
    "id, name, agent, prompt, model, mode, use_fincept_tools, max_budget_usd, kind, interval_minutes, daily_time, "
    "weekdays_mask, enabled, last_run_at, next_run_at, created_at, updated_at";

/// SQLite's historical bound-parameter limit is 999 — chunk IN (...) lists well below it.
constexpr int kDeleteChunk = 400;

QVariant ms_or_null(qint64 ms) {
    return ms > 0 ? QVariant(ms) : QVariant();
}

QVariant str_or_null(const QString& s) {
    return s.isEmpty() ? QVariant() : QVariant(s);
}

/// Bind value for a NOT NULL text column. Qt's SQLite driver binds a *null*
/// QString as SQL NULL (and `DEFAULT ''` only applies when the column is
/// omitted), so an empty-but-null field — a default model, a run without a
/// session yet, an event without a body — would fail the NOT NULL constraint.
QVariant text(const QString& s) {
    return s.isNull() ? QVariant(QStringLiteral("")) : QVariant(s);
}

CodingAgentRun map_run(QSqlQuery& q) {
    CodingAgentRun r;
    r.id = q.value(0).toString();
    r.title = q.value(1).toString();
    r.agent = agent_from_id(q.value(2).toString()).value_or(AgentKind::Claude);
    r.prompt = q.value(3).toString();
    r.model = q.value(4).toString();
    r.mode = mode_from_id(q.value(5).toString());
    r.workdir = q.value(6).toString();
    r.workdir_auto = q.value(7).toInt() != 0;
    r.use_fincept_tools = q.value(8).toInt() != 0;
    r.max_budget_usd = q.value(9).isNull() ? 0.0 : q.value(9).toDouble();
    r.status = status_from_id(q.value(10).toString());
    r.created_at_ms = q.value(11).toLongLong();
    r.started_at_ms = q.value(12).isNull() ? 0 : q.value(12).toLongLong();
    r.finished_at_ms = q.value(13).isNull() ? 0 : q.value(13).toLongLong();
    r.session_id = q.value(14).toString();
    if (!q.value(15).isNull())
        r.exit_code = q.value(15).toInt();
    if (!q.value(16).isNull())
        r.cost_usd = q.value(16).toDouble();
    r.input_tokens = q.value(17).toLongLong();
    r.output_tokens = q.value(18).toLongLong();
    r.final_text = q.value(19).toString();
    r.error = q.value(20).toString();
    r.parent_run_id = q.value(21).toString();
    r.schedule_id = q.value(22).toString();
    r.origin = q.value(23).toString();
    return r;
}

AgentEvent map_event(QSqlQuery& q) {
    AgentEvent e;
    e.seq = q.value(0).toInt();
    e.ts_ms = q.value(1).toLongLong();
    e.kind = event_kind_from_id(q.value(2).toString());
    e.title = q.value(3).toString();
    e.body = q.value(4).toString();
    e.raw_json = q.value(5).toString();
    e.is_error = q.value(6).toInt() != 0;
    e.ref_id = q.value(7).toString();
    return e;
}

CodingAgentSchedule map_schedule(QSqlQuery& q) {
    CodingAgentSchedule s;
    s.id = q.value(0).toString();
    s.name = q.value(1).toString();
    s.agent = agent_from_id(q.value(2).toString()).value_or(AgentKind::Claude);
    s.prompt = q.value(3).toString();
    s.model = q.value(4).toString();
    s.mode = mode_from_id(q.value(5).toString());
    s.use_fincept_tools = q.value(6).toInt() != 0;
    s.max_budget_usd = q.value(7).isNull() ? 0.0 : q.value(7).toDouble();
    s.kind = schedule_kind_from_id(q.value(8).toString());
    s.interval_minutes = q.value(9).toInt();
    s.daily_time = q.value(10).toString();
    s.weekdays_mask = q.value(11).toInt();
    s.enabled = q.value(12).toInt() != 0;
    s.last_run_at_ms = q.value(13).isNull() ? 0 : q.value(13).toLongLong();
    s.next_run_at_ms = q.value(14).isNull() ? 0 : q.value(14).toLongLong();
    s.created_at_ms = q.value(15).toLongLong();
    s.updated_at_ms = q.value(16).toLongLong();
    return s;
}

QString placeholders(int n) {
    QStringList p;
    p.reserve(n);
    for (int i = 0; i < n; ++i)
        p << QStringLiteral("?");
    return p.join(QStringLiteral(", "));
}

} // namespace coding_agent_repo_detail

using namespace coding_agent_repo_detail;

CodingAgentRepository& CodingAgentRepository::instance() {
    static CodingAgentRepository inst;
    return inst;
}

Result<QVector<CodingAgentRun>> CodingAgentRepository::load_runs(int limit) const {
    return query_list(QStringLiteral("SELECT %1 FROM coding_agent_runs ORDER BY created_at DESC LIMIT ?")
                          .arg(QLatin1String(kRunColumns)),
                      {qMax(1, limit)}, map_run);
}

Result<void> CodingAgentRepository::upsert_run(const CodingAgentRun& r) const {
    const QString sql =
        QStringLiteral("INSERT INTO coding_agent_runs (%1) VALUES (%2) "
                       "ON CONFLICT(id) DO UPDATE SET title=excluded.title, agent=excluded.agent, "
                       "prompt=excluded.prompt, model=excluded.model, mode=excluded.mode, workdir=excluded.workdir, "
                       "workdir_auto=excluded.workdir_auto, use_fincept_tools=excluded.use_fincept_tools, "
                       "max_budget_usd=excluded.max_budget_usd, status=excluded.status, "
                       "started_at=excluded.started_at, finished_at=excluded.finished_at, "
                       "session_id=excluded.session_id, exit_code=excluded.exit_code, cost_usd=excluded.cost_usd, "
                       "input_tokens=excluded.input_tokens, output_tokens=excluded.output_tokens, "
                       "final_text=excluded.final_text, error=excluded.error, parent_run_id=excluded.parent_run_id, "
                       "schedule_id=excluded.schedule_id, origin=excluded.origin")
            .arg(QLatin1String(kRunColumns), placeholders(24));
    return exec_write(sql, {r.id,
                            text(r.title),
                            agent_id(r.agent),
                            text(r.prompt),
                            text(r.model),
                            mode_id(r.mode),
                            text(r.workdir),
                            r.workdir_auto ? 1 : 0,
                            r.use_fincept_tools ? 1 : 0,
                            r.max_budget_usd > 0.0 ? QVariant(r.max_budget_usd) : QVariant(),
                            status_id(r.status),
                            r.created_at_ms,
                            ms_or_null(r.started_at_ms),
                            ms_or_null(r.finished_at_ms),
                            text(r.session_id),
                            r.exit_code.has_value() ? QVariant(*r.exit_code) : QVariant(),
                            r.cost_usd.has_value() ? QVariant(*r.cost_usd) : QVariant(),
                            r.input_tokens,
                            r.output_tokens,
                            text(r.final_text),
                            text(r.error),
                            str_or_null(r.parent_run_id),
                            str_or_null(r.schedule_id),
                            r.origin.isEmpty() ? QStringLiteral("user") : r.origin});
}

Result<void> CodingAgentRepository::delete_runs(const QStringList& ids) const {
    if (ids.isEmpty())
        return Result<void>::ok();
    auto tx = db().begin_transaction();
    if (tx.is_err())
        return tx;
    for (qsizetype off = 0; off < ids.size(); off += kDeleteChunk) {
        const QStringList chunk = ids.mid(off, kDeleteChunk);
        QVariantList params;
        params.reserve(chunk.size());
        for (const auto& id : chunk)
            params << id;
        const QString ph = placeholders(static_cast<int>(chunk.size()));
        // Explicit event delete as well as the FK cascade: correct even on a
        // connection where foreign_keys did not get switched on.
        auto r1 = db().execute(QStringLiteral("DELETE FROM coding_agent_events WHERE run_id IN (%1)").arg(ph), params);
        auto r2 = db().execute(QStringLiteral("DELETE FROM coding_agent_runs WHERE id IN (%1)").arg(ph), params);
        if (r1.is_err() || r2.is_err()) {
            db().rollback();
            return Result<void>::err(r1.is_err() ? r1.error() : r2.error());
        }
    }
    return db().commit();
}

Result<int> CodingAgentRepository::prune_runs(int keep_newest) const {
    const QVariantList params{qMax(0, keep_newest)};
    auto tx = db().begin_transaction();
    if (tx.is_err())
        return Result<int>::err(tx.error());
    auto r1 = db().execute(QStringLiteral("DELETE FROM coding_agent_events WHERE run_id IN (SELECT id FROM "
                                          "coding_agent_runs ORDER BY created_at DESC LIMIT -1 OFFSET ?)"),
                           params);
    if (r1.is_err()) {
        db().rollback();
        return Result<int>::err(r1.error());
    }
    auto r2 = db().execute(QStringLiteral("DELETE FROM coding_agent_runs WHERE id IN (SELECT id FROM coding_agent_runs "
                                          "ORDER BY created_at DESC LIMIT -1 OFFSET ?)"),
                           params);
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

Result<int> CodingAgentRepository::mark_unfinished_interrupted(qint64 before_ms, qint64 now_ms,
                                                               const QString& note) const {
    auto r = db().execute(QStringLiteral("UPDATE coding_agent_runs SET status = 'interrupted', finished_at = ?, "
                                         "error = CASE WHEN error = '' THEN ? ELSE error END "
                                         "WHERE status IN ('queued', 'running') AND created_at < ?"),
                          {now_ms, note, before_ms});
    if (r.is_err())
        return Result<int>::err(r.error());
    return Result<int>::ok(qMax(0, r.value().numRowsAffected()));
}

Result<int> CodingAgentRepository::insert_events(const QVector<RunEventRow>& rows) const {
    if (rows.isEmpty())
        return Result<int>::ok(0);
    QSqlDatabase conn = db().connection();
    if (!conn.isOpen())
        return Result<int>::err("DB connection unavailable on this thread");
    auto tx = db().begin_transaction();
    if (tx.is_err())
        return Result<int>::err(tx.error());

    QSqlQuery q(conn);
    if (!q.prepare(QStringLiteral("INSERT INTO coding_agent_events (run_id, %1) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)")
                       .arg(QLatin1String(kEventColumns)))) {
        const std::string err = q.lastError().text().toStdString();
        db().rollback();
        return Result<int>::err("prepare failed: " + err);
    }
    int written = 0;
    int failed = 0;
    for (const auto& row : rows) {
        const AgentEvent& e = row.second;
        q.bindValue(0, row.first);
        q.bindValue(1, e.seq);
        q.bindValue(2, e.ts_ms);
        q.bindValue(3, event_kind_id(e.kind));
        q.bindValue(4, text(e.title));
        q.bindValue(5, text(e.body));
        q.bindValue(6, text(e.raw_json));
        q.bindValue(7, e.is_error ? 1 : 0);
        q.bindValue(8, text(e.ref_id));
        if (q.exec())
            ++written;
        else
            ++failed;
    }
    if (failed > 0)
        LOG_WARN(kCodingAgentRepoTag, QStringLiteral("insert_events: %1 of %2 rows skipped (run deleted or constraint)")
                                          .arg(failed)
                                          .arg(rows.size()));
    auto c = db().commit();
    if (c.is_err()) {
        db().rollback();
        return Result<int>::err(c.error());
    }
    return Result<int>::ok(written);
}

Result<QVector<AgentEvent>> CodingAgentRepository::load_events(const QString& run_id, int last_n) const {
    constexpr int kMaxEventsLoaded = 20000;
    if (last_n > 0) {
        return query_list_as<AgentEvent>(
            QStringLiteral("SELECT %1 FROM (SELECT %1 FROM coding_agent_events WHERE run_id = ? ORDER BY seq DESC "
                           "LIMIT ?) ORDER BY seq ASC")
                .arg(QLatin1String(kEventColumns)),
            {run_id, qMin(last_n, kMaxEventsLoaded)}, map_event);
    }
    return query_list_as<AgentEvent>(
        QStringLiteral("SELECT %1 FROM coding_agent_events WHERE run_id = ? ORDER BY seq ASC LIMIT ?")
            .arg(QLatin1String(kEventColumns)),
        {run_id, kMaxEventsLoaded}, map_event);
}

Result<QVector<CodingAgentSchedule>> CodingAgentRepository::load_schedules() const {
    return query_list_as<CodingAgentSchedule>(
        QStringLiteral("SELECT %1 FROM coding_agent_schedules ORDER BY created_at ASC")
            .arg(QLatin1String(kScheduleColumns)),
        {}, map_schedule);
}

Result<void> CodingAgentRepository::upsert_schedule(const CodingAgentSchedule& s) const {
    const QString sql =
        QStringLiteral("INSERT INTO coding_agent_schedules (%1) VALUES (%2) "
                       "ON CONFLICT(id) DO UPDATE SET name=excluded.name, agent=excluded.agent, "
                       "prompt=excluded.prompt, model=excluded.model, mode=excluded.mode, "
                       "use_fincept_tools=excluded.use_fincept_tools, max_budget_usd=excluded.max_budget_usd, "
                       "kind=excluded.kind, interval_minutes=excluded.interval_minutes, "
                       "daily_time=excluded.daily_time, weekdays_mask=excluded.weekdays_mask, "
                       "enabled=excluded.enabled, last_run_at=excluded.last_run_at, "
                       "next_run_at=excluded.next_run_at, updated_at=excluded.updated_at")
            .arg(QLatin1String(kScheduleColumns), placeholders(17));
    return exec_write(sql, {s.id,
                            text(s.name),
                            agent_id(s.agent),
                            text(s.prompt),
                            text(s.model),
                            mode_id(s.mode),
                            s.use_fincept_tools ? 1 : 0,
                            s.max_budget_usd > 0.0 ? QVariant(s.max_budget_usd) : QVariant(),
                            schedule_kind_id(s.kind),
                            s.interval_minutes,
                            text(s.daily_time),
                            s.weekdays_mask,
                            s.enabled ? 1 : 0,
                            ms_or_null(s.last_run_at_ms),
                            ms_or_null(s.next_run_at_ms),
                            s.created_at_ms,
                            s.updated_at_ms});
}

Result<void> CodingAgentRepository::delete_schedule(const QString& id) const {
    return exec_write(QStringLiteral("DELETE FROM coding_agent_schedules WHERE id = ?"), {id});
}

} // namespace fincept::services::coding_agents
