// src/services/portfolio/PortfolioService_Summary.cpp
//
// Summary loading: load_summary, refresh_summary, build_summary,
// try_broker_quotes, finalize_summary. These produce the PortfolioSummary
// object the UI binds to — valued in the portfolio's base currency
// (PortfolioValuation.h), with an explicit incomplete state.
//
// Part of the partial-class split of PortfolioService.cpp.

#include "core/logging/Logger.h"
#include "core/market/InstrumentCurrency.h"
#include "core/market/MarketSessions.h"
#include "python/PythonRunner.h"
#include "services/markets/FxRateService.h"
#include "services/portfolio/PortfolioService.h"
#include "services/portfolio/PortfolioValuation.h"
#include "services/sectors/SectorResolver.h"
#include "storage/repositories/CustomIndexRepository.h"
#include "storage/repositories/PortfolioRepository.h"
#include "storage/repositories/SettingsRepository.h"
#include "trading/AccountManager.h"
#include "trading/BrokerInterface.h"
#include "trading/BrokerRegistry.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QtConcurrent>

#include <algorithm>
#include <cmath>
#include <memory>
#include <numeric>

namespace fincept::services {

void PortfolioService::load_summary(const QString& portfolio_id) {
    // Check cache first (P11)
    {
        QMutexLocker lock(&cache_mutex_);
        auto it = summary_cache_.find(portfolio_id);
        if (it != summary_cache_.end()) {
            qint64 now = QDateTime::currentSecsSinceEpoch();
            if (now - it->timestamp < kCacheTtlSec) {
                emit summary_loaded(it->summary);
                return;
            }
        }
    }

    auto portfolio_r = PortfolioRepository::instance().get_portfolio(portfolio_id);
    // The raw driver message goes to the log; listeners get the translated text.
    if (portfolio_r.is_err()) {
        const QString raw = QString::fromStdString(portfolio_r.error());
        LOG_ERROR("PortfolioSvc", logf("Portfolio summary failed: the portfolio could not be read",
                                       {{"portfolio", portfolio_id}, {"error", raw.left(300)}}));
        emit summary_error(portfolio_id, storage_failure_text(tr("This portfolio could not be read."), raw));
        return;
    }

    auto assets_r = PortfolioRepository::instance().get_assets(portfolio_id);
    if (assets_r.is_err()) {
        const QString raw = QString::fromStdString(assets_r.error());
        LOG_ERROR("PortfolioSvc", logf("Portfolio summary failed: the holdings could not be read",
                                       {{"portfolio", portfolio_id}, {"error", raw.left(300)}}));
        emit summary_error(portfolio_id, storage_failure_text(tr("The holdings of this portfolio could not be read."), raw));
        return;
    }

    if (assets_r.value().isEmpty()) {
        // No positions: still value the cash / ledger totals (a cash-only book is a book).
        finalize_summary(portfolio_id, {}, portfolio_r.value(), {});
        return;
    }

    build_summary(portfolio_id, assets_r.value(), portfolio_r.value());
}

void PortfolioService::refresh_summary(const QString& portfolio_id) {
    invalidate_cache(portfolio_id);
    load_summary(portfolio_id);
}

void PortfolioService::build_summary(const QString& portfolio_id, const QVector<portfolio::PortfolioAsset>& assets,
                                     const portfolio::Portfolio& portfolio) {
    // Routing decision: broker-first when the portfolio is linked to a
    // connected broker account (broker_account_id non-empty + creds present
    // + broker exposes get_quotes). yfinance is the universal fallback.
    if (try_broker_quotes(portfolio_id, assets, portfolio))
        return;

    // ── yfinance path (legacy / unlinked portfolios) ─────────────────────────
    QStringList symbols;
    symbols.reserve(assets.size());
    for (const auto& a : assets)
        symbols.append(a.symbol);

    QPointer<PortfolioService> self = this;
    MarketDataService::instance().fetch_quotes(
        symbols, [self, portfolio_id, assets, portfolio](bool ok, QVector<QuoteData> quotes) {
            if (!self)
                return;
            QHash<QString, QuoteData> quote_map;
            if (ok) {
                for (const auto& q : quotes)
                    quote_map[q.symbol] = q;
            }
            self->finalize_summary(portfolio_id, assets, portfolio, quote_map);
        });
}

bool PortfolioService::try_broker_quotes(const QString& portfolio_id, const QVector<portfolio::PortfolioAsset>& assets,
                                         const portfolio::Portfolio& portfolio) {
    if (portfolio.broker_account_id.isEmpty())
        return false; // unlinked portfolio — yfinance owns it.

    // Resolve broker + creds. Any of these missing → fall back silently.
    auto& acct_mgr = trading::AccountManager::instance();
    const auto account = acct_mgr.get_account(portfolio.broker_account_id);
    if (account.account_id.isEmpty() || account.broker_id.isEmpty()) {
        LOG_WARN("PortfolioSvc", QString("broker_account_id %1 not found — falling back to yfinance for %2")
                                     .arg(portfolio.broker_account_id, portfolio_id));
        return false;
    }

    auto* broker = trading::BrokerRegistry::instance().get(account.broker_id);
    if (!broker) {
        LOG_WARN("PortfolioSvc", QString("broker '%1' not registered — falling back to yfinance for %2")
                                     .arg(account.broker_id, portfolio_id));
        return false;
    }

    const auto creds = acct_mgr.load_credentials(portfolio.broker_account_id);
    if (creds.access_token.isEmpty()) {
        LOG_WARN("PortfolioSvc", QString("broker account %1 has no access_token — falling back to yfinance for %2")
                                     .arg(portfolio.broker_account_id, portfolio_id));
        return false;
    }

    // Build the broker-native key list. Each asset stores broker_symbol +
    // exchange (populated at import time). Skip assets without a broker
    // pair — those slipped in before the v022 link or are non-broker rows
    // mixed into a broker-linked portfolio. They get no quote from the broker,
    // so they are reported unpriced (summary incomplete) — never valued at cost.
    QVector<QString> broker_keys;
    QHash<QString, QString> key_to_yf; // EXCHANGE:SYMBOL → asset.symbol (yfinance)
    broker_keys.reserve(assets.size());
    for (const auto& a : assets) {
        if (a.broker_symbol.isEmpty() || a.exchange.isEmpty())
            continue;
        const QString key = a.exchange + ":" + a.broker_symbol;
        broker_keys.append(key);
        key_to_yf.insert(key, a.symbol);
    }
    if (broker_keys.isEmpty()) {
        // Linked portfolio but no asset has a broker pair — nothing to fetch
        // via broker. Fall back to yfinance so the portfolio still renders.
        LOG_WARN("PortfolioSvc",
                 QString("broker-linked portfolio %1 has no broker_symbol/exchange on any asset — yfinance fallback")
                     .arg(portfolio_id));
        return false;
    }

    // BrokerHttp::execute blocks on QEventLoop (memory: project_broker_http_blocking).
    // Wrap the broker call in QtConcurrent::run so the UI thread stays responsive.
    // P8: capture a QPointer; result is posted back via QMetaObject::invokeMethod.
    // The QFuture return value is intentionally discarded — we don't need to
    // wait or cancel; lifetime is governed by the captured QPointer guards.
    QPointer<PortfolioService> self = this;
    (void)QtConcurrent::run([self, portfolio_id, assets, portfolio, broker, creds, broker_keys, key_to_yf]() {
        const auto resp = broker->get_quotes(creds, broker_keys);

        // Hop back to the UI thread (the one that owns *self) before touching
        // any shared state — finalize_summary writes to the cache mutex and
        // emits a signal that drives UI updates.
        QMetaObject::invokeMethod(
            self,
            [self, portfolio_id, assets, portfolio, resp, key_to_yf]() {
                if (!self)
                    return;

                QHash<QString, QuoteData> quote_map;
                if (resp.success && resp.data.has_value()) {
                    for (const auto& bq : resp.data.value()) {
                        // bq.symbol is the EXCHANGE:SYMBOL key (Zerodha echoes the
                        // request key in its response). Translate back to the
                        // canonical yfinance key the rest of the pipeline uses.
                        const QString yf_key = key_to_yf.value(bq.symbol);
                        if (yf_key.isEmpty())
                            continue; // unknown key — broker returned a row we didn't ask for, skip.

                        QuoteData q;
                        q.symbol = yf_key;
                        q.name = yf_key; // brokers don't return a friendly name on /quote.
                        q.price = bq.ltp;
                        q.change = bq.change;
                        q.change_pct = bq.change_pct;
                        q.high = bq.high;
                        q.low = bq.low;
                        q.volume = bq.volume;
                        quote_map.insert(yf_key, q);
                    }
                    LOG_DEBUG("PortfolioSvc", QString("Broker quotes: %1 of %2 for portfolio %3")
                                                 .arg(quote_map.size())
                                                 .arg(key_to_yf.size())
                                                 .arg(portfolio_id));
                } else {
                    // Broker call failed — log and fall back to yfinance for
                    // _this_ refresh. The portfolio stays linked; next refresh
                    // will retry the broker.
                    LOG_WARN("PortfolioSvc", QString("broker get_quotes failed for %1: %2 — falling back to yfinance")
                                                 .arg(portfolio_id, resp.error.left(200)));
                    QStringList symbols;
                    symbols.reserve(assets.size());
                    for (const auto& a : assets)
                        symbols.append(a.symbol);
                    MarketDataService::instance().fetch_quotes(
                        symbols, [self, portfolio_id, assets, portfolio](bool ok, QVector<QuoteData> quotes) {
                            if (!self)
                                return;
                            QHash<QString, QuoteData> qm;
                            if (ok)
                                for (const auto& q : quotes)
                                    qm.insert(q.symbol, q);
                            self->finalize_summary(portfolio_id, assets, portfolio, qm);
                        });
                    return;
                }

                self->finalize_summary(portfolio_id, assets, portfolio, quote_map);
            },
            Qt::QueuedConnection);
    });

    return true; // broker path was taken
}

// ── FX history for trade-date cost in the base currency ───────────────────────

std::optional<double> PortfolioService::historical_fx(const QString& ccy, const QString& base, const QDate& date) const {
    const QString c = core::market::major_currency(ccy);
    const QString b = core::market::major_currency(base);
    if (c == b)
        return 1.0;
    const auto it = fx_history_.constFind(c + QLatin1Char('>') + b);
    if (it == fx_history_.constEnd() || it->isEmpty())
        return std::nullopt;
    auto ub = it->upperBound(date);
    if (ub == it->constBegin())
        return std::nullopt;
    --ub;
    if (ub.key().daysTo(date) > 5)
        return std::nullopt;
    return ub.value();
}

void PortfolioService::request_fx_history(const QString& portfolio_id, const QStringList& ccys, const QString& base,
                                          const QDate& from) {
    Q_UNUSED(portfolio_id)
    const QDate today = QDate::currentDate();
    for (const QString& ccy : ccys) {
        const QString key = ccy + QLatin1Char('>') + base;
        if (fx_history_.contains(key) || fx_history_requested_.contains(key))
            continue;
        fx_history_requested_.insert(key);
        QPointer<PortfolioService> self = this;
        FxRateService::instance().historical_rates(
            ccy, base, from.addDays(-7), today,
            [self, key](bool ok, const QMap<QDate, double>& rates, const QString& error) {
                if (!self)
                    return;
                self->fx_history_requested_.remove(key);
                // A failure is cached as "no history" so the lookup stops waiting; the
                // trade-date FX of those rows stays unknown (fx_pnl absent), never invented.
                self->fx_history_.insert(key, ok ? rates : QMap<QDate, double>{});
                if (!ok)
                    LOG_WARN("PortfolioSvc", QString("FX history %1 unavailable: %2").arg(key, error.left(200)));
                if (self->fx_pending_.isEmpty())
                    return;
                QTimer::singleShot(200, self, [self]() {
                    if (self)
                        self->revalue_pending(/*history_arrived=*/true);
                });
            });
    }
}

void PortfolioService::finalize_summary(const QString& portfolio_id, const QVector<portfolio::PortfolioAsset>& assets,
                                        const portfolio::Portfolio& portfolio,
                                        const QHash<QString, QuoteData>& quote_map) {
    // Quote currencies tell the ledger which currency its legacy (currency-less) rows were in.
    QHash<QString, QString> quote_ccy;
    for (auto it = quote_map.constBegin(); it != quote_map.constEnd(); ++it)
        if (!it->currency.isEmpty())
            quote_ccy.insert(it.key().toUpper(), it->currency);
    // Thread-safe snapshot (the FX-history lookup copies the cache).
    const portfolio::LedgerOptions opt = ledger_options(portfolio, quote_ccy);

    // Reading the whole ledger and replaying it ran on the GUI thread on every
    // refresh (each quote tick, FX leg, demo / import burst), stalling the UI for
    // large books: read + replay on a worker, value and emit back on this thread.
    // A newer request for the same portfolio supersedes one still running.
    const quint64 seq = ++summary_seq_[portfolio_id];
    QPointer<PortfolioService> self = this;
    (void)QtConcurrent::run([self, portfolio_id, assets, portfolio, quote_map, opt, seq]() {
        auto txns = std::make_shared<QVector<portfolio::Transaction>>();
        if (auto r = PortfolioRepository::instance().get_transactions(portfolio_id, /*limit=*/1000000); r.is_ok())
            *txns = r.value();
        auto ledger = std::make_shared<portfolio::LedgerResult>(portfolio::replay_ledger(*txns, opt));
        QMetaObject::invokeMethod(
            self,
            [self, portfolio_id, assets, portfolio, quote_map, opt, seq, txns, ledger]() {
                if (!self || self->summary_seq_.value(portfolio_id) != seq)
                    return; // superseded by a newer refresh
                self->finish_summary(portfolio_id, assets, portfolio, quote_map, opt, *txns, *ledger);
            },
            Qt::QueuedConnection);
    });
}

void PortfolioService::finish_summary(const QString& portfolio_id, const QVector<portfolio::PortfolioAsset>& assets,
                                      const portfolio::Portfolio& portfolio, const QHash<QString, QuoteData>& quote_map,
                                      const portfolio::LedgerOptions& opt,
                                      const QVector<portfolio::Transaction>& txns,
                                      const portfolio::LedgerResult& ledger) {
    auto& repo = PortfolioRepository::instance();
    const QString base = portfolio::base_currency_of(portfolio);

    // Rows in a foreign currency without their own trade FX need the daily history.
    QStringList missing_hist;
    QDate earliest;
    for (const auto& t : txns) {
        if (t.fx_rate > 0)
            continue;
        const QString type = t.transaction_type.toUpper();
        const QString quoted = !t.currency.isEmpty() ? t.currency
                               : (portfolio::txn::is_cash_only(type) || t.symbol.isEmpty())
                                   ? base
                                   : opt.instrument_currency(t.symbol);
        const QString ccy = core::market::major_currency(quoted);
        if (ccy.isEmpty() || ccy == base)
            continue;
        const QDate d = portfolio::ledger_row_date(t);
        if (d.isValid() && (!earliest.isValid() || d < earliest))
            earliest = d;
        if (!fx_history_.contains(ccy + QLatin1Char('>') + base) && !missing_hist.contains(ccy))
            missing_hist.append(ccy);
    }
    if (!missing_hist.isEmpty() && earliest.isValid())
        request_fx_history(portfolio_id, missing_hist, base, earliest);

    portfolio::ValuationInputs in;
    in.portfolio = portfolio;
    in.assets = assets;
    in.quotes = quote_map;
    in.ledger = &ledger;
    auto& fx = FxRateService::instance();
    // The quotes behind each conversion are kept: a rate older than the user's "FX stale
    // after" setting is still used (never dropped or replaced by 1.0) but named in the
    // summary, so the views can mark the converted totals approximate and date the rate.
    QHash<QString, FxQuote> used_fx;
    in.spot_fx = [&fx, &used_fx, base](const QString& ccy) -> std::optional<double> {
        const auto q = fx.quote(ccy, base);
        if (!q)
            return std::nullopt;
        used_fx.insert(ccy, *q);
        return q->rate;
    };
    in.sector_for = [](const QString& s) { return SectorResolver::instance().sector_for(s); };
    portfolio::PortfolioSummary summary = portfolio::value_portfolio(in);
    in.spot_fx = nullptr; // captured locals end here
    for (auto it = used_fx.constBegin(); it != used_fx.constEnd(); ++it) {
        if (!it->stale)
            continue;
        summary.fx_stale.append(it.key());
        if (it->as_of.isValid() && (!summary.fx_stale_as_of.isValid() || it->as_of < summary.fx_stale_as_of))
            summary.fx_stale_as_of = it->as_of;
    }
    summary.fx_stale.sort();

    // Subscribe every leg this book needs (kept fresh by the hub from then on).
    const QStringList ccys = portfolio::valuation_currencies(in);
    if (!ccys.isEmpty())
        fx.ensure_currencies(ccys, base);
    bool history_pending = false;
    for (const QString& c : missing_hist)
        history_pending = history_pending || fx_history_requested_.contains(c + QLatin1Char('>') + base);
    QStringList waiting_spot;
    for (const QString& c : ccys)
        if (!fx.rate(c, base).has_value())
            waiting_spot.append(c);
    if (!waiting_spot.isEmpty() || history_pending)
        fx_pending_.insert(portfolio_id, {assets, portfolio, quote_map, waiting_spot, history_pending});
    else
        fx_pending_.remove(portfolio_id);

    {
        const RiskFree rf = risk_free_for(portfolio);
        rf_rate_ = rf.rate;
        rf_source_ = rf.source;
    }

    // An incomplete book (a holding without a quote or an FX rate) is shown, but it
    // is neither cached nor snapshotted: its total is not the book's NAV, and
    // save_snapshot() is INSERT OR REPLACE keyed by day — a partial total would
    // overwrite the real NAV and put a fake jump into every return statistic.
    if (!summary.complete) {
        LOG_WARN("PortfolioSvc", QString("Summary for %1 incomplete (%2 of %3 valued; unpriced: %4; no FX: %5) — "
                                         "not cached or snapshotted")
                                     .arg(portfolio_id)
                                     .arg(summary.valued_positions)
                                     .arg(summary.total_positions)
                                     .arg(summary.unpriced.join(QLatin1Char(',')).left(200),
                                          summary.fx_missing.join(QLatin1Char(',')).left(200)));
        emit summary_loaded(summary);
        return;
    }

    {
        QMutexLocker lock(&cache_mutex_);
        summary_cache_[portfolio_id] = {summary, QDateTime::currentSecsSinceEpoch()};
    }

    if (!assets.isEmpty() || (portfolio.track_cash && !summary.cash_balances.isEmpty())) {
        // Snapshot under the session date the prices belong to (the quotes' own
        // market date) — the NAV replay writes exchange dates too, so a late-evening
        // refresh no longer files today's prices under tomorrow's local date.
        QString snap_date = summary.as_of;
        if (snap_date.isEmpty()) {
            const auto legs = parse_benchmark(benchmark_for(portfolio));
            const auto* venue = core::market::venue_for_symbol(legs.isEmpty() ? QStringLiteral("SPY") : legs.first().first);
            snap_date = venue ? core::market::trading_date(*venue, QDateTime::currentDateTimeUtc()).toString(Qt::ISODate)
                              : QDate::currentDate().toString(Qt::ISODate);
        }
        const double cost = summary.total_cost_basis + (summary.cash_in_nav ? summary.cash_value : 0.0);
        const double pnl = summary.total_market_value - cost;
        const double pnl_pct = cost > 0 ? pnl / cost * 100.0 : 0.0;
        if (repo.save_snapshot(portfolio_id, summary.total_market_value, cost, pnl, pnl_pct, snap_date).is_ok())
            summary.snapshot_date = snap_date;
        record_custom_index_values(summary);
    }

    emit summary_loaded(summary);
}

// ── Custom indices ───────────────────────────────────────────────────────────

double PortfolioService::compute_custom_index_value(const fincept::CustomIndex& idx,
                                                    const portfolio::PortfolioSummary& summary) {
    if (idx.constituents.isEmpty())
        return idx.base_value;

    // symbol → live price (quoted units, the same units price_at_create was taken in).
    QHash<QString, double> price_map;
    for (const auto& h : summary.holdings)
        if (h.priced && std::isfinite(h.current_price) && h.current_price > 0)
            price_map.insert(h.symbol, h.current_price);

    // Every constituent needs a live price: a level built from stale creation prices
    // would be recorded as if it were real.
    for (const auto& c : idx.constituents)
        if (!price_map.contains(c.symbol) || !(c.price_at_create > 0.0))
            return portfolio::kAbsent;

    const int n = static_cast<int>(idx.constituents.size());
    const QString& method = idx.method;

    if (method == QLatin1String("Price Weighted")) {
        // Sum of current prices / sum of creation prices × base (Dow-style divisor).
        double sum_cur = 0.0, sum_base = 0.0;
        for (const auto& c : idx.constituents) {
            sum_cur += price_map.value(c.symbol);
            sum_base += c.price_at_create;
        }
        return sum_base > 0.0 ? sum_cur / sum_base * idx.base_value : portfolio::kAbsent;
    }
    if (method == QLatin1String("Equal Weighted")) {
        double ratio_sum = 0.0;
        for (const auto& c : idx.constituents)
            ratio_sum += price_map.value(c.symbol) / c.price_at_create;
        return ratio_sum / n * idx.base_value;
    }
    if (method == QLatin1String("Geometric Mean")) {
        double log_sum = 0.0;
        for (const auto& c : idx.constituents)
            log_sum += std::log(price_map.value(c.symbol) / c.price_at_create);
        return std::exp(log_sum / n) * idx.base_value;
    }

    // "Portfolio Weighted": Σ wᵢ·(Pᵢ/P₀ᵢ) / Σ wᵢ × base, wᵢ = the constituent's weight
    // when the index was created (a buy-and-hold basket). Indices stored under the
    // retired names (Market Cap Weighted, Float Adjusted, Fundamental Weighted,
    // Modified Market Cap, Factor Weighted, Risk Parity, Capped Weighted) were always
    // this computation; they keep it, and the view labels them as such.
    double weighted_ratio = 0.0, total_weight = 0.0;
    for (const auto& c : idx.constituents) {
        if (c.weight > 0.0) {
            weighted_ratio += c.weight * (price_map.value(c.symbol) / c.price_at_create);
            total_weight += c.weight;
        }
    }
    return total_weight > 0.0 ? weighted_ratio / total_weight * idx.base_value : portfolio::kAbsent;
}

void PortfolioService::record_custom_index_values(const portfolio::PortfolioSummary& summary) {
    if (summary.holdings.isEmpty() || !summary.complete)
        return;
    auto indices = CustomIndexRepository::instance().list_all();
    if (indices.is_err())
        return;

    // Prices in `summary` describe THIS portfolio only — an index built on a
    // different one would be scored against the wrong prices.
    const QString day = summary.snapshot_date.isEmpty() ? summary.as_of : summary.snapshot_date;
    const QString today = day.isEmpty() ? QDate::currentDate().toString(Qt::ISODate) : day;
    for (const auto& idx : indices.value()) {
        if (idx.portfolio_id != summary.portfolio.id)
            continue;
        const double level = compute_custom_index_value(idx, summary);
        if (!std::isfinite(level) || level <= 0.0)
            continue;
        CustomIndexRepository::instance().save_value(idx.id, today, level);
    }
}

} // namespace fincept::services
