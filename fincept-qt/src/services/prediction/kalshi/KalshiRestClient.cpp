#include "services/prediction/kalshi/KalshiRestClient.h"

#include "core/logging/Logger.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QUrl>
#include <QUrlQuery>

namespace fincept::services::prediction::kalshi_ns {

namespace pr = fincept::services::prediction;

static const QString kProdBase = QStringLiteral("https://api.elections.kalshi.com/trade-api/v2");
static const QString kDemoBase = QStringLiteral("https://demo-api.kalshi.co/trade-api/v2");
static constexpr const char* kExchangeId = "kalshi";

// ── Fixed-point helpers ─────────────────────────────────────────────────────
//
// Kalshi expresses contract counts in fixed-point strings (_fp suffix)
// with 2 decimal places (1.00 == 1 contract). Convert to double at the
// boundary.

static double fp_to_double(const QJsonValue& v) {
    if (v.isString()) return v.toString().toDouble();
    if (v.isDouble()) return v.toDouble();
    return 0.0;
}

static double str_or_num(const QJsonValue& v) {
    if (v.isString()) return v.toString().toDouble();
    return v.toDouble();
}

// ── Market mapping ──────────────────────────────────────────────────────────

static pr::PredictionMarket parse_market(const QJsonObject& obj) {
    pr::PredictionMarket m;
    m.key.exchange_id = QString::fromLatin1(kExchangeId);
    m.key.market_id = obj.value("ticker").toString();
    m.key.event_id = obj.value("event_ticker").toString();
    m.key.asset_ids = {m.key.market_id + QStringLiteral(":yes"),
                       m.key.market_id + QStringLiteral(":no")};

    // Kalshi has title + yes_sub_title / no_sub_title. We use title as the
    // question and fall back to the combined sub-titles if title is missing.
    m.question = obj.value("title").toString();
    if (m.question.isEmpty()) m.question = obj.value("yes_sub_title").toString();
    m.description = obj.value("rules_primary").toString();
    m.category = obj.value("event_ticker").toString();  // Kalshi has no real category field
    m.image_url = QString();
    m.end_date_iso = obj.value("close_time").toString();

    m.volume = fp_to_double(obj.value("volume_fp"));
    m.open_interest = fp_to_double(obj.value("open_interest_fp"));
    m.liquidity = str_or_num(obj.value("liquidity_dollars"));

    const QString status = obj.value("status").toString();
    m.active = (status == QStringLiteral("active"));
    m.closed = (status == QStringLiteral("closed") || status == QStringLiteral("determined") ||
                status == QStringLiteral("finalized"));

    pr::Outcome yes;
    yes.name = QStringLiteral("Yes");
    yes.asset_id = m.key.asset_ids[0];
    yes.price = str_or_num(obj.value("yes_bid_dollars"));
    pr::Outcome no;
    no.name = QStringLiteral("No");
    no.asset_id = m.key.asset_ids[1];
    no.price = str_or_num(obj.value("no_bid_dollars"));
    m.outcomes = {yes, no};

    m.extras.insert(QStringLiteral("yes_ask_dollars"), str_or_num(obj.value("yes_ask_dollars")));
    m.extras.insert(QStringLiteral("no_ask_dollars"), str_or_num(obj.value("no_ask_dollars")));
    m.extras.insert(QStringLiteral("last_price_dollars"), str_or_num(obj.value("last_price_dollars")));
    m.extras.insert(QStringLiteral("volume_24h_fp"), fp_to_double(obj.value("volume_24h_fp")));
    m.extras.insert(QStringLiteral("can_close_early"), obj.value("can_close_early").toBool());
    m.extras.insert(QStringLiteral("market_type"), obj.value("market_type").toString());
    m.extras.insert(QStringLiteral("yes_sub_title"), obj.value("yes_sub_title").toString());
    m.extras.insert(QStringLiteral("no_sub_title"), obj.value("no_sub_title").toString());
    m.extras.insert(QStringLiteral("settlement_ts"), qint64(obj.value("settlement_ts").toDouble()));
    return m;
}

static pr::PredictionEvent parse_event(const QJsonObject& obj) {
    pr::PredictionEvent e;
    e.key.exchange_id = QString::fromLatin1(kExchangeId);
    e.key.event_id = obj.value("event_ticker").toString();
    e.title = obj.value("title").toString();
    if (e.title.isEmpty()) e.title = obj.value("sub_title").toString();
    e.description = obj.value("sub_title").toString();
    e.category = obj.value("series_ticker").toString();
    // Kalshi doesn't give event-level volume; aggregate from markets below.
    e.volume = 0.0;

    const QString status = obj.value("status").toString();
    e.active = status == QStringLiteral("active") || status == QStringLiteral("open");
    e.closed = status == QStringLiteral("closed") || status == QStringLiteral("settled");

    const auto mk_arr = obj.value("markets").toArray();
    e.markets.reserve(mk_arr.size());
    for (const auto& v : mk_arr) {
        auto m = parse_market(v.toObject());
        e.volume += m.volume;
        e.markets.push_back(m);
    }
    return e;
}

// ── KalshiRestClient ────────────────────────────────────────────────────────

KalshiRestClient::KalshiRestClient(QObject* parent) : QObject(parent) {
    nam_ = new QNetworkAccessManager(this);
}

KalshiRestClient::~KalshiRestClient() = default;

void KalshiRestClient::set_demo_mode(bool demo) { demo_ = demo; }

QString KalshiRestClient::base_url() const { return demo_ ? kDemoBase : kProdBase; }

QString KalshiRestClient::absolute_url(const QString& path) const {
    if (path.startsWith(QStringLiteral("http"))) return path;
    return base_url() + (path.startsWith('/') ? path : QStringLiteral("/") + path);
}

void KalshiRestClient::get_json(const QString& path, JsonCallback on_success, const QString& error_ctx) {
    QNetworkRequest req{QUrl(absolute_url(path))};
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("FinceptTerminal/4.0"));
    auto* reply = nam_->get(req);
    QPointer<KalshiRestClient> self = this;
    connect(reply, &QNetworkReply::finished, this, [self, reply, on_success, error_ctx]() {
        reply->deleteLater();
        if (!self) return;
        const auto data = reply->readAll();
        if (reply->error() != QNetworkReply::NoError) {
            const QString msg = QStringLiteral("%1: %2").arg(reply->errorString(),
                                                             QString::fromUtf8(data.left(200)));
            emit self->request_error(error_ctx, msg);
            return;
        }
        QJsonParseError perr;
        auto doc = QJsonDocument::fromJson(data, &perr);
        if (doc.isNull()) {
            emit self->request_error(error_ctx, QStringLiteral("JSON parse: ") + perr.errorString());
            return;
        }
        on_success(doc);
    });
}

// ── fetch_markets ───────────────────────────────────────────────────────────

void KalshiRestClient::fetch_markets(const QString& status, const QString& event_ticker,
                                     const QString& series_ticker, const QString& tickers,
                                     int limit, const QString& cursor) {
    QUrlQuery q;
    if (!status.isEmpty()) q.addQueryItem("status", status);
    if (!event_ticker.isEmpty()) q.addQueryItem("event_ticker", event_ticker);
    if (!series_ticker.isEmpty()) q.addQueryItem("series_ticker", series_ticker);
    if (!tickers.isEmpty()) q.addQueryItem("tickers", tickers);
    q.addQueryItem("limit", QString::number(qBound(1, limit, 1000)));
    if (!cursor.isEmpty()) q.addQueryItem("cursor", cursor);
    QUrl u(base_url() + QStringLiteral("/markets"));
    u.setQuery(q);

    QPointer<KalshiRestClient> self = this;
    get_json(u.toString(base_url().startsWith(QStringLiteral("https")) ?
                           QUrl::FullyEncoded : QUrl::PrettyDecoded),
             [self](const QJsonDocument& doc) {
                 if (!self) return;
                 const auto obj = doc.object();
                 QVector<pr::PredictionMarket> markets;
                 const auto arr = obj.value("markets").toArray();
                 markets.reserve(arr.size());
                 for (const auto& v : arr) markets.push_back(parse_market(v.toObject()));
                 emit self->markets_ready(markets, obj.value("cursor").toString());
             },
             QStringLiteral("Kalshi.fetch_markets"));
}

void KalshiRestClient::fetch_market(const QString& ticker) {
    QPointer<KalshiRestClient> self = this;
    get_json(QStringLiteral("/markets/") + ticker,
             [self](const QJsonDocument& doc) {
                 if (!self) return;
                 const auto m = parse_market(doc.object().value("market").toObject());
                 emit self->market_detail_ready(m);
             },
             QStringLiteral("Kalshi.fetch_market"));
}

// ── fetch_events ────────────────────────────────────────────────────────────

void KalshiRestClient::fetch_events(const QString& status, const QString& series_ticker,
                                    bool with_nested_markets, int limit, const QString& cursor) {
    QUrlQuery q;
    if (!status.isEmpty()) q.addQueryItem("status", status);
    if (!series_ticker.isEmpty()) q.addQueryItem("series_ticker", series_ticker);
    q.addQueryItem("with_nested_markets", with_nested_markets ? "true" : "false");
    q.addQueryItem("limit", QString::number(qBound(1, limit, 1000)));
    if (!cursor.isEmpty()) q.addQueryItem("cursor", cursor);
    QUrl u(base_url() + QStringLiteral("/events"));
    u.setQuery(q);

    QPointer<KalshiRestClient> self = this;
    get_json(u.toString(),
             [self](const QJsonDocument& doc) {
                 if (!self) return;
                 const auto obj = doc.object();
                 QVector<pr::PredictionEvent> events;
                 const auto arr = obj.value("events").toArray();
                 events.reserve(arr.size());
                 for (const auto& v : arr) events.push_back(parse_event(v.toObject()));
                 emit self->events_ready(events, obj.value("cursor").toString());
             },
             QStringLiteral("Kalshi.fetch_events"));
}

void KalshiRestClient::fetch_event(const QString& event_ticker) {
    QPointer<KalshiRestClient> self = this;
    get_json(QStringLiteral("/events/") + event_ticker + QStringLiteral("?with_nested_markets=true"),
             [self](const QJsonDocument& doc) {
                 if (!self) return;
                 auto obj = doc.object();
                 auto ev_obj = obj.value("event").toObject();
                 // Depending on with_nested_markets, markets may live at the
                 // top level or inside the event object. Normalise.
                 if (!ev_obj.contains("markets")) {
                     ev_obj.insert("markets", obj.value("markets"));
                 }
                 emit self->event_detail_ready(parse_event(ev_obj));
             },
             QStringLiteral("Kalshi.fetch_event"));
}

// ── fetch_series (used as tags) ─────────────────────────────────────────────

void KalshiRestClient::fetch_series(const QString& status) {
    QUrlQuery q;
    if (!status.isEmpty()) q.addQueryItem("status", status);
    QUrl u(base_url() + QStringLiteral("/series"));
    u.setQuery(q);

    QPointer<KalshiRestClient> self = this;
    get_json(u.toString(),
             [self](const QJsonDocument& doc) {
                 if (!self) return;
                 const auto arr = doc.object().value("series").toArray();
                 QStringList tickers;
                 tickers.reserve(arr.size());
                 for (const auto& v : arr) {
                     const auto o = v.toObject();
                     const QString t = o.value("ticker").toString();
                     if (!t.isEmpty()) tickers.push_back(t);
                 }
                 emit self->tags_ready(tickers);
             },
             QStringLiteral("Kalshi.fetch_series"));
}

// ── fetch_order_book ────────────────────────────────────────────────────────

static void fill_book_from_levels(pr::PredictionOrderBook& book, const QJsonArray& levels_bids,
                                  const QJsonArray& opposite_bids) {
    // `levels_bids` are this side's bids (best first). Convert directly.
    for (const auto& v : levels_bids) {
        const auto arr = v.toArray();
        if (arr.size() < 2) continue;
        book.bids.push_back({str_or_num(arr[0]), fp_to_double(arr[1])});
    }
    // Asks are synthesised from the opposite side's bids:
    // "a YES ask at $X is equivalent to a NO bid at $(1 - X)."
    for (const auto& v : opposite_bids) {
        const auto arr = v.toArray();
        if (arr.size() < 2) continue;
        const double opp_price = str_or_num(arr[0]);
        book.asks.push_back({1.0 - opp_price, fp_to_double(arr[1])});
    }
    // Asks naturally come out ascending after this inversion, because
    // Kalshi returns NO bids best-first (highest NO price), which maps
    // to the lowest YES ask first.
}

void KalshiRestClient::fetch_order_book(const QString& ticker, int depth) {
    QUrlQuery q;
    if (depth > 0) q.addQueryItem("depth", QString::number(depth));
    QUrl u(base_url() + QStringLiteral("/markets/") + ticker + QStringLiteral("/orderbook"));
    u.setQuery(q);

    const QString ticker_copy = ticker;
    QPointer<KalshiRestClient> self = this;
    get_json(u.toString(),
             [self, ticker_copy](const QJsonDocument& doc) {
                 if (!self) return;
                 const auto ob = doc.object().value("orderbook_fp").toObject();
                 const auto yes_bids = ob.value("yes_dollars").toArray();
                 const auto no_bids = ob.value("no_dollars").toArray();

                 pr::PredictionOrderBook yes_book;
                 yes_book.asset_id = ticker_copy + QStringLiteral(":yes");
                 fill_book_from_levels(yes_book, yes_bids, no_bids);

                 pr::PredictionOrderBook no_book;
                 no_book.asset_id = ticker_copy + QStringLiteral(":no");
                 fill_book_from_levels(no_book, no_bids, yes_bids);

                 emit self->order_book_ready(yes_book, no_book, ticker_copy);
             },
             QStringLiteral("Kalshi.fetch_order_book"));
}

// ── fetch_candlesticks ──────────────────────────────────────────────────────

void KalshiRestClient::fetch_candlesticks(const QString& series_ticker, const QString& ticker,
                                          int period_interval_min, qint64 start_ts, qint64 end_ts) {
    QUrlQuery q;
    q.addQueryItem("period_interval", QString::number(period_interval_min));
    if (start_ts > 0) q.addQueryItem("start_ts", QString::number(start_ts));
    if (end_ts > 0) q.addQueryItem("end_ts", QString::number(end_ts));
    QUrl u(base_url() + QStringLiteral("/series/") + series_ticker +
           QStringLiteral("/markets/") + ticker + QStringLiteral("/candlesticks"));
    u.setQuery(q);

    const QString ticker_copy = ticker;
    QPointer<KalshiRestClient> self = this;
    get_json(u.toString(),
             [self, ticker_copy](const QJsonDocument& doc) {
                 if (!self) return;
                 pr::PriceHistory h;
                 h.asset_id = ticker_copy + QStringLiteral(":yes");
                 const auto arr = doc.object().value("candlesticks").toArray();
                 h.points.reserve(arr.size());
                 for (const auto& v : arr) {
                     const auto o = v.toObject();
                     const qint64 ts = qint64(o.value("end_period_ts").toDouble()) * 1000;
                     const auto price = o.value("price").toObject();
                     // Fall back to yes_bid close if no trades happened.
                     QJsonValue close = price.value("close_dollars");
                     if (close.isNull() || close.isUndefined()) {
                         close = o.value("yes_bid").toObject().value("close_dollars");
                     }
                     const double p = str_or_num(close);
                     if (ts > 0 && p > 0) h.points.push_back({ts, p});
                 }
                 emit self->price_history_ready(h, ticker_copy);
             },
             QStringLiteral("Kalshi.fetch_candlesticks"));
}

// ── fetch_market_trades ─────────────────────────────────────────────────────

void KalshiRestClient::fetch_market_trades(const QString& ticker, int limit, const QString& cursor) {
    QUrlQuery q;
    q.addQueryItem("ticker", ticker);
    q.addQueryItem("limit", QString::number(qBound(1, limit, 1000)));
    if (!cursor.isEmpty()) q.addQueryItem("cursor", cursor);
    QUrl u(base_url() + QStringLiteral("/markets/trades"));
    u.setQuery(q);

    QPointer<KalshiRestClient> self = this;
    get_json(u.toString(),
             [self](const QJsonDocument& doc) {
                 if (!self) return;
                 QVector<pr::PredictionTrade> trades;
                 const auto arr = doc.object().value("trades").toArray();
                 trades.reserve(arr.size());
                 for (const auto& v : arr) {
                     const auto o = v.toObject();
                     pr::PredictionTrade t;
                     t.asset_id = o.value("ticker").toString() + QStringLiteral(":yes");
                     t.side = o.value("taker_side").toString().toUpper();
                     if (t.side.isEmpty()) t.side = QStringLiteral("BUY");
                     t.price = str_or_num(o.value("yes_price_dollars"));
                     if (t.price <= 0) t.price = str_or_num(o.value("price_dollars"));
                     t.size = fp_to_double(o.value("count_fp"));
                     t.ts_ms = qint64(o.value("created_time_ts").toDouble()) * 1000;
                     if (t.ts_ms <= 0) {
                         const QString iso = o.value("created_time").toString();
                         t.ts_ms = QDateTime::fromString(iso, Qt::ISODate).toMSecsSinceEpoch();
                     }
                     trades.push_back(t);
                 }
                 emit self->trades_ready(trades);
             },
             QStringLiteral("Kalshi.fetch_market_trades"));
}

} // namespace fincept::services::prediction::kalshi_ns
