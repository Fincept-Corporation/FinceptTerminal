// ArenaStore — all SQLite access for the Alpha Arena rewrite (arena_* tables).
// Schema: storage/sqlite/migrations/v050_alpha_arena_rewrite.cpp

#include "services/alpha_arena/ArenaStore.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSqlQuery>
#include <QUuid>

#include <algorithm>

namespace fincept::arena {

namespace {

QString make_id() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }
qint64 now_millis() { return QDateTime::currentMSecsSinceEpoch(); }

// Qt binds a null QString as SQL NULL, which trips the NOT NULL constraints on
// the arena_* columns. Coerce null → empty string before binding.
QString nn(const QString& v) { return v.isNull() ? QString::fromLatin1("") : v; }

QString instruments_to_json(const QStringList& instruments) {
    return QString::fromUtf8(
        QJsonDocument(QJsonArray::fromStringList(instruments)).toJson(QJsonDocument::Compact));
}

QStringList instruments_from_json(const QString& json) {
    QStringList out;
    const auto doc = QJsonDocument::fromJson(json.toUtf8());
    for (const auto& v : doc.array())
        out << v.toString();
    return out;
}

// Row mappers — column indices match the SELECT column order used below.
CompetitionRow map_competition(QSqlQuery& q) {
    CompetitionRow c;
    c.id = q.value(0).toString();
    c.name = q.value(1).toString();
    c.status = q.value(2).toString();
    c.venue = q.value(3).toString();
    c.instruments = instruments_from_json(q.value(4).toString());
    c.capital_per_agent = q.value(5).toDouble();
    c.cadence_seconds = q.value(6).toInt();
    c.max_leverage = q.value(7).toDouble();
    c.system_prompt_override = q.value(8).toString();
    c.live_mode = q.value(9).toInt() != 0;
    c.created_at = q.value(10).toLongLong();
    c.started_at = q.value(11).toLongLong();
    c.ended_at = q.value(12).toLongLong();
    return c;
}
constexpr const char* kCompetitionCols =
    "id, name, status, venue, instruments_json, capital_per_agent, cadence_seconds,"
    " max_leverage, system_prompt_override, live_mode, created_at, started_at, ended_at";

AgentRow map_agent(QSqlQuery& q) {
    AgentRow a;
    a.id = q.value(0).toString();
    a.competition_id = q.value(1).toString();
    a.provider = q.value(2).toString();
    a.model_id = q.value(3).toString();
    a.display_name = q.value(4).toString();
    a.color_hex = q.value(5).toString();
    a.source_kind = q.value(6).toString();
    a.source_ref = q.value(7).toString();
    a.status = q.value(8).toString();
    a.halt_reason = q.value(9).toString();
    a.consecutive_failures = q.value(10).toInt();
    return a;
}
constexpr const char* kAgentCols =
    "id, competition_id, provider, model_id, display_name, color_hex, source_kind,"
    " source_ref, status, halt_reason, consecutive_failures";

DecisionRow map_decision(QSqlQuery& q) {
    DecisionRow d;
    d.id = q.value(0).toString();
    d.competition_id = q.value(1).toString();
    d.agent_id = q.value(2).toString();
    d.round_seq = q.value(3).toInt();
    d.system_prompt = q.value(4).toString();
    d.user_prompt = q.value(5).toString();
    d.raw_response = q.value(6).toString();
    d.actions_json = q.value(7).toString();
    d.parse_error = q.value(8).toString();
    d.status = q.value(9).toString();
    d.latency_ms = q.value(10).toLongLong();
    d.prompt_tokens = q.value(11).toInt();
    d.completion_tokens = q.value(12).toInt();
    d.ts = q.value(13).toLongLong();
    return d;
}
constexpr const char* kDecisionCols =
    "id, competition_id, agent_id, round_seq, system_prompt, user_prompt, raw_response,"
    " actions_json, parse_error, status, latency_ms, prompt_tokens, completion_tokens, ts";

OrderRow map_order(QSqlQuery& q) {
    OrderRow o;
    o.id = q.value(0).toString();
    o.competition_id = q.value(1).toString();
    o.agent_id = q.value(2).toString();
    o.round_seq = q.value(3).toInt();
    o.coin = q.value(4).toString();
    o.side = q.value(5).toString();
    o.action = q.value(6).toString();
    o.qty = q.value(7).toDouble();
    o.price = q.value(8).toDouble();
    o.notional_usd = q.value(9).toDouble();
    o.leverage = q.value(10).toDouble();
    o.fee = q.value(11).toDouble();
    o.realized_pnl = q.value(12).toDouble();
    o.status = q.value(13).toString();
    o.reject_reason = q.value(14).toString();
    o.ts = q.value(15).toLongLong();
    return o;
}
constexpr const char* kOrderCols =
    "id, competition_id, agent_id, round_seq, coin, side, action, qty, price,"
    " notional_usd, leverage, fee, realized_pnl, status, reject_reason, ts";

PositionRow map_position(QSqlQuery& q) {
    PositionRow p;
    p.competition_id = q.value(0).toString();
    p.agent_id = q.value(1).toString();
    p.coin = q.value(2).toString();
    p.side = q.value(3).toString();
    p.qty = q.value(4).toDouble();
    p.entry_price = q.value(5).toDouble();
    p.leverage = q.value(6).toDouble();
    p.funding_accrued = q.value(7).toDouble();
    return p;
}
constexpr const char* kPositionCols =
    "competition_id, agent_id, coin, side, qty, entry_price, leverage, funding_accrued";

EquitySnapshotRow map_equity(QSqlQuery& q) {
    EquitySnapshotRow s;
    s.competition_id = q.value(0).toString();
    s.agent_id = q.value(1).toString();
    s.round_seq = q.value(2).toInt();
    s.ts = q.value(3).toLongLong();
    s.equity = q.value(4).toDouble();
    s.balance = q.value(5).toDouble();
    s.unrealized_pnl = q.value(6).toDouble();
    return s;
}
constexpr const char* kEquityCols =
    "competition_id, agent_id, round_seq, ts, equity, balance, unrealized_pnl";

HitlPendingRow map_hitl(QSqlQuery& q) {
    HitlPendingRow h;
    h.id = q.value(0).toString();
    h.competition_id = q.value(1).toString();
    h.agent_id = q.value(2).toString();
    h.round_seq = q.value(3).toInt();
    h.kind = q.value(4).toString();
    h.coin = q.value(5).toString();
    h.side = q.value(6).toString();
    h.size_usd = q.value(7).toDouble();
    h.leverage = q.value(8).toDouble();
    h.reason = q.value(9).toString();
    h.ts = q.value(10).toLongLong();
    return h;
}
constexpr const char* kHitlCols =
    "id, competition_id, agent_id, round_seq, kind, coin, side, size_usd, leverage, reason, ts";

// arena_hitl_pending postdates migration v050 — create it lazily (same pattern
// as workflow_audit_log) so existing profiles pick it up without a migration.
Result<void> ensure_hitl_table() {
    static bool ready = false;
    if (ready) return Result<void>::ok();
    auto r = Database::instance().exec(
        "CREATE TABLE IF NOT EXISTS arena_hitl_pending ("
        "  id TEXT PRIMARY KEY, competition_id TEXT NOT NULL, agent_id TEXT NOT NULL,"
        "  round_seq INTEGER NOT NULL DEFAULT 0, kind TEXT NOT NULL DEFAULT 'open',"
        "  coin TEXT NOT NULL DEFAULT '', side TEXT NOT NULL DEFAULT '',"
        "  size_usd REAL NOT NULL DEFAULT 0, leverage REAL NOT NULL DEFAULT 1,"
        "  reason TEXT NOT NULL DEFAULT '', ts INTEGER NOT NULL DEFAULT 0)");
    if (r.is_ok()) ready = true;
    return r;
}

} // namespace

ArenaStore& ArenaStore::instance() {
    static ArenaStore s;
    return s;
}

// ── Competitions ─────────────────────────────────────────────────────────────

Result<QString> ArenaStore::insert_competition(const ArenaConfig& cfg) {
    const QString id = make_id();
    auto r = db().execute(
        "INSERT INTO arena_competitions (id, name, status, venue, instruments_json,"
        " capital_per_agent, cadence_seconds, max_leverage, system_prompt_override,"
        " live_mode, created_at) VALUES (?,?,?,?,?,?,?,?,?,?,?)",
        {id, nn(cfg.name), QStringLiteral("created"), nn(cfg.venue),
         instruments_to_json(cfg.instruments), cfg.capital_per_agent, cfg.cadence_seconds,
         cfg.max_leverage, nn(cfg.system_prompt_override), cfg.live_mode ? 1 : 0, now_millis()});
    if (r.is_err())
        return Result<QString>::err(r.error());
    for (const auto& spec : cfg.agents) {
        auto ar = insert_agent(id, spec);
        if (ar.is_err())
            return Result<QString>::err(ar.error());
        auto acc = upsert_account(id, ar.value(), cfg.capital_per_agent);
        if (acc.is_err())
            return Result<QString>::err(acc.error());
    }
    return Result<QString>::ok(id);
}

Result<CompetitionRow> ArenaStore::competition(const QString& id) {
    return query_one(
        QStringLiteral("SELECT %1 FROM arena_competitions WHERE id = ?").arg(kCompetitionCols),
        {id}, map_competition);
}

Result<QVector<CompetitionRow>> ArenaStore::list_competitions() {
    return query_list(
        QStringLiteral("SELECT %1 FROM arena_competitions ORDER BY created_at DESC")
            .arg(kCompetitionCols),
        {}, map_competition);
}

Result<void> ArenaStore::set_competition_status(const QString& id, const QString& status) {
    return exec_write("UPDATE arena_competitions SET status = ? WHERE id = ?", {nn(status), id});
}

Result<void> ArenaStore::set_competition_timestamps(const QString& id, qint64 started_at,
                                                    qint64 ended_at) {
    return exec_write("UPDATE arena_competitions SET started_at = ?, ended_at = ? WHERE id = ?",
                      {started_at, ended_at, id});
}

Result<void> ArenaStore::set_cadence(const QString& id, int seconds) {
    return exec_write("UPDATE arena_competitions SET cadence_seconds = ? WHERE id = ?",
                      {seconds, id});
}

Result<void> ArenaStore::delete_competition_cascade(const QString& id) {
    if (auto hr = ensure_hitl_table(); hr.is_err())
        return hr;
    for (const char* table : {"arena_agents", "arena_rounds", "arena_decisions", "arena_orders",
                              "arena_positions", "arena_accounts", "arena_equity_snapshots",
                              "arena_events", "arena_hitl_pending"}) {
        auto r = exec_write(
            QStringLiteral("DELETE FROM %1 WHERE competition_id = ?").arg(QLatin1String(table)),
            {id});
        if (r.is_err())
            return r;
    }
    return exec_write("DELETE FROM arena_competitions WHERE id = ?", {id});
}

Result<QStringList> ArenaStore::competitions_with_status(const QString& status) {
    auto r = db().execute("SELECT id FROM arena_competitions WHERE status = ?"
                          " ORDER BY created_at DESC",
                          {status});
    if (r.is_err())
        return Result<QStringList>::err(r.error());
    QStringList out;
    auto& q = r.value();
    while (q.next())
        out << q.value(0).toString();
    return Result<QStringList>::ok(out);
}

// ── Agents ───────────────────────────────────────────────────────────────────

Result<QString> ArenaStore::insert_agent(const QString& competition_id,
                                         const ArenaAgentSpec& spec) {
    const QString id = make_id();
    auto r = db().execute(
        "INSERT INTO arena_agents (id, competition_id, provider, model_id, display_name,"
        " color_hex, source_kind, source_ref, status) VALUES (?,?,?,?,?,?,?,?,'active')",
        {id, competition_id, nn(spec.provider), nn(spec.model_id), nn(spec.display_name),
         nn(spec.color_hex), nn(spec.source_kind), nn(spec.source_ref)});
    if (r.is_err())
        return Result<QString>::err(r.error());
    return Result<QString>::ok(id);
}

Result<QVector<AgentRow>> ArenaStore::agents_for(const QString& competition_id) {
    return query_list_as<AgentRow>(
        QStringLiteral("SELECT %1 FROM arena_agents WHERE competition_id = ? ORDER BY rowid")
            .arg(kAgentCols),
        {competition_id}, map_agent);
}

Result<void> ArenaStore::set_agent_status(const QString& agent_id, const QString& status,
                                          const QString& reason) {
    return exec_write("UPDATE arena_agents SET status = ?, halt_reason = ? WHERE id = ?",
                      {nn(status), nn(reason), agent_id});
}

Result<void> ArenaStore::set_agent_failures(const QString& agent_id, int count) {
    return exec_write("UPDATE arena_agents SET consecutive_failures = ? WHERE id = ?",
                      {count, agent_id});
}

// ── Rounds / decisions / orders ──────────────────────────────────────────────

Result<void> ArenaStore::insert_round(const QString& competition_id, int seq, qint64 started_at,
                                      const QString& snapshot_json) {
    return exec_write(
        "INSERT INTO arena_rounds (competition_id, seq, started_at, market_snapshot_json,"
        " status) VALUES (?,?,?,?,'running')",
        {competition_id, seq, started_at, nn(snapshot_json)});
}

Result<void> ArenaStore::complete_round(const QString& competition_id, int seq,
                                        qint64 completed_at, const QString& status) {
    return exec_write(
        "UPDATE arena_rounds SET completed_at = ?, status = ? WHERE competition_id = ? AND seq = ?",
        {completed_at, nn(status), competition_id, seq});
}

Result<int> ArenaStore::last_round_seq(const QString& competition_id) {
    auto r = db().execute("SELECT COALESCE(MAX(seq), 0) FROM arena_rounds WHERE competition_id = ?",
                          {competition_id});
    if (r.is_err())
        return Result<int>::err(r.error());
    auto& q = r.value();
    if (!q.next())
        return Result<int>::ok(0);
    return Result<int>::ok(q.value(0).toInt());
}

Result<QString> ArenaStore::insert_decision(DecisionRow d) {
    d.id = make_id();
    d.ts = now_millis();
    auto r = db().execute(
        "INSERT INTO arena_decisions (id, competition_id, agent_id, round_seq, system_prompt,"
        " user_prompt, raw_response, actions_json, parse_error, status, latency_ms,"
        " prompt_tokens, completion_tokens, ts) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
        {d.id, d.competition_id, d.agent_id, d.round_seq, nn(d.system_prompt), nn(d.user_prompt),
         nn(d.raw_response), nn(d.actions_json), nn(d.parse_error), nn(d.status), d.latency_ms,
         d.prompt_tokens, d.completion_tokens, d.ts});
    if (r.is_err())
        return Result<QString>::err(r.error());
    return Result<QString>::ok(d.id);
}

Result<QVector<DecisionRow>> ArenaStore::recent_decisions(const QString& competition_id,
                                                          const QString& agent_id, int limit) {
    auto r = query_list_as<DecisionRow>(
        QStringLiteral("SELECT %1 FROM arena_decisions WHERE competition_id = ? AND agent_id = ?"
                       " ORDER BY round_seq DESC LIMIT ?")
            .arg(kDecisionCols),
        {competition_id, agent_id, limit}, map_decision);
    if (r.is_err())
        return r;
    auto rows = r.value();
    std::reverse(rows.begin(), rows.end());  // oldest-first
    return Result<QVector<DecisionRow>>::ok(std::move(rows));
}

Result<QVector<DecisionRow>> ArenaStore::latest_decisions(const QString& competition_id,
                                                          const QString& agent_id, int limit) {
    QString sql = QStringLiteral("SELECT %1 FROM arena_decisions WHERE competition_id = ?")
                      .arg(kDecisionCols);
    QVariantList params{competition_id};
    if (!agent_id.isEmpty()) {
        sql += QStringLiteral(" AND agent_id = ?");
        params << agent_id;
    }
    sql += QStringLiteral(" ORDER BY ts DESC LIMIT ?");
    params << limit;
    return query_list_as<DecisionRow>(sql, params, map_decision);
}

Result<QHash<QString, qint64>> ArenaStore::token_totals(const QString& competition_id) {
    auto r = db().execute(
        "SELECT agent_id, SUM(prompt_tokens + completion_tokens) FROM arena_decisions"
        " WHERE competition_id = ? GROUP BY agent_id",
        {competition_id});
    if (r.is_err())
        return Result<QHash<QString, qint64>>::err(r.error());
    QHash<QString, qint64> out;
    auto& q = r.value();
    while (q.next())
        out[q.value(0).toString()] = q.value(1).toLongLong();
    return Result<QHash<QString, qint64>>::ok(out);
}

Result<QVector<DecisionRow>> ArenaStore::decisions_for_round(const QString& competition_id,
                                                             int seq) {
    return query_list_as<DecisionRow>(
        QStringLiteral("SELECT %1 FROM arena_decisions WHERE competition_id = ? AND round_seq = ?"
                       " ORDER BY ts ASC")
            .arg(kDecisionCols),
        {competition_id, seq}, map_decision);
}

Result<QString> ArenaStore::insert_order(OrderRow o) {
    o.id = make_id();
    o.ts = now_millis();
    auto r = db().execute(
        "INSERT INTO arena_orders (id, competition_id, agent_id, round_seq, coin, side, action,"
        " qty, price, notional_usd, leverage, fee, realized_pnl, status, reject_reason, ts)"
        " VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
        {o.id, o.competition_id, o.agent_id, o.round_seq, nn(o.coin), nn(o.side), nn(o.action), o.qty,
         o.price, o.notional_usd, o.leverage, o.fee, o.realized_pnl, nn(o.status),
         nn(o.reject_reason), o.ts});
    if (r.is_err())
        return Result<QString>::err(r.error());
    return Result<QString>::ok(o.id);
}

Result<QVector<OrderRow>> ArenaStore::orders_for(const QString& competition_id,
                                                 const QString& agent_id, int limit) {
    if (agent_id.isEmpty())
        return query_list_as<OrderRow>(
            QStringLiteral("SELECT %1 FROM arena_orders WHERE competition_id = ?"
                           " ORDER BY ts DESC LIMIT ?")
                .arg(kOrderCols),
            {competition_id, limit}, map_order);
    return query_list_as<OrderRow>(
        QStringLiteral("SELECT %1 FROM arena_orders WHERE competition_id = ? AND agent_id = ?"
                       " ORDER BY ts DESC LIMIT ?")
            .arg(kOrderCols),
        {competition_id, agent_id, limit}, map_order);
}

// ── Positions / accounts / equity ────────────────────────────────────────────

Result<void> ArenaStore::upsert_position(const PositionRow& p) {
    return exec_write(
        "INSERT INTO arena_positions (competition_id, agent_id, coin, side, qty, entry_price,"
        " leverage, funding_accrued) VALUES (?,?,?,?,?,?,?,?)"
        " ON CONFLICT(competition_id, agent_id, coin) DO UPDATE SET side=excluded.side,"
        " qty=excluded.qty, entry_price=excluded.entry_price, leverage=excluded.leverage,"
        " funding_accrued=excluded.funding_accrued",
        {p.competition_id, p.agent_id, nn(p.coin), nn(p.side), p.qty, p.entry_price, p.leverage,
         p.funding_accrued});
}

Result<void> ArenaStore::delete_position(const QString& competition_id, const QString& agent_id,
                                         const QString& coin) {
    return exec_write(
        "DELETE FROM arena_positions WHERE competition_id = ? AND agent_id = ? AND coin = ?",
        {competition_id, agent_id, coin});
}

Result<QVector<PositionRow>> ArenaStore::positions_for(const QString& competition_id,
                                                       const QString& agent_id) {
    if (agent_id.isEmpty())
        return query_list_as<PositionRow>(
            QStringLiteral("SELECT %1 FROM arena_positions WHERE competition_id = ?"
                           " ORDER BY agent_id, coin")
                .arg(kPositionCols),
            {competition_id}, map_position);
    return query_list_as<PositionRow>(
        QStringLiteral("SELECT %1 FROM arena_positions WHERE competition_id = ? AND agent_id = ?"
                       " ORDER BY coin")
            .arg(kPositionCols),
        {competition_id, agent_id}, map_position);
}

Result<void> ArenaStore::upsert_account(const QString& competition_id, const QString& agent_id,
                                        double balance) {
    return exec_write("INSERT INTO arena_accounts (competition_id, agent_id, balance)"
                      " VALUES (?,?,?) ON CONFLICT(competition_id, agent_id)"
                      " DO UPDATE SET balance=excluded.balance",
                      {nn(competition_id), nn(agent_id), balance});
}

Result<double> ArenaStore::account_balance(const QString& competition_id,
                                           const QString& agent_id) {
    auto r = db().execute(
        "SELECT balance FROM arena_accounts WHERE competition_id = ? AND agent_id = ?",
        {competition_id, agent_id});
    if (r.is_err())
        return Result<double>::err(r.error());
    auto& q = r.value();
    if (!q.next())
        return Result<double>::err("Not found");
    return Result<double>::ok(q.value(0).toDouble());
}

Result<void> ArenaStore::insert_equity_snapshot(const EquitySnapshotRow& s) {
    // Upsert on the (competition_id, agent_id, round_seq) PK so re-snapshotting a
    // round (e.g. retry after a partial failure) overwrites instead of erroring.
    return exec_write(
        "INSERT INTO arena_equity_snapshots (competition_id, agent_id, round_seq, ts, equity,"
        " balance, unrealized_pnl) VALUES (?,?,?,?,?,?,?)"
        " ON CONFLICT(competition_id, agent_id, round_seq) DO UPDATE SET ts=excluded.ts,"
        " equity=excluded.equity, balance=excluded.balance,"
        " unrealized_pnl=excluded.unrealized_pnl",
        {s.competition_id, s.agent_id, s.round_seq, s.ts, s.equity, s.balance, s.unrealized_pnl});
}

Result<QVector<EquitySnapshotRow>> ArenaStore::equity_series(const QString& competition_id) {
    return query_list_as<EquitySnapshotRow>(
        QStringLiteral("SELECT %1 FROM arena_equity_snapshots WHERE competition_id = ?"
                       " ORDER BY round_seq ASC, agent_id")
            .arg(kEquityCols),
        {competition_id}, map_equity);
}

// ── Events ───────────────────────────────────────────────────────────────────

Result<void> ArenaStore::insert_event(const QString& competition_id, const QString& agent_id,
                                      const QString& type, const QString& payload_json) {
    return exec_write("INSERT INTO arena_events (competition_id, agent_id, type, payload_json,"
                      " ts) VALUES (?,?,?,?,?)",
                      {nn(competition_id), nn(agent_id), nn(type), nn(payload_json), now_millis()});
}

Result<QVector<QString>> ArenaStore::recent_events(const QString& competition_id, int limit) {
    auto r = db().execute("SELECT ts, agent_id, type, payload_json FROM arena_events"
                          " WHERE competition_id = ? ORDER BY seq DESC LIMIT ?",
                          {competition_id, limit});
    if (r.is_err())
        return Result<QVector<QString>>::err(r.error());
    QVector<QString> out;
    auto& q = r.value();
    while (q.next())
        out << QStringLiteral("%1|%2|%3|%4")
                   .arg(q.value(0).toLongLong())
                   .arg(q.value(1).toString(), q.value(2).toString(), q.value(3).toString());
    std::reverse(out.begin(), out.end());  // oldest-first
    return Result<QVector<QString>>::ok(std::move(out));
}

// ── HITL pending approvals ───────────────────────────────────────────────────

Result<void> ArenaStore::insert_hitl_pending(HitlPendingRow h) {
    if (auto r = ensure_hitl_table(); r.is_err())
        return r;
    h.ts = now_millis();
    return exec_write(
        "INSERT OR REPLACE INTO arena_hitl_pending (id, competition_id, agent_id, round_seq,"
        " kind, coin, side, size_usd, leverage, reason, ts) VALUES (?,?,?,?,?,?,?,?,?,?,?)",
        {h.id, h.competition_id, h.agent_id, h.round_seq, nn(h.kind), nn(h.coin), nn(h.side),
         h.size_usd, h.leverage, nn(h.reason), h.ts});
}

Result<void> ArenaStore::delete_hitl_pending(const QString& id) {
    if (auto r = ensure_hitl_table(); r.is_err())
        return r;
    return exec_write("DELETE FROM arena_hitl_pending WHERE id = ?", {id});
}

Result<QVector<HitlPendingRow>> ArenaStore::hitl_pending_for(const QString& competition_id) {
    if (auto r = ensure_hitl_table(); r.is_err())
        return Result<QVector<HitlPendingRow>>::err(r.error());
    return query_list_as<HitlPendingRow>(
        QStringLiteral("SELECT %1 FROM arena_hitl_pending WHERE competition_id = ? ORDER BY ts ASC")
            .arg(kHitlCols),
        {competition_id}, map_hitl);
}

} // namespace fincept::arena
