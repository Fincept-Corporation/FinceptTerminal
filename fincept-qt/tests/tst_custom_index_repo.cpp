// CustomIndexRepository::get_values — the most recent N levels, oldest first —
// on a temp-file database through the real storage layer (Database + migration
// v011). The defect this guards: "ORDER BY date ASC LIMIT 365" returned the
// OLDEST 365 levels, so an index older than a year showed its first year forever.
#include "storage/repositories/CustomIndexRepository.h"
#include "storage/sqlite/Database.h"

#include <QDate>
#include <QTemporaryDir>
#include <QtTest>

#include <algorithm>
#include <numeric>
#include <random>

namespace fincept {
void register_migration_v011();
}

using namespace fincept;

class TestCustomIndexRepo : public QObject {
    Q_OBJECT
  private slots:
    void initTestCase() {
        QVERIFY(dir_.isValid());
        register_migration_v011();
        const auto opened = Database::instance().open(dir_.filePath(QStringLiteral("fincept.db")));
        QVERIFY2(opened.is_ok(), opened.is_ok() ? "" : opened.error().c_str());

        CustomIndex idx;
        idx.name = QStringLiteral("Test index");
        idx.method = QStringLiteral("equal_weight");
        const auto created = CustomIndexRepository::instance().create(idx);
        QVERIFY2(created.is_ok(), created.is_ok() ? "" : created.error().c_str());
        id_ = created.value();

        // 400 daily levels, written out of order: value = day number.
        QVector<int> days(kDays);
        std::iota(days.begin(), days.end(), 0);
        std::shuffle(days.begin(), days.end(), std::mt19937(42));
        for (int d : days) {
            const auto w = CustomIndexRepository::instance().save_value(id_, date_of(d), 1000.0 + d);
            QVERIFY(w.is_ok());
        }
    }

    void cleanupTestCase() { Database::instance().close(); }

    void latest_n_oldest_first() {
        const auto r = CustomIndexRepository::instance().get_values(id_, 365);
        QVERIFY(r.is_ok());
        const auto& v = r.value();
        QCOMPARE(v.size(), 365);
        QCOMPARE(v.first().date, date_of(kDays - 365)); // day 35: the oldest of the latest 365
        QCOMPARE(v.last().date, date_of(kDays - 1));     // the newest level is included
        QCOMPARE(v.last().value, 1000.0 + (kDays - 1));
        for (int i = 1; i < v.size(); ++i)
            QVERIFY(v[i - 1].date < v[i].date); // ascending, as the chart expects
        QCOMPARE(v.last().value, CustomIndexRepository::instance().latest_value(id_));
    }

    void small_limit() {
        const auto r = CustomIndexRepository::instance().get_values(id_, 10);
        QVERIFY(r.is_ok());
        QCOMPARE(r.value().size(), 10);
        QCOMPARE(r.value().first().date, date_of(kDays - 10));
        QCOMPARE(r.value().last().date, date_of(kDays - 1));
    }

    void limit_larger_than_history_and_no_limit() {
        auto r = CustomIndexRepository::instance().get_values(id_, 10000);
        QVERIFY(r.is_ok());
        QCOMPARE(r.value().size(), kDays);
        QCOMPARE(r.value().first().date, date_of(0));
        r = CustomIndexRepository::instance().get_values(id_, 0); // 0 = everything
        QVERIFY(r.is_ok());
        QCOMPARE(r.value().size(), kDays);
        QCOMPARE(r.value().last().date, date_of(kDays - 1));
    }

    void other_index_and_unknown_index() {
        CustomIndex other;
        other.name = QStringLiteral("Other");
        other.method = QStringLiteral("equal_weight");
        const auto created = CustomIndexRepository::instance().create(other);
        QVERIFY(created.is_ok());
        QVERIFY(CustomIndexRepository::instance().save_value(created.value(), date_of(500), 7.0).is_ok());
        const auto mine = CustomIndexRepository::instance().get_values(id_, 365);
        QVERIFY(mine.is_ok());
        QCOMPARE(mine.value().last().date, date_of(kDays - 1)); // the other index's row is not mixed in
        const auto none = CustomIndexRepository::instance().get_values(QStringLiteral("no-such-index"), 365);
        QVERIFY(none.is_ok());
        QVERIFY(none.value().isEmpty());
    }

  private:
    static constexpr int kDays = 400;
    static QString date_of(int day) { return QDate(2025, 1, 1).addDays(day).toString(Qt::ISODate); }

    QTemporaryDir dir_;
    QString id_;
};

QTEST_GUILESS_MAIN(TestCustomIndexRepo)
#include "tst_custom_index_repo.moc"
