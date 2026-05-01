#include "trading/exchanges/kraken/KrakenSymbolMapper.h"

namespace fincept::trading::kraken {

QString to_kraken(const QString& fincept_symbol) {
    return fincept_symbol.trimmed();
}

QString from_kraken(const QString& kraken_symbol, const QString& original_request) {
    return original_request.isEmpty() ? kraken_symbol : original_request;
}

QString usd_fallback(const QString& fincept_symbol) {
    QString s = fincept_symbol.trimmed();
    if (s.isEmpty())
        return {};

    const int colon = s.indexOf(QLatin1Char(':'));
    if (colon >= 0)
        s.truncate(colon);

    const int slash = s.indexOf(QLatin1Char('/'));
    if (slash < 0)
        return {};

    const QString base = s.left(slash);
    const QString quote = s.mid(slash + 1).toUpper();

    if (quote == QLatin1String("USDT"))
        return base + QLatin1String("/USD");
    return {};
}

int interval_minutes(const QString& timeframe) {
    const QString tf = timeframe.trimmed().toLower();
    if (tf == QLatin1String("1m") || tf == QLatin1String("1min")) return 1;
    if (tf == QLatin1String("5m"))  return 5;
    if (tf == QLatin1String("15m")) return 15;
    if (tf == QLatin1String("30m")) return 30;
    if (tf == QLatin1String("1h") || tf == QLatin1String("60m")) return 60;
    if (tf == QLatin1String("4h") || tf == QLatin1String("240m")) return 240;
    if (tf == QLatin1String("1d") || tf == QLatin1String("1440m")) return 1440;
    if (tf == QLatin1String("1w") || tf == QLatin1String("10080m")) return 10080;
    if (tf == QLatin1String("15d") || tf == QLatin1String("21600m")) return 21600;
    return 0;
}

QString timeframe_string(int kraken_interval) {
    switch (kraken_interval) {
        case 1:     return QStringLiteral("1m");
        case 5:     return QStringLiteral("5m");
        case 15:    return QStringLiteral("15m");
        case 30:    return QStringLiteral("30m");
        case 60:    return QStringLiteral("1h");
        case 240:   return QStringLiteral("4h");
        case 1440:  return QStringLiteral("1d");
        case 10080: return QStringLiteral("1w");
        case 21600: return QStringLiteral("15d");
        default:    return QStringLiteral("1m");
    }
}

} // namespace fincept::trading::kraken
