// tests/tst_key_sort_proxy.cpp — ui::KeySortProxy (src/ui/widgets/KeySortProxy.cpp, QtCore only):
// typed sort keys, nulls last in both orders, pinned rows, text filter across chosen columns.
#include "ui/widgets/KeySortProxy.h"

#include <QAbstractTableModel>
#include <QtTest/QtTest>

#include <cmath>
#include <limits>

using fincept::ui::KeySortProxy;
namespace ks = fincept::ui::key_sort;

namespace {

constexpr int kKeyRole = Qt::UserRole + 1;
constexpr int kPinRole = Qt::UserRole + 2;

/// Column 0 = the sort key (typed, at kKeyRole; its text at DisplayRole), column 1 = a name.
class KspModel : public QAbstractTableModel {
  public:
    QVector<QVariant> keys;
    QStringList names;
    int pinned_row = -1;
    mutable int key_reads = 0;

    int rowCount(const QModelIndex& p = {}) const override { return p.isValid() ? 0 : int(keys.size()); }
    int columnCount(const QModelIndex& p = {}) const override { return p.isValid() ? 0 : 2; }
    QVariant data(const QModelIndex& i, int role) const override {
        if (!i.isValid())
            return {};
        if (role == kPinRole)
            return i.row() == pinned_row;
        if (i.column() == 0) {
            if (role == kKeyRole) {
                ++key_reads;
                return keys.at(i.row());
            }
            if (role == Qt::DisplayRole)
                return keys.at(i.row()).toString();
        } else if (role == Qt::DisplayRole) {
            return names.value(i.row());
        }
        return {};
    }
};

QStringList column1(const QSortFilterProxyModel& p) {
    QStringList out;
    for (int r = 0; r < p.rowCount(); ++r)
        out << p.index(r, 1).data().toString();
    return out;
}

QCollator ci_collator() {
    QCollator c;
    c.setCaseSensitivity(Qt::CaseInsensitive);
    return c;
}

} // namespace

class TestKeySortProxy : public QObject {
    Q_OBJECT
  private slots:
    void classify_nulls() {
        QCOMPARE(ks::classify(QVariant()), ks::KeyClass::Null);
        QCOMPARE(ks::classify(QVariant(std::numeric_limits<double>::quiet_NaN())), ks::KeyClass::Null);
        QCOMPARE(ks::classify(QVariant(QString())), ks::KeyClass::Null);
        QCOMPARE(ks::classify(QVariant(QStringLiteral(""))), ks::KeyClass::Null);
        QCOMPARE(ks::classify(QVariant(QDate())), ks::KeyClass::Null);
        QCOMPARE(ks::classify(QVariant(QDateTime())), ks::KeyClass::Null);
        QCOMPARE(ks::classify(QVariant(QVariantList{1, 2})), ks::KeyClass::Null);
        ks::Options keep_empty;
        keep_empty.empty_string_is_null = false;
        QCOMPARE(ks::classify(QVariant(QStringLiteral("")), keep_empty), ks::KeyClass::String);
    }

    void classify_types() {
        QCOMPARE(ks::classify(QVariant(3)), ks::KeyClass::Number);
        QCOMPARE(ks::classify(QVariant(qint64(3))), ks::KeyClass::Number);
        QCOMPARE(ks::classify(QVariant(2.5)), ks::KeyClass::Number);
        QCOMPARE(ks::classify(QVariant(2.5f)), ks::KeyClass::Number);
        QCOMPARE(ks::classify(QVariant(true)), ks::KeyClass::Number);
        QCOMPARE(ks::classify(QVariant(std::numeric_limits<double>::infinity())), ks::KeyClass::Number);
        QCOMPARE(ks::classify(QVariant(QDate(2026, 11, 17))), ks::KeyClass::Date);
        QCOMPARE(ks::classify(QVariant(QTime(9, 15))), ks::KeyClass::Date);
        QCOMPARE(ks::classify(QVariant(QStringLiteral("AAPL"))), ks::KeyClass::String);
        QCOMPARE(ks::classify(QVariant(QByteArray("x"))), ks::KeyClass::String);
    }

    void numbers_compare_exactly() {
        const QCollator c = ci_collator();
        QVERIFY(ks::compare_keys(QVariant(2), QVariant(2.5), c) < 0);
        QVERIFY(ks::compare_keys(QVariant(-1.0), QVariant(0), c) < 0);
        // beyond double precision (2^53 + 1 vs 2^53)
        QVERIFY(ks::compare_keys(QVariant(qint64(9007199254740993LL)), QVariant(qint64(9007199254740992LL)), c) > 0);
        QVERIFY(ks::compare_keys(QVariant(qint64(-1)), QVariant(quint64(0)), c) < 0);
        QVERIFY(ks::compare_keys(QVariant(std::numeric_limits<quint64>::max()),
                                 QVariant(std::numeric_limits<qint64>::max()), c) > 0);
        QVERIFY(ks::compare_keys(QVariant(qint64(5)), QVariant(quint64(5)), c) == 0);
        QVERIFY(ks::compare_keys(QVariant(-std::numeric_limits<double>::infinity()), QVariant(-1e300), c) < 0);
    }

    void dates_compare_chronologically() {
        const QCollator c = ci_collator();
        QVERIFY(ks::compare_keys(QVariant(QDate(2026, 1, 2)), QVariant(QDate(2026, 1, 1)), c) > 0);
        QVERIFY(ks::compare_keys(QVariant(QDate(2026, 1, 2)), QVariant(QDateTime(QDate(2026, 1, 1), QTime(23, 0))), c) > 0);
        QVERIFY(ks::compare_keys(QVariant(QDateTime(QDate(2026, 1, 1), QTime(1, 0))), QVariant(QDate(2026, 1, 1)), c) > 0);
        QVERIFY(ks::compare_keys(QVariant(QTime(9, 0)), QVariant(QTime(15, 30)), c) < 0);
        QVERIFY(ks::compare_keys(QVariant(QTime(9, 0)), QVariant(QDate(2030, 1, 1)), c) > 0); // a bare time after dates
    }

    void strings_use_the_collator() {
        QCollator c = ci_collator();
        QVERIFY(ks::compare_keys(QVariant(QStringLiteral("apple")), QVariant(QStringLiteral("Banana")), c) < 0);
        QCOMPARE(ks::compare_keys(QVariant(QStringLiteral("abc")), QVariant(QStringLiteral("ABC")), c), 0);
        c.setNumericMode(true);
        QVERIFY(ks::compare_keys(QVariant(QStringLiteral("A2")), QVariant(QStringLiteral("A10")), c) < 0);
    }

    void classes_keep_a_fixed_order() {
        const QCollator c = ci_collator();
        QVERIFY(ks::compare_keys(QVariant(1e9), QVariant(QDate(2000, 1, 1)), c) < 0);
        QVERIFY(ks::compare_keys(QVariant(QDate(2000, 1, 1)), QVariant(QStringLiteral("a")), c) < 0);
        QVERIFY(ks::compare_keys(QVariant(QStringLiteral("a")), QVariant(), c) < 0);
        QCOMPARE(ks::compare_keys(QVariant(), QVariant(QStringLiteral("")), c), 0);
    }

    void less_than_puts_nulls_last_both_ways() {
        const QCollator c = ci_collator();
        const QVariant null;
        const QVariant one(1);
        QVERIFY(ks::less_than(one, null, Qt::AscendingOrder, c));
        QVERIFY(!ks::less_than(null, one, Qt::AscendingOrder, c));
        // descending: Qt calls lessThan(right, left) — null must be "smaller" to end up last
        QVERIFY(ks::less_than(null, one, Qt::DescendingOrder, c));
        QVERIFY(!ks::less_than(one, null, Qt::DescendingOrder, c));
        QVERIFY(!ks::less_than(null, null, Qt::AscendingOrder, c));
        QVERIFY(!ks::less_than(null, null, Qt::DescendingOrder, c));
    }

    void proxy_sorts_typed_keys_with_nulls_last() {
        KspModel m;
        m.keys = {QVariant(3), QVariant(), QVariant(1.5), QVariant(QStringLiteral("")), QVariant(2),
                  QVariant(std::numeric_limits<double>::quiet_NaN())};
        m.names = {"three", "null", "one-and-half", "empty", "two", "nan"};
        KeySortProxy p;
        p.setSourceModel(&m);
        p.setSortRole(kKeyRole);
        p.sort(0, Qt::AscendingOrder);
        QCOMPARE(column1(p), QStringList({"one-and-half", "two", "three", "null", "empty", "nan"}));
        p.sort(0, Qt::DescendingOrder);
        QCOMPARE(column1(p).mid(0, 3), QStringList({"three", "two", "one-and-half"}));
        QCOMPARE(column1(p).mid(3).size(), 3); // the three nulls stay at the bottom
        QVERIFY(!column1(p).mid(3).contains("two"));
    }

    void proxy_ties_keep_source_order() {
        KspModel m;
        m.keys = {QVariant(1), QVariant(0), QVariant(1), QVariant(1)};
        m.names = {"a", "z", "b", "c"};
        KeySortProxy p;
        p.setSourceModel(&m);
        p.setSortRole(kKeyRole);
        p.sort(0, Qt::AscendingOrder);
        QCOMPARE(column1(p), QStringList({"z", "a", "b", "c"}));
    }

    void proxy_pinned_row_stays_last_and_unfiltered() {
        KspModel m;
        m.keys = {QVariant(100), QVariant(1), QVariant(2)};
        m.names = {"TOTAL", "alpha", "beta"};
        m.pinned_row = 0;
        KeySortProxy p;
        p.setSourceModel(&m);
        p.setSortRole(kKeyRole);
        p.set_pinned_role(kPinRole);
        p.sort(0, Qt::AscendingOrder);
        QCOMPARE(column1(p), QStringList({"alpha", "beta", "TOTAL"}));
        p.sort(0, Qt::DescendingOrder);
        QCOMPARE(column1(p), QStringList({"beta", "alpha", "TOTAL"}));
        p.set_filter_text(QStringLiteral("zzz"));
        QCOMPARE(column1(p), QStringList({"TOTAL"}));
    }

    void proxy_natural_and_case_options() {
        KspModel m;
        m.keys = {QVariant(QStringLiteral("A10")), QVariant(QStringLiteral("a2")), QVariant(QStringLiteral("A1"))};
        m.names = {"A10", "a2", "A1"};
        KeySortProxy p;
        p.setSourceModel(&m);
        p.setSortRole(kKeyRole);
        p.set_natural_numbers(true);
        p.sort(0, Qt::AscendingOrder);
        QCOMPARE(column1(p), QStringList({"A1", "a2", "A10"}));
        p.set_natural_numbers(false); // re-sorts by itself
        QCOMPARE(column1(p), QStringList({"A1", "A10", "a2"}));
    }

    void proxy_key_provider_bypasses_data() {
        KspModel m;
        m.keys = {QVariant(1), QVariant(2), QVariant(3)};
        m.names = {"x", "y", "z"};
        const QVector<double> host_keys = {30.0, 10.0, 20.0};
        KeySortProxy p;
        p.setSourceModel(&m);
        p.setSortRole(kKeyRole);
        p.set_key_provider([&](int row, int, const QModelIndex&) { return QVariant(host_keys.at(row)); });
        m.key_reads = 0;
        p.sort(0, Qt::AscendingOrder);
        QCOMPARE(column1(p), QStringList({"y", "z", "x"}));
        QCOMPARE(m.key_reads, 0);
    }

    void filter_terms_and_matching() {
        QCOMPARE(ks::filter_terms(QStringLiteral("  aapl \t inc  ")), QStringList({"aapl", "inc"}));
        QVERIFY(ks::filter_terms(QStringLiteral("   ")).isEmpty());
        QVERIFY(ks::matches_terms({"AAPL", "Apple Inc."}, {"aapl", "inc"}));
        QVERIFY(!ks::matches_terms({"AAPL", "Apple Inc."}, {"aapl", "msft"}));
        QVERIFY(ks::matches_terms({}, {}));
        QVERIFY(!ks::matches_terms({}, {"x"}));
    }

    void proxy_text_filter_across_chosen_columns() {
        KspModel m;
        m.keys = {QVariant(QStringLiteral("AAPL")), QVariant(QStringLiteral("MSFT")), QVariant(QStringLiteral("IBM"))};
        m.names = {"Apple Inc.", "Microsoft Corp.", "International Business Machines"};
        KeySortProxy p;
        p.setSourceModel(&m);
        p.set_filter_text(QStringLiteral("inc aapl"));
        QCOMPARE(column1(p), QStringList({"Apple Inc."}));
        p.set_filter_columns({1}); // names only: "aapl" is no longer found
        QCOMPARE(p.rowCount(), 0);
        p.set_filter_text(QStringLiteral("corp"));
        QCOMPARE(column1(p), QStringList({"Microsoft Corp."}));
        QVERIFY(p.is_filtering());
        p.set_filter_text({});
        QCOMPARE(p.rowCount(), 3);
        QVERIFY(!p.is_filtering());
    }

    void proxy_row_filter_and_refilter() {
        KspModel m;
        m.keys = {QVariant(1), QVariant(2), QVariant(3)};
        m.names = {"a", "b", "c"};
        QVector<char> accept = {1, 0, 1};
        KeySortProxy p;
        p.setSourceModel(&m);
        p.set_row_filter([&](int row, const QModelIndex&) { return accept.value(row) != 0; });
        QCOMPARE(column1(p), QStringList({"a", "c"}));
        accept = {0, 1, 1};
        p.refilter();
        QCOMPARE(column1(p), QStringList({"b", "c"}));
        // the base class filter still applies too
        p.setFilterKeyColumn(1);
        p.setFilterFixedString(QStringLiteral("c"));
        QCOMPARE(column1(p), QStringList({"c"}));
    }
};

QTEST_APPLESS_MAIN(TestKeySortProxy)
#include "tst_key_sort_proxy.moc"
