#pragma once
// WorkflowDbQuery.h — what the workflow "Database" node (utility.database) runs.
//
// Every call opens its OWN uniquely named connection on the calling thread and
// removes it before returning: nothing is shared with the app's connections,
// nothing stays open between runs, and no connection is used on a second thread.
//
//  - The application's own database (source "local", or a "custom" path that is
//    the same file) is read-only for workflows. The statement must pass the SQL
//    console's policy (storage_sql_guard::check: one statement; SELECT / WITH /
//    VALUES / EXPLAIN, or a schema PRAGMA from an allow-list and never with
//    "= value"; a WITH that fronts INSERT / UPDATE / DELETE is refused; no
//    credential tables; no secret columns). It then runs on a
//    SQLITE_OPEN_READONLY connection with PRAGMA query_only = 1, so a gap in the
//    text check still cannot write, and values are masked as the console masks
//    them. It used to run on the app's own read-write connection: "WITH x AS
//    (SELECT 1) DELETE FROM settings" passed a first-word check and deleted, and
//    "PRAGMA foreign_keys=OFF" changed the connection every repository uses.
//  - A custom database file: "select" gets the same read-only connection and the
//    same statement-shape rules (the credential-table / secret-column rules
//    describe fincept.db and do not apply); insert / update / delete / raw run on
//    a read-write connection. The connection used to be cached by path for the
//    life of the process and reused from whichever thread ran the node.
// Failures come back as one short sentence; the driver's own text is returned
// separately (Outcome::log_detail) for the log only.
//
// The statement policy (WorkflowDbPolicy.h → the SQL console's guard) and the
// masking rules are the SQL console's (screens/settings, header-only, Qt Core /
// Qt Sql, no widgets); they are reused here rather than copied. Every symbol
// lives in this namespace.

#include "screens/settings/SettingsSecretMask.h"
#include "screens/settings/StorageSqlConsoleRunner.h"
#include "services/workflow/adapters/WorkflowDbPolicy.h"

#include <QAtomicInt>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVector>

namespace fincept::workflow::db_node {

/// One short sentence for a failed open / statement, from the SQLite result code.
/// For SQLITE_ERROR (syntax, "no such table", "no such column") and constraint
/// failures SQLite's own one-line reason is kept: it describes the user's SQL.
inline QString failure_text(const QSqlError& e, bool read_only) {
    bool numeric = false;
    const int code = e.nativeErrorCode().toInt(&numeric) & 0xFF;
    switch (numeric ? code : -1) {
        case 5: // SQLITE_BUSY
        case 6: // SQLITE_LOCKED
            return QStringLiteral("The database is busy (another program or task is using it). Try again");
        case 8: // SQLITE_READONLY (query_only / read-only open)
            return read_only ? QStringLiteral("The Select operation is read-only and this statement would change the "
                                              "database")
                             : QStringLiteral("The database file is read-only");
        case 10: // SQLITE_IOERR
            return QStringLiteral("The computer reported a disk read or write error");
        case 11: // SQLITE_CORRUPT
            return QStringLiteral("The database file is damaged");
        case 13: // SQLITE_FULL
            return QStringLiteral("The disk is full");
        case 14: // SQLITE_CANTOPEN
            return QStringLiteral("The database file could not be opened");
        case 23: // SQLITE_AUTH
            return QStringLiteral("This statement is not allowed");
        case 26: // SQLITE_NOTADB
            return QStringLiteral("The file is not an SQLite database");
        case 1:  // SQLITE_ERROR
        case 19: // SQLITE_CONSTRAINT
        {
            QString reason = e.databaseText().section(QLatin1Char('\n'), 0, 0).trimmed();
            if (reason.isEmpty())
                break;
            if (reason.size() > 200)
                reason = reason.left(200) + QStringLiteral("…");
            settings_secret_mask::mask_token_patterns(reason);
            return QStringLiteral("SQL error: ") + reason;
        }
        default:
            break;
    }
    return QStringLiteral("The query could not be run (details are in the log)");
}

/// A result cell as JSON: numbers as numbers, booleans, null, everything else text.
inline QJsonValue cell_json(const QVariant& v) {
    if (v.isNull())
        return QJsonValue::Null;
    switch (v.typeId()) {
        case QMetaType::Double:
        case QMetaType::Int:
        case QMetaType::LongLong:
            return v.toDouble();
        case QMetaType::Bool:
            return v.toBool();
        default:
            return v.toString();
    }
}

struct Outcome {
    bool ok = false;
    QJsonObject output;  ///< the node's output on success
    QString error;       ///< one short sentence on failure (shown to the user)
    QString log_detail;  ///< driver text behind `error`, for the log only
};

/// Run `sql` for the Database node against the SQLite file at `db_path`.
/// `op` is the node's operation ("select" when anything but a write).
inline Outcome run(const QString& db_path, Source source, const QString& op, const QString& sql) {
    namespace mask = settings_secret_mask;
    Outcome r;
    const bool write = is_write_operation(op);
    r.error = refusal(sql, op, source);
    if (!r.error.isEmpty())
        return r;

    static QAtomicInt s_counter;
    const QString conn_name = QStringLiteral("fincept_workflow_db_%1").arg(s_counter.fetchAndAddRelaxed(1));
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), conn_name);
        db.setDatabaseName(db_path);
        db.setConnectOptions(write ? QStringLiteral("QSQLITE_BUSY_TIMEOUT=5000")
                                   : QStringLiteral("QSQLITE_OPEN_READONLY;QSQLITE_BUSY_TIMEOUT=3000"));
        if (!db.open()) {
            r.error = failure_text(db.lastError(), !write);
            r.log_detail = db.lastError().text();
        } else {
            bool ready = true;
            if (!write) {
                // Belt and braces with the read-only open: SQLite itself refuses
                // any write on this connection.
                QSqlQuery pragma(db);
                if (!pragma.exec(QStringLiteral("PRAGMA query_only = 1"))) {
                    ready = false;
                    r.error = failure_text(pragma.lastError(), true);
                    r.log_detail = pragma.lastError().text();
                }
            }
            if (ready) {
                const bool masked_source = !write && source == Source::AppDatabase;
                const QStringList kv_secrets =
                    masked_source ? storage_sql_runner::collect_kv_secrets(db) : QStringList();

                QSqlQuery q(db);
                q.setForwardOnly(true);
                if (!q.exec(sql)) {
                    r.error = failure_text(q.lastError(), !write);
                    r.log_detail = q.lastError().text();
                } else if (write) {
                    r.ok = true;
                    r.output[QStringLiteral("query")] = sql;
                    r.output[QStringLiteral("operation")] = op;
                    if (op == QLatin1String("raw"))
                        r.output[QStringLiteral("success")] = true;
                    r.output[QStringLiteral("rows_affected")] = q.numRowsAffected();
                } else {
                    const QSqlRecord rec = q.record();
                    const int cols = rec.count();
                    // The console's masking rules (StorageSqlConsoleRunner.h), on typed values.
                    static const QStringList kRowMetaColumns = {
                        QStringLiteral("category"),   QStringLiteral("updated_at"),   QStringLiteral("created_at"),
                        QStringLiteral("expires_at"), QStringLiteral("content_type"), QStringLiteral("ttl_seconds"),
                        QStringLiteral("hit_count"),  QStringLiteral("size_bytes")};
                    QVector<mask::SecretStrength> col_strength(cols, mask::SecretStrength::None);
                    QVector<bool> row_meta(cols, false);
                    int key_col = -1;
                    for (int c = 0; c < cols; ++c) {
                        const QString name = rec.fieldName(c);
                        col_strength[c] = mask::name_strength(name);
                        row_meta[c] = kRowMetaColumns.contains(name, Qt::CaseInsensitive);
                        if (key_col < 0 && name.compare(QLatin1String("key"), Qt::CaseInsensitive) == 0)
                            key_col = c;
                    }
                    QJsonArray rows;
                    int masked = 0;
                    while (q.next()) {
                        // A key/value row whose KEY names a secret hides the rest of the row.
                        const mask::SecretStrength row_strength = (masked_source && key_col >= 0)
                                                                      ? mask::name_strength(q.value(key_col).toString())
                                                                      : mask::SecretStrength::None;
                        QJsonObject row;
                        for (int c = 0; c < cols; ++c) {
                            const QVariant v = q.value(c);
                            QJsonValue cell = cell_json(v);
                            if (masked_source && !v.isNull()) {
                                const QString raw = v.toString();
                                bool hide = mask::should_hide(col_strength[c], raw) ||
                                            (c != key_col && !row_meta[c] && mask::should_hide(row_strength, raw));
                                if (!hide && !raw.isEmpty()) {
                                    for (const auto& secret : kv_secrets) {
                                        if (raw.contains(secret)) {
                                            hide = true;
                                            break;
                                        }
                                    }
                                }
                                if (hide) {
                                    cell = mask::placeholder();
                                    ++masked;
                                } else if (cell.isString()) {
                                    bool changed = false;
                                    const QString shown = mask::redact_value(raw, &changed);
                                    if (changed) {
                                        cell = shown;
                                        ++masked;
                                    }
                                }
                            }
                            row[rec.fieldName(c)] = cell;
                        }
                        rows.append(row);
                    }
                    r.ok = true;
                    r.output[QStringLiteral("query")] = sql;
                    r.output[QStringLiteral("operation")] = op;
                    r.output[QStringLiteral("row_count")] = rows.size();
                    r.output[QStringLiteral("rows")] = rows;
                    if (masked > 0)
                        r.output[QStringLiteral("masked_values")] = masked;
                }
                q.finish();
            }
        }
        db.close();
    }
    QSqlDatabase::removeDatabase(conn_name);
    return r;
}

} // namespace fincept::workflow::db_node
