#include "trading/replication/PortfolioReplicationService.h"

#include "trading/PaperTrading.h"
#include "trading/instruments/InstrumentService.h"

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

} // namespace

QVector<SourceItem> to_source_items(const QVector<BrokerHolding>& holdings,
                                    const QVector<BrokerPosition>& positions) {
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
        out.push_back(s);
    }
    return out;
}

QVector<SourceItem> paper_source_items(const QString& portfolio_id) {
    QVector<SourceItem> out;
    for (const auto& p : pt_get_positions(portfolio_id)) {
        if (p.quantity == 0)
            continue;
        SourceItem s;
        s.src_symbol = p.symbol;
        s.exchange = QStringLiteral("NSE"); // paper positions carry no exchange column
        s.quantity = std::fabs(p.quantity);
        s.avg_price = p.entry_price;
        s.ltp = p.current_price > 0 ? p.current_price : p.entry_price;
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

ReplicationPlan build_plan(const QVector<SourceItem>& items,
                           const ReplicationOptions& opts,
                           double target_available,
                           const QString& source_account_id,
                           const QString& target_account_id,
                           const QString& target_paper_portfolio_id,
                           const SymbolResolver& resolve) {
    ReplicationPlan plan;
    plan.source_account_id = source_account_id;
    plan.target_account_id = target_account_id;
    plan.target_paper_portfolio_id = target_paper_portfolio_id;
    plan.target_available = target_available;

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

        if (po.included)
            plan.required_capital += po.est_value;
        plan.orders.push_back(po);
    }
    return plan;
}

SymbolResolver make_instrument_resolver(const QString& source_broker_id,
                                        const QString& target_broker_id) {
    return [source_broker_id, target_broker_id](const QString& src_symbol,
                                                const QString& exchange) -> ResolveResult {
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
        try {
            std::optional<double> price_opt;
            if (po.est_price > 0)
                price_opt = po.est_price; // market reference price for margin calc
            auto ord = pt_place_order(plan.target_paper_portfolio_id, po.norm_symbol, po.side,
                                      QStringLiteral("market"), po.quantity, price_opt, std::nullopt,
                                      /*reduce_only=*/false, po.exchange, po.product);
            pt_fill_order(ord.id, po.est_price);
            already_held.insert(po.norm_symbol.toUpper()); // a later same-symbol leg must not net against this
            ++res.placed;
            res.rows.push_back({po.norm_symbol, true, QString()});
        } catch (const std::exception& e) {
            ++res.failed;
            res.rows.push_back({po.norm_symbol, false, QString::fromUtf8(e.what())});
        }
    }
    return res;
}

} // namespace fincept::trading::replication
