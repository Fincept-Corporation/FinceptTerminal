// src/services/portfolio/PortfolioService.cpp
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
                        // Refresh the active portfolio view so sectors appear
                        // without the user having to hit refresh manually.
                        load_summary(p.id);
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
    }
}

void PortfolioService::delete_portfolio(const QString& id) {
    invalidate_cache(id);
    auto r = PortfolioRepository::instance().delete_portfolio(id);
    if (r.is_ok()) {
        emit portfolio_deleted(id);
        load_portfolios();
    } else {
        LOG_ERROR("PortfolioSvc", "Failed to delete portfolio: " + QString::fromStdString(r.error()));
    }
}

// ── Summary ──────────────────────────────────────────────────────────────────

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

void PortfolioService::add_asset(const QString& portfolio_id, const QString& symbol, double qty, double price,
                                 const QString& date,
                                 const QString& broker_symbol, const QString& exchange) {
    auto& repo = PortfolioRepository::instance();

    // Sector left empty here — SectorResolver fills it asynchronously after
    // the asset lands in the DB. Broker fields pass through verbatim.
    auto r = repo.add_asset(portfolio_id, symbol, qty, price, date, /*sector=*/QString(),
                            broker_symbol, exchange);
    if (r.is_err()) {
        LOG_ERROR("PortfolioSvc", "Failed to add asset: " + QString::fromStdString(r.error()));
        return;
    }

    // Record transaction
    QString txn_date = date.isEmpty() ? QDateTime::currentDateTimeUtc().toString(Qt::ISODate) : date;
    repo.add_transaction(portfolio_id, symbol, "BUY", qty, price, txn_date);

    invalidate_cache(portfolio_id);
    emit asset_added(portfolio_id);
}

void PortfolioService::sell_asset(const QString& portfolio_id, const QString& symbol, double qty, double price,
                                  const QString& date) {
    auto& repo = PortfolioRepository::instance();

    // Get current holdings to update quantity
    auto assets_r = repo.get_assets(portfolio_id);
    if (assets_r.is_err()) {
        LOG_ERROR("PortfolioSvc", "Failed to get assets for sell: " + QString::fromStdString(assets_r.error()));
        return;
    }

    // Find the asset
    const portfolio::PortfolioAsset* found = nullptr;
    for (const auto& a : assets_r.value()) {
        if (a.symbol == symbol.toUpper()) {
            found = &a;
            break;
        }
    }

    if (!found) {
        LOG_ERROR("PortfolioSvc", QString("Asset %1 not found in portfolio %2").arg(symbol, portfolio_id));
        return;
    }

    double remaining = found->quantity - qty;
    if (remaining <= 0.0001) {
        // Full sell — remove asset
        repo.remove_asset(portfolio_id, symbol);
    } else {
        // Partial sell — keep same avg price
        repo.update_asset(portfolio_id, symbol, remaining, found->avg_buy_price);
    }

    // Record transaction
    QString txn_date = date.isEmpty() ? QDateTime::currentDateTimeUtc().toString(Qt::ISODate) : date;
    repo.add_transaction(portfolio_id, symbol, "SELL", qty, price, txn_date);

    invalidate_cache(portfolio_id);
    emit asset_sold(portfolio_id);
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

void PortfolioService::update_transaction(const QString& id, double qty, double price, const QString& date,
                                          const QString& notes) {
    PortfolioRepository::instance().update_transaction(id, qty, price, date, notes);
}

void PortfolioService::delete_transaction(const QString& id, const QString& portfolio_id) {
    PortfolioRepository::instance().delete_transaction(id);
    invalidate_cache(portfolio_id);
}

// ── Dividend ──────────────────────────────────────────────────────────────────

void PortfolioService::record_dividend(const QString& portfolio_id, const QString& symbol, double qty,
                                       double amount_per_share, double total, const QString& date,
                                       const QString& notes) {
    const QString txn_date = date.isEmpty() ? QDateTime::currentDateTimeUtc().toString(Qt::ISODate) : date;
    auto& repo = PortfolioRepository::instance();
    auto r = repo.add_transaction(portfolio_id, symbol, "DIVIDEND", qty, amount_per_share, txn_date, notes);
    if (r.is_err()) {
        LOG_ERROR("PortfolioSvc", "Failed to record dividend: " + QString::fromStdString(r.error()));
        return;
    }
    Q_UNUSED(total)
    invalidate_cache(portfolio_id);
    // Reload transactions so the txn panel updates
    load_transactions(portfolio_id, 50);
}

// ── Historical correlation ────────────────────────────────────────────────────

void PortfolioService::fetch_correlation(const QStringList& symbols) {
    if (symbols.size() < 2) {
        emit correlation_computed({});
        return;
    }

    // Build inline Python that embeds the symbol list, fetches 30-day closes,
    // and prints a JSON correlation matrix to stdout.
    QJsonArray sym_arr;
    for (const auto& s : symbols)
        sym_arr.append(s);
    const QString sym_json = QString::fromUtf8(QJsonDocument(sym_arr).toJson(QJsonDocument::Compact));

    const QString code = QString(R"python(
import json, sys
import yfinance as yf
import numpy as np

symbols = %1
data = {}
for sym in symbols:
    try:
        hist = yf.download(sym, period="30d", interval="1d", progress=False)
        if hist is not None and not hist.empty:
            closes = hist["Close"].dropna().tolist()
            if hasattr(closes[0], 'item'):
                closes = [v.item() for v in closes]
            data[sym] = closes
    except Exception:
        pass

# Compute daily returns
returns = {}
for sym, prices in data.items():
    if len(prices) >= 5:
        r = [(prices[i] - prices[i-1]) / prices[i-1] for i in range(1, len(prices))]
        returns[sym] = r

syms = list(returns.keys())
matrix = {}
for i in range(len(syms)):
    for j in range(len(syms)):
        a = returns[syms[i]]
        b = returns[syms[j]]
        n = min(len(a), len(b))
        if n < 2:
            val = 1.0 if i == j else 0.0
        else:
            a, b = a[-n:], b[-n:]
            ma, mb = sum(a)/n, sum(b)/n
            num = sum((a[k]-ma)*(b[k]-mb) for k in range(n))
            da  = sum((a[k]-ma)**2 for k in range(n))
            db  = sum((b[k]-mb)**2 for k in range(n))
            denom = (da*db)**0.5
            val = num/denom if denom > 1e-10 else (1.0 if i==j else 0.0)
            val = max(-1.0, min(1.0, val))
        matrix[syms[i] + "|" + syms[j]] = round(val, 4)

print(json.dumps(matrix))
)python")
                             .arg(sym_json);

    QPointer<PortfolioService> self = this;
    python::PythonRunner::instance().run_code(code, [self](python::PythonResult result) {
        if (!self)
            return;
        if (!result.success || result.output.trimmed().isEmpty()) {
            LOG_WARN("PortfolioSvc", "Correlation fetch failed: " + result.error.left(200));
            emit self->correlation_computed({});
            return;
        }
        QJsonParseError err;
        const auto doc = QJsonDocument::fromJson(result.output.trimmed().toUtf8(), &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject()) {
            emit self->correlation_computed({});
            return;
        }
        QHash<QString, double> matrix;
        const auto obj = doc.object();
        for (auto it = obj.begin(); it != obj.end(); ++it)
            matrix[it.key()] = it.value().toDouble();
        emit self->correlation_computed(matrix);
    });
}

// ── SPY benchmark data ────────────────────────────────────────────────────────

QString PortfolioService::default_benchmark_for_currency(const QString& currency) {
    const QString c = currency.trimmed().toUpper();
    if (c == "CAD") return QStringLiteral("^GSPTSE");
    if (c == "GBP") return QStringLiteral("^FTSE");
    if (c == "EUR") return QStringLiteral("^STOXX50E");
    if (c == "AUD") return QStringLiteral("^AXJO");
    if (c == "INR") return QStringLiteral("^NSEI");
    if (c == "JPY") return QStringLiteral("^N225");
    if (c == "HKD") return QStringLiteral("^HSI");
    return QStringLiteral("SPY"); // USD and unknown
}

void PortfolioService::fetch_spy_history(const QString& period) {
    fetch_benchmark_history(QStringLiteral("SPY"), period);
}

void PortfolioService::fetch_benchmark_history(const QString& symbol, const QString& period) {
    // Allow callers to omit the symbol → defaults to SPY (legacy behaviour).
    const QString sym = symbol.isEmpty() ? QStringLiteral("SPY") : symbol;

    const QString code = QString(R"python(
import json, sys
import yfinance as yf

symbol = "%1"
period = "%2"
try:
    hist = yf.download(symbol, period=period, interval="1d", progress=False, auto_adjust=True)
    dates  = []
    closes = []
    if hist is not None and not hist.empty:
        for dt, row in hist.iterrows():
            v = row["Close"]
            if hasattr(v, "item"): v = v.item()
            dates.append(dt.strftime("%Y-%m-%d"))
            closes.append(float(v))
    print(json.dumps({"symbol": symbol, "dates": dates, "closes": closes}))
except Exception as e:
    print(json.dumps({"symbol": symbol, "dates": [], "closes": [], "error": str(e)}))
)python")
                             .arg(sym, period);

    QPointer<PortfolioService> self = this;
    python::PythonRunner::instance().run_code(code, [self, sym](python::PythonResult result) {
        if (!self)
            return;
        QStringList dates;
        QVector<double> closes;
        if (!result.success || result.output.trimmed().isEmpty()) {
            LOG_WARN("PortfolioSvc",
                     QString("Benchmark %1 fetch failed: %2").arg(sym, result.error.left(200)));
        } else {
            QJsonParseError err;
            const auto doc = QJsonDocument::fromJson(result.output.trimmed().toUtf8(), &err);
            if (err.error == QJsonParseError::NoError && doc.isObject()) {
                const auto obj = doc.object();
                const auto dates_arr = obj["dates"].toArray();
                const auto closes_arr = obj["closes"].toArray();
                dates.reserve(dates_arr.size());
                closes.reserve(closes_arr.size());
                for (const auto& v : dates_arr)
                    dates.append(v.toString());
                for (const auto& v : closes_arr)
                    closes.append(v.toDouble());
            }
        }

        // Beta computation in compute_metrics() always regresses against SPY,
        // so only update that cache when SPY is what the caller asked for —
        // otherwise we would corrupt Beta with e.g. TSX returns.
        if (sym == QStringLiteral("SPY")) {
            self->spy_dates_cache_ = dates;
            self->spy_closes_cache_ = closes;
            emit self->spy_history_loaded(dates, closes);
        }
        emit self->benchmark_history_loaded(sym, dates, closes);
    });
}

// ── Risk-free rate (FRED DGS10) ───────────────────────────────────────────────

void PortfolioService::fetch_risk_free_rate() {
    // Check 24h cache in SettingsRepository
    auto& settings = SettingsRepository::instance();
    const qint64 now_secs = QDateTime::currentSecsSinceEpoch();

    auto ts_r = settings.get("portfolio.rf_rate_timestamp");
    auto val_r = settings.get("portfolio.rf_rate_value");
    if (ts_r.is_ok() && val_r.is_ok()) {
        bool ts_ok = false, val_ok = false;
        const qint64 cached_ts = ts_r.value().toLongLong(&ts_ok);
        const double cached_val = val_r.value().toDouble(&val_ok);
        if (ts_ok && val_ok && (now_secs - cached_ts) < 86400) {
            // Cache still valid — use stored value
            rf_rate_ = cached_val;
            emit risk_free_rate_loaded(rf_rate_);
            return;
        }
    }

    // Fetch from FRED via inline Python (requests library)
    const QString code = QString(R"python(
import json, urllib.request
api_key = "9da0d86e0cf58f4a4023e5e686226d69"
url = (
    "https://api.stlouisfed.org/fred/series/observations"
    "?series_id=DGS10&api_key=" + api_key +
    "&file_type=json&sort_order=desc&limit=5"
)
try:
    with urllib.request.urlopen(url, timeout=10) as r:
        data = json.loads(r.read().decode())
    rate = None
    for obs in data.get("observations", []):
        v = obs.get("value", ".")
        if v != ".":
            rate = float(v) / 100.0  # convert percent to decimal
            break
    print(json.dumps({"rate": rate if rate is not None else 0.04}))
except Exception as e:
    print(json.dumps({"rate": 0.04, "error": str(e)}))
)python");

    QPointer<PortfolioService> self = this;
    python::PythonRunner::instance().run_code(code, [self, now_secs](python::PythonResult result) {
        if (!self)
            return;
        double rate = 0.04; // fallback
        if (result.success && !result.output.trimmed().isEmpty()) {
            QJsonParseError err;
            const auto doc = QJsonDocument::fromJson(result.output.trimmed().toUtf8(), &err);
            if (err.error == QJsonParseError::NoError && doc.object().contains("rate"))
                rate = doc.object()["rate"].toDouble(0.04);
        }
        // Persist to 24h cache
        auto& settings = SettingsRepository::instance();
        settings.set("portfolio.rf_rate_timestamp", QString::number(now_secs));
        settings.set("portfolio.rf_rate_value", QString::number(rate, 'f', 6));
        self->rf_rate_ = rate;
        emit self->risk_free_rate_loaded(rate);
    });
}

// ── Metrics computation (async, P8-safe) ─────────────────────────────────────

void PortfolioService::compute_metrics(const portfolio::PortfolioSummary& summary) {
    if (summary.holdings.isEmpty()) {
        emit metrics_computed({});
        return;
    }

    portfolio::ComputedMetrics metrics;

    // ── Concentration top-3 ───────────────────────────────────────────────────
    QVector<double> weights;
    weights.reserve(summary.holdings.size());
    for (const auto& h : summary.holdings)
        weights.append(h.weight);
    std::sort(weights.begin(), weights.end(), std::greater<>());
    double conc = 0;
    for (qsizetype i = 0; i < std::min(qsizetype{3}, weights.size()); ++i)
        conc += weights[i];
    metrics.concentration_top3 = conc;
    metrics.risk_score = std::min(conc / 80.0, 1.0) * 50.0; // concentration-only baseline

    // ── Load snapshots synchronously for time-series metrics ─────────────────
    // (this runs on the calling thread — compute_metrics is always called from
    //  the UI thread after summary_loaded, so we keep computation fast by
    //  loading snapshots from SQLite which is sub-millisecond for <365 rows)
    auto snap_r = PortfolioRepository::instance().get_snapshots(summary.portfolio.id, 365);
    if (snap_r.is_err() || snap_r.value().size() < 3) {
        // Trigger an async backfill so the next compute_metrics call has data.
        // This is one-shot per process to avoid hammering yfinance — once we've
        // attempted, the user can manually re-trigger via re-import.
        if (!backfill_attempted_.contains(summary.portfolio.id)) {
            backfill_attempted_.insert(summary.portfolio.id);
            QPointer<PortfolioService> self = this;
            const QString pid = summary.portfolio.id;
            QMetaObject::invokeMethod(this, [self, pid]() {
                if (self) self->backfill_history(pid, "1y");
            }, Qt::QueuedConnection);
        }
        // Fallback: derive volatility from cross-sectional day changes only
        double sum = 0, sum_sq = 0;
        int n = 0;
        for (const auto& h : summary.holdings) {
            if (std::abs(h.day_change_percent) > 0.0001) {
                sum += h.day_change_percent;
                sum_sq += h.day_change_percent * h.day_change_percent;
                ++n;
            }
        }
        if (n >= 2) {
            const double mean = sum / n;
            const double var = (sum_sq / n) - (mean * mean);
            const double daily_vol = std::sqrt(std::max(var, 0.0));
            const double ann_vol = daily_vol * std::sqrt(252.0);
            metrics.volatility = ann_vol * 100.0; // store as %
            const double rf_daily = rf_rate_ / 252.0;
            if (daily_vol > 1e-6)
                metrics.sharpe = ((mean / 100.0 - rf_daily) / (daily_vol / 100.0)) * std::sqrt(252.0);
            if (summary.total_market_value > 0)
                metrics.var_95 = summary.total_market_value * std::abs(mean / 100.0 - 1.645 * daily_vol / 100.0);
            const double vol_score = std::min(ann_vol / 40.0, 1.0) * 50.0;
            const double conc_score = std::min(conc / 80.0, 1.0) * 50.0;
            metrics.risk_score = vol_score + conc_score;
        }
        emit metrics_computed(metrics);
        return;
    }

    // ── Build daily return series from snapshots ──────────────────────────────
    auto snaps = snap_r.value();
    // Sort ascending by date
    std::sort(snaps.begin(), snaps.end(),
              [](const auto& a, const auto& b) { return a.snapshot_date < b.snapshot_date; });

    QVector<double> port_returns; // daily log returns (%)
    port_returns.reserve(snaps.size() - 1);
    for (int i = 1; i < snaps.size(); ++i) {
        const double prev = snaps[i - 1].total_value;
        const double curr = snaps[i].total_value;
        if (prev > 1e-6)
            port_returns.append((curr - prev) / prev * 100.0);
    }

    if (port_returns.size() < 2) {
        emit metrics_computed(metrics);
        return;
    }

    const int n = port_returns.size();

    // ── Mean and std-dev ──────────────────────────────────────────────────────
    const double mean = std::accumulate(port_returns.begin(), port_returns.end(), 0.0) / n;
    double sum_sq = 0;
    for (const double r : port_returns)
        sum_sq += (r - mean) * (r - mean);
    const double daily_vol = std::sqrt(sum_sq / (n - 1)); // sample std-dev
    const double ann_vol = daily_vol * std::sqrt(252.0);
    metrics.volatility = ann_vol; // already in %

    // ── Sharpe ratio (annualised) ─────────────────────────────────────────────
    // rf_rate_ = live DGS10 annual decimal (e.g. 0.043); convert to daily %
    const double rf_daily = rf_rate_ / 252.0 * 100.0;
    if (daily_vol > 1e-6)
        metrics.sharpe = ((mean - rf_daily) / daily_vol) * std::sqrt(252.0);

    // ── Max drawdown ──────────────────────────────────────────────────────────
    double peak = snaps.first().total_value;
    double max_dd = 0.0;
    for (const auto& s : snaps) {
        if (s.total_value > peak)
            peak = s.total_value;
        if (peak > 1e-6) {
            const double dd = (s.total_value - peak) / peak * 100.0;
            if (dd < max_dd)
                max_dd = dd;
        }
    }
    metrics.max_drawdown = max_dd; // negative %

    // ── Beta vs SPY (OLS regression on aligned date windows) ─────────────────
    // Build SPY daily return series from cached closes, aligned to snapshot dates.
    if (spy_closes_cache_.size() >= 2 && spy_dates_cache_.size() == spy_closes_cache_.size()) {
        // Build a date→close map for O(1) lookup
        QHash<QString, double> spy_map;
        spy_map.reserve(spy_dates_cache_.size());
        for (int i = 0; i < spy_dates_cache_.size(); ++i)
            spy_map[spy_dates_cache_[i]] = spy_closes_cache_[i];

        // For each consecutive snapshot pair, find SPY return for the same day
        QVector<double> spy_aligned;
        QVector<double> port_aligned;
        spy_aligned.reserve(snaps.size() - 1);
        port_aligned.reserve(snaps.size() - 1);

        for (int i = 1; i < snaps.size(); ++i) {
            const QString date = snaps[i].snapshot_date;
            if (!spy_map.contains(date))
                continue;
            // Find previous available SPY close
            const QString prev_date = snaps[i - 1].snapshot_date;
            if (!spy_map.contains(prev_date))
                continue;

            const double spy_prev = spy_map[prev_date];
            const double spy_curr = spy_map[date];
            if (spy_prev < 1e-6)
                continue;

            const double spy_ret = (spy_curr - spy_prev) / spy_prev * 100.0;
            const double pv = snaps[i - 1].total_value;
            const double cv = snaps[i].total_value;
            if (pv < 1e-6)
                continue;
            const double port_ret = (cv - pv) / pv * 100.0;
            spy_aligned.append(spy_ret);
            port_aligned.append(port_ret);
        }

        const int m = spy_aligned.size();
        if (m >= 5) {
            // OLS: beta = cov(port, spy) / var(spy)
            const double spy_mean = std::accumulate(spy_aligned.begin(), spy_aligned.end(), 0.0) / m;
            const double port_mean = std::accumulate(port_aligned.begin(), port_aligned.end(), 0.0) / m;
            double cov = 0.0, var_spy = 0.0;
            for (int i = 0; i < m; ++i) {
                cov += (port_aligned[i] - port_mean) * (spy_aligned[i] - spy_mean);
                var_spy += (spy_aligned[i] - spy_mean) * (spy_aligned[i] - spy_mean);
            }
            if (var_spy > 1e-10)
                metrics.beta = cov / var_spy;
        }
    }

    // ── VaR 95% and CVaR 95% (historical simulation) ─────────────────────────
    // Sort returns ascending; VaR = worst 5th percentile; CVaR = mean of tail.
    if (summary.total_market_value > 0 && !port_returns.isEmpty()) {
        QVector<double> sorted_rets = port_returns;
        std::sort(sorted_rets.begin(), sorted_rets.end());
        const int tail_count = std::max(1, static_cast<int>(std::floor(sorted_rets.size() * 0.05)));
        // VaR: loss at 95th percentile (positive value = amount at risk)
        const double var_pct = -sorted_rets[tail_count - 1]; // worst 5th pct return (%)
        metrics.var_95 = summary.total_market_value * std::max(var_pct, 0.0) / 100.0;
        // CVaR: expected loss beyond VaR (average of worst tail_count returns)
        double tail_sum = 0.0;
        for (int i = 0; i < tail_count; ++i)
            tail_sum += sorted_rets[i];
        const double cvar_pct = -(tail_sum / tail_count);
        metrics.cvar_95 = summary.total_market_value * std::max(cvar_pct, 0.0) / 100.0;
    }

    // ── Composite risk score (0-100) ─────────────────────────────────────────
    {
        const double vol_score = std::min(ann_vol / 40.0, 1.0) * 30.0;
        const double conc_score = std::min(conc / 80.0, 1.0) * 25.0;
        const double dd_score = std::min(std::abs(max_dd) / 50.0, 1.0) * 25.0;
        const double beta_val = metrics.beta.value_or(1.0);
        const double beta_score = std::min(std::abs(beta_val) / 2.0, 1.0) * 20.0;
        metrics.risk_score = vol_score + conc_score + dd_score + beta_score;
    }

    emit metrics_computed(metrics);
}

// ── Import / Export ──────────────────────────────────────────────────────────

void PortfolioService::export_csv(const QString& portfolio_id, const QString& file_path) {
    auto assets_r = PortfolioRepository::instance().get_assets(portfolio_id);
    auto portfolio_r = PortfolioRepository::instance().get_portfolio(portfolio_id);
    auto txns_r = PortfolioRepository::instance().get_transactions(portfolio_id, 10000);

    if (assets_r.is_err() || portfolio_r.is_err()) {
        LOG_ERROR("PortfolioSvc", "Export CSV failed: cannot load data");
        return;
    }

    QFile file(file_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        LOG_ERROR("PortfolioSvc", "Cannot open file for writing: " + file_path);
        return;
    }

    QTextStream out(&file);
    auto& p = portfolio_r.value();
    out << "# Portfolio: " << p.name << "\n";
    out << "# Owner: " << p.owner << "\n";
    out << "# Currency: " << p.currency << "\n";
    out << "# Exported: " << QDateTime::currentDateTimeUtc().toString(Qt::ISODate) << "\n\n";

    // Holdings section
    out << "## HOLDINGS\n";
    out << "Symbol,Quantity,AvgBuyPrice,CostBasis\n";
    for (const auto& a : assets_r.value()) {
        out << a.symbol << "," << a.quantity << "," << a.avg_buy_price << "," << (a.quantity * a.avg_buy_price) << "\n";
    }

    // Transactions section
    if (txns_r.is_ok() && !txns_r.value().isEmpty()) {
        out << "\n## TRANSACTIONS\n";
        out << "Date,Symbol,Type,Quantity,Price,TotalValue,Notes\n";
        for (const auto& t : txns_r.value()) {
            out << t.transaction_date << "," << t.symbol << "," << t.transaction_type << "," << t.quantity << ","
                << t.price << "," << t.total_value << ",\""
                << QString(t.notes).replace(QLatin1Char('"'), QStringLiteral("\"\"")) << "\"\n";
        }
    }

    file.close();
    LOG_INFO("PortfolioSvc", "Exported CSV to " + file_path);
    emit export_complete(file_path);
}

void PortfolioService::export_json(const QString& portfolio_id, const QString& file_path) {
    auto portfolio_r = PortfolioRepository::instance().get_portfolio(portfolio_id);
    auto txns_r = PortfolioRepository::instance().get_transactions(portfolio_id, 10000);

    if (portfolio_r.is_err()) {
        LOG_ERROR("PortfolioSvc", "Export JSON failed: cannot load portfolio");
        return;
    }

    auto& p = portfolio_r.value();
    QJsonObject root;
    root["format_version"] = "1.0";
    root["portfolio_name"] = p.name;
    root["owner"] = p.owner;
    root["currency"] = p.currency;
    root["export_date"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    QJsonArray txn_arr;
    if (txns_r.is_ok()) {
        for (const auto& t : txns_r.value()) {
            QJsonObject obj;
            obj["date"] = t.transaction_date;
            obj["symbol"] = t.symbol;
            obj["type"] = t.transaction_type;
            obj["quantity"] = t.quantity;
            obj["price"] = t.price;
            obj["total_value"] = t.total_value;
            obj["notes"] = t.notes;
            txn_arr.append(obj);
        }
    }
    root["transactions"] = txn_arr;

    QFile file(file_path);
    if (!file.open(QIODevice::WriteOnly)) {
        LOG_ERROR("PortfolioSvc", "Cannot open file for writing: " + file_path);
        return;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.close();

    LOG_INFO("PortfolioSvc", "Exported JSON to " + file_path);
    emit export_complete(file_path);
}

void PortfolioService::import_json(const QString& file_path, portfolio::ImportMode mode,
                                   const QString& merge_target_id) {
    QFile file(file_path);
    if (!file.open(QIODevice::ReadOnly)) {
        emit import_complete({"", "", 0, {"Cannot open file: " + file_path}});
        return;
    }

    QJsonParseError parse_err;
    auto doc = QJsonDocument::fromJson(file.readAll(), &parse_err);
    file.close();

    if (doc.isNull()) {
        emit import_complete({"", "", 0, {"Invalid JSON: " + parse_err.errorString()}});
        return;
    }

    auto root = doc.object();

    // Schema validation — the importer only accepts the terminal's own export format:
    //   { "portfolio_name": "...", "currency": "...", "transactions": [ {date,symbol,type,quantity,price}, ... ] }
    // Reject anything else up front so we don't create empty/mis-named portfolios.
    const QString schema_msg =
        "Unsupported JSON format. Expected the terminal's export format with fields "
        "'portfolio_name' (string) and 'transactions' (array of {date, symbol, type, quantity, price}). "
        "Holdings-only snapshots are not supported — convert each holding to a BUY transaction first.";

    if (!root.contains("portfolio_name") || !root.value("portfolio_name").isString() ||
        root.value("portfolio_name").toString().trimmed().isEmpty()) {
        emit import_complete({"", "", 0, {schema_msg}});
        LOG_ERROR("PortfolioSvc", "Import rejected: missing/invalid 'portfolio_name'");
        return;
    }
    if (!root.contains("transactions") || !root.value("transactions").isArray()) {
        emit import_complete({"", "", 0, {schema_msg}});
        LOG_ERROR("PortfolioSvc", "Import rejected: missing/invalid 'transactions' array");
        return;
    }

    QString name = root["portfolio_name"].toString();
    QString owner = root["owner"].toString("");
    QString currency = root["currency"].toString("USD");
    auto txn_arr = root["transactions"].toArray();

    if (txn_arr.isEmpty()) {
        emit import_complete({"", name, 0, {"No transactions found in file. " + schema_msg}});
        LOG_ERROR("PortfolioSvc", "Import rejected: 'transactions' array is empty");
        return;
    }

    // Collect symbol → sector mapping from any hints the file provides:
    //   1. top-level "holdings[]" (legacy broker-export format) — symbol + sector
    //   2. per-transaction "sector" field
    // Either/both populate an authoritative override we hand to SectorResolver
    // so the Sectors tab is correct without waiting on a yfinance round-trip.
    QHash<QString, QString> sector_hints;
    if (root.contains("holdings") && root.value("holdings").isArray()) {
        for (const auto& v : root.value("holdings").toArray()) {
            auto obj = v.toObject();
            QString sym = obj.value("symbol").toString().trimmed().toUpper();
            QString sec = obj.value("sector").toString().trimmed();
            if (!sym.isEmpty() && !sec.isEmpty())
                sector_hints.insert(sym, sec);
        }
    }
    for (const auto& v : txn_arr) {
        auto obj = v.toObject();
        QString sym = obj.value("symbol").toString().trimmed().toUpper();
        QString sec = obj.value("sector").toString().trimmed();
        if (!sym.isEmpty() && !sec.isEmpty() && !sector_hints.contains(sym))
            sector_hints.insert(sym, sec);
    }

    auto& repo = PortfolioRepository::instance();
    QString target_id;

    if (mode == portfolio::ImportMode::New) {
        auto r = repo.create_portfolio(name, owner, currency);
        if (r.is_err()) {
            emit import_complete({"", name, 0, {"Failed to create portfolio: " + QString::fromStdString(r.error())}});
            return;
        }
        target_id = r.value();
    } else {
        target_id = merge_target_id;
        if (target_id.isEmpty()) {
            emit import_complete({"", "", 0, {"No merge target specified"}});
            return;
        }
    }

    int replayed = 0;
    QStringList errors;

    // Replay transactions chronologically
    for (const auto& val : txn_arr) {
        auto obj = val.toObject();
        QString type = obj["type"].toString();
        if (type != "BUY" && type != "SELL")
            continue; // Skip DIVIDEND/SPLIT for now

        QString sym = obj["symbol"].toString();
        double qty = obj["quantity"].toDouble();
        double price = obj["price"].toDouble();
        QString date = obj["date"].toString();

        if (type == "BUY") {
            QString hint_sector = sector_hints.value(sym.toUpper());
            auto r = repo.add_asset(target_id, sym, qty, price, date, hint_sector);
            if (r.is_err()) {
                errors.append(QString("BUY %1: %2").arg(sym, QString::fromStdString(r.error())));
                continue;
            }
        } else { // SELL
            auto assets = repo.get_assets(target_id);
            bool sell_ok = false;
            if (assets.is_ok()) {
                for (const auto& a : assets.value()) {
                    if (a.symbol == sym.toUpper()) {
                        if (qty > a.quantity + 0.0001) {
                            errors.append(QString("SELL %1: qty %2 > held %3").arg(sym).arg(qty).arg(a.quantity));
                        } else {
                            double remaining = a.quantity - qty;
                            if (remaining <= 0.0001)
                                repo.remove_asset(target_id, sym);
                            else
                                repo.update_asset(target_id, sym, remaining, a.avg_buy_price);
                            sell_ok = true;
                        }
                        break;
                    }
                }
                if (!sell_ok && errors.isEmpty())
                    errors.append(QString("SELL %1: asset not found").arg(sym));
            }
            if (!sell_ok)
                continue; // Don't record failed sell as transaction
        }

        // Record the transaction only on success
        repo.add_transaction(target_id, sym, type, qty, price, date, obj["notes"].toString());
        ++replayed;
    }

    // Seed SectorResolver with authoritative mapping from the import file,
    // and kick off async resolution for any symbols that had no hint.
    for (auto it = sector_hints.constBegin(); it != sector_hints.constEnd(); ++it)
        SectorResolver::instance().remember(it.key(), it.value());

    QStringList unresolved;
    if (auto assets = repo.get_assets(target_id); assets.is_ok()) {
        for (const auto& a : assets.value())
            if (a.sector.isEmpty())
                unresolved << a.symbol;
    }
    if (!unresolved.isEmpty())
        SectorResolver::instance().prefetch(unresolved);

    invalidate_cache(target_id);
    load_portfolios();

    emit import_complete({target_id, name, replayed, errors});
    LOG_INFO("PortfolioSvc",
             QString("Imported %1 transactions into %2, %3 errors").arg(replayed).arg(target_id).arg(errors.size()));

    // Backfill 1y of historical NAV from yfinance so Beta and MDD have data
    // immediately. Async — fires history_backfilled when done; the screen's
    // metrics_computed handler will already have reloaded snapshots once the
    // user refreshes (or on next compute_metrics call).
    backfill_history(target_id, "1y");
}

// ── Snapshots ────────────────────────────────────────────────────────────────

void PortfolioService::load_snapshots(const QString& portfolio_id, int days) {
    auto r = PortfolioRepository::instance().get_snapshots(portfolio_id, days);
    if (r.is_ok()) {
        emit snapshots_loaded(portfolio_id, r.value());
    } else {
        LOG_WARN("PortfolioSvc", "Failed to load snapshots: " + QString::fromStdString(r.error()));
    }
}

void PortfolioService::backfill_history(const QString& portfolio_id, const QString& /*period*/) {
    if (portfolio_id.isEmpty())
        return;

    auto& repo = PortfolioRepository::instance();

    // Pull the full transaction history (not just the latest 50 — we need
    // every BUY/SELL since the portfolio was created so the replay walks the
    // whole timeline).
    auto txns_r = repo.get_transactions(portfolio_id, /*limit=*/100000);
    if (txns_r.is_err() || txns_r.value().isEmpty()) {
        // No transactions → nothing to replay. Emit zero so callers don't wait.
        emit history_backfilled(portfolio_id, 0);
        return;
    }
    const auto txns = txns_r.value();

    // Serialize transactions to a temp JSON file. The Python side (action
    // `portfolio_nav_history_replay`) reads it and walks the transactions in
    // order to reconstruct daily position vectors and per-day cost basis.
    // A file is used rather than CLI args because a portfolio with 500
    // transactions × ~40 chars each would exceed Windows' 8191-char arg limit.
    QJsonArray tx_arr;
    for (const auto& t : txns) {
        QJsonObject obj;
        obj["date"] = t.transaction_date.left(10); // YYYY-MM-DD
        obj["symbol"] = t.symbol;
        obj["type"] = t.transaction_type;
        obj["quantity"] = t.quantity;
        obj["price"] = t.price;
        tx_arr.append(obj);
    }

    const QString tmp_dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QDir().mkpath(tmp_dir);
    const QString tmp_path = QDir(tmp_dir).filePath(
        QString("fincept_replay_%1_%2.json")
            .arg(portfolio_id.left(16))
            .arg(QDateTime::currentMSecsSinceEpoch()));

    QFile f(tmp_path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        LOG_WARN("PortfolioSvc", "backfill_history: cannot open temp file " + tmp_path);
        emit history_backfilled(portfolio_id, 0);
        return;
    }
    f.write(QJsonDocument(tx_arr).toJson(QJsonDocument::Compact));
    f.close();

    QStringList args;
    args << "portfolio_nav_history_replay" << tmp_path;

    QPointer<PortfolioService> self = this;
    python::PythonRunner::instance().run("yfinance_data.py", args,
                                         [self, portfolio_id, tmp_path](python::PythonResult result) {
        // Always remove the temp file when we're done with it, regardless of
        // success — leaks add up if a user re-imports often.
        QFile::remove(tmp_path);

        if (!self)
            return;
        if (!result.success || result.output.trimmed().isEmpty()) {
            LOG_WARN("PortfolioSvc",
                     QString("backfill_history failed for %1: %2").arg(portfolio_id, result.error.left(200)));
            emit self->history_backfilled(portfolio_id, 0);
            return;
        }
        QJsonParseError err;
        const auto doc = QJsonDocument::fromJson(result.output.trimmed().toUtf8(), &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject()) {
            LOG_WARN("PortfolioSvc", "backfill_history: bad JSON: " + err.errorString());
            emit self->history_backfilled(portfolio_id, 0);
            return;
        }
        const auto obj = doc.object();
        if (obj.contains("error")) {
            LOG_WARN("PortfolioSvc", "backfill_history: " + obj["error"].toString());
            emit self->history_backfilled(portfolio_id, 0);
            return;
        }
        const auto dates = obj["dates"].toArray();
        const auto navs = obj["navs"].toArray();
        const auto costs = obj["costs"].toArray();
        if (dates.isEmpty() || dates.size() != navs.size()) {
            emit self->history_backfilled(portfolio_id, 0);
            return;
        }
        const bool have_costs = (costs.size() == dates.size());

        // Upsert each row. INSERT OR REPLACE keyed by (portfolio_id, snapshot_date)
        // so re-running backfill corrects existing rows in place. Per-day cost
        // basis comes from the replay (BUY adds, SELL reduces by WAC); falls
        // back to NAV if the Python side didn't return a costs array.
        auto& repo = PortfolioRepository::instance();
        int written = 0;
        for (int i = 0; i < dates.size(); ++i) {
            const QString d = dates[i].toString();
            const double nav = navs[i].toDouble();
            const double cost_basis = have_costs ? costs[i].toDouble() : 0.0;
            const double pnl = nav - cost_basis;
            const double pnl_pct = cost_basis > 0 ? (pnl / cost_basis) * 100.0 : 0.0;
            auto wr = repo.save_snapshot(portfolio_id, nav, cost_basis, pnl, pnl_pct, d);
            if (wr.is_ok())
                ++written;
        }

        LOG_INFO("PortfolioSvc",
                 QString("Replayed %1 historical snapshots for %2").arg(written).arg(portfolio_id));
        self->invalidate_cache(portfolio_id);
        emit self->history_backfilled(portfolio_id, written);
    });
}

// ── Cache control ────────────────────────────────────────────────────────────

void PortfolioService::invalidate_cache(const QString& portfolio_id) {
    QMutexLocker lock(&cache_mutex_);
    summary_cache_.remove(portfolio_id);
}

} // namespace fincept::services
