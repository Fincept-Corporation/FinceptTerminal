#include "services/alpha_arena/ArenaEngine.h"
#include "core/logging/Logger.h"
#include "services/alpha_arena/ArenaDecisionParser.h"
#include "services/alpha_arena/ArenaMarketData.h"
#include "services/alpha_arena/ArenaModelRegistry.h"
#include "services/alpha_arena/ArenaPrompts.h"
#include "services/alpha_arena/HyperliquidLiveVenue.h"
#include "storage/secure/SecureStorage.h"
#include <QDateTime>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QUuid>

namespace fincept::arena {

static constexpr const char* TAG = "ArenaEngine";
static constexpr int kKillMarksTimeoutMs = 2000;   // bounded wait; a kill must never hang

static QString approval_summary(const AgentAction& act) {
    return QStringLiteral("%1 %2 %3 $%4 @%5x")
        .arg(act.kind == ActionKind::Open ? QStringLiteral("OPEN") : QStringLiteral("CLOSE"),
             act.side.toUpper(), act.coin, QString::number(act.size_usd, 'f', 0),
             QString::number(act.leverage, 'f', 0));
}

ArenaEngine& ArenaEngine::instance() {
    static ArenaEngine e;
    return e;
}

ArenaEngine::ArenaEngine() {
    timer_ = new QTimer(this);
    connect(timer_, &QTimer::timeout, this, &ArenaEngine::run_round);
    marks_timer_ = new QTimer(this);
    connect(marks_timer_, &QTimer::timeout, this, &ArenaEngine::run_marks_tick);
}

bool ArenaEngine::agent_is_active(const QString& agent_id) const {
    for (const auto& a : agents_)
        if (a.id == agent_id) return a.status == QLatin1String("active");
    return false;   // unknown id → never execute on its behalf
}

void ArenaEngine::init() {
    if (inited_) return;
    inited_ = true;
    if (!market_) market_ = new ArenaMarketData(this);
    if (!llm_) llm_ = new ArenaLlmClient(this);
    if (auto r = ArenaStore::instance().competitions_with_status("running");
        r.is_ok() && !r.value().isEmpty()) {
        crash_pending_ = r.value();   // kept queryable — init() runs before any UI exists
        emit crash_recovery_pending(crash_pending_);
    }
}

void ArenaEngine::dismiss_crash_recovery(const QString& id) {
    crash_pending_.removeAll(id);
    ArenaStore::instance().set_competition_status(id, "ended");
    emit competition_status_changed(id, "ended");
}

void ArenaEngine::set_market_for_test(IArenaMarketData* m) {
    delete market_;
    market_ = m;
    m->setParent(this);
    inited_ = true;
    if (!llm_) llm_ = new ArenaLlmClient(this);
}

void ArenaEngine::set_llm_for_test(IArenaLlmClient* c) {
    delete llm_;
    llm_ = c;
    c->setParent(this);
    inited_ = true;
    if (!market_) market_ = new ArenaMarketData(this);
}

Result<QString> ArenaEngine::create_competition(const ArenaConfig& cfg) {
    if (cfg.agents.isEmpty()) return Result<QString>::err("select at least one model");
    if (cfg.instruments.isEmpty()) return Result<QString>::err("select at least one instrument");
    return ArenaStore::instance().insert_competition(cfg);
}

Result<void> ArenaEngine::start(const QString& competition_id) {
    ++epoch_;   // invalidate in-flight async callbacks from any previous run
    if (!active_id_.isEmpty()) {   // never two running: end the old one cleanly first
        auto r = stop();
        if (r.is_err()) return r;
    }
    auto comp_r = ArenaStore::instance().competition(competition_id);
    if (comp_r.is_err()) return Result<void>::err(comp_r.error());
    comp_ = comp_r.value();
    auto agents_r = ArenaStore::instance().agents_for(competition_id);
    if (agents_r.is_err()) return Result<void>::err(agents_r.error());
    agents_ = agents_r.value();
    active_id_ = competition_id;
    crash_pending_.removeAll(competition_id);   // resuming clears the pending-recovery flag
    limits_.max_leverage = comp_.max_leverage;
    if (comp_.venue == QLatin1String("hyperliquid") && comp_.live_mode) {
        // The wizard's gate dialog ran before the competition existed and stored
        // the agent wallet key under ".../pending" — move it to this competition's
        // handle so the live venue's signer can find it (store new, then delete old).
        auto& ss = SecureStorage::instance();
        const QString pending_handle = QStringLiteral("alpha_arena/agent_key/pending");
        if (auto key = ss.retrieve(pending_handle); key.is_ok()) {
            const QString comp_handle = QStringLiteral("alpha_arena/agent_key/%1").arg(competition_id);
            if (ss.store(comp_handle, key.value()).is_ok())
                ss.remove(pending_handle);
            else
                LOG_ERROR(TAG, QStringLiteral("failed to re-key live wallet handle for %1").arg(competition_id));
        }
        venue_ = std::make_unique<HyperliquidLiveVenue>(ArenaStore::instance(), competition_id);
    } else {
        venue_ = std::make_unique<PaperExchange>(ArenaStore::instance(), competition_id);
    }
    venue_->load();
    auto seq_r = ArenaStore::instance().last_round_seq(competition_id);
    round_seq_ = seq_r.is_ok() ? seq_r.value() : 0;
    last_mark_ts_ = QDateTime::currentMSecsSinceEpoch();
    if (comp_.started_at == 0) {
        comp_.started_at = last_mark_ts_;   // keep the in-memory row in sync so stop() persists it
        ArenaStore::instance().set_competition_timestamps(competition_id, last_mark_ts_, 0);
    }
    ArenaStore::instance().set_competition_status(competition_id, "running");
    emit competition_status_changed(competition_id, "running");
    // Re-surface HITL approvals persisted by a previous run — a hidden panel or
    // an app restart must not lose them. resume_after_hitl re-checks agent
    // status and executes at the venue's CURRENT marks, so an old seq is safe.
    pending_hitl_.clear();
    if (auto hitl_r = ArenaStore::instance().hitl_pending_for(competition_id); hitl_r.is_ok())
        for (const auto& h : hitl_r.value()) {
            PendingApproval pa;
            pa.id = h.id;
            pa.agent_id = h.agent_id;
            pa.seq = h.round_seq;
            pa.action.kind = h.kind == QLatin1String("close") ? ActionKind::Close : ActionKind::Open;
            pa.action.coin = h.coin;
            pa.action.side = h.side;
            pa.action.size_usd = h.size_usd;
            pa.action.leverage = h.leverage;
            pa.action.reason = h.reason;
            pending_hitl_.append(pa);
            emit hitl_requested(pa.id, pa.agent_id, approval_summary(pa.action));
        }
    timer_->start(comp_.cadence_seconds * 1000);
    marks_timer_->start(marks_interval_ms_);   // between-round live marks
    force_round();   // first round immediately
    return Result<void>::ok();
}

Result<void> ArenaEngine::pause() {
    if (active_id_.isEmpty()) return Result<void>::err("no active competition");
    timer_->stop();
    marks_timer_->stop();
    ArenaStore::instance().set_competition_status(active_id_, "paused");
    emit competition_status_changed(active_id_, "paused");
    return Result<void>::ok();
}

Result<void> ArenaEngine::resume() {
    if (active_id_.isEmpty()) return Result<void>::err("no active competition");
    timer_->start(comp_.cadence_seconds * 1000);
    marks_timer_->start(marks_interval_ms_);
    ArenaStore::instance().set_competition_status(active_id_, "running");
    emit competition_status_changed(active_id_, "running");
    return Result<void>::ok();
}

Result<void> ArenaEngine::stop() {
    if (active_id_.isEmpty()) return Result<void>::err("no active competition");
    ++epoch_;   // any snapshot/LLM callback still in flight becomes a no-op
    timer_->stop();
    marks_timer_->stop();
    ArenaStore::instance().set_competition_status(active_id_, "ended");
    ArenaStore::instance().set_competition_timestamps(active_id_, comp_.started_at,
                                                      QDateTime::currentMSecsSinceEpoch());
    emit competition_status_changed(active_id_, "ended");
    const QString id = active_id_;
    active_id_.clear();
    agents_.clear();
    venue_.reset();
    round_in_flight_ = false;
    pending_agents_ = 0;
    round_notional_.clear();
    pending_hitl_.clear();   // in-memory only — persisted rows survive for the next start()
    LOG_INFO(TAG, QStringLiteral("competition %1 stopped").arg(id));
    return Result<void>::ok();
}

Result<void> ArenaEngine::kill_all() {
    if (!venue_) return Result<void>::err("no active competition");
    timer_->stop();              // no new round may start mid-kill
    marks_timer_->stop();
    refresh_marks_blocking();    // close at fresh prices, not last-tick marks
    for (const auto& a : agents_) venue_->close_all(a.id, round_seq_);
    // Verify the book is actually flat. On a resumed venue whose marks are empty
    // (crash-recovery reload + a marks refresh that timed out), close_all skips any
    // position with no mark and leaves it OPEN — surface that instead of a clean ok.
    int open_left = 0;
    for (const auto& a : agents_) open_left += venue_->account(a.id).positions.size();
    // Un-actioned HITL approvals are moot once we've closed out. Resolve them as
    // rejected orders (auditable, never silently dropped) and drop the persisted
    // rows so a later restart cannot resurrect them.
    for (const auto& pa : pending_hitl_) {
        ArenaStore::instance().delete_hitl_pending(pa.id);
        reject_approval(pa, QStringLiteral("kill_all"));
    }
    pending_hitl_.clear();
    ArenaStore::instance().insert_event(
        active_id_, "", "kill_all",
        open_left ? QStringLiteral("{\"positions_left_open\":%1}").arg(open_left) : QStringLiteral("{}"));
    auto stopped = stop();
    if (open_left > 0) {
        LOG_ERROR(TAG, QStringLiteral("kill_all left %1 position(s) OPEN (no live marks to close at)").arg(open_left));
        return Result<void>::err(
            QStringLiteral("Kill incomplete: %1 position(s) could not be closed (no live marks) and remain open.")
                .arg(open_left)
                .toStdString());
    }
    return stopped;
}

Result<void> ArenaEngine::kill_agent(const QString& agent_id) {
    if (active_id_.isEmpty() || !venue_) return Result<void>::err("no active competition");
    bool known = false;
    for (const auto& a : agents_)
        if (a.id == agent_id) { known = true; break; }
    if (!known) return Result<void>::err("unknown agent id");

    // Halt FIRST (immediate-halt rule): an LLM callback landing during the
    // marks refresh below must execute nothing for this agent.
    const QString reason = QStringLiteral("killed by user");
    ArenaStore::instance().set_agent_status(agent_id, "halted_user", reason);
    for (auto& a : agents_)
        if (a.id == agent_id) { a.status = "halted_user"; a.halt_reason = reason; }
    emit agent_status_changed(agent_id, "halted_user", reason);

    refresh_marks_blocking();    // close at fresh prices, not last-tick marks
    const auto closes = venue_->close_all(agent_id, round_seq_);
    for (const auto& o : closes) emit order_executed(o.agent_id, o.id, o.status);

    ArenaStore::instance().insert_event(
        active_id_, agent_id, "agent_killed",
        QString::fromUtf8(QJsonDocument(QJsonObject{{"agent_id", agent_id}, {"round_seq", round_seq_}})
                              .toJson(QJsonDocument::Compact)));

    // Final equity snapshot at the current round (PK (comp,agent,round) upserts).
    const auto acct = venue_->account(agent_id);
    EquitySnapshotRow s;
    s.competition_id = active_id_;
    s.agent_id = agent_id;
    s.round_seq = round_seq_;
    s.ts = QDateTime::currentMSecsSinceEpoch();
    s.equity = acct.equity;
    s.balance = acct.balance;
    for (auto it = acct.upnl.begin(); it != acct.upnl.end(); ++it) s.unrealized_pnl += it.value();
    ArenaStore::instance().insert_equity_snapshot(s);
    // If the (post-close) book still holds positions, the marks were missing and
    // close_all left them OPEN — surface it instead of reporting a clean kill.
    if (!acct.positions.isEmpty()) {
        LOG_ERROR(TAG, QStringLiteral("kill_agent %1 left %2 position(s) OPEN (no live marks to close at)")
                           .arg(agent_id)
                           .arg(acct.positions.size()));
        return Result<void>::err(
            QStringLiteral("Kill incomplete: %1 position(s) could not be closed (no live marks) and remain open.")
                .arg(acct.positions.size())
                .toStdString());
    }
    return Result<void>::ok();
}

Result<void> ArenaEngine::halt_agent(const QString& agent_id) {
    auto r = ArenaStore::instance().set_agent_status(agent_id, "halted_user", "halted by user");
    if (r.is_err()) return r;
    for (auto& a : agents_)
        if (a.id == agent_id) a.status = "halted_user";
    emit agent_status_changed(agent_id, "halted_user", "halted by user");
    return Result<void>::ok();
}

Result<void> ArenaEngine::resume_agent(const QString& agent_id) {
    auto r = ArenaStore::instance().set_agent_status(agent_id, "active", "");
    if (r.is_err()) return r;
    ArenaStore::instance().set_agent_failures(agent_id, 0);
    for (auto& a : agents_)
        if (a.id == agent_id) {
            a.status = "active";
            a.consecutive_failures = 0;
        }
    emit agent_status_changed(agent_id, "active", "");
    return Result<void>::ok();
}

Result<void> ArenaEngine::set_cadence(int seconds) {
    if (active_id_.isEmpty()) return Result<void>::err("no active competition");
    if (seconds < 60 || seconds > 3600) return Result<void>::err("cadence must be 60–3600s");
    comp_.cadence_seconds = seconds;
    ArenaStore::instance().set_cadence(active_id_, seconds);
    if (timer_->isActive()) timer_->start(seconds * 1000);
    return Result<void>::ok();
}

int ArenaEngine::next_round_in_seconds() const {
    return timer_->isActive() ? timer_->remainingTime() / 1000 : -1;
}

void ArenaEngine::force_round() { run_round(); }

void ArenaEngine::set_marks_interval_for_test(int ms) {
    marks_interval_ms_ = ms;
    if (marks_timer_->isActive()) marks_timer_->start(ms);
}

// Between-round live marks: re-mark the venue every marks_interval_ms_ so the
// dashboard sees live equity instead of waiting a whole cadence (default 180s).
// Runs the same funding/liquidation sweep as a round — last_mark_ts_ keeps the
// funding elapsed-time correct across BOTH kinds of mark_all.
void ArenaEngine::run_marks_tick() {
    if (round_in_flight_ || active_id_.isEmpty()) return;
    const quint64 epoch = epoch_;
    market_->fetch_mids(comp_.instruments, [this, epoch](Result<QHash<QString, double>> r) {
        if (epoch != epoch_ || active_id_.isEmpty()) return;   // stopped/replaced mid-fetch
        if (round_in_flight_) return;   // a round started meanwhile — on_snapshot owns marking
        if (r.is_err()) return;         // transient; next tick retries
        venue_->set_marks(r.value());
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        const auto liqs = venue_->mark_all(now - last_mark_ts_, round_seq_);
        last_mark_ts_ = now;
        for (const auto& o : liqs) emit order_executed(o.agent_id, o.id, o.status);
        emit marks_updated();
    });
}

// Kill paths close positions immediately, but the venue's stored marks can be a
// full marks-interval stale (or missing before the first tick — close_all then
// leaves such positions open). Run one synchronous marks tick — set_marks plus
// the mark_all sweep close_all's contract requires — with a bounded wait; on
// timeout/error fall through and close at the stored marks: a kill never hangs.
void ArenaEngine::refresh_marks_blocking() {
    if (!venue_ || !market_) return;
    QEventLoop loop;
    QPointer<QEventLoop> alive(&loop);   // late (post-timeout) callbacks find it null
    const quint64 epoch = epoch_;
    market_->fetch_mids(comp_.instruments, [this, epoch, alive](Result<QHash<QString, double>> r) {
        if (epoch == epoch_ && venue_ && r.is_ok()) {
            venue_->set_marks(r.value());
            const qint64 now = QDateTime::currentMSecsSinceEpoch();
            const auto liqs = venue_->mark_all(now - last_mark_ts_, round_seq_);
            last_mark_ts_ = now;
            for (const auto& o : liqs) emit order_executed(o.agent_id, o.id, o.status);
        }
        if (alive) alive->quit();
    });
    QTimer::singleShot(kKillMarksTimeoutMs, &loop, &QEventLoop::quit);
    // ExcludeUserInputEvents: this wait must not deliver clicks — a "New"/resume/
    // second-kill within these <=2s would re-enter the engine and corrupt the kill.
    loop.exec(QEventLoop::ExcludeUserInputEvents);
}

void ArenaEngine::run_round() {
    if (round_in_flight_ || active_id_.isEmpty()) return;
    round_in_flight_ = true;
    const int seq = round_seq_ + 1;
    const quint64 epoch = epoch_;
    emit round_started(seq);
    market_->fetch_snapshot(comp_.instruments, [this, epoch, seq](Result<MarketSnapshot> r) {
        on_snapshot(epoch, seq, std::move(r));
    });
}

void ArenaEngine::on_snapshot(quint64 epoch, int seq, Result<MarketSnapshot> r) {
    if (epoch != epoch_) return;   // stale callback from a stopped/replaced run; a new
                                   // run owns round_in_flight_ — do not touch it here
    if (active_id_.isEmpty()) {    // stopped mid-fetch
        round_in_flight_ = false;
        return;
    }
    if (r.is_err()) {
        ArenaStore::instance().insert_event(
            active_id_, "", "market_data_error",
            QString::fromUtf8(QJsonDocument(QJsonObject{{"error", QString::fromStdString(r.error())}})
                                  .toJson(QJsonDocument::Compact)));
        round_in_flight_ = false;
        emit round_failed(seq, QString::fromStdString(r.error()));
        return;   // retry on next natural tick
    }
    round_seq_ = seq;
    last_snapshot_ = r.value();
    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    QJsonObject snap_json;
    QHash<QString, double> mids, fundings;
    for (const auto& c : last_snapshot_.coins) {
        mids[c.coin] = c.mid;
        fundings[c.coin] = c.funding_rate;
        snap_json[c.coin] = QJsonObject{{"mid", c.mid},
                                        {"funding", c.funding_rate},
                                        {"oi", c.open_interest},
                                        {"chg", c.day_change_pct}};
    }
    ArenaStore::instance().insert_round(
        active_id_, seq, now, QString::fromUtf8(QJsonDocument(snap_json).toJson(QJsonDocument::Compact)));

    venue_->set_marks(mids);
    venue_->set_funding(fundings);
    const auto liqs = venue_->mark_all(now - last_mark_ts_, seq);
    for (const auto& o : liqs) emit order_executed(o.agent_id, o.id, o.status);
    last_mark_ts_ = now;

    round_notional_.clear();
    pending_agents_ = 0;
    for (const auto& a : agents_)
        if (a.status == QLatin1String("active")) ++pending_agents_;
    if (pending_agents_ == 0) {
        settle_round(seq);
        return;
    }
    for (const auto& a : agents_)
        if (a.status == QLatin1String("active"))
            dispatch_agent(seq, a, last_snapshot_, false, {}, {});
}

void ArenaEngine::dispatch_agent(int seq, const AgentRow& agent, const MarketSnapshot& snap,
                                 bool is_retry, const QString& prior_raw, const QString& prior_error) {
    auto cred = ArenaModelRegistry::instance().resolve_credentials(agent.source_kind, agent.source_ref,
                                                                   agent.provider);
    if (cred.is_err()) {
        DecisionRow d;
        d.competition_id = active_id_;
        d.agent_id = agent.id;
        d.round_seq = seq;
        d.status = "skipped_no_key";
        d.parse_error = QString::fromStdString(cred.error());
        auto id_r = ArenaStore::instance().insert_decision(d);
        emit decision_received(id_r.is_ok() ? id_r.value() : QString(), agent.id, d.status);
        agent_done(seq);
        return;
    }
    const QString sys = system_prompt(comp_);
    auto recent_r = ArenaStore::instance().recent_decisions(active_id_, agent.id, 5);
    QString user = user_prompt(snap, venue_->account(agent.id),
                               recent_r.is_ok() ? recent_r.value() : QVector<DecisionRow>{}, seq,
                               comp_.started_at);
    if (is_retry)
        user += QStringLiteral("\n\nYour previous reply was not valid: %1\nPrevious reply:\n%2\n"
                               "Reply again with ONLY the JSON object.")
                    .arg(prior_error, prior_raw.left(2000));

    ArenaLlmRequest req;
    req.provider = agent.provider;
    req.model_id = agent.model_id;
    req.api_key = cred.value().api_key;
    req.base_url = cred.value().base_url;
    req.system_prompt = sys;
    req.user_prompt = user;

    const AgentRow agent_copy = agent;
    const quint64 epoch = epoch_;
    llm_->complete(req, [this, epoch, seq, agent_copy, sys, user, is_retry](ArenaLlmResult r) {
        if (epoch != epoch_ || active_id_.isEmpty()) return;   // stopped/replaced mid-call
        finish_agent(seq, agent_copy, r, sys, user, is_retry);
    });
}

void ArenaEngine::finish_agent(int seq, const AgentRow& agent, const ArenaLlmResult& llm,
                               const QString& sys, const QString& user, bool was_retry) {
    // Halt is immediate (user-reported bug): the user may have halted/killed this
    // agent while its LLM call was in flight (up to 60s). `agent` is the row as
    // of dispatch — re-check the CURRENT status and, if no longer active, persist
    // the decision with its real status but execute nothing (no venue calls, no
    // HITL queueing, no repair retry).
    const bool halted = !agent_is_active(agent.id);
    auto skip_halted = [this, seq, &agent]() {
        ArenaStore::instance().insert_event(
            active_id_, agent.id, "actions_skipped_agent_halted",
            QString::fromUtf8(QJsonDocument(QJsonObject{{"agent_id", agent.id}, {"round_seq", seq}})
                                  .toJson(QJsonDocument::Compact)));
        agent_done(seq);
    };

    DecisionRow d;
    d.competition_id = active_id_;
    d.agent_id = agent.id;
    d.round_seq = seq;
    d.system_prompt = sys;
    d.user_prompt = user;
    d.raw_response = llm.content;
    d.latency_ms = llm.latency_ms;
    d.prompt_tokens = llm.prompt_tokens;
    d.completion_tokens = llm.completion_tokens;

    if (!llm.success) {
        d.status = llm.error == QLatin1String("timeout") ? "timeout" : "llm_error";
        d.parse_error = llm.error;
        auto id_r = ArenaStore::instance().insert_decision(d);
        emit decision_received(id_r.is_ok() ? id_r.value() : QString(), agent.id, d.status);
        if (halted) { skip_halted(); return; }
        record_failure(agent, d.status);
        agent_done(seq);
        return;
    }

    auto parsed = parse_decision(llm.content, comp_.instruments, comp_.max_leverage);
    if (!parsed.ok()) {
        if (!was_retry && !halted) {   // one repair retry (spec §5); none for a halted agent
            dispatch_agent(seq, agent, last_snapshot_, true, llm.content, parsed.error);
            return;
        }
        d.status = "parse_error";
        d.parse_error = parsed.error;
        auto id_r = ArenaStore::instance().insert_decision(d);
        emit decision_received(id_r.is_ok() ? id_r.value() : QString(), agent.id, d.status);
        if (halted) { skip_halted(); return; }
        record_failure(agent, d.status);
        agent_done(seq);
        return;
    }

    d.status = "ok";
    d.actions_json = parsed.actions_json();
    auto id_r = ArenaStore::instance().insert_decision(d);
    ArenaStore::instance().set_agent_failures(agent.id, 0);
    for (auto& a : agents_)
        if (a.id == agent.id) a.consecutive_failures = 0;
    emit decision_received(id_r.is_ok() ? id_r.value() : QString(), agent.id, d.status);
    if (halted) { skip_halted(); return; }   // decision recorded; actions never reach the venue

    for (const auto& act : parsed.actions) {
        if (act.kind == ActionKind::Hold) continue;
        const auto acct = venue_->account(agent.id);
        const auto verdict = evaluate(act, acct, limits_, round_notional_.value(agent.id));
        if (!verdict.approved) {
            OrderRow rej;
            rej.competition_id = active_id_;
            rej.agent_id = agent.id;
            rej.round_seq = seq;
            rej.coin = act.coin;
            rej.side = act.side;
            rej.action = act.kind == ActionKind::Open ? "open" : "close";
            rej.notional_usd = act.size_usd;
            rej.leverage = act.leverage;
            rej.status = "rejected_risk";
            rej.reject_reason = verdict.reason;
            auto or_r = ArenaStore::instance().insert_order(rej);
            emit order_executed(agent.id, or_r.is_ok() ? or_r.value() : QString(), rej.status);
            continue;
        }
        if (comp_.live_mode) {
            // HITL: live competitions queue risk-approved actions until a human
            // decides (resume_after_hitl) — nothing reaches the venue before that.
            // Count queued opens against the round-notional cap NOW — otherwise a
            // multi-action decision is risk-checked with round_notional_so_far == 0.
            if (act.kind == ActionKind::Open) round_notional_[agent.id] += act.size_usd;
            PendingApproval pa;
            pa.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            pa.agent_id = agent.id;
            pa.action = act;
            pa.seq = seq;
            pending_hitl_.append(pa);
            HitlPendingRow hp;
            hp.id = pa.id;
            hp.competition_id = active_id_;
            hp.agent_id = agent.id;
            hp.round_seq = seq;
            hp.kind = act.kind == ActionKind::Open ? QStringLiteral("open") : QStringLiteral("close");
            hp.coin = act.coin;
            hp.side = act.side;
            hp.size_usd = act.size_usd;
            hp.leverage = act.leverage;
            hp.reason = act.reason;
            ArenaStore::instance().insert_hitl_pending(hp);   // survives hidden panel / restart
            emit hitl_requested(pa.id, agent.id, approval_summary(act));
            continue;   // executed only after approval
        }
        if (act.kind == ActionKind::Open) round_notional_[agent.id] += act.size_usd;
        auto fill = venue_->execute(agent.id, act, seq);
        if (fill.is_ok()) emit order_executed(agent.id, fill.value().id, fill.value().status);
    }
    agent_done(seq);
}

void ArenaEngine::resume_after_hitl(const QString& approval_id, bool approved) {
    for (int i = 0; i < pending_hitl_.size(); ++i) {
        if (pending_hitl_[i].id != approval_id) continue;
        const PendingApproval pa = pending_hitl_.takeAt(i);
        ArenaStore::instance().delete_hitl_pending(pa.id);   // actioned — drop the persisted row
        if (!approved) {
            reject_approval(pa, QStringLiteral("HITL rejected"));
            return;
        }
        if (!venue_) return;   // competition stopped between request and decision
        if (!agent_is_active(pa.agent_id)) {
            // Agent halted between the HITL request and the approval — same
            // immediate-halt rule as finish_agent: record, never execute.
            reject_approval(pa, QStringLiteral("agent halted"));
            return;
        }
        auto fill = venue_->execute(pa.agent_id, pa.action, pa.seq);
        if (fill.is_ok()) emit order_executed(pa.agent_id, fill.value().id, fill.value().status);
        return;
    }
}

// Reject one queued HITL action: persist a rejected order row (audit trail —
// approvals are never silently dropped) and notify listeners.
void ArenaEngine::reject_approval(const PendingApproval& pa, const QString& reason) {
    OrderRow rej;
    rej.competition_id = active_id_;
    rej.agent_id = pa.agent_id;
    rej.round_seq = pa.seq;
    rej.coin = pa.action.coin;
    rej.side = pa.action.side;
    rej.action = pa.action.kind == ActionKind::Open ? "open" : "close";
    rej.notional_usd = pa.action.size_usd;
    rej.leverage = pa.action.leverage;
    rej.status = "rejected_risk";
    rej.reject_reason = reason;
    auto r = ArenaStore::instance().insert_order(rej);
    emit order_executed(pa.agent_id, r.is_ok() ? r.value() : QString(), rej.status);
}

void ArenaEngine::record_failure(const AgentRow& agent, const QString& kind) {
    for (auto& a : agents_) {
        if (a.id != agent.id) continue;
        a.consecutive_failures += 1;
        ArenaStore::instance().set_agent_failures(a.id, a.consecutive_failures);
        if (a.consecutive_failures >= limits_.circuit_threshold) {
            a.status = "halted_circuit";
            ArenaStore::instance().set_agent_status(
                a.id, a.status, QStringLiteral("%1 consecutive %2").arg(a.consecutive_failures).arg(kind));
            emit agent_status_changed(a.id, a.status, kind);
        }
    }
}

void ArenaEngine::agent_done(int seq) {
    if (--pending_agents_ > 0) return;
    settle_round(seq);
}

void ArenaEngine::settle_round(int seq) {
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (const auto& a : agents_) {
        const auto acct = venue_->account(a.id);
        EquitySnapshotRow s;
        s.competition_id = active_id_;
        s.agent_id = a.id;
        s.round_seq = seq;
        s.ts = now;
        s.equity = acct.equity;
        s.balance = acct.balance;
        for (auto it = acct.upnl.begin(); it != acct.upnl.end(); ++it) s.unrealized_pnl += it.value();
        ArenaStore::instance().insert_equity_snapshot(s);
    }
    ArenaStore::instance().complete_round(active_id_, seq, now, "completed");
    round_in_flight_ = false;
    emit round_completed(seq);
}

AccountView ArenaEngine::account_view(const QString& agent_id) const {
    return venue_ ? venue_->account(agent_id) : AccountView{};
}

} // namespace fincept::arena
