#pragma once

#include "core/result/Result.h"

#include <QDateTime>
#include <QJsonObject>
#include <QObject>
#include <QSqlQuery>
#include <QString>
#include <QVector>

namespace fincept::workflow {

/// Audit action types.
enum class AuditAction {
    OrderPlaced,
    OrderCancelled,
    OrderModified,
    OrderFilled,
    WorkflowStarted,
    WorkflowCompleted,
    WorkflowFailed,
    NodeExecuted,
    NodeFailed,
    RiskCheckPassed,
    RiskCheckFailed,
    ConfirmationApproved,
    ConfirmationRejected,
    LimitChanged,
    SettingsChanged,
};

/// A single audit log entry.
struct AuditEntry {
    qint64 id = 0;
    AuditAction action;
    QString workflow_id;
    QString node_id;
    QString symbol;
    QString details;
    QJsonObject metadata;
    bool paper_trading = true;
    QDateTime timestamp;
};

/// SQLite-backed audit logger for workflow execution events.
class AuditLogger : public QObject {
    Q_OBJECT
  public:
    static AuditLogger& instance();

    /// Log an audit event.
    void log(AuditAction action, const QString& workflow_id = {},
             const QString& node_id = {}, const QString& symbol = {},
             const QString& details = {}, const QJsonObject& metadata = {},
             bool paper_trading = true);

    /// Query logs by date range.
    Result<QVector<AuditEntry>> query(const QDateTime& from, const QDateTime& to,
                                      AuditAction action_filter = AuditAction::OrderPlaced,
                                      bool filter_by_action = false) const;

    /// Query logs for a specific workflow.
    Result<QVector<AuditEntry>> by_workflow(const QString& workflow_id) const;

    /// Get recent entries (last N).
    Result<QVector<AuditEntry>> recent(int limit = 50) const;

    /// Export all logs as JSON string.
    QString export_json() const;

    /// Get total log count.
    int count() const;

  private:
    AuditLogger();
    void ensure_table();

    static QString action_to_string(AuditAction action);
    static AuditAction string_to_action(const QString& str);
    static AuditEntry map_entry(QSqlQuery& q);
};

} // namespace fincept::workflow
