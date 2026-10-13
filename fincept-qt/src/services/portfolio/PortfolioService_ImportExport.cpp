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

#include <QDate>
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
#include <optional>

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

namespace portfolio_import_detail {

struct ImportRow {
    QString date; // YYYY-MM-DD
    QString symbol;
    QString type; // BUY / SELL / DIVIDEND
    double quantity = 0;
    double price = 0;
    QString notes;
    QString sector;
};

/// First present key among @p keys, as a JSON value.
QJsonValue first_of(const QJsonObject& o, std::initializer_list<const char*> keys) {
    for (const char* k : keys) {
        const QString key = QString::fromLatin1(k);
        if (o.contains(key) && !o.value(key).isNull())
            return o.value(key);
    }
    return QJsonValue(QJsonValue::Undefined);
}

/// Numbers may arrive as JSON numbers or as strings ("1,234.50").
std::optional<double> to_number(const QJsonValue& v) {
    if (v.isDouble())
        return std::isfinite(v.toDouble()) ? std::optional<double>(v.toDouble()) : std::nullopt;
    if (v.isString()) {
        QString s = v.toString().trimmed();
        s.remove(QLatin1Char(','));
        bool ok = false;
        const double d = s.toDouble(&ok);
        if (ok && std::isfinite(d))
            return d;
    }
    return std::nullopt;
}

/// Short rendering of an offending value for an error message.
QString describe(const QJsonValue& v) {
    if (v.isUndefined())
        return QStringLiteral("missing");
    if (v.isString())
        return QStringLiteral("\"%1\"").arg(v.toString().left(30));
    if (v.isDouble())
        return QString::number(v.toDouble(), 'g', 12);
    if (v.isBool())
        return v.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    if (v.isArray())
        return QStringLiteral("an array");
    if (v.isObject())
        return QStringLiteral("an object");
    return QStringLiteral("null");
}

} // namespace portfolio_import_detail

void PortfolioService::import_json(const QString& file_path, portfolio::ImportMode mode,
                                   const QString& merge_target_id) {
    namespace pimp = portfolio_import_detail;
    QFile file(file_path);
    if (!file.open(QIODevice::ReadOnly)) {
        emit import_complete({"", "", 0, {tr("Cannot open file: %1").arg(file_path)}});
        return;
    }
    QJsonParseError parse_err;
    const auto doc = QJsonDocument::fromJson(file.readAll(), &parse_err);
    file.close();
    if (doc.isNull()) {
        emit import_complete(
            {"", "", 0, {tr("Not valid JSON: %1 (at byte %2)").arg(parse_err.errorString()).arg(parse_err.offset)}});
        return;
    }
    if (!doc.isObject()) {
        emit import_complete({"", "", 0, {tr("The file must contain a JSON object at the top level, not an array.")}});
        return;
    }
    const auto root = doc.object();

    // ── Shape detection ─────────────────────────────────────────────────────
    // Two shapes are accepted:
    //   export   : {"portfolio_name", "currency", "transactions": [{date,symbol,type,quantity,price,notes?}]}
    //   holdings : {"name", "currency", "holdings": [{symbol, quantity, avg_cost, date?, sector?}]}
    // Keys are interchangeable (portfolio_name|name; quantity|qty|shares;
    // price|avg_cost|avg_buy_price|cost). Each holding becomes a BUY.
    QStringList problems;
    const QString name = pimp::first_of(root, {"portfolio_name", "name"}).toString().trimmed();
    if (mode == portfolio::ImportMode::New && name.isEmpty())
        problems << tr("'portfolio_name' (or 'name') is missing or empty.");
    const QString owner = root.value("owner").toString();
    const QString currency = root.value("currency").toString("USD").trimmed().toUpper();
    if (currency.size() != 3 || !std::all_of(currency.begin(), currency.end(), [](QChar c) { return c.isLetter(); }))
        problems << tr("'currency' must be a 3-letter ISO code (got %1).").arg(pimp::describe(root.value("currency")));

    const bool has_txns = root.value("transactions").isArray();
    const bool has_holdings = root.value("holdings").isArray();
    if (!has_txns && !has_holdings)
        problems << tr("No 'transactions' array and no 'holdings' array found.");
    const QJsonArray rows_json = has_txns ? root.value("transactions").toArray() : root.value("holdings").toArray();
    const QString arr_name = has_txns ? QStringLiteral("transactions") : QStringLiteral("holdings");
    if ((has_txns || has_holdings) && rows_json.isEmpty())
        problems << tr("'%1' is empty — nothing to import.").arg(arr_name);

    // Sector hints: holdings[] (also when transactions[] carries the trades) and per-row "sector".
    QHash<QString, QString> sector_hints;
    if (has_holdings) {
        for (const auto& v : root.value("holdings").toArray()) {
            const auto obj = v.toObject();
            const QString sym = obj.value("symbol").toString().trimmed().toUpper();
            const QString sec = obj.value("sector").toString().trimmed();
            if (!sym.isEmpty() && !sec.isEmpty())
                sector_hints.insert(sym, sec);
        }
    }

    // ── Row validation (nothing is written until the whole file passes) ─────
    const QDate today = QDate::currentDate();
    QVector<pimp::ImportRow> rows;
    QStringList skipped;
    rows.reserve(rows_json.size());
    for (int i = 0; i < rows_json.size(); ++i) {
        const QJsonValue rv = rows_json.at(i);
        if (!rv.isObject()) {
            problems << tr("%1[%2]: not an object.").arg(arr_name).arg(i);
            continue;
        }
        const auto o = rv.toObject();
        pimp::ImportRow row;
        row.symbol = o.value("symbol").toString().trimmed().toUpper();
        const QString where = QStringLiteral("%1[%2]%3")
                                  .arg(arr_name)
                                  .arg(i)
                                  .arg(row.symbol.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(row.symbol));
        if (row.symbol.isEmpty())
            problems << tr("%1: 'symbol' is missing.").arg(where);

        row.type = has_txns ? o.value("type").toString().trimmed().toUpper() : QStringLiteral("BUY");
        if (row.type == QLatin1String("SPLIT")) {
            skipped << tr("%1: SPLIT rows are not supported and were skipped — record the post-split position "
                          "as BUY/SELL rows.")
                           .arg(where);
            continue;
        }
        if (row.type != QLatin1String("BUY") && row.type != QLatin1String("SELL") &&
            row.type != QLatin1String("DIVIDEND")) {
            problems << tr("%1: 'type' must be BUY, SELL or DIVIDEND (got %2).")
                            .arg(where, pimp::describe(o.value("type")));
            continue;
        }

        const QJsonValue qv = pimp::first_of(o, {"quantity", "qty", "shares", "units"});
        const auto qty = pimp::to_number(qv);
        if (!qty || *qty <= 0)
            problems << tr("%1: quantity must be a number > 0 (got %2).").arg(where, pimp::describe(qv));
        else
            row.quantity = *qty;

        const QJsonValue pv = has_txns ? pimp::first_of(o, {"price", "amount_per_share"})
                                       : pimp::first_of(o, {"avg_cost", "avg_buy_price", "average_cost", "cost",
                                                            "price"});
        const auto price = pimp::to_number(pv);
        if (!price || *price <= 0)
            problems << tr("%1: %2 must be a number > 0 (got %3).")
                            .arg(where, has_txns ? QStringLiteral("price") : QStringLiteral("avg_cost"),
                                 pimp::describe(pv));
        else
            row.price = *price;

        const QJsonValue dv = pimp::first_of(o, {"date", "purchase_date", "first_purchase_date", "trade_date"});
        if (dv.isUndefined() || dv.toString().trimmed().isEmpty()) {
            if (has_txns)
                problems << tr("%1: 'date' (YYYY-MM-DD) is missing.").arg(where);
            else
                row.date = today.toString(Qt::ISODate); // undated holding: treated as bought today
        } else {
            const QDate d = QDate::fromString(dv.toString().trimmed().left(10), Qt::ISODate);
            if (!d.isValid())
                problems << tr("%1: date %2 is not YYYY-MM-DD.").arg(where, pimp::describe(dv));
            else if (d > today)
                problems << tr("%1: date %2 is in the future.").arg(where, d.toString(Qt::ISODate));
            else
                row.date = d.toString(Qt::ISODate);
        }
        row.notes = o.value("notes").toString();
        row.sector = o.value("sector").toString().trimmed();
        if (!row.symbol.isEmpty() && !row.sector.isEmpty() && !sector_hints.contains(row.symbol))
            sector_hints.insert(row.symbol, row.sector);
        rows.append(row);
    }

    auto& repo = PortfolioRepository::instance();
    QString target_id;
    if (mode == portfolio::ImportMode::Merge) {
        target_id = merge_target_id;
        if (target_id.isEmpty())
            problems << tr("No portfolio selected to merge into.");
    }

    // Chronological replay: exports are newest-first, and replaying a SELL
    // before the BUY that funded it would reject it. Ties: BUY first.
    std::stable_sort(rows.begin(), rows.end(), [](const pimp::ImportRow& a, const pimp::ImportRow& b) {
        if (a.date != b.date)
            return a.date < b.date;
        return (a.type == QLatin1String("BUY") ? 0 : 1) < (b.type == QLatin1String("BUY") ? 0 : 1);
    });

    // Simulate the positions so an over-sell is caught before anything is written.
    if (problems.isEmpty()) {
        QHash<QString, double> held;
        if (!target_id.isEmpty()) {
            if (auto assets = repo.get_assets(target_id); assets.is_ok())
                for (const auto& a : assets.value())
                    held[a.symbol] = a.quantity;
        }
        for (const auto& r : rows) {
            if (r.type == QLatin1String("BUY")) {
                held[r.symbol] += r.quantity;
            } else if (r.type == QLatin1String("SELL")) {
                const double h = held.value(r.symbol, 0.0);
                if (r.quantity > h + std::max(1e-9, h * 1e-9))
                    problems << tr("SELL %1 %2 on %3 exceeds the %4 held at that date.")
                                    .arg(QString::number(r.quantity, 'g', 12), r.symbol, r.date,
                                         QString::number(h, 'g', 12));
                else
                    held[r.symbol] = std::max(0.0, h - r.quantity);
            }
        }
    }

    if (!problems.isEmpty()) {
        constexpr int kMaxShown = 15;
        QStringList shown = problems.mid(0, kMaxShown);
        if (problems.size() > kMaxShown)
            shown << tr("… and %1 more problem(s).").arg(problems.size() - kMaxShown);
        shown.prepend(tr("Nothing was imported — %n problem(s) found:", "", static_cast<int>(problems.size())));
        LOG_ERROR("PortfolioSvc",
                  QString("Import rejected: %1 problem(s), first: %2").arg(problems.size()).arg(problems.first()));
        emit import_complete({"", name, 0, shown});
        return;
    }

    if (mode == portfolio::ImportMode::New) {
        auto r = repo.create_portfolio(name, owner, currency);
        if (r.is_err()) {
            emit import_complete(
                {"", name, 0, {tr("Failed to create portfolio: %1").arg(QString::fromStdString(r.error()))}});
            return;
        }
        target_id = r.value();
    }

    int replayed = 0;
    QStringList errors = skipped;
    for (const auto& r : rows) {
        if (r.type == QLatin1String("BUY")) {
            auto ar = repo.add_asset(target_id, r.symbol, r.quantity, r.price, r.date, sector_hints.value(r.symbol));
            if (ar.is_err()) {
                errors << QStringLiteral("BUY %1: %2").arg(r.symbol, QString::fromStdString(ar.error()));
                continue;
            }
        } else if (r.type == QLatin1String("SELL")) {
            auto assets = repo.get_assets(target_id);
            bool sold = false;
            if (assets.is_ok()) {
                for (const auto& a : assets.value()) {
                    if (a.symbol != r.symbol)
                        continue;
                    const double remaining = a.quantity - r.quantity;
                    if (remaining <= std::max(1e-9, a.quantity * 1e-9))
                        repo.remove_asset(target_id, r.symbol);
                    else
                        repo.update_asset(target_id, r.symbol, remaining, a.avg_buy_price);
                    sold = true;
                    break;
                }
            }
            if (!sold) {
                errors << QStringLiteral("SELL %1: position not found").arg(r.symbol);
                continue;
            }
        }
        auto tr_r = repo.add_transaction(target_id, r.symbol, r.type, r.quantity, r.price, r.date, r.notes);
        if (tr_r.is_err())
            errors << QStringLiteral("%1 %2: %3").arg(r.type, r.symbol, QString::fromStdString(tr_r.error()));
        else
            ++replayed;
    }

    // Seed SectorResolver with the file's authoritative sectors and resolve the rest.
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
    analytics_cache_.remove(target_id);
    load_portfolios();

    emit import_complete({target_id, name, replayed, errors});
    LOG_INFO("PortfolioSvc", QString("Imported %1 rows (%2) into %3, %4 note(s)")
                                 .arg(replayed)
                                 .arg(has_txns ? "transactions" : "holdings")
                                 .arg(target_id)
                                 .arg(errors.size()));

    // Reconstruct NAV history from the imported ledger so the realised
    // figures have data immediately.
    backfill_history(target_id);
}

} // namespace fincept::services
