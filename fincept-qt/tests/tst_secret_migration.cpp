// storage/secure/SecretMigration.h — the rule for moving a secret out of an old
// place into SecureStorage (write -> read back -> compare -> only then delete the
// old copy) — and SecureStorage::store_verified() over a FAKE backend.
//
// Every branch: write fails, read-back mismatch, read-back unreadable, store
// unreadable before the write, old copy unreadable, already migrated, both
// present and different (keep both / old wins), delete fails, a crash between
// the write and the delete (re-run), a failed move retried after the store
// recovers; the later-in-session retire rule; the wrapped-key drop rule.
//
// Links the SecureStorage facade + the logger; this file supplies
// make_default_secret_backend() (the SQLite backend is not linked).
#include "storage/secure/SecretMigration.h"
#include "storage/secure/SecureStorage.h"

#include <QHash>
#include <QtTest>

using namespace fincept;
namespace sm = fincept::secret_migration;

namespace {

/// An in-memory "new place" with injectable failures, plus the old copy.
struct Sim {
    QHash<QString, QString> store;   ///< the new place (one key used: "k")
    bool read_fails = false;          ///< every read of the store fails
    int read_fails_after_writes = -1; ///< reads fail once this many writes happened (-1 = never)
    bool write_fails = false;
    bool write_garbles = false;       ///< the write "succeeds" but stores another value
    bool delete_fails = false;
    int writes = 0;
    int reads = 0;
    int deletes = 0;
    QString old_value;                ///< the old copy ("" = deleted)

    SecretRead read() {
        ++reads;
        if (read_fails || (read_fails_after_writes >= 0 && writes >= read_fails_after_writes))
            return SecretRead::failure(SecretRead::Error::Busy, QStringLiteral("database is locked"));
        if (!store.contains(QStringLiteral("k")))
            return SecretRead::not_found();
        return SecretRead::found_value(store.value(QStringLiteral("k")));
    }
    sm::Io io() {
        sm::Io io;
        io.read_stored = [this]() { return read(); };
        io.write_stored = [this](const QString& v) {
            ++writes;
            if (write_fails)
                return Result<void>::err("Failed to write credential row");
            store.insert(QStringLiteral("k"), write_garbles ? v + QStringLiteral("~") : v);
            return Result<void>::ok();
        };
        io.delete_old = [this]() {
            ++deletes;
            if (delete_fails)
                return Result<void>::err("settings row could not be removed");
            old_value.clear();
            return Result<void>::ok();
        };
        return io;
    }
    sm::OldCopy old() const { return sm::OldCopy::present(old_value); }
};

/// SecureStorage backend for store_verified().
class FakeBackend : public ISecretBackend {
  public:
    QHash<QString, QString> values;
    bool write_fails = false;
    bool write_garbles = false;
    bool read_fails = false;

    void reset() {
        values.clear();
        write_fails = write_garbles = read_fails = false;
    }

    SecretRead read(const QString& key) override {
        if (read_fails)
            return SecretRead::failure(SecretRead::Error::Io, QStringLiteral("disk I/O error"));
        return values.contains(key) ? SecretRead::found_value(values.value(key)) : SecretRead::not_found();
    }
    Result<void> write(const QString& key, const QString& value) override {
        if (write_fails)
            return Result<void>::err("Failed to write credential row");
        values.insert(key, write_garbles ? QStringLiteral("garbled") : value);
        return Result<void>::ok();
    }
    Result<void> erase(const QString& key) override {
        values.remove(key);
        return Result<void>::ok();
    }
    Result<void> erase_all() override {
        values.clear();
        return Result<void>::ok();
    }
};

FakeBackend* g_backend = nullptr;

} // namespace

std::unique_ptr<ISecretBackend> fincept::make_default_secret_backend() {
    auto f = std::make_unique<FakeBackend>();
    g_backend = f.get();
    return f;
}

class TestSecretMigration : public QObject {
    Q_OBJECT
  private slots:
    // ── decide() ────────────────────────────────────────────────────────────
    void decide_table() {
        const auto present = sm::OldCopy::present(QStringLiteral("v1"));
        const auto nf = SecretRead::not_found();
        const auto same = SecretRead::found_value(QStringLiteral("v1"));
        const auto other = SecretRead::found_value(QStringLiteral("v2"));
        const auto empty = SecretRead::found_value(QString());

        QCOMPARE(sm::decide(sm::OldCopy::absent(), nf, sm::OnConflict::KeepBoth), sm::Action::Nothing);
        QCOMPARE(sm::decide(sm::OldCopy::present(QString()), same, sm::OnConflict::KeepBoth), sm::Action::Nothing);
        QCOMPARE(sm::decide(sm::OldCopy::unreadable(), nf, sm::OnConflict::OldWins), sm::Action::Retry);
        QCOMPARE(sm::decide(present, nf, sm::OnConflict::KeepBoth), sm::Action::WriteNew);
        QCOMPARE(sm::decide(present, empty, sm::OnConflict::KeepBoth), sm::Action::WriteNew);
        QCOMPARE(sm::decide(present, same, sm::OnConflict::KeepBoth), sm::Action::DeleteOld);
        QCOMPARE(sm::decide(present, same, sm::OnConflict::OldWins), sm::Action::DeleteOld);
        QCOMPARE(sm::decide(present, other, sm::OnConflict::KeepBoth), sm::Action::KeepBoth);
        QCOMPARE(sm::decide(present, other, sm::OnConflict::OldWins), sm::Action::WriteNew);
    }

    void unreadable_store_is_never_not_set() {
        const auto present = sm::OldCopy::present(QStringLiteral("v1"));
        for (auto e : {SecretRead::Error::StoreClosed, SecretRead::Error::Busy, SecretRead::Error::Io,
                       SecretRead::Error::KeyUnavailable, SecretRead::Error::Corrupt, SecretRead::Error::Crypto}) {
            QCOMPARE(sm::decide(present, SecretRead::failure(e), sm::OnConflict::KeepBoth), sm::Action::Retry);
            QCOMPARE(sm::decide(present, SecretRead::failure(e), sm::OnConflict::OldWins), sm::Action::Retry);
        }
    }

    // ── verify_written() ────────────────────────────────────────────────────
    void verify_written_table() {
        const QString v = QStringLiteral("v1");
        QCOMPARE(sm::verify_written(Result<void>::err("x"), SecretRead::found_value(v), v), sm::Failure::WriteFailed);
        QCOMPARE(sm::verify_written(Result<void>::ok(), SecretRead::not_found(), v), sm::Failure::ReadBackFailed);
        QCOMPARE(sm::verify_written(Result<void>::ok(), SecretRead::failure(SecretRead::Error::Busy), v),
                 sm::Failure::ReadBackFailed);
        QCOMPARE(sm::verify_written(Result<void>::ok(), SecretRead::found_value(QStringLiteral("v2")), v),
                 sm::Failure::ReadBackMismatch);
        QCOMPARE(sm::verify_written(Result<void>::ok(), SecretRead::found_value(v), v), sm::Failure::None);
    }

    // ── run() ───────────────────────────────────────────────────────────────
    void moves_and_deletes_when_verified() {
        Sim s;
        s.old_value = QStringLiteral("secret");
        const auto o = sm::run(s.old(), sm::OnConflict::KeepBoth, s.io());
        QCOMPARE(o.action, sm::Action::WriteNew);
        QCOMPARE(o.failure, sm::Failure::None);
        QVERIFY(o.stored_verified && o.old_deleted && o.moved() && !o.pending());
        QCOMPARE(s.store.value(QStringLiteral("k")), QStringLiteral("secret"));
        QVERIFY(s.old_value.isEmpty());
        QCOMPARE(s.writes, 1);
        QCOMPARE(s.deletes, 1);
    }

    void write_fails_keeps_old() {
        Sim s;
        s.old_value = QStringLiteral("secret");
        s.write_fails = true;
        const auto o = sm::run(s.old(), sm::OnConflict::KeepBoth, s.io());
        QCOMPARE(o.failure, sm::Failure::WriteFailed);
        QVERIFY(o.pending() && !o.old_deleted && !o.stored_verified);
        QCOMPARE(s.deletes, 0);
        QCOMPARE(s.old_value, QStringLiteral("secret"));
        QVERIFY(!o.detail.isEmpty());
    }

    void read_back_mismatch_keeps_old() {
        Sim s;
        s.old_value = QStringLiteral("secret");
        s.write_garbles = true;
        const auto o = sm::run(s.old(), sm::OnConflict::KeepBoth, s.io());
        QCOMPARE(o.failure, sm::Failure::ReadBackMismatch);
        QCOMPARE(s.deletes, 0);
        QCOMPARE(s.old_value, QStringLiteral("secret"));
    }

    void read_back_unreadable_keeps_old() {
        Sim s;
        s.old_value = QStringLiteral("secret");
        s.read_fails_after_writes = 1; // the read before the write works, the read-back does not
        const auto o = sm::run(s.old(), sm::OnConflict::KeepBoth, s.io());
        QCOMPARE(o.failure, sm::Failure::ReadBackFailed);
        QCOMPARE(s.deletes, 0);
        QCOMPARE(s.old_value, QStringLiteral("secret"));
        QVERIFY(o.detail.startsWith(QStringLiteral("busy")));
    }

    void unreadable_store_no_write_no_delete() {
        Sim s;
        s.old_value = QStringLiteral("secret");
        s.store.insert(QStringLiteral("k"), QStringLiteral("newer")); // there, but cannot be read
        s.read_fails = true;
        const auto o = sm::run(s.old(), sm::OnConflict::OldWins, s.io());
        QCOMPARE(o.action, sm::Action::Retry);
        QCOMPARE(o.failure, sm::Failure::StoreUnreadable);
        QCOMPARE(s.writes, 0);
        QCOMPARE(s.deletes, 0);
        QCOMPARE(s.store.value(QStringLiteral("k")), QStringLiteral("newer"));
        QCOMPARE(s.old_value, QStringLiteral("secret"));
    }

    void old_copy_unreadable_touches_nothing() {
        Sim s;
        const auto o = sm::run(sm::OldCopy::unreadable(QStringLiteral("settings read failed")),
                               sm::OnConflict::KeepBoth, s.io());
        QCOMPARE(o.failure, sm::Failure::OldUnreadable);
        QCOMPARE(s.reads, 0);
        QCOMPARE(s.writes, 0);
        QCOMPARE(s.deletes, 0);
        QCOMPARE(o.detail, QStringLiteral("settings read failed"));
    }

    void nothing_to_move() {
        Sim s;
        const auto o = sm::run(sm::OldCopy::absent(), sm::OnConflict::KeepBoth, s.io());
        QCOMPARE(o.action, sm::Action::Nothing);
        QVERIFY(!o.pending());
        QCOMPARE(s.reads + s.writes + s.deletes, 0);
    }

    void already_migrated_only_deletes_old() {
        Sim s;
        s.old_value = QStringLiteral("secret");
        s.store.insert(QStringLiteral("k"), QStringLiteral("secret"));
        const auto o = sm::run(s.old(), sm::OnConflict::KeepBoth, s.io());
        QCOMPARE(o.action, sm::Action::DeleteOld);
        QVERIFY(o.stored_verified && o.old_deleted);
        QCOMPARE(s.writes, 0);
        QVERIFY(s.old_value.isEmpty());
    }

    void both_present_and_different_keep_both() {
        Sim s;
        s.old_value = QStringLiteral("older");
        s.store.insert(QStringLiteral("k"), QStringLiteral("stored"));
        const auto o = sm::run(s.old(), sm::OnConflict::KeepBoth, s.io());
        QCOMPARE(o.action, sm::Action::KeepBoth);
        QCOMPARE(o.failure, sm::Failure::Conflict);
        QCOMPARE(s.writes, 0);
        QCOMPARE(s.deletes, 0);
        QCOMPARE(s.store.value(QStringLiteral("k")), QStringLiteral("stored"));
        QCOMPARE(s.old_value, QStringLiteral("older"));
    }

    void both_present_and_different_old_wins() {
        Sim s;
        s.old_value = QStringLiteral("newer-in-column");
        s.store.insert(QStringLiteral("k"), QStringLiteral("stale"));
        const auto o = sm::run(s.old(), sm::OnConflict::OldWins, s.io());
        QCOMPARE(o.action, sm::Action::WriteNew);
        QVERIFY(o.old_deleted);
        QCOMPARE(s.store.value(QStringLiteral("k")), QStringLiteral("newer-in-column"));
        // ...and a failed overwrite keeps the column copy.
        Sim f;
        f.old_value = QStringLiteral("newer-in-column");
        f.store.insert(QStringLiteral("k"), QStringLiteral("stale"));
        f.write_fails = true;
        const auto fo = sm::run(f.old(), sm::OnConflict::OldWins, f.io());
        QCOMPARE(fo.failure, sm::Failure::WriteFailed);
        QCOMPARE(f.old_value, QStringLiteral("newer-in-column"));
    }

    void delete_fails_keeps_both_equal() {
        Sim s;
        s.old_value = QStringLiteral("secret");
        s.delete_fails = true;
        const auto o = sm::run(s.old(), sm::OnConflict::KeepBoth, s.io());
        QCOMPARE(o.failure, sm::Failure::DeleteFailed);
        QVERIFY(o.stored_verified && !o.old_deleted && o.pending());
        QCOMPARE(s.store.value(QStringLiteral("k")), QStringLiteral("secret"));
        QCOMPARE(s.old_value, QStringLiteral("secret"));
    }

    void crash_between_write_and_delete_then_rerun() {
        // Run 1 "crashes" after the verified write: the delete never happens.
        Sim s;
        s.old_value = QStringLiteral("secret");
        sm::Io crashing = s.io();
        bool delete_reached = false;
        crashing.delete_old = [&delete_reached]() {
            delete_reached = true; // the process dies here: nothing is deleted
            return Result<void>::err("process terminated");
        };
        (void)sm::run(s.old(), sm::OnConflict::KeepBoth, crashing);
        QVERIFY(delete_reached);
        QCOMPARE(s.old_value, QStringLiteral("secret"));
        QCOMPARE(s.store.value(QStringLiteral("k")), QStringLiteral("secret"));
        const int writes_before = s.writes;

        // Next start: the old copy is found again (it is the retry marker).
        const auto o = sm::run(s.old(), sm::OnConflict::KeepBoth, s.io());
        QCOMPARE(o.action, sm::Action::DeleteOld);
        QVERIFY(o.old_deleted);
        QCOMPARE(s.writes, writes_before); // no second write
        QVERIFY(s.old_value.isEmpty());

        // A third start has nothing to do.
        const auto again = sm::run(s.old(), sm::OnConflict::KeepBoth, s.io());
        QCOMPARE(again.action, sm::Action::Nothing);
    }

    void failed_move_is_retried_next_start() {
        Sim s;
        s.old_value = QStringLiteral("secret");
        s.write_fails = true;
        QVERIFY(sm::run(s.old(), sm::OnConflict::KeepBoth, s.io()).pending());
        s.write_fails = false; // the store works again
        const auto o = sm::run(s.old(), sm::OnConflict::KeepBoth, s.io());
        QVERIFY(o.old_deleted);
        QCOMPARE(s.store.value(QStringLiteral("k")), QStringLiteral("secret"));
    }

    void failure_names_are_stable() {
        QCOMPARE(QString::fromLatin1(sm::failure_name(sm::Failure::WriteFailed)), QStringLiteral("write_failed"));
        QCOMPARE(QString::fromLatin1(sm::failure_name(sm::Failure::ReadBackMismatch)),
                 QStringLiteral("read_back_mismatch"));
        QCOMPARE(QString::fromLatin1(sm::failure_name(sm::Failure::StoreUnreadable)), QStringLiteral("store_unreadable"));
        QCOMPARE(QString::fromLatin1(sm::failure_name(sm::Failure::Conflict)), QStringLiteral("conflict"));
    }

    // ── later in the session ───────────────────────────────────────────────
    void retire_rule() {
        const QString cur = QStringLiteral("k-current");
        QVERIFY(!sm::may_retire_copy(QString(), cur, true, true));       // nothing left
        QVERIFY(sm::may_retire_copy(cur, cur, true, false));             // equal + verified in the store
        QVERIFY(!sm::may_retire_copy(cur, cur, false, true));            // equal but the store write failed
        QVERIFY(!sm::may_retire_copy(QStringLiteral("k-old"), cur, true, false)); // differs, not confirmed
        QVERIFY(sm::may_retire_copy(QStringLiteral("k-old"), cur, true, true));   // differs, superseded
        QVERIFY(!sm::may_retire_copy(cur, QString(), true, false));      // in use is empty (dead token), unconfirmed
        QVERIFY(sm::may_retire_copy(cur, QString(), false, true));       // ...superseded once the key is confirmed
    }

    void key_wrapping_drop_rule() {
        const QByteArray k(32, 'a');
        const QByteArray other(32, 'b');
        QVERIFY(sm::may_drop_old_key_wrapping(true, k, true, k));
        QVERIFY(!sm::may_drop_old_key_wrapping(false, k, true, k)); // new row not confirmed
        QVERIFY(!sm::may_drop_old_key_wrapping(true, k, false, k)); // old cannot be unwrapped here: keep it
        QVERIFY(!sm::may_drop_old_key_wrapping(true, k, true, other)); // wraps another key
        QVERIFY(!sm::may_drop_old_key_wrapping(true, QByteArray(), true, QByteArray()));
    }

    // ── SecureStorage::store_verified() ─────────────────────────────────────
    void store_verified_ok() {
        (void)SecureStorage::instance().read(QStringLiteral("probe")); // creates the fake backend
        QVERIFY(g_backend);
        g_backend->reset();
        QVERIFY(SecureStorage::instance().store_verified(QStringLiteral("api_key"), QStringLiteral("v1")).is_ok());
        QCOMPARE(g_backend->values.value(QStringLiteral("api_key")), QStringLiteral("v1"));
    }

    void store_verified_write_failure() {
        (void)SecureStorage::instance().read(QStringLiteral("probe"));
        g_backend->reset();
        g_backend->write_fails = true;
        QVERIFY(SecureStorage::instance().store_verified(QStringLiteral("api_key"), QStringLiteral("v1")).is_err());
    }

    void store_verified_read_back_mismatch() {
        (void)SecureStorage::instance().read(QStringLiteral("probe"));
        g_backend->reset();
        g_backend->write_garbles = true;
        const auto r = SecureStorage::instance().store_verified(QStringLiteral("api_key"), QStringLiteral("v1"));
        QVERIFY(r.is_err());
        QVERIFY(QString::fromStdString(r.error()).contains(QStringLiteral("read_back_mismatch")));
    }

    void store_verified_read_back_unreadable() {
        (void)SecureStorage::instance().read(QStringLiteral("probe"));
        g_backend->reset();
        g_backend->read_fails = true;
        const auto r = SecureStorage::instance().store_verified(QStringLiteral("api_key"), QStringLiteral("v1"));
        QVERIFY(r.is_err());
        QVERIFY(QString::fromStdString(r.error()).contains(QStringLiteral("read_back_failed")));
        g_backend->read_fails = false;
    }
};

QTEST_GUILESS_MAIN(TestSecretMigration)
#include "tst_secret_migration.moc"
