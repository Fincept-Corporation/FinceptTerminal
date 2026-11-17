#pragma once
// SqliteSupport — small shared helpers for every SQLite file the terminal opens
// (fincept.db, cache.db, workspace.db, layouts/_index.db).
//
// Everything here works on a QSqlDatabase the CALLER owns and that belongs to the
// calling thread; nothing is cached and nothing is opened behind the caller's back.
// Raw driver text is for the log only: show users `user_message()` instead.
#include "core/result/Result.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QString>

namespace fincept::db {

/// What went wrong, independent of the driver's wording. Derived from the SQLite
/// primary result code (QSqlError::nativeErrorCode() & 0xFF), with a fallback on
/// SQLite's fixed English messages when the driver supplied no code.
enum class SqlErrorKind {
    None,       ///< not an error
    Busy,       ///< SQLITE_BUSY: another connection or process holds the lock
    Locked,     ///< SQLITE_LOCKED: a conflicting lock inside this process
    Corrupt,    ///< SQLITE_CORRUPT: the file is damaged
    NotADb,     ///< SQLITE_NOTADB: the file is not an SQLite database at all
    ReadOnly,   ///< SQLITE_READONLY: the file or its folder cannot be written
    Full,       ///< SQLITE_FULL: disk (or quota) full
    IoErr,      ///< SQLITE_IOERR: the operating system reported a read/write error
    CantOpen,   ///< SQLITE_CANTOPEN / SQLITE_PERM: missing folder, no permission, bad path
    Schema,     ///< schema mismatch: SQLITE_SCHEMA, "no such table", "no such column"
    Constraint, ///< SQLITE_CONSTRAINT: UNIQUE / NOT NULL / FOREIGN KEY / CHECK
    Other,      ///< anything else (driver not loaded, out of memory, syntax error, ...)
};

/// Classify a driver error. QSqlError with type NoError gives None.
SqlErrorKind classify(const QSqlError& error);

/// Classify from text only (e.g. the std::string carried by a Result error).
/// Recognises SQLite's own fixed messages; anything else is Other.
SqlErrorKind classify_text(const QString& text);

/// Stable short name for logs ("Busy", "NotADb", ...).
const char* kind_name(SqlErrorKind kind);

/// The statement's first keyword and the table it names, for logs: "INSERT settings",
/// "SELECT watchlist_items", "UPDATE OR REPLACE portfolios" -> "UPDATE portfolios",
/// "PRAGMA journal_mode", "COMMIT". Never contains a literal or a bound value — only
/// the keyword and an identifier made of [A-Za-z0-9_.] (at most 64 characters).
/// Leading comments and whitespace are skipped; an empty or unparsable statement
/// gives "?".
QString statement_summary(const QString& sql);

/// One or two plain-language sentences for a dialog or status line. `db_label`
/// names the store in user terms, e.g. "The terminal database" or "The cache".
/// Never contains driver text.
QString user_message(SqlErrorKind kind, const QString& db_label);

/// Per-connection settings. mmap is always off (mmap_size=0): an I/O error under
/// mmap is a SIGBUS crash instead of an error code, and on Windows a mapped file
/// cannot be truncated or renamed.
struct PragmaProfile {
    int cache_kb = 8000;                         ///< page cache size in KiB (cache_size = -cache_kb)
    int busy_timeout_ms = 5000;                  ///< how long a statement waits for a lock
    bool foreign_keys = true;                    ///< PRAGMA foreign_keys
    qint64 journal_size_limit = 64LL * 1024 * 1024; ///< bytes the -wal is truncated to after a checkpoint
};

/// Apply the connection PRAGMAs. With `set_journal_mode` (once, on the owning
/// connection) it also requests WAL and READS BACK the mode SQLite actually chose;
/// when WAL is unavailable (network share, some FUSE mounts) it falls back to
/// journal_mode=TRUNCATE with synchronous=FULL and logs that once. Without it, the
/// current mode is read and synchronous is matched to it (clones).
/// Always sets synchronous, cache_size, temp_store=MEMORY, mmap_size=0,
/// foreign_keys, busy_timeout, journal_size_limit, wal_autocheckpoint=1000.
/// Returns the journal mode in effect in lower case ("wal", "truncate", "delete",
/// "memory", ...), or an empty string if the file could not be read at all (the
/// caller's next statement will report why). Individual PRAGMA failures are logged.
QString apply_pragmas(QSqlDatabase& db, const PragmaProfile& profile, bool set_journal_mode);

/// `PRAGMA quick_check(1)`. ok() when the file is intact. On failure the error
/// text is for the log; `kind` (optional) receives Corrupt for a failed check or
/// the classified driver error when the check could not run (NotADb, IoErr, ...).
Result<void> quick_check(QSqlDatabase& db, SqlErrorKind* kind = nullptr);

/// `PRAGMA wal_checkpoint(TRUNCATE)` — folds the -wal back into the main file and
/// truncates it. Best effort: a busy or failed checkpoint is logged, never fatal.
/// No-op when the connection is not open or not in WAL mode.
void checkpoint_truncate(QSqlDatabase& db);

/// Consistent snapshot of an OPEN database to `path` via `VACUUM INTO` (safe while
/// other connections read and write; includes committed WAL content). Checks free
/// space first and fails with "not enough disk space" (kind Full) rather than
/// writing a partial copy. Writes `<path>.partial` and renames it over `path` only
/// when complete, so an existing file at `path` survives a failed backup.
/// May take seconds on a large database: call it off the GUI thread when you can,
/// using that thread's own connection (Database::connection()).
Result<void> backup_to(QSqlDatabase& db, const QString& path, SqlErrorKind* kind = nullptr);

/// True when the SQLite library behind `db` has the FTS5 extension. Probes with a
/// TEMP virtual table, so the database file itself is never written.
bool has_fts5(QSqlDatabase& db);

/// sqlite_version() of the library behind `db` ("3.45.1"), or empty on failure.
QString sqlite_version(QSqlDatabase& db);

} // namespace fincept::db
