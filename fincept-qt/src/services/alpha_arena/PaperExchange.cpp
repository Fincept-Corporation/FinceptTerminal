// PaperExchange — isolated-margin perp simulator per agent (see PaperExchange.h).
#include "services/alpha_arena/PaperExchange.h"
#include "core/logging/Logger.h"
#include <QDateTime>
#include <cmath>

namespace fincept::arena {
namespace {
// I2: store writes are best-effort in the hot path, but failures must be visible.
template <typename T>
void log_store_err(const Result<T>& r, const char* what) {
    if (r.is_err())
        LOG_WARN("PaperExchange", QStringLiteral("store %1 failed: %2")
                                      .arg(QString::fromUtf8(what), QString::fromStdString(r.error())));
}
} // namespace

PaperExchange::PaperExchange(ArenaStore& store, QString competition_id)
    : store_(store), competition_id_(std::move(competition_id)) {}

void PaperExchange::load() {
    balances_.clear(); positions_.clear();
    auto agents = store_.agents_for(competition_id_);
    if (agents.is_err()) return;
    for (const auto& a : agents.value()) {
        auto b = store_.account_balance(competition_id_, a.id);
        balances_[a.id] = b.is_ok() ? b.value() : 0.0;
        auto ps = store_.positions_for(competition_id_, a.id);
        positions_[a.id] = ps.is_ok() ? ps.value() : QVector<PositionRow>{};
    }
}

void PaperExchange::set_marks(const QHash<QString, double>& mids) { for (auto it = mids.begin(); it != mids.end(); ++it) marks_[it.key()] = it.value(); }
void PaperExchange::set_funding(const QHash<QString, double>& rates) { funding_rates_ = rates; }

double PaperExchange::position_upnl(const PositionRow& p, double mk) const {
    const double dir = p.side == QLatin1String("long") ? 1.0 : -1.0;
    return (mk - p.entry_price) * p.qty * dir - p.funding_accrued;
}

AccountView PaperExchange::account(const QString& agent_id) const {
    AccountView v;
    v.balance = balances_.value(agent_id, 0.0);
    v.equity = v.balance;
    v.positions = positions_.value(agent_id);
    for (const auto& p : v.positions) {
        const double margin = p.qty * p.entry_price / p.leverage;
        const double up = position_upnl(p, mark(p.coin));
        v.margin_used += margin;
        v.upnl[p.coin] = up;
        v.equity += margin + up;
    }
    return v;
}

Result<OrderRow> PaperExchange::execute(const QString& agent_id, const AgentAction& a, int round_seq) {
    if (a.kind == ActionKind::Hold) return Result<OrderRow>::err("hold is not an order");
    const double mk = mark(a.coin);
    if (mk <= 0) return Result<OrderRow>::err("no mark price for " + a.coin.toStdString());
    auto& book = positions_[agent_id];
    int idx = -1;
    for (int i = 0; i < book.size(); ++i) if (book[i].coin == a.coin) { idx = i; break; }

    if (a.kind == ActionKind::Close) {
        if (idx < 0) return Result<OrderRow>::err("no open position in " + a.coin.toStdString());
        const double notional_at_mark = book[idx].qty * mk;
        const double fraction = a.size_usd > 0 ? std::min(1.0, a.size_usd / notional_at_mark) : 1.0;
        OrderRow o = close_position(agent_id, book[idx], fraction, round_seq, "filled", a.reason);
        if (book[idx].qty * mk < 0.01) book.removeAt(idx);   // sub-cent residual = dust
        persist(agent_id);
        return Result<OrderRow>::ok(o);
    }

    // Open path.
    if (a.size_usd <= 0 || a.leverage < 1)
        return Result<OrderRow>::err("invalid order parameters");

    const double fill = mk * (a.side == QLatin1String("long") ? 1.0 + kSlippage : 1.0 - kSlippage);
    const double qty = a.size_usd / fill;
    const double margin = a.size_usd / a.leverage;
    const double fee = a.size_usd * kTakerFee;

    // Opposite side present → flip (full close first). C1: the flip is atomic —
    // reject up-front if the new leg won't be affordable after the close credit,
    // so a rejection leaves balances/positions/store completely untouched.
    if (idx >= 0 && book[idx].side != a.side) {
        const PositionRow& p = book[idx];
        const double cfill = mk * (p.side == QLatin1String("long") ? 1.0 - kSlippage : 1.0 + kSlippage);
        const double cdir = p.side == QLatin1String("long") ? 1.0 : -1.0;
        const double crealized = (cfill - p.entry_price) * p.qty * cdir - p.funding_accrued;
        const double cmargin_back = p.qty * p.entry_price / p.leverage;
        const double cfee = p.qty * cfill * kTakerFee;
        const double projected = balances_[agent_id] + (cmargin_back + crealized - cfee);
        if (projected < margin + fee)
            return Result<OrderRow>::err("insufficient balance for flip");
        close_position(agent_id, book[idx], 1.0, round_seq, "filled", QStringLiteral("flip"));
        book.removeAt(idx); idx = -1;
    }
    if (balances_[agent_id] < margin + fee)
        return Result<OrderRow>::err("insufficient balance");
    balances_[agent_id] -= margin + fee;

    if (idx >= 0) {   // same-side increase: weighted entry, summed margin via recomputed leverage
        PositionRow& p = book[idx];
        const double old_margin = p.qty * p.entry_price / p.leverage;
        const double new_qty = p.qty + qty;
        p.entry_price = (p.entry_price * p.qty + fill * qty) / new_qty;
        p.qty = new_qty;
        p.leverage = (p.qty * p.entry_price) / (old_margin + margin);
    } else {
        PositionRow p; p.competition_id = competition_id_; p.agent_id = agent_id;
        p.coin = a.coin; p.side = a.side; p.qty = qty; p.entry_price = fill; p.leverage = a.leverage;
        book.append(p);
    }
    persist(agent_id);

    OrderRow o; o.competition_id = competition_id_; o.agent_id = agent_id; o.round_seq = round_seq;
    o.coin = a.coin; o.side = a.side; o.action = "open"; o.qty = qty; o.price = fill;
    o.notional_usd = a.size_usd; o.leverage = a.leverage; o.fee = fee; o.status = "filled";
    log_store_err(store_.insert_order(o), "insert_order(open)");
    return Result<OrderRow>::ok(o);
}

OrderRow PaperExchange::close_position(const QString& agent_id, PositionRow& p, double fraction,
                                       int round_seq, const QString& status, const QString& reason) {
    const double mk = mark(p.coin);
    if (mk <= 0) {   // defense-in-depth: never close/liquidate at a non-positive mark.
        // Callers all guard mk<=0 already; if one slips through, reject without
        // mutating balances/positions and without persisting anything.
        OrderRow rej;
        rej.competition_id = competition_id_; rej.agent_id = agent_id; rej.round_seq = round_seq;
        rej.coin = p.coin; rej.side = p.side; rej.action = "close";
        rej.status = "rejected_risk"; rej.reject_reason = "no mark price";
        return rej;
    }
    const double fill = mk * (p.side == QLatin1String("long") ? 1.0 - kSlippage : 1.0 + kSlippage);
    const double close_qty = p.qty * fraction;
    const double dir = p.side == QLatin1String("long") ? 1.0 : -1.0;
    const double realized = (fill - p.entry_price) * close_qty * dir - p.funding_accrued * fraction;
    const double margin_back = close_qty * p.entry_price / p.leverage;
    const double fee = close_qty * fill * kTakerFee;
    const double credit = status == QLatin1String("liquidated")
                              ? std::max(0.0, margin_back + realized - fee)
                              : margin_back + realized - fee;
    balances_[agent_id] += credit;
    p.qty -= close_qty;
    p.funding_accrued *= (1.0 - fraction);

    OrderRow o; o.competition_id = competition_id_; o.agent_id = agent_id; o.round_seq = round_seq;
    o.coin = p.coin; o.side = p.side; o.action = "close"; o.qty = close_qty; o.price = fill;
    o.notional_usd = close_qty * fill; o.leverage = p.leverage; o.fee = fee;
    o.realized_pnl = realized; o.status = status; o.reject_reason = reason;
    log_store_err(store_.insert_order(o), "insert_order(close)");
    return o;
}

QVector<OrderRow> PaperExchange::mark_all(qint64 elapsed_ms, int round_seq) {
    QVector<OrderRow> liquidations;
    const double window = double(elapsed_ms) / double(3600 * 1000);   // funding per 1h (HL rates are hourly)
    for (auto it = positions_.begin(); it != positions_.end(); ++it) {
        auto& book = it.value();
        for (qsizetype i = book.size() - 1; i >= 0; --i) {
            PositionRow& p = book[i];
            const double mk = mark(p.coin);
            if (mk <= 0) continue;
            const double dir = p.side == QLatin1String("long") ? 1.0 : -1.0;
            p.funding_accrued += funding_rates_.value(p.coin, 0.0) * p.qty * mk * window * dir;
            const double margin = p.qty * p.entry_price / p.leverage;
            if (margin + position_upnl(p, mk) <= kMaintMarginRate * p.qty * mk) {
                liquidations.append(close_position(it.key(), p, 1.0, round_seq,
                                                   QStringLiteral("liquidated"), QStringLiteral("maintenance margin")));
                book.removeAt(i);
            }
        }
        persist(it.key());
    }
    return liquidations;
}

QVector<OrderRow> PaperExchange::close_all(const QString& agent_id, int round_seq) {
    QVector<OrderRow> out;
    auto& book = positions_[agent_id];
    for (qsizetype i = book.size() - 1; i >= 0; --i) {
        if (mark(book[i].coin) <= 0) {   // no mark → leave the position open, never close at 0
            LOG_WARN("PaperExchange", QStringLiteral("close_all: no mark for %1, leaving position open")
                                          .arg(book[i].coin));
            continue;
        }
        out.append(close_position(agent_id, book[i], 1.0, round_seq, "filled", QStringLiteral("kill_all")));
        book.removeAt(i);
    }
    persist(agent_id);
    return out;
}

void PaperExchange::persist(const QString& agent_id) {
    log_store_err(store_.upsert_account(competition_id_, agent_id, balances_.value(agent_id, 0.0)),
                  "upsert_account");
    // Replace stored positions for this agent: delete-all then upsert current.
    auto stored = store_.positions_for(competition_id_, agent_id);
    if (stored.is_ok())
        for (const auto& sp : stored.value()) {
            bool still = false;
            for (const auto& p : positions_.value(agent_id)) if (p.coin == sp.coin) { still = true; break; }
            if (!still) log_store_err(store_.delete_position(competition_id_, agent_id, sp.coin), "delete_position");
        }
    for (const auto& p : positions_.value(agent_id)) log_store_err(store_.upsert_position(p), "upsert_position");
}

} // namespace fincept::arena
