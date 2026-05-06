#include "services/alpha_arena/PaperVenue.h"

#include "core/logging/Logger.h"

#include <QDateTime>
#include <QString>
#include <QTimer>

#include <cmath>

namespace fincept::services::alpha_arena {

namespace {

/// Hyperliquid taker fee, S1.
constexpr double kTakerFee = 0.00045;
/// 5bps slippage each side.
constexpr double kSlippageBps = 0.0005;
/// Simulated round-trip latency (ms).
constexpr int kSimLatencyMs = 200;

inline double sign(double x) { return (x > 0) - (x < 0); }

QString gen_venue_order_id(quint64 seq) {
    return QStringLiteral("paper-%1").arg(seq);
}

} // namespace

PaperVenue::PaperVenue(QObject* parent) : QObject(parent) {
}

double PaperVenue::last_mark(const QString& coin) const {
    auto it = marks_.constFind(coin);
    return (it != marks_.constEnd()) ? it.value() : std::nan("");
}

void PaperVenue::set_maintenance_margin(const QString& coin, double frac) {
    mm_overrides_.insert(coin, frac);
}

void PaperVenue::seed_agent_if_needed(const QString& agent_id, double initial_capital) {
    if (!books_.contains(agent_id)) {
        AgentBook book;
        book.cash = initial_capital;
        books_.insert(agent_id, book);
    }
}

double PaperVenue::equity_for(const QString& agent_id) const {
    auto it = books_.constFind(agent_id);
    if (it == books_.constEnd()) return 0.0;
    const AgentBook& book = it.value();
    double eq = book.cash;
    for (auto pit = book.positions.constBegin(); pit != book.positions.constEnd(); ++pit) {
        const auto& p = pit.value();
        // Unrealized PnL on a perp: qty × (mark − entry).
        eq += p.qty * (p.mark - p.entry);
    }
    return eq;
}

double PaperVenue::liquidation_price(const PaperPosition& p) {
    if (p.entry <= 0.0 || p.leverage < 1 || p.qty == 0.0) return 0.0;
    const bool is_long = p.qty > 0.0;
    const double term = (1.0 / p.leverage) * (1.0 - p.maintenance_margin_frac);
    return is_long ? p.entry * (1.0 - term) : p.entry * (1.0 + term);
}

// ── Order placement ─────────────────────────────────────────────────────────

void PaperVenue::place_order(const OrderRequest& req,
                             std::function<void(OrderAck)> ack_cb) {
    // Snapshot mark at intake — slippage + fee applied off this. We assume the
    // engine's caller has already validated the request through RiskEngine, so
    // we accept and execute on the simulated round-trip timer.
    const double mark = last_mark(req.coin);
    if (!std::isfinite(mark) || mark <= 0.0) {
        if (ack_cb) {
            OrderAck a;
            a.status = QStringLiteral("rejected");
            a.error = QStringLiteral("no mark price for ") + req.coin;
            ack_cb(a);
        }
        return;
    }

    const QString id = gen_venue_order_id(next_order_seq_++);

    // Synthesise fill price: mark ± slippage in the unfavourable direction.
    const bool is_buy = (req.side == QLatin1String("buy"));
    const double fill_price = is_buy ? mark * (1.0 + kSlippageBps)
                                      : mark * (1.0 - kSlippageBps);
    const double notional = req.qty * fill_price;
    const double fee = notional * kTakerFee;

    // Acknowledge immediately.
    if (ack_cb) {
        OrderAck a;
        a.venue_order_id = id;
        a.status = QStringLiteral("accepted");
        ack_cb(a);
    }

    // Schedule the simulated fill after latency.
    QTimer::singleShot(kSimLatencyMs, this, [this, req, id, fill_price, fee]() {
        // Apply to the agent's book.
        // We assume the agent was seeded externally via paper_seed_agent.
        auto bookIt = books_.find(req.agent_id);
        if (bookIt == books_.end()) {
            LOG_ERROR("PaperVenue", QString("fill on unknown agent %1 (was paper_seed_agent called?)")
                                        .arg(req.agent_id));
            return;
        }
        AgentBook& book = bookIt.value();

        const bool is_buy = (req.side == QLatin1String("buy"));
        const double signed_qty = is_buy ? req.qty : -req.qty;

        auto posIt = book.positions.find(req.coin);
        if (req.reduce_only) {
            // Closing or reducing — settle realized PnL into cash.
            if (posIt == book.positions.end()) {
                LOG_WARN("PaperVenue", QString("reduce_only but no position for %1/%2")
                                           .arg(req.agent_id, req.coin));
                return;
            }
            PaperPosition& p = posIt.value();
            const double close_qty = std::min(std::fabs(p.qty), req.qty);
            const double realized = sign(p.qty) * close_qty * (fill_price - p.entry);
            book.cash += realized;
            book.cash -= fee;
            p.qty -= sign(p.qty) * close_qty;
            if (std::fabs(p.qty) < 1e-12) {
                book.positions.remove(req.coin);
            }
        } else {
            // Open a new position. RiskEngine guarantees no existing position in this coin.
            PaperPosition p;
            p.agent_id = req.agent_id;
            p.coin = req.coin;
            p.qty = signed_qty;
            p.entry = fill_price;
            p.mark = fill_price;
            p.leverage = req.leverage;
            p.profit_target = req.profit_target;
            p.stop_loss = req.stop_loss;
            const auto mmIt = mm_overrides_.constFind(req.coin);
            p.maintenance_margin_frac = (mmIt != mm_overrides_.constEnd()) ? mmIt.value() : 0.05;
            book.positions.insert(req.coin, p);
            // Margin bookkeeping: notional / leverage is locked from cash; we
            // model that simply by debiting the fee — the simulator treats
            // margin as a soft constraint visible only through liquidation.
            book.cash -= fee;
        }

        if (fill_cb_) {
            FillEvent f;
            f.agent_id = req.agent_id;
            f.venue_order_id = id;
            f.coin = req.coin;
            f.side = req.side;
            f.qty = req.qty;
            f.price = fill_price;
            f.fee = fee;
            f.utc_ms = QDateTime::currentMSecsSinceEpoch();
            fill_cb_(f);
        }
    });
}

void PaperVenue::cancel_order(const QString& /*venue_order_id*/) {
    // Paper orders execute on a 200ms timer — we don't keep a pending list.
    // No-op is correct: by the time a cancel arrives, the fill has already
    // landed (or there was no pending order to cancel).
}

// ── Mark feed + liquidations ────────────────────────────────────────────────

void PaperVenue::push_mark(const QString& coin, double mark, qint64 utc_ms) {
    if (!std::isfinite(mark) || mark <= 0.0) return;
    marks_.insert(coin, mark);

    // Update each agent's mark on this coin.
    for (auto bit = books_.begin(); bit != books_.end(); ++bit) {
        auto pit = bit.value().positions.find(coin);
        if (pit != bit.value().positions.end()) pit.value().mark = mark;
    }

    if (mark_update_cb_) mark_update_cb_(coin, mark, utc_ms);
    check_liquidations(coin, mark, utc_ms);
}

void PaperVenue::check_liquidations(const QString& coin, double mark, qint64 utc_ms) {
    // Walk every agent's position on this coin. If mark crossed liq_price,
    // emit a LiquidationEvent and drop the position with a realized PnL of
    // -margin (engine logs this as "liquidation").
    QStringList to_remove_agent;
    for (auto bit = books_.begin(); bit != books_.end(); ++bit) {
        auto& book = bit.value();
        auto pit = book.positions.find(coin);
        if (pit == book.positions.end()) continue;
        PaperPosition& p = pit.value();
        const double liq = liquidation_price(p);
        if (liq <= 0.0) continue;
        const bool is_long = p.qty > 0.0;
        const bool crossed = is_long ? (mark <= liq) : (mark >= liq);
        if (!crossed) continue;

        // Realized PnL — at liquidation, the position closes at liq_price.
        const double realized = sign(p.qty) * std::fabs(p.qty) * (liq - p.entry);
        book.cash += realized;
        // Margin lost: roughly notional/leverage. We charge it implicitly by
        // letting equity drop through realized PnL — Hyperliquid's actual
        // engine takes maintenance margin into account; this is an
        // approximation acceptable for paper.

        if (liq_cb_) {
            LiquidationEvent e;
            e.agent_id = bit.key();
            e.coin = coin;
            e.exit_price = liq;
            e.realized_pnl_usd = realized;
            e.utc_ms = utc_ms;
            liq_cb_(e);
        }
        book.positions.remove(coin);
    }
}

// ── Funding ─────────────────────────────────────────────────────────────────

void PaperVenue::push_funding(const QString& agent_id, const QString& coin,
                              double amount_usd, qint64 utc_ms) {
    auto bit = books_.find(agent_id);
    if (bit == books_.end()) return;
    bit.value().cash += amount_usd;
    if (funding_cb_) {
        FundingEvent f;
        f.agent_id = agent_id;
        f.coin = coin;
        f.amount_usd = amount_usd;
        f.utc_ms = utc_ms;
        funding_cb_(f);
    }
}

// ── External seeding ────────────────────────────────────────────────────────

void paper_seed_agent(PaperVenue& venue, const QString& agent_id, double initial_capital) {
    venue.seed_agent_if_needed(agent_id, initial_capital);
}

} // namespace fincept::services::alpha_arena
