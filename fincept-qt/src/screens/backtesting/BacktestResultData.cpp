// src/screens/backtesting/BacktestResultData.cpp
#include "screens/backtesting/BacktestResultData.h"

#include "core/currency/Currency.h"
#include "core/market/InstrumentCurrency.h"
#include "core/market/MarketSessions.h"
#include "ui/formatting/FormatSettings.h"
#include "ui/formatting/NumberFormat.h"

#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QLocale>
#include <QRegularExpression>
#include <QTime>
#include <QTimeZone>

#include <cmath>

namespace fincept::screens::btview {

namespace fmt = fincept::ui::formatting;

QJsonValue value_at(const QJsonObject& root, const QString& dotted_path) {
    const QStringList parts = dotted_path.split(QLatin1Char('.'));
    QJsonValue cur = root;
    for (const QString& p : parts) {
        if (!cur.isObject())
            return QJsonValue(QJsonValue::Undefined);
        cur = cur.toObject().value(p);
    }
    return cur;
}

double number_at(const QJsonObject& root, const QString& dotted_path) {
    const QJsonValue v = value_at(root, dotted_path);
    if (!v.isDouble())
        return kNaN;
    const double d = v.toDouble();
    return std::isfinite(d) ? d : kNaN;
}

QString format_value(double v, Unit unit, const FmtCtx& ctx) {
    if (unit == Unit::Text)
        return QString();
    if (!std::isfinite(v))
        return fmt::placeholder();
    switch (unit) {
        case Unit::Fraction:
            return fmt::fraction_pct(v, 2, /*signed_=*/false);
        case Unit::SignedFraction:
            return fmt::fraction_pct(v, 2, /*signed_=*/true);
        case Unit::PctPoints:
            return fmt::pp(v * 100.0, 2, /*signed_=*/true);
        case Unit::Ratio:
            return fmt::number(v, std::abs(v) >= 100.0 ? 1 : 2);
        case Unit::Money:
        case Unit::SignedMoney: {
            const bool big = std::abs(v) >= 1e7;
            const auto scheme = big ? fmt::Compact::FromSettings : fmt::Compact::None;
            // No run currency: the display currency (as cur::money did), sign placed by the formatter.
            const QString ccy = ctx.ccy.isEmpty() ? fmt::display_currency() : ctx.ccy;
            return unit == Unit::SignedMoney ? fmt::signed_money(v, ccy, scheme) : fmt::money(v, ccy, scheme);
        }
        case Unit::Count:
            return fmt::number(std::round(v), 0);
        case Unit::Bars:
            return QCoreApplication::translate("BacktestTearsheet", "%1 bars").arg(fmt::number(std::round(v), 0));
        case Unit::Plain:
            return fmt::number(v, std::abs(v) >= 1000 ? 0 : 2);
        case Unit::Text:
            break;
    }
    return fmt::placeholder();
}

QString format_json(const QJsonValue& v, Unit unit, const FmtCtx& ctx) {
    if (v.isString())
        return v.toString();
    if (v.isBool())
        return v.toBool() ? QCoreApplication::translate("BacktestTearsheet", "Yes")
                          : QCoreApplication::translate("BacktestTearsheet", "No");
    if (!v.isDouble())
        return fmt::placeholder();
    return format_value(v.toDouble(), unit, ctx);
}

ui::StatTile::Tone tone_for(double v, ToneRule rule) {
    using Tone = ui::StatTile::Tone;
    if (!std::isfinite(v))
        return Tone::Neutral;
    switch (rule) {
        case ToneRule::None:
            return Tone::Neutral;
        case ToneRule::Sign:
            return v > 0 ? Tone::Positive : v < 0 ? Tone::Negative : Tone::Neutral;
        case ToneRule::Drawdown: {
            const double m = std::abs(v);
            return m >= 0.25 ? Tone::Danger : m >= 0.10 ? Tone::Warning : Tone::Neutral; // a breached limit = state
        }
        case ToneRule::BelowOne:
            return v < 1.0 ? Tone::Warning : Tone::Neutral;
        case ToneRule::Negative:
            return v < 0.0 ? Tone::Warning : Tone::Neutral;
    }
    return Tone::Neutral;
}

QString run_currency(const QStringList& symbols) {
    for (const QString& s : symbols) {
        const QString sym = s.trimmed();
        if (sym.isEmpty())
            continue;
        return core::market::currency_for_symbol(sym);
    }
    return {};
}

QStringList run_currencies(const QStringList& symbols) {
    QStringList out;
    for (const QString& s : symbols) {
        const QString sym = s.trimmed();
        if (sym.isEmpty())
            continue;
        const QString c = core::market::currency_for_symbol(sym);
        if (!c.isEmpty() && !out.contains(c))
            out << c;
    }
    return out;
}

namespace {
QString bt_index_for_venue(const core::market::Venue* v);
} // namespace

QString default_benchmark_for(const QString& symbol) {
    return bt_index_for_venue(core::market::venue_for_symbol(symbol.trimmed()));
}

QString default_symbol_for_locale() {
    const QString cc = QLocale::territoryToCode(fmt::FormatSettings::instance().locale().territory());
    const QString idx = cc.isEmpty() ? QString() : bt_index_for_venue(core::market::venue_by_code(cc));
    return idx.isEmpty() ? QStringLiteral("SPY") : idx;
}

namespace {
QString bt_index_for_venue(const core::market::Venue* v) {
    if (!v)
        return {};
    struct Row {
        const char* venue;
        const char* index;
    };
    // Broad, liquid Yahoo index tickers per venue id (MarketSessions.cpp). Venues
    // without a dependable Yahoo index are left out on purpose.
    static constexpr Row kRows[] = {
        {"NYSE", "SPY"},          {"NASDAQ", "SPY"},       {"CME", "SPY"},          {"TSX", "^GSPTSE"},
        {"TSXV", "^GSPTSE"},      {"CSE", "^GSPTSE"},      {"CBOE_CA", "^GSPTSE"},  {"B3", "^BVSP"},
        {"BMV", "^MXX"},          {"LSE", "^FTSE"},        {"XETRA", "^GDAXI"},     {"FRA", "^GDAXI"},
        {"BER", "^GDAXI"},        {"DUS", "^GDAXI"},       {"HAM", "^GDAXI"},       {"HAN", "^GDAXI"},
        {"MUN", "^GDAXI"},        {"STU", "^GDAXI"},       {"VIE", "^ATX"},         {"PARIS", "^FCHI"},
        {"AMSTERDAM", "^AEX"},    {"BRUSSELS", "^BFX"},    {"MILAN", "FTSEMIB.MI"}, {"SIX", "^SSMI"},
        {"BME", "^IBEX"},         {"STOCKHOLM", "^OMX"},   {"ISTANBUL", "XU100.IS"}, {"NSE", "^NSEI"},
        {"BSE", "^BSESN"},        {"SGX", "^STI"},         {"HKEX", "^HSI"},        {"SSE", "000001.SS"},
        {"SZSE", "399001.SZ"},    {"TSE", "^N225"},        {"KRX", "^KS11"},        {"TWSE", "^TWII"},
        {"TPEX", "^TWII"},        {"ASX", "^AXJO"},        {"NZX", "^NZ50"},        {"IDX", "^JKSE"},
        {"MYX", "^KLSE"},         {"CRYPTO", "BTC-USD"},
    };
    for (const Row& r : kRows)
        if (v->id == QLatin1String(r.venue))
            return QString::fromLatin1(r.index);
    return {};
}
} // namespace

double indicative_risk_free_pct(const QString& ccy) {
    const QString c = core::market::major_currency(ccy);
    struct Row {
        const char* ccy;
        double pct;
    };
    // Rounded policy-rate levels; indicative only (shown as such in the UI).
    static constexpr Row kRows[] = {
        {"USD", 3.75}, {"EUR", 2.0},  {"GBP", 3.75}, {"JPY", 0.5},  {"CHF", 0.0},  {"CAD", 2.25}, {"AUD", 3.6},
        {"NZD", 2.5},  {"INR", 5.5},  {"CNY", 1.4},  {"HKD", 3.5},  {"SGD", 1.8},  {"KRW", 2.5},  {"TWD", 1.9},
        {"BRL", 14.0}, {"MXN", 7.5},  {"ZAR", 7.0},  {"SEK", 1.75}, {"NOK", 4.0},  {"DKK", 1.75}, {"PLN", 4.5},
        {"IDR", 5.0},  {"THB", 1.5},  {"MYR", 2.75}, {"SAR", 4.5},  {"AED", 3.9},  {"ILS", 4.25},
    };
    for (const Row& r : kRows)
        if (c == QLatin1String(r.ccy))
            return r.pct;
    return kNaN;
}

bool is_intraday_stamp(const QString& s) {
    if (s.size() <= 10)
        return false;
    const QString time = s.mid(11, 8);
    return !time.isEmpty() && time != QLatin1String("00:00:00") && time != QLatin1String("00:00");
}

double parse_time_ms(const QString& raw) {
    const QString s = raw.trimmed();
    if (s.size() < 10)
        return kNaN;
    const QDate d = QDate::fromString(s.left(10), QStringLiteral("yyyy-MM-dd"));
    if (!d.isValid())
        return kNaN;
    QTime t(0, 0);
    if (s.size() >= 16) {
        // "yyyy-MM-dd HH:mm[:ss]" or ISO "yyyy-MM-ddTHH:mm:ss[.zzz][Z|±hh:mm]" - wall time kept.
        const QString tp = s.mid(11, 8);
        t = QTime::fromString(tp, QStringLiteral("HH:mm:ss"));
        if (!t.isValid())
            t = QTime::fromString(tp.left(5), QStringLiteral("HH:mm"));
        if (!t.isValid())
            t = QTime(0, 0);
    }
    return static_cast<double>(QDateTime(d, t, QTimeZone::UTC).toMSecsSinceEpoch());
}

EquityData parse_equity(const QJsonArray& equity) {
    EquityData out;
    out.t.reserve(equity.size());
    out.equity.reserve(equity.size());
    out.drawdown.reserve(equity.size());
    out.returns.reserve(equity.size());
    bool any_bench = false;
    QVector<double> bench;
    bench.reserve(equity.size());
    for (const QJsonValue& v : equity) {
        const QJsonObject o = v.toObject();
        const QString date = o.value(QStringLiteral("date")).toString();
        const double t = parse_time_ms(date);
        const double e = o.value(QStringLiteral("equity")).toDouble(kNaN);
        if (!std::isfinite(t) || !std::isfinite(e))
            continue;
        if (!out.intraday && is_intraday_stamp(date))
            out.intraday = true;
        out.t.append(t);
        out.equity.append(e);
        out.drawdown.append(o.value(QStringLiteral("drawdown")).toDouble(0.0));
        out.returns.append(o.value(QStringLiteral("returns")).toDouble(kNaN));
        const QJsonValue b = o.value(QStringLiteral("benchmark"));
        if (b.isDouble()) {
            any_bench = true;
            bench.append(b.toDouble());
        } else {
            bench.append(kNaN);
        }
    }
    if (any_bench)
        out.benchmark = bench;
    // Per-bar returns from the curve when the provider sent something else
    // (pre-normalisation payloads carried the cumulative return here).
    bool returns_ok = !out.returns.isEmpty();
    for (double r : out.returns)
        if (!std::isfinite(r)) {
            returns_ok = false;
            break;
        }
    if (!returns_ok) {
        for (int i = 0; i < out.returns.size(); ++i)
            out.returns[i] = (i == 0 || out.equity[i - 1] == 0.0) ? 0.0 : out.equity[i] / out.equity[i - 1] - 1.0;
    }
    return out;
}

QVector<QPointF> dated_points(const QJsonArray& rows, const QString& value_key, double scale) {
    QVector<QPointF> out;
    out.reserve(rows.size());
    for (const QJsonValue& v : rows) {
        const QJsonObject o = v.toObject();
        const double t = parse_time_ms(o.value(QStringLiteral("date")).toString());
        const QJsonValue val = o.value(value_key);
        if (!std::isfinite(t) || !val.isDouble())
            continue;
        out.append(QPointF(t, val.toDouble() * scale));
    }
    return out;
}

QVector<double> thin(const QVector<double>& v, int max_points) {
    if (v.size() <= max_points || max_points < 2)
        return v;
    QVector<double> out;
    out.reserve(max_points + 1);
    const double step = static_cast<double>(v.size() - 1) / (max_points - 1);
    for (int i = 0; i < max_points; ++i)
        out.append(v.at(static_cast<int>(std::round(i * step))));
    return out;
}

QString humanize_key(const QString& key) {
    QString k = key;
    k.replace(QLatin1Char('_'), QLatin1Char(' '));
    static const QRegularExpression camel(QStringLiteral("([a-z0-9])([A-Z])"));
    k.replace(camel, QStringLiteral("\\1 \\2"));
    if (!k.isEmpty())
        k[0] = k[0].toUpper();
    return k;
}

} // namespace fincept::screens::btview
