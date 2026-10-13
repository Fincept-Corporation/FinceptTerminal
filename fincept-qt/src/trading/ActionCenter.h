#pragma once
// ActionCenter — semi-auto order approval (OpenAlgo bridge, Phase 3 §2).
//
// In SemiAuto mode, queueable order operations (placeorder/smartorder/
// basketorder/splitorder/placegttorder) are NOT sent to the broker
// immediately — they are persisted as PendingOrder records and surfaced in
// the Action Center UI, where the user can Approve (execute) or Reject (drop
// with a reason). Risk-management and read-only operations always execute
// immediately and are never queued.
//
// Design note (anti-recursion): ActionCenter is a *pre-gate* that the UI/screen
// layer consults BEFORE calling UnifiedTrading::place_order(). It is NOT an
// interceptor inside place_order(). On approval, ActionCenter deserializes the
// stored order and calls UnifiedTrading::instance().place_order(account_id,
// order) directly — place_order does not itself consult ActionCenter, so there
// is no recursion. (See OpenAlgo order_router_service.py for the original flow.)
//
// Persistence: pending orders live in the SQLite `pending_orders` table
// (migration v032). Per-account order mode is stored in the `settings` table
// via SettingsRepository (key "action_center.mode.<account_id>").

#include "trading/TradingTypes.h"

#include <QDateTime>
#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QString>
#include <QVector>

#include <atomic>
#include <optional>

class QTimer;

namespace fincept::trading {

enum class OrderMode {
    Auto,     // execute immediately (default)
    SemiAuto, // queue queueable ops for manual approval
};

inline const char* order_mode_str(OrderMode m) {
    return m == OrderMode::SemiAuto ? "semi_auto" : "auto";
}

inline OrderMode parse_order_mode(const QString& s) {
    return s == "semi_auto" ? OrderMode::SemiAuto : OrderMode::Auto;
}

/// A queued order awaiting approval / rejection.
struct PendingOrder {
    QString id;             // UUID
    QString account_id;     // owning broker account
    QString order_type;     // "placeorder","smartorder","basketorder","splitorder","placegttorder"
    QJsonObject order_data; // serialized order (no credentials)
    QString status;         // "pending","approved","rejected"
    QDateTime created_at;
    QDateTime approved_at;
    QDateTime rejected_at;
    QString rejection_reason;
    QString broker_order_id; // set after execution on approval

    // End of the approval window. Stamped into order_data["expires_at"] (UTC ISO)
    // when the order is queued, so it needs no schema migration; rows queued
    // before that existed derive it from created_at + the configured TTL.
    // Invalid only when created_at is unreadable — is_expired() treats that as
    // expired (fail closed).
    QDateTime expires_at;

    // Parsed display fields (derived from order_data for the table view).
    QString symbol;
    QString exchange;
    QString action; // "BUY" / "SELL" / "Multiple"
    QString quantity;
    QString price_type;
    QString strategy;
};

class ActionCenter : public QObject {
    Q_OBJECT
  public:
    static ActionCenter& instance();

    // Route decision: returns true only when the account is in SemiAuto mode
    // AND the operation is queueable (not in IMMEDIATE_OPS). The caller (screen
    // layer) consults this before placing the order.
    bool should_queue(const QString& account_id, const QString& order_type) const;

    // Queue an order for approval. Returns the new pending id (empty on failure).
    // `order_data` should NOT contain credentials. Emits pending_order_created.
    QString queue_order(const QString& account_id, const QString& order_type, const QJsonObject& order_data);

    // ── Expiry ──────────────────────────────────────────────────────────────
    // A queued order carries a price and a market view that both decay. An
    // order queued before lunch and approved after the close would execute at
    // a market the user never saw and never intended to trade — the approval
    // dialog would show the original details while the fill happens at the
    // current price.
    //
    // Every queued order therefore has an approval window (default 5 minutes,
    // user-configurable, setting "action_center.pending_ttl_minutes"). The end of
    // the window is stamped into order_data["expires_at"] at queue time (no
    // migration needed); legacy rows derive it from created_at + the TTL, so the
    // policy also applies to orders already sitting in the queue. Expired orders
    // are auto-rejected with a reason by a service-side sweep (runs only while
    // something is pending) and approve_order() refuses them regardless, so the
    // UI is never the only line of defence.
    static constexpr int kDefaultPendingTtlMinutes = 5;
    static constexpr int kMinPendingTtlMinutes = 1;
    static constexpr int kMaxPendingTtlMinutes = 240;

    /// PURE: end of a row's approval window — the stamped order_data["expires_at"]
    /// (UTC ISO) when present and valid, else created_at + ttl_minutes, else invalid.
    static QDateTime expiry_for(const QJsonObject& order_data, const QDateTime& created_at, int ttl_minutes) {
        const QString stamped = order_data.value(QStringLiteral("expires_at")).toString();
        if (!stamped.isEmpty()) {
            const QDateTime dt = QDateTime::fromString(stamped, Qt::ISODateWithMs);
            if (dt.isValid())
                return dt;
        }
        if (!created_at.isValid())
            return {};
        return created_at.addSecs(static_cast<qint64>(ttl_minutes) * 60);
    }

    /// PURE: whether a row with this status/expiry is expired at `now`. Only
    /// "pending" rows expire; an invalid expiry counts as expired (fail closed).
    static bool expired_at(const QString& status, const QDateTime& expires_at, const QDateTime& now) {
        if (status != QLatin1String("pending"))
            return false;
        if (!expires_at.isValid())
            return true;
        return now >= expires_at;
    }

    /// Configured approval window in minutes (clamped; default on read error).
    static int pending_ttl_minutes();
    void set_pending_ttl_minutes(int minutes);

    /// True when a still-pending order's approval window has closed (or its
    /// timestamps are unreadable — fail closed). Approved/rejected rows never expire.
    static bool is_expired(const PendingOrder& po);

    /// Seconds since the order was queued (-1 if created_at is invalid).
    static qint64 age_secs(const PendingOrder& po);

    /// Seconds left in the approval window (<= 0 once expired / unreadable).
    static qint64 secs_until_expiry(const PendingOrder& po);

    /// Auto-reject every pending order whose window has closed. Returns how many
    /// rows were rejected. Called by the internal sweep timer; safe to call any time.
    int expire_stale_orders();

    // ── One-click trading (per account, persisted, OFF by default) ───────────
    // LIVE orders entered from the order ticket always go through the order
    // confirmation dialog unless the user has explicitly opted this account into
    // one-click trading (setting "action_center.one_click.<account_id>").
    // Headless paths (MCP / workflow / algo / agents) are unaffected: they keep
    // their own Semi-Auto approval queue. Read errors answer false (confirm).
    bool one_click_enabled(const QString& account_id) const;
    void set_one_click_enabled(const QString& account_id, bool enabled);

    // ── Kill switch ─────────────────────────────────────────────────────────
    // For every active LIVE account: reject its queued (unapproved) orders, then
    // cancel all open broker orders, then square off all open positions, using
    // the existing per-broker cancel_all_orders / close_all_positions paths.
    // Holdings (delivery stock already in the demat) are not sold. Paper accounts
    // are listed as skipped. Runs on the order executor; the per-account outcome
    // arrives via kill_switch_finished on the GUI thread. Returns false (and does
    // nothing) when a kill switch is already running.
    struct KillSwitchAccountResult {
        QString account_id;
        QString display_name;
        QString broker_id;
        bool skipped = false;     // not a LIVE account — nothing sent
        QString skip_reason;
        int queued_rejected = 0;  // Action Center rows rejected
        int orders_cancelled = 0;
        int orders_cancel_failed = 0;
        int positions_closed = 0;
        int positions_close_failed = 0;
        QStringList errors;
        bool ok() const {
            return skipped || (errors.isEmpty() && orders_cancel_failed == 0 && positions_close_failed == 0);
        }
    };
    bool run_kill_switch();
    bool kill_switch_running() const { return kill_switch_running_; }
    /// Active accounts the kill switch would act on (LIVE mode). For the confirmation UI.
    QStringList kill_switch_target_labels() const;

    // User actions.
    // approve_order refuses an expired order and emits order_rejected with an
    // explanatory reason rather than silently executing it.
    void approve_order(const QString& pending_id);
    void reject_order(const QString& pending_id, const QString& reason);
    void approve_all_pending(const QString& account_id = {});
    void reject_all_pending(const QString& account_id, const QString& reason);

    // Query.
    QVector<PendingOrder> get_pending_orders(const QString& account_id = {}) const;
    QVector<PendingOrder> get_all_orders(const QString& account_id = {}, int limit = 100) const;
    std::optional<PendingOrder> get_order(const QString& pending_id) const;

    // Statistics (mirrors OpenAlgo's calculate_action_center_stats).
    struct Stats {
        int total_pending = 0;
        int total_approved = 0;
        int total_rejected = 0;
        int total_buy = 0;
        int total_sell = 0;
        int by_type_place = 0;
        int by_type_smart = 0;
        int by_type_basket = 0;
    };
    Stats get_stats(const QString& account_id = {}) const;

    // Per-account order mode (persisted in settings).
    void set_order_mode(const QString& account_id, OrderMode mode);
    OrderMode get_order_mode(const QString& account_id) const;

    // ── Serialization helpers (shared with screens / order entry) ────────────
    // Serialize a UnifiedOrder into the JSON form stored in order_data. Round
    // trips with deserialize_unified_order(). No credentials are included.
    static QJsonObject serialize_unified_order(const UnifiedOrder& order);
    static UnifiedOrder deserialize_unified_order(const QJsonObject& json);

    // Serialize a basket into the order_data form stored for a "basketorder"
    // pending row. Round-trips with the basketorder branch of execute_pending()
    // ({"strategy": name, "orders": [serialize_unified_order(leg)...]}).
    static QJsonObject serialize_basket_order(const BasketOrderRequest& basket);

  signals:
    void pending_order_created(const fincept::trading::PendingOrder& order);
    void order_approved(const QString& pending_id, const QString& broker_order_id);
    void order_rejected(const QString& pending_id, const QString& reason);
    void stats_updated(const QString& account_id);
    void kill_switch_finished(const QVector<fincept::trading::ActionCenter::KillSwitchAccountResult>& results);
    void one_click_changed(const QString& account_id, bool enabled);

  private:
    ActionCenter();

    // Start the expiry sweep (GUI thread) if it is not already running.
    void ensure_expiry_sweep();
    void on_expiry_tick();

    QTimer* expiry_timer_ = nullptr;
    std::atomic<bool> kill_switch_running_{false};

    // Operations that always execute immediately (never queued).
    static const QSet<QString>& immediate_ops();

    // Parse the display fields on a PendingOrder from its order_data + type.
    static void populate_display_fields(PendingOrder& po);

    // Row mapper for the pending_orders table.
    static PendingOrder map_row(class QSqlQuery& q);

    // Execute a queued order against UnifiedTrading (placegttorder routes to the
    // broker's native gtt_place — never an immediate order). Returns the broker
    // order id / GTT id (empty on failure) and sets `ok`/`err`.
    //
    // Basket and split orders are ASYNCHRONOUS: their outcome is not known when
    // this returns, so they set `async` and finalize the pending row themselves
    // from their result callback. When `async` is true the caller must not touch
    // the row — writing "approved" there is exactly the lie this fixes.
    QString execute_pending(const PendingOrder& po, bool& ok, QString& err, bool& async);

    // Write the terminal status of an approved row and emit the matching signal.
    // Shared by the synchronous path and the async (basket/split) callbacks so
    // the audit trail is produced in one place, from a real result.
    void finalize_approval(const PendingOrder& po, bool ok, const QString& err, const QString& broker_order_id);
};

} // namespace fincept::trading

#include <QMetaType>
Q_DECLARE_METATYPE(fincept::trading::PendingOrder)
Q_DECLARE_METATYPE(fincept::trading::ActionCenter::KillSwitchAccountResult)
