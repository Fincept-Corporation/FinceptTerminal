#include "services/prediction/polymarket/PolymarketAdapter.h"

#include "core/logging/Logger.h"
#include "python/PythonRunner.h"
#include "services/polymarket/PolymarketService.h"
#include "services/polymarket/PolymarketWebSocket.h"
#include "services/prediction/PredictionCredentialStore.h"
#include "services/prediction/polymarket/PolymarketTypeMap.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QUuid>

namespace fincept::services::prediction::polymarket_ns {

namespace pm = fincept::services::polymarket;
namespace pr = fincept::services::prediction;
namespace pmap = fincept::services::prediction::polymarket_map;

PolymarketAdapter::PolymarketAdapter(QObject* parent)
    : fincept::services::prediction::PredictionExchangeAdapter(parent) {
    service_ = &pm::PolymarketService::instance();
    ws_ = &pm::PolymarketWebSocket::instance();
    wire_service();
    wire_ws();
}

PolymarketAdapter::~PolymarketAdapter() = default;

// ── Identity ────────────────────────────────────────────────────────────────

QString PolymarketAdapter::id() const { return QStringLiteral("polymarket"); }
QString PolymarketAdapter::display_name() const { return QStringLiteral("Polymarket"); }

pr::ExchangeCapabilities PolymarketAdapter::capabilities() const {
    pr::ExchangeCapabilities c;
    c.has_events = true;
    c.has_multi_outcome = true;   // neg-risk events
    c.has_orderbook_ws = true;
    c.has_trade_ws = false;
    c.has_rewards = true;
    c.has_maker_rebates = true;
    c.has_leaderboard = true;
    c.supports_limit_orders = true;
    c.supports_market_orders = true;
    c.supports_gtd = true;
    c.supports_fok = true;
    c.supports_fak = true;
    c.supports_decrease_order = false;
    c.supports_batch_orders = false;
    c.quote_currency = QStringLiteral("USDC");
    c.min_order_size = 5.0;
    c.default_tick_size = 0.01;
    c.max_requests_per_sec = 10;
    return c;
}

// ── Read endpoints ──────────────────────────────────────────────────────────

void PolymarketAdapter::list_markets(const QString& category, const QString& sort_by,
                                     int limit, int offset) {
    if (category.isEmpty() || category == QStringLiteral("ALL")) {
        service_->fetch_markets(sort_by, limit, offset, /*closed=*/false);
    } else {
        service_->fetch_markets_by_tag(category, sort_by, limit, offset);
    }
}

void PolymarketAdapter::list_events(const QString& /*category*/, const QString& sort_by,
                                    int limit, int offset) {
    service_->fetch_events(sort_by, limit, offset, /*closed=*/false);
}

void PolymarketAdapter::search(const QString& query, int limit) {
    service_->search_markets(query, limit);
}

void PolymarketAdapter::list_tags() {
    service_->fetch_tags();
}

void PolymarketAdapter::fetch_market(const pr::MarketKey& key) {
    bool ok = false;
    const int id = key.market_id.toInt(&ok);
    if (ok && id > 0) {
        service_->fetch_market_by_id(id);
    } else {
        emit error_occurred(QStringLiteral("fetch_market"),
                            QStringLiteral("Polymarket fetch_market requires numeric market id"));
    }
}

void PolymarketAdapter::fetch_event(const pr::MarketKey& key) {
    bool ok = false;
    const int id = key.event_id.toInt(&ok);
    if (ok && id > 0) {
        service_->fetch_event_by_id(id);
    } else {
        emit error_occurred(QStringLiteral("fetch_event"),
                            QStringLiteral("Polymarket fetch_event requires numeric event id"));
    }
}

void PolymarketAdapter::fetch_order_book(const QString& asset_id) {
    service_->fetch_order_book(asset_id);
}

void PolymarketAdapter::fetch_price_history(const QString& asset_id,
                                            const QString& interval, int fidelity) {
    last_history_asset_id_ = asset_id;
    service_->fetch_price_history(asset_id, interval, fidelity);
}

void PolymarketAdapter::fetch_recent_trades(const pr::MarketKey& key, int limit) {
    if (key.market_id.isEmpty()) {
        emit error_occurred(QStringLiteral("fetch_recent_trades"),
                            QStringLiteral("Polymarket trades require condition id"));
        return;
    }
    service_->fetch_trades(key.market_id, limit);
}

void PolymarketAdapter::fetch_top_holders(const pr::MarketKey& key, int limit) {
    if (key.market_id.isEmpty()) return;
    service_->fetch_top_holders(key.market_id, limit);
}

void PolymarketAdapter::fetch_leaderboard(int limit) {
    service_->fetch_leaderboard(limit);
}

// ── WebSocket ───────────────────────────────────────────────────────────────

void PolymarketAdapter::subscribe_market(const QStringList& asset_ids) {
    ws_->subscribe(asset_ids);
}

void PolymarketAdapter::unsubscribe_market(const QStringList& asset_ids) {
    ws_->unsubscribe(asset_ids);
}

bool PolymarketAdapter::is_ws_connected() const {
    return ws_->is_connected();
}

// ── Auth ────────────────────────────────────────────────────────────────────

bool PolymarketAdapter::has_credentials() const {
    return creds_.has_value() && creds_->is_valid();
}

QString PolymarketAdapter::account_label() const {
    if (!creds_) return {};
    if (!cached_wallet_address_.isEmpty()) {
        return QStringLiteral("Poly:") + cached_wallet_address_.left(6) +
               QStringLiteral("…") + cached_wallet_address_.right(4);
    }
    return QStringLiteral("Polymarket");
}

void PolymarketAdapter::reload_credentials() {
    creds_ = pr::PredictionCredentialStore::load_polymarket();
    if (creds_) {
        LOG_INFO("PolymarketAdapter", "Credentials loaded from SecureStorage");
    } else {
        LOG_INFO("PolymarketAdapter", "No Polymarket credentials in SecureStorage");
        cached_wallet_address_.clear();
    }
    emit credentials_changed();
}

void PolymarketAdapter::stub_unsupported(const QString& ctx) {
    emit error_occurred(ctx, QStringLiteral("Polymarket adapter: not supported in this call"));
}

// ── Python bridge ───────────────────────────────────────────────────────────

QJsonObject PolymarketAdapter::creds_to_json(const pr::PolymarketCredentials& c) const {
    QJsonObject o;
    o.insert("private_key", c.private_key);
    if (!c.funder_address.isEmpty()) o.insert("funder", c.funder_address);
    o.insert("signature_type", c.signature_type);
    if (!c.api_key.isEmpty()) o.insert("api_key", c.api_key);
    if (!c.api_secret.isEmpty()) o.insert("api_secret", c.api_secret);
    if (!c.api_passphrase.isEmpty()) o.insert("api_passphrase", c.api_passphrase);
    return o;
}

void PolymarketAdapter::run_py(const QString& command, const QJsonObject& payload,
                               std::function<void(const QJsonObject&)> on_ok, const QString& ctx) {
    const QString payload_str =
        QString::fromUtf8(QJsonDocument(payload).toJson(QJsonDocument::Compact));
    QPointer<PolymarketAdapter> self = this;
    fincept::python::PythonRunner::instance().run(
        QStringLiteral("prediction_polymarket.py"),
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
                    ctx,
                    QStringLiteral("Python response is not JSON: ") + perr.errorString());
                return;
            }
            const auto obj = doc.object();
            if (!obj.value("ok").toBool()) {
                emit self->error_occurred(ctx, obj.value("error").toString(
                    QStringLiteral("Python command failed")));
                return;
            }
            on_ok(obj);
        });
}

void PolymarketAdapter::ensure_api_creds(
    std::function<void(const pr::PolymarketCredentials&)> then, const QString& ctx) {
    if (!creds_ || !creds_->is_valid()) {
        emit error_occurred(ctx, QStringLiteral("No Polymarket credentials — connect an account first"));
        return;
    }
    if (!creds_->api_key.isEmpty()) {
        then(*creds_);
        return;
    }
    // Derive L2 creds via L1 EIP-712 signature and cache them.
    QPointer<PolymarketAdapter> self = this;
    run_py(QStringLiteral("derive_api_creds"), creds_to_json(*creds_),
           [self, then](const QJsonObject& resp) {
               if (!self || !self->creds_) return;
               self->creds_->api_key = resp.value("api_key").toString();
               self->creds_->api_secret = resp.value("api_secret").toString();
               self->creds_->api_passphrase = resp.value("api_passphrase").toString();
               pr::PredictionCredentialStore::save_polymarket(*self->creds_);
               LOG_INFO("PolymarketAdapter", "L2 API credentials derived and persisted");
               then(*self->creds_);
           },
           ctx);
}

// ── Authenticated endpoints ─────────────────────────────────────────────────

void PolymarketAdapter::fetch_balance() {
    const QString ctx = QStringLiteral("fetch_balance");
    QPointer<PolymarketAdapter> self = this;
    ensure_api_creds(
        [self](const pr::PolymarketCredentials& c) {
            if (!self) return;
            self->run_py(QStringLiteral("balance"), self->creds_to_json(c),
                         [self](const QJsonObject& resp) {
                             if (!self) return;
                             pr::AccountBalance b;
                             b.available = resp.value("available").toDouble();
                             b.total_value = resp.value("total_value").toDouble();
                             b.currency = resp.value("currency").toString("USDC");
                             emit self->balance_ready(b);
                         },
                         QStringLiteral("fetch_balance"));
        },
        ctx);
}

void PolymarketAdapter::fetch_positions() {
    const QString ctx = QStringLiteral("fetch_positions");
    QPointer<PolymarketAdapter> self = this;
    ensure_api_creds(
        [self](const pr::PolymarketCredentials& c) {
            if (!self) return;
            self->run_py(QStringLiteral("positions"), self->creds_to_json(c),
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
        },
        ctx);
}

void PolymarketAdapter::fetch_open_orders() {
    const QString ctx = QStringLiteral("fetch_open_orders");
    QPointer<PolymarketAdapter> self = this;
    ensure_api_creds(
        [self](const pr::PolymarketCredentials& c) {
            if (!self) return;
            self->run_py(QStringLiteral("open_orders"), self->creds_to_json(c),
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
                                 ord.created_ms = qint64(o.value("created_ms").toDouble());
                                 ord.expires_ms = qint64(o.value("expires_ms").toDouble());
                                 out.push_back(ord);
                             }
                             emit self->open_orders_ready(out);
                         },
                         QStringLiteral("fetch_open_orders"));
        },
        ctx);
}

void PolymarketAdapter::fetch_user_activity(int limit) {
    const QString ctx = QStringLiteral("fetch_user_activity");
    const int lim = limit;
    QPointer<PolymarketAdapter> self = this;
    ensure_api_creds(
        [self, lim](const pr::PolymarketCredentials& c) {
            if (!self) return;
            QJsonObject p = self->creds_to_json(c);
            p.insert("limit", lim);
            self->run_py(QStringLiteral("activity"), p,
                         [self](const QJsonObject& resp) {
                             if (!self) return;
                             emit self->account_activity_ready(
                                 resp.value("activities").toArray().toVariantList());
                         },
                         QStringLiteral("fetch_user_activity"));
        },
        ctx);
}

void PolymarketAdapter::place_order(const pr::OrderRequest& req) {
    const QString ctx = QStringLiteral("place_order");
    // Copy into a struct that can be captured by value.
    struct Req {
        QString asset_id; QString side; QString order_type;
        double price; double size; qint64 expires_ms;
        QString tick_size; bool neg_risk;
    };
    Req r{req.asset_id, req.side, req.order_type, req.price, req.size, req.expires_ms,
          req.extras.value(QStringLiteral("tick_size"), QStringLiteral("0.01")).toString(),
          req.extras.value(QStringLiteral("neg_risk"), false).toBool()};

    QPointer<PolymarketAdapter> self = this;
    ensure_api_creds(
        [self, r](const pr::PolymarketCredentials& c) {
            if (!self) return;
            QJsonObject p = self->creds_to_json(c);
            p.insert("token_id", r.asset_id);
            p.insert("price", r.price);
            p.insert("size", r.size);
            p.insert("side", r.side);
            p.insert("order_type", r.order_type.isEmpty() ? QStringLiteral("GTC") : r.order_type);
            p.insert("tick_size", r.tick_size);
            p.insert("neg_risk", r.neg_risk);
            if (r.expires_ms > 0) p.insert("expiration", qint64(r.expires_ms / 1000));

            self->run_py(QStringLiteral("place_order"), p,
                         [self](const QJsonObject& resp) {
                             if (!self) return;
                             pr::OrderResult out;
                             out.ok = resp.value("ok").toBool();
                             out.order_id = resp.value("order_id").toString();
                             out.status = resp.value("status").toString();
                             out.error_message = resp.value("error").toString();
                             emit self->order_placed(out);
                         },
                         QStringLiteral("place_order"));
        },
        ctx);
}

void PolymarketAdapter::cancel_order(const QString& order_id) {
    const QString ctx = QStringLiteral("cancel_order");
    const QString oid = order_id;
    QPointer<PolymarketAdapter> self = this;
    ensure_api_creds(
        [self, oid](const pr::PolymarketCredentials& c) {
            if (!self) return;
            QJsonObject p = self->creds_to_json(c);
            p.insert("order_id", oid);
            self->run_py(QStringLiteral("cancel_order"), p,
                         [self, oid](const QJsonObject& resp) {
                             if (!self) return;
                             emit self->order_cancelled(oid, resp.value("ok").toBool(),
                                                        resp.value("error").toString());
                         },
                         QStringLiteral("cancel_order"));
        },
        ctx);
}

void PolymarketAdapter::cancel_all_for_market(const pr::MarketKey& key, const QString& asset_id) {
    const QString ctx = QStringLiteral("cancel_all_for_market");
    const QString market_id = key.market_id;
    const QString aid = asset_id;
    QPointer<PolymarketAdapter> self = this;
    ensure_api_creds(
        [self, market_id, aid](const pr::PolymarketCredentials& c) {
            if (!self) return;
            QJsonObject p = self->creds_to_json(c);
            p.insert("market_id", market_id);
            p.insert("asset_id", aid);
            self->run_py(QStringLiteral("cancel_market"), p,
                         [self](const QJsonObject& /*resp*/) {
                             if (!self) return;
                             emit self->credentials_changed();  // triggers portfolio refresh
                         },
                         QStringLiteral("cancel_all_for_market"));
        },
        ctx);
}

// ── Hub registration ────────────────────────────────────────────────────────
// The hub producer for prediction:polymarket:* is PolymarketWebSocket — it
// registers itself from main.cpp. This method is a no-op but kept idempotent
// so screens can call it unconditionally on the active adapter.

void PolymarketAdapter::ensure_registered_with_hub() {
    if (hub_registered_) return;
    pm::PolymarketWebSocket::instance().ensure_registered_with_hub();
    hub_registered_ = true;
}

// ── Service wiring ──────────────────────────────────────────────────────────

void PolymarketAdapter::wire_service() {
    connect(service_, &pm::PolymarketService::markets_ready,
            this, &PolymarketAdapter::on_markets);
    connect(service_, &pm::PolymarketService::events_ready,
            this, &PolymarketAdapter::on_events);
    connect(service_, &pm::PolymarketService::tags_ready,
            this, &PolymarketAdapter::on_tags);
    connect(service_, &pm::PolymarketService::market_detail_ready,
            this, &PolymarketAdapter::on_market_detail);
    connect(service_, &pm::PolymarketService::event_detail_ready,
            this, &PolymarketAdapter::on_event_detail);
    connect(service_, &pm::PolymarketService::order_book_ready,
            this, &PolymarketAdapter::on_order_book);
    connect(service_, &pm::PolymarketService::price_history_ready,
            this, &PolymarketAdapter::on_price_history);
    connect(service_, &pm::PolymarketService::trades_ready,
            this, &PolymarketAdapter::on_trades);
    connect(service_, &pm::PolymarketService::request_error,
            this, &PolymarketAdapter::on_service_error);

    // Search results forward unchanged (converted to prediction types).
    connect(service_, &pm::PolymarketService::search_results_ready,
            this, [this](const QVector<pm::Market>& markets, const QVector<pm::Event>& events) {
                emit search_results_ready(pmap::to_prediction(markets),
                                          pmap::to_prediction(events));
            });

    // Leaderboard / holders forward as QVariantList — keep exchange-shaped
    // so the leaderboard widget can choose to upgrade to a typed model later.
    connect(service_, &pm::PolymarketService::leaderboard_ready,
            this, [this](const QVector<pm::LeaderboardEntry>& entries) {
                QVariantList out;
                out.reserve(entries.size());
                for (const auto& e : entries) {
                    QVariantMap m;
                    m.insert(QStringLiteral("rank"), e.rank);
                    m.insert(QStringLiteral("address"), e.address);
                    m.insert(QStringLiteral("display_name"), e.display_name);
                    m.insert(QStringLiteral("profile_image"), e.profile_image);
                    m.insert(QStringLiteral("pnl"), e.pnl);
                    m.insert(QStringLiteral("volume"), e.volume);
                    m.insert(QStringLiteral("num_trades"), e.num_trades);
                    out.push_back(m);
                }
                emit leaderboard_ready(out);
            });
    connect(service_, &pm::PolymarketService::top_holders_ready,
            this, [this](const QVector<pm::TopHolder>& holders) {
                QVariantList out;
                out.reserve(holders.size());
                for (const auto& h : holders) {
                    QVariantMap m;
                    m.insert(QStringLiteral("rank"), h.rank);
                    m.insert(QStringLiteral("address"), h.address);
                    m.insert(QStringLiteral("display_name"), h.display_name);
                    m.insert(QStringLiteral("position_size"), h.position_size);
                    m.insert(QStringLiteral("entry_price"), h.entry_price);
                    out.push_back(m);
                }
                emit top_holders_ready(out);
            });
}

void PolymarketAdapter::wire_ws() {
    connect(ws_, &pm::PolymarketWebSocket::price_updated,
            this, &PolymarketAdapter::on_ws_price);
    connect(ws_, &pm::PolymarketWebSocket::orderbook_updated,
            this, &PolymarketAdapter::on_ws_orderbook);
    connect(ws_, &pm::PolymarketWebSocket::connection_status_changed,
            this, &PolymarketAdapter::on_ws_status);
}

// ── Service slot handlers ───────────────────────────────────────────────────

void PolymarketAdapter::on_markets(const QVector<pm::Market>& markets) {
    emit markets_ready(pmap::to_prediction(markets));
}

void PolymarketAdapter::on_events(const QVector<pm::Event>& events) {
    emit events_ready(pmap::to_prediction(events));
}

void PolymarketAdapter::on_tags(const QVector<pm::Tag>& tags) {
    QStringList out;
    out.reserve(tags.size());
    for (const auto& t : tags) out.push_back(t.label);
    emit tags_ready(out);
}

void PolymarketAdapter::on_market_detail(const pm::Market& market) {
    emit market_detail_ready(pmap::to_prediction(market));
}

void PolymarketAdapter::on_event_detail(const pm::Event& event) {
    emit event_detail_ready(pmap::to_prediction(event));
}

void PolymarketAdapter::on_order_book(const pm::OrderBook& book) {
    emit order_book_ready(pmap::to_prediction(book));
}

void PolymarketAdapter::on_price_history(const pm::PriceHistory& history) {
    emit price_history_ready(pmap::to_prediction(history, last_history_asset_id_));
}

void PolymarketAdapter::on_trades(const QVector<pm::Trade>& trades) {
    emit recent_trades_ready(pmap::to_prediction(trades));
}

void PolymarketAdapter::on_service_error(const QString& ctx, const QString& msg) {
    emit error_occurred(ctx, msg);
}

// ── WebSocket slot handlers ─────────────────────────────────────────────────

void PolymarketAdapter::on_ws_price(const QString& asset_id, double price) {
    if (asset_id.isEmpty()) return;
    emit ws_price_updated(asset_id, price);
    // Hub publish is done by PolymarketWebSocket (the Producer).
}

void PolymarketAdapter::on_ws_orderbook(const QString& asset_id, const pm::OrderBook& book) {
    if (asset_id.isEmpty()) return;
    auto predicted = pmap::to_prediction(book);
    predicted.asset_id = asset_id;
    emit ws_orderbook_updated(asset_id, predicted);
}

void PolymarketAdapter::on_ws_status(bool connected) {
    emit ws_connection_changed(connected);
}

} // namespace fincept::services::prediction::polymarket_ns
