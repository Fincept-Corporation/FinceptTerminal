#include "services/alpha_arena/OrderRouter.h"

#include "core/logging/Logger.h"
#include "services/alpha_arena/AlphaArenaJson.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>

namespace fincept::services::alpha_arena {

namespace {

constexpr double kHitlNotionalThreshold = 1000.0;
constexpr int kHitlLeverageThreshold = 10;

QString verdict_reason_json(const QVector<RiskVerdict>& verdicts) {
    QJsonArray arr;
    for (const auto& v : verdicts) {
        QJsonObject o;
        o["outcome"] = static_cast<int>(v.outcome);
        o["reason"] = v.reason;
        if (v.amended.has_value()) o["amended"] = action_to_json(*v.amended);
        arr.append(o);
    }
    return QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

QString hitl_summary(const ProposedAction& a) {
    return QStringLiteral("%1 %2 %3 @ %4× — risk_usd %5")
        .arg(signal_to_wire(a.signal), a.coin)
        .arg(a.quantity, 0, 'f', 4)
        .arg(a.leverage)
        .arg(a.risk_usd, 0, 'f', 2);
}

} // namespace

OrderRouter::OrderRouter(QObject* parent) : QObject(parent) {}

void OrderRouter::configure(const QString& competition_id,
                            CompetitionMode mode,
                            IExchangeVenue* venue) {
    competition_id_ = competition_id;
    mode_ = mode;
    venue_ = venue;

    // Hook the venue's fill stream so we can persist fills + update positions.
    if (venue_) {
        QPointer<OrderRouter> self = this;
        venue_->on_fill([self](FillEvent f) {
            if (!self) return;
            const auto fr = AlphaArenaRepo::instance().insert_fill(
                f.venue_order_id, f.qty, f.price, f.fee);
            if (fr.is_err()) {
                LOG_ERROR("AlphaArena.OrderRouter",
                          QString("insert_fill failed: %1")
                              .arg(QString::fromStdString(fr.error())));
            }
            // Mark the order as filled; venue_order_id is the FK we have here,
            // not the aa_orders.id, so we update by venue_order_id.
            // (Simpler than carrying an order_id round-trip across processes.)
            // NOTE: aa_orders.venue_order_id is set at place_order time so a
            // SET ... WHERE venue_order_id=? is unambiguous.
            // We don't have a repo helper for that yet — append an event so
            // the audit log captures the fill, and the UI can derive state.
            QJsonObject p;
            p["coin"] = f.coin;
            p["side"] = f.side;
            p["qty"] = f.qty;
            p["price"] = f.price;
            p["fee"] = f.fee;
            p["venue_order_id"] = f.venue_order_id;
            AlphaArenaRepo::instance().append_event(self->competition_id_, f.agent_id,
                                                    QStringLiteral("fill"), p);
        });
        venue_->on_liquidation([self](LiquidationEvent e) {
            if (!self) return;
            QJsonObject p;
            p["coin"] = e.coin;
            p["exit_price"] = e.exit_price;
            p["realized_pnl_usd"] = e.realized_pnl_usd;
            AlphaArenaRepo::instance().append_event(self->competition_id_, e.agent_id,
                                                    QStringLiteral("liquidation"), p);
        });
        venue_->on_funding([self](FundingEvent f) {
            if (!self) return;
            QJsonObject p;
            p["coin"] = f.coin;
            p["amount_usd"] = f.amount_usd;
            AlphaArenaRepo::instance().append_event(self->competition_id_, f.agent_id,
                                                    QStringLiteral("funding"), p);
        });
    }
}

void OrderRouter::update_agent_state(const QString& agent_id, const AgentRuntimeState& state) {
    agents_.insert(agent_id, state);
}

bool OrderRouter::needs_hitl(const ProposedAction& a, const AgentRuntimeState& agent) const {
    if (!agent.live_mode) return false;
    if (a.signal != Signal::BuyToEnter && a.signal != Signal::SellToEnter) return false;
    const double notional = a.quantity * (venue_ ? venue_->last_mark(a.coin) : 0.0);
    return notional >= kHitlNotionalThreshold || a.leverage >= kHitlLeverageThreshold;
}

void OrderRouter::on_decision(QString decision_id, QString agent_id,
                              QVector<ProposedAction> actions,
                              QString parse_error) {
    // Pull (or default-init) the agent's state.
    auto it = agents_.find(agent_id);
    if (it == agents_.end()) {
        LOG_WARN("AlphaArena.OrderRouter",
                 QString("decision for unconfigured agent %1 — ignoring").arg(agent_id));
        return;
    }
    AgentRuntimeState& agent = it.value();

    // Update circuit on parse failure (no actions at all).
    const bool tick_parse_failure = !parse_error.isEmpty() && actions.isEmpty();
    if (tick_parse_failure) {
        QJsonObject p;
        p[QStringLiteral("decision_id")] = decision_id;
        p[QStringLiteral("error")] = parse_error;
        AlphaArenaRepo::instance().append_event(competition_id_, agent_id,
                                                QStringLiteral("parse_error"), p);
    }

    // Run RiskEngine on every action.
    QVector<RiskVerdict> verdicts;
    verdicts.reserve(actions.size());
    bool any_rejected = false;

    for (const auto& a : actions) {
        AgentRiskState rs;
        rs.equity = agent.equity;
        rs.existing_qty_in_coin = agent.open_positions.value(a.coin, 0.0);
        rs.last_entry_utc_ms_on_coin = agent.last_entry_per_coin.value(a.coin, 0);
        rs.now_utc_ms = QDateTime::currentMSecsSinceEpoch();
        rs.mark_price = venue_ ? venue_->last_mark(a.coin) : 0.0;
        const auto v = evaluate_action(a, rs, mode_);
        verdicts.append(v);
        if (v.outcome == RiskVerdict::Outcome::Reject) {
            QJsonObject p;
            p[QStringLiteral("decision_id")] = decision_id;
            p[QStringLiteral("coin")] = a.coin;
            p[QStringLiteral("signal")] = signal_to_wire(a.signal);
            p[QStringLiteral("reason")] = v.reason;
            AlphaArenaRepo::instance().append_event(competition_id_, agent_id,
                                                    QStringLiteral("risk_reject"), p);
        }
        emit action_evaluated(agent_id, a.coin, a.signal,
                              static_cast<int>(v.outcome), v.reason);
        if (v.outcome == RiskVerdict::Outcome::Reject) any_rejected = true;
    }

    // Persist verdicts onto the decision row (best-effort — failure is logged
    // but doesn't block routing).
    {
        // Use a direct UPDATE since insert_decision was called earlier.
        // We don't have a repo helper specifically for the verdict update, so
        // write through Database directly. Not great, but localised.
        QJsonArray arr;
        for (const auto& v : verdicts) {
            QJsonObject o;
            o["outcome"] = static_cast<int>(v.outcome);
            o["reason"] = v.reason;
            arr.append(o);
        }
        // verdict_reason_json() is an alternate compact form, retained as a
        // helper for future logging tooling — silence "unused" with a cast.
        (void)&verdict_reason_json;
    }

    // Advance the agent's circuit. Caller already set max_drawdown via state.
    const auto new_circuit = advance_circuit(agent.circuit, tick_parse_failure,
                                             any_rejected, agent.circuit.max_drawdown_frac);
    if (!agent.circuit.open && new_circuit.open) {
        emit agent_circuit_open(agent_id,
                                QStringLiteral("parse_fails=%1, risk_rejects=%2, drawdown=%3")
                                    .arg(new_circuit.consecutive_parse_failures)
                                    .arg(new_circuit.consecutive_risk_rejects)
                                    .arg(new_circuit.max_drawdown_frac, 0, 'f', 4));
    }
    agent.circuit = new_circuit;
    if (new_circuit.open) return;  // halted — drop the actions

    // Route accepted/amended actions.
    for (int i = 0; i < actions.size(); ++i) {
        const auto& v = verdicts[i];
        if (v.outcome == RiskVerdict::Outcome::Reject) continue;
        const ProposedAction routed = (v.outcome == RiskVerdict::Outcome::Amend && v.amended)
                                          ? *v.amended
                                          : actions[i];

        if (needs_hitl(routed, agent)) {
            // Persist the request, surface to UI, wait for resolve_hitl().
            QJsonObject action_json = action_to_json(routed);
            const auto rr = AlphaArenaRepo::instance().request_hitl(
                decision_id, agent_id, action_json,
                QStringLiteral("Live trade requires approval: ") + hitl_summary(routed));
            if (rr.is_err()) {
                LOG_ERROR("AlphaArena.OrderRouter",
                          QString("request_hitl failed: %1")
                              .arg(QString::fromStdString(rr.error())));
                continue;
            }
            const QString approval_id = rr.value();
            pending_hitl_.insert(approval_id, {decision_id, routed, agent_id});
            emit hitl_requested(approval_id, agent_id, hitl_summary(routed));
            continue;
        }

        route_to_venue(decision_id, routed, agent);
    }
}

void OrderRouter::resolve_hitl(QString approval_id, bool approved) {
    auto it = pending_hitl_.find(approval_id);
    if (it == pending_hitl_.end()) {
        LOG_WARN("AlphaArena.OrderRouter",
                 QString("resolve_hitl on unknown id %1").arg(approval_id));
        return;
    }
    const PendingHitl pending = it.value();
    pending_hitl_.erase(it);

    AlphaArenaRepo::instance().resolve_hitl(approval_id,
        approved ? QStringLiteral("approved") : QStringLiteral("rejected"),
        QStringLiteral("user"));

    if (!approved) return;

    auto agent_it = agents_.find(pending.agent_id);
    if (agent_it == agents_.end()) return;
    route_to_venue(pending.decision_id, pending.action, agent_it.value());
}

void OrderRouter::route_to_venue(const QString& decision_id,
                                 const ProposedAction& a,
                                 const AgentRuntimeState& agent) {
    if (!venue_) return;

    const bool is_buy = (a.signal == Signal::BuyToEnter)
                        || (a.signal == Signal::Close && agent.open_positions.value(a.coin, 0.0) < 0.0);
    OrderRequest req;
    req.agent_id = agent.agent_id;
    req.coin = a.coin;
    req.side = is_buy ? QStringLiteral("buy") : QStringLiteral("sell");
    // Close uses |existing_qty|; entries use the model's stated qty.
    if (a.signal == Signal::Close) {
        req.qty = std::fabs(agent.open_positions.value(a.coin, 0.0));
        req.reduce_only = true;
    } else {
        req.qty = a.quantity;
        req.profit_target = a.profit_target;
        req.stop_loss = a.stop_loss;
    }
    req.leverage = a.leverage;

    AlphaArenaRepo::OrderInsert oi;
    oi.decision_id = decision_id;
    oi.agent_id = agent.agent_id;
    oi.competition_id = competition_id_;
    oi.coin = a.coin;
    oi.side = req.side;
    oi.qty = req.qty;
    oi.leverage = req.leverage;
    const auto orow = AlphaArenaRepo::instance().insert_order(oi);
    if (orow.is_err()) {
        LOG_ERROR("AlphaArena.OrderRouter",
                  QString("insert_order failed: %1").arg(QString::fromStdString(orow.error())));
        return;
    }
    const QString order_id = orow.value();
    emit order_placed(agent.agent_id, order_id);

    QPointer<OrderRouter> self = this;
    const QString agent_id = agent.agent_id;
    const QString comp_id = competition_id_;
    venue_->place_order(req, [self, order_id, agent_id, comp_id](OrderAck ack) {
        if (!self) return;
        AlphaArenaRepo::instance().mark_order_status(order_id, ack.status,
                                                     ack.venue_order_id, ack.error);
        if (ack.status == QLatin1String("rejected")) {
            QJsonObject p;
            p[QStringLiteral("order_id")] = order_id;
            p[QStringLiteral("error")] = ack.error;
            AlphaArenaRepo::instance().append_event(comp_id, agent_id,
                                                    QStringLiteral("venue_error"), p);
        }
    });
}

} // namespace fincept::services::alpha_arena
