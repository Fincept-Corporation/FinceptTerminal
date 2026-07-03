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

#include <optional>

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
    QString id;         // UUID
    QString account_id; // owning broker account
    QString order_type; // "placeorder","smartorder","basketorder","splitorder","placegttorder"
    QJsonObject order_data; // serialized order (no credentials)
    QString status;     // "pending","approved","rejected"
    QDateTime created_at;
    QDateTime approved_at;
    QDateTime rejected_at;
    QString rejection_reason;
    QString broker_order_id; // set after execution on approval

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
    QString queue_order(const QString& account_id, const QString& order_type,
                        const QJsonObject& order_data);

    // User actions.
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

  private:
    ActionCenter() = default;

    // Operations that always execute immediately (never queued).
    static const QSet<QString>& immediate_ops();

    // Parse the display fields on a PendingOrder from its order_data + type.
    static void populate_display_fields(PendingOrder& po);

    // Row mapper for the pending_orders table.
    static PendingOrder map_row(class QSqlQuery& q);

    // Execute a queued order against UnifiedTrading (placegttorder routes to the
    // broker's native gtt_place — never an immediate order). Returns the broker
    // order id / GTT id (empty on failure) and sets `ok`/`err`.
    QString execute_pending(const PendingOrder& po, bool& ok, QString& err);
};

} // namespace fincept::trading

#include <QMetaType>
Q_DECLARE_METATYPE(fincept::trading::PendingOrder)
