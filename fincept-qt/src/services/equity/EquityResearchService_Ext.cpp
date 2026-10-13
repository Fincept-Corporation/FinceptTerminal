// src/services/equity/EquityResearchService_Ext.cpp
//
// One-shot, user-initiated EquityResearchService entry points (no caching, no
// broadcast). Kept out of EquityResearchService.cpp so the streaming/cached loaders
// there stay readable.
#include "services/equity/EquityResearchService.h"

#include "core/logging/Logger.h"
#include "python/PythonRunner.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>

namespace fincept::services::equity {

void EquityResearchService::fetch_price_history(const QString& symbol, const QString& period,
                                                const QString& interval, HistoryCallback done) {
    if (!done)
        return;
    const QString sym = symbol.trimmed();
    if (sym.isEmpty()) {
        done(false, {}, tr("No symbol to download."));
        return;
    }
    // Screens never call PythonRunner themselves (rule D1/P6): the CSV export dialog asks here.
    run_python("yfinance_data.py", {"historical_period", sym, period, interval},
               [this, sym, period, interval, done](bool ok, const QString& out) {
                   if (!ok) {
                       LOG_WARN("EquityResearch", QString("Price history export failed for %1 (%2/%3)")
                                                      .arg(sym, period, interval));
                       done(false, {}, out.trimmed().isEmpty() ? tr("The price history request failed.")
                                                               : out.trimmed().left(300));
                       return;
                   }
                   const QJsonDocument doc = QJsonDocument::fromJson(python::extract_json(out).toUtf8());
                   if (doc.isObject() && doc.object().contains(QStringLiteral("error"))) {
                       done(false, {}, doc.object().value(QStringLiteral("error")).toString());
                       return;
                   }
                   QVector<Candle> candles = parse_candles(doc.array());
                   std::sort(candles.begin(), candles.end(),
                             [](const Candle& a, const Candle& b) { return a.timestamp < b.timestamp; });
                   done(true, candles, {});
               });
}

} // namespace fincept::services::equity
