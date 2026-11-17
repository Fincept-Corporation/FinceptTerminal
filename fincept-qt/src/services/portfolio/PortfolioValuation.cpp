// src/services/portfolio/PortfolioValuation.cpp — see the header for the rules.
#include "services/portfolio/PortfolioValuation.h"

#include "core/logging/Logger.h"
#include "core/market/InstrumentCurrency.h"
#include "ui/formatting/FormatSettings.h"

#include <QDateTime>
#include <QMutex>
#include <QMutexLocker>
#include <QSet>

#include <algorithm>
#include <cmath>

namespace fincept::portfolio {

QString base_currency_of(const QString& currency, const QString& portfolio_id) {
    if (!currency.isEmpty())
        return core::market::major_currency(currency);
    const QString display = ui::formatting::FormatSettings::instance().display_currency();
    // Logged once per portfolio (every summary, metric and replay asks again).
    static QMutex logged_mutex;
    static QSet<QString> logged;
    bool first = false;
    {
        QMutexLocker lock(&logged_mutex);
        if (!logged.contains(portfolio_id)) {
            logged.insert(portfolio_id);
            first = true;
        }
    }
    if (first)
        LOG_WARN("PortfolioSvc", QString("Portfolio %1 has no currency stored; valued in the display currency %2")
                                     .arg(portfolio_id.isEmpty() ? QStringLiteral("(unsaved)") : portfolio_id, display));
    return display;
}

namespace portfolio_valuation_detail {

bool val_finite(double v) {
    return std::isfinite(v);
}

/// Quoted currency of a holding: the quote's own, else the symbol's, else base.
QString val_price_currency(const PortfolioAsset& a, const services::QuoteData* q, const QString& base) {
    if (q && !q->currency.trimmed().isEmpty())
        return q->currency.trimmed();
    const QString guessed = core::market::currency_for_symbol(a.symbol, a.exchange);
    return guessed.isEmpty() ? base : guessed;
}

double val_spot(const ValuationInputs& in, const QString& ccy, const QString& base) {
    if (ccy.compare(base, Qt::CaseInsensitive) == 0)
        return 1.0;
    if (!in.spot_fx)
        return kAbsent;
    const auto r = in.spot_fx(ccy);
    return (r && val_finite(*r) && *r > 0) ? *r : kAbsent;
}

} // namespace portfolio_valuation_detail

namespace pvd = portfolio_valuation_detail;

QStringList valuation_currencies(const ValuationInputs& in) {
    const QString base = core::market::major_currency(in.portfolio.currency);
    QStringList out;
    for (const auto& a : in.assets) {
        const auto it = in.quotes.constFind(a.symbol);
        const QString ccy =
            core::market::major_currency(pvd::val_price_currency(a, it == in.quotes.constEnd() ? nullptr : &*it, base));
        if (!ccy.isEmpty() && ccy != base && !out.contains(ccy))
            out.append(ccy);
    }
    if (in.ledger) {
        for (auto it = in.ledger->cash.constBegin(); it != in.ledger->cash.constEnd(); ++it) {
            const QString ccy = core::market::major_currency(it.key());
            if (!ccy.isEmpty() && ccy != base && !out.contains(ccy))
                out.append(ccy);
        }
    }
    return out;
}

PortfolioSummary value_portfolio(const ValuationInputs& in) {
    PortfolioSummary s;
    s.portfolio = in.portfolio;
    const QString base = base_currency_of(in.portfolio);
    s.base_currency = base;
    s.total_positions = static_cast<int>(in.assets.size());
    s.last_updated = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    s.holdings.reserve(in.assets.size());

    double total_mv = 0, total_cost = 0, total_day = 0, total_prev = 0;
    double price_pnl = 0, fx_pnl = 0;
    bool fx_split_complete = true;
    int valued = 0;

    for (const auto& a : in.assets) {
        HoldingWithQuote h;
        h.symbol = a.symbol;
        h.quantity = a.quantity;
        h.sector = a.sector;
        if (h.sector.isEmpty() && in.sector_for)
            h.sector = in.sector_for(a.symbol);

        const auto qit = in.quotes.constFind(a.symbol);
        const services::QuoteData* q = qit == in.quotes.constEnd() ? nullptr : &*qit;
        h.price_currency = pvd::val_price_currency(a, q, base);
        h.currency = core::market::major_currency(h.price_currency);
        const double divisor = core::market::minor_unit_divisor(h.price_currency);
        h.fx_rate = pvd::val_spot(in, h.currency, base);
        h.fx_ok = pvd::val_finite(h.fx_rate);
        if (q) {
            h.name = q->name;
            h.quote_type = q->quote_type;
            h.exchange = q->exchange;
            h.market_state = q->market_state;
            h.as_of = q->market_date;
            h.priced = pvd::val_finite(q->price) && q->price > 0;
        }
        if (h.exchange.isEmpty())
            h.exchange = a.exchange;

        // ── Cost basis ─────────────────────────────────────────────────────
        const LedgerPosition* lp = nullptr;
        if (in.ledger) {
            const auto lit = in.ledger->positions.constFind(a.symbol.toUpper());
            if (lit != in.ledger->positions.constEnd())
                lp = &*lit;
        }
        if (lp) {
            h.realized_pnl = lp->realized_base;
            h.dividends = lp->dividends_base;
        }
        const bool explained = lp && !lp->lots.isEmpty() && !lp->mixed_currency && lp->currency == h.currency &&
                               std::abs(lp->quantity - a.quantity) <= std::max(1e-9, std::abs(a.quantity) * 1e-6);
        h.ledger_explained = explained;
        if (explained) {
            h.cost_basis_local = lp->cost_local;
            h.avg_buy_price = a.quantity > 0 ? lp->cost_local / a.quantity * divisor : 0.0;
            h.open_lots = static_cast<int>(lp->lots.size());
            for (const auto& l : lp->lots)
                if (l.long_term)
                    h.long_term_quantity += l.quantity;
            if (pvd::val_finite(lp->cost_base)) {
                h.cost_basis = lp->cost_base;
                h.cost_fx_historical = true;
            } else {
                h.cost_basis = h.fx_ok ? h.cost_basis_local * h.fx_rate : kAbsent;
            }
        } else {
            h.avg_buy_price = a.avg_buy_price;
            h.cost_basis_local = core::market::to_major_units(a.quantity * a.avg_buy_price, h.price_currency);
            h.cost_basis = h.fx_ok ? h.cost_basis_local * h.fx_rate : kAbsent;
        }

        // ── Market value ───────────────────────────────────────────────────
        if (h.priced) {
            h.current_price = q->price;
            h.day_change = q->change;
            h.day_change_percent = q->change_pct;
            h.previous_close = pvd::val_finite(q->previous_close) ? q->previous_close : q->price - q->change;
            h.market_value_local = a.quantity * (q->price / divisor);
        } else {
            s.unpriced.append(a.symbol);
        }
        if (!h.fx_ok)
            s.fx_missing.append(QStringLiteral("%1 (%2)").arg(a.symbol, h.currency));

        if (h.valued()) {
            h.market_value = h.market_value_local * h.fx_rate;
            h.unrealized_pnl = h.market_value - h.cost_basis;
            h.unrealized_pnl_percent = h.cost_basis > 0 ? h.unrealized_pnl / h.cost_basis * 100.0 : kAbsent;
            h.price_pnl = (h.market_value_local - h.cost_basis_local) * h.fx_rate;
            // A base-currency holding has no currency effect (rate 1 then and now): its
            // FX part is 0, not unknown — otherwise one stored-average base holding left
            // the whole book's FX total unknown.
            const bool same_ccy = h.currency.compare(base, Qt::CaseInsensitive) == 0;
            h.fx_pnl = (h.cost_fx_historical || same_ccy) ? h.cost_basis_local * h.fx_rate - h.cost_basis : kAbsent;
            h.day_change_value = a.quantity * (h.day_change / divisor) * h.fx_rate;

            ++valued;
            total_mv += h.market_value;
            total_cost += h.cost_basis;
            if (pvd::val_finite(h.day_change_value)) {
                total_day += h.day_change_value;
                total_prev += h.market_value - h.day_change_value;
            }
            price_pnl += h.price_pnl;
            if (pvd::val_finite(h.fx_pnl))
                fx_pnl += h.fx_pnl;
            else
                fx_split_complete = false;
            if (h.unrealized_pnl > 0)
                ++s.gainers;
            else if (h.unrealized_pnl < 0)
                ++s.losers;
            if (!h.as_of.isEmpty() && h.as_of > s.as_of)
                s.as_of = h.as_of;
        }
        s.holdings.append(h);
    }

    for (auto& h : s.holdings)
        h.weight = (h.valued() && total_mv > 0) ? h.market_value / total_mv * 100.0 : kAbsent;

    s.valued_positions = valued;
    s.total_market_value = total_mv;
    s.total_cost_basis = total_cost;
    s.total_unrealized_pnl = total_mv - total_cost;
    s.total_unrealized_pnl_percent = total_cost > 0 ? s.total_unrealized_pnl / total_cost * 100.0 : 0.0;
    s.total_day_change = total_day;
    s.total_day_change_percent = total_prev > 0 ? total_day / total_prev * 100.0 : 0.0;
    s.total_price_pnl = valued > 0 ? price_pnl : kAbsent;
    s.total_fx_pnl = (valued > 0 && fx_split_complete) ? fx_pnl : kAbsent;
    s.complete = s.unpriced.isEmpty() && s.fx_missing.isEmpty();

    // ── Ledger totals and cash ────────────────────────────────────────────────
    if (in.ledger) {
        const LedgerResult& l = *in.ledger;
        s.total_realized_pnl = l.realized_base;
        s.realized_short_term = l.realized_short_base;
        s.realized_long_term = l.realized_long_base;
        s.total_dividends = l.dividends_base;
        s.total_fees = l.fees_base;
        s.total_taxes = l.taxes_base;
        s.total_interest = l.interest_base;
        s.cash_balances = l.cash;
        s.ledger_warnings = l.warnings.mid(0, 50);
        double cash = 0;
        bool cash_known = true;
        for (auto it = l.cash.constBegin(); it != l.cash.constEnd(); ++it) {
            const QString ccy = core::market::major_currency(it.key());
            const double fx = pvd::val_spot(in, ccy, base);
            if (pvd::val_finite(fx))
                cash += it.value() * fx;
            else
                cash_known = false;
        }
        s.cash_value = cash_known ? cash : kAbsent;
        if (in.portfolio.track_cash) {
            s.cash_in_nav = true;
            if (cash_known)
                s.total_market_value += cash;
            else {
                s.complete = false;
                for (auto it = l.cash.constBegin(); it != l.cash.constEnd(); ++it) {
                    const QString ccy = core::market::major_currency(it.key());
                    if (!pvd::val_finite(pvd::val_spot(in, ccy, base)))
                        s.fx_missing.append(QStringLiteral("CASH (%1)").arg(ccy));
                }
            }
        }
    }
    return s;
}

} // namespace fincept::portfolio
