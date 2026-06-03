// src/services/backtesting/BacktestBrokerData.cpp
#include "services/backtesting/BacktestBrokerData.h"

#include "core/logging/Logger.h"
#include "trading/AccountManager.h"
#include "trading/BrokerAccount.h"
#include "trading/BrokerInterface.h"
#include "trading/BrokerRegistry.h"
#include "trading/TradingTypes.h"
#include "trading/instruments/InstrumentService.h"

#include <QDateTime>
#include <QJsonArray>
#include <QPointer>
#include <QSet>
#include <QtConcurrent>

#include <optional>

namespace fincept::services::backtest {

namespace {

// NOTE: helpers are bt_-prefixed and TU-local. The unity build (batch 20) may
// place this file in the same blob as algo_engine/CandleDataFetcher.cpp, which
// has same-purpose anonymous-namespace helpers; distinct names prevent an ODR
// redefinition error.

const QSet<QString>& bt_indian_brokers() {
    static const QSet<QString> s = {
        QStringLiteral("fyers"),    QStringLiteral("zerodha"),  QStringLiteral("angelone"),
        QStringLiteral("upstox"),   QStringLiteral("dhan"),     QStringLiteral("kotak"),
        QStringLiteral("groww"),    QStringLiteral("aliceblue"),QStringLiteral("fivepaisa"),
        QStringLiteral("iifl"),     QStringLiteral("motilal"),  QStringLiteral("shoonya"),
        QStringLiteral("samco"),    QStringLiteral("flattrade"),QStringLiteral("paytm"),
        QStringLiteral("tradejini"),QStringLiteral("icicidirect"),
    };
    return s;
}

const QSet<QString>& bt_brokers_needing_token() {
    static const QSet<QString> s = {
        QStringLiteral("motilal"),   QStringLiteral("aliceblue"), QStringLiteral("fivepaisa"),
        QStringLiteral("iifl"),
    };
    return s;
}

// Mirror of CandleDataFetcher::timeframe_to_broker_resolution. Returns "" for
// intervals we deliberately don't source from brokers in v1 (weekly/monthly) so
// the caller skips broker fetch and Python uses yfinance.
QString bt_timeframe_to_resolution(const QString& tf) {
    if (tf == "1m")  return QStringLiteral("1");
    if (tf == "3m")  return QStringLiteral("3");
    if (tf == "5m")  return QStringLiteral("5");
    if (tf == "15m") return QStringLiteral("15");
    if (tf == "30m") return QStringLiteral("30");
    if (tf == "1h")  return QStringLiteral("60");
    if (tf == "4h")  return QStringLiteral("240");
    if (tf == "1d")  return QStringLiteral("D");
    return {}; // 1wk, 1mo, unknown → yfinance
}

// Worker-thread-safe instrument-master load (private named connection inside svc).
bool bt_ensure_instruments_loaded(const QString& broker_id) {
    auto& svc = fincept::trading::InstrumentService::instance();
    if (svc.is_loaded(broker_id))
        return true;
    svc.load_from_db_worker(broker_id);
    return svc.is_loaded(broker_id);
}

// Build the symbol string get_history expects. Empty = token broker we couldn't
// resolve (caller skips that symbol → yfinance).
QString bt_resolve_broker_symbol(const QString& broker_id, const QString& bare_symbol) {
    using fincept::trading::InstrumentService;
    if (bare_symbol.contains(':'))
        return bare_symbol;
    const QString sym = bare_symbol.trimmed().toUpper();
    const QString exch = QStringLiteral("NSE");

    if (bt_brokers_needing_token().contains(broker_id)) {
        if (!bt_ensure_instruments_loaded(broker_id))
            return {};
        auto tok = InstrumentService::instance().instrument_token(sym, exch, broker_id);
        if (!tok.has_value() || tok.value() <= 0)
            return {};
        return exch + ":" + sym + ":" + QString::number(tok.value());
    }
    static const QSet<QString> kInternalResolvers = {
        QStringLiteral("zerodha"), QStringLiteral("angelone"),
        QStringLiteral("dhan"),    QStringLiteral("upstox"),
    };
    if (kInternalResolvers.contains(broker_id))
        bt_ensure_instruments_loaded(broker_id);
    return sym;
}

// Canonical symbol key matching the Python guard's _canon_symbol.
QString bt_canon_symbol(const QString& sym) {
    QString s = sym.trimmed().toUpper();
    if (s.contains(':')) {
        const auto parts = s.split(':');
        s = (parts.size() > 1 && !parts[1].isEmpty()) ? parts[1] : parts[0];
    }
    for (const auto& suf : {QStringLiteral(".NS"), QStringLiteral(".BO")}) {
        if (s.endsWith(suf))
            s.chop(suf.size());
    }
    return s;
}

// First active account that belongs to an Indian broker, preferring Connected.
std::optional<fincept::trading::BrokerAccount> bt_first_active_indian_account() {
    using fincept::trading::ConnectionState;
    const auto accts = fincept::trading::AccountManager::instance().active_accounts();
    std::optional<fincept::trading::BrokerAccount> fallback;
    for (const auto& a : accts) {
        if (!bt_indian_brokers().contains(a.broker_id))
            continue;
        if (fincept::trading::AccountManager::instance().connection_state(a.account_id)
            == ConnectionState::Connected)
            return a;
        if (!fallback)
            fallback = a;
    }
    return fallback;
}

} // namespace

bool BacktestBrokerData::has_active_indian_broker() {
    return bt_first_active_indian_account().has_value();
}

void BacktestBrokerData::fetch(QObject* context, const QStringList& symbols,
                               const QString& start_date, const QString& end_date,
                               const QString& interval, Callback cb) {
    QPointer<QObject> ctx = context;
    (void)QtConcurrent::run([ctx, symbols, start_date, end_date, interval, cb]() {
        QJsonObject out;
        QString broker_id;

        const QString resolution = bt_timeframe_to_resolution(interval);
        auto acct = bt_first_active_indian_account();

        if (acct.has_value() && !resolution.isEmpty()) {
            broker_id = acct->broker_id;
            auto* broker = fincept::trading::BrokerRegistry::instance().get(broker_id);
            const auto creds =
                fincept::trading::AccountManager::instance().load_credentials(acct->account_id);

            auto has_data = [](const fincept::trading::ApiResponse<QVector<fincept::trading::BrokerCandle>>& r) {
                return r.success && r.data.value_or(QVector<fincept::trading::BrokerCandle>{}).size() > 0;
            };

            if (broker && !creds.api_key.isEmpty()) {
                for (const QString& symbol : symbols) {
                    const QString resolved = bt_resolve_broker_symbol(broker_id, symbol);
                    if (resolved.isEmpty())
                        continue; // unresolved token broker → yfinance for this symbol

                    auto r = broker->get_history(creds, resolved, resolution, start_date, end_date);
                    if (!has_data(r) && resolved != symbol)
                        r = broker->get_history(creds, symbol, resolution, start_date, end_date);
                    if (!has_data(r))
                        continue;

                    QJsonArray arr;
                    for (const auto& c : r.data.value()) {
                        QJsonObject o;
                        o["t"] = static_cast<double>(c.timestamp);
                        o["o"] = c.open;
                        o["h"] = c.high;
                        o["l"] = c.low;
                        o["c"] = c.close;
                        o["v"] = c.volume;
                        arr.append(o);
                    }
                    out[bt_canon_symbol(symbol)] = arr;
                }
            }
        }

        if (!ctx)
            return;
        QMetaObject::invokeMethod(ctx, [cb, out, broker_id]() { cb(out, broker_id); },
                                  Qt::QueuedConnection);
    });
}

} // namespace fincept::services::backtest
