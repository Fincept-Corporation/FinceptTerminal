#include "services/alpha_arena/AlphaArenaEngine.h"

#include "core/logging/Logger.h"
#include "services/alpha_arena/ContextBuilder.h"
#include "storage/secure/SecureStorage.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QSysInfo>
#include <QUuid>

namespace fincept::services::alpha_arena {

namespace {

QString new_uuid() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }

} // namespace

AlphaArenaEngine& AlphaArenaEngine::instance() {
    static AlphaArenaEngine s;
    return s;
}

AlphaArenaEngine::AlphaArenaEngine() = default;
AlphaArenaEngine::~AlphaArenaEngine() = default;

void AlphaArenaEngine::init() {
    if (inited_) return;
    inited_ = true;

    clock_      = new TickClock(this);
    dispatcher_ = new ModelDispatcher(this);
    router_     = new OrderRouter(this);
    paper_venue_ = new PaperVenue(this);

    // Wire components together. Engine stays in the middle so the UI only
    // ever subscribes to engine signals (one reference per widget).
    connect(clock_, &TickClock::tick, this, &AlphaArenaEngine::on_tick);
    connect(clock_, &TickClock::tick_skipped, this, &AlphaArenaEngine::on_tick_skipped);

    connect(dispatcher_, &ModelDispatcher::tick_dispatch_complete,
            this, &AlphaArenaEngine::on_dispatch_complete);
    connect(dispatcher_, &ModelDispatcher::decision_received,
            this, &AlphaArenaEngine::on_decision_received);
    connect(dispatcher_, &ModelDispatcher::decision_received,
            router_, &OrderRouter::on_decision);

    connect(router_, &OrderRouter::action_evaluated,
            this, [this](const QString& agent_id, const QString& coin,
                         fincept::services::alpha_arena::Signal sig,
                         int risk_outcome, const QString& reason) {
                emit action_evaluated(agent_id, coin, static_cast<int>(sig),
                                      risk_outcome, reason);
            });
    connect(router_, &OrderRouter::order_placed,
            this, &AlphaArenaEngine::order_placed);
    connect(router_, &OrderRouter::hitl_requested,
            this, &AlphaArenaEngine::hitl_requested);
    connect(router_, &OrderRouter::agent_circuit_open,
            this, &AlphaArenaEngine::on_circuit_open);

    // Crash recovery: surface any competitions left in `running` status when
    // the app died. UI shows a Resume / Halt / Reconcile dialog.
    auto orphans = AlphaArenaRepo::instance().find_orphaned_running();
    if (orphans.is_ok() && !orphans.value().isEmpty()) {
        QStringList ids;
        for (const auto& c : orphans.value()) ids << c.id;
        LOG_WARN("AlphaArena.Engine",
                 QString("crash recovery: %1 orphaned running competition(s)").arg(ids.size()));
        emit crash_recovery_pending(ids);
    }

    LOG_INFO("AlphaArena.Engine", "init complete");
}

// ─── User-facing actions ────────────────────────────────────────────────────

QString AlphaArenaEngine::deterministic_agent_id(const QString& competition_id, int slot) {
    const QString seed = competition_id + QStringLiteral(":") + QString::number(slot);
    return QString::fromLatin1(
        QCryptographicHash::hash(seed.toUtf8(), QCryptographicHash::Sha1).toHex().left(32));
}

Result<QString> AlphaArenaEngine::create_competition(NewCompetitionConfig cfg) {
    if (!inited_) return Result<QString>::err("engine not initialised");
    if (cfg.agents.size() < 2) return Result<QString>::err("need at least 2 agents");
    if (cfg.cadence_seconds < 10 || cfg.cadence_seconds > 3600)
        return Result<QString>::err("cadence_seconds out of range [10, 3600]");

    if (cfg.instruments.isEmpty()) cfg.instruments = kPerpUniverse();
    for (const auto& s : cfg.instruments) {
        if (!kPerpUniverse().contains(s))
            return Result<QString>::err(("instrument not in perp universe: " + s).toStdString());
    }

    const QString comp_id = new_uuid();

    CompetitionRow row;
    row.id = comp_id;
    row.name = cfg.name.isEmpty() ? QStringLiteral("Alpha Arena") : cfg.name;
    row.mode = cfg.mode;
    row.venue = cfg.venue_kind;
    row.instruments = cfg.instruments;
    row.initial_capital = cfg.initial_capital_per_agent;
    row.cadence_seconds = cfg.cadence_seconds;
    row.live_mode = cfg.live_mode;
    row.status = QStringLiteral("created");

    if (auto r = AlphaArenaRepo::instance().insert_competition(row); r.is_err())
        return Result<QString>::err(r.error());

    if (cfg.live_mode) {
        // Record the live-mode acknowledgement timestamp + hostname. The UI
        // is responsible for collecting the typed-acknowledgement; the engine
        // just records that the user actually got there.
        AlphaArenaRepo::instance().mark_live_mode_ack(comp_id,
                                                      QSysInfo::machineHostName());
    }

    // Persist agents + stash API keys in SecureStorage.
    for (int slot = 0; slot < cfg.agents.size(); ++slot) {
        AgentSpec spec = cfg.agents[slot];
        AgentRow agent;
        agent.id = deterministic_agent_id(comp_id, slot);
        agent.competition_id = comp_id;
        agent.slot = slot;
        agent.provider = spec.provider;
        agent.model_id = spec.model_id;
        agent.display_name = spec.display_name.isEmpty()
                                 ? (spec.provider + ":" + spec.model_id)
                                 : spec.display_name;
        agent.color_hex = spec.color_hex.isEmpty() ? QStringLiteral("#FF8800") : spec.color_hex;
        // Store the API key under a key that IS the secret_handle. The handle
        // is what we hand to the subprocess; the plaintext lives only in
        // SecureStorage.
        const QString secret_handle = QStringLiteral("alpha_arena/api_key/") + agent.id;
        agent.api_secret_handle = secret_handle;
        if (!spec.api_key.isEmpty()) {
            auto sr = SecureStorage::instance().store(secret_handle, spec.api_key);
            if (sr.is_err()) {
                LOG_ERROR("AlphaArena.Engine",
                          QString("could not store API key for agent %1: %2")
                              .arg(agent.id, QString::fromStdString(sr.error())));
                return Result<QString>::err("failed to store api key in keystore");
            }
        }
        if (auto r = AlphaArenaRepo::instance().insert_agent(agent); r.is_err()) {
            return Result<QString>::err(r.error());
        }
    }

    // Audit event.
    QJsonObject p;
    p["mode"] = mode_to_wire(cfg.mode);
    p["venue"] = cfg.venue_kind;
    p["agents"] = cfg.agents.size();
    p["cadence_seconds"] = cfg.cadence_seconds;
    AlphaArenaRepo::instance().append_event(comp_id, QString(),
                                            QStringLiteral("competition_created"), p);
    return Result<QString>::ok(comp_id);
}

Result<void> AlphaArenaEngine::start(const QString& competition_id) {
    if (!inited_) return Result<void>::err("engine not initialised");
    if (!active_competition_id_.isEmpty() && active_competition_id_ != competition_id) {
        return Result<void>::err("another competition is already active");
    }

    auto comp_opt = AlphaArenaRepo::instance().find_competition(competition_id);
    if (!comp_opt) return Result<void>::err("competition not found");
    const auto comp = *comp_opt;

    venue_ = select_venue_for_kind(comp.venue);
    if (!venue_) return Result<void>::err("unsupported venue: " + comp.venue.toStdString());

    auto agents_r = AlphaArenaRepo::instance().agents_for(competition_id);
    if (agents_r.is_err()) return Result<void>::err(agents_r.error());
    const auto agents = agents_r.value();

    QVector<DispatchAgent> dispatch_agents;
    dispatch_agents.reserve(agents.size());
    for (const auto& a : agents) {
        DispatchAgent d;
        d.agent_id = a.id;
        d.provider = a.provider;
        d.model_id = a.model_id;
        d.display_name = a.display_name;
        d.api_secret_handle = a.api_secret_handle;
        dispatch_agents.append(d);

        // Seed the paper book at competition start.
        if (paper_venue_ == venue_) {
            paper_seed_agent(*paper_venue_, a.id, comp.initial_capital);
        }

        // Seed router state.
        AgentRuntimeState s;
        s.agent_id = a.id;
        s.competition_id = competition_id;
        s.equity = comp.initial_capital;
        s.live_mode = comp.live_mode;
        router_->update_agent_state(a.id, s);
    }

    active_competition_id_ = competition_id;
    active_mode_ = comp.mode;

    dispatcher_->configure(competition_id, comp.mode, dispatch_agents);
    router_->configure(competition_id, comp.mode, venue_);
    clock_->set_cadence_seconds(comp.cadence_seconds);
    clock_->start();

    AlphaArenaRepo::instance().update_competition_status(competition_id,
                                                         QStringLiteral("running"));
    AlphaArenaRepo::instance().append_event(competition_id, QString(),
                                            QStringLiteral("competition_started"), {});
    emit competition_status_changed(competition_id, QStringLiteral("running"));

    LOG_INFO("AlphaArena.Engine",
             QString("started competition %1 (mode=%2, venue=%3, %4 agents, cadence=%5s)")
                 .arg(competition_id, mode_to_wire(comp.mode), comp.venue)
                 .arg(agents.size())
                 .arg(comp.cadence_seconds));
    return Result<void>::ok();
}

Result<void> AlphaArenaEngine::stop() {
    if (active_competition_id_.isEmpty()) return Result<void>::ok();
    clock_->stop();
    AlphaArenaRepo::instance().update_competition_status(active_competition_id_,
                                                         QStringLiteral("halted_by_user"));
    AlphaArenaRepo::instance().append_event(active_competition_id_, QString(),
                                            QStringLiteral("competition_stopped"), {});
    emit competition_status_changed(active_competition_id_, QStringLiteral("halted_by_user"));
    detach_active();
    return Result<void>::ok();
}

Result<void> AlphaArenaEngine::kill_all() {
    if (active_competition_id_.isEmpty()) return Result<void>::ok();
    clock_->stop();

    auto agents_r = AlphaArenaRepo::instance().agents_for(active_competition_id_);
    if (agents_r.is_ok() && venue_) {
        for (const auto& a : agents_r.value()) {
            auto positions_r = AlphaArenaRepo::instance().open_positions_for(a.id);
            if (positions_r.is_err()) continue;
            for (const auto& p : positions_r.value()) {
                OrderRequest req;
                req.agent_id = a.id;
                req.coin = p.coin;
                req.qty = std::fabs(p.qty);
                req.side = (p.qty > 0.0) ? QStringLiteral("sell") : QStringLiteral("buy");
                req.leverage = p.leverage;
                req.reduce_only = true;
                venue_->place_order(req, [](OrderAck) {});
            }
        }
    }

    AlphaArenaRepo::instance().update_competition_status(active_competition_id_,
                                                         QStringLiteral("halted_by_user"));
    AlphaArenaRepo::instance().append_event(active_competition_id_, QString(),
                                            QStringLiteral("kill_all"), {});
    emit competition_status_changed(active_competition_id_, QStringLiteral("halted_by_user"));
    detach_active();
    return Result<void>::ok();
}

Result<void> AlphaArenaEngine::halt_agent(const QString& agent_id) {
    if (active_competition_id_.isEmpty()) return Result<void>::err("no active competition");
    AlphaArenaRepo::instance().update_agent_state(agent_id, QStringLiteral("paused"));
    QJsonObject p; p["agent_id"] = agent_id;
    AlphaArenaRepo::instance().append_event(active_competition_id_, agent_id,
                                            QStringLiteral("agent_halted"), p);
    return Result<void>::ok();
}

Result<void> AlphaArenaEngine::resume_agent(const QString& agent_id) {
    if (active_competition_id_.isEmpty()) return Result<void>::err("no active competition");
    AlphaArenaRepo::instance().update_agent_state(agent_id, QStringLiteral("active"));
    AlphaArenaRepo::instance().bump_parse_failures(agent_id, 0);
    AlphaArenaRepo::instance().bump_risk_rejects(agent_id, 0);
    QJsonObject p; p["agent_id"] = agent_id;
    AlphaArenaRepo::instance().append_event(active_competition_id_, agent_id,
                                            QStringLiteral("agent_resumed"), p);
    return Result<void>::ok();
}

void AlphaArenaEngine::resume_after_hitl(const QString& approval_id, bool approved) {
    if (router_) router_->resolve_hitl(approval_id, approved);
}

bool AlphaArenaEngine::is_running() const {
    return clock_ && !active_competition_id_.isEmpty() && clock_->in_flight() == false
           ? clock_->last_seq() > 0
           : (clock_ && !active_competition_id_.isEmpty());
}

int AlphaArenaEngine::next_tick_in_seconds() const {
    return clock_ ? clock_->cadence_seconds() : 0;
}

IExchangeVenue::ConnectionState AlphaArenaEngine::venue_connection_state() const {
    return venue_ ? venue_->connection_state() : IExchangeVenue::ConnectionState::Disconnected;
}

// ─── Internal slots ─────────────────────────────────────────────────────────

void AlphaArenaEngine::on_tick(Tick t) {
    if (active_competition_id_.isEmpty()) {
        clock_->tick_complete();
        return;
    }
    emit tick_fired(t.seq);

    TickContext ctx;
    QHash<QString, AgentAccountState> per_agent;
    std::optional<SituationalContext> situational;
    prepare_tick_inputs(t, ctx, per_agent, situational);

    AlphaArenaRepo::instance().increment_cycle_count(active_competition_id_);
    dispatcher_->dispatch_tick(t, ctx, per_agent, situational);
}

void AlphaArenaEngine::on_tick_skipped(int seq) {
    LOG_WARN("AlphaArena.Engine", QString("tick %1 skipped (previous in flight)").arg(seq));
    if (!active_competition_id_.isEmpty()) {
        QJsonObject p; p["seq"] = seq;
        AlphaArenaRepo::instance().append_event(active_competition_id_, QString(),
                                                QStringLiteral("tick_skipped"), p);
    }
    emit tick_skipped(seq);
}

void AlphaArenaEngine::on_dispatch_complete(int seq) {
    Q_UNUSED(seq);
    if (clock_) clock_->tick_complete();
}

void AlphaArenaEngine::on_decision_received(QString decision_id, QString agent_id,
                                            QVector<ProposedAction> actions,
                                            QString parse_error) {
    Q_UNUSED(actions);
    Q_UNUSED(parse_error);
    emit decision_received(decision_id, agent_id);
}

void AlphaArenaEngine::on_circuit_open(QString agent_id, QString reason) {
    AlphaArenaRepo::instance().update_agent_state(agent_id, QStringLiteral("circuit_open"));
    QJsonObject p;
    p["agent_id"] = agent_id;
    p["reason"] = reason;
    AlphaArenaRepo::instance().append_event(active_competition_id_, agent_id,
                                            QStringLiteral("agent_circuit_open"), p);
    emit agent_circuit_open(agent_id, reason);
}

// ─── Helpers ────────────────────────────────────────────────────────────────

void AlphaArenaEngine::prepare_tick_inputs(const Tick& t,
                                            TickContext& ctx_out,
                                            QHash<QString, AgentAccountState>& per_agent_out,
                                            std::optional<SituationalContext>& situational_out) {
    ctx_out.tick = t;

    auto comp_opt = AlphaArenaRepo::instance().find_competition(active_competition_id_);
    if (!comp_opt) return;
    const auto comp = *comp_opt;

    // Markets — minimal snapshot. In v1 the engine seeds bid/ask/mid from the
    // venue's last_mark; bars + indicators are populated by a future
    // MarketDataIngestor (Phase 5c builds the Hyperliquid public-WS feeder).
    // For paper-mode bring-up, we degrade gracefully: empty bar arrays mean
    // the model sees only the snapshot fields.
    for (const auto& coin : comp.instruments) {
        MarketSnapshot m;
        m.coin = coin;
        const double mark = venue_ ? venue_->last_mark(coin) : 0.0;
        if (std::isfinite(mark) && mark > 0.0) {
            m.mid = mark;
            m.bid = mark * 0.9999;
            m.ask = mark * 1.0001;
        }
        ctx_out.markets.append(m);
    }

    // Per-agent account state.
    auto agents_r = AlphaArenaRepo::instance().agents_for(active_competition_id_);
    if (agents_r.is_err()) return;
    for (const auto& a : agents_r.value()) {
        AgentAccountState st;
        if (paper_venue_ == venue_) {
            st.equity = paper_venue_->equity_for(a.id);
        } else {
            st.equity = comp.initial_capital;
        }
        // Open positions from repo.
        auto pr = AlphaArenaRepo::instance().open_positions_for(a.id);
        if (pr.is_ok()) st.positions = pr.value();
        // Cash approximation: equity − unrealized PnL on open positions.
        double upnl = 0.0;
        for (const auto& p : st.positions) upnl += (p.mark - p.entry) * p.qty;
        st.cash = st.equity - upnl;
        per_agent_out.insert(a.id, st);
    }

    // Situational mode populates peer view (no equity / pnl disclosed).
    if (active_mode_ == CompetitionMode::Situational) {
        SituationalContext sc;
        for (const auto& a : agents_r.value()) {
            const auto pr = AlphaArenaRepo::instance().open_positions_for(a.id);
            if (pr.is_err()) continue;
            for (const auto& p : pr.value()) {
                PeerPositionView pv;
                pv.agent_display_name = a.display_name;
                pv.coin = p.coin;
                pv.qty = p.qty;
                pv.leverage = p.leverage;
                sc.peers.append(pv);
            }
        }
        situational_out = sc;
    }
}

void AlphaArenaEngine::detach_active() {
    active_competition_id_.clear();
    venue_ = nullptr;  // we don't delete; PaperVenue stays alive for reuse
}

IExchangeVenue* AlphaArenaEngine::select_venue_for_kind(const QString& venue_kind) {
    if (venue_kind == QLatin1String("paper")) return paper_venue_;
    // hyperliquid path lands in Phase 5c.
    return nullptr;
}

} // namespace fincept::services::alpha_arena
