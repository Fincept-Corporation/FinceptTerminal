// FinceptDataParse.cpp — /data API answers → models; failures → DataError.
//
// Shapes follow apiback data_api/ (core.py respond()/http_error(), options.py,
// fundamentals.py + yahoo_source.py, calendars.py, discovery.py). The service
// files are unity-built, so helpers live in a named namespace, not an anonymous one.
#include "services/fincept_data/FinceptDataParse.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QTimeZone>
#include <QUrl>

#include <algorithm>
#include <cmath>
#include <map>

namespace fincept::services::fincept_data {

namespace fdata_parse_detail {

/// A JSON number, NaN for null / missing / anything else.
double num(const QJsonValue& v) {
    return v.isDouble() ? v.toDouble() : kNaN;
}

double num(const QJsonObject& o, const char* key) {
    return num(o.value(QLatin1String(key)));
}

QString str(const QJsonObject& o, const char* key) {
    const QJsonValue v = o.value(QLatin1String(key));
    return v.isString() ? v.toString() : QString();
}

int integer(const QJsonObject& o, const char* key, int fallback = 0) {
    const QJsonValue v = o.value(QLatin1String(key));
    return v.isDouble() ? static_cast<int>(v.toDouble()) : fallback;
}

QDate date(const QJsonObject& o, const char* key) {
    return QDate::fromString(str(o, key).left(10), Qt::ISODate);
}

QJsonObject obj(const QJsonObject& o, const char* key) {
    return o.value(QLatin1String(key)).toObject();
}

QJsonArray arr(const QJsonObject& o, const char* key) {
    return o.value(QLatin1String(key)).toArray();
}

/// Error kind named by the server's X-Data-Error header (data_api/core.py DataError.STATUS).
ErrorKind kind_from_code(const QString& code) {
    if (code == QLatin1String("not_found"))
        return ErrorKind::NotFound;
    if (code == QLatin1String("invalid"))
        return ErrorKind::Invalid;
    if (code == QLatin1String("throttled") || code == QLatin1String("busy"))
        return ErrorKind::Throttled;
    if (code == QLatin1String("not_configured"))
        return ErrorKind::NotConfigured;
    if (code == QLatin1String("unreachable") || code == QLatin1String("upstream_error"))
        return ErrorKind::Upstream;
    if (code == QLatin1String("timeout"))
        return ErrorKind::Timeout;
    return ErrorKind::None;
}

EstimateBand band(const QJsonObject& o) {
    EstimateBand b;
    b.avg = num(o, "avg");
    b.low = num(o, "low");
    b.high = num(o, "high");
    b.year_ago = num(o, "year_ago");
    b.analysts = num(o, "analysts");
    b.growth = num(o, "growth");
    b.currency = str(o, "currency");
    return b;
}

QVector<Holder> holders(const QJsonArray& a) {
    QVector<Holder> out;
    out.reserve(a.size());
    for (const auto& v : a) {
        const QJsonObject o = v.toObject();
        Holder h;
        h.holder = str(o, "holder");
        h.report_date = date(o, "report_date");
        h.pct_held = num(o, "pct_held");
        h.shares = num(o, "shares");
        h.value = num(o, "value");
        h.pct_change = num(o, "pct_change");
        out.append(h);
    }
    return out;
}

} // namespace fdata_parse_detail

using namespace fdata_parse_detail;

// ── Envelope + errors ────────────────────────────────────────────────────────

QDateTime parse_instant(const QString& text) {
    const QString t = text.trimmed();
    if (t.isEmpty())
        return {};
    QDateTime dt = QDateTime::fromString(t, Qt::ISODateWithMs);
    if (!dt.isValid())
        dt = QDateTime::fromString(t, Qt::ISODate);
    if (!dt.isValid()) {
        const QDate d = QDate::fromString(t.left(10), Qt::ISODate);
        if (!d.isValid())
            return {};
        return QDateTime(d, QTime(0, 0), QTimeZone::UTC);
    }
    // No offset in the text: the API writes UTC instants (meta.as_of, calendar times).
    if (dt.timeSpec() == Qt::LocalTime)
        dt = QDateTime(dt.date(), dt.time(), QTimeZone::UTC);
    return dt.toUTC();
}

DataMeta parse_meta(const QJsonObject& m) {
    DataMeta meta;
    meta.dataset = str(m, "dataset");
    meta.source = str(m, "source");
    meta.source_name = str(m, "source_name");
    meta.as_of = parse_instant(str(m, "as_of"));
    meta.cached = m.value(QLatin1String("cached")).toBool();
    meta.age_seconds = std::max(0, integer(m, "age_seconds"));
    meta.max_age_seconds = std::max(0, integer(m, "max_age_seconds"));
    meta.stale = m.value(QLatin1String("stale")).toBool();
    meta.stale_reason = str(m, "stale_reason");
    meta.delayed_minutes = std::max(0, integer(m, "delayed_minutes"));
    meta.source_time = str(m, "source_time");
    meta.source_timezone = str(m, "source_timezone");
    const QJsonValue p = m.value(QLatin1String("pagination"));
    if (p.isObject()) {
        const QJsonObject po = p.toObject();
        meta.pagination.present = true;
        meta.pagination.page = integer(po, "page", 1);
        meta.pagination.page_size = integer(po, "page_size");
        meta.pagination.total = integer(po, "total");
        meta.pagination.total_pages = integer(po, "total_pages");
        meta.pagination.has_more = po.value(QLatin1String("has_more")).toBool();
    }
    return meta;
}

DataResult parse_envelope(const QJsonDocument& doc, int status) {
    const int fallback_status = status;
    DataResult r;
    if (!doc.isObject()) {
        r.error.kind = ErrorKind::BadResponse;
        r.error.status = fallback_status;
        return r;
    }
    const QJsonObject root = doc.object();
    const QJsonValue success = root.value(QLatin1String("success"));
    if (success.isBool() && !success.toBool()) {
        // {"success": false, "message": ...} on a 2xx: a refusal, never data.
        QString message;
        const QJsonValue m = root.value(QLatin1String("message"));
        if (m.isString())
            message = m.toString();
        else if (m.isObject())
            message = m.toObject().value(QLatin1String("message")).toString();
        r.error.kind = ErrorKind::BadResponse;
        r.error.code = QStringLiteral("refused");
        r.error.status = fallback_status;
        r.error.message = message.trimmed();
        return r;
    }
    const QJsonValue data = root.value(QLatin1String("data"));
    const QJsonValue meta = root.value(QLatin1String("meta"));
    if (!data.isObject() || !meta.isObject()) {
        r.error.kind = ErrorKind::BadResponse;
        r.error.status = fallback_status;
        return r;
    }
    r.data = data.toObject();
    r.meta = parse_meta(meta.toObject());
    return r;
}

QStringList suggestions_from_message(const QString& message) {
    // data_api/core.py http_error(): message += ". Did you mean: " + ", ".join(s) + "?"
    static const QLatin1String kMarker("Did you mean:");
    const qsizetype at = message.indexOf(kMarker, 0, Qt::CaseInsensitive);
    if (at < 0)
        return {};
    QString tail = message.mid(at + kMarker.size()).trimmed();
    if (tail.endsWith(QLatin1Char('?')))
        tail.chop(1);
    QStringList out;
    for (const QString& part : tail.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        const QString s = part.trimmed();
        if (!s.isEmpty() && !s.contains(QLatin1Char(' ')))
            out.append(s);
    }
    return out;
}

bool is_missing_route(int status, const QString& message) {
    if (status != 404)
        return false;
    const QString m = message.trimmed();
    return m.isEmpty() || m.compare(QLatin1String("Not Found"), Qt::CaseInsensitive) == 0;
}

int default_retry_after_s(const DataError& e) {
    switch (e.kind) {
    case ErrorKind::Throttled:
        // Server: Retry-After = the provider's remaining cooldown (30-60 s), or the
        // budget window / 2 s for "busy"; 30 s when it knows nothing better.
        return e.code == QLatin1String("busy") ? 10 : 30;
    case ErrorKind::RateLimited:
        return 60;
    case ErrorKind::NotConfigured:
        return 300;
    default:
        return 0;
    }
}

DataError classify_error(int status, const QString& message, int retry_after_s, const QString& suggestions_header,
                         const QString& kind_header) {
    DataError e;
    e.status = status;
    e.message = message.trimmed();
    const QString m = e.message;
    const QString code = kind_header.trimmed().toLower();

    if (status == 0) {
        e.kind = ErrorKind::Network;
        e.code = QStringLiteral("network");
    } else if (status == 401) {
        e.kind = ErrorKind::NotSignedIn;
        e.code = QStringLiteral("unauthorized");
    } else if (status == 402) {
        e.kind = ErrorKind::NoCredits;
        e.code = QStringLiteral("insufficient_credits");
    } else if (status == 403) {
        e.kind = ErrorKind::Forbidden;
        e.code = QStringLiteral("forbidden");
    } else if (status == 429) {
        e.kind = ErrorKind::RateLimited;
        e.code = QStringLiteral("rate_limited");
    } else if (status == 404 && code.isEmpty() && is_missing_route(status, m)) {
        e.kind = ErrorKind::ServerLacksApi;
        e.code = QStringLiteral("missing_route");
    } else if (!code.isEmpty() && kind_from_code(code) != ErrorKind::None) {
        e.kind = kind_from_code(code);
        e.code = code;
    } else if (status == 404 || status == 410) {
        e.kind = ErrorKind::NotFound;
        e.code = QStringLiteral("not_found");
    } else if (status == 422 || status == 400) {
        e.kind = ErrorKind::Invalid;
        e.code = QStringLiteral("invalid");
    } else if (status == 503) {
        // Without X-Data-Error, the server's fixed wording tells the kinds apart.
        if (m.contains(QLatin1String("not configured"), Qt::CaseInsensitive)) {
            e.kind = ErrorKind::NotConfigured;
            e.code = QStringLiteral("not_configured");
        } else if (m.contains(QLatin1String("request limit"), Qt::CaseInsensitive)) {
            e.kind = ErrorKind::Throttled;
            e.code = QStringLiteral("busy");
        } else {
            e.kind = ErrorKind::Throttled;
            e.code = QStringLiteral("throttled");
        }
    } else if (status == 502) {
        e.kind = ErrorKind::Upstream;
        e.code = m.contains(QLatin1String("unreachable"), Qt::CaseInsensitive) ? QStringLiteral("unreachable")
                                                                               : QStringLiteral("upstream_error");
    } else if (status == 504) {
        e.kind = ErrorKind::Timeout;
        e.code = QStringLiteral("timeout");
    } else {
        e.kind = ErrorKind::Server;
        e.code = QStringLiteral("http_%1").arg(status);
    }

    if (e.kind == ErrorKind::NotFound) {
        if (!suggestions_header.trimmed().isEmpty()) {
            for (const QString& s : suggestions_header.split(QLatin1Char(','), Qt::SkipEmptyParts))
                if (!s.trimmed().isEmpty())
                    e.suggestions.append(s.trimmed());
        } else {
            e.suggestions = suggestions_from_message(m);
        }
    }
    e.retry_after_s = retry_after_s > 0 ? retry_after_s : default_retry_after_s(e);
    return e;
}

QString path_segment(const QString& value) {
    return QString::fromUtf8(QUrl::toPercentEncoding(value.trimmed()));
}

QString request_key(const QString& path, const QueryParams& query) {
    QueryParams sorted = query;
    std::sort(sorted.begin(), sorted.end());
    QString key = path;
    QChar sep = QLatin1Char('?');
    for (const auto& kv : sorted) {
        key += sep + QString::fromUtf8(QUrl::toPercentEncoding(kv.first)) + QLatin1Char('=') +
               QString::fromUtf8(QUrl::toPercentEncoding(kv.second));
        sep = QLatin1Char('&');
    }
    return key;
}

// ── Catalogue ────────────────────────────────────────────────────────────────

DatasetCatalog parse_catalog(const QJsonObject& data) {
    DatasetCatalog cat;
    for (const auto& v : arr(data, "datasets")) {
        const QJsonObject o = v.toObject();
        DatasetInfo d;
        d.id = str(o, "id");
        if (d.id.isEmpty())
            continue;
        d.path = str(o, "path");
        d.group = str(o, "group");
        d.summary = str(o, "summary");
        const QJsonObject src = obj(o, "source");
        d.source_id = str(src, "id");
        d.source_name = str(src, "name");
        const QJsonObject fresh = obj(o, "freshness");
        d.freshness = str(fresh, "description");
        d.ttl_seconds = integer(fresh, "ttl_seconds");
        const QJsonValue avail = o.value(QLatin1String("available"));
        d.available = avail.isBool() ? avail.toBool() : true;
        d.paginated = o.value(QLatin1String("paginated")).toBool();
        const double credits = num(o, "credits_per_call");
        d.credits_per_call = has(credits) ? credits : 0.0;
        cat.datasets.insert(d.id, d);
    }
    return cat;
}

// ── Options ──────────────────────────────────────────────────────────────────

OptionChain parse_option_chain(const QJsonObject& data) {
    OptionChain c;
    c.symbol = str(data, "symbol");
    c.resolved_symbol = str(data, "resolved_symbol");
    c.exchange = str(data, "exchange");
    c.expiration = str(data, "expiration");
    c.greeks_available = data.value(QLatin1String("greeks_available")).toBool();
    for (const auto& v : arr(data, "expirations")) {
        // options.chain lists dates; options.expirations lists {"date", "dte", ...}.
        const QString d = v.isObject() ? v.toObject().value(QLatin1String("date")).toString() : v.toString();
        if (!d.isEmpty())
            c.expirations.append(d);
    }

    const QJsonObject u = obj(data, "underlying");
    c.underlying.symbol = str(u, "symbol");
    c.underlying.security_type = str(u, "security_type");
    c.underlying.price = num(u, "price");
    c.underlying.bid = num(u, "bid");
    c.underlying.ask = num(u, "ask");
    c.underlying.prev_close = num(u, "prev_close");
    c.underlying.change = num(u, "change");
    c.underlying.change_pct = num(u, "change_pct");
    c.underlying.volume = num(u, "volume");
    c.underlying.iv30 = num(u, "iv30");
    c.underlying.last_trade_time = str(u, "last_trade_time");

    const QJsonArray contracts = arr(data, "contracts");
    c.contracts.reserve(contracts.size());
    for (const auto& v : contracts) {
        const QJsonObject o = v.toObject();
        OptionContract k;
        k.contract = str(o, "contract");
        k.expiration = str(o, "expiration");
        const QString type = str(o, "type");
        if (type != QLatin1String("call") && type != QLatin1String("put"))
            continue;
        k.is_call = type == QLatin1String("call");
        k.strike = num(o, "strike");
        k.bid = num(o, "bid");
        k.ask = num(o, "ask");
        k.mid = num(o, "mid");
        k.last = num(o, "last");
        k.change = num(o, "change");
        k.change_pct = num(o, "change_pct");
        k.volume = num(o, "volume");
        k.open_interest = num(o, "open_interest");
        k.iv = num(o, "iv");
        k.delta = num(o, "delta");
        k.gamma = num(o, "gamma");
        k.theta = num(o, "theta");
        k.vega = num(o, "vega");
        k.rho = num(o, "rho");
        k.theo = num(o, "theo");
        k.dte = integer(o, "dte", -1);
        const QJsonValue itm = o.value(QLatin1String("in_the_money"));
        k.in_the_money = itm.isBool() ? (itm.toBool() ? 1 : 0) : -1;
        k.last_trade_time = str(o, "last_trade_time");
        c.contracts.append(k);
    }
    return c;
}

QVector<StraddleRow> straddle_rows(const QVector<OptionContract>& contracts) {
    std::map<double, StraddleRow> by_strike;
    for (int i = 0; i < contracts.size(); ++i) {
        const auto& k = contracts[i];
        if (!has(k.strike))
            continue;
        auto& row = by_strike[k.strike];
        row.strike = k.strike;
        int& slot = k.is_call ? row.call : row.put;
        if (slot < 0)
            slot = i;
    }
    QVector<StraddleRow> out;
    out.reserve(static_cast<qsizetype>(by_strike.size()));
    for (const auto& kv : by_strike)
        out.append(kv.second);
    return out;
}

int at_the_money_row(const QVector<StraddleRow>& rows, double spot) {
    if (!has(spot) || rows.isEmpty())
        return -1;
    int best = -1;
    double best_gap = 0.0;
    for (int i = 0; i < rows.size(); ++i) {
        const double gap = std::fabs(rows[i].strike - spot);
        if (best < 0 || gap < best_gap) {
            best = i;
            best_gap = gap;
        }
    }
    return best;
}

// ── Analysts ─────────────────────────────────────────────────────────────────

AnalystOverview parse_analyst(const QJsonObject& data) {
    AnalystOverview a;
    a.symbol = str(data, "symbol");
    a.resolved_symbol = str(data, "resolved_symbol");
    a.name = str(data, "name");
    a.currency = str(data, "currency");

    const QJsonObject pt = obj(data, "price_targets");
    a.targets.current_price = num(pt, "current_price");
    a.targets.low = num(pt, "low");
    a.targets.mean = num(pt, "mean");
    a.targets.median = num(pt, "median");
    a.targets.high = num(pt, "high");
    a.targets.analysts = num(pt, "analysts");
    a.targets.recommendation_mean = num(pt, "recommendation_mean");
    a.targets.recommendation_key = str(pt, "recommendation_key");

    for (const auto& v : arr(data, "recommendation_trend")) {
        const QJsonObject o = v.toObject();
        RecommendationPeriod p;
        p.period = str(o, "period");
        p.strong_buy = num(o, "strong_buy");
        p.buy = num(o, "buy");
        p.hold = num(o, "hold");
        p.sell = num(o, "sell");
        p.strong_sell = num(o, "strong_sell");
        a.trend.append(p);
    }

    for (const auto& v : arr(data, "estimates")) {
        const QJsonObject o = v.toObject();
        Estimate e;
        e.period = str(o, "period");
        e.end_date = str(o, "end_date");
        e.growth = num(o, "growth");
        e.eps = band(obj(o, "eps"));
        e.revenue = band(obj(o, "revenue"));
        const QJsonObject t = obj(o, "eps_trend");
        e.eps_current = num(t, "current");
        e.eps_7d_ago = num(t, "7_days_ago");
        e.eps_30d_ago = num(t, "30_days_ago");
        e.eps_60d_ago = num(t, "60_days_ago");
        e.eps_90d_ago = num(t, "90_days_ago");
        const QJsonObject r = obj(o, "eps_revisions");
        e.up_7d = num(r, "up_last_7_days");
        e.up_30d = num(r, "up_last_30_days");
        e.down_7d = num(r, "down_last_7_days");
        e.down_30d = num(r, "down_last_30_days");
        a.estimates.append(e);
    }

    for (const auto& v : arr(data, "earnings_history")) {
        const QJsonObject o = v.toObject();
        EarningsSurprise s;
        s.quarter = str(o, "quarter");
        s.period = str(o, "period");
        s.eps_actual = num(o, "eps_actual");
        s.eps_estimate = num(o, "eps_estimate");
        s.eps_difference = num(o, "eps_difference");
        s.surprise_pct = num(o, "surprise_pct");
        a.history.append(s);
    }

    for (const auto& v : arr(data, "upgrades_downgrades")) {
        const QJsonObject o = v.toObject();
        RatingChange g;
        g.date = parse_instant(str(o, "date"));
        g.firm = str(o, "firm");
        g.to_grade = str(o, "to_grade");
        g.from_grade = str(o, "from_grade");
        g.action = str(o, "action");
        g.price_target_action = str(o, "price_target_action");
        g.price_target = num(o, "price_target");
        g.prior_price_target = num(o, "prior_price_target");
        a.rating_changes.append(g);
    }
    a.rating_changes_total =
        std::max(integer(data, "upgrades_downgrades_total"), static_cast<int>(a.rating_changes.size()));
    return a;
}

// ── Ownership ────────────────────────────────────────────────────────────────

OwnershipOverview parse_ownership(const QJsonObject& data) {
    OwnershipOverview w;
    w.symbol = str(data, "symbol");
    w.resolved_symbol = str(data, "resolved_symbol");
    const QJsonObject b = obj(data, "breakdown");
    w.insiders_pct = num(b, "insiders_pct");
    w.institutions_pct = num(b, "institutions_pct");
    w.institutions_float_pct = num(b, "institutions_float_pct");
    w.institutions_count = num(b, "institutions_count");
    w.institutional = holders(arr(data, "institutional_holders"));
    w.funds = holders(arr(data, "fund_holders"));

    for (const auto& v : arr(data, "insider_transactions")) {
        const QJsonObject o = v.toObject();
        InsiderTransaction t;
        t.date = date(o, "date");
        t.insider = str(o, "insider");
        t.relation = str(o, "relation");
        t.transaction = str(o, "transaction");
        t.ownership = str(o, "ownership");
        t.shares = num(o, "shares");
        t.value = num(o, "value");
        w.transactions.append(t);
    }
    w.transactions_total =
        std::max(integer(data, "insider_transactions_total"), static_cast<int>(w.transactions.size()));

    for (const auto& v : arr(data, "insider_roster")) {
        const QJsonObject o = v.toObject();
        InsiderRosterEntry r;
        r.name = str(o, "name");
        r.relation = str(o, "relation");
        r.latest_transaction = str(o, "latest_transaction");
        r.latest_transaction_date = date(o, "latest_transaction_date");
        r.shares_direct = num(o, "shares_direct");
        r.shares_indirect = num(o, "shares_indirect");
        w.roster.append(r);
    }

    const QJsonValue act = data.value(QLatin1String("insider_activity"));
    if (act.isObject()) {
        const QJsonObject o = act.toObject();
        w.activity.present = true;
        w.activity.period = str(o, "period");
        w.activity.buy_count = num(o, "buy_count");
        w.activity.buy_shares = num(o, "buy_shares");
        w.activity.sell_count = num(o, "sell_count");
        w.activity.sell_shares = num(o, "sell_shares");
        w.activity.net_count = num(o, "net_count");
        w.activity.net_shares = num(o, "net_shares");
        w.activity.net_pct_insider_shares = num(o, "net_pct_insider_shares");
        w.activity.total_insider_shares = num(o, "total_insider_shares");
        w.activity.net_institutional_shares_buying = num(o, "net_institutional_shares_buying");
    }
    return w;
}

// ── Calendars ────────────────────────────────────────────────────────────────

CalendarPage parse_calendar(CalendarKind kind, const QJsonObject& data) {
    CalendarPage page;
    page.kind = kind;
    page.start = date(data, "start_date");
    page.end = date(data, "end_date");
    page.symbol = str(data, "resolved_symbol");
    if (page.symbol.isEmpty())
        page.symbol = str(data, "symbol");

    const QJsonArray events = arr(data, "events");
    page.events.reserve(events.size());
    for (const auto& v : events) {
        const QJsonObject o = v.toObject();
        CalendarEvent e;
        e.symbol = str(o, "symbol");
        e.company = str(o, "company");
        // Field names per kind: yahoo_source.CALENDAR_FIELDS.
        switch (kind) {
        case CalendarKind::Earnings:
            e.when = parse_instant(str(o, "datetime"));
            e.event = str(o, "event");
            e.timing = str(o, "timing");
            e.market_cap = num(o, "market_cap");
            e.eps_estimate = num(o, "eps_estimate");
            e.eps_actual = num(o, "eps_actual");
            e.eps_surprise_pct = num(o, "eps_surprise_pct");
            break;
        case CalendarKind::Ipos:
            e.when = parse_instant(str(o, "date"));
            e.exchange = str(o, "exchange");
            e.currency = str(o, "currency");
            e.deal_type = str(o, "deal_type");
            e.filing_date = parse_instant(str(o, "filing_date"));
            e.amended_date = parse_instant(str(o, "amended_date"));
            e.price_from = num(o, "price_from");
            e.price_to = num(o, "price_to");
            e.offer_price = num(o, "offer_price");
            e.shares = num(o, "shares");
            break;
        case CalendarKind::Splits:
            e.when = parse_instant(str(o, "payable_date"));
            e.optionable = o.value(QLatin1String("optionable")).toBool();
            e.old_shares = num(o, "old_shares");
            e.new_shares = num(o, "new_shares");
            break;
        case CalendarKind::Economic:
            e.when = parse_instant(str(o, "datetime"));
            e.event = str(o, "event");
            e.region = str(o, "region");
            e.period = str(o, "period");
            e.actual = num(o, "actual");
            e.consensus = num(o, "consensus");
            e.prior = num(o, "prior");
            e.revised_from = num(o, "revised_from");
            break;
        }
        page.events.append(e);
    }
    return page;
}

QString calendar_name(CalendarKind kind) {
    switch (kind) {
    case CalendarKind::Earnings:
        return QStringLiteral("earnings");
    case CalendarKind::Ipos:
        return QStringLiteral("ipos");
    case CalendarKind::Splits:
        return QStringLiteral("splits");
    case CalendarKind::Economic:
        return QStringLiteral("economic");
    }
    return {};
}

QString calendar_dataset_id(CalendarKind kind) {
    return QStringLiteral("calendar.") + calendar_name(kind);
}

QString calendar_path(CalendarKind kind) {
    return QStringLiteral("/data/calendar/") + calendar_name(kind);
}

bool calendar_supports_symbol(CalendarKind kind) {
    return kind != CalendarKind::Economic;
}

int calendar_max_days(CalendarKind kind, bool with_symbol) {
    int days = 90;
    switch (kind) {
    case CalendarKind::Earnings:
        days = 90;
        break;
    case CalendarKind::Ipos:
    case CalendarKind::Splits:
        days = 180;
        break;
    case CalendarKind::Economic:
        days = 31;
        break;
    }
    // calendars.py: one company → a year of events is a handful of rows.
    if (with_symbol && calendar_supports_symbol(kind))
        days = std::max(days, 366);
    return days;
}

} // namespace fincept::services::fincept_data
