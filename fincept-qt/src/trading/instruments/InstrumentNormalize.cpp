#include "trading/instruments/InstrumentNormalize.h"

#include <QDate>
#include <QDateTime>
#include <QHash>
#include <QRegularExpression>

namespace fincept::trading::norm {

namespace {

// Spaced-name index map (broker tradingsymbol/name → canonical). Mirrors
// OpenAlgo's index_mapping dict and the existing Zerodha parser. Comprehensive
// so index naming is uniform across every broker.
const QHash<QString, QString>& spaced_index_map() {
    static const QHash<QString, QString> m = {
        {"NIFTY 50", "NIFTY"},
        {"NIFTY BANK", "BANKNIFTY"},
        {"NIFTY FIN SERVICE", "FINNIFTY"},
        {"NIFTY MID SELECT", "MIDCPNIFTY"},
        {"NIFTY MIDCAP SELECT", "MIDCPNIFTY"},
        {"NIFTY NEXT 50", "NIFTYNXT50"},
        {"INDIA VIX", "INDIAVIX"},
        {"NIFTY100 LIQ 15", "NIFTY100LIQ15"},
        {"NIFTY MIDCAP 50", "NIFTYMIDCAP50"},
        {"NIFTY MIDCAP 100", "NIFTYMIDCAP100"},
        {"NIFTY SMALLCAP 50", "NIFTYSMALLCAP50"},
        {"NIFTY SMALLCAP 100", "NIFTYSMALLCAP100"},
        {"NIFTY 100", "NIFTY100"},
        {"NIFTY 200", "NIFTY200"},
        {"NIFTY 500", "NIFTY500"},
        {"NIFTY AUTO", "NIFTYAUTO"},
        {"NIFTY ENERGY", "NIFTYENERGY"},
        {"NIFTY FMCG", "NIFTYFMCG"},
        {"NIFTY IT", "NIFTYIT"},
        {"NIFTY MEDIA", "NIFTYMEDIA"},
        {"NIFTY METAL", "NIFTYMETAL"},
        {"NIFTY PHARMA", "NIFTYPHARMA"},
        {"NIFTY PSU BANK", "NIFTYPSUBANK"},
        {"NIFTY REALTY", "NIFTYREALTY"},
        {"NIFTY INFRA", "NIFTYINFRA"},
        {"NIFTY SERV SECTOR", "NIFTYSERVSECTOR"},
        {"NIFTY COMMODITIES", "NIFTYCOMMODITIES"},
        {"NIFTY CONSUMPTION", "NIFTYCONSUMPTION"},
        {"NIFTY CPSE", "NIFTYCPSE"},
        {"NIFTY GROWSECT 15", "NIFTYGROWSECT15"},
        {"NIFTY100 QUALTY30", "NIFTY100QUALTY30"},
        {"NIFTY50 VALUE 20", "NIFTY50VALUE20"},
        {"NIFTY ALPHA 50", "NIFTYALPHA50"},
        {"NIFTY ALPHA LOW-VOL", "NIFTYALPHALOWVOL"},
        {"NIFTY200 QUALTY30", "NIFTY200QUALTY30"},
        // BSE indices
        {"SENSEX", "SENSEX"},
        {"BSE SENSEX", "SENSEX"},
        {"SENSEX 50", "SENSEX50"},
        {"BANKEX", "BANKEX"},
        {"BSE BANKEX", "BANKEX"},
    };
    return m;
}

// Pre-stripped index map (spaces/hyphens already removed, upper-cased).
const QHash<QString, QString>& stripped_index_map() {
    static const QHash<QString, QString> m = {
        {"NIFTY50", "NIFTY"},
        {"NIFTYBANK", "BANKNIFTY"},
        {"NIFTYFINSERVICE", "FINNIFTY"},
        {"NIFTYNEXT50", "NIFTYNXT50"},
        {"NIFTYMIDSELECT", "MIDCPNIFTY"},
        {"NIFTYMIDCAPSELECT", "MIDCPNIFTY"},
        {"SNSX50", "SENSEX50"},
        {"INDIAVIX", "INDIAVIX"},
        {"BSESENSEX", "SENSEX"},
        {"BSEBANKEX", "BANKEX"},
        {"NIFTYMIDCAPSELECT", "MIDCPNIFTY"},
        {"NIFTYMCAP50", "NIFTYMIDCAP50"},
    };
    return m;
}

} // namespace

QString expiry_to_nodash(const QString& raw) {
    const QString s = raw.trimmed();
    if (s.isEmpty())
        return {};

    // Try the most specific / common formats first. Upper-case so month names match.
    const QString up = s.toUpper();
    static const char* fmts[] = {"yyyy-MM-dd", "ddMMMyyyy", "dd-MMM-yyyy", "dd-MMM-yy",
                                 "ddMMMyy",    "dd/MM/yyyy", "dd/MM/yy"};
    for (const char* f : fmts) {
        QDate d = QDate::fromString(up, QString::fromLatin1(f));
        if (d.isValid())
            return d.toString("ddMMMyy").toUpper();
    }
    // Datetime forms like "2026-06-30 14:00:00".
    QDateTime dt = QDateTime::fromString(s, "yyyy-MM-dd HH:mm:ss");
    if (dt.isValid())
        return dt.date().toString("ddMMMyy").toUpper();
    // ISO-ish fallback.
    QDate d = QDate::fromString(s.left(10), "yyyy-MM-dd");
    if (d.isValid())
        return d.toString("ddMMMyy").toUpper();
    return {};
}

QString nodash_to_display(const QString& nd) {
    if (nd.size() != 7)
        return {};
    return nd.left(2) + "-" + nd.mid(2, 3) + "-" + nd.right(2);
}

QString expiry_display(const QString& raw) {
    return nodash_to_display(expiry_to_nodash(raw));
}

QString format_strike(double strike) {
    // Whole number → no decimal. e.g. 25000.0 → "25000".
    if (qFuzzyCompare(strike, static_cast<double>(qRound64(strike))))
        return QString::number(qRound64(strike));
    // Fractional → trim trailing zeros (and a dangling '.').
    QString s = QString::number(strike, 'f', 4);
    while (s.endsWith('0'))
        s.chop(1);
    if (s.endsWith('.'))
        s.chop(1);
    return s;
}

QString expiry_friendly(const QString& display_expiry) {
    // Stored form is "DD-MMM-YY" (e.g. "07-JUL-26"). Make it "7 Jul 26": drop the
    // day's leading zero and title-case the month.
    const QStringList parts = display_expiry.split('-', Qt::SkipEmptyParts);
    if (parts.size() != 3)
        return display_expiry;
    const int day = parts[0].toInt();
    if (day <= 0)
        return display_expiry;
    const QString mon = parts[1].left(1).toUpper() + parts[1].mid(1).toLower();
    return QStringLiteral("%1 %2 %3").arg(QString::number(day), mon, parts[2]);
}

QString display_name(const QString& name, InstrumentType itype, const QString& expiry, double strike,
                     const QString& fallback_symbol) {
    switch (itype) {
        case InstrumentType::FUT:
            return QStringLiteral("%1 %2 FUT").arg(name, expiry_friendly(expiry));
        case InstrumentType::CE:
            return QStringLiteral("%1 %2 %3 CE").arg(name, expiry_friendly(expiry), format_strike(strike));
        case InstrumentType::PE:
            return QStringLiteral("%1 %2 %3 PE").arg(name, expiry_friendly(expiry), format_strike(strike));
        case InstrumentType::INDEX:
            return name.isEmpty() ? fallback_symbol : name;
        case InstrumentType::EQ:
        case InstrumentType::UNKNOWN:
        default:
            return fallback_symbol.isEmpty() ? name : fallback_symbol;
    }
}

QString strip_eq_suffix(const QString& trading_symbol) {
    QString s = trading_symbol.trimmed().toUpper();
    static const QRegularExpression re("(-EQ|-BE|-MF|-SG|-SM)$");
    s.replace(re, "");
    return s;
}

QString normalise_index_symbol(const QString& raw_name) {
    QString up = raw_name.trimmed().toUpper();
    up.replace("S&P ", "");
    // Spaced form first (preserves multi-word keys).
    auto it = spaced_index_map().constFind(up);
    if (it != spaced_index_map().constEnd())
        return it.value();
    // Strip spaces/hyphens, retry.
    QString stripped = up;
    stripped.remove(' ');
    stripped.remove('-');
    auto jt = stripped_index_map().constFind(stripped);
    if (jt != stripped_index_map().constEnd())
        return jt.value();
    return stripped;
}

QString synthesize_symbol(const QString& name, InstrumentType itype, const QString& expiry_nd, double strike,
                          const QString& eq_or_index_fallback) {
    switch (itype) {
        case InstrumentType::FUT:
            return name.toUpper() + expiry_nd + "FUT";
        case InstrumentType::CE:
            return name.toUpper() + expiry_nd + format_strike(strike) + "CE";
        case InstrumentType::PE:
            return name.toUpper() + expiry_nd + format_strike(strike) + "PE";
        case InstrumentType::INDEX:
            return normalise_index_symbol(eq_or_index_fallback);
        case InstrumentType::EQ:
        case InstrumentType::UNKNOWN:
        default:
            return strip_eq_suffix(eq_or_index_fallback);
    }
}

QString normalise_exchange(const QString& exchange, InstrumentType itype) {
    QString ex = exchange.trimmed().toUpper();
    if (itype == InstrumentType::INDEX && !ex.endsWith("_INDEX"))
        ex += "_INDEX";
    return ex;
}

qint64 stable_token(const QString& broker_native_token) {
    const QString s = broker_native_token.trimmed();
    if (s.isEmpty())
        return 0;
    // Leading integer (e.g. Samco "758960_NSE" → 758960).
    int i = 0;
    while (i < s.size() && s[i].isDigit())
        ++i;
    if (i > 0) {
        bool ok = false;
        const qint64 n = QStringView(s).left(i).toLongLong(&ok);
        if (ok && n > 0)
            return n;
    }
    // 64-bit FNV-1a hash of the full key, forced positive & non-zero.
    const QByteArray bytes = s.toUtf8();
    quint64 h = 1469598103934665603ULL; // FNV offset basis
    for (char c : bytes) {
        h ^= static_cast<unsigned char>(c);
        h *= 1099511628211ULL; // FNV prime
    }
    qint64 v = static_cast<qint64>(h & 0x7FFFFFFFFFFFFFFFULL);
    return v == 0 ? 1 : v;
}

} // namespace fincept::trading::norm
