// src/services/algo_trading/AlgoUniverses.cpp
#include "services/algo_trading/AlgoUniverses.h"

#include "core/config/AppPaths.h"
#include "core/logging/Logger.h"
#include "core/market/MarketProfile.h"
#include "services/algo_trading/AlgoTradingTypes.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace fincept::services::algo {

namespace {

QString au_tr(const char* text) {
    return QCoreApplication::translate("AlgoUniverses", text);
}

QJsonArray au_read_user_array() {
    QFile f(algo_universes_path());
    if (!f.open(QIODevice::ReadOnly)) {
        if (f.exists()) // no file = no user lists yet
            LOG_WARN("AlgoUniverses", logf("User universes file could not be opened",
                                           {{"path", log_path(f.fileName())}, {"error", f.errorString()}}));
        return {};
    }
    return QJsonDocument::fromJson(f.readAll()).object().value(QStringLiteral("universes")).toArray();
}

bool au_write_user_array(const QJsonArray& arr, QString* error) {
    QDir().mkpath(QFileInfo(algo_universes_path()).absolutePath());
    QSaveFile f(algo_universes_path());
    if (!f.open(QIODevice::WriteOnly)) {
        LOG_ERROR("AlgoUniverses", logf("User universes file could not be written",
                                        {{"path", log_path(f.fileName())}, {"error", f.errorString()}}));
        if (error)
            *error = f.errorString();
        return false;
    }
    QJsonObject root;
    root[QStringLiteral("universes")] = arr;
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!f.commit()) {
        LOG_ERROR("AlgoUniverses", logf("User universes file could not be written",
                                        {{"path", log_path(f.fileName())}, {"error", f.errorString()}}));
        if (error)
            *error = f.errorString();
        return false;
    }
    return true;
}

} // namespace

QString algo_universes_path() {
    return fincept::AppPaths::data() + QStringLiteral("/algo_universes.json");
}

QVector<AlgoUniverse> algo_universes() {
    QVector<AlgoUniverse> out;

    // 1. The user's own lists.
    for (const auto& v : au_read_user_array()) {
        const QJsonObject o = v.toObject();
        AlgoUniverse u;
        u.name = o.value(QStringLiteral("name")).toString().trimmed();
        if (u.name.isEmpty())
            continue;
        u.id = QStringLiteral("user:") + u.name;
        u.region = o.value(QStringLiteral("region")).toString();
        u.exchange_hint = o.value(QStringLiteral("exchange")).toString();
        for (const auto& s : o.value(QStringLiteral("symbols")).toArray()) {
            const QString sym = s.toString().trimmed().toUpper();
            if (!sym.isEmpty() && !u.symbols.contains(sym))
                u.symbols << sym;
        }
        u.source = au_tr("Your list (%1)").arg(algo_universes_path());
        u.user = true;
        if (!u.symbols.isEmpty())
            out.append(u);
    }

    // 2. Every market region's large caps, the user's region first.
    const QString current = fincept::core::market::MarketProfileStore::instance().current_id();
    QVector<AlgoUniverse> regions;
    for (const auto& p : fincept::core::market::all_market_profiles()) {
        if (p.movers.isEmpty() || p.id == QLatin1String("GLOBAL"))
            continue;
        AlgoUniverse u;
        u.id = QStringLiteral("region:") + p.id;
        u.name = au_tr("%1 — large caps (%2)").arg(fincept::core::market::market_profile_name(p)).arg(p.movers.size());
        u.region = p.id;
        u.symbols = p.movers; // Yahoo tickers with suffixes
        u.source = au_tr("Region large-cap list from the market profile (a short list, not an index).");
        if (p.id == current)
            regions.prepend(u);
        else
            regions.append(u);
    }
    out += regions;

    // 3. Legacy NSE lists.
    {
        AlgoUniverse u;
        u.id = QStringLiteral("nse:nifty_top20");
        u.name = au_tr("NSE — largest NIFTY 50 names (20, partial)");
        u.region = QStringLiteral("IN");
        u.exchange_hint = QStringLiteral("NSE");
        u.symbols = nifty50_symbols();
        u.source = au_tr("Built-in partial list: 20 of the 50 NIFTY constituents. Import the full index through %1.")
                       .arg(algo_universes_path());
        out.append(u);
    }
    {
        AlgoUniverse u;
        u.id = QStringLiteral("nse:banknifty");
        u.name = au_tr("NSE — Bank NIFTY (12)");
        u.region = QStringLiteral("IN");
        u.exchange_hint = QStringLiteral("NSE");
        u.symbols = bank_nifty_symbols();
        u.source = au_tr("Built-in list; index constituents change — check against the exchange's current list.");
        out.append(u);
    }
    return out;
}

bool save_user_universe(const AlgoUniverse& universe, QString* error) {
    const QString name = universe.name.trimmed();
    if (name.isEmpty() || universe.symbols.isEmpty()) {
        if (error)
            *error = au_tr("A list needs a name and at least one symbol.");
        return false;
    }
    QJsonArray arr = au_read_user_array();
    QJsonObject o;
    o[QStringLiteral("name")] = name;
    o[QStringLiteral("region")] = universe.region;
    o[QStringLiteral("exchange")] = universe.exchange_hint;
    o[QStringLiteral("symbols")] = QJsonArray::fromStringList(universe.symbols);
    bool replaced = false;
    for (int i = 0; i < arr.size(); ++i) {
        if (arr[i].toObject().value(QStringLiteral("name")).toString().trimmed() == name) {
            arr[i] = o;
            replaced = true;
            break;
        }
    }
    if (!replaced)
        arr.append(o);
    return au_write_user_array(arr, error);
}

bool remove_user_universe(const QString& name, QString* error) {
    QJsonArray arr = au_read_user_array();
    for (int i = arr.size() - 1; i >= 0; --i)
        if (arr[i].toObject().value(QStringLiteral("name")).toString().trimmed() == name.trimmed())
            arr.removeAt(i);
    return au_write_user_array(arr, error);
}

} // namespace fincept::services::algo
