// src/services/portfolio/PortfolioService_ImportExport.cpp
//
// CSV / JSON export and JSON import (with the various ImportMode resolutions).
//
// Part of the partial-class split of PortfolioService.cpp.

#include "core/logging/Logger.h"
#include "python/PythonRunner.h"
#include "services/portfolio/PortfolioService.h"
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

void PortfolioService::export_csv(const QString& portfolio_id, const QString& file_path) {
    auto assets_r = PortfolioRepository::instance().get_assets(portfolio_id);
    auto portfolio_r = PortfolioRepository::instance().get_portfolio(portfolio_id);
    auto txns_r = PortfolioRepository::instance().get_transactions(portfolio_id, 10000);

    if (assets_r.is_err() || portfolio_r.is_err()) {
        LOG_ERROR("PortfolioSvc", "Export CSV failed: cannot load data");
        emit export_failed(file_path, tr("Could not read the portfolio from the database."));
        return;
    }

    QFile file(file_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        LOG_ERROR("PortfolioSvc", "Cannot open file for writing: " + file_path);
        emit export_failed(file_path, file.errorString());
        return;
    }

    QTextStream out(&file);
    // QTextStream defaults to 6 significant digits, which silently rounded
    // prices/quantities (1234.5678 -> 1234.57, 1234567 -> 1.23457e+06) in the
    // exported file. 12 keeps any realistic holding exact without printing
    // binary-float noise.
    out.setRealNumberPrecision(12);
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
        emit export_failed(file_path, tr("Could not read the portfolio from the database."));
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
        emit export_failed(file_path, file.errorString());
        return;
    }
    if (file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) < 0) {
        LOG_ERROR("PortfolioSvc", "Write failed for " + file_path + ": " + file.errorString());
        emit export_failed(file_path, file.errorString());
        return;
    }
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

    // Replay chronologically. export_json() writes transactions newest-first
    // (get_transactions orders by date DESC), so replaying in file order hit
    // every SELL before the BUY that funded it: the sell was rejected as
    // "asset not found" and the position came back overstated. Sort by day; on
    // a tie BUY goes first so a same-day round trip is not rejected as "qty >
    // held" (the file carries no intra-day ordering).
    QVector<QJsonObject> ordered;
    ordered.reserve(txn_arr.size());
    for (const auto& val : txn_arr)
        ordered.append(val.toObject());
    std::stable_sort(ordered.begin(), ordered.end(), [](const QJsonObject& a, const QJsonObject& b) {
        const QString da = a.value("date").toString().left(10);
        const QString db = b.value("date").toString().left(10);
        if (da != db)
            return da < db;
        const int ra = a.value("type").toString() == QLatin1String("BUY") ? 0 : 1;
        const int rb = b.value("type").toString() == QLatin1String("BUY") ? 0 : 1;
        return ra < rb;
    });

    for (const auto& obj : ordered) {
        QString type = obj["type"].toString();
        if (type == "DIVIDEND") {
            // A dividend never moves a position, so it can be recorded as-is.
            // export_json() writes them, so dropping them on import silently
            // lost the dividend history on every round trip.
            const QString dsym = obj["symbol"].toString().trimmed();
            if (dsym.isEmpty())
                continue;
            auto dr = repo.add_transaction(target_id, dsym, "DIVIDEND", obj["quantity"].toDouble(),
                                           obj["price"].toDouble(), obj["date"].toString(), obj["notes"].toString());
            if (dr.is_err())
                errors.append(QString("DIVIDEND %1: %2").arg(dsym, QString::fromStdString(dr.error())));
            else
                ++replayed;
            continue;
        }
        if (type != "BUY" && type != "SELL")
            continue; // SPLIT is not replayed yet

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
            bool found = false;
            if (assets.is_ok()) {
                for (const auto& a : assets.value()) {
                    if (a.symbol == sym.toUpper()) {
                        found = true;
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
                // Was gated on errors.isEmpty(), so any earlier error hid this one.
                if (!found)
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

} // namespace fincept::services
