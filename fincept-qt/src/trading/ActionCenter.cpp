// ActionCenter.cpp — semi-auto order approval implementation (Phase 3 §2).
#include "trading/ActionCenter.h"

#include "algo_engine/AlgoEngine.h"

#include "core/logging/Logger.h"
#include "storage/repositories/SettingsRepository.h"
#include "storage/sqlite/Database.h"
#include "trading/AccountManager.h"
#include "trading/BrokerRegistry.h"
#include "trading/OrderThreadPool.h"
#include "trading/OrderValidator.h"
#include "trading/TradingEvents.h"
#include "trading/UnifiedTrading.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMetaObject>
#include <QPointer>
#include <QSet>
#include <QSqlQuery>
#include <QStringList>
#include <QThreadPool>
#include <QTimer>
#include <QUuid>
#include <QtConcurrent/QtConcurrent>

#include <algorithm>
#include <cmath>

namespace fincept::trading {

namespace {
constexpr const char* kLog = "ActionCenter";

// Dedicated single-thread pool for approval execution. One thread keeps approvals
// strictly FIFO — approve_all_pending() must send the queue in the order it was
// read, and two orders for the same symbol must never race each other to the
// broker — while taking the blocking broker round-trip off the GUI thread (P1).
// Deliberately leaked: the pool must outlive every singleton it calls into, and a
// static QThreadPool would block process exit on an in-flight broker call.
// Kept separate from trading::order_thread_pool() (4 threads) because approvals
// must stay strictly FIFO on ONE thread. The thread never expires, so
// BrokerHttp's per-thread QNetworkAccessManager keeps its TLS connections warm
// between approvals instead of re-handshaking after every idle period.
QThreadPool& action_center_exec_pool() {
    static QThreadPool* const pool = []() {
        auto* p = new QThreadPool();
        p->setMaxThreadCount(1);
        p->setExpiryTimeout(-1);
        return p;
    }();
    return *pool;
}

QString iso(const QDateTime& dt) {
    return dt.isValid() ? dt.toString(Qt::ISODate) : QString();
}
QDateTime from_iso(const QString& s) {
    return s.isEmpty() ? QDateTime() : QDateTime::fromString(s, Qt::ISODate);
}

// Expiry sweep cadence. The approval window is minutes long; a 5 s sweep keeps the
// auto-reject within a few seconds of the deadline (approve_order() enforces the
// exact deadline regardless). The timer only runs while something is pending.
constexpr int kAcExpirySweepMs = 5000;
constexpr const char* kAcTtlKey = "action_center.pending_ttl_minutes";
constexpr const char* kAcOneClickPrefix = "action_center.one_click.";

// Cached approval-window length (0 = not loaded yet). map_row() needs it for
// every legacy row, and get_stats() maps up to 100k rows — one settings query
// per row would be absurd. set_pending_ttl_minutes() keeps it coherent.
std::atomic<int> g_ac_ttl_minutes{0};

int ac_clamp_ttl(int minutes) {
    return std::clamp(minutes, ActionCenter::kMinPendingTtlMinutes, ActionCenter::kMaxPendingTtlMinutes);
}

// End of a pending order's approval window: the stamped order_data["expires_at"]
// when present, else created_at + the configured TTL (rows queued before the
// stamp existed). Invalid when neither is readable.
QDateTime ac_compute_expiry(const PendingOrder& po) {
    return ActionCenter::expiry_for(po.order_data, po.created_at, ActionCenter::pending_ttl_minutes());
}

QString ac_expiry_reason(const PendingOrder& po) {
    const qint64 window_min = (po.created_at.isValid() && po.expires_at.isValid())
                                  ? std::max<qint64>(1, po.created_at.secsTo(po.expires_at) / 60)
                                  : ActionCenter::pending_ttl_minutes();
    const QString queued = po.created_at.isValid() ? po.created_at.toLocalTime().toString("HH:mm:ss")
                                                   : QStringLiteral("unknown time");
    return QString("Expired: not approved within the %1-minute approval window (queued %2) — auto-rejected, "
                   "nothing was sent. The market has moved since; re-place the order if you still want the trade.")
        .arg(window_min)
        .arg(queued);
}
} // namespace

ActionCenter& ActionCenter::instance() {
    static ActionCenter s;
    return s;
}

ActionCenter::ActionCenter() : QObject(nullptr) {
    // Created BEFORE any thread move so it travels with us (a child cannot be
    // created for a parent living on another thread).
    expiry_timer_ = new QTimer(this);
    expiry_timer_->setInterval(kAcExpirySweepMs);
    connect(expiry_timer_, &QTimer::timeout, this, &ActionCenter::on_expiry_tick);
    // The singleton can be first touched from a worker (MCP tool, workflow node,
    // order pool). Re-home it on the application thread so its timer and queued
    // signal deliveries run on a live event loop (same pattern as RiskManager).
    if (auto* app = QCoreApplication::instance(); app && thread() != app->thread())
        moveToThread(app->thread());
    // Orders left in the queue by a previous session are swept as soon as the
    // event loop runs.
    ensure_expiry_sweep();
}

// ── Expiry ─────────────────────────────────────────────────────────────────

int ActionCenter::pending_ttl_minutes() {
    const int cached = g_ac_ttl_minutes.load();
    if (cached > 0)
        return cached;
    int minutes = kDefaultPendingTtlMinutes;
    auto r = SettingsRepository::instance().get(QString::fromLatin1(kAcTtlKey),
                                                QString::number(kDefaultPendingTtlMinutes));
    if (r.is_ok()) {
        bool ok = false;
        const int v = r.value().trimmed().toInt(&ok);
        if (ok && v > 0)
            minutes = ac_clamp_ttl(v);
    } else {
        // Do not cache a read failure — fall back to the (short) default and retry next time.
        LOG_WARN(kLog, QString("Approval-window setting unreadable, using %1 min: %2")
                           .arg(kDefaultPendingTtlMinutes)
                           .arg(QString::fromStdString(r.error())));
        return kDefaultPendingTtlMinutes;
    }
    g_ac_ttl_minutes.store(minutes);
    return minutes;
}

void ActionCenter::set_pending_ttl_minutes(int minutes) {
    const int v = ac_clamp_ttl(minutes);
    auto r = SettingsRepository::instance().set(QString::fromLatin1(kAcTtlKey), QString::number(v), "action_center");
    if (r.is_err()) {
        LOG_ERROR(kLog, QString("Saving the approval window failed: %1").arg(QString::fromStdString(r.error())));
        return;
    }
    g_ac_ttl_minutes.store(v);
    LOG_INFO(kLog, QString("Pending-order approval window set to %1 min").arg(v));
}

qint64 ActionCenter::secs_until_expiry(const PendingOrder& po) {
    if (!po.expires_at.isValid())
        return 0;
    return QDateTime::currentDateTime().secsTo(po.expires_at);
}

int ActionCenter::expire_stale_orders() {
    int expired = 0;
    QSet<QString> touched;
    const auto pending = get_pending_orders();
    for (const auto& po : pending) {
        if (!is_expired(po))
            continue;
        const QString reason = ac_expiry_reason(po);
        // Conditional on 'pending': an approval that claimed the row first wins.
        auto r = Database::instance().execute("UPDATE pending_orders SET status = 'rejected', rejected_at = ?, "
                                              "rejection_reason = ? WHERE id = ? AND status = 'pending'",
                                              {iso(QDateTime::currentDateTime()), reason, po.id});
        if (r.is_err()) {
            LOG_ERROR(kLog, QString("Auto-expiry update failed for %1: %2")
                                .arg(po.id, QString::fromStdString(r.error())));
            continue;
        }
        if (r.value().numRowsAffected() != 1)
            continue;
        ++expired;
        touched.insert(po.account_id);
        LOG_INFO(kLog, QString("Auto-rejected expired pending order %1 (%2 %3 %4)")
                           .arg(po.id, po.action, po.quantity, po.symbol));
        emit order_rejected(po.id, reason);
    }
    for (const auto& acct : touched)
        emit stats_updated(acct);
    return expired;
}

void ActionCenter::ensure_expiry_sweep() {
    // Always hop to this object's (GUI) thread: QTimer::start() must run there.
    QMetaObject::invokeMethod(
        this,
        [this]() {
            if (expiry_timer_ && !expiry_timer_->isActive())
                expiry_timer_->start();
            on_expiry_tick();
        },
        Qt::QueuedConnection);
}

void ActionCenter::on_expiry_tick() {
    expire_stale_orders();
    // Nothing left to watch — stop until the next queue_order().
    if (expiry_timer_ && get_pending_orders().isEmpty())
        expiry_timer_->stop();
}

// ── One-click trading ──────────────────────────────────────────────────────

bool ActionCenter::one_click_enabled(const QString& account_id) const {
    if (account_id.isEmpty())
        return false;
    auto r = SettingsRepository::instance().get(QString::fromLatin1(kAcOneClickPrefix) + account_id, "0");
    if (r.is_err()) {
        // Fail closed: an unreadable opt-in means the confirmation dialog shows.
        LOG_WARN(kLog, QString("One-click setting unreadable for %1 — confirmation stays on: %2")
                           .arg(account_id, QString::fromStdString(r.error())));
        return false;
    }
    return r.value().trimmed() == QLatin1String("1");
}

void ActionCenter::set_one_click_enabled(const QString& account_id, bool enabled) {
    if (account_id.isEmpty())
        return;
    auto r = SettingsRepository::instance().set(QString::fromLatin1(kAcOneClickPrefix) + account_id,
                                                enabled ? QStringLiteral("1") : QStringLiteral("0"), "action_center");
    if (r.is_err()) {
        LOG_ERROR(kLog, QString("Saving one-click setting for %1 failed: %2")
                            .arg(account_id, QString::fromStdString(r.error())));
        return;
    }
    LOG_INFO(kLog, QString("One-click trading %1 for account %2").arg(enabled ? "ENABLED" : "disabled", account_id));
    emit one_click_changed(account_id, enabled);
}

// ── Kill switch ────────────────────────────────────────────────────────────

QStringList ActionCenter::kill_switch_target_labels() const {
    QStringList out;
    for (const auto& a : AccountManager::instance().list_accounts()) {
        if (!a.is_active || a.trading_mode != QLatin1String("live"))
            continue;
        out << QString("%1  [%2]").arg(a.display_name.isEmpty() ? a.account_id : a.display_name, a.broker_id);
    }
    return out;
}

bool ActionCenter::run_kill_switch() {
    bool expected = false;
    if (!kill_switch_running_.compare_exchange_strong(expected, true)) {
        LOG_WARN(kLog, "Kill switch already running — ignoring a second trigger");
        return false;
    }
    LOG_WARN(kLog, "KILL SWITCH engaged: rejecting queued orders, cancelling open orders and squaring off "
                   "positions on every active LIVE account");

    QVector<KillSwitchAccountResult> plan;

    // FIRST stop every LIVE algo deployment: otherwise a running strategy could
    // re-enter seconds after its account was squared off. stop_all_live() blocks
    // their orders immediately (kill flag + engine-side gate) and marks them
    // stopped so they never auto-resume. Reported as rows with broker_id "algo"
    // (skipped = no broker call was made for the row itself).
    for (const auto& s : fincept::algo::AlgoEngine::instance().stop_all_live(QStringLiteral("Kill switch engaged"))) {
        KillSwitchAccountResult r;
        r.account_id = QStringLiteral("algo:") + s.deployment_id;
        r.display_name = QStringLiteral("ALGO %1 on %2").arg(s.strategy_name, s.symbol);
        r.broker_id = QStringLiteral("algo");
        r.skipped = true;
        r.skip_reason = QStringLiteral("live algo deployment stopped before the square-off (%1%2)")
                            .arg(s.note, s.db_marked_stopped ? QString() : QStringLiteral("; DB update FAILED"));
        plan.append(r);
    }
    for (const auto& a : AccountManager::instance().list_accounts()) {
        if (!a.is_active)
            continue;
        KillSwitchAccountResult r;
        r.account_id = a.account_id;
        r.display_name = a.display_name.isEmpty() ? a.account_id : a.display_name;
        r.broker_id = a.broker_id;
        if (a.trading_mode != QLatin1String("live")) {
            r.skipped = true;
            r.skip_reason = QStringLiteral("paper account");
        }
        // Unapproved queued orders go first, on this thread, for EVERY account
        // (paper included): nothing still waiting for approval may be sent once
        // the user has hit the kill switch.
        const auto pending = get_pending_orders(a.account_id);
        for (const auto& po : pending)
            reject_order(po.id, QStringLiteral("Kill switch engaged — queued order rejected, nothing was sent"));
        r.queued_rejected = int(pending.size());
        plan.append(r);
    }

    QPointer<ActionCenter> self(this);
    (void)QtConcurrent::run(&order_thread_pool(), [self, plan]() mutable {
        for (auto& r : plan) {
            if (r.skipped)
                continue;
            try {
                // Cancel FIRST: a resting stop-loss left behind after the square-off
                // would trigger later and open a fresh position in the other direction.
                const auto c = UnifiedTrading::instance().cancel_all_orders(r.account_id);
                if (c.success && c.data.has_value()) {
                    r.orders_cancelled = int(c.data->canceled_order_ids.size());
                    r.orders_cancel_failed = int(c.data->failed.size());
                    for (const auto& f : c.data->failed)
                        r.errors << QString("cancel %1: %2").arg(f.first, f.second);
                } else {
                    r.errors << QString("cancel-all failed: %1").arg(c.error.isEmpty() ? QStringLiteral("no detail")
                                                                                        : c.error);
                }
                const auto p = UnifiedTrading::instance().close_all_positions(r.account_id);
                if (p.success && p.data.has_value()) {
                    r.positions_closed = int(p.data->closed_symbols.size());
                    r.positions_close_failed = int(p.data->failed.size());
                    for (const auto& f : p.data->failed)
                        r.errors << QString("close %1: %2").arg(f.first, f.second);
                } else {
                    r.errors << QString("square-off failed: %1").arg(p.error.isEmpty() ? QStringLiteral("no detail")
                                                                                       : p.error);
                }
            } catch (const std::exception& e) {
                r.errors << QString("unexpected error: %1 — check the broker terminal").arg(QString::fromUtf8(e.what()));
            } catch (...) {
                r.errors << QStringLiteral("unexpected error — check the broker terminal");
            }
            LOG_WARN(kLog, QString("Kill switch %1 (%2): cancelled %3 (%4 failed), closed %5 (%6 failed)%7")
                               .arg(r.display_name, r.broker_id)
                               .arg(r.orders_cancelled)
                               .arg(r.orders_cancel_failed)
                               .arg(r.positions_closed)
                               .arg(r.positions_close_failed)
                               .arg(r.errors.isEmpty() ? QString() : " — " + r.errors.join("; ")));
        }
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self,
            [self, plan]() {
                if (!self)
                    return;
                self->kill_switch_running_ = false;
                emit self->kill_switch_finished(plan);
            },
            Qt::QueuedConnection);
    });
    return true;
}

const QSet<QString>& ActionCenter::immediate_ops() {
    // Risk-management + read-only ops always execute immediately, never queued.
    // Mirrors OpenAlgo order_router_service.py IMMEDIATE_EXECUTION_OPERATIONS.
    static const QSet<QString> ops = {
        "closeallpositions", "closeposition",  "cancelorder",  "cancelallorder", "modifyorder", "orderstatus",
        "orderbook",         "tradebook",      "positions",    "holdings",       "funds",       "openposition",
        "modifygttorder",    "cancelgttorder", "gttorderbook",
    };
    return ops;
}

// ── Route decision ─────────────────────────────────────────────────────────

bool ActionCenter::should_queue(const QString& account_id, const QString& order_type) const {
    if (immediate_ops().contains(order_type.toLower()))
        return false;
    return get_order_mode(account_id) == OrderMode::SemiAuto;
}

// ── Mode (persisted in settings) ───────────────────────────────────────────

void ActionCenter::set_order_mode(const QString& account_id, OrderMode mode) {
    const QString key = "action_center.mode." + account_id;
    SettingsRepository::instance().set(key, order_mode_str(mode), "action_center");
    LOG_INFO(kLog, QString("Order mode for %1 set to %2").arg(account_id, order_mode_str(mode)));
    emit stats_updated(account_id);
}

OrderMode ActionCenter::get_order_mode(const QString& account_id) const {
    const QString key = "action_center.mode." + account_id;
    auto r = SettingsRepository::instance().get(key, "auto");
    if (r.is_err()) {
        // Fail closed. should_queue() derives straight from this, so reporting
        // the "auto" default on a read error would send a SemiAuto account's
        // orders to the broker instead of the approval queue — silently
        // defeating a risk control the user explicitly turned on.
        LOG_WARN(kLog, QString("settings read failed for '%1' — treating the account as SemiAuto (approval "
                               "required) until the read succeeds: %2")
                           .arg(key, QString::fromStdString(r.error())));
        return OrderMode::SemiAuto;
    }
    return parse_order_mode(r.value());
}

// ── Serialization ──────────────────────────────────────────────────────────

QJsonObject ActionCenter::serialize_unified_order(const UnifiedOrder& order) {
    QJsonObject j;
    j["symbol"] = order.symbol;
    j["exchange"] = order.exchange;
    j["action"] = order.side == OrderSide::Buy ? "BUY" : "SELL";
    j["quantity"] = order.quantity;
    j["price"] = order.price;
    j["trigger_price"] = order.stop_price;
    j["stop_price"] = order.stop_price;
    // price_type in OpenAlgo terms (MARKET/LIMIT/SL/SL-M).
    QString pt;
    switch (order.order_type) {
        case OrderType::Market:
            pt = "MARKET";
            break;
        case OrderType::Limit:
            pt = "LIMIT";
            break;
        case OrderType::StopLoss:
            pt = "SL-M";
            break;
        case OrderType::StopLossLimit:
            pt = "SL";
            break;
    }
    j["pricetype"] = pt;
    j["product"] = QString::fromLatin1(product_type_str(order.product_type));
    j["validity"] = order.validity;
    j["stop_loss"] = order.stop_loss;
    j["take_profit"] = order.take_profit;
    j["amo"] = order.amo;
    j["instrument_token"] = order.instrument_token;
    return j;
}

QJsonObject ActionCenter::serialize_basket_order(const BasketOrderRequest& basket) {
    QJsonObject j;
    j["strategy"] = basket.strategy_name;
    QJsonArray orders;
    for (const auto& o : basket.orders)
        orders.append(serialize_unified_order(o));
    j["orders"] = orders;
    return j;
}

UnifiedOrder ActionCenter::deserialize_unified_order(const QJsonObject& json) {
    UnifiedOrder o;
    o.symbol = json.value("symbol").toString();
    o.exchange = json.value("exchange").toString();
    o.side = json.value("action").toString().toUpper() == "SELL" ? OrderSide::Sell : OrderSide::Buy;
    o.quantity = json.value("quantity").toDouble();
    o.price = json.value("price").toDouble();
    o.stop_price =
        json.contains("stop_price") ? json.value("stop_price").toDouble() : json.value("trigger_price").toDouble();

    const QString pt = json.value("pricetype").toString().toUpper();
    if (pt == "LIMIT")
        o.order_type = OrderType::Limit;
    else if (pt == "SL")
        o.order_type = OrderType::StopLossLimit;
    else if (pt == "SL-M")
        o.order_type = OrderType::StopLoss;
    else
        o.order_type = OrderType::Market;

    const QString prod = json.value("product").toString().toLower();
    if (prod == "delivery" || prod == "cnc")
        o.product_type = ProductType::Delivery;
    else if (prod == "margin" || prod == "nrml")
        o.product_type = ProductType::Margin;
    else if (prod == "cover_order")
        o.product_type = ProductType::CoverOrder;
    else if (prod == "bracket_order")
        o.product_type = ProductType::BracketOrder;
    else if (prod == "mtf")
        o.product_type = ProductType::MTF;
    else
        o.product_type = ProductType::Intraday;

    o.validity = json.value("validity").toString("DAY");
    o.stop_loss = json.value("stop_loss").toDouble();
    o.take_profit = json.value("take_profit").toDouble();
    o.amo = json.value("amo").toBool();
    o.instrument_token = json.value("instrument_token").toString();
    return o;
}

// ── Display fields ─────────────────────────────────────────────────────────

void ActionCenter::populate_display_fields(PendingOrder& po) {
    const QJsonObject& d = po.order_data;
    po.strategy = d.value("strategy").toString("N/A");

    auto qty_str = [](const QJsonValue& v) -> QString {
        if (v.isDouble()) {
            double q = v.toDouble();
            // show ints without trailing .0
            return q == std::floor(q) ? QString::number((qint64)q) : QString::number(q);
        }
        return v.toString();
    };

    if (po.order_type == "basketorder") {
        const QJsonArray orders = d.value("orders").toArray();
        po.symbol = QString("Basket (%1 orders)").arg(orders.size());
        po.exchange = orders.size() > 1
                          ? "Multiple"
                          : (orders.isEmpty() ? "" : orders.first().toObject().value("exchange").toString());
        po.action = orders.size() > 1 ? "Multiple"
                                      : (orders.isEmpty() ? "" : orders.first().toObject().value("action").toString());
        qint64 total = 0;
        for (const auto& v : orders)
            total += (qint64)v.toObject().value("quantity").toDouble();
        po.quantity = QString::number(total);
        po.price_type = orders.size() > 1
                            ? "Multiple"
                            : (orders.isEmpty() ? "" : orders.first().toObject().value("pricetype").toString("MARKET"));
        return;
    }

    if (po.order_type == "splitorder") {
        po.symbol = d.value("symbol").toString();
        po.exchange = d.value("exchange").toString();
        po.action = d.value("action").toString();
        po.quantity = QString("%1 (split: %2)").arg(qty_str(d.value("quantity")), qty_str(d.value("splitsize")));
        po.price_type = d.value("pricetype").toString("MARKET");
        return;
    }

    // placeorder / smartorder / placegttorder default.
    po.symbol = d.value("symbol").toString();
    po.exchange = d.value("exchange").toString();
    po.action = d.value("action").toString();
    po.quantity = qty_str(d.value("quantity"));
    po.price_type = d.value("pricetype").toString(d.value("price_type").toString("MARKET"));
}

// ── Persistence ────────────────────────────────────────────────────────────

QString ActionCenter::queue_order(const QString& account_id, const QString& order_type, const QJsonObject& order_data) {
    PendingOrder po;
    po.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    po.account_id = account_id;
    po.order_type = order_type;
    // Strip credential-like keys defensively.
    QJsonObject clean = order_data;
    clean.remove("apikey");
    clean.remove("api_key");
    clean.remove("api_secret");
    clean.remove("access_token");
    po.status = "pending";
    po.created_at = QDateTime::currentDateTime();
    // Stamp the end of the approval window into the row itself so a later change
    // of the TTL setting never silently extends an order already in the queue. A
    // caller may ask for a SHORTER window via its own "expires_at"; never a longer one.
    QDateTime expiry = po.created_at.addSecs(static_cast<qint64>(pending_ttl_minutes()) * 60);
    const QDateTime caller_expiry =
        QDateTime::fromString(clean.value(QStringLiteral("expires_at")).toString(), Qt::ISODateWithMs);
    if (caller_expiry.isValid() && caller_expiry < expiry)
        expiry = caller_expiry;
    clean["expires_at"] = expiry.toUTC().toString(Qt::ISODateWithMs);
    po.order_data = clean;
    po.expires_at = expiry;
    populate_display_fields(po);

    const QString json_str = QString::fromUtf8(QJsonDocument(po.order_data).toJson(QJsonDocument::Compact));
    auto r = Database::instance().execute(
        "INSERT INTO pending_orders (id, account_id, order_type, order_data, status, created_at) "
        "VALUES (?, ?, ?, ?, ?, ?)",
        {po.id, po.account_id, po.order_type, json_str, po.status, iso(po.created_at)});
    if (r.is_err()) {
        LOG_ERROR(kLog, QString("queue_order failed: %1").arg(QString::fromStdString(r.error())));
        return {};
    }

    LOG_INFO(kLog, QString("Queued %1 order %2 for account %3 (approval window ends %4)")
                       .arg(order_type, po.id, account_id, expiry.toLocalTime().toString("HH:mm:ss")));
    emit pending_order_created(po);
    emit stats_updated(account_id);
    ensure_expiry_sweep();
    return po.id;
}

PendingOrder ActionCenter::map_row(QSqlQuery& q) {
    PendingOrder po;
    po.id = q.value("id").toString();
    po.account_id = q.value("account_id").toString();
    po.order_type = q.value("order_type").toString();
    po.order_data = QJsonDocument::fromJson(q.value("order_data").toString().toUtf8()).object();
    po.status = q.value("status").toString();
    po.created_at = from_iso(q.value("created_at").toString());
    po.approved_at = from_iso(q.value("approved_at").toString());
    po.rejected_at = from_iso(q.value("rejected_at").toString());
    po.rejection_reason = q.value("rejection_reason").toString();
    po.broker_order_id = q.value("broker_order_id").toString();
    po.expires_at = ac_compute_expiry(po);
    populate_display_fields(po);
    return po;
}

std::optional<PendingOrder> ActionCenter::get_order(const QString& pending_id) const {
    auto r = Database::instance().execute("SELECT * FROM pending_orders WHERE id = ?", {pending_id});
    if (r.is_err())
        return std::nullopt;
    auto& q = r.value();
    if (!q.next())
        return std::nullopt;
    return map_row(q);
}

QVector<PendingOrder> ActionCenter::get_pending_orders(const QString& account_id) const {
    QVector<PendingOrder> out;
    QString sql = "SELECT * FROM pending_orders WHERE status = 'pending'";
    QVariantList params;
    if (!account_id.isEmpty()) {
        sql += " AND account_id = ?";
        params << account_id;
    }
    sql += " ORDER BY created_at DESC";
    auto r = Database::instance().execute(sql, params);
    if (r.is_err())
        return out;
    auto& q = r.value();
    while (q.next())
        out.append(map_row(q));
    return out;
}

QVector<PendingOrder> ActionCenter::get_all_orders(const QString& account_id, int limit) const {
    QVector<PendingOrder> out;
    QString sql = "SELECT * FROM pending_orders";
    QVariantList params;
    if (!account_id.isEmpty()) {
        sql += " WHERE account_id = ?";
        params << account_id;
    }
    sql += " ORDER BY created_at DESC LIMIT ?";
    params << limit;
    auto r = Database::instance().execute(sql, params);
    if (r.is_err())
        return out;
    auto& q = r.value();
    while (q.next())
        out.append(map_row(q));
    return out;
}

ActionCenter::Stats ActionCenter::get_stats(const QString& account_id) const {
    Stats s;
    // Stats are computed across ALL orders (matches OpenAlgo behaviour: tabs
    // always show correct totals regardless of the active status filter).
    const auto orders = get_all_orders(account_id, 100000);
    for (const auto& o : orders) {
        if (o.status == "pending")
            ++s.total_pending;
        else if (o.status == "approved")
            ++s.total_approved;
        else if (o.status == "rejected")
            ++s.total_rejected;

        const QString act = o.action.toUpper();
        if (act == "BUY")
            ++s.total_buy;
        else if (act == "SELL")
            ++s.total_sell;

        if (o.order_type == "placeorder")
            ++s.by_type_place;
        else if (o.order_type == "smartorder")
            ++s.by_type_smart;
        else if (o.order_type == "basketorder")
            ++s.by_type_basket;
    }
    return s;
}

// ── Execution on approval ──────────────────────────────────────────────────

QString ActionCenter::execute_pending(const PendingOrder& po, bool& ok, QString& err, bool& async) {
    ok = false;
    async = false;
    // An approval still waiting on the FIFO executor when the kill switch is hit
    // must not reach the broker in the middle of the flatten.
    if (kill_switch_running_) {
        err = QStringLiteral("Kill switch engaged — order not sent");
        return {};
    }
    if (po.order_type == "placeorder" || po.order_type == "smartorder") {
        // Smart collapses to a standard place via UnifiedTrading for now —
        // the serialized order carries the same fields. (place_smart_order
        // routing can be wired here later without changing the queue/approve
        // contract.)
        UnifiedOrder order = deserialize_unified_order(po.order_data);
        UnifiedOrderResponse resp = UnifiedTrading::instance().place_order(po.account_id, order);
        ok = resp.success;
        err = resp.message;
        return resp.order_id;
    }

    if (po.order_type == "placegttorder") {
        // A GTT is a RESTING trigger at the broker — it must NEVER collapse
        // into an immediate place_order (that fires the order right away).
        // Route to the broker's native GTT API; when the account has no GTT
        // path, fail loudly so the user is not misled into an instant fill.
        auto account = AccountManager::instance().get_account(po.account_id);
        if (account.account_id.isEmpty()) {
            err = "Account not found: " + po.account_id;
            return {};
        }
        if (account.trading_mode == "paper") {
            err = "GTT not supported for paper accounts";
            return {};
        }
        auto* broker = BrokerRegistry::instance().get(account.broker_id);
        if (!broker) {
            err = "Broker not found: " + account.broker_id;
            return {};
        }
        const UnifiedOrder base = deserialize_unified_order(po.order_data);
        if (base.stop_price <= 0.0) {
            err = "GTT order missing trigger_price";
            return {};
        }
        // GTTs bypass UnifiedTrading::place_order, so they take the same
        // validation + settings-driven risk gate here.
        if (const auto vr = OrderValidator::validate(base); !vr.valid) {
            err = "Validation failed: " + vr.errors.join("; ");
            return {};
        }
        if (const auto gate = OrderValidator::pre_trade_risk(po.account_id, base, /*broker_routed=*/true);
            !gate.valid) {
            err = "Risk check failed: " + gate.errors.join("; ");
            return {};
        }
        GttOrder gtt;
        gtt.symbol = base.symbol;
        gtt.exchange = base.exchange;
        gtt.type = GttOrderType::Single;
        gtt.last_price = po.order_data.value("last_price").toDouble();
        GttTrigger trig;
        trig.trigger_price = base.stop_price; // deserialize maps trigger_price/stop_price here
        trig.limit_price = base.price;
        trig.quantity = base.quantity;
        trig.side = base.side;
        // GTT legs are Market or Limit only: SL-M fires at market on trigger,
        // Limit / SL carry their limit price.
        trig.order_type = (base.order_type == OrderType::Market || base.order_type == OrderType::StopLoss)
                              ? OrderType::Market
                              : OrderType::Limit;
        trig.product = base.product_type;
        gtt.triggers.append(trig);

        const auto creds = AccountManager::instance().load_credentials(po.account_id);
        // Brokers without a gtt_place override return the base-class
        // "GTT not supported for this broker" — surfaced as a rejection.
        const GttPlaceResponse resp = broker->gtt_place(creds, gtt);
        ok = resp.success;
        err = resp.error;
        return resp.gtt_id;
    }

    if (po.order_type == "basketorder") {
        BasketOrderRequest basket;
        basket.strategy_name = po.order_data.value("strategy").toString();
        const QJsonArray arr = po.order_data.value("orders").toArray();
        for (const auto& v : arr)
            basket.orders.append(deserialize_unified_order(v.toObject()));
        // place_basket_orders is async (background thread + callback). It used to
        // be fired with an EMPTY callback and the row marked "approved" on the
        // spot — which discarded "Account not found", "No credentials … Please
        // authenticate" and every per-leg rejection, so the approval audit trail
        // claimed success for a basket that never left the building. Capture the
        // real result and finalize the row from the callback instead.
        UnifiedTrading::instance().place_basket_orders(
            po.account_id, basket, [this, po](const BasketOrderResult& res) {
                const bool all_ok = (res.failed == 0 && res.successful > 0);
                QStringList errs;
                for (const auto& r : res.results) {
                    if (!r.success)
                        errs.append(QString("%1: %2").arg(r.symbol, r.error));
                }
                const QString detail =
                    all_ok ? QString()
                           : QString("%1/%2 legs failed — %3")
                                 .arg(res.failed)
                                 .arg(res.total)
                                 .arg(errs.isEmpty() ? QStringLiteral("no legs placed") : errs.join("; "));
                finalize_approval(po, all_ok, detail, {}); // no single broker order id for a basket
            });
        async = true;
        return {};
    }

    if (po.order_type == "splitorder") {
        SplitOrderRequest req;
        req.base_order = deserialize_unified_order(po.order_data);
        req.split_size = po.order_data.value("splitsize").toInt((int)po.order_data.value("split_size").toDouble());
        req.delay_between_ms = po.order_data.value("delay_between_ms").toInt(100);
        // Same as basketorder: the outcome only exists in the callback, so the row
        // is finalized there rather than pre-declared approved.
        UnifiedTrading::instance().place_split_orders(po.account_id, req, [this, po](const SplitOrderResult& res) {
            const bool all_ok = (res.chunks_failed == 0 && res.chunks_successful > 0);
            QStringList errs;
            for (const auto& r : res.results) {
                if (!r.success)
                    errs.append(r.error);
            }
            const QString detail = all_ok ? QString()
                                          : QString("%1/%2 chunks failed — %3")
                                                .arg(res.chunks_failed)
                                                .arg(res.chunks_failed + res.chunks_successful)
                                                .arg(errs.isEmpty() ? QStringLiteral("no chunks placed")
                                                                    : errs.join("; "));
            // A split has one broker order per chunk; record the first for traceability.
            QString first_id;
            for (const auto& r : res.results) {
                if (r.success && !r.order_id.isEmpty()) {
                    first_id = r.order_id;
                    break;
                }
            }
            finalize_approval(po, all_ok, detail, first_id);
        });
        async = true;
        return {};
    }

    err = "Unknown order_type: " + po.order_type;
    return {};
}

qint64 ActionCenter::age_secs(const PendingOrder& po) {
    if (!po.created_at.isValid())
        return -1;
    return po.created_at.secsTo(QDateTime::currentDateTime());
}

bool ActionCenter::is_expired(const PendingOrder& po) {
    // Only a still-pending order can expire; approved/rejected rows are history.
    // Unreadable timestamps: we cannot prove the order is fresh, so it may not
    // be executed (fail closed). The sweep rejects it with an explanation.
    return expired_at(po.status, po.expires_at, QDateTime::currentDateTime());
}

void ActionCenter::approve_order(const QString& pending_id) {
    auto po_opt = get_order(pending_id);
    if (!po_opt) {
        LOG_WARN(kLog, "approve_order: pending id not found: " + pending_id);
        return;
    }
    PendingOrder po = *po_opt;
    if (po.status != "pending") {
        LOG_WARN(kLog, QString("approve_order: %1 is not pending (status=%2)").arg(pending_id, po.status));
        return;
    }

    // Refuse a stale order outright. The UI flags these, but enforcing it here
    // means no caller — the approval panel, a bulk approve, or a future
    // automation path — can execute one by taking a different route.
    if (is_expired(po)) {
        const QString reason = ac_expiry_reason(po);
        LOG_WARN(kLog, QString("approve_order: refusing expired order %1 (age %2 s)").arg(pending_id).arg(age_secs(po)));

        // Conditional on 'pending' so it can never overwrite a row another path
        // already finalized.
        auto r = Database::instance().execute("UPDATE pending_orders SET status = 'rejected', rejected_at = ?, "
                                              "rejection_reason = ? WHERE id = ? AND status = 'pending'",
                                              {iso(QDateTime::currentDateTime()), reason, pending_id});
        if (r.is_err())
            LOG_ERROR(kLog, QString("approve_order expiry update failed: %1").arg(QString::fromStdString(r.error())));

        emit order_rejected(pending_id, reason);
        emit stats_updated(po.account_id);
        return;
    }

    // Claim the row BEFORE executing it. execute_pending() blocks on a broker
    // round-trip that spins a nested event loop (up to ~8 s/order), which keeps
    // delivering UI events — so the "is it still pending?" read above and the
    // status write below used to straddle a window in which a second click, or
    // Approve All running while a per-row approve was in flight, could send the
    // same queued order twice. The conditional UPDATE is atomic: exactly one
    // caller sees one affected row and proceeds.
    auto claim = Database::instance().execute(
        "UPDATE pending_orders SET status = 'approving' WHERE id = ? AND status = 'pending'", {pending_id});
    if (claim.is_err()) {
        LOG_ERROR(kLog, QString("approve_order claim failed: %1").arg(QString::fromStdString(claim.error())));
        return;
    }
    if (claim.value().numRowsAffected() != 1) {
        LOG_WARN(kLog, QString("approve_order: %1 was already claimed by another approval — ignoring").arg(pending_id));
        return;
    }
    po.status = QStringLiteral("approving");

    // placeorder / smartorder (UnifiedTrading::place_order) and placegttorder
    // (broker->gtt_place) are blocking broker round-trips, up to ~8 s each. They used
    // to run right here on the GUI thread, freezing the terminal for the whole
    // exchange and — in approve_all_pending — for every order in the queue (P1).
    // They now run on the dedicated FIFO worker and hand the real result back to the
    // GUI thread, where the row is finalized and the same signals fire. The order
    // contents (`po`) are passed through untouched; only WHERE the call runs moves.
    // Basket / split already hand off to their own worker and finalize from their
    // result callback, so they stay as they were.
    const bool blocking_type = po.order_type == QLatin1String("placeorder") ||
                               po.order_type == QLatin1String("smartorder") ||
                               po.order_type == QLatin1String("placegttorder");
    if (blocking_type) {
        QPointer<ActionCenter> self(this);
        (void)QtConcurrent::run(&action_center_exec_pool(), [self, po]() {
            if (!self)
                return;
            bool w_ok = false;
            bool w_async = false;
            QString w_err;
            QString w_broker_order_id;
            try {
                w_broker_order_id = self->execute_pending(po, w_ok, w_err, w_async);
            } catch (const std::exception& e) {
                // An exception that escapes a QtConcurrent task terminates the process,
                // and swallowing it would strand the row in 'approving' forever. The
                // order MAY already be at the broker, so say so rather than "rejected".
                w_ok = false;
                w_broker_order_id.clear();
                w_err = QStringLiteral("unexpected error (%1) — check the broker order book before re-placing")
                            .arg(QString::fromUtf8(e.what()));
            } catch (...) {
                w_ok = false;
                w_broker_order_id.clear();
                w_err = QStringLiteral("unexpected error — check the broker order book before re-placing");
            }
            QMetaObject::invokeMethod(
                self,
                [self, po, w_ok, w_err, w_broker_order_id]() {
                    if (!self)
                        return;
                    self->finalize_approval(po, w_ok, w_err, w_broker_order_id);
                },
                Qt::QueuedConnection);
        });
        return;
    }

    bool ok = false;
    bool async = false;
    QString err;
    const QString broker_order_id = execute_pending(po, ok, err, async);
    if (async)
        return; // basket / split: the result callback finalizes the row.

    finalize_approval(po, ok, err, broker_order_id);
}

void ActionCenter::finalize_approval(const PendingOrder& po, bool ok, const QString& err,
                                     const QString& broker_order_id) {
    const QDateTime now = QDateTime::currentDateTime();
    auto r = Database::instance().execute("UPDATE pending_orders SET status = ?, approved_at = ?, broker_order_id = ?, "
                                          "rejection_reason = ? WHERE id = ?",
                                          {ok ? "approved" : "rejected", iso(now), broker_order_id,
                                           ok ? QString() : ("Execution failed: " + err), po.id});
    if (r.is_err())
        LOG_ERROR(kLog, QString("approve_order update failed: %1").arg(QString::fromStdString(r.error())));

    if (ok) {
        LOG_INFO(kLog, QString("Approved %1 → broker order %2").arg(po.id, broker_order_id));
        emit order_approved(po.id, broker_order_id);
    } else {
        LOG_WARN(kLog, QString("Approval of %1 executed but broker rejected: %2").arg(po.id, err));
        emit order_rejected(po.id, "Execution failed: " + err);
    }
    emit stats_updated(po.account_id);
}

void ActionCenter::reject_order(const QString& pending_id, const QString& reason) {
    auto po_opt = get_order(pending_id);
    if (!po_opt) {
        LOG_WARN(kLog, "reject_order: pending id not found: " + pending_id);
        return;
    }
    PendingOrder po = *po_opt;
    if (po.status != "pending")
        return;

    const QDateTime now = QDateTime::currentDateTime();
    auto r = Database::instance().execute(
        "UPDATE pending_orders SET status = 'rejected', rejected_at = ?, rejection_reason = ? WHERE id = ?",
        {iso(now), reason, pending_id});
    if (r.is_err()) {
        LOG_ERROR(kLog, QString("reject_order update failed: %1").arg(QString::fromStdString(r.error())));
        return;
    }
    LOG_INFO(kLog, QString("Rejected %1: %2").arg(pending_id, reason));
    emit order_rejected(pending_id, reason);
    emit stats_updated(po.account_id);
}

void ActionCenter::approve_all_pending(const QString& account_id) {
    const auto pending = get_pending_orders(account_id);
    for (const auto& po : pending)
        approve_order(po.id);
    if (!pending.isEmpty())
        emit stats_updated(account_id);
}

void ActionCenter::reject_all_pending(const QString& account_id, const QString& reason) {
    const auto pending = get_pending_orders(account_id);
    for (const auto& po : pending)
        reject_order(po.id, reason);
    if (!pending.isEmpty())
        emit stats_updated(account_id);
}

} // namespace fincept::trading
