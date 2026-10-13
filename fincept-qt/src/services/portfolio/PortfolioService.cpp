// src/services/portfolio/PortfolioService.cpp
//
// Core: singleton, portfolio CRUD (load/create/delete), asset CRUD
// (add/sell), transactions + ledger reconciliation, dividends,
// invalidate_cache. Split concerns:
//   - PortfolioService_Summary.cpp      — load/build/finalize summary
//   - PortfolioService_Metrics.cpp      — market analytics, TWR metrics, history backfill
//   - PortfolioService_ImportExport.cpp — CSV/JSON round-trip
#include "services/portfolio/PortfolioService.h"

#include "core/logging/Logger.h"
#include "services/sectors/SectorResolver.h"
#include "storage/repositories/PortfolioRepository.h"

#include <QDateTime>
#include <QMutexLocker>

#include <algorithm>
#include <cmath>
#include <utility>

namespace fincept::services {

namespace portfolio_service_detail {

/// Trade dates arrive as "YYYY-MM-DD" (dialogs, imports) or as a full ISO
/// date-time (legacy rows). Empty means "now".
QString normalise_trade_date(const QString& date) {
    const QString d = date.trimmed();
    if (d.isEmpty())
        return QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    return d;
}

bool positive_finite(double v) {
    return std::isfinite(v) && v > 0.0;
}

/// Quantity tolerance relative to the position size — an absolute 1e-4 used to
/// close a 0.00005 BTC position on any partial sell.
double qty_epsilon(double held) {
    return std::max(1e-9, std::abs(held) * 1e-9);
}

struct LedgerPosition {
    double qty = 0;
    double avg_cost = 0;
    QString first_date;
    bool any = false;
};

/// Weighted-average-cost replay of one symbol's BUY/SELL rows, oldest first.
/// get_transactions() returns newest-first (date DESC, created_at DESC); the
/// reverse is the order the trades were entered in.
LedgerPosition replay_symbol(const QVector<portfolio::Transaction>& txns_newest_first, const QString& symbol) {
    LedgerPosition p;
    for (auto it = txns_newest_first.crbegin(); it != txns_newest_first.crend(); ++it) {
        const auto& t = *it;
        if (t.symbol != symbol)
            continue;
        const QString type = t.transaction_type.toUpper();
        if (type == QLatin1String("BUY")) {
            if (!positive_finite(t.quantity) || !std::isfinite(t.price))
                continue;
            const double new_qty = p.qty + t.quantity;
            p.avg_cost = new_qty > 0 ? (p.qty * p.avg_cost + t.quantity * t.price) / new_qty : 0.0;
            p.qty = new_qty;
            if (!p.any)
                p.first_date = t.transaction_date;
            p.any = true;
        } else if (type == QLatin1String("SELL")) {
            const double sold = std::min(std::max(t.quantity, 0.0), p.qty);
            p.qty -= sold;
            if (p.qty <= qty_epsilon(sold)) {
                p.qty = 0;
                p.avg_cost = 0;
            }
            p.any = true;
        }
    }
    return p;
}

} // namespace portfolio_service_detail

namespace psd = portfolio_service_detail;

PortfolioService& PortfolioService::instance() {
    static PortfolioService s;
    return s;
}

PortfolioService::PortfolioService() : QObject(nullptr) {
    // When the SectorResolver lands a fresh sector for a symbol, persist it
    // onto whichever portfolios hold it and invalidate their summary caches
    // so the Sectors tab refreshes on next refresh.
    connect(&SectorResolver::instance(), &SectorResolver::sector_resolved, this,
            [this](QString symbol, QString sector) {
                if (symbol.isEmpty() || sector.isEmpty())
                    return;
                auto& repo = PortfolioRepository::instance();
                auto ports = repo.list_portfolios();
                if (ports.is_err())
                    return;
                for (const auto& p : ports.value()) {
                    auto assets = repo.get_assets(p.id);
                    if (assets.is_err())
                        continue;
                    bool touched = false;
                    for (const auto& a : assets.value()) {
                        if (a.symbol == symbol && a.sector != sector) {
                            repo.set_asset_sector(p.id, symbol, sector);
                            touched = true;
                        }
                    }
                    if (touched) {
                        invalidate_cache(p.id);
                        // Refresh the portfolio view so sectors appear without
                        // the user having to hit refresh manually. Debounced: a
                        // fresh import resolves one sector per holding, and a
                        // full rebuild (quote fetch + metrics + snapshot) per
                        // symbol is a request storm.
                        const bool first = sector_refresh_pending_.isEmpty();
                        sector_refresh_pending_.insert(p.id);
                        if (first) {
                            QTimer::singleShot(400, this, [this]() {
                                const QSet<QString> pending = std::exchange(sector_refresh_pending_, QSet<QString>{});
                                for (const QString& pid : pending)
                                    load_summary(pid);
                            });
                        }
                    }
                }
            });
}

// ── Portfolio CRUD ───────────────────────────────────────────────────────────

void PortfolioService::load_portfolios() {
    auto r = PortfolioRepository::instance().list_portfolios();
    if (r.is_ok()) {
        emit portfolios_loaded(r.value());
    } else {
        LOG_ERROR("PortfolioSvc", "Failed to load portfolios: " + QString::fromStdString(r.error()));
    }
}

void PortfolioService::create_portfolio(const QString& name, const QString& owner, const QString& currency,
                                        const QString& description, const QString& broker_account_id) {
    auto r = PortfolioRepository::instance().create_portfolio(name, owner, currency, description, broker_account_id);
    if (r.is_ok()) {
        auto p = PortfolioRepository::instance().get_portfolio(r.value());
        if (p.is_ok())
            emit portfolio_created(p.value());
        load_portfolios();
    } else {
        LOG_ERROR("PortfolioSvc", "Failed to create portfolio: " + QString::fromStdString(r.error()));
        emit operation_failed({}, tr("Could not create the portfolio: %1").arg(QString::fromStdString(r.error())));
    }
}

void PortfolioService::delete_portfolio(const QString& id) {
    invalidate_cache(id);
    analytics_cache_.remove(id);
    auto r = PortfolioRepository::instance().delete_portfolio(id);
    if (r.is_ok()) {
        emit portfolio_deleted(id);
        load_portfolios();
    } else {
        LOG_ERROR("PortfolioSvc", "Failed to delete portfolio: " + QString::fromStdString(r.error()));
        emit operation_failed(id, tr("Could not delete the portfolio: %1").arg(QString::fromStdString(r.error())));
    }
}

// ── Asset operations ─────────────────────────────────────────────────────────

bool PortfolioService::add_asset(const QString& portfolio_id, const QString& symbol, double qty, double price,
                                 const QString& date, const QString& broker_symbol, const QString& exchange) {
    const QString sym = symbol.trimmed().toUpper();
    if (sym.isEmpty() || !psd::positive_finite(qty) || !psd::positive_finite(price)) {
        emit operation_failed(portfolio_id, tr("BUY rejected: a symbol, a quantity > 0 and a price > 0 are required."));
        return false;
    }
    auto& repo = PortfolioRepository::instance();
    const QString txn_date = psd::normalise_trade_date(date);

    // Sector left empty here — SectorResolver fills it asynchronously after
    // the asset lands in the DB. Broker fields pass through verbatim.
    auto r = repo.add_asset(portfolio_id, sym, qty, price, txn_date, /*sector=*/QString(), broker_symbol, exchange);
    if (r.is_err()) {
        LOG_ERROR("PortfolioSvc", "Failed to add asset: " + QString::fromStdString(r.error()));
        emit operation_failed(portfolio_id, tr("Could not record the BUY of %1: %2")
                                                .arg(sym, QString::fromStdString(r.error())));
        return false;
    }

    auto tr_r = repo.add_transaction(portfolio_id, sym, "BUY", qty, price, txn_date);
    if (tr_r.is_err())
        LOG_ERROR("PortfolioSvc", "BUY recorded on the position but not in the ledger: " +
                                      QString::fromStdString(tr_r.error()));

    invalidate_cache(portfolio_id);
    emit asset_added(portfolio_id);
    return true;
}

bool PortfolioService::sell_asset(const QString& portfolio_id, const QString& symbol, double qty, double price,
                                  const QString& date) {
    const QString sym = symbol.trimmed().toUpper();
    if (!psd::positive_finite(qty) || !psd::positive_finite(price)) {
        emit operation_failed(portfolio_id, tr("SELL rejected: quantity and price must both be greater than 0."));
        return false;
    }
    auto& repo = PortfolioRepository::instance();

    auto assets_r = repo.get_assets(portfolio_id);
    if (assets_r.is_err()) {
        LOG_ERROR("PortfolioSvc", "Failed to get assets for sell: " + QString::fromStdString(assets_r.error()));
        emit operation_failed(portfolio_id, tr("Could not read the portfolio to record the SELL."));
        return false;
    }

    const portfolio::PortfolioAsset* found = nullptr;
    for (const auto& a : assets_r.value()) {
        if (a.symbol == sym) {
            found = &a;
            break;
        }
    }
    if (!found) {
        LOG_ERROR("PortfolioSvc", QString("Asset %1 not found in portfolio %2").arg(sym, portfolio_id));
        emit operation_failed(portfolio_id, tr("SELL rejected: %1 is not held in this portfolio.").arg(sym));
        return false;
    }

    const double held = found->quantity;
    if (qty > held + psd::qty_epsilon(held)) {
        emit operation_failed(portfolio_id, tr("SELL rejected: cannot sell %1 %2 — only %3 held.")
                                                .arg(QString::number(qty, 'g', 12), sym,
                                                     QString::number(held, 'g', 12)));
        return false;
    }
    const double sell_qty = std::min(qty, held);
    const double remaining = held - sell_qty;
    if (remaining <= psd::qty_epsilon(held))
        repo.remove_asset(portfolio_id, sym); // full sell
    else
        repo.update_asset(portfolio_id, sym, remaining, found->avg_buy_price); // WAC is unchanged by a sale

    auto tr_r = repo.add_transaction(portfolio_id, sym, "SELL", sell_qty, price, psd::normalise_trade_date(date));
    if (tr_r.is_err())
        LOG_ERROR("PortfolioSvc", "SELL applied to the position but not in the ledger: " +
                                      QString::fromStdString(tr_r.error()));

    invalidate_cache(portfolio_id);
    emit asset_sold(portfolio_id);
    return true;
}

// ── Transactions ─────────────────────────────────────────────────────────────

void PortfolioService::load_transactions(const QString& portfolio_id, int limit) {
    auto r = PortfolioRepository::instance().get_transactions(portfolio_id, limit);
    if (r.is_ok()) {
        emit transactions_loaded(r.value());
    } else {
        LOG_ERROR("PortfolioSvc", "Failed to load transactions: " + QString::fromStdString(r.error()));
    }
}

QVector<portfolio::Transaction> PortfolioService::ledger(const QString& portfolio_id) const {
    auto r = PortfolioRepository::instance().get_transactions(portfolio_id, /*limit=*/1000000);
    return r.is_ok() ? r.value() : QVector<portfolio::Transaction>{};
}

void PortfolioService::reconcile_position(const QString& portfolio_id, const QString& symbol,
                                          const QVector<portfolio::Transaction>& before) {
    auto& repo = PortfolioRepository::instance();
    auto after_r = repo.get_transactions(portfolio_id, /*limit=*/1000000);
    if (after_r.is_err())
        return;

    const auto b = psd::replay_symbol(before, symbol);
    const auto a = psd::replay_symbol(after_r.value(), symbol);

    double cur_qty = 0, cur_avg = 0;
    bool have_asset = false;
    if (auto assets = repo.get_assets(portfolio_id); assets.is_ok()) {
        for (const auto& asset : assets.value()) {
            if (asset.symbol == symbol) {
                cur_qty = asset.quantity;
                cur_avg = asset.avg_buy_price;
                have_asset = true;
                break;
            }
        }
    }

    const bool ledger_explained = std::abs(cur_qty - b.qty) <= psd::qty_epsilon(std::max(cur_qty, b.qty)) * 1e3;
    double new_qty = a.qty;
    double new_avg = a.avg_cost;
    if (!ledger_explained) {
        // The position pre-dates its ledger (e.g. imported from a broker or
        // added before transactions were recorded). Rewriting it from the
        // ledger would drop that base, so move it by the ledger delta only.
        new_qty = cur_qty + (a.qty - b.qty);
        new_avg = cur_avg;
        LOG_WARN("PortfolioSvc", QString("%1: position %2 not explained by its ledger (%3) — applied delta only")
                                     .arg(symbol)
                                     .arg(cur_qty)
                                     .arg(b.qty));
    }

    if (new_qty <= psd::qty_epsilon(std::max(cur_qty, a.qty))) {
        if (have_asset)
            repo.remove_asset(portfolio_id, symbol);
    } else if (have_asset) {
        repo.update_asset(portfolio_id, symbol, new_qty, new_avg);
    } else {
        repo.add_asset(portfolio_id, symbol, new_qty, new_avg, a.first_date);
    }
}

bool PortfolioService::update_transaction(const QString& portfolio_id, const QString& id, double qty, double price,
                                          const QString& date, const QString& notes) {
    if (!psd::positive_finite(qty) || !std::isfinite(price) || price < 0) {
        emit operation_failed(portfolio_id, tr("Edit rejected: quantity must be > 0 and price ≥ 0."));
        return false;
    }
    auto& repo = PortfolioRepository::instance();
    auto before_r = repo.get_transactions(portfolio_id, /*limit=*/1000000);
    if (before_r.is_err()) {
        emit operation_failed(portfolio_id, tr("Could not read the ledger to apply the edit."));
        return false;
    }
    const auto before = before_r.value();
    const auto it = std::find_if(before.begin(), before.end(), [&id](const auto& t) { return t.id == id; });
    if (it == before.end()) {
        emit operation_failed(portfolio_id, tr("Edit rejected: the transaction no longer exists."));
        return false;
    }
    const QString symbol = it->symbol;
    const QString type = it->transaction_type.toUpper();

    auto r = repo.update_transaction(id, qty, price, date, notes);
    if (r.is_err()) {
        LOG_ERROR("PortfolioSvc", "update_transaction failed: " + QString::fromStdString(r.error()));
        emit operation_failed(portfolio_id, tr("Could not save the edit: %1").arg(QString::fromStdString(r.error())));
        return false;
    }
    if (type == QLatin1String("BUY") || type == QLatin1String("SELL"))
        reconcile_position(portfolio_id, symbol, before);

    invalidate_cache(portfolio_id);
    // The stored NAV history was replayed from the old ledger — replay again.
    backfill_history(portfolio_id);
    emit ledger_changed(portfolio_id);
    return true;
}

bool PortfolioService::delete_transaction(const QString& id, const QString& portfolio_id) {
    auto& repo = PortfolioRepository::instance();
    auto before_r = repo.get_transactions(portfolio_id, /*limit=*/1000000);
    QString symbol;
    QString type;
    QVector<portfolio::Transaction> before;
    if (before_r.is_ok()) {
        before = before_r.value();
        for (const auto& t : before) {
            if (t.id == id) {
                symbol = t.symbol;
                type = t.transaction_type.toUpper();
                break;
            }
        }
    }

    auto r = repo.delete_transaction(id);
    if (r.is_err()) {
        LOG_ERROR("PortfolioSvc", "delete_transaction failed: " + QString::fromStdString(r.error()));
        emit operation_failed(portfolio_id,
                              tr("Could not delete the transaction: %1").arg(QString::fromStdString(r.error())));
        return false;
    }
    if (!symbol.isEmpty() && (type == QLatin1String("BUY") || type == QLatin1String("SELL")))
        reconcile_position(portfolio_id, symbol, before);

    invalidate_cache(portfolio_id);
    backfill_history(portfolio_id);
    emit ledger_changed(portfolio_id);
    return true;
}

// ── Dividend ──────────────────────────────────────────────────────────────────

bool PortfolioService::record_dividend(const QString& portfolio_id, const QString& symbol, double qty,
                                       double amount_per_share, double total, const QString& date,
                                       const QString& notes) {
    Q_UNUSED(total) // the ledger stores qty × amount/share as total_value
    if (!psd::positive_finite(qty) || !psd::positive_finite(amount_per_share)) {
        emit operation_failed(portfolio_id, tr("Dividend rejected: the holding quantity and the amount per share "
                                               "must both be greater than 0."));
        return false;
    }
    auto& repo = PortfolioRepository::instance();
    auto r = repo.add_transaction(portfolio_id, symbol.trimmed().toUpper(), "DIVIDEND", qty, amount_per_share,
                                  psd::normalise_trade_date(date), notes);
    if (r.is_err()) {
        LOG_ERROR("PortfolioSvc", "Failed to record dividend: " + QString::fromStdString(r.error()));
        emit operation_failed(portfolio_id,
                              tr("Could not record the dividend: %1").arg(QString::fromStdString(r.error())));
        return false;
    }
    invalidate_cache(portfolio_id);
    // Reload transactions so the txn panel updates, and let the summary pick up
    // the new dividend income.
    load_transactions(portfolio_id, 50);
    emit ledger_changed(portfolio_id);
    return true;
}

// ── Cache control ────────────────────────────────────────────────────────────

void PortfolioService::invalidate_cache(const QString& portfolio_id) {
    QMutexLocker lock(&cache_mutex_);
    summary_cache_.remove(portfolio_id);
}

} // namespace fincept::services
