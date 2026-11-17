// src/services/portfolio/PortfolioService.cpp
//
// Core: singleton, portfolio CRUD (load/create/delete/settings), ledger writes
// (every transaction type, with fees / taxes / currency / trade FX), position
// reconciliation, dividends, ledger replay, allocation, invalidate_cache.
// Split concerns:
//   - PortfolioService_Summary.cpp      — load/build/finalize summary (FX valuation)
//   - PortfolioService_Metrics.cpp      — market analytics, metrics, NAV history replay
//   - PortfolioService_ImportExport.cpp — CSV/JSON round-trip
// Pure logic lives in PortfolioLots / PortfolioValuation / PortfolioPerformance /
// PortfolioAllocation (unit-testable, no service state).
#include "services/portfolio/PortfolioService.h"

#include "core/logging/Logger.h"
#include "core/market/InstrumentCurrency.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "services/markets/FxRateService.h"
#include "services/portfolio/PortfolioValuation.h"
#include "services/sectors/SectorResolver.h"
#include "storage/repositories/PortfolioRepository.h"

#include <QDate>
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

/// Quoted currency of an instrument when nothing better is known.
QString psd_instrument_ccy(const QString& symbol, const QString& base) {
    const QString c = core::market::currency_for_symbol(symbol);
    return c.isEmpty() ? base : c;
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

    // Spot FX legs arrive asynchronously: a summary left incomplete for want of a
    // rate is re-valued from its stored quotes (no new quote fetch) when they land.
    connect(&FxRateService::instance(), &FxRateService::rates_changed, this, [this]() {
        if (fx_pending_.isEmpty() || fx_refresh_scheduled_)
            return;
        fx_refresh_scheduled_ = true;
        QTimer::singleShot(300, this, [this]() {
            fx_refresh_scheduled_ = false;
            revalue_pending(/*history_arrived=*/false);
        });
    });
}

void PortfolioService::revalue_pending(bool history_arrived) {
    // Only books whose missing rate is now known are re-valued: a currency Yahoo never
    // answers must not make every unrelated FX tick rebuild (and re-emit) the summary.
    auto& fx = FxRateService::instance();
    QHash<QString, PendingValuation> ready;
    for (auto it = fx_pending_.begin(); it != fx_pending_.end();) {
        const QString base = portfolio::base_currency_of(it->portfolio);
        bool go = history_arrived && it->waiting_history;
        for (const QString& c : it->waiting_spot)
            go = go || fx.rate(c, base).has_value();
        if (go) {
            ready.insert(it.key(), *it);
            it = fx_pending_.erase(it);
        } else {
            ++it;
        }
    }
    for (auto it = ready.constBegin(); it != ready.constEnd(); ++it)
        finalize_summary(it.key(), it->assets, it->portfolio, it->quotes);
}

// ── Storage failures ─────────────────────────────────────────────────────────

bool PortfolioService::is_schema_failure(const QString& raw_error) {
    const QString e = raw_error.toLower();
    return e.contains(QLatin1String("no such table")) || e.contains(QLatin1String("no such column")) ||
           e.contains(QLatin1String("has no column named")) || e.contains(QLatin1String("not a database")) ||
           e.contains(QLatin1String("malformed")) || e.contains(QLatin1String("unable to open database")) ||
           e.contains(QLatin1String("database not open")) || e.contains(QLatin1String("driver not loaded"));
}

QString PortfolioService::storage_failure_text(const QString& action, const QString& raw_error) {
    QString why;
    if (is_schema_failure(raw_error))
        why = tr("This profile's database cannot be read by this build of Fincept Terminal: its portfolio tables "
                 "are missing or were created by a different (newer) version. Your data has not been changed. "
                 "Open the profile with the build that created it, or use another profile.");
    else
        why = tr("The local database refused the change, so nothing was saved. If this keeps happening, "
                 "restart the terminal; the technical reason is in the details.");
    return action.isEmpty() ? why : action + QStringLiteral("\n\n") + why;
}

void PortfolioService::fail(const QString& portfolio_id, const QString& message, const QString& details) {
    last_failure_details_ = details;
    emit operation_failed(portfolio_id, message);
    last_failure_details_.clear();
}

void PortfolioService::fail_storage(const QString& portfolio_id, const QString& action, const std::string& raw) {
    const QString details = QString::fromStdString(raw);
    LOG_ERROR("PortfolioSvc", logf("Portfolio storage operation failed",
                                   {{"portfolio", portfolio_id}, {"action", action}, {"error", details.left(300)}}));
    fail(portfolio_id, storage_failure_text(action, details), details);
}

// ── Portfolio CRUD ───────────────────────────────────────────────────────────

void PortfolioService::load_portfolios() {
    auto r = PortfolioRepository::instance().list_portfolios();
    if (r.is_ok()) {
        load_failure_message_.clear();
        load_failure_details_.clear();
        emit portfolios_loaded(r.value());
    } else {
        // Used to be log-only: the screen then offered CREATE / DEMO / IMPORT on a
        // database that cannot hold portfolios, and each failed with a raw SQL string.
        load_failure_details_ = QString::fromStdString(r.error());
        load_failure_message_ = storage_failure_text(tr("Your portfolios could not be loaded."), load_failure_details_);
        LOG_ERROR("PortfolioSvc", "Failed to load portfolios: " + load_failure_details_);
        emit portfolios_load_failed(load_failure_message_, load_failure_details_);
    }
}

void PortfolioService::create_portfolio(const QString& name, const QString& owner, const QString& currency,
                                        const QString& description, const QString& broker_account_id) {
    auto r = PortfolioRepository::instance().create_portfolio(name, owner, currency.trimmed().toUpper(), description,
                                                             broker_account_id);
    if (r.is_ok()) {
        auto p = PortfolioRepository::instance().get_portfolio(r.value());
        if (p.is_ok())
            emit portfolio_created(p.value());
        load_portfolios();
    } else {
        fail_storage({}, tr("Could not create the portfolio \"%1\".").arg(name), r.error());
    }
}

void PortfolioService::delete_portfolio(const QString& id) {
    invalidate_cache(id);
    analytics_cache_.remove(id);
    fx_pending_.remove(id);
    auto r = PortfolioRepository::instance().delete_portfolio(id);
    if (r.is_ok()) {
        emit portfolio_deleted(id);
        load_portfolios();
    } else {
        fail_storage(id, tr("Could not delete the portfolio."), r.error());
    }
}

bool PortfolioService::update_portfolio_settings(const QString& id, const QString& benchmark,
                                                 std::optional<double> risk_free_override, bool track_cash,
                                                 const QString& lot_method, int long_term_days) {
    const QString spec = benchmark.trimmed();
    if (!spec.isEmpty() && parse_benchmark(spec).isEmpty()) {
        emit operation_failed(id, tr("Benchmark must be a symbol (SPY) or a blend such as SPY:60,AGG:40."));
        return false;
    }
    if (risk_free_override && (!std::isfinite(*risk_free_override) || *risk_free_override <= -0.05 ||
                               *risk_free_override >= 1.0)) {
        emit operation_failed(id, tr("Risk-free rate must be an annual fraction between -0.05 and 1 (0.035 = 3.5 %)."));
        return false;
    }
    const QString method = portfolio::lot_method_key(portfolio::lot_method_from_key(lot_method));
    if (long_term_days <= 0 || long_term_days > 36500) {
        emit operation_failed(id, tr("The long-term holding period must be between 1 and 36500 days."));
        return false;
    }
    auto r = PortfolioRepository::instance().update_portfolio_settings(id, spec, risk_free_override, track_cash,
                                                                      method, long_term_days);
    if (r.is_err()) {
        fail_storage(id, tr("Could not save the settings."), r.error());
        return false;
    }
    invalidate_cache(id);
    analytics_cache_.remove(id);
    // Cash tracking changes which rows are flows and what NAV is: replay history.
    schedule_history_replay(id);
    emit portfolio_settings_changed(id);
    load_portfolios();
    return true;
}

// ── Ledger writes ────────────────────────────────────────────────────────────

bool PortfolioService::add_asset(const QString& portfolio_id, const QString& symbol, double qty, double price,
                                 const QString& date, const QString& broker_symbol, const QString& exchange) {
    portfolio::Transaction t;
    t.portfolio_id = portfolio_id;
    t.symbol = symbol;
    t.transaction_type = portfolio::txn::kBuy;
    t.quantity = qty;
    t.price = price;
    t.transaction_date = date;
    return record_transaction(t, broker_symbol, exchange);
}

bool PortfolioService::sell_asset(const QString& portfolio_id, const QString& symbol, double qty, double price,
                                  const QString& date) {
    portfolio::Transaction t;
    t.portfolio_id = portfolio_id;
    t.symbol = symbol;
    t.transaction_type = portfolio::txn::kSell;
    t.quantity = qty;
    t.price = price;
    t.transaction_date = date;
    return record_transaction(t);
}

bool PortfolioService::record_split(const QString& portfolio_id, const QString& symbol, double factor,
                                    const QString& date, const QString& notes) {
    portfolio::Transaction t;
    t.portfolio_id = portfolio_id;
    t.symbol = symbol;
    t.transaction_type = portfolio::txn::kSplit;
    t.quantity = factor;
    t.price = 0;
    t.transaction_date = date;
    t.notes = notes;
    return record_transaction(t);
}

bool PortfolioService::record_transaction(const portfolio::Transaction& in, const QString& broker_symbol,
                                          const QString& exchange) {
    namespace tx = portfolio::txn;
    portfolio::Transaction t = in;
    const QString pid = t.portfolio_id;
    const QString type = t.transaction_type.trimmed().toUpper();
    t.transaction_type = type;
    t.symbol = t.symbol.trimmed().toUpper();
    t.currency = t.currency.trimmed();
    const QString date_in = t.transaction_date;
    t.transaction_date = psd::normalise_trade_date(t.transaction_date);
    const QString type_label = type.isEmpty() ? tr("Transaction") : type;

    auto reject = [this, &pid](const QString& why) {
        fail(pid, why);
        return false;
    };
    auto storage_reject = [this, &pid](const QString& action, const std::string& raw) {
        fail_storage(pid, action, raw);
        return false;
    };
    if (pid.isEmpty())
        return reject(tr("No portfolio selected."));
    if (!tx::is_valid_type(type))
        return reject(tr("Unknown transaction type '%1'.").arg(t.transaction_type));
    if (!std::isfinite(t.fees) || t.fees < 0 || !std::isfinite(t.taxes) || t.taxes < 0)
        return reject(tr("%1 rejected: fees and taxes must be 0 or more.").arg(type_label));
    if (!std::isfinite(t.fx_rate) || t.fx_rate < 0)
        return reject(tr("%1 rejected: the FX rate must be greater than 0 (or 0 = not recorded).").arg(type_label));
    if (!t.currency.isEmpty() &&
        (core::market::major_currency(t.currency).size() != 3 ||
         !std::all_of(t.currency.begin(), t.currency.end(), [](QChar c) { return c.isLetter(); })))
        return reject(tr("%1 rejected: '%2' is not a currency code.").arg(type_label, t.currency));
    const QDate d = portfolio::ledger_row_date(t);
    if (!d.isValid())
        return reject(tr("%1 rejected: the date must be YYYY-MM-DD.").arg(type_label));
    if (d > QDate::currentDate().addDays(1))
        return reject(tr("%1 rejected: the date is in the future.").arg(type_label));

    const bool position_row = type == tx::kBuy || type == tx::kSell || type == tx::kSplit;
    if (tx::is_cash_only(type)) {
        if (!psd::positive_finite(t.quantity))
            return reject(tr("%1 rejected: the amount must be greater than 0.").arg(type_label));
        if (!(std::isfinite(t.price) && t.price > 0))
            t.price = 1.0;
    } else if (t.symbol.isEmpty()) {
        return reject(tr("%1 rejected: a symbol is required.").arg(type_label));
    } else if (type == tx::kBuy || type == tx::kSell) {
        if (!psd::positive_finite(t.quantity) || !psd::positive_finite(t.price))
            return reject(tr("%1 rejected: a quantity > 0 and a price > 0 are required.").arg(type_label));
    } else if (type == tx::kSplit) {
        if (!psd::positive_finite(t.quantity) || std::abs(t.quantity - 1.0) < 1e-12)
            return reject(tr("SPLIT rejected: the factor (new units per old unit) must be > 0 and not 1."));
        t.price = 0;
    } else if (type == tx::kDividend) {
        if (!psd::positive_finite(t.price))
            return reject(tr("Dividend rejected: the amount per share must be greater than 0."));
    }

    auto& repo = PortfolioRepository::instance();
    auto before_r = repo.get_transactions(pid, /*limit=*/1000000);
    if (before_r.is_err())
        return storage_reject(tr("Could not read the ledger to record the %1.").arg(type_label), before_r.error());
    const QVector<portfolio::Transaction> before = before_r.value();

    // DIVIDEND without a quantity: the units held at the start of the ex-date.
    if (type == tx::kDividend && !(t.quantity > 0)) {
        const double q = portfolio::quantity_held_before(before, t.symbol, d);
        if (!(q > 0))
            return reject(tr("Dividend rejected: the ledger shows no %1 held before %2 — enter the quantity.")
                              .arg(t.symbol, d.toString(Qt::ISODate)));
        t.quantity = q;
    }

    // SELL: refuse an over-sell against the stored position.
    double held = 0;
    bool have_asset = false;
    if (position_row) {
        auto assets_r = repo.get_assets(pid);
        if (assets_r.is_err())
            return storage_reject(tr("Could not read the portfolio to record the %1.").arg(type_label),
                                  assets_r.error());
        for (const auto& a : assets_r.value()) {
            if (a.symbol == t.symbol) {
                held = a.quantity;
                have_asset = true;
                break;
            }
        }
        if (type == tx::kSell) {
            if (!have_asset)
                return reject(tr("SELL rejected: %1 is not held in this portfolio.").arg(t.symbol));
            if (t.quantity > held + psd::qty_epsilon(held))
                return reject(tr("SELL rejected: cannot sell %1 %2 — only %3 held.")
                                  .arg(QString::number(t.quantity, 'g', 12), t.symbol, QString::number(held, 'g', 12)));
            t.quantity = std::min(t.quantity, held);
        }
        if (type == tx::kSplit && !have_asset)
            return reject(tr("SPLIT rejected: %1 is not held in this portfolio.").arg(t.symbol));
    }

    auto ins = repo.add_transaction(t);
    if (ins.is_err()) {
        return storage_reject(t.symbol.isEmpty() ? tr("Could not record the %1.").arg(type_label)
                                                 : tr("Could not record the %1 of %2.").arg(type_label, t.symbol),
                              ins.error());
    }
    t.id = ins.value();

    if (position_row) {
        // A brand-new holding needs its row (with the broker pair) before reconciling.
        if (type == tx::kBuy && !have_asset) {
            const QString base = [&]() {
                auto p = repo.get_portfolio(pid);
                return portfolio::base_currency_of(p.is_ok() ? p.value().currency : QString(), pid);
            }();
            const QString instr = psd::psd_instrument_ccy(t.symbol, base);
            double price_quoted = t.price;
            if (!t.currency.isEmpty() &&
                core::market::major_currency(t.currency) == core::market::major_currency(instr))
                price_quoted = core::market::to_major_units(t.price, t.currency) * core::market::minor_unit_divisor(instr);
            auto ar = repo.add_asset(pid, t.symbol, t.quantity, price_quoted, t.transaction_date, QString(),
                                     broker_symbol, exchange);
            if (ar.is_err())
                LOG_ERROR("PortfolioSvc", "BUY in the ledger but the position row failed: " +
                                              QString::fromStdString(ar.error()));
        } else {
            reconcile_position(pid, t.symbol, before);
        }
    }

    invalidate_cache(pid);
    // A back-dated row changes NAV on days that already have stored snapshots.
    if (!date_in.trimmed().isEmpty() && d < QDate::currentDate())
        schedule_history_replay(pid);
    if (type == tx::kBuy)
        emit asset_added(pid);
    else if (type == tx::kSell)
        emit asset_sold(pid);
    else
        load_transactions(pid, 50);
    emit ledger_changed(pid);
    return true;
}

void PortfolioService::schedule_history_replay(const QString& portfolio_id) {
    if (portfolio_id.isEmpty())
        return;
    // We are replaying anyway: compute_metrics()' one-shot replay need not start a second one.
    backfill_attempted_.insert(portfolio_id);
    const bool first = replay_pending_.isEmpty();
    replay_pending_.insert(portfolio_id);
    if (!first)
        return;
    QTimer::singleShot(500, this, [this]() {
        const QSet<QString> pending = std::exchange(replay_pending_, QSet<QString>{});
        for (const QString& pid : pending)
            backfill_history(pid);
    });
}

QStringList PortfolioService::held_symbols() {
    QStringList out;
    auto& repo = PortfolioRepository::instance();
    auto ports = repo.list_portfolios();
    if (ports.is_err())
        return out;
    for (const auto& p : ports.value()) {
        auto assets = repo.get_assets(p.id);
        if (assets.is_err())
            continue;
        for (const auto& a : assets.value()) {
            if (!a.symbol.isEmpty() && a.quantity > 0 && !out.contains(a.symbol))
                out.append(a.symbol);
        }
    }
    return out;
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

portfolio::LedgerOptions PortfolioService::ledger_options(const portfolio::Portfolio& p,
                                                          const QHash<QString, QString>& quote_ccy) const {
    portfolio::LedgerOptions opt;
    opt.method = portfolio::lot_method_from_key(p.lot_method);
    opt.long_term_days = p.long_term_days > 0 ? p.long_term_days : 365;
    opt.base_currency = portfolio::base_currency_of(p);
    opt.track_cash = p.track_cash;
    const QString base = opt.base_currency;
    opt.instrument_currency = [quote_ccy, base](const QString& symbol) {
        const QString q = quote_ccy.value(symbol.trimmed().toUpper());
        return q.isEmpty() ? psd::psd_instrument_ccy(symbol, base) : q;
    };
    // Snapshot of the cached daily FX history: the lambda may run on a worker thread.
    QHash<QString, QMap<QDate, double>> hist;
    for (auto it = fx_history_.constBegin(); it != fx_history_.constEnd(); ++it)
        if (it.key().endsWith(QLatin1Char('>') + base))
            hist.insert(it.key().section(QLatin1Char('>'), 0, 0), it.value());
    opt.historical_fx = [hist](const QString& ccy, const QDate& date) -> std::optional<double> {
        const auto it = hist.constFind(ccy);
        if (it == hist.constEnd() || it->isEmpty())
            return std::nullopt;
        auto lb = it->upperBound(date); // first > date
        if (lb == it->constBegin())
            return std::nullopt;
        --lb;
        if (lb.key().daysTo(date) > 5)
            return std::nullopt;
        return lb.value();
    };
    return opt;
}

portfolio::LedgerResult PortfolioService::replay_ledger(const QString& portfolio_id,
                                                        std::optional<portfolio::LotMethod> method,
                                                        int long_term_days) const {
    auto& repo = PortfolioRepository::instance();
    auto p = repo.get_portfolio(portfolio_id);
    if (p.is_err())
        return {};
    portfolio::LedgerOptions opt = ledger_options(p.value());
    if (method)
        opt.method = *method;
    if (long_term_days > 0)
        opt.long_term_days = long_term_days;
    auto txns = repo.get_transactions(portfolio_id, /*limit=*/1000000);
    if (txns.is_err())
        return {};
    return portfolio::replay_ledger(txns.value(), opt);
}

void PortfolioService::reconcile_position(const QString& portfolio_id, const QString& symbol,
                                          const QVector<portfolio::Transaction>& before) {
    auto& repo = PortfolioRepository::instance();
    auto after_r = repo.get_transactions(portfolio_id, /*limit=*/1000000);
    auto port_r = repo.get_portfolio(portfolio_id);
    if (after_r.is_err() || port_r.is_err())
        return;
    const portfolio::Portfolio port = port_r.value();
    portfolio::LedgerOptions opt = ledger_options(port);
    const auto b_res = portfolio::replay_ledger(before, opt);
    const auto a_res = portfolio::replay_ledger(after_r.value(), opt);
    const auto b = b_res.positions.value(symbol);
    const auto a = a_res.positions.value(symbol);

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

    const QString instr = psd::psd_instrument_ccy(symbol, port.currency);
    const double divisor = core::market::minor_unit_divisor(instr);
    const bool ledger_explained = std::abs(cur_qty - b.quantity) <= psd::qty_epsilon(std::max(cur_qty, b.quantity)) * 1e3;
    double new_qty = a.quantity;
    double new_avg = (a.quantity > 0 && std::isfinite(a.cost_local)) ? a.cost_local / a.quantity * divisor : cur_avg;
    if (!ledger_explained) {
        // The position pre-dates its ledger (imported from a broker, or added before
        // transactions were recorded). Rewriting it from the ledger would drop that
        // base, so move it by the ledger delta only. A split scales the stored
        // quantity and average price by its factor; a buy blends its price in.
        double factor = 1.0;
        for (const auto& t : after_r.value()) {
            if (t.transaction_type.toUpper() != portfolio::txn::kSplit || t.symbol != symbol)
                continue;
            const bool existed = std::any_of(before.begin(), before.end(), [&t](const auto& o) { return o.id == t.id; });
            if (!existed && t.quantity > 0)
                factor *= t.quantity;
        }
        const double delta = a.quantity - b.quantity * factor;
        new_qty = cur_qty * factor + delta;
        new_avg = factor > 0 ? cur_avg / factor : cur_avg;
        if (delta > 0 && new_qty > 0 && std::isfinite(a.cost_local) && std::isfinite(b.cost_local)) {
            const double added_cost_quoted = (a.cost_local - b.cost_local) * divisor;
            new_avg = (cur_qty * factor * new_avg + added_cost_quoted) / new_qty;
        }
        LOG_WARN("PortfolioSvc", QString("%1: position %2 not explained by its ledger (%3) — applied delta only")
                                     .arg(symbol)
                                     .arg(cur_qty)
                                     .arg(b.quantity));
    }

    if (new_qty <= psd::qty_epsilon(std::max(cur_qty, a.quantity))) {
        if (have_asset)
            repo.remove_asset(portfolio_id, symbol);
    } else if (have_asset) {
        repo.update_asset(portfolio_id, symbol, new_qty, new_avg);
    } else {
        repo.add_asset(portfolio_id, symbol, new_qty, new_avg,
                       a.first_date.isValid() ? a.first_date.toString(Qt::ISODate) : QString());
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
        fail_storage(portfolio_id, tr("Could not read the ledger to apply the edit."), before_r.error());
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
        fail_storage(portfolio_id, tr("Could not save the edit."), r.error());
        return false;
    }
    if (type == portfolio::txn::kBuy || type == portfolio::txn::kSell || type == portfolio::txn::kSplit)
        reconcile_position(portfolio_id, symbol, before);

    invalidate_cache(portfolio_id);
    schedule_history_replay(portfolio_id); // the stored NAV history was replayed from the old ledger
    emit ledger_changed(portfolio_id);
    return true;
}

bool PortfolioService::update_transaction_costs(const QString& portfolio_id, const QString& id, double fees,
                                                double taxes, const QString& currency, double fx_rate,
                                                const QString& lot_id) {
    if (!std::isfinite(fees) || fees < 0 || !std::isfinite(taxes) || taxes < 0 || !std::isfinite(fx_rate) ||
        fx_rate < 0) {
        emit operation_failed(portfolio_id, tr("Edit rejected: fees, taxes and FX rate must be 0 or more."));
        return false;
    }
    auto& repo = PortfolioRepository::instance();
    auto before_r = repo.get_transactions(portfolio_id, /*limit=*/1000000);
    if (before_r.is_err()) {
        fail_storage(portfolio_id, tr("Could not read the ledger to apply the edit."), before_r.error());
        return false;
    }
    const auto before = before_r.value();
    const auto it = std::find_if(before.begin(), before.end(), [&id](const auto& t) { return t.id == id; });
    if (it == before.end()) {
        emit operation_failed(portfolio_id, tr("Edit rejected: the transaction no longer exists."));
        return false;
    }
    auto r = repo.update_transaction_costs(id, fees, taxes, currency, fx_rate, lot_id);
    if (r.is_err()) {
        fail_storage(portfolio_id, tr("Could not save the edit."), r.error());
        return false;
    }
    const QString type = it->transaction_type.toUpper();
    if (type == portfolio::txn::kBuy || type == portfolio::txn::kSell)
        reconcile_position(portfolio_id, it->symbol, before);
    invalidate_cache(portfolio_id);
    schedule_history_replay(portfolio_id);
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
        fail_storage(portfolio_id, tr("Could not delete the transaction."), r.error());
        return false;
    }
    if (!symbol.isEmpty() &&
        (type == portfolio::txn::kBuy || type == portfolio::txn::kSell || type == portfolio::txn::kSplit))
        reconcile_position(portfolio_id, symbol, before);

    invalidate_cache(portfolio_id);
    schedule_history_replay(portfolio_id);
    emit ledger_changed(portfolio_id);
    return true;
}

// ── Dividend ──────────────────────────────────────────────────────────────────

bool PortfolioService::record_dividend(const QString& portfolio_id, const QString& symbol, double qty,
                                       double amount_per_share, double total, const QString& date,
                                       const QString& notes) {
    Q_UNUSED(total) // the ledger stores qty × amount/share as total_value
    const QString sym = symbol.trimmed().toUpper();
    const QString d_str = psd::normalise_trade_date(date);
    const QDate d = QDate::fromString(d_str.left(10), Qt::ISODate);
    // Entitlement is the quantity held at the ex-date, not today's: ask the ledger
    // first, fall back to the caller's figure only when the ledger cannot tell.
    const auto txns = ledger(portfolio_id);
    const bool ledger_knows = std::any_of(txns.begin(), txns.end(), [&sym](const auto& t) {
        return t.symbol == sym && t.transaction_type.toUpper() == portfolio::txn::kBuy;
    });
    double entitled = qty;
    if (ledger_knows && d.isValid()) {
        const double q = portfolio::quantity_held_before(txns, sym, d);
        if (q > 0)
            entitled = q;
    }
    if (!psd::positive_finite(entitled) || !psd::positive_finite(amount_per_share)) {
        emit operation_failed(portfolio_id, tr("Dividend rejected: the holding quantity and the amount per share "
                                               "must both be greater than 0."));
        return false;
    }
    portfolio::Transaction t;
    t.portfolio_id = portfolio_id;
    t.symbol = sym;
    t.transaction_type = portfolio::txn::kDividend;
    t.quantity = entitled;
    t.price = amount_per_share;
    t.transaction_date = d_str;
    t.notes = notes;
    return record_transaction(t);
}

// ── Allocation ───────────────────────────────────────────────────────────────

portfolio::AllocationReport PortfolioService::allocation(const portfolio::PortfolioSummary& summary) {
    const QString pid = summary.portfolio.id;
    QHash<QString, InfoData> info;
    for (const auto& h : summary.holdings) {
        const auto it = info_cache_.constFind(h.symbol);
        if (it != info_cache_.constEnd()) {
            info.insert(h.symbol, *it);
            continue;
        }
        // Not cached yet: take the hub's value if it has one, else subscribe (the
        // hub's cold-start fetch requests it) and tell the caller when it lands.
        info_waiters_[h.symbol].insert(pid);
        const QString topic = MarketDataService::info_topic(h.symbol);
        const QVariant v = datahub::DataHub::instance().peek(topic);
        if (v.isValid() && v.canConvert<InfoData>()) {
            info_cache_.insert(h.symbol, v.value<InfoData>());
            info.insert(h.symbol, info_cache_.value(h.symbol));
            continue;
        }
        if (info_subscribed_.contains(h.symbol))
            continue;
        info_subscribed_.insert(h.symbol);
        const QString sym = h.symbol;
        datahub::DataHub::instance().subscribe(this, topic, [this, sym](const QVariant& value) {
            if (!value.canConvert<InfoData>())
                return;
            info_cache_.insert(sym, value.value<InfoData>());
            const QSet<QString> waiters = info_waiters_.take(sym);
            for (const QString& w : waiters)
                emit allocation_updated(w);
        });
    }
    auto& fx = FxRateService::instance();
    // USD on purpose: the market-cap buckets (mega >= 200 bn ...) are defined in US dollars.
    return portfolio::compute_allocation(summary, info,
                                         [&fx](const QString& ccy) { return fx.rate(ccy, QStringLiteral("USD")); });
}

// ── Cache control ────────────────────────────────────────────────────────────

void PortfolioService::invalidate_cache(const QString& portfolio_id) {
    QMutexLocker lock(&cache_mutex_);
    summary_cache_.remove(portfolio_id);
}

} // namespace fincept::services
