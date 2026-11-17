#include "services/markets/MarketSearchService.h"

#include "core/logging/Logger.h"
#include "network/http/HttpClient.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <QStringList>
#include <QUrl>

#include <algorithm>

namespace fincept::services {

namespace {

constexpr int kMinLimit = 1;
// GET /market/search validates `limit` as 1..50 and rejects anything larger
// (422), so a caller asking for more must get 50, not an error.
constexpr int kMaxLimit = 50;

/// Yahoo form of a listing, for responses from a backend that does not send `yf_symbol` yet.
/// Mirrors the backend's _yahoo_symbol_for_listing for the venues the ticker database covers.
QString yahoo_symbol_for_listing(const QString& symbol, const QString& exchange, const QString& country) {
    static const QHash<QString, QString> kSuffix = {
        {"NSE", ".NS"},    {"BSE", ".BO"},   {"SGX", ".SI"},   {"HKEX", ".HK"},  {"TSE", ".T"},
        {"KRX", ".KS"},    {"TWSE", ".TW"},  {"TPEX", ".TWO"}, {"ASX", ".AX"},   {"NZX", ".NZ"},
        {"IDX", ".JK"},    {"MYX", ".KL"},   {"SET", ".BK"},   {"PSE", ".PS"},   {"SZSE", ".SZ"},
        {"SSE", ".SS"},    {"LSE", ".L"},    {"XETR", ".DE"},  {"FWB", ".F"},    {"SIX", ".SW"},
        {"MIL", ".MI"},    {"BME", ".MC"},   {"OMXSTO", ".ST"}, {"OMXCOP", ".CO"}, {"OMXHEX", ".HE"},
        {"OSL", ".OL"},    {"VIE", ".VI"},   {"GPW", ".WA"},   {"TSX", ".TO"},   {"TSXV", ".V"},
        {"BMFBOVESPA", ".SA"}, {"BMV", ".MX"}, {"TASE", ".TA"}, {"TADAWUL", ".SR"}, {"JSE", ".JO"},
        {"BIST", ".IS"},   {"EGX", ".CA"},
    };
    static const QHash<QString, QString> kEuronextSuffix = {
        {"FR", ".PA"}, {"NL", ".AS"}, {"BE", ".BR"}, {"PT", ".LS"}, {"IE", ".IR"}, {"NO", ".OL"},
    };
    static const QStringList kUsExchanges = {"NYSE", "NASDAQ", "AMEX", "OTC", "CBOE"};

    const QString exch = exchange.trimmed().toUpper();
    QString base = symbol.trimmed().toUpper();
    if (kUsExchanges.contains(exch))
        return base.replace(QLatin1Char('.'), QLatin1Char('-')); // BRK.B -> BRK-B
    const QString suffix = exch == QLatin1String("EURONEXT")
                               ? kEuronextSuffix.value(country.trimmed().toUpper(), QStringLiteral(".PA"))
                               : kSuffix.value(exch);
    if (suffix.isEmpty())
        return base; // venue without a Yahoo suffix (or unknown): as listed
    if (exch == QLatin1String("HKEX") && !base.isEmpty() &&
        std::all_of(base.cbegin(), base.cend(), [](QChar c) { return c.isDigit(); }))
        base = base.rightJustified(4, QLatin1Char('0')); // Yahoo: 0700.HK, never 700.HK
    return base.endsWith(suffix) ? base : base + suffix;
}

QList<MarketSearchService::Item> parse_items(const QJsonDocument& doc) {
    QJsonArray arr;
    if (doc.isArray()) {
        arr = doc.array();
    } else if (doc.isObject()) {
        const auto obj = doc.object();
        if (obj.contains("results"))
            arr = obj["results"].toArray();
        else if (obj.contains("data"))
            arr = obj["data"].toArray();
    }

    QList<MarketSearchService::Item> out;
    out.reserve(arr.size());
    for (const auto& v : arr) {
        const auto obj = v.toObject();
        const QString sym = obj["symbol"].toString();
        if (sym.isEmpty())
            continue;
        const QString exchange = obj["exchange"].toString();
        const QString country = obj["country"].toString();
        // The backend sends `yf_symbol` (null when the venue has no Yahoo mapping); an older
        // backend does not, so derive it from the exchange the row already carries.
        QString yf = obj["yf_symbol"].toString();
        if (yf.isEmpty())
            yf = yahoo_symbol_for_listing(sym, exchange, country);
        out.push_back({sym, obj["name"].toString(), exchange, obj["type"].toString(), country, yf});
    }
    return out;
}

} // namespace

MarketSearchService& MarketSearchService::instance() {
    static MarketSearchService s;
    return s;
}

MarketSearchService::MarketSearchService() = default;

void MarketSearchService::search(const QString& query, const QString& type, int limit, const QString& request_id) {
    const QString q = query.trimmed();
    if (q.isEmpty()) {
        emit results_ready(request_id, query, {});
        return;
    }
    const int clamped = std::clamp(limit, kMinLimit, kMaxLimit);

    // Percent-encode the query so symbols like "S&P" / "AT&T" / spaces don't
    // break the query string (the raw '&' would be read as a param separator).
    QString url = QString("/market/search?q=%1&limit=%2")
                      .arg(QString::fromUtf8(QUrl::toPercentEncoding(q)), QString::number(clamped));
    if (!type.isEmpty())
        url += "&type=" + QString::fromUtf8(QUrl::toPercentEncoding(type));

    QPointer<MarketSearchService> self = this;
    HttpClient::instance().get(
        url,
        [self, request_id, query](Result<QJsonDocument> result) {
            if (!self)
                return;
            if (!result.is_ok()) {
                const QString reason = QString::fromStdString(result.error());
                LOG_WARN("MarketSearch", "search failed: " + reason);
                emit self->search_failed(request_id, query, reason);
                return;
            }
            emit self->results_ready(request_id, query, parse_items(result.value()));
        },
        this);
}

} // namespace fincept::services
