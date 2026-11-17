// src/services/portfolio/PortfolioLots.cpp — see the header for the conventions.
//
// External-flow rules (LedgerResult::flows, base currency, + = money in):
//   track_cash = true : DEPOSIT +amount, WITHDRAWAL -amount. Everything else is
//                       internal (it moves cash inside the book).
//   track_cash = false: the book holds no cash, so money crosses its boundary on
//                       every cash-moving row:
//                         BUY       +(gross + fees + taxes)   paid in from outside
//                         SELL      -(gross - fees - taxes)   proceeds taken out
//                         DIVIDEND  -(gross - taxes - fees)   income paid out
//                         INTEREST  -(amount - taxes)         income paid out
//                         FEE / TAX +amount                   paid from outside and consumed
//                       DEPOSIT / WITHDRAWAL do not touch the (cash-less) book.
//   Fees and taxes therefore lower the time-weighted return, and dividends raise it,
//   whichever way cash is tracked.
#include "services/portfolio/PortfolioLots.h"

#include "core/market/InstrumentCurrency.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace fincept::portfolio {

namespace portfolio_lots_detail {

constexpr double kQtyEps = 1e-9;

bool lots_finite(double v) {
    return std::isfinite(v);
}

/// Quantity tolerance relative to the amounts involved.
double lots_eps(double ref) {
    return std::max(kQtyEps, std::abs(ref) * 1e-9);
}

struct RowCtx {
    const Transaction* t = nullptr;
    QDate date;
    QString type;
    int order = 0; // input order, tie-breaker
};

int lots_same_day_rank(const QString& type) {
    // A split is effective at the open of its ex-date; a deposit funds that day's buys.
    if (type == txn::kSplit)
        return 0;
    if (type == txn::kDeposit)
        return 1;
    return 2;
}

/// Quoted currency of a row's money fields.
QString lots_row_quoted_ccy(const Transaction& t, const QString& type, const LedgerOptions& opt) {
    const QString c = t.currency.trimmed();
    if (!c.isEmpty())
        return c;
    if (txn::is_cash_only(type) || t.symbol.trimmed().isEmpty())
        return opt.base_currency;
    if (opt.instrument_currency) {
        const QString ic = opt.instrument_currency(t.symbol);
        if (!ic.isEmpty())
            return ic;
    }
    const QString guessed = core::market::currency_for_symbol(t.symbol);
    return guessed.isEmpty() ? opt.base_currency : guessed;
}

/// Base units per 1 major unit of `ccy` for a row: its own fx_rate, else the
/// historical lookup, else NaN.
double lots_row_fx(const Transaction& t, const QString& ccy_major, const QDate& date, const LedgerOptions& opt) {
    if (ccy_major.compare(opt.base_currency, Qt::CaseInsensitive) == 0)
        return 1.0;
    if (lots_finite(t.fx_rate) && t.fx_rate > 0)
        return t.fx_rate;
    if (opt.historical_fx && date.isValid()) {
        if (auto r = opt.historical_fx(ccy_major, date); r && lots_finite(*r) && *r > 0)
            return *r;
    }
    return kAbsent;
}

double lots_nonneg(double v) {
    return (lots_finite(v) && v > 0) ? v : 0.0;
}

} // namespace portfolio_lots_detail

namespace pld = portfolio_lots_detail;

LotMethod lot_method_from_key(const QString& key) {
    const QString k = key.trimmed().toUpper();
    if (k == QLatin1String("LIFO"))
        return LotMethod::Lifo;
    if (k == QLatin1String("HIFO"))
        return LotMethod::Hifo;
    if (k == QLatin1String("AVERAGE") || k == QLatin1String("AVG") || k == QLatin1String("WAC") ||
        k == QLatin1String("AVERAGE_COST") || k == QLatin1String("AVERAGE COST"))
        return LotMethod::AverageCost;
    if (k == QLatin1String("SPECIFIC") || k == QLatin1String("SPECIFIC_LOT") || k == QLatin1String("SPEC") ||
        k == QLatin1String("SPECIFIC LOT"))
        return LotMethod::SpecificLot;
    return LotMethod::Fifo;
}

QString lot_method_key(LotMethod method) {
    switch (method) {
        case LotMethod::Lifo:
            return QStringLiteral("LIFO");
        case LotMethod::Hifo:
            return QStringLiteral("HIFO");
        case LotMethod::AverageCost:
            return QStringLiteral("AVERAGE");
        case LotMethod::SpecificLot:
            return QStringLiteral("SPECIFIC");
        case LotMethod::Fifo:
            break;
    }
    return QStringLiteral("FIFO");
}

QDate ledger_row_date(const Transaction& t) {
    return QDate::fromString(t.transaction_date.trimmed().left(10), Qt::ISODate);
}

namespace {

/// Order in which the open lots of a position are consumed by a sale.
QVector<int> portfolio_lots_consume_order(const LedgerPosition& pos, LotMethod method, const QString& lot_id) {
    QVector<int> idx(pos.lots.size());
    std::iota(idx.begin(), idx.end(), 0);
    switch (method) {
        case LotMethod::Lifo:
            std::reverse(idx.begin(), idx.end());
            break;
        case LotMethod::Hifo:
            std::stable_sort(idx.begin(), idx.end(), [&pos](int a, int b) {
                return pos.lots[a].unit_cost_local() > pos.lots[b].unit_cost_local();
            });
            break;
        case LotMethod::SpecificLot:
            if (!lot_id.isEmpty()) {
                const auto it = std::find_if(idx.begin(), idx.end(),
                                             [&pos, &lot_id](int i) { return pos.lots[i].lot_id == lot_id; });
                if (it != idx.end())
                    std::rotate(idx.begin(), it, it + 1);
            }
            break;
        case LotMethod::Fifo:
        case LotMethod::AverageCost:
            break;
    }
    return idx;
}

void portfolio_lots_recompute_position(LedgerPosition& pos) {
    pos.quantity = 0;
    pos.cost_local = 0;
    pos.cost_base = 0;
    pos.mixed_currency = false;
    bool base_known = true;
    for (const auto& l : pos.lots) {
        pos.quantity += l.quantity;
        if (!pos.currency.isEmpty() && l.currency != pos.currency)
            pos.mixed_currency = true;
        pos.cost_local += l.cost_local;
        if (pld::lots_finite(l.cost_base))
            pos.cost_base += l.cost_base;
        else
            base_known = false;
    }
    if (pos.mixed_currency)
        pos.cost_local = kAbsent;
    if (!base_known)
        pos.cost_base = kAbsent;
}

} // namespace

LedgerResult replay_ledger(const QVector<Transaction>& txns, const LedgerOptions& opt_in) {
    LedgerOptions opt = opt_in;
    // The service passes the resolved base (portfolio::base_currency_of(): the display currency
    // for a portfolio stored without one). Deliberate USD here: this leaf TU (unit-tested on its
    // own) has no settings access; it only serves a caller that leaves the option empty.
    opt.base_currency = core::market::major_currency(opt.base_currency.isEmpty() ? QStringLiteral("USD")
                                                                                  : opt.base_currency);
    const QDate as_of = opt.as_of.isValid() ? opt.as_of : QDate::currentDate();
    const int lt_days = opt.long_term_days > 0 ? opt.long_term_days : 365;

    LedgerResult out;

    // ── Order the rows ────────────────────────────────────────────────────────
    QVector<pld::RowCtx> rows;
    rows.reserve(txns.size());
    for (int i = 0; i < txns.size(); ++i) {
        pld::RowCtx c;
        c.t = &txns[i];
        c.date = ledger_row_date(txns[i]);
        c.type = txns[i].transaction_type.trimmed().toUpper();
        c.order = i;
        if (!c.date.isValid()) {
            out.warnings << QStringLiteral("%1 %2: unreadable date '%3' — row ignored")
                                .arg(c.type, txns[i].symbol, txns[i].transaction_date);
            continue;
        }
        if (!txn::is_valid_type(c.type)) {
            out.warnings << QStringLiteral("%1 %2: unknown type — row ignored").arg(c.type, txns[i].symbol);
            continue;
        }
        rows.append(c);
    }
    // Input may be newest-first (repository order). Entry order within a day comes
    // from created_at; rows without one keep their relative input order reversed
    // only if the whole input was newest-first — decided by the dates.
    bool newest_first = false;
    if (rows.size() >= 2)
        newest_first = rows.first().date > rows.last().date;
    std::stable_sort(rows.begin(), rows.end(), [newest_first](const pld::RowCtx& a, const pld::RowCtx& b) {
        if (a.date != b.date)
            return a.date < b.date;
        const int ra = pld::lots_same_day_rank(a.type), rb = pld::lots_same_day_rank(b.type);
        if (ra != rb)
            return ra < rb;
        const QString& ca = a.t->created_at;
        const QString& cb = b.t->created_at;
        if (!ca.isEmpty() && !cb.isEmpty() && ca != cb)
            return ca < cb;
        return newest_first ? a.order > b.order : a.order < b.order;
    });

    auto add_flow = [&out](const QDate& d, double amount_base, const QString& type, const QString& id) {
        ExternalFlow f;
        f.date = d;
        f.amount_base = amount_base;
        f.type = type;
        f.txn_id = id;
        out.flows.append(f);
    };
    auto fx_note = [&out](const Transaction& t, const QString& ccy) {
        out.warnings << QStringLiteral("%1 %2 on %3: no %4→base FX rate — base-currency figures for it are absent")
                            .arg(t.transaction_type.toUpper(), t.symbol, t.transaction_date.left(10), ccy);
    };

    for (const auto& row : rows) {
        const Transaction& t = *row.t;
        const QString& type = row.type;
        const QString sym = t.symbol.trimmed().toUpper();
        const QString quoted = pld::lots_row_quoted_ccy(t, type, opt);
        const QString ccy = core::market::major_currency(quoted);
        auto major = [&quoted](double v) { return core::market::to_major_units(v, quoted); };
        const double fees = major(pld::lots_nonneg(t.fees));
        const double taxes = major(pld::lots_nonneg(t.taxes));

        if (!pld::lots_finite(t.quantity) || !pld::lots_finite(t.price)) {
            out.warnings << QStringLiteral("%1 %2 on %3: non-numeric quantity/price — row ignored")
                                .arg(type, sym, t.transaction_date.left(10));
            continue;
        }
        ++out.rows_used;

        if (type == txn::kBuy) {
            if (t.quantity <= 0 || t.price < 0 || sym.isEmpty()) {
                out.warnings << QStringLiteral("BUY %1 on %2: quantity must be > 0 — row ignored")
                                    .arg(sym, t.transaction_date.left(10));
                continue;
            }
            const double gross = t.quantity * major(t.price);
            const double cost = gross + fees + taxes;
            const double fx = pld::lots_row_fx(t, ccy, row.date, opt);
            if (!pld::lots_finite(fx))
                fx_note(t, ccy);
            LedgerPosition& pos = out.positions[sym];
            pos.symbol = sym;
            if (pos.currency.isEmpty())
                pos.currency = ccy;
            if (!pos.first_date.isValid())
                pos.first_date = row.date;
            TaxLot lot;
            lot.lot_id = t.id;
            lot.symbol = sym;
            lot.currency = ccy;
            lot.open_date = row.date;
            lot.quantity = t.quantity;
            lot.original_quantity = t.quantity;
            lot.cost_local = cost;
            lot.fx_rate = fx;
            lot.cost_base = pld::lots_finite(fx) ? cost * fx : kAbsent;
            pos.lots.append(lot);
            portfolio_lots_recompute_position(pos);
            out.cash[ccy] -= cost;
            if (pld::lots_finite(fx)) {
                out.fees_base += fees * fx;
                out.taxes_base += taxes * fx;
            }
            if (!opt.track_cash)
                add_flow(row.date, pld::lots_finite(fx) ? cost * fx : kAbsent, type, t.id);
        } else if (type == txn::kSell) {
            if (t.quantity <= 0 || t.price < 0 || sym.isEmpty()) {
                out.warnings << QStringLiteral("SELL %1 on %2: quantity must be > 0 — row ignored")
                                    .arg(sym, t.transaction_date.left(10));
                continue;
            }
            const double gross = t.quantity * major(t.price);
            const double net = gross - fees - taxes;
            const double fx = pld::lots_row_fx(t, ccy, row.date, opt);
            if (!pld::lots_finite(fx))
                fx_note(t, ccy);
            LedgerPosition& pos = out.positions[sym];
            pos.symbol = sym;
            if (pos.currency.isEmpty())
                pos.currency = ccy;

            // Average cost: re-pool the remaining lots before the sale.
            if (opt.method == LotMethod::AverageCost && pos.quantity > 0 && !pos.mixed_currency) {
                const double unit_local = pos.cost_local / pos.quantity;
                const double unit_base = pos.cost_base / pos.quantity; // NaN propagates
                for (auto& l : pos.lots) {
                    l.cost_local = unit_local * l.quantity;
                    l.cost_base = unit_base * l.quantity;
                }
            }
            if (opt.method == LotMethod::SpecificLot && !t.lot_id.isEmpty() &&
                std::none_of(pos.lots.begin(), pos.lots.end(), [&t](const TaxLot& l) { return l.lot_id == t.lot_id; }))
                out.warnings << QStringLiteral("SELL %1 on %2: lot %3 is not open — sold FIFO instead")
                                    .arg(sym, t.transaction_date.left(10), t.lot_id);

            double remaining = t.quantity;
            const QVector<int> order = portfolio_lots_consume_order(pos, opt.method, t.lot_id);
            for (int li : order) {
                if (remaining <= pld::lots_eps(t.quantity))
                    break;
                TaxLot& lot = pos.lots[li];
                if (lot.quantity <= pld::kQtyEps)
                    continue;
                const double q = std::min(remaining, lot.quantity);
                const double share = q / lot.quantity;
                RealizedLot rl;
                rl.sell_id = t.id;
                rl.lot_id = lot.lot_id;
                rl.symbol = sym;
                rl.currency = lot.currency;
                rl.open_date = lot.open_date;
                rl.close_date = row.date;
                rl.quantity = q;
                rl.cost_local = lot.cost_local * share;
                rl.cost_base = lot.cost_base * share; // NaN propagates
                rl.proceeds_local = net * (q / t.quantity);
                rl.proceeds_base = pld::lots_finite(fx) ? rl.proceeds_local * fx : kAbsent;
                if (lot.currency != ccy) {
                    // Sold in another currency than bought: no local-currency gain.
                    rl.gain_local = kAbsent;
                } else {
                    rl.gain_local = rl.proceeds_local - rl.cost_local;
                }
                rl.gain_base = (pld::lots_finite(rl.proceeds_base) && pld::lots_finite(rl.cost_base))
                                   ? rl.proceeds_base - rl.cost_base
                                   : kAbsent;
                rl.holding_days = static_cast<int>(lot.open_date.daysTo(row.date));
                rl.long_term = rl.holding_days > lt_days;
                lot.cost_local -= rl.cost_local;
                lot.cost_base -= pld::lots_finite(rl.cost_base) ? rl.cost_base : 0.0;
                lot.quantity -= q;
                remaining -= q;

                if (pld::lots_finite(rl.gain_local))
                    pos.realized_local += rl.gain_local;
                if (pld::lots_finite(rl.gain_base)) {
                    pos.realized_base += rl.gain_base;
                    out.realized_base += rl.gain_base;
                    (rl.long_term ? out.realized_long_base : out.realized_short_base) += rl.gain_base;
                } else {
                    pos.realized_base_complete = false;
                    out.realized_base_complete = false;
                }
                out.realized.append(rl);
            }
            pos.lots.erase(std::remove_if(pos.lots.begin(), pos.lots.end(),
                                          [](const TaxLot& l) { return l.quantity <= pld::kQtyEps; }),
                           pos.lots.end());
            if (remaining > pld::lots_eps(t.quantity)) {
                pos.unmatched_sell_quantity += remaining;
                out.warnings << QStringLiteral("SELL %1 on %2: %3 units exceed the ledger's position — no realised "
                                               "gain computed for them (cost unknown)")
                                    .arg(sym, t.transaction_date.left(10), QString::number(remaining, 'g', 12));
            }
            portfolio_lots_recompute_position(pos);
            out.cash[ccy] += net;
            if (pld::lots_finite(fx)) {
                out.fees_base += fees * fx;
                out.taxes_base += taxes * fx;
            }
            if (!opt.track_cash)
                add_flow(row.date, pld::lots_finite(fx) ? -net * fx : kAbsent, type, t.id);
        } else if (type == txn::kSplit) {
            const double factor = t.quantity;
            if (!(factor > 0) || sym.isEmpty()) {
                out.warnings << QStringLiteral("SPLIT %1 on %2: factor must be > 0 — row ignored")
                                    .arg(sym, t.transaction_date.left(10));
                continue;
            }
            auto it = out.positions.find(sym);
            if (it != out.positions.end()) {
                for (auto& l : it->lots) {
                    l.quantity *= factor;
                    l.original_quantity *= factor;
                }
                portfolio_lots_recompute_position(*it);
            }
        } else if (type == txn::kDividend) {
            const double gross = std::max(0.0, t.quantity) * major(t.price);
            const double net = gross - taxes - fees;
            const double fx = pld::lots_row_fx(t, ccy, row.date, opt);
            if (!pld::lots_finite(fx))
                fx_note(t, ccy);
            if (!sym.isEmpty()) {
                LedgerPosition& pos = out.positions[sym];
                pos.symbol = sym;
                pos.dividends_local += net;
                if (pld::lots_finite(fx))
                    pos.dividends_base += net * fx;
            }
            out.cash[ccy] += net;
            if (pld::lots_finite(fx)) {
                out.dividends_base += net * fx;
                out.withholding_base += taxes * fx;
                out.taxes_base += taxes * fx;
                out.fees_base += fees * fx;
            }
            if (!opt.track_cash)
                add_flow(row.date, pld::lots_finite(fx) ? -net * fx : kAbsent, type, t.id);
        } else {
            // Cash rows: amount = quantity × price (price 1 by convention).
            const double amount = std::abs(t.quantity * (t.price > 0 ? major(t.price) : major(1.0)));
            const double fx = pld::lots_row_fx(t, ccy, row.date, opt);
            if (!pld::lots_finite(fx))
                fx_note(t, ccy);
            const double to_base = pld::lots_finite(fx) ? fx : kAbsent;
            if (type == txn::kDeposit) {
                out.cash[ccy] += amount;
                if (opt.track_cash)
                    add_flow(row.date, amount * to_base, type, t.id);
            } else if (type == txn::kWithdrawal) {
                out.cash[ccy] -= amount;
                if (opt.track_cash)
                    add_flow(row.date, -amount * to_base, type, t.id);
            } else if (type == txn::kFee) {
                out.cash[ccy] -= amount;
                if (pld::lots_finite(fx))
                    out.fees_base += amount * fx;
                if (!opt.track_cash)
                    add_flow(row.date, amount * to_base, type, t.id);
            } else if (type == txn::kTax) {
                out.cash[ccy] -= amount;
                if (pld::lots_finite(fx))
                    out.taxes_base += amount * fx;
                if (!opt.track_cash)
                    add_flow(row.date, amount * to_base, type, t.id);
            } else if (type == txn::kInterest) {
                const double net = amount - taxes;
                out.cash[ccy] += net;
                if (pld::lots_finite(fx)) {
                    out.interest_base += net * fx;
                    out.withholding_base += taxes * fx;
                    out.taxes_base += taxes * fx;
                }
                if (!opt.track_cash)
                    add_flow(row.date, -net * to_base, type, t.id);
            }
        }
    }

    // Holding periods to as_of.
    for (auto& pos : out.positions) {
        for (auto& l : pos.lots) {
            l.holding_days = static_cast<int>(l.open_date.daysTo(as_of));
            l.long_term = l.holding_days > lt_days;
        }
    }
    // Drop zero cash lines (pure rounding).
    for (auto it = out.cash.begin(); it != out.cash.end();) {
        if (std::abs(it.value()) < 1e-9)
            it = out.cash.erase(it);
        else
            ++it;
    }
    return out;
}

double quantity_held_before(const QVector<Transaction>& txns, const QString& symbol, const QDate& date) {
    const QString sym = symbol.trimmed().toUpper();
    QVector<const Transaction*> rows;
    for (const auto& t : txns) {
        if (t.symbol.trimmed().toUpper() != sym)
            continue;
        const QDate d = ledger_row_date(t);
        if (!d.isValid())
            continue;
        const QString type = t.transaction_type.trimmed().toUpper();
        if ((type == txn::kBuy || type == txn::kSell) && d < date)
            rows.append(&t);
        else if (type == txn::kSplit && d <= date)
            rows.append(&t);
    }
    std::stable_sort(rows.begin(), rows.end(), [](const Transaction* a, const Transaction* b) {
        const QDate da = ledger_row_date(*a), db = ledger_row_date(*b);
        if (da != db)
            return da < db;
        const bool sa = a->transaction_type.trimmed().toUpper() == txn::kSplit;
        const bool sb = b->transaction_type.trimmed().toUpper() == txn::kSplit;
        return sa && !sb;
    });
    double qty = 0;
    for (const Transaction* t : rows) {
        const QString type = t->transaction_type.trimmed().toUpper();
        if (!std::isfinite(t->quantity))
            continue;
        if (type == txn::kBuy && t->quantity > 0)
            qty += t->quantity;
        else if (type == txn::kSell && t->quantity > 0)
            qty = std::max(0.0, qty - t->quantity);
        else if (type == txn::kSplit && t->quantity > 0)
            qty *= t->quantity;
    }
    return qty;
}

} // namespace fincept::portfolio
