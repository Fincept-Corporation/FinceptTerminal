#pragma once
// StorageSqlConsoleRunner.h — executes one SQL-console statement and returns
// display-ready, secret-masked rows.
//
// The statement never touches the app's own connections. It runs on a private
// connection opened with SQLITE_OPEN_READONLY (plus PRAGMA query_only), created
// and removed on the calling thread — so the caller can run it from
// QtConcurrent::run without blocking the UI, and a policy gap in
// StorageSqlConsoleGuard.h still cannot write. Check the statement with
// storage_sql_guard::check() first; this file does not repeat that policy.

#include "screens/settings/SettingsSecretMask.h"

#include <QAtomicInt>
#include <QElapsedTimer>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVector>

namespace fincept::storage_sql_runner {

struct Result {
    bool ok = false;
    QString error;
    QStringList columns;
    QVector<QStringList> rows;
    QVector<QVector<bool>> null_cells; ///< aligned with rows: true where the stored value is SQL NULL
    bool truncated = false;            ///< more rows existed than max_rows
    int masked = 0;                    ///< number of cells (or parts of cells) hidden
    qint64 elapsed_ms = 0;
};

/// Longest text kept per cell; the console is for inspection, not export.
inline constexpr qsizetype kMaxCellChars = 4096;

/// Values stored under secret-looking keys in the key/value tables. A query such
/// as `SELECT value FROM settings WHERE key = '…'` returns no key column to judge
/// by, so those values are masked wherever they reappear in a result.
inline QStringList collect_kv_secrets(QSqlDatabase& db) {
    QStringList out;
    for (const QString& table : {QStringLiteral("settings"), QStringLiteral("key_value_storage")}) {
        QSqlQuery q(db);
        q.setForwardOnly(true);
        if (!q.exec(QStringLiteral("SELECT key, value FROM %1").arg(table)))
            continue; // the table does not exist in this database (cache.db)
        while (q.next()) {
            const QString value = q.value(1).toString().trimmed();
            // Short values ("1", "true", "en") would mask half the result set and
            // are not credentials anyway.
            if (value.size() >= 6 &&
                settings_secret_mask::should_hide(settings_secret_mask::name_strength(q.value(0).toString()), value))
                out.append(value);
        }
    }
    return out;
}

inline QString cell_text(const QVariant& v) {
    if (v.typeId() == QMetaType::QByteArray)
        return QStringLiteral("<blob %1 bytes>").arg(v.toByteArray().size());
    QString s = v.toString();
    if (s.size() > kMaxCellChars)
        s = s.left(kMaxCellChars) + QStringLiteral(" …");
    return s;
}

/// Run `sql` against the SQLite file at `db_path` and return at most `max_rows`
/// rows, masked for display.
inline Result run(const QString& db_path, const QString& sql, int max_rows) {
    namespace mask = settings_secret_mask;
    Result r;
    QElapsedTimer timer;
    timer.start();

    static QAtomicInt s_counter;
    const QString conn_name = QStringLiteral("fincept_sql_console_%1").arg(s_counter.fetchAndAddRelaxed(1));
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), conn_name);
        db.setDatabaseName(db_path);
        db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY;QSQLITE_BUSY_TIMEOUT=3000"));
        if (!db.open()) {
            r.error = db.lastError().text();
        } else {
            {
                QSqlQuery pragma(db);
                pragma.exec(QStringLiteral("PRAGMA query_only = 1"));
            }
            const QStringList kv_secrets = collect_kv_secrets(db);

            QSqlQuery q(db);
            q.setForwardOnly(true);
            if (!q.exec(sql)) {
                r.error = q.lastError().text();
            } else {
                r.ok = true;
                const QSqlRecord rec = q.record();
                const int cols = rec.count();
                QVector<mask::SecretStrength> col_strength(cols, mask::SecretStrength::None);
                // Bookkeeping columns of the key/value tables: never a secret, so a
                // secret-keyed row keeps them readable.
                static const QStringList kRowMetaColumns = {
                    QStringLiteral("category"),     QStringLiteral("updated_at"),  QStringLiteral("created_at"),
                    QStringLiteral("expires_at"),   QStringLiteral("content_type"), QStringLiteral("ttl_seconds"),
                    QStringLiteral("hit_count"),    QStringLiteral("size_bytes")};
                QVector<bool> row_meta(cols, false);
                int key_col = -1;
                for (int c = 0; c < cols; ++c) {
                    const QString name = rec.fieldName(c);
                    r.columns.append(name);
                    col_strength[c] = mask::name_strength(name);
                    row_meta[c] = kRowMetaColumns.contains(name, Qt::CaseInsensitive);
                    if (key_col < 0 && name.compare(QLatin1String("key"), Qt::CaseInsensitive) == 0)
                        key_col = c;
                }
                while (cols > 0 && q.next()) {
                    if (r.rows.size() >= max_rows) {
                        r.truncated = true;
                        break;
                    }
                    // A key/value row whose KEY names a secret hides the rest of the row.
                    const mask::SecretStrength row_strength =
                        key_col >= 0 ? mask::name_strength(q.value(key_col).toString()) : mask::SecretStrength::None;
                    QStringList row;
                    QVector<bool> nulls;
                    row.reserve(cols);
                    nulls.reserve(cols);
                    for (int c = 0; c < cols; ++c) {
                        const QVariant v = q.value(c);
                        if (v.isNull()) {
                            row.append(QStringLiteral("NULL"));
                            nulls.append(true);
                            continue;
                        }
                        nulls.append(false);
                        const QString raw = v.typeId() == QMetaType::QByteArray ? QString() : v.toString();
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
                            row.append(mask::placeholder());
                            ++r.masked;
                            continue;
                        }
                        bool changed = false;
                        const QString shown = v.typeId() == QMetaType::QByteArray
                                                  ? cell_text(v)
                                                  : cell_text(mask::redact_value(raw, &changed));
                        if (changed)
                            ++r.masked;
                        row.append(shown);
                    }
                    r.rows.append(row);
                    r.null_cells.append(nulls);
                }
                q.finish();
            }
        }
        db.close();
    }
    QSqlDatabase::removeDatabase(conn_name);
    r.elapsed_ms = timer.elapsed();
    return r;
}

} // namespace fincept::storage_sql_runner
