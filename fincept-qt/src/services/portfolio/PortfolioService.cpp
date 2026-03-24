// src/services/portfolio/PortfolioService.cpp
#include "services/portfolio/PortfolioService.h"

#include "core/logging/Logger.h"
#include "storage/repositories/PortfolioRepository.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutexLocker>
#include <QtConcurrent>

#include <algorithm>
#include <cmath>
#include <numeric>

namespace fincept::services {

PortfolioService& PortfolioService::instance() {
    static PortfolioService s;
    return s;
}

PortfolioService::PortfolioService() : QObject(nullptr) {}

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
                                        const QString& description) {
    auto r = PortfolioRepository::instance().create_portfolio(name, owner, currency, description);
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
    // Collect symbols for batch quote fetch
    QStringList symbols;
    symbols.reserve(assets.size());
    for (const auto& a : assets) {
        symbols.append(a.symbol);
    }

    // Use QPointer for safe async callback (P8)
    QPointer<PortfolioService> self = this;

    MarketDataService::instance().fetch_quotes(symbols, [self, portfolio_id, assets,
                                                         portfolio](bool ok, QVector<QuoteData> quotes) {
        if (!self)
            return;

        // Build quote lookup
        QHash<QString, QuoteData> quote_map;
        if (ok) {
            for (const auto& q : quotes)
                quote_map[q.symbol] = q;
        }

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

            auto it = quote_map.find(asset.symbol);
            if (it != quote_map.end()) {
                h.current_price = it->price;
                h.day_change = it->change;
                h.day_change_percent = it->change_pct;
            } else {
                // Fallback to avg buy price if no quote
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
            QMutexLocker lock(&self->cache_mutex_);
            self->summary_cache_[portfolio_id] = {summary, QDateTime::currentSecsSinceEpoch()};
        }

        // Save snapshot for performance history
        QString today = QDate::currentDate().toString(Qt::ISODate);
        PortfolioRepository::instance().save_snapshot(portfolio_id, summary.total_market_value,
                                                      summary.total_cost_basis, summary.total_unrealized_pnl,
                                                      summary.total_unrealized_pnl_percent, today);

        emit self->summary_loaded(summary);
    });
}

// ── Asset operations ─────────────────────────────────────────────────────────

void PortfolioService::add_asset(const QString& portfolio_id, const QString& symbol, double qty, double price,
                                 const QString& date) {
    auto& repo = PortfolioRepository::instance();

    auto r = repo.add_asset(portfolio_id, symbol, qty, price, date);
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

// ── Metrics computation (async, P8-safe) ─────────────────────────────────────

void PortfolioService::compute_metrics(const portfolio::PortfolioSummary& summary) {
    if (summary.holdings.isEmpty()) {
        emit metrics_computed({});
        return;
    }

    // Compute concentration immediately (no async needed)
    portfolio::ComputedMetrics metrics;

    // Concentration top 3
    QVector<double> weights;
    weights.reserve(summary.holdings.size());
    for (const auto& h : summary.holdings)
        weights.append(h.weight);
    std::sort(weights.begin(), weights.end(), std::greater<>());
    double conc = 0;
    for (qsizetype i = 0; i < std::min(qsizetype{3}, weights.size()); ++i)
        conc += weights[i];
    metrics.concentration_top3 = conc;

    // Risk score from concentration alone (vol-based portion needs historical data)
    metrics.risk_score = std::min(conc / 80.0, 1.0) * 50.0;

    // Volatility from day changes (proxy, not annualized from historical)
    if (summary.holdings.size() >= 2) {
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
            double mean = sum / n;
            double var = (sum_sq / n) - (mean * mean);
            double daily_vol = std::sqrt(std::max(var, 0.0));
            double annualized = daily_vol * std::sqrt(252.0);
            metrics.volatility = annualized;

            // Sharpe proxy (using day change mean)
            double risk_free_daily = 0.04 / 252.0; // 4% annual
            if (daily_vol > 0.0001) {
                double excess = mean - risk_free_daily;
                metrics.sharpe = (excess / daily_vol) * std::sqrt(252.0);
            }

            // VaR 95% (parametric)
            if (summary.total_market_value > 0) {
                metrics.var_95 = summary.total_market_value * std::abs(mean - 1.645 * daily_vol) / 100.0;
            }

            // Update risk score with volatility component
            double vol_score = std::min(annualized / 40.0, 1.0) * 50.0;
            double conc_score = std::min(conc / 80.0, 1.0) * 50.0;
            metrics.risk_score = vol_score + conc_score;
        }
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
    QString name = root["portfolio_name"].toString("Imported Portfolio");
    QString owner = root["owner"].toString("");
    QString currency = root["currency"].toString("USD");
    auto txn_arr = root["transactions"].toArray();

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
            auto r = repo.add_asset(target_id, sym, qty, price, date);
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

    invalidate_cache(target_id);
    load_portfolios();

    emit import_complete({target_id, name, replayed, errors});
    LOG_INFO("PortfolioSvc",
             QString("Imported %1 transactions into %2, %3 errors").arg(replayed).arg(target_id).arg(errors.size()));
}

// ── Cache control ────────────────────────────────────────────────────────────

void PortfolioService::invalidate_cache(const QString& portfolio_id) {
    QMutexLocker lock(&cache_mutex_);
    summary_cache_.remove(portfolio_id);
}

} // namespace fincept::services
