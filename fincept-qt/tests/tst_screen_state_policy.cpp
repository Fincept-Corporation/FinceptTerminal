// Unit tests for src/core/session/ScreenStatePolicy.h
//
// The restore decision and write-order gate behind ScreenStateManager. The bug
// these pin: after an abnormal exit the restore read the panel's own row, which
// was older than the screen-type row every edit had updated, so the newest
// state was lost and then overwritten by the next save.

#include "core/session/ScreenStatePolicy.h"

#include <QTest>

using fincept::session::choose_restore_source;
using fincept::session::parse_state_timestamp;
using fincept::session::RestoreSource;
using fincept::session::StoredScreenState;
using fincept::session::WriteOrderGate;

namespace {
const QString kPanel = QStringLiteral("11111111-1111-1111-1111-111111111111");
const QString kOtherPanel = QStringLiteral("22222222-2222-2222-2222-222222222222");

StoredScreenState row(const QString& updated_at, const QString& writer = {}) {
    StoredScreenState s;
    s.found = true;
    s.updated_at = updated_at;
    s.writer_uuid = writer;
    return s;
}
} // namespace

class TstScreenStatePolicy : public QObject {
    Q_OBJECT

  private slots:
    void timestamps_parse_both_sqlite_formats();
    void timestamps_invalid_are_null();
    void nothing_stored_restores_nothing();
    void only_one_row_uses_it();
    void newer_screen_type_row_from_older_build_wins();
    void older_screen_type_row_loses();
    void tie_keeps_panel_row();
    void screen_type_row_written_by_this_panel_and_newer_wins();
    void screen_type_row_written_by_another_panel_never_replaces_own_row();
    void unparseable_timestamps_keep_panel_row();
    void gate_skips_older_write_after_newer();
    void gate_is_per_row();
    void gate_ignores_failed_write();
};

void TstScreenStatePolicy::timestamps_parse_both_sqlite_formats() {
    const QDateTime a = parse_state_timestamp(QStringLiteral("2026-11-16T10:20:30.123Z"));
    QVERIFY(a.isValid());
    QCOMPARE(a.offsetFromUtc(), 0);
    QCOMPARE(a.time().msec(), 123);

    // datetime('now') form: space separator, no zone, UTC by definition.
    const QDateTime b = parse_state_timestamp(QStringLiteral("2026-11-16 10:20:30"));
    QVERIFY(b.isValid());
    QCOMPARE(b, QDateTime(QDate(2026, 11, 16), QTime(10, 20, 30), QTimeZone::UTC));

    QVERIFY(a > b);
}

void TstScreenStatePolicy::timestamps_invalid_are_null() {
    QVERIFY(!parse_state_timestamp(QString()).isValid());
    QVERIFY(!parse_state_timestamp(QStringLiteral("   ")).isValid());
    QVERIFY(!parse_state_timestamp(QStringLiteral("not a date")).isValid());
}

void TstScreenStatePolicy::nothing_stored_restores_nothing() {
    QCOMPARE(choose_restore_source({}, {}, kPanel), RestoreSource::None);
}

void TstScreenStatePolicy::only_one_row_uses_it() {
    QCOMPARE(choose_restore_source(row(QStringLiteral("2026-11-16T10:00:00.000Z")), {}, kPanel),
             RestoreSource::Instance);
    // First open of a panel (or migration from a pre-UUID build): the
    // screen-type row is the fallback, whoever wrote it.
    QCOMPARE(choose_restore_source({}, row(QStringLiteral("2026-11-16T10:00:00.000Z"), kOtherPanel), kPanel),
             RestoreSource::ScreenType);
    QCOMPARE(choose_restore_source({}, row(QStringLiteral("2026-11-16T10:00:00.000Z")), kPanel),
             RestoreSource::ScreenType);
}

void TstScreenStatePolicy::newer_screen_type_row_from_older_build_wins() {
    // The reproduced defect: the panel row was written at the last normal quit
    // (09:00), the edits after it went to the screen-type row (09:30) without a
    // writer tag, then the app was killed. The newer copy must win.
    const auto own = row(QStringLiteral("2026-11-16T09:00:00.000Z"));
    const auto type = row(QStringLiteral("2026-11-16T09:30:00.500Z"));
    QCOMPARE(choose_restore_source(own, type, kPanel), RestoreSource::ScreenType);
}

void TstScreenStatePolicy::older_screen_type_row_loses() {
    // Normal quit: the shutdown flush wrote the panel row last.
    const auto own = row(QStringLiteral("2026-11-16T09:30:01.000Z"));
    const auto type = row(QStringLiteral("2026-11-16T09:30:00.500Z"));
    QCOMPARE(choose_restore_source(own, type, kPanel), RestoreSource::Instance);
    // Mixed formats: an old datetime('now') screen-type row vs a newer panel row.
    QCOMPARE(choose_restore_source(row(QStringLiteral("2026-11-16T09:30:01.000Z")),
                                   row(QStringLiteral("2026-11-16 09:30:00")), kPanel),
             RestoreSource::Instance);
}

void TstScreenStatePolicy::tie_keeps_panel_row() {
    const QString t = QStringLiteral("2026-11-16T09:30:00.500Z");
    QCOMPARE(choose_restore_source(row(t), row(t, kPanel), kPanel), RestoreSource::Instance);
    QCOMPARE(choose_restore_source(row(t), row(t), kPanel), RestoreSource::Instance);
}

void TstScreenStatePolicy::screen_type_row_written_by_this_panel_and_newer_wins() {
    const auto own = row(QStringLiteral("2026-11-16T09:00:00.000Z"));
    const auto type = row(QStringLiteral("2026-11-16T09:00:05.000Z"), kPanel);
    QCOMPARE(choose_restore_source(own, type, kPanel), RestoreSource::ScreenType);
}

void TstScreenStatePolicy::screen_type_row_written_by_another_panel_never_replaces_own_row() {
    // Two watchlists: the other one saved later. This panel keeps its own state.
    const auto own = row(QStringLiteral("2026-11-16T09:00:00.000Z"));
    const auto type = row(QStringLiteral("2026-11-16T10:00:00.000Z"), kOtherPanel);
    QCOMPARE(choose_restore_source(own, type, kPanel), RestoreSource::Instance);
}

void TstScreenStatePolicy::unparseable_timestamps_keep_panel_row() {
    QCOMPARE(choose_restore_source(row(QStringLiteral("2026-11-16T09:00:00.000Z")), row(QString()), kPanel),
             RestoreSource::Instance);
    // Panel row without a usable time, screen-type row with one: the dated copy wins.
    QCOMPARE(choose_restore_source(row(QString()), row(QStringLiteral("2026-11-16T09:00:00.000Z")), kPanel),
             RestoreSource::ScreenType);
}

void TstScreenStatePolicy::gate_skips_older_write_after_newer() {
    WriteOrderGate gate;
    const QString key = QStringLiteral("uuid:") + kPanel;
    // Dispatched as 1 (debounced flush) then 2 (hide); 2 finishes first.
    QVERIFY(gate.admit(key, 2));
    gate.mark_applied(key, 2);
    QVERIFY(!gate.admit(key, 1)); // the older write must not land on top
    QVERIFY(!gate.admit(key, 2)); // nor the same one twice
    QVERIFY(gate.admit(key, 3));
    gate.mark_applied(key, 3);
    gate.mark_applied(key, 1); // marking an older seq never moves the gate back
    QCOMPARE(gate.last_applied(key), quint64(3));
}

void TstScreenStatePolicy::gate_is_per_row() {
    WriteOrderGate gate;
    gate.mark_applied(QStringLiteral("alt_investments"), 10);
    QVERIFY(gate.admit(QStringLiteral("uuid:") + kPanel, 1));
    QVERIFY(!gate.admit(QStringLiteral("alt_investments"), 9));
}

void TstScreenStatePolicy::gate_ignores_failed_write() {
    // A write that failed is never marked, so an older one may still land
    // (better than leaving the even older row in place).
    WriteOrderGate gate;
    const QString key = QStringLiteral("watchlist");
    QVERIFY(gate.admit(key, 5)); // seq 5 attempted, failed -> not marked
    QVERIFY(gate.admit(key, 4));
    gate.mark_applied(key, 4);
    QVERIFY(gate.admit(key, 5));
}

QTEST_GUILESS_MAIN(TstScreenStatePolicy)
#include "tst_screen_state_policy.moc"
