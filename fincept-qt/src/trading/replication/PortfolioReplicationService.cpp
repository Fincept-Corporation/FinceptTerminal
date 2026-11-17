#include "trading/replication/PortfolioReplicationService.h"

#include "core/logging/Logger.h"
#include "core/market/InstrumentCurrency.h"
#include "core/market/MarketSessions.h"
#include "trading/OrderSourceCapture.h"
#include "trading/PaperMarkService.h"
#include "trading/PaperTrading.h"
#include "trading/instruments/InstrumentService.h"

#include <QCoreApplication>
#include <QSet>

#include <cmath>

namespace fincept::trading::replication {

namespace {

bool is_delivery_product(const QString& product) {
    // CNC = equity delivery. (NRML/MIS stay on the Positions side.)
    return product.compare(QStringLiteral("CNC"), Qt::CaseInsensitive) == 0;
}

bool is_short_side(const QString& side) {
    return side.compare(QStringLiteral("short"), Qt::CaseInsensitive) == 0 ||
           side.compare(QStringLiteral("sell"), Qt::CaseInsensitive) == 0;
}

// The venue table's quote currency for an exchange code (NSE INR, LSE GBp ...);
// empty for a segment it does not list.
QString venue_currency(const QString& exchange) {
    if (const auto* v = core::market::venue_by_code(exchange.trimmed().toUpper()))
        return v->currency;
    return {};
}

QString rp_tr(const char* text) {
    return QCoreApplication::translate("PortfolioReplication", text);
}

} // namespace

QVector<SourceItem> to_source_items(const QVector<BrokerHolding>& holdings, const QVector<BrokerPosition>& positions) {
    QVector<SourceItem> out;
    out.reserve(holdings.size() + positions.size());
    for (const auto& h : holdings) {
        SourceItem s;
        s.kind = ItemKind::Holding;
        s.src_symbol = h.symbol;
        s.exchange = h.exchange;
        s.quantity = h.quantity;
        s.avg_price = h.avg_price;
        s.ltp = h.ltp > 0 ? h.ltp : h.avg_price;
        s.currency = venue_currency(h.exchange.isEmpty() ? QStringLiteral("NSE") : h.exchange);
        out.push_back(s);
    }
    for (const auto& p : positions) {
        if (p.quantity == 0)
            continue; // closed/net-flat intraday legs
        SourceItem s;
        s.kind = ItemKind::Position;
        s.src_symbol = p.symbol;
        s.exchange = p.exchange;
        s.product = p.product_type.isEmpty() ? QStringLiteral("MIS") : p.product_type;
        s.side = p.side;
        s.quantity = std::fabs(p.quantity);
        s.avg_price = p.avg_price;
        s.ltp = p.ltp > 0 ? p.ltp : p.avg_price;
        s.currency = venue_currency(p.exchange.isEmpty() ? QStringLiteral("NSE") : p.exchange);
        out.push_back(s);
    }
    return out;
}

QVector<SourceItem> paper_source_items(const QString& portfolio_id) {
    QVector<SourceItem> out;
    // A paper position's prices are in its instrument's currency. The engine marks a
    // symbol quoted in another currency than the book with a per-symbol cash factor
    // (multi-currency built-in account: canonical Yahoo-style symbols); every other
    // symbol is booked in the portfolio's own currency.
    QString book_ccy;
    try {
        book_ccy = pt_get_portfolio(portfolio_id).currency.trimmed();
    } catch (...) {
    }
    for (const auto& p : pt_get_positions(portfolio_id)) {
        if (p.quantity == 0)
            continue;
        SourceItem s;
        s.src_symbol = p.symbol;
        s.exchange = QStringLiteral("NSE"); // paper positions carry no exchange column
        s.quantity = std::fabs(p.quantity);
        s.avg_price = p.entry_price;
        s.ltp = p.current_price > 0 ? p.current_price : p.entry_price;
        s.currency = pt_has_symbol_fx(portfolio_id, p.symbol) ? core::market::currency_for_symbol(p.symbol) : book_ccy;
        if (is_delivery_product(p.product)) {
            s.kind = ItemKind::Holding;
        } else {
            s.kind = ItemKind::Position;
            s.product = p.product.isEmpty() ? QStringLiteral("MIS") : p.product;
            s.side = p.side;
        }
        out.push_back(s);
    }
    return out;
}

ReplicationPlan build_plan(const QVector<SourceItem>& items, const ReplicationOptions& opts, double target_available,
                           const QString& source_account_id, const QString& target_account_id,
                           const QString& target_paper_portfolio_id, const SymbolResolver& resolve,
                           const QString& target_currency, const FxFactor& fx) {
    ReplicationPlan plan;
    plan.source_account_id = source_account_id;
    plan.target_account_id = target_account_id;
    plan.target_paper_portfolio_id = target_paper_portfolio_id;
    plan.target_available = target_available;
    plan.target_currency = target_currency.trimmed();

    for (const auto& item : items) {
        if (item.kind == ItemKind::Holding && !opts.include_holdings)
            continue;
        if (item.kind == ItemKind::Position && !opts.include_positions)
            continue;

        PlannedOrder po;
        po.kind = item.kind;
        po.src_symbol = item.src_symbol;
        po.exchange = item.exchange.isEmpty() ? QStringLiteral("NSE") : item.exchange;
        po.quantity = std::floor(item.quantity); // whole shares
        po.est_price = item.ltp > 0 ? item.ltp : item.avg_price;
        po.est_value = po.quantity * po.est_price;

        if (item.kind == ItemKind::Holding) {
            po.side = QStringLiteral("buy");
            po.product = QStringLiteral("CNC");
        } else {
            po.side = is_short_side(item.side) ? QStringLiteral("sell") : QStringLiteral("buy");
            po.product = item.product.isEmpty() ? QStringLiteral("MIS") : item.product;
        }

        ResolveResult r = resolve ? resolve(item.src_symbol, po.exchange) : ResolveResult{true, item.src_symbol};
        po.norm_symbol = r.norm_symbol.isEmpty() ? item.src_symbol : r.norm_symbol;
        po.mapped = r.tradable_on_target;

        if (!po.mapped) {
            po.warning = QStringLiteral("not tradable on target broker");
            po.included = false;
        } else if (po.quantity <= 0) {
            po.warning = QStringLiteral("zero quantity after rounding");
            po.included = false;
        } else if (po.est_price <= 0) {
            po.warning = QStringLiteral("no price available");
            po.included = false;
        } else {
            po.included = true;
        }

        // Value in the target paper portfolio's currency before it is summed or compared
        // with the paper balance. A line that cannot be converted is left out and listed
        // — never taken 1:1.
        po.currency = item.currency.trimmed();
        if (plan.target_currency.isEmpty()) {
            po.est_value_target = po.est_value; // single-currency plan
        } else if (po.est_value > 0) {
            std::optional<double> factor;
            if (po.currency == plan.target_currency)
                factor = 1.0;
            else if (!po.currency.isEmpty() && fx)
                factor = fx(po.currency, plan.target_currency);
            if (factor && std::isfinite(*factor) && *factor > 0.0) {
                po.est_value_target = po.est_value * *factor;
                if (po.currency != plan.target_currency) {
                    po.fx_converted = true;
                    po.fx_to_target = *factor;
                }
            } else if (po.included) {
                po.fx_missing = true;
                po.included = false;
                po.warning = po.currency.isEmpty()
                                 ? rp_tr("price currency unknown — not counted")
                                 : rp_tr("no %1→%2 FX rate yet — not counted").arg(po.currency, plan.target_currency);
                plan.fx_missing << po.norm_symbol;
            }
        }

        if (po.included)
            plan.required_capital += po.est_value_target;
        plan.orders.push_back(po);
    }
    return plan;
}

FxFactor make_fx_factor() {
    return [](const QString& price_ccy, const QString& target_ccy) {
        return PaperMarkService::fx_factor(price_ccy, target_ccy);
    };
}

SymbolResolver make_instrument_resolver(const QString& source_broker_id, const QString& target_broker_id) {
    return [source_broker_id, target_broker_id](const QString& src_symbol, const QString& exchange) -> ResolveResult {
        auto& svc = InstrumentService::instance();
        // 1) source brsymbol → canonical normalized symbol (fallback: as-is)
        QString norm = src_symbol;
        if (auto n = svc.from_brsymbol(src_symbol, exchange, source_broker_id))
            norm = *n;
        // 2) tradable on the target broker? (also rules out cross-broker symbol gaps)
        bool tradable = svc.find(norm, exchange, target_broker_id).has_value();
        return ResolveResult{tradable, norm};
    };
}

ReplicationResult execute_plan(const ReplicationPlan& plan) {
    ReplicationResult res;
    // Who created these paper orders (audit "source"): the agent's while an agent step is
    // in progress on the GUI thread (empty off it), else "replication".
    const QString replication_source = capture_order_source(QStringLiteral("replication"));

    // Idempotency + no-close guard. Replication must ESTABLISH positions in the
    // target — it must never trade AGAINST an existing one. Routing a sell (or a
    // re-imported symbol) through the normal fill engine takes its close-netting
    // leg and books phantom REALIZED P&L (e.g. a source short selling into a just-
    // bought long), and re-running would double exposure. So we skip any symbol the
    // target already holds. To intentionally re-replicate, clear the paper account
    // first. (Matched on the broker-native/normalised symbol the order will use.)
    QSet<QString> already_held;
    for (const auto& p : pt_get_positions(plan.target_paper_portfolio_id))
        already_held.insert(p.symbol.toUpper());

    for (const auto& po : plan.orders) {
        if (!po.included) {
            ++res.skipped;
            res.rows.push_back({po.norm_symbol, false, po.warning.isEmpty() ? QStringLiteral("skipped") : po.warning});
            continue;
        }
        if (already_held.contains(po.norm_symbol.toUpper())) {
            ++res.skipped;
            res.rows.push_back({po.norm_symbol, false,
                                QStringLiteral("already in target — skipped (clear paper account to re-replicate)")});
            continue;
        }
        // Priced in another currency than the paper book: the engine must book this
        // symbol's cash at the plan's rate (its default factor is 1:1). No usable rate:
        // the row fails rather than booking dollars as rupees.
        if (po.fx_converted) {
            if (!std::isfinite(po.fx_to_target) || po.fx_to_target <= 0.0) {
                ++res.failed;
                res.rows.push_back({po.norm_symbol, false,
                                    rp_tr("no %1→%2 FX rate — not placed").arg(po.currency, plan.target_currency)});
                continue;
            }
            pt_set_symbol_fx(plan.target_paper_portfolio_id, po.norm_symbol, po.fx_to_target);
        }
        try {
            std::optional<double> price_opt;
            if (po.est_price > 0)
                price_opt = po.est_price; // market reference price for margin calc
            auto ord = pt_place_order(plan.target_paper_portfolio_id, po.norm_symbol, po.side, QStringLiteral("market"),
                                      po.quantity, price_opt, std::nullopt,
                                      /*reduce_only=*/false, po.exchange, po.product,
                                      replication_source); // audit provenance (UnifiedOrder::source format)
            pt_fill_order(ord.id, po.est_price);
            already_held.insert(po.norm_symbol.toUpper()); // a later same-symbol leg must not net against this
            ++res.placed;
            res.rows.push_back({po.norm_symbol, true, QString()});
        } catch (const std::exception& e) {
            ++res.failed;
            LOG_ERROR("PortfolioReplication",
                      logf("Replication order failed", {{"mode", "paper"},
                                                        {"symbol", po.norm_symbol},
                                                        {"side", po.side},
                                                        {"qty", po.quantity},
                                                        {"error", QString::fromUtf8(e.what())}}));
            res.rows.push_back({po.norm_symbol, false, QString::fromUtf8(e.what())});
        }
    }
    return res;
}

} // namespace fincept::trading::replication
