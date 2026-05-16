// src/services/portfolio/PortfolioService_Summary.cpp
//
// Summary loading: load_summary, refresh_summary, build_summary,
// try_broker_quotes, finalize_summary. These produce the PortfolioSummary
// object the UI binds to.
//
// Part of the partial-class split of PortfolioService.cpp.

#include "services/portfolio/PortfolioService.h"

#include "core/logging/Logger.h"
#include "python/PythonRunner.h"
#include "services/sectors/SectorResolver.h"
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
    if (portfolio_r.is_err()) {
        emit summary_error(portfolio_id, QString::fromStdString(portfolio_r.error()));
        return;
    }

    auto assets_r = PortfolioRepository::instance().get_assets(portfolio_id);
    if (assets_r.is_err()) {
        emit summary_error(portfolio_id, QString::fromStdString(assets_r.error()));
        return;
    }

    if (assets_r.value().isEmpty()) {
        // Empty portfolio — emit summary with zero values
        portfolio::PortfolioSummary empty;
        empty.portfolio = portfolio_r.value();
        empty.last_updated = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        emit summary_loaded(empty);
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
    MarketDataService::instance().fetch_quotes(symbols, [self, portfolio_id, assets,
                                                         portfolio](bool ok, QVector<QuoteData> quotes) {
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

bool PortfolioService::try_broker_quotes(const QString& portfolio_id,
                                         const QVector<portfolio::PortfolioAsset>& assets,
                                         const portfolio::Portfolio& portfolio) {
    if (portfolio.broker_account_id.isEmpty())
        return false; // unlinked portfolio — yfinance owns it.

    // Resolve broker + creds. Any of these missing → fall back silently.
    auto& acct_mgr = trading::AccountManager::instance();
    const auto account = acct_mgr.get_account(portfolio.broker_account_id);
    if (account.account_id.isEmpty() || account.broker_id.isEmpty()) {
        LOG_WARN("PortfolioSvc",
                 QString("broker_account_id %1 not found — falling back to yfinance for %2")
                     .arg(portfolio.broker_account_id, portfolio_id));
        return false;
    }

    auto* broker = trading::BrokerRegistry::instance().get(account.broker_id);
    if (!broker) {
        LOG_WARN("PortfolioSvc",
                 QString("broker '%1' not registered — falling back to yfinance for %2")
                     .arg(account.broker_id, portfolio_id));
        return false;
    }

    const auto creds = acct_mgr.load_credentials(portfolio.broker_account_id);
    if (creds.access_token.isEmpty()) {
        LOG_WARN("PortfolioSvc",
                 QString("broker account %1 has no access_token — falling back to yfinance for %2")
                     .arg(portfolio.broker_account_id, portfolio_id));
        return false;
    }

    // Build the broker-native key list. Each asset stores broker_symbol +
    // exchange (populated at import time). Skip assets without a broker
    // pair — those slipped in before the v022 link or are non-broker rows
    // mixed into a broker-linked portfolio. They get current_price = 0 from
    // the lookup and the existing fallback (avg_buy_price) kicks in.
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
        QMetaObject::invokeMethod(self, [self, portfolio_id, assets, portfolio, resp, key_to_yf]() {
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
                LOG_INFO("PortfolioSvc",
                         QString("Broker quotes: %1 of %2 for portfolio %3")
                             .arg(quote_map.size()).arg(key_to_yf.size()).arg(portfolio_id));
            } else {
                // Broker call failed — log and fall back to yfinance for
                // _this_ refresh. The portfolio stays linked; next refresh
                // will retry the broker.
                LOG_WARN("PortfolioSvc",
                         QString("broker get_quotes failed for %1: %2 — falling back to yfinance")
                             .arg(portfolio_id, resp.error.left(200)));
                QStringList symbols;
                symbols.reserve(assets.size());
                for (const auto& a : assets)
                    symbols.append(a.symbol);
                MarketDataService::instance().fetch_quotes(symbols, [self, portfolio_id, assets,
                                                                     portfolio](bool ok, QVector<QuoteData> quotes) {
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
        }, Qt::QueuedConnection);
    });

    return true; // broker path was taken
}

void PortfolioService::finalize_summary(const QString& portfolio_id,
                                        const QVector<portfolio::PortfolioAsset>& assets,
                                        const portfolio::Portfolio& portfolio,
                                        const QHash<QString, QuoteData>& quote_map) {
    portfolio::PortfolioSummary summary;
    summary.portfolio = portfolio;
    summary.holdings.reserve(assets.size());

    double total_mv = 0;
    double total_cost = 0;
    double total_day = 0;

    for (const auto& asset : assets) {
        portfolio::HoldingWithQuote h;
        h.symbol = asset.symbol;
        h.quantity = asset.quantity;
        h.avg_buy_price = asset.avg_buy_price;
        h.cost_basis = asset.quantity * asset.avg_buy_price;
        // Prefer stored sector; fall back to resolver cache (which may
        // populate async — see sector_resolved handler in constructor).
        h.sector = asset.sector.isEmpty()
                       ? SectorResolver::instance().sector_for(asset.symbol)
                       : asset.sector;

        auto it = quote_map.find(asset.symbol);
        if (it != quote_map.end()) {
            h.current_price = it->price;
            h.day_change = it->change;
            h.day_change_percent = it->change_pct;
        } else {
            // Fallback to avg buy price if no quote (broker missed the symbol,
            // or yfinance returned nothing).
            h.current_price = asset.avg_buy_price;
        }

        h.market_value = h.quantity * h.current_price;
        h.unrealized_pnl = h.market_value - h.cost_basis;
        h.unrealized_pnl_percent = (h.cost_basis > 0) ? (h.unrealized_pnl / h.cost_basis) * 100.0 : 0;

        total_mv += h.market_value;
        total_cost += h.cost_basis;
        total_day += h.day_change * h.quantity;

        if (h.unrealized_pnl >= 0)
            summary.gainers++;
        else
            summary.losers++;

        summary.holdings.append(h);
    }

    // Compute weights
    for (auto& h : summary.holdings) {
        h.weight = (total_mv > 0) ? (h.market_value / total_mv) * 100.0 : 0;
    }

    summary.total_market_value = total_mv;
    summary.total_cost_basis = total_cost;
    summary.total_unrealized_pnl = total_mv - total_cost;
    summary.total_unrealized_pnl_percent = (total_cost > 0) ? ((total_mv - total_cost) / total_cost) * 100.0 : 0;
    summary.total_day_change = total_day;
    summary.total_day_change_percent =
        (total_mv - total_day > 0) ? (total_day / (total_mv - total_day)) * 100.0 : 0;
    summary.total_positions = assets.size();
    summary.last_updated = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    // Cache the result (P11)
    {
        QMutexLocker lock(&cache_mutex_);
        summary_cache_[portfolio_id] = {summary, QDateTime::currentSecsSinceEpoch()};
    }

    // Save snapshot for performance history
    QString today = QDate::currentDate().toString(Qt::ISODate);
    PortfolioRepository::instance().save_snapshot(portfolio_id, summary.total_market_value,
                                                  summary.total_cost_basis, summary.total_unrealized_pnl,
                                                  summary.total_unrealized_pnl_percent, today);

    emit summary_loaded(summary);
}

// ── Asset operations ─────────────────────────────────────────────────────────

} // namespace fincept::services
