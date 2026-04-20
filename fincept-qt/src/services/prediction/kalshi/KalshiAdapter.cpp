#include "services/prediction/kalshi/KalshiAdapter.h"

#include "core/logging/Logger.h"
#include "python/PythonRunner.h"
#include "services/prediction/kalshi/KalshiRestClient.h"
#include "services/prediction/kalshi/KalshiWsClient.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QUuid>

#include <cmath>

namespace fincept::services::prediction::kalshi_ns {

namespace pr = fincept::services::prediction;

// ── Construction ────────────────────────────────────────────────────────────

KalshiAdapter::KalshiAdapter(QObject* parent)
    : fincept::services::prediction::PredictionExchangeAdapter(parent),
      rest_(std::make_unique<KalshiRestClient>(this)),
      ws_(std::make_unique<KalshiWsClient>(this)) {
    wire();
}

KalshiAdapter::~KalshiAdapter() = default;

// ── Identity ────────────────────────────────────────────────────────────────

QString KalshiAdapter::id() const { return QStringLiteral("kalshi"); }
QString KalshiAdapter::display_name() const { return QStringLiteral("Kalshi"); }

pr::ExchangeCapabilities KalshiAdapter::capabilities() const {
    pr::ExchangeCapabilities c;
    c.has_events = true;
    c.has_multi_outcome = false;    // binary yes/no only
    c.has_orderbook_ws = true;
    c.has_trade_ws = true;
    c.has_rewards = false;
    c.has_maker_rebates = false;
    c.has_leaderboard = false;
    c.supports_limit_orders = true;
    c.supports_market_orders = true;
    c.supports_gtd = true;
    c.supports_fok = true;
    c.supports_fak = true;
    c.supports_decrease_order = true;
    c.supports_batch_orders = true;
    c.quote_currency = QStringLiteral("USD");
    c.min_order_size = 1.0;          // 1 contract
    c.default_tick_size = 0.01;      // 1 cent
    c.max_requests_per_sec = 2;       // conservative for standard tier
    return c;
}

// ── Asset ID helpers ────────────────────────────────────────────────────────

std::pair<QString, QString> KalshiAdapter::split_asset_id(const QString& asset_id) {
    const int colon = asset_id.lastIndexOf(':');
    if (colon <= 0) return {};
    return {asset_id.left(colon), asset_id.mid(colon + 1)};
}

// ── Read endpoints ──────────────────────────────────────────────────────────

void KalshiAdapter::list_markets(const QString& category, const QString& /*sort_by*/,
                                 int limit, int /*offset*/) {
    // Kalshi uses cursor-based pagination. For MVP we serve the first page;
    // pagination wiring is Phase 8 polish.
    const QString series = (category.isEmpty() || category == QStringLiteral("ALL")) ? QString() : category;
    rest_->fetch_markets(QStringLiteral("open"), QString(), series, QString(), limit, QString());
}

void KalshiAdapter::list_events(const QString& category, const QString& /*sort_by*/,
                                int limit, int /*offset*/) {
    const QString series = (category.isEmpty() || category == QStringLiteral("ALL")) ? QString() : category;
    rest_->fetch_events(QStringLiteral("open"), series, /*with_nested_markets=*/true, limit, QString());
}

void KalshiAdapter::search(const QString& query, int limit) {
    // Kalshi doesn't offer a free-text search endpoint. As a best-effort,
    // treat the query as a series_ticker filter. If the user enters a
    // full ticker like "KXPRES" it'll pull matching markets. For richer
    // search we'd need to list + client-side filter, which is Phase 8.
    search_pending_ = true;
    rest_->fetch_markets(QStringLiteral("open"), QString(), query.toUpper(), QString(), limit, QString());
}

void KalshiAdapter::list_tags() {
    rest_->fetch_series(QStringLiteral("open"));
}

void KalshiAdapter::fetch_market(const pr::MarketKey& key) {
    if (key.market_id.isEmpty()) {
        emit error_occurred(QStringLiteral("fetch_market"),
                            QStringLiteral("Kalshi fetch_market requires a ticker"));
        return;
    }
    rest_->fetch_market(key.market_id);
}

void KalshiAdapter::fetch_event(const pr::MarketKey& key) {
    if (key.event_id.isEmpty()) {
        emit error_occurred(QStringLiteral("fetch_event"),
                            QStringLiteral("Kalshi fetch_event requires an event_ticker"));
        return;
    }
    rest_->fetch_event(key.event_id);
}

void KalshiAdapter::fetch_order_book(const QString& asset_id) {
    const auto [ticker, side] = split_asset_id(asset_id);
    if (ticker.isEmpty()) {
        emit error_occurred(QStringLiteral("fetch_order_book"),
                            QStringLiteral("Kalshi asset_id must be '<ticker>:yes|no'"));
        return;
    }
    rest_->fetch_order_book(ticker, 20);
}

void KalshiAdapter::fetch_price_history(const QString& asset_id, const QString& interval, int /*fidelity*/) {
    const auto [ticker, side] = split_asset_id(asset_id);
    if (ticker.isEmpty()) {
        emit error_occurred(QStringLiteral("fetch_price_history"),
                            QStringLiteral("Kalshi asset_id must be '<ticker>:yes|no'"));
        return;
    }
    last_history_asset_id_ = asset_id;

    // Map UI interval to Kalshi's period_interval (minutes). Kalshi only
    // supports 1, 60, 1440.
    int period = 60;
    if (interval == QStringLiteral("1h") || interval == QStringLiteral("6h")) period = 60;
    else if (interval == QStringLiteral("1d")) period = 60;
    else if (interval == QStringLiteral("1w") || interval == QStringLiteral("1m") ||
             interval == QStringLiteral("max")) period = 1440;
    else if (interval == QStringLiteral("1min") || interval == QStringLiteral("5min")) period = 1;

    // Default to last 7 days if the caller didn't give us a range.
    const qint64 end = QDateTime::currentSecsSinceEpoch();
    qint64 start = end - (7 * 24 * 3600);
    if (interval == QStringLiteral("1w")) start = end - (7 * 24 * 3600);
    else if (interval == QStringLiteral("1m")) start = end - (30 * 24 * 3600);
    else if (interval == QStringLiteral("6h")) start = end - (6 * 3600);
    else if (interval == QStringLiteral("1h")) start = end - 3600;

    // Kalshi candlestick endpoint requires the series ticker. Prefer the
    // cached mapping populated from markets_ready/events_ready responses;
    // fall back to the prefix-before-first-dash heuristic for tickers we
    // haven't seen. Many Kalshi tickers have multi-segment structure (e.g.
    // KXPRES-24-DJT) so the heuristic is best-effort, not authoritative.
    QString series = series_by_market_.value(ticker);
    if (series.isEmpty()) {
        const int dash = ticker.indexOf('-');
        series = (dash > 0) ? ticker.left(dash) : ticker;
    }

    rest_->fetch_candlesticks(series, ticker, period, start, end);
}

void KalshiAdapter::fetch_recent_trades(const pr::MarketKey& key, int limit) {
    if (key.market_id.isEmpty()) {
        emit error_occurred(QStringLiteral("fetch_recent_trades"),
                            QStringLiteral("Kalshi trades require a ticker"));
        return;
    }
    rest_->fetch_market_trades(key.market_id, limit);
}

// ── WebSocket ───────────────────────────────────────────────────────────────

void KalshiAdapter::subscribe_market(const QStringList& asset_ids) {
    // Kalshi WS subscribes per market ticker, not per YES/NO asset. Dedup
    // the ticker portion of the asset IDs before forwarding.
    QStringList tickers;
    tickers.reserve(asset_ids.size());
    for (const auto& aid : asset_ids) {
        const auto [t, s] = split_asset_id(aid);
        if (!t.isEmpty() && !tickers.contains(t)) tickers.push_back(t);
    }
    ws_->subscribe(tickers);
}

void KalshiAdapter::unsubscribe_market(const QStringList& asset_ids) {
    QStringList tickers;
    tickers.reserve(asset_ids.size());
    for (const auto& aid : asset_ids) {
        const auto [t, s] = split_asset_id(aid);
        if (!t.isEmpty() && !tickers.contains(t)) tickers.push_back(t);
    }
    ws_->unsubscribe(tickers);
}

bool KalshiAdapter::is_ws_connected() const { return ws_->is_connected(); }

// ── Auth / trading (Python bridge) ──────────────────────────────────────────

bool KalshiAdapter::has_credentials() const { return creds_.is_valid(); }

QString KalshiAdapter::account_label() const {
    if (!creds_.is_valid()) return {};
    return QStringLiteral("Kalshi:") + creds_.api_key_id.left(8);
}

void KalshiAdapter::stub_unsupported(const QString& ctx) {
    emit error_occurred(ctx, QStringLiteral("Kalshi adapter: not supported in this call"));
}

// prediction_kalshi.py also exposes `settlements` and `decrease_order`
// commands. They're reachable directly via PythonRunner for scripted
// testing but intentionally not surfaced on the adapter because no UI
// currently consumes a settlements view or a decrease-order control.
// Wire an adapter method here when that UI lands.

QJsonObject KalshiAdapter::creds_to_json() const {
    QJsonObject o;
    o.insert("api_key_id", creds_.api_key_id);
    o.insert("private_key_pem", creds_.private_key_pem);
    o.insert("use_demo", creds_.use_demo);
    return o;
}

void KalshiAdapter::run_py(const QString& command, const QJsonObject& extra,
                           std::function<void(const QJsonObject&)> on_ok, const QString& ctx) {
    if (!creds_.is_valid()) {
        emit error_occurred(ctx, QStringLiteral("No Kalshi credentials — connect an account first"));
        return;
    }
    QJsonObject payload = creds_to_json();
    for (auto it = extra.begin(); it != extra.end(); ++it) {
        payload.insert(it.key(), it.value());
    }
    const QString payload_str =
        QString::fromUtf8(QJsonDocument(payload).toJson(QJsonDocument::Compact));
    QPointer<KalshiAdapter> self = this;
    fincept::python::PythonRunner::instance().run(
        QStringLiteral("prediction_kalshi.py"),
        {command, payload_str},
        [self, on_ok, ctx](fincept::python::PythonResult r) {
            if (!self) return;
            if (!r.success) {
                emit self->error_occurred(ctx, r.error.isEmpty() ? r.output : r.error);
                return;
            }
            const QString json_str = fincept::python::extract_json(r.output);
            QJsonParseError perr;
            auto doc = QJsonDocument::fromJson(json_str.toUtf8(), &perr);
            if (doc.isNull() || !doc.isObject()) {
                emit self->error_occurred(
                    ctx, QStringLiteral("Kalshi bridge: non-JSON response — ") + perr.errorString());
                return;
            }
            const auto obj = doc.object();
            if (!obj.value("ok").toBool()) {
                emit self->error_occurred(ctx, obj.value("error").toString(
                    QStringLiteral("Kalshi command failed")));
                return;
            }
            on_ok(obj);
        });
}

void KalshiAdapter::fetch_balance() {
    QPointer<KalshiAdapter> self = this;
    run_py(QStringLiteral("balance"), {},
           [self](const QJsonObject& resp) {
               if (!self) return;
               pr::AccountBalance b;
               b.available = resp.value("available").toDouble();
               b.total_value = resp.value("total_value").toDouble();
               b.currency = resp.value("currency").toString("USD");
               emit self->balance_ready(b);
           },
           QStringLiteral("fetch_balance"));
}

void KalshiAdapter::fetch_positions() {
    QPointer<KalshiAdapter> self = this;
    run_py(QStringLiteral("positions"), {},
           [self](const QJsonObject& resp) {
               if (!self) return;
               QVector<pr::PredictionPosition> out;
               const auto arr = resp.value("positions").toArray();
               out.reserve(arr.size());
               for (const auto& v : arr) {
                   const auto o = v.toObject();
                   pr::PredictionPosition p;
                   p.asset_id = o.value("asset_id").toString();
                   p.market_id = o.value("market_id").toString();
                   p.outcome = o.value("outcome").toString();
                   p.size = o.value("size").toDouble();
                   p.avg_price = o.value("avg_price").toDouble();
                   p.realized_pnl = o.value("realized_pnl").toDouble();
                   p.unrealized_pnl = o.value("unrealized_pnl").toDouble();
                   p.current_value = o.value("current_value").toDouble();
                   out.push_back(p);
               }
               emit self->positions_ready(out);
           },
           QStringLiteral("fetch_positions"));
}

void KalshiAdapter::fetch_open_orders() {
    QPointer<KalshiAdapter> self = this;
    run_py(QStringLiteral("open_orders"), {},
           [self](const QJsonObject& resp) {
               if (!self) return;
               QVector<pr::OpenOrder> out;
               const auto arr = resp.value("orders").toArray();
               out.reserve(arr.size());
               for (const auto& v : arr) {
                   const auto o = v.toObject();
                   pr::OpenOrder ord;
                   ord.order_id = o.value("order_id").toString();
                   ord.asset_id = o.value("asset_id").toString();
                   ord.market_id = o.value("market_id").toString();
                   ord.outcome = o.value("outcome").toString();
                   ord.side = o.value("side").toString();
                   ord.order_type = o.value("order_type").toString();
                   ord.price = o.value("price").toDouble();
                   ord.size = o.value("size").toDouble();
                   ord.filled = o.value("filled").toDouble();
                   ord.status = o.value("status").toString();
                   ord.expires_ms = qint64(o.value("expires_ms").toDouble());
                   out.push_back(ord);
               }
               emit self->open_orders_ready(out);
           },
           QStringLiteral("fetch_open_orders"));
}

void KalshiAdapter::fetch_user_activity(int limit) {
    QPointer<KalshiAdapter> self = this;
    QJsonObject extra;
    extra.insert("limit", limit);
    run_py(QStringLiteral("fills"), extra,
           [self](const QJsonObject& resp) {
               if (!self) return;
               emit self->account_activity_ready(resp.value("fills").toArray().toVariantList());
           },
           QStringLiteral("fetch_user_activity"));
}

void KalshiAdapter::place_order(const pr::OrderRequest& req) {
    const auto [ticker, side] = split_asset_id(req.asset_id);
    if (ticker.isEmpty()) {
        emit error_occurred(QStringLiteral("place_order"),
                            QStringLiteral("Kalshi asset_id must be '<ticker>:yes|no'"));
        return;
    }

    QJsonObject extra;
    extra.insert("ticker", ticker);
    extra.insert("side", side);
    extra.insert("action", req.side.toLower() == QStringLiteral("sell") ? QStringLiteral("sell")
                                                                         : QStringLiteral("buy"));
    extra.insert("count", int(req.size));
    extra.insert("order_type", req.order_type.isEmpty() ? QStringLiteral("limit")
                                                        : req.order_type.toLower());
    // Kalshi limit price is integer cents 1-99.
    const int price_cents = qBound(1, int(std::round(req.price * 100.0)), 99);
    if (side == QStringLiteral("yes")) extra.insert("yes_price_cents", price_cents);
    else extra.insert("no_price_cents", price_cents);
    if (req.expires_ms > 0) extra.insert("expiration_ts", qint64(req.expires_ms / 1000));
    extra.insert("client_order_id", req.client_order_id.isEmpty()
                                        ? QUuid::createUuid().toString(QUuid::WithoutBraces)
                                        : req.client_order_id);

    QPointer<KalshiAdapter> self = this;
    run_py(QStringLiteral("place_order"), extra,
           [self](const QJsonObject& resp) {
               if (!self) return;
               pr::OrderResult out;
               out.ok = resp.value("ok").toBool();
               out.order_id = resp.value("order_id").toString();
               out.status = resp.value("status").toString();
               emit self->order_placed(out);
           },
           QStringLiteral("place_order"));
}

void KalshiAdapter::cancel_order(const QString& order_id) {
    const QString oid = order_id;
    QJsonObject extra;
    extra.insert("order_id", oid);
    QPointer<KalshiAdapter> self = this;
    run_py(QStringLiteral("cancel_order"), extra,
           [self, oid](const QJsonObject& /*resp*/) {
               if (!self) return;
               emit self->order_cancelled(oid, true, QString());
           },
           QStringLiteral("cancel_order"));
}

void KalshiAdapter::cancel_all_for_market(const pr::MarketKey& key, const QString& /*asset_id*/) {
    // Fetch the ticker-filtered open orders and cancel them in one batched
    // DELETE /portfolio/orders/batched round-trip (instead of N individual
    // cancels). Empty result set is a silent success.
    if (key.market_id.isEmpty()) {
        emit error_occurred(QStringLiteral("cancel_all_for_market"),
                            QStringLiteral("Kalshi cancel-all requires a ticker"));
        return;
    }
    const QString ticker = key.market_id;
    QPointer<KalshiAdapter> self = this;
    QJsonObject extra;
    extra.insert("ticker", ticker);
    run_py(QStringLiteral("open_orders"), extra,
           [self](const QJsonObject& resp) {
               if (!self) return;
               QStringList ids;
               const auto arr = resp.value("orders").toArray();
               ids.reserve(arr.size());
               for (const auto& v : arr) {
                   const QString oid = v.toObject().value("order_id").toString();
                   if (!oid.isEmpty()) ids.push_back(oid);
               }
               if (!ids.isEmpty()) self->cancel_orders_batch(ids);
           },
           QStringLiteral("cancel_all_for_market"));
}

// ── Kalshi-specific public reads ────────────────────────────────────────────

void KalshiAdapter::fetch_exchange_status() { rest_->fetch_exchange_status(); }
void KalshiAdapter::fetch_exchange_schedule() { rest_->fetch_exchange_schedule(); }

void KalshiAdapter::fetch_batch_candles(const QStringList& tickers, int period_interval_min,
                                        qint64 start_ts, qint64 end_ts) {
    rest_->fetch_batch_candlesticks(tickers, period_interval_min, start_ts, end_ts);
}

void KalshiAdapter::fetch_series_detail(const QString& series_ticker) {
    if (series_ticker.isEmpty()) return;
    // If cached, re-emit synchronously so consumers don't have to track
    // "did I already fetch this?" — they can just call the method.
    const auto it = series_cache_.constFind(series_ticker);
    if (it != series_cache_.constEnd()) {
        QMetaObject::invokeMethod(this, [this, series_ticker]() {
            emit series_detail_ready(series_ticker, series_cache_.value(series_ticker));
        }, Qt::QueuedConnection);
        return;
    }
    rest_->fetch_series_detail(series_ticker);
}

QJsonObject KalshiAdapter::cached_series(const QString& series_ticker) const {
    return series_cache_.value(series_ticker);
}

void KalshiAdapter::fetch_historical_markets(const QString& series_ticker, int limit,
                                             const QString& cursor) {
    rest_->fetch_historical_markets(series_ticker, limit, cursor);
}

void KalshiAdapter::fetch_historical_candles(const QString& ticker, int period_interval_min,
                                             qint64 start_ts, qint64 end_ts) {
    rest_->fetch_historical_candlesticks(ticker, period_interval_min, start_ts, end_ts);
}

void KalshiAdapter::fetch_historical_trades(const QString& ticker, int limit,
                                            const QString& cursor) {
    rest_->fetch_historical_trades(ticker, limit, cursor);
}

// ── Kalshi-specific trading ─────────────────────────────────────────────────

void KalshiAdapter::amend_order(const QString& order_id, const QString& side,
                                int price_cents, const QString& client_order_id) {
    const int p = qBound(1, price_cents, 99);
    QJsonObject extra;
    extra.insert("order_id", order_id);
    if (side.toLower() == QStringLiteral("yes"))
        extra.insert("yes_price_cents", p);
    else
        extra.insert("no_price_cents", p);
    if (!client_order_id.isEmpty()) extra.insert("client_order_id", client_order_id);
    const QString oid = order_id;
    QPointer<KalshiAdapter> self = this;
    run_py(QStringLiteral("amend_order"), extra,
           [self, oid](const QJsonObject& /*resp*/) {
               if (!self) return;
               emit self->order_amended(oid, true, QString());
           },
           QStringLiteral("amend_order"));
}

void KalshiAdapter::fetch_order(const QString& order_id) {
    QJsonObject extra;
    extra.insert("order_id", order_id);
    QPointer<KalshiAdapter> self = this;
    run_py(QStringLiteral("get_order"), extra,
           [self](const QJsonObject& resp) {
               if (!self) return;
               emit self->single_order_ready(resp.value("order").toObject());
           },
           QStringLiteral("fetch_order"));
}

void KalshiAdapter::cancel_orders_batch(const QStringList& order_ids) {
    if (order_ids.isEmpty()) return;
    QJsonObject extra;
    QJsonArray ids;
    for (const auto& id : order_ids) ids.append(id);
    extra.insert("order_ids", ids);
    const QStringList ids_copy = order_ids;
    QPointer<KalshiAdapter> self = this;
    run_py(QStringLiteral("cancel_orders_batch"), extra,
           [self, ids_copy](const QJsonObject& /*resp*/) {
               if (!self) return;
               emit self->orders_batch_cancelled(ids_copy, true, QString());
           },
           QStringLiteral("cancel_orders_batch"));
}

// ── Credentials plumbing ────────────────────────────────────────────────────

void KalshiAdapter::set_credentials(const KalshiCredentials& creds) {
    creds_ = creds;
    ws_->set_credentials(creds);
    rest_->set_demo_mode(creds.use_demo);
    emit credentials_changed();
}

// ── Hub registration ────────────────────────────────────────────────────────

void KalshiAdapter::ensure_registered_with_hub() {
    if (hub_registered_) return;
    ws_->ensure_registered_with_hub();
    hub_registered_ = true;
}

// ── Signal wiring ───────────────────────────────────────────────────────────

void KalshiAdapter::wire() {
    // REST → adapter signal forwarding. Also harvests market→series
    // mappings so fetch_price_history can hit the correct endpoint.
    connect(rest_.get(), &KalshiRestClient::markets_ready, this,
            [this](const QVector<pr::PredictionMarket>& markets, const QString& /*cursor*/) {
                for (const auto& m : markets) {
                    const QString s = m.extras.value(QStringLiteral("series_ticker")).toString();
                    if (!s.isEmpty()) series_by_market_.insert(m.key.market_id, s);
                }
                if (search_pending_) {
                    search_pending_ = false;
                    emit search_results_ready(markets, {});
                } else {
                    emit markets_ready(markets);
                }
            });
    connect(rest_.get(), &KalshiRestClient::events_ready, this,
            [this](const QVector<pr::PredictionEvent>& events, const QString& /*cursor*/) {
                for (const auto& e : events) {
                    for (const auto& m : e.markets) {
                        const QString s = m.extras.value(QStringLiteral("series_ticker")).toString();
                        if (!s.isEmpty()) series_by_market_.insert(m.key.market_id, s);
                    }
                }
                emit events_ready(events);
            });
    connect(rest_.get(), &KalshiRestClient::market_detail_ready, this,
            [this](const pr::PredictionMarket& m) {
                const QString s = m.extras.value(QStringLiteral("series_ticker")).toString();
                if (!s.isEmpty()) series_by_market_.insert(m.key.market_id, s);
                emit market_detail_ready(m);
            });
    connect(rest_.get(), &KalshiRestClient::event_detail_ready, this,
            [this](const pr::PredictionEvent& e) {
                for (const auto& m : e.markets) {
                    const QString s = m.extras.value(QStringLiteral("series_ticker")).toString();
                    if (!s.isEmpty()) series_by_market_.insert(m.key.market_id, s);
                }
                emit event_detail_ready(e);
            });
    connect(rest_.get(), &KalshiRestClient::tags_ready,
            this, &KalshiAdapter::tags_ready);
    connect(rest_.get(), &KalshiRestClient::trades_ready,
            this, &KalshiAdapter::recent_trades_ready);
    connect(rest_.get(), &KalshiRestClient::request_error,
            this, &KalshiAdapter::error_occurred);

    // Kalshi REST returns two books (yes + no) in a single response.
    // We emit both books as order_book_ready events. Consumers should
    // filter on asset_id if they only want one side.
    connect(rest_.get(), &KalshiRestClient::order_book_ready, this,
            [this](const pr::PredictionOrderBook& yes_book,
                   const pr::PredictionOrderBook& no_book, const QString& /*ticker*/) {
                emit order_book_ready(yes_book);
                emit order_book_ready(no_book);
            });

    connect(rest_.get(), &KalshiRestClient::price_history_ready, this,
            [this](const pr::PriceHistory& hist, const QString& /*ticker*/) {
                // Retag with the originally-requested asset id (could be :yes or :no).
                pr::PriceHistory out = hist;
                if (!last_history_asset_id_.isEmpty()) out.asset_id = last_history_asset_id_;
                emit price_history_ready(out);
            });

    // Kalshi-specific REST signal forwards.
    connect(rest_.get(), &KalshiRestClient::exchange_status_ready,
            this, &KalshiAdapter::exchange_status_ready);
    connect(rest_.get(), &KalshiRestClient::exchange_schedule_ready,
            this, &KalshiAdapter::exchange_schedule_ready);
    connect(rest_.get(), &KalshiRestClient::batch_candlesticks_ready,
            this, &KalshiAdapter::batch_candles_ready);
    connect(rest_.get(), &KalshiRestClient::historical_markets_ready,
            this, &KalshiAdapter::historical_markets_ready);
    connect(rest_.get(), &KalshiRestClient::historical_trades_ready,
            this, &KalshiAdapter::historical_trades_ready);
    connect(rest_.get(), &KalshiRestClient::historical_candlesticks_ready,
            this, &KalshiAdapter::historical_candles_ready);

    // Series detail — cache before emitting so sync consumers can read.
    connect(rest_.get(), &KalshiRestClient::series_detail_ready, this,
            [this](const QJsonObject& series) {
                const QString t = series.value(QStringLiteral("ticker")).toString();
                if (!t.isEmpty()) series_cache_.insert(t, series);
                emit series_detail_ready(t, series);
            });

    // WebSocket → adapter.
    connect(ws_.get(), &KalshiWsClient::price_updated,
            this, &KalshiAdapter::ws_price_updated);
    connect(ws_.get(), &KalshiWsClient::orderbook_updated,
            this, &KalshiAdapter::ws_orderbook_updated);
    connect(ws_.get(), &KalshiWsClient::trade_received,
            this, &KalshiAdapter::ws_trade_received);
    connect(ws_.get(), &KalshiWsClient::market_lifecycle_changed,
            this, &KalshiAdapter::ws_market_lifecycle_changed);
    connect(ws_.get(), &KalshiWsClient::connection_status_changed,
            this, &KalshiAdapter::ws_connection_changed);
}

} // namespace fincept::services::prediction::kalshi_ns
