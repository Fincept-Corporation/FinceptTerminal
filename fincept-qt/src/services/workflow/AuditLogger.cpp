#include "services/workflow/AuditLogger.h"

#include "core/logging/Logger.h"
#include "storage/sqlite/Database.h"

#include <QJsonDocument>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

namespace fincept::workflow {

AuditLogger& AuditLogger::instance() {
    static AuditLogger s;
    return s;
}

AuditLogger::AuditLogger() : QObject(nullptr) {
    ensure_table();
}

void AuditLogger::ensure_table() {
    auto r = fincept::Database::instance().exec("CREATE TABLE IF NOT EXISTS workflow_audit_log ("
                                                "  id          INTEGER PRIMARY KEY AUTOINCREMENT,"
                                                "  action      TEXT NOT NULL,"
                                                "  workflow_id TEXT DEFAULT '',"
                                                "  node_id     TEXT DEFAULT '',"
                                                "  symbol      TEXT DEFAULT '',"
                                                "  details     TEXT DEFAULT '',"
                                                "  metadata    TEXT DEFAULT '{}',"
                                                "  paper       INTEGER NOT NULL DEFAULT 1,"
                                                "  timestamp   TEXT DEFAULT (datetime('now'))"
                                                ")");
    if (r.is_err())
        LOG_ERROR("AuditLogger", QString("Failed to create audit table: %1").arg(QString::fromStdString(r.error())));

    fincept::Database::instance().exec("CREATE INDEX IF NOT EXISTS idx_audit_ts ON workflow_audit_log(timestamp)");
    fincept::Database::instance().exec("CREATE INDEX IF NOT EXISTS idx_audit_action ON workflow_audit_log(action)");
    fincept::Database::instance().exec("CREATE INDEX IF NOT EXISTS idx_audit_wf ON workflow_audit_log(workflow_id)");
}

void AuditLogger::log(AuditAction action, const QString& workflow_id, const QString& node_id, const QString& symbol,
                      const QString& details, const QJsonObject& metadata, bool paper_trading) {
    auto& db = fincept::Database::instance();
    auto r =
        db.execute("INSERT INTO workflow_audit_log (action, workflow_id, node_id, symbol, details, metadata, paper) "
                   "VALUES (?, ?, ?, ?, ?, ?, ?)",
                   {action_to_string(action), workflow_id, node_id, symbol, details,
                    QString::fromUtf8(QJsonDocument(metadata).toJson(QJsonDocument::Compact)), paper_trading ? 1 : 0});

    if (r.is_err())
        LOG_ERROR("AuditLogger", QString("Failed to log: %1").arg(QString::fromStdString(r.error())));
}

Result<QVector<AuditEntry>> AuditLogger::query(const QDateTime& from, const QDateTime& to, AuditAction action_filter,
                                               bool filter_by_action) const {
    QString sql = "SELECT id, action, workflow_id, node_id, symbol, details, metadata, paper, timestamp "
                  "FROM workflow_audit_log WHERE timestamp BETWEEN ? AND ?";
    QVariantList params = {from.toString(Qt::ISODate), to.toString(Qt::ISODate)};

    if (filter_by_action) {
        sql += " AND action = ?";
        params.append(action_to_string(action_filter));
    }
    sql += " ORDER BY timestamp DESC";

    auto r = fincept::Database::instance().execute(sql, params);
    if (r.is_err())
        return Result<QVector<AuditEntry>>::err(r.error());

    QVector<AuditEntry> entries;
    auto& q = r.value();
    while (q.next())
        entries.append(map_entry(q));
    return Result<QVector<AuditEntry>>::ok(std::move(entries));
}

Result<QVector<AuditEntry>> AuditLogger::by_workflow(const QString& workflow_id) const {
    auto r = fincept::Database::instance().execute(
        "SELECT id, action, workflow_id, node_id, symbol, details, metadata, paper, timestamp "
        "FROM workflow_audit_log WHERE workflow_id = ? ORDER BY timestamp DESC",
        {workflow_id});
    if (r.is_err())
        return Result<QVector<AuditEntry>>::err(r.error());

    QVector<AuditEntry> entries;
    auto& q = r.value();
    while (q.next())
        entries.append(map_entry(q));
    return Result<QVector<AuditEntry>>::ok(std::move(entries));
}

Result<QVector<AuditEntry>> AuditLogger::recent(int limit) const {
    auto r = fincept::Database::instance().execute(
        "SELECT id, action, workflow_id, node_id, symbol, details, metadata, paper, timestamp "
        "FROM workflow_audit_log ORDER BY timestamp DESC LIMIT ?",
        {limit});
    if (r.is_err())
        return Result<QVector<AuditEntry>>::err(r.error());

    QVector<AuditEntry> entries;
    auto& q = r.value();
    while (q.next())
        entries.append(map_entry(q));
    return Result<QVector<AuditEntry>>::ok(std::move(entries));
}

int AuditLogger::count() const {
    auto r = fincept::Database::instance().execute("SELECT COUNT(*) FROM workflow_audit_log", {});
    if (r.is_err())
        return 0;
    auto& q = r.value();
    return q.next() ? q.value(0).toInt() : 0;
}

QString AuditLogger::export_json() const {
    auto r = recent(1000);
    if (r.is_err())
        return "[]";

    QJsonArray arr;
    for (const auto& e : r.value()) {
        QJsonObject obj;
        obj["id"] = static_cast<qint64>(e.id);
        obj["action"] = action_to_string(e.action);
        obj["workflow_id"] = e.workflow_id;
        obj["node_id"] = e.node_id;
        obj["symbol"] = e.symbol;
        obj["details"] = e.details;
        obj["metadata"] = e.metadata;
        obj["paper"] = e.paper_trading;
        obj["timestamp"] = e.timestamp.toString(Qt::ISODate);
        arr.append(obj);
    }
    return QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Indented));
}

AuditEntry AuditLogger::map_entry(QSqlQuery& q) {
    AuditEntry e;
    e.id = q.value(0).toLongLong();
    e.action = string_to_action(q.value(1).toString());
    e.workflow_id = q.value(2).toString();
    e.node_id = q.value(3).toString();
    e.symbol = q.value(4).toString();
    e.details = q.value(5).toString();
    e.metadata = QJsonDocument::fromJson(q.value(6).toString().toUtf8()).object();
    e.paper_trading = q.value(7).toBool();
    e.timestamp = QDateTime::fromString(q.value(8).toString(), Qt::ISODate);
    return e;
}

QString AuditLogger::action_to_string(AuditAction a) {
    switch (a) {
        case AuditAction::OrderPlaced:
            return "order_placed";
        case AuditAction::OrderCancelled:
            return "order_cancelled";
        case AuditAction::OrderModified:
            return "order_modified";
        case AuditAction::OrderFilled:
            return "order_filled";
        case AuditAction::WorkflowStarted:
            return "workflow_started";
        case AuditAction::WorkflowCompleted:
            return "workflow_completed";
        case AuditAction::WorkflowFailed:
            return "workflow_failed";
        case AuditAction::NodeExecuted:
            return "node_executed";
        case AuditAction::NodeFailed:
            return "node_failed";
        case AuditAction::RiskCheckPassed:
            return "risk_check_passed";
        case AuditAction::RiskCheckFailed:
            return "risk_check_failed";
        case AuditAction::ConfirmationApproved:
            return "confirmation_approved";
        case AuditAction::ConfirmationRejected:
            return "confirmation_rejected";
        case AuditAction::LimitChanged:
            return "limit_changed";
        case AuditAction::SettingsChanged:
            return "settings_changed";
    }
    return "unknown";
}

AuditAction AuditLogger::string_to_action(const QString& s) {
    if (s == "order_placed")
        return AuditAction::OrderPlaced;
    if (s == "order_cancelled")
        return AuditAction::OrderCancelled;
    if (s == "order_modified")
        return AuditAction::OrderModified;
    if (s == "order_filled")
        return AuditAction::OrderFilled;
    if (s == "workflow_started")
        return AuditAction::WorkflowStarted;
    if (s == "workflow_completed")
        return AuditAction::WorkflowCompleted;
    if (s == "workflow_failed")
        return AuditAction::WorkflowFailed;
    if (s == "node_executed")
        return AuditAction::NodeExecuted;
    if (s == "node_failed")
        return AuditAction::NodeFailed;
    if (s == "risk_check_passed")
        return AuditAction::RiskCheckPassed;
    if (s == "risk_check_failed")
        return AuditAction::RiskCheckFailed;
    if (s == "confirmation_approved")
        return AuditAction::ConfirmationApproved;
    if (s == "confirmation_rejected")
        return AuditAction::ConfirmationRejected;
    if (s == "limit_changed")
        return AuditAction::LimitChanged;
    if (s == "settings_changed")
        return AuditAction::SettingsChanged;
    return AuditAction::WorkflowStarted;
}

} // namespace fincept::workflow
