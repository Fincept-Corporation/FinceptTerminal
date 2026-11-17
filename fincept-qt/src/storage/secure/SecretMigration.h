#pragma once
// SecretMigration — the one rule for moving a secret out of an old place (a
// plaintext settings row, a legacy column, an older slot) into SecureStorage,
// and for replacing one stored copy of a key with another:
//
//     write the new copy -> read it back and compare -> only then delete the old one.
//
// On any failure the old copy stays where it is. It is also the retry marker:
// the next start finds it again and repeats the move. A store that could not be
// READ is never taken for "nothing stored" — nothing is written over it and the
// old copy is not deleted on its strength.
//
// Header-only, Qt Core only, no logging (tests/tst_secret_migration.cpp covers
// every branch). Callers log the outcome once per session (LOG_TRANSITION keyed
// by the site) and never log a value.

#include "core/result/Result.h"
#include "storage/secure/SecretRead.h"

#include <QByteArray>
#include <QString>

#include <functional>

namespace fincept::secret_migration {

/// The old copy as it was read from its old place.
struct OldCopy {
    enum class State { Absent, Present, Unreadable };
    State state = State::Absent;
    QString value;  ///< Present only, never empty
    QString detail; ///< Unreadable only: why (for the log — never a value)

    static OldCopy absent() { return {}; }
    /// An empty value is "absent": there is nothing to move.
    static OldCopy present(const QString& v) {
        OldCopy c;
        if (!v.isEmpty()) {
            c.state = State::Present;
            c.value = v;
        }
        return c;
    }
    static OldCopy unreadable(const QString& why = {}) {
        OldCopy c;
        c.state = State::Unreadable;
        c.detail = why;
        return c;
    }
    bool is_present() const { return state == State::Present; }
};

/// What to do when the store already holds a DIFFERENT value under the key.
enum class OnConflict {
    /// Neither copy is changed or deleted; the caller decides which one is used
    /// (the API key: the server is asked). The old copy stays until then.
    KeepBoth,
    /// The old copy is the newer one — the site only ever writes the old place
    /// when the store failed (a "column wins" repository) — so it replaces the
    /// stored value, through the same write -> read back -> compare.
    OldWins,
};

enum class Action {
    Nothing,   ///< no old copy: nothing to move (or it was moved earlier)
    WriteNew,  ///< write the old value into the store, read it back, compare, then delete the old copy
    DeleteOld, ///< the store already holds the same value (an earlier run stopped before its delete)
    KeepBoth,  ///< a conflict under OnConflict::KeepBoth: touch nothing
    Retry,     ///< the old copy or the store could not be read: touch nothing, try again next start
};

enum class Failure {
    None,
    OldUnreadable,    ///< the old copy could not be read
    StoreUnreadable,  ///< the store could not be read before the write
    WriteFailed,      ///< the write returned an error
    ReadBackFailed,   ///< the write reported success but the value could not be read back
    ReadBackMismatch, ///< the value read back differs from the one written
    DeleteFailed,     ///< the new copy is verified, the old one could not be deleted (both hold it)
    Conflict,         ///< the store holds a different value (OnConflict::KeepBoth)
};

/// Stable short name for logs.
inline const char* failure_name(Failure f) {
    switch (f) {
        case Failure::None:
            return "none";
        case Failure::OldUnreadable:
            return "old_copy_unreadable";
        case Failure::StoreUnreadable:
            return "store_unreadable";
        case Failure::WriteFailed:
            return "write_failed";
        case Failure::ReadBackFailed:
            return "read_back_failed";
        case Failure::ReadBackMismatch:
            return "read_back_mismatch";
        case Failure::DeleteFailed:
            return "delete_failed";
        case Failure::Conflict:
            return "conflict";
    }
    return "none";
}

/// The decision, from the old copy and what the store holds now. A stored empty
/// string counts as "nothing stored".
inline Action decide(const OldCopy& old, const SecretRead& stored, OnConflict on_conflict) {
    if (old.state == OldCopy::State::Absent)
        return Action::Nothing;
    if (old.state == OldCopy::State::Unreadable)
        return Action::Retry;
    if (stored.failed())
        return Action::Retry; // unreadable is never "not set": no write over it, no delete
    if (stored.found() && !stored.value.isEmpty()) {
        if (stored.value == old.value)
            return Action::DeleteOld;
        return on_conflict == OnConflict::OldWins ? Action::WriteNew : Action::KeepBoth;
    }
    return Action::WriteNew;
}

/// After a write: Failure::None only when the write succeeded AND the value read
/// back equals the one written — the only state in which an old copy may go.
inline Failure verify_written(const Result<void>& write, const SecretRead& read_back, const QString& expected) {
    if (write.is_err())
        return Failure::WriteFailed;
    if (!read_back.found())
        return Failure::ReadBackFailed;
    if (read_back.value != expected)
        return Failure::ReadBackMismatch;
    return Failure::None;
}

/// The three operations of one move. `delete_old` deletes (or blanks) the old copy.
struct Io {
    std::function<SecretRead()> read_stored;
    std::function<Result<void>(const QString&)> write_stored;
    std::function<Result<void>()> delete_old;
};

struct Outcome {
    Action action = Action::Nothing;
    Failure failure = Failure::None;
    QString detail;           ///< error text of the failing step (never a value)
    bool stored_verified = false; ///< the store holds the old value, read back and compared, after this run
    bool old_deleted = false;

    /// The old copy is still in place: the move is tried again at the next start.
    bool pending() const { return failure != Failure::None; }
    /// Something was moved or cleaned up by this run.
    bool moved() const { return old_deleted; }
};

/// Run one move: read the store, decide, write + read back + compare, delete the
/// old copy. Stops at the first failure with the old copy untouched (a failed
/// delete leaves both copies holding the same value).
inline Outcome run(const OldCopy& old, OnConflict on_conflict, const Io& io) {
    Outcome o;
    if (old.state == OldCopy::State::Absent)
        return o;
    if (old.state == OldCopy::State::Unreadable) {
        o.action = Action::Retry;
        o.failure = Failure::OldUnreadable;
        o.detail = old.detail;
        return o;
    }
    const SecretRead stored = io.read_stored();
    o.action = decide(old, stored, on_conflict);
    switch (o.action) {
        case Action::Nothing:
            return o;
        case Action::Retry:
            o.failure = Failure::StoreUnreadable;
            o.detail = QString::fromLatin1(stored.error_name()) +
                       (stored.detail.isEmpty() ? QString() : QStringLiteral(": ") + stored.detail);
            return o;
        case Action::KeepBoth:
            o.failure = Failure::Conflict;
            return o;
        case Action::WriteNew: {
            const Result<void> w = io.write_stored(old.value);
            const SecretRead back = w.is_ok() ? io.read_stored() : SecretRead::not_found();
            o.failure = verify_written(w, back, old.value);
            if (o.failure != Failure::None) {
                if (w.is_err())
                    o.detail = QString::fromStdString(w.error());
                else if (back.failed())
                    o.detail = QString::fromLatin1(back.error_name()) +
                               (back.detail.isEmpty() ? QString() : QStringLiteral(": ") + back.detail);
                return o;
            }
            o.stored_verified = true;
            break;
        }
        case Action::DeleteOld:
            o.stored_verified = true;
            break;
    }
    const Result<void> d = io.delete_old();
    if (d.is_err()) {
        o.failure = Failure::DeleteFailed;
        o.detail = QString::fromStdString(d.error());
        return o;
    }
    o.old_deleted = true;
    return o;
}

/// Later in the session: may an old copy that stayed behind (a failed move or a
/// conflict) be deleted now?
///   equal to the value in use  -> only when the store was just verified to hold it;
///   different from it          -> only when the caller knows it is superseded
///                                  (the API key in use was accepted by the server
///                                  and is itself safely stored).
inline bool may_retire_copy(const QString& copy, const QString& in_use, bool in_use_stored_verified,
                            bool copy_superseded) {
    if (copy.isEmpty())
        return false;
    if (copy == in_use)
        return in_use_stored_verified;
    return copy_superseded;
}

/// The wrapped-key upgrade (SecureStorageSqlite): an older wrapping of the data
/// key may be dropped only when the new wrapping was read back, unwraps, and
/// yields the SAME key as the old one. A copy that cannot be unwrapped here, or
/// that wraps a different key, is kept (rows encrypted under it would otherwise
/// be lost for good).
inline bool may_drop_old_key_wrapping(bool new_unwrapped, const QByteArray& new_key, bool old_unwrapped,
                                      const QByteArray& old_key) {
    return new_unwrapped && old_unwrapped && !new_key.isEmpty() && new_key == old_key;
}

} // namespace fincept::secret_migration
