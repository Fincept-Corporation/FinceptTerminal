#include "storage/sqlite/SqliteSupport.h"

#include "core/logging/Logger.h"
#include "core/privacy/LogRedact.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlQuery>
#include <QStorageInfo>
#include <QVariant>

namespace fincept::db {

namespace {
constexpr const char* kSupportTag = "DB";

// SQLite primary result codes (sqlite3.h). The driver reports the code it got
// back, possibly extended (e.g. SQLITE_IOERR_SHORT_READ = 522); the low byte is
// always the primary code.
enum : int {
    kSqliteError = 1,
    kSqlitePerm = 3,
    kSqliteBusy = 5,
    kSqliteLocked = 6,
    kSqliteReadOnly = 8,
    kSqliteIoErr = 10,
    kSqliteCorrupt = 11,
    kSqliteFull = 13,
    kSqliteCantOpen = 14,
    kSqliteSchema = 17,
    kSqliteConstraint = 19,
    kSqliteNotADb = 26,
};

/// The database for a log line: its file with the home directory shown as "~".
QString support_label(const QSqlDatabase& db) {
    const QString name = db.databaseName();
    return name.isEmpty() ? db.connectionName() : privacy::redact_home(name);
}

/// Run a statement that returns a single value and return that value as text.
/// Empty on failure (logged when `log_failure`).
QString support_scalar(QSqlDatabase& db, const QString& sql, bool log_failure = true, QSqlError* error_out = nullptr) {
    QSqlQuery q(db);
    if (!q.exec(sql)) {
        if (error_out)
            *error_out = q.lastError();
        if (log_failure)
            LOG_WARN(kSupportTag, QString("%1 failed on %2: %3").arg(sql, support_label(db), q.lastError().text()));
        return {};
    }
    if (!q.next())
        return {};
    return q.value(0).toString();
}

void support_pragma(QSqlDatabase& db, const QString& sql) {
    QSqlQuery q(db);
    if (!q.exec(sql))
        LOG_WARN(kSupportTag, QString("PRAGMA failed on %1: %2 — %3").arg(support_label(db), sql, q.lastError().text()));
}

bool support_is_memory(const QSqlDatabase& db) {
    const QString name = db.databaseName();
    return name.isEmpty() || name.startsWith(QLatin1String(":memory:")) ||
           name.contains(QLatin1String("mode=memory"));
}

QString support_mb(qint64 bytes) {
    return QString::number(double(bytes) / (1024.0 * 1024.0), 'f', 1) + QStringLiteral(" MB");
}
} // namespace

// ── Statement summary (logs) ────────────────────────────────────────────────

namespace {
/// An identifier as written after INTO / FROM / TABLE, unquoted; empty when the
/// next token is not a plain identifier (a sub-query, a string, a placeholder).
QString support_identifier(const QString& sql, qsizetype from) {
    qsizetype i = from;
    while (i < sql.size() && sql.at(i).isSpace())
        ++i;
    if (i < sql.size() && (sql.at(i) == QLatin1Char('"') || sql.at(i) == QLatin1Char('`') ||
                           sql.at(i) == QLatin1Char('[')))
        ++i;
    QString out;
    while (i < sql.size() && out.size() < 64) {
        const QChar c = sql.at(i);
        const char16_t u = c.unicode();
        const bool ok = (u >= u'a' && u <= u'z') || (u >= u'A' && u <= u'Z') || (u >= u'0' && u <= u'9') ||
                        u == u'_' || u == u'.';
        if (!ok)
            break;
        out.append(c);
        ++i;
    }
    return out;
}

/// Position just after the first whole-word, case-insensitive `word` at or after `from`
/// outside quotes; -1 when absent.
qsizetype support_after_word(const QString& sql, const QString& word, qsizetype from = 0) {
    QChar quote;
    for (qsizetype i = from; i + word.size() <= sql.size(); ++i) {
        const QChar c = sql.at(i);
        if (!quote.isNull()) {
            if (c == quote)
                quote = QChar();
            continue;
        }
        if (c == QLatin1Char('\'') || c == QLatin1Char('"')) {
            quote = c;
            continue;
        }
        if (QStringView(sql).mid(i, word.size()).compare(word, Qt::CaseInsensitive) != 0)
            continue;
        const bool start_ok = i == 0 || !(sql.at(i - 1).isLetterOrNumber() || sql.at(i - 1) == QLatin1Char('_'));
        const qsizetype end = i + word.size();
        const bool end_ok = end >= sql.size() || !(sql.at(end).isLetterOrNumber() || sql.at(end) == QLatin1Char('_'));
        if (start_ok && end_ok)
            return end;
    }
    return -1;
}
} // namespace

QString statement_summary(const QString& sql) {
    // Skip whitespace and comments.
    qsizetype i = 0;
    while (i < sql.size()) {
        if (sql.at(i).isSpace()) {
            ++i;
        } else if (QStringView(sql).mid(i, 2) == QLatin1String("--")) {
            while (i < sql.size() && sql.at(i) != QLatin1Char('\n'))
                ++i;
        } else if (QStringView(sql).mid(i, 2) == QLatin1String("/*")) {
            const qsizetype end = sql.indexOf(QLatin1String("*/"), i + 2);
            i = end < 0 ? sql.size() : end + 2;
        } else {
            break;
        }
    }
    qsizetype k = i;
    while (k < sql.size() && sql.at(k).isLetter())
        ++k;
    const QString keyword = sql.mid(i, qMin<qsizetype>(k - i, 16)).toUpper();
    if (keyword.isEmpty())
        return QStringLiteral("?");

    QString table;
    auto ident_after = [&](const QString& word) {
        const qsizetype at = support_after_word(sql, word, k);
        return at < 0 ? QString() : support_identifier(sql, at);
    };
    if (keyword == QLatin1String("INSERT") || keyword == QLatin1String("REPLACE")) {
        table = ident_after(QStringLiteral("INTO"));
    } else if (keyword == QLatin1String("UPDATE")) {
        table = support_identifier(sql, k);
        if (table.compare(QLatin1String("OR"), Qt::CaseInsensitive) == 0) {
            // UPDATE OR <conflict> <table>
            qsizetype j = k;
            while (j < sql.size() && sql.at(j).isSpace())
                ++j;
            j += 2; // OR
            while (j < sql.size() && sql.at(j).isSpace())
                ++j;
            while (j < sql.size() && sql.at(j).isLetter())
                ++j; // the conflict clause
            table = support_identifier(sql, j);
        }
    } else if (keyword == QLatin1String("DELETE") || keyword == QLatin1String("SELECT") ||
               keyword == QLatin1String("WITH")) {
        table = ident_after(QStringLiteral("FROM"));
    } else if (keyword == QLatin1String("CREATE") || keyword == QLatin1String("DROP") ||
               keyword == QLatin1String("ALTER")) {
        for (const char* what : {"TABLE", "INDEX", "VIEW", "TRIGGER"}) {
            qsizetype at = support_after_word(sql, QString::fromLatin1(what), k);
            if (at < 0)
                continue;
            for (const char* skip : {"IF", "NOT", "EXISTS"}) {
                qsizetype j = at;
                while (j < sql.size() && sql.at(j).isSpace())
                    ++j;
                if (QStringView(sql).mid(j, qstrlen(skip)).compare(QLatin1String(skip), Qt::CaseInsensitive) == 0)
                    at = j + qstrlen(skip);
            }
            table = support_identifier(sql, at);
            break;
        }
    } else if (keyword == QLatin1String("PRAGMA")) {
        table = support_identifier(sql, k);
    }
    return table.isEmpty() ? keyword : keyword + QLatin1Char(' ') + table;
}

// ── Classification ──────────────────────────────────────────────────────────

SqlErrorKind classify_text(const QString& text) {
    const QString t = text.trimmed().toLower();
    if (t.isEmpty())
        return SqlErrorKind::None;
    // Order matters: "database table is locked" must not match the BUSY text
    // "database is locked", and "readonly" must win over the generic phrases.
    if (t.contains(QLatin1String("not a database")))
        return SqlErrorKind::NotADb;
    if (t.contains(QLatin1String("malformed")) || t.contains(QLatin1String("database corrupt")))
        return SqlErrorKind::Corrupt;
    if (t.contains(QLatin1String("table is locked")) || t.contains(QLatin1String("schema is locked")))
        return SqlErrorKind::Locked;
    if (t.contains(QLatin1String("database is locked")) || t.contains(QLatin1String("database is busy")))
        return SqlErrorKind::Busy;
    if (t.contains(QLatin1String("readonly database")) || t.contains(QLatin1String("read-only database")))
        return SqlErrorKind::ReadOnly;
    if (t.contains(QLatin1String("disk is full")) || t.contains(QLatin1String("not enough disk space")))
        return SqlErrorKind::Full;
    if (t.contains(QLatin1String("disk i/o error")))
        return SqlErrorKind::IoErr;
    if (t.contains(QLatin1String("unable to open database")) || t.contains(QLatin1String("access permission denied")))
        return SqlErrorKind::CantOpen;
    if (t.contains(QLatin1String("no such table")) || t.contains(QLatin1String("no such column")) ||
        t.contains(QLatin1String("has no column named")) || t.contains(QLatin1String("schema has changed")))
        return SqlErrorKind::Schema;
    if (t.contains(QLatin1String("constraint failed")))
        return SqlErrorKind::Constraint;
    return SqlErrorKind::Other;
}

SqlErrorKind classify(const QSqlError& error) {
    if (error.type() == QSqlError::NoError)
        return SqlErrorKind::None;

    bool numeric = false;
    const int native = error.nativeErrorCode().toInt(&numeric);
    if (numeric && native != 0) {
        switch (native & 0xFF) {
            case kSqliteBusy:
                return SqlErrorKind::Busy;
            case kSqliteLocked:
                return SqlErrorKind::Locked;
            case kSqliteCorrupt:
                return SqlErrorKind::Corrupt;
            case kSqliteNotADb:
                return SqlErrorKind::NotADb;
            case kSqliteReadOnly:
                return SqlErrorKind::ReadOnly;
            case kSqliteFull:
                return SqlErrorKind::Full;
            case kSqliteIoErr:
                return SqlErrorKind::IoErr;
            case kSqliteCantOpen:
            case kSqlitePerm:
                return SqlErrorKind::CantOpen;
            case kSqliteSchema:
                return SqlErrorKind::Schema;
            case kSqliteConstraint:
                return SqlErrorKind::Constraint;
            case kSqliteError:
            default:
                // SQLITE_ERROR is the generic code for "no such table", syntax
                // errors and the like — refine from SQLite's own message.
                break;
        }
    }
    const SqlErrorKind by_text = classify_text(error.databaseText() + QLatin1Char(' ') + error.driverText());
    return by_text == SqlErrorKind::None ? SqlErrorKind::Other : by_text;
}

const char* kind_name(SqlErrorKind kind) {
    switch (kind) {
        case SqlErrorKind::None:
            return "None";
        case SqlErrorKind::Busy:
            return "Busy";
        case SqlErrorKind::Locked:
            return "Locked";
        case SqlErrorKind::Corrupt:
            return "Corrupt";
        case SqlErrorKind::NotADb:
            return "NotADb";
        case SqlErrorKind::ReadOnly:
            return "ReadOnly";
        case SqlErrorKind::Full:
            return "Full";
        case SqlErrorKind::IoErr:
            return "IoErr";
        case SqlErrorKind::CantOpen:
            return "CantOpen";
        case SqlErrorKind::Schema:
            return "Schema";
        case SqlErrorKind::Constraint:
            return "Constraint";
        case SqlErrorKind::Other:
            return "Other";
    }
    return "Other";
}

QString user_message(SqlErrorKind kind, const QString& db_label) {
    const QString label = db_label.isEmpty() ? QStringLiteral("The database") : db_label;
    switch (kind) {
        case SqlErrorKind::None:
            return {};
        case SqlErrorKind::Busy:
        case SqlErrorKind::Locked:
            return QStringLiteral("%1 is being used by another task or program right now. "
                                  "Please try again in a moment.")
                .arg(label);
        case SqlErrorKind::Corrupt:
            return QStringLiteral("%1 is damaged and cannot be read reliably.").arg(label);
        case SqlErrorKind::NotADb:
            return QStringLiteral("%1 is not a valid database file.").arg(label);
        case SqlErrorKind::ReadOnly:
            return QStringLiteral("%1 cannot be changed. Check that its folder is not read-only and that "
                                  "your user account is allowed to write there.")
                .arg(label);
        case SqlErrorKind::Full:
            return QStringLiteral("There is not enough free disk space to save changes to %1. "
                                  "Free some space and try again.")
                .arg(label.left(1).toLower() + label.mid(1));
        case SqlErrorKind::IoErr:
            return QStringLiteral("The computer reported a disk read or write error while using %1.")
                .arg(label.left(1).toLower() + label.mid(1));
        case SqlErrorKind::CantOpen:
            return QStringLiteral("%1 could not be opened. Its folder may be missing, or your user account "
                                  "may not be allowed to use it.")
                .arg(label);
        case SqlErrorKind::Schema:
            return QStringLiteral("%1 does not have the layout this version expects. Restart the terminal; "
                                  "if this keeps happening, the details are in the log.")
                .arg(label);
        case SqlErrorKind::Constraint:
            return QStringLiteral("The change was not saved because it conflicts with data that is already stored.");
        case SqlErrorKind::Other:
            break;
    }
    return QStringLiteral("A storage error occurred in %1. The details are in the log.")
        .arg(label.left(1).toLower() + label.mid(1));
}

// ── PRAGMAs ─────────────────────────────────────────────────────────────────

QString apply_pragmas(QSqlDatabase& db, const PragmaProfile& profile, bool set_journal_mode) {
    // busy_timeout first, so every later statement here already waits for locks
    // instead of failing at once when another connection is mid-write.
    support_pragma(db, QStringLiteral("PRAGMA busy_timeout = %1").arg(profile.busy_timeout_ms));

    QString mode;
    if (set_journal_mode) {
        // PRAGMA journal_mode returns the mode now in effect. On a filesystem
        // without shared-memory support SQLite silently keeps the old mode, so
        // the requested value proves nothing — read the row.
        mode = support_scalar(db, QStringLiteral("PRAGMA journal_mode = WAL")).toLower();
        if (!mode.isEmpty() && mode != QLatin1String("wal") && !support_is_memory(db)) {
            LOG_WARN(kSupportTag, QString("WAL is not available for %1 (SQLite kept journal_mode=%2) — using "
                                   "journal_mode=TRUNCATE with synchronous=FULL. Network drives and some "
                                   "mounted filesystems cannot hold a WAL database safely.")
                               .arg(support_label(db), mode));
            const QString fallback = support_scalar(db, QStringLiteral("PRAGMA journal_mode = TRUNCATE")).toLower();
            if (!fallback.isEmpty())
                mode = fallback;
        }
    } else {
        mode = support_scalar(db, QStringLiteral("PRAGMA journal_mode"), /*log_failure=*/false).toLower();
    }

    // NORMAL is durable enough under WAL (a power cut can lose the last commits,
    // never corrupt). Without WAL it can corrupt, so use FULL there.
    const bool wal = (mode == QLatin1String("wal"));
    support_pragma(db, wal ? QStringLiteral("PRAGMA synchronous = NORMAL") : QStringLiteral("PRAGMA synchronous = FULL"));
    support_pragma(db, QStringLiteral("PRAGMA cache_size = -%1").arg(profile.cache_kb));
    support_pragma(db, QStringLiteral("PRAGMA temp_store = MEMORY"));
    support_pragma(db, QStringLiteral("PRAGMA mmap_size = 0"));
    support_pragma(db, profile.foreign_keys ? QStringLiteral("PRAGMA foreign_keys = ON")
                                        : QStringLiteral("PRAGMA foreign_keys = OFF"));
    support_pragma(db, QStringLiteral("PRAGMA journal_size_limit = %1").arg(profile.journal_size_limit));
    if (wal)
        support_pragma(db, QStringLiteral("PRAGMA wal_autocheckpoint = 1000"));
    return mode;
}

// ── Integrity / maintenance ─────────────────────────────────────────────────

Result<void> quick_check(QSqlDatabase& db, SqlErrorKind* kind) {
    auto set_kind = [kind](SqlErrorKind k) {
        if (kind)
            *kind = k;
    };
    set_kind(SqlErrorKind::None);
    if (!db.isOpen()) {
        set_kind(SqlErrorKind::CantOpen);
        return Result<void>::err("quick_check: connection is not open");
    }
    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("PRAGMA quick_check(1)"))) {
        const SqlErrorKind k = classify(q.lastError());
        set_kind(k);
        return Result<void>::err(QString("quick_check could not run on %1 (%2): %3")
                                     .arg(support_label(db), QString::fromLatin1(kind_name(k)), q.lastError().text())
                                     .toStdString());
    }
    if (!q.next()) {
        set_kind(SqlErrorKind::Other);
        return Result<void>::err("quick_check returned no rows");
    }
    const QString verdict = q.value(0).toString();
    if (verdict.compare(QLatin1String("ok"), Qt::CaseInsensitive) == 0)
        return Result<void>::ok();
    set_kind(SqlErrorKind::Corrupt);
    return Result<void>::err(QString("quick_check failed on %1: %2").arg(support_label(db), verdict).toStdString());
}

void checkpoint_truncate(QSqlDatabase& db) {
    if (!db.isOpen())
        return;
    const QString mode = support_scalar(db, QStringLiteral("PRAGMA journal_mode"), /*log_failure=*/false).toLower();
    if (mode != QLatin1String("wal"))
        return;
    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("PRAGMA wal_checkpoint(TRUNCATE)"))) {
        LOG_WARN(kSupportTag, QString("wal_checkpoint(TRUNCATE) failed on %1: %2").arg(support_label(db), q.lastError().text()));
        return;
    }
    // Row: (busy, wal pages, pages checkpointed). busy=1 means another
    // connection was reading or writing and the WAL could not be reset.
    if (q.next() && q.value(0).toInt() != 0) {
        LOG_INFO(kSupportTag, QString("wal_checkpoint(TRUNCATE) on %1 incomplete (database busy: %2 of %3 pages)")
                           .arg(support_label(db))
                           .arg(q.value(2).toInt())
                           .arg(q.value(1).toInt()));
    }
}

Result<void> backup_to(QSqlDatabase& db, const QString& path, SqlErrorKind* kind) {
    auto fail = [kind](SqlErrorKind k, const QString& why) {
        if (kind)
            *kind = k;
        LOG_WARN(kSupportTag, QStringLiteral("Backup failed kind=%1 error=\"%2\"")
                                  .arg(QLatin1String(kind_name(k)), privacy::redact_text(why).left(300)));
        return Result<void>::err(why.toStdString());
    };
    if (kind)
        *kind = SqlErrorKind::None;
    if (!db.isOpen())
        return fail(SqlErrorKind::CantOpen, QStringLiteral("database connection is not open"));
    if (path.isEmpty())
        return fail(SqlErrorKind::CantOpen, QStringLiteral("no backup path given"));

    const QFileInfo target(path);
    const QString dir = target.absolutePath();
    if (!QDir(dir).exists())
        return fail(SqlErrorKind::CantOpen, QStringLiteral("backup folder does not exist: %1").arg(dir));

    // VACUUM INTO writes the live pages only (freelist excluded). Require that
    // plus a margin, so a backup never fills the disk the live database needs.
    const qint64 page_size = support_scalar(db, QStringLiteral("PRAGMA page_size")).toLongLong();
    const qint64 page_count = support_scalar(db, QStringLiteral("PRAGMA page_count")).toLongLong();
    const qint64 free_pages = support_scalar(db, QStringLiteral("PRAGMA freelist_count")).toLongLong();
    const qint64 needed = qMax<qint64>(0, page_count - free_pages) * page_size;
    const QStorageInfo storage(dir);
    if (storage.isValid() && storage.isReady() && storage.bytesAvailable() >= 0) {
        const qint64 margin = qMax<qint64>(16LL * 1024 * 1024, needed / 10);
        if (storage.bytesAvailable() < needed + margin) {
            return fail(SqlErrorKind::Full, QStringLiteral("not enough disk space for a backup of %1 in %2: needs about "
                                                           "%3, %4 free")
                                                .arg(support_label(db), dir, support_mb(needed + margin),
                                                     support_mb(storage.bytesAvailable())));
        }
    }

    const QString partial = path + QStringLiteral(".partial");
    QFile::remove(partial); // VACUUM INTO refuses to overwrite; a stale partial is garbage
    QString literal = QDir::toNativeSeparators(partial);
    literal.replace(QLatin1Char('\''), QStringLiteral("''"));
    {
        QSqlQuery q(db);
        if (!q.exec(QStringLiteral("VACUUM INTO '%1'").arg(literal))) {
            const QSqlError e = q.lastError();
            QFile::remove(partial);
            return fail(classify(e), QStringLiteral("VACUUM INTO %1 failed: %2").arg(partial, e.text()));
        }
    }
    if (QFile::exists(path) && !QFile::remove(path)) {
        QFile::remove(partial);
        return fail(SqlErrorKind::CantOpen, QStringLiteral("cannot replace existing backup %1").arg(path));
    }
    if (!QFile::rename(partial, path)) {
        QFile::remove(partial);
        return fail(SqlErrorKind::CantOpen, QStringLiteral("cannot rename %1 to %2").arg(partial, path));
    }
    LOG_INFO(kSupportTag, QString("Database backed up db=%1 backup=%2 size=\"%3\"")
                              .arg(support_label(db), privacy::redact_home(path), support_mb(QFileInfo(path).size())));
    return Result<void>::ok();
}

bool has_fts5(QSqlDatabase& db) {
    if (!db.isOpen())
        return false;
    QSqlQuery q(db);
    // TEMP schema: the probe never touches the database file itself.
    if (!q.exec(QStringLiteral("CREATE VIRTUAL TABLE IF NOT EXISTS temp.fincept_fts5_probe USING fts5(x)")))
        return false;
    q.exec(QStringLiteral("DROP TABLE IF EXISTS temp.fincept_fts5_probe"));
    return true;
}

QString sqlite_version(QSqlDatabase& db) {
    if (!db.isOpen())
        return {};
    return support_scalar(db, QStringLiteral("SELECT sqlite_version()"), /*log_failure=*/false);
}

} // namespace fincept::db
