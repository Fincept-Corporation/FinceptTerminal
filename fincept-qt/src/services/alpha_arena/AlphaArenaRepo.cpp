#include "services/alpha_arena/AlphaArenaRepo.h"

#include "core/logging/Logger.h"
#include "storage/sqlite/Database.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QSqlQuery>
#include <QUuid>
#include <QVariantList>

namespace fincept::services::alpha_arena {

namespace {

inline Database& db() { return Database::instance(); }

QString new_id() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString json_compact(const QJsonObject& obj) {
    return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

QString string_list_to_json(const QStringList& list) {
    QJsonArray arr;
    for (const auto& s : list) arr.append(s);
    return QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

QStringList json_to_string_list(const QString& s) {
    auto doc = QJsonDocument::fromJson(s.toUtf8());
    if (!doc.isArray()) return {};
    QStringList out;
    for (const auto& v : doc.array()) out << v.toString();
    return out;
}

// Common LOG_ERROR shape used when an INSERT/UPDATE fails. The Result is then
// returned to the caller, which decides whether to retry / give up / surface.
template <typename T>
Result<T> log_and_forward_err(const QString& op, const std::string& msg) {
    LOG_ERROR("AlphaArenaRepo", QString("%1: %2").arg(op, QString::fromStdString(msg)));
    return Result<T>::err(msg);
}

} // namespace

AlphaArenaRepo& AlphaArenaRepo::instance() {
    static AlphaArenaRepo s;
    return s;
}

// ─── Competitions ───────────────────────────────────────────────────────────

Result<void> AlphaArenaRepo::insert_competition(const CompetitionRow& comp) {
    auto r = db().execute(
        QStringLiteral("INSERT INTO aa_competitions "
                       "(id,name,mode,venue,instruments_json,initial_capital,"
                       " cadence_seconds,live_mode,status) "
                       "VALUES (?,?,?,?,?,?,?,?,?)"),
        {comp.id, comp.name, mode_to_wire(comp.mode), comp.venue,
         string_list_to_json(comp.instruments), comp.initial_capital,
         comp.cadence_seconds, comp.live_mode ? 1 : 0,
         comp.status.isEmpty() ? QStringLiteral("created") : comp.status});
    if (r.is_err()) return log_and_forward_err<void>("insert_competition", r.error());
    return Result<void>::ok();
}

Result<void> AlphaArenaRepo::update_competition_status(const QString& id, const QString& status) {
    auto r = db().execute(
        QStringLiteral("UPDATE aa_competitions SET status=?, "
                       "started_at=CASE WHEN status='created' AND ?='running' "
                       "                THEN CURRENT_TIMESTAMP ELSE started_at END, "
                       "ended_at=CASE WHEN ? IN ('halted_by_user','halted_by_crash','completed','failed') "
                       "              THEN CURRENT_TIMESTAMP ELSE ended_at END "
                       "WHERE id=?"),
        {status, status, status, id});
    if (r.is_err()) return log_and_forward_err<void>("update_competition_status", r.error());
    return Result<void>::ok();
}

Result<void> AlphaArenaRepo::mark_live_mode_ack(const QString& id, const QString& host) {
    auto r = db().execute(
        QStringLiteral("UPDATE aa_competitions SET live_mode_acked_at=CURRENT_TIMESTAMP, "
                       "live_mode_acked_host=? WHERE id=?"),
        {host, id});
    if (r.is_err()) return log_and_forward_err<void>("mark_live_mode_ack", r.error());
    return Result<void>::ok();
}

Result<void> AlphaArenaRepo::increment_cycle_count(const QString& id) {
    auto r = db().execute(
        QStringLiteral("UPDATE aa_competitions SET cycle_count=cycle_count+1 WHERE id=?"), {id});
    if (r.is_err()) return log_and_forward_err<void>("increment_cycle_count", r.error());
    return Result<void>::ok();
}

namespace {
CompetitionRow row_to_competition(QSqlQuery& q) {
    CompetitionRow c;
    c.id = q.value("id").toString();
    c.name = q.value("name").toString();
    if (auto m = mode_from_wire(q.value("mode").toString())) c.mode = *m;
    c.venue = q.value("venue").toString();
    c.instruments = json_to_string_list(q.value("instruments_json").toString());
    c.initial_capital = q.value("initial_capital").toDouble();
    c.cadence_seconds = q.value("cadence_seconds").toInt();
    c.live_mode = q.value("live_mode").toInt() != 0;
    c.status = q.value("status").toString();
    c.cycle_count = q.value("cycle_count").toInt();
    c.created_at = q.value("created_at").toString();
    c.started_at = q.value("started_at").toString();
    c.ended_at = q.value("ended_at").toString();
    return c;
}
} // namespace

std::optional<CompetitionRow> AlphaArenaRepo::find_competition(const QString& id) {
    auto r = db().execute(
        QStringLiteral("SELECT id,name,mode,venue,instruments_json,initial_capital,"
                       "cadence_seconds,live_mode,status,cycle_count,created_at,started_at,ended_at "
                       "FROM aa_competitions WHERE id=?"),
        {id});
    if (r.is_err()) return std::nullopt;
    auto& q = r.value();
    if (!q.next()) return std::nullopt;
    return row_to_competition(q);
}

Result<QVector<CompetitionRow>> AlphaArenaRepo::list_competitions(int limit) {
    auto r = db().execute(
        QStringLiteral("SELECT id,name,mode,venue,instruments_json,initial_capital,"
                       "cadence_seconds,live_mode,status,cycle_count,created_at,started_at,ended_at "
                       "FROM aa_competitions ORDER BY created_at DESC LIMIT ?"),
        {limit});
    if (r.is_err()) return log_and_forward_err<QVector<CompetitionRow>>("list_competitions", r.error());
    QVector<CompetitionRow> out;
    auto& q = r.value();
    while (q.next()) out.append(row_to_competition(q));
    return Result<QVector<CompetitionRow>>::ok(std::move(out));
}

Result<QVector<CompetitionRow>> AlphaArenaRepo::find_orphaned_running() {
    auto r = db().execute(
        QStringLiteral("SELECT id,name,mode,venue,instruments_json,initial_capital,"
                       "cadence_seconds,live_mode,status,cycle_count,created_at,started_at,ended_at "
                       "FROM aa_competitions WHERE status='running'"),
        {});
    if (r.is_err()) return log_and_forward_err<QVector<CompetitionRow>>("find_orphaned_running", r.error());
    QVector<CompetitionRow> out;
    auto& q = r.value();
    while (q.next()) out.append(row_to_competition(q));
    return Result<QVector<CompetitionRow>>::ok(std::move(out));
}

// ─── Agents ─────────────────────────────────────────────────────────────────

Result<void> AlphaArenaRepo::insert_agent(const AgentRow& agent) {
    auto r = db().execute(
        QStringLiteral("INSERT INTO aa_agents "
                       "(id,competition_id,slot,provider,model_id,display_name,color_hex,"
                       " api_secret_handle,state) "
                       "VALUES (?,?,?,?,?,?,?,?,?)"),
        {agent.id, agent.competition_id, agent.slot, agent.provider, agent.model_id,
         agent.display_name, agent.color_hex, agent.api_secret_handle,
         agent.state.isEmpty() ? QStringLiteral("active") : agent.state});
    if (r.is_err()) return log_and_forward_err<void>("insert_agent", r.error());
    return Result<void>::ok();
}

Result<void> AlphaArenaRepo::update_agent_state(const QString& id, const QString& state) {
    auto r = db().execute(QStringLiteral("UPDATE aa_agents SET state=? WHERE id=?"), {state, id});
    if (r.is_err()) return log_and_forward_err<void>("update_agent_state", r.error());
    return Result<void>::ok();
}

Result<void> AlphaArenaRepo::bump_parse_failures(const QString& agent_id, int new_value) {
    auto r = db().execute(
        QStringLiteral("UPDATE aa_agents SET consecutive_parse_failures=? WHERE id=?"),
        {new_value, agent_id});
    if (r.is_err()) return log_and_forward_err<void>("bump_parse_failures", r.error());
    return Result<void>::ok();
}

Result<void> AlphaArenaRepo::bump_risk_rejects(const QString& agent_id, int new_value) {
    auto r = db().execute(
        QStringLiteral("UPDATE aa_agents SET consecutive_risk_rejects=? WHERE id=?"),
        {new_value, agent_id});
    if (r.is_err()) return log_and_forward_err<void>("bump_risk_rejects", r.error());
    return Result<void>::ok();
}

namespace {
AgentRow row_to_agent(QSqlQuery& q) {
    AgentRow a;
    a.id = q.value("id").toString();
    a.competition_id = q.value("competition_id").toString();
    a.slot = q.value("slot").toInt();
    a.provider = q.value("provider").toString();
    a.model_id = q.value("model_id").toString();
    a.display_name = q.value("display_name").toString();
    a.color_hex = q.value("color_hex").toString();
    a.api_secret_handle = q.value("api_secret_handle").toString();
    a.state = q.value("state").toString();
    a.consecutive_parse_failures = q.value("consecutive_parse_failures").toInt();
    a.consecutive_risk_rejects = q.value("consecutive_risk_rejects").toInt();
    return a;
}
} // namespace

Result<QVector<AgentRow>> AlphaArenaRepo::agents_for(const QString& competition_id) {
    auto r = db().execute(
        QStringLiteral("SELECT id,competition_id,slot,provider,model_id,display_name,color_hex,"
                       "api_secret_handle,state,consecutive_parse_failures,consecutive_risk_rejects "
                       "FROM aa_agents WHERE competition_id=? ORDER BY slot"),
        {competition_id});
    if (r.is_err()) return log_and_forward_err<QVector<AgentRow>>("agents_for", r.error());
    QVector<AgentRow> out;
    auto& q = r.value();
    while (q.next()) out.append(row_to_agent(q));
    return Result<QVector<AgentRow>>::ok(std::move(out));
}

std::optional<AgentRow> AlphaArenaRepo::find_agent(const QString& id) {
    auto r = db().execute(
        QStringLiteral("SELECT id,competition_id,slot,provider,model_id,display_name,color_hex,"
                       "api_secret_handle,state,consecutive_parse_failures,consecutive_risk_rejects "
                       "FROM aa_agents WHERE id=?"),
        {id});
    if (r.is_err()) return std::nullopt;
    auto& q = r.value();
    if (!q.next()) return std::nullopt;
    return row_to_agent(q);
}

// ─── Prompts ────────────────────────────────────────────────────────────────

Result<void> AlphaArenaRepo::upsert_prompt(const QString& sha256, const QString& text) {
    auto r = db().execute(
        QStringLiteral("INSERT OR IGNORE INTO aa_prompts (sha256,length,text) VALUES (?,?,?)"),
        {sha256, text.size(), text});
    if (r.is_err()) return log_and_forward_err<void>("upsert_prompt", r.error());
    return Result<void>::ok();
}

// ─── Ticks ──────────────────────────────────────────────────────────────────

Result<QString> AlphaArenaRepo::insert_tick(const QString& competition_id,
                                            int seq,
                                            qint64 utc_ms,
                                            const QString& system_prompt_sha256,
                                            const QString& market_snapshot_json) {
    const QString id = new_id();
    auto r = db().execute(
        QStringLiteral("INSERT INTO aa_ticks "
                       "(id,competition_id,seq,utc_ms,system_prompt_sha256,market_snapshot_json) "
                       "VALUES (?,?,?,?,?,?)"),
        {id, competition_id, seq, utc_ms, system_prompt_sha256, market_snapshot_json});
    if (r.is_err()) return log_and_forward_err<QString>("insert_tick", r.error());
    return Result<QString>::ok(id);
}

Result<void> AlphaArenaRepo::close_tick(const QString& tick_id) {
    auto r = db().execute(
        QStringLiteral("UPDATE aa_ticks SET status='closed', closed_at=CURRENT_TIMESTAMP WHERE id=?"),
        {tick_id});
    if (r.is_err()) return log_and_forward_err<void>("close_tick", r.error());
    return Result<void>::ok();
}

Result<void> AlphaArenaRepo::mark_tick_skipped(const QString& tick_id) {
    auto r = db().execute(
        QStringLiteral("UPDATE aa_ticks SET status='skipped' WHERE id=?"), {tick_id});
    if (r.is_err()) return log_and_forward_err<void>("mark_tick_skipped", r.error());
    return Result<void>::ok();
}

// ─── Decisions ──────────────────────────────────────────────────────────────

Result<QString> AlphaArenaRepo::insert_decision(const DecisionInsert& d) {
    const QString id = new_id();
    auto r = db().execute(
        QStringLiteral("INSERT INTO aa_decisions "
                       "(id,tick_id,agent_id,user_prompt_sha256,raw_response,parsed_actions_json,"
                       " parse_error,risk_verdict_json,latency_ms,tokens_in,tokens_out,cost_usd) "
                       "VALUES (?,?,?,?,?,?,?,?,?,?,?,?)"),
        {id, d.tick_id, d.agent_id, d.user_prompt_sha256, d.raw_response,
         d.parsed_actions_json, d.parse_error, d.risk_verdict_json,
         d.latency_ms, d.tokens_in, d.tokens_out, d.cost_usd});
    if (r.is_err()) return log_and_forward_err<QString>("insert_decision", r.error());
    return Result<QString>::ok(id);
}

Result<QVector<ModelChatRow>> AlphaArenaRepo::modelchat_for(const QString& competition_id,
                                                            const QString& agent_id,
                                                            int limit,
                                                            int offset) {
    auto r = db().execute(
        QStringLiteral("SELECT d.id, d.agent_id, d.tick_id, t.seq, t.utc_ms, "
                       "       d.user_prompt_sha256, d.raw_response, d.parsed_actions_json, "
                       "       d.parse_error, d.risk_verdict_json, d.latency_ms, "
                       "       d.tokens_in, d.tokens_out, d.cost_usd "
                       "FROM aa_decisions d JOIN aa_ticks t ON t.id = d.tick_id "
                       "WHERE t.competition_id=? AND d.agent_id=? "
                       "ORDER BY t.seq DESC LIMIT ? OFFSET ?"),
        {competition_id, agent_id, limit, offset});
    if (r.is_err()) return log_and_forward_err<QVector<ModelChatRow>>("modelchat_for", r.error());
    QVector<ModelChatRow> out;
    auto& q = r.value();
    while (q.next()) {
        ModelChatRow row;
        row.decision_id = q.value(0).toString();
        row.agent_id = q.value(1).toString();
        row.tick_id = q.value(2).toString();
        row.tick_seq = q.value(3).toInt();
        row.tick_utc_ms = q.value(4).toLongLong();
        row.user_prompt_sha256 = q.value(5).toString();
        row.raw_response = q.value(6).toString();
        row.parsed_actions_json = q.value(7).toString();
        row.parse_error = q.value(8).toString();
        row.risk_verdict_json = q.value(9).toString();
        row.latency_ms = q.value(10).toInt();
        row.tokens_in = q.value(11).toInt();
        row.tokens_out = q.value(12).toInt();
        row.cost_usd = q.value(13).toDouble();
        out.append(row);
    }
    return Result<QVector<ModelChatRow>>::ok(std::move(out));
}

// ─── Orders ─────────────────────────────────────────────────────────────────

Result<QString> AlphaArenaRepo::insert_order(const OrderInsert& o) {
    const QString id = new_id();
    auto r = db().execute(
        QStringLiteral("INSERT INTO aa_orders "
                       "(id,decision_id,agent_id,competition_id,coin,side,qty,leverage,type,tif,price) "
                       "VALUES (?,?,?,?,?,?,?,?,?,?,?)"),
        {id, o.decision_id, o.agent_id, o.competition_id, o.coin, o.side, o.qty,
         o.leverage, o.type, o.tif, o.price.has_value() ? *o.price : QVariant()});
    if (r.is_err()) return log_and_forward_err<QString>("insert_order", r.error());
    return Result<QString>::ok(id);
}

Result<void> AlphaArenaRepo::mark_order_status(const QString& order_id,
                                               const QString& status,
                                               const QString& venue_order_id,
                                               const QString& error) {
    auto r = db().execute(
        QStringLiteral("UPDATE aa_orders SET status=?, venue_order_id=?, error=?, "
                       "resolved_at=CASE WHEN ? IN ('filled','rejected','cancelled') "
                       "                 THEN CURRENT_TIMESTAMP ELSE resolved_at END "
                       "WHERE id=?"),
        {status, venue_order_id, error, status, order_id});
    if (r.is_err()) return log_and_forward_err<void>("mark_order_status", r.error());
    return Result<void>::ok();
}

// ─── Fills ──────────────────────────────────────────────────────────────────

Result<QString> AlphaArenaRepo::insert_fill(const QString& order_id,
                                            double qty, double price, double fee) {
    const QString id = new_id();
    auto r = db().execute(
        QStringLiteral("INSERT INTO aa_fills (id,order_id,qty,price,fee) VALUES (?,?,?,?,?)"),
        {id, order_id, qty, price, fee});
    if (r.is_err()) return log_and_forward_err<QString>("insert_fill", r.error());
    return Result<QString>::ok(id);
}

// ─── Positions ──────────────────────────────────────────────────────────────

Result<QString> AlphaArenaRepo::open_position(const QString& agent_id,
                                              const QString& competition_id,
                                              const Position& p) {
    const QString id = new_id();
    auto r = db().execute(
        QStringLiteral("INSERT INTO aa_positions "
                       "(id,agent_id,competition_id,coin,qty,entry,mark,liq_price,leverage,"
                       " profit_target,stop_loss,invalidation_condition) "
                       "VALUES (?,?,?,?,?,?,?,?,?,?,?,?)"),
        {id, agent_id, competition_id, p.coin, p.qty, p.entry, p.mark, p.liq_price,
         p.leverage, p.profit_target, p.stop_loss, p.invalidation_condition});
    if (r.is_err()) return log_and_forward_err<QString>("open_position", r.error());
    return Result<QString>::ok(id);
}

Result<void> AlphaArenaRepo::update_position_marks(const QString& position_id,
                                                   double mark, double liq_price) {
    auto r = db().execute(
        QStringLiteral("UPDATE aa_positions SET mark=?, liq_price=? WHERE id=?"),
        {mark, liq_price, position_id});
    if (r.is_err()) return log_and_forward_err<void>("update_position_marks", r.error());
    return Result<void>::ok();
}

Result<void> AlphaArenaRepo::close_position(const QString& position_id, const QString& reason) {
    auto r = db().execute(
        QStringLiteral("UPDATE aa_positions SET closed_at=CURRENT_TIMESTAMP, close_reason=? "
                       "WHERE id=? AND closed_at IS NULL"),
        {reason, position_id});
    if (r.is_err()) return log_and_forward_err<void>("close_position", r.error());
    return Result<void>::ok();
}

Result<QVector<Position>> AlphaArenaRepo::open_positions_for(const QString& agent_id) {
    auto r = db().execute(
        QStringLiteral("SELECT coin,qty,entry,mark,liq_price,leverage,"
                       "profit_target,stop_loss,invalidation_condition "
                       "FROM aa_positions WHERE agent_id=? AND closed_at IS NULL"),
        {agent_id});
    if (r.is_err()) return log_and_forward_err<QVector<Position>>("open_positions_for", r.error());
    QVector<Position> out;
    auto& q = r.value();
    while (q.next()) {
        Position p;
        p.coin = q.value(0).toString();
        p.qty = q.value(1).toDouble();
        p.entry = q.value(2).toDouble();
        p.mark = q.value(3).toDouble();
        p.liq_price = q.value(4).toDouble();
        p.leverage = q.value(5).toInt();
        p.profit_target = q.value(6).toDouble();
        p.stop_loss = q.value(7).toDouble();
        p.invalidation_condition = q.value(8).toString();
        // unrealized_pnl computed on demand by caller from mark vs entry
        out.append(p);
    }
    return Result<QVector<Position>>::ok(std::move(out));
}

// ─── PnL snapshots ──────────────────────────────────────────────────────────

Result<void> AlphaArenaRepo::insert_pnl_snapshot(const QString& tick_id,
                                                 const QString& agent_id,
                                                 const AgentAccountState& s,
                                                 double avg_leverage) {
    const QString id = new_id();
    auto r = db().execute(
        QStringLiteral("INSERT INTO aa_pnl_snapshots "
                       "(id,tick_id,agent_id,equity,cash,return_pct,sharpe,max_drawdown,"
                       " fees_paid,win_rate,trades_count,avg_leverage) "
                       "VALUES (?,?,?,?,?,?,?,?,?,?,?,?)"),
        {id, tick_id, agent_id, s.equity, s.cash, s.return_pct, s.sharpe, s.max_drawdown,
         s.fees_paid, s.win_rate, s.trades_count, avg_leverage});
    if (r.is_err()) return log_and_forward_err<void>("insert_pnl_snapshot", r.error());
    return Result<void>::ok();
}

Result<QVector<LeaderboardRow>> AlphaArenaRepo::leaderboard(const QString& competition_id) {
    // Latest snapshot per agent in this competition (rank by equity DESC).
    auto r = db().execute(
        QStringLiteral(
            "WITH latest AS ("
            "  SELECT s.*, ROW_NUMBER() OVER (PARTITION BY s.agent_id ORDER BY s.created_at DESC) rn"
            "  FROM aa_pnl_snapshots s"
            "  JOIN aa_agents a ON a.id=s.agent_id"
            "  WHERE a.competition_id=?"
            ")"
            "SELECT a.id, a.display_name, a.color_hex, "
            "       l.equity, l.return_pct, l.sharpe, l.max_drawdown, l.fees_paid, "
            "       l.avg_leverage, l.trades_count, l.win_rate "
            "FROM aa_agents a "
            "LEFT JOIN latest l ON l.agent_id=a.id AND l.rn=1 "
            "WHERE a.competition_id=? "
            "ORDER BY COALESCE(l.equity, 0) DESC, COALESCE(l.sharpe, 0) DESC"),
        {competition_id, competition_id});
    if (r.is_err()) return log_and_forward_err<QVector<LeaderboardRow>>("leaderboard", r.error());

    QVector<LeaderboardRow> out;
    auto& q = r.value();
    int rank = 1;
    while (q.next()) {
        LeaderboardRow lr;
        lr.agent_id = q.value(0).toString();
        lr.display_name = q.value(1).toString();
        lr.color_hex = q.value(2).toString();
        lr.rank = rank++;
        lr.equity = q.value(3).toDouble();
        lr.return_pct = q.value(4).toDouble();
        lr.sharpe = q.value(5).toDouble();
        lr.max_drawdown = q.value(6).toDouble();
        lr.fees_paid = q.value(7).toDouble();
        lr.avg_leverage = q.value(8).toDouble();
        lr.trades_count = q.value(9).toInt();
        lr.win_rate = q.value(10).toDouble();
        out.append(lr);
    }
    return Result<QVector<LeaderboardRow>>::ok(std::move(out));
}

// ─── Events ─────────────────────────────────────────────────────────────────

Result<void> AlphaArenaRepo::append_event(const QString& competition_id,
                                          const QString& agent_id,
                                          const QString& type,
                                          const QJsonObject& payload) {
    auto r = db().execute(
        QStringLiteral("INSERT INTO aa_events (competition_id,agent_id,type,payload_json) "
                       "VALUES (?,?,?,?)"),
        {competition_id, agent_id.isEmpty() ? QVariant() : QVariant(agent_id),
         type, json_compact(payload)});
    if (r.is_err()) return log_and_forward_err<void>("append_event", r.error());
    return Result<void>::ok();
}

Result<QVector<EventRow>> AlphaArenaRepo::events_since(const QString& competition_id,
                                                       qint64 since_seq, int limit) {
    auto r = db().execute(
        QStringLiteral("SELECT seq,competition_id,COALESCE(agent_id,''),type,payload_json,ts "
                       "FROM aa_events WHERE competition_id=? AND seq>? "
                       "ORDER BY seq ASC LIMIT ?"),
        {competition_id, since_seq, limit});
    if (r.is_err()) return log_and_forward_err<QVector<EventRow>>("events_since", r.error());
    QVector<EventRow> out;
    auto& q = r.value();
    while (q.next()) {
        EventRow e;
        e.seq = q.value(0).toLongLong();
        e.competition_id = q.value(1).toString();
        e.agent_id = q.value(2).toString();
        e.type = q.value(3).toString();
        e.payload_json = q.value(4).toString();
        e.ts = q.value(5).toString();
        out.append(e);
    }
    return Result<QVector<EventRow>>::ok(std::move(out));
}

// ─── HITL ───────────────────────────────────────────────────────────────────

Result<QString> AlphaArenaRepo::request_hitl(const QString& decision_id,
                                             const QString& agent_id,
                                             const QJsonObject& action,
                                             const QString& prompt_text) {
    const QString id = new_id();
    auto r = db().execute(
        QStringLiteral("INSERT INTO aa_hitl_approvals "
                       "(id,decision_id,agent_id,action_json,prompt_text) VALUES (?,?,?,?,?)"),
        {id, decision_id, agent_id, json_compact(action), prompt_text});
    if (r.is_err()) return log_and_forward_err<QString>("request_hitl", r.error());
    return Result<QString>::ok(id);
}

Result<void> AlphaArenaRepo::resolve_hitl(const QString& approval_id,
                                          const QString& status,
                                          const QString& resolved_by) {
    auto r = db().execute(
        QStringLiteral("UPDATE aa_hitl_approvals SET status=?, resolved_at=CURRENT_TIMESTAMP, "
                       "resolved_by=? WHERE id=?"),
        {status, resolved_by, approval_id});
    if (r.is_err()) return log_and_forward_err<void>("resolve_hitl", r.error());
    return Result<void>::ok();
}

} // namespace fincept::services::alpha_arena
