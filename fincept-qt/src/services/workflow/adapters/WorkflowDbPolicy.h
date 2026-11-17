#pragma once
// WorkflowDbPolicy.h — which statements the workflow "Database" node
// (utility.database) may run, before any connection is opened. Pure decisions
// over the node's settings and the SQL text (Qt Core only, header-only) so they
// can be unit-tested; WorkflowDbQuery.h applies them and runs the statement.
//
//  - The application's own database (fincept.db) is read-only for workflows and
//    gets the SQL console's full policy (storage_sql_guard::check): one
//    statement; SELECT / WITH / VALUES / EXPLAIN, or a schema PRAGMA from an
//    allow-list and never with "= value"; a WITH that fronts INSERT / UPDATE /
//    DELETE is refused; no credential tables; no secret columns.
//  - A custom database file: "select" gets the same statement-shape rules; the
//    credential-table / secret-column rules describe fincept.db and do not apply.
//    Insert / update / delete / raw are the user's own business on their file.
// Every symbol lives in this namespace so unity builds cannot collide with it.

#include "screens/settings/StorageSqlConsoleGuard.h"

#include <QFileInfo>
#include <QString>

namespace fincept::workflow::db_node {

enum class Source {
    AppDatabase, ///< fincept.db: read-only, console policy, masked values
    CustomFile,  ///< a user-chosen SQLite file
};

/// True when both paths name the same existing file.
inline bool same_file(const QString& a, const QString& b) {
    if (a.isEmpty() || b.isEmpty())
        return false;
    const QString ca = QFileInfo(a).canonicalFilePath();
    const QString cb = QFileInfo(b).canonicalFilePath();
    if (ca.isEmpty() || cb.isEmpty())
        return false;
#ifdef Q_OS_WIN
    return ca.compare(cb, Qt::CaseInsensitive) == 0;
#else
    return ca == cb;
#endif
}

/// Operations that change data. Anything else (including an unknown value) runs
/// as the read-only "select", as before.
inline bool is_write_operation(const QString& op) {
    return op == QLatin1String("insert") || op == QLatin1String("update") || op == QLatin1String("delete") ||
           op == QLatin1String("raw");
}

/// Why the node refuses to run `sql` as `op` against `source`, or an empty string
/// when it may run.
inline QString refusal(const QString& sql, const QString& op, Source source) {
    if (is_write_operation(op)) {
        if (source == Source::AppDatabase)
            return QStringLiteral("A workflow cannot modify the application's own database — set Database to "
                                  "\"custom\" and use a database file");
        return {};
    }
    namespace guard = storage_sql_guard;
    const guard::Decision d = guard::check(sql);
    switch (d.verdict) {
        case guard::Verdict::Allowed:
            return {};
        case guard::Verdict::Empty:
            return QStringLiteral("SQL query is required");
        case guard::Verdict::Unparseable:
            return QStringLiteral("The query has an unterminated string or quoted name");
        case guard::Verdict::MultipleStatements:
            return QStringLiteral("Run one SQL statement per Database node");
        case guard::Verdict::NotReadOnly:
            return QStringLiteral("The Select operation only runs read-only queries (SELECT, WITH, VALUES or EXPLAIN). "
                                  "Use Insert / Update / Delete / Raw with a custom database file to change data");
        case guard::Verdict::PragmaNotAllowed:
            return QStringLiteral("Only read-only schema PRAGMAs (such as table_info, index_list or user_version) "
                                  "can run here, and never with \"= value\"");
        case guard::Verdict::BlockedFunction:
            return QStringLiteral("The function %1 cannot be used in a workflow query").arg(d.subject);
        case guard::Verdict::BlockedTable:
            if (source == Source::CustomFile)
                return {};
            return QStringLiteral("The %1 table holds credentials and cannot be read from a workflow").arg(d.subject);
        case guard::Verdict::SecretColumn:
            if (source == Source::CustomFile)
                return {};
            return QStringLiteral("%1 holds secrets and cannot be read from a workflow; name the columns you need "
                                  "instead of using * or that column")
                .arg(d.subject);
    }
    return QStringLiteral("This query is not allowed");
}

} // namespace fincept::workflow::db_node
