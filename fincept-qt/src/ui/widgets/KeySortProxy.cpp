// src/ui/widgets/KeySortProxy.cpp
#include "ui/widgets/KeySortProxy.h"

#include <QDate>
#include <QDateTime>
#include <QTime>

#include <cmath>

namespace fincept::ui {

namespace key_sort {

namespace {

bool ksp_is_signed_int(int t) {
    switch (t) {
        case QMetaType::Int:
        case QMetaType::LongLong:
        case QMetaType::Short:
        case QMetaType::Long:
        case QMetaType::Char:
        case QMetaType::SChar:
            return true;
        default:
            return false;
    }
}

bool ksp_is_unsigned_int(int t) {
    switch (t) {
        case QMetaType::UInt:
        case QMetaType::ULongLong:
        case QMetaType::UShort:
        case QMetaType::ULong:
        case QMetaType::UChar:
        case QMetaType::Bool:
            return true;
        default:
            return false;
    }
}

bool ksp_is_float(int t) {
    return t == QMetaType::Double || t == QMetaType::Float || t == QMetaType::Float16;
}

[[maybe_unused]] bool ksp_is_date(int t) {
    return t == QMetaType::QDate || t == QMetaType::QDateTime || t == QMetaType::QTime;
}

bool ksp_is_text(int t) {
    return t == QMetaType::QString || t == QMetaType::QByteArray || t == QMetaType::QChar;
}

template <typename T>
int ksp_cmp(const T& x, const T& y) {
    return x < y ? -1 : (y < x ? 1 : 0);
}

int ksp_compare_numbers(const QVariant& a, const QVariant& b) {
    const int ta = a.typeId();
    const int tb = b.typeId();
    const bool ia = ksp_is_signed_int(ta) || ksp_is_unsigned_int(ta);
    const bool ib = ksp_is_signed_int(tb) || ksp_is_unsigned_int(tb);
    if (ia && ib) {
        // Exact for every 64-bit value (a double would round above 2^53).
        if (ksp_is_signed_int(ta) && ksp_is_signed_int(tb))
            return ksp_cmp(a.toLongLong(), b.toLongLong());
        if (ksp_is_unsigned_int(ta) && ksp_is_unsigned_int(tb))
            return ksp_cmp(a.toULongLong(), b.toULongLong());
        const bool a_signed = ksp_is_signed_int(ta);
        const qint64 s = a_signed ? a.toLongLong() : b.toLongLong();
        const quint64 u = a_signed ? b.toULongLong() : a.toULongLong();
        const int signed_vs_unsigned = s < 0 ? -1 : ksp_cmp(static_cast<quint64>(s), u);
        return a_signed ? signed_vs_unsigned : -signed_vs_unsigned;
    }
    return ksp_cmp(a.toDouble(), b.toDouble());
}

int ksp_compare_dates(const QVariant& a, const QVariant& b) {
    const int ta = a.typeId();
    const int tb = b.typeId();
    const bool time_a = ta == QMetaType::QTime;
    const bool time_b = tb == QMetaType::QTime;
    if (time_a && time_b)
        return ksp_cmp(a.toTime().msecsSinceStartOfDay(), b.toTime().msecsSinceStartOfDay());
    if (time_a != time_b) // a bare time of day has no date: after every date
        return time_a ? 1 : -1;
    const QDateTime x = ta == QMetaType::QDate ? a.toDate().startOfDay() : a.toDateTime();
    const QDateTime y = tb == QMetaType::QDate ? b.toDate().startOfDay() : b.toDateTime();
    return ksp_cmp(x, y);
}

int ksp_rank(KeyClass c) {
    switch (c) {
        case KeyClass::Number:
            return 0;
        case KeyClass::Date:
            return 1;
        case KeyClass::String:
            return 2;
        case KeyClass::Null:
            break;
    }
    return 3;
}

} // namespace

KeyClass classify(const QVariant& key, const Options& options) {
    if (!key.isValid() || key.isNull())
        return KeyClass::Null;
    const int t = key.typeId();
    if (ksp_is_signed_int(t) || ksp_is_unsigned_int(t))
        return KeyClass::Number;
    if (ksp_is_float(t))
        return std::isnan(key.toDouble()) ? KeyClass::Null : KeyClass::Number;
    if (t == QMetaType::QDate)
        return key.toDate().isValid() ? KeyClass::Date : KeyClass::Null;
    if (t == QMetaType::QDateTime)
        return key.toDateTime().isValid() ? KeyClass::Date : KeyClass::Null;
    if (t == QMetaType::QTime)
        return key.toTime().isValid() ? KeyClass::Date : KeyClass::Null;
    if (!ksp_is_text(t) && !key.canConvert<QString>())
        return KeyClass::Null; // a list / map / custom type: nothing to order by
    if (options.empty_string_is_null && key.toString().isEmpty())
        return KeyClass::Null;
    return KeyClass::String;
}

// "A2" before "A10". QCollator's numeric mode does this only where Qt collates through ICU
// or the Windows / macOS APIs; a Qt built without ICU (the Linux packages) falls back to the
// C library, which ignores the mode and sorted "A10" before "A2". So digit runs are compared
// here, by value, and only the text between them goes to the collator: the same order on
// every platform.
static int ksp_natural_compare(const QString& a, const QString& b, const QCollator& collator) {
    qsizetype i = 0;
    qsizetype j = 0;
    while (i < a.size() && j < b.size()) {
        const bool da = a.at(i).isDigit();
        const bool db = b.at(j).isDigit();
        qsizetype ie = i;
        qsizetype je = j;
        if (da && db) {
            while (ie < a.size() && a.at(ie).isDigit())
                ++ie;
            while (je < b.size() && b.at(je).isDigit())
                ++je;
            // Leading zeros carry no value; a longer run of the rest is the larger number.
            qsizetype ia = i;
            qsizetype jb = j;
            while (ia < ie - 1 && a.at(ia).digitValue() == 0)
                ++ia;
            while (jb < je - 1 && b.at(jb).digitValue() == 0)
                ++jb;
            if (ie - ia != je - jb)
                return (ie - ia) < (je - jb) ? -1 : 1;
            for (qsizetype k = 0; k < ie - ia; ++k) {
                const int x = a.at(ia + k).digitValue();
                const int y = b.at(jb + k).digitValue();
                if (x != y)
                    return x < y ? -1 : 1;
            }
        } else {
            while (ie < a.size() && !a.at(ie).isDigit())
                ++ie;
            while (je < b.size() && !b.at(je).isDigit())
                ++je;
            // Digits sort before text when one side has a number where the other has letters.
            if (da != db)
                return da ? -1 : 1;
            const int c = collator.compare(QStringView(a).mid(i, ie - i), QStringView(b).mid(j, je - j));
            if (c != 0)
                return c;
        }
        i = ie;
        j = je;
    }
    if (i < a.size())
        return 1;
    if (j < b.size())
        return -1;
    return 0;
}

int compare_keys(const QVariant& a, const QVariant& b, const QCollator& collator, const Options& options) {
    const KeyClass ca = classify(a, options);
    const KeyClass cb = classify(b, options);
    if (ca != cb)
        return ksp_cmp(ksp_rank(ca), ksp_rank(cb));
    switch (ca) {
        case KeyClass::Null:
            return 0;
        case KeyClass::Number:
            return ksp_compare_numbers(a, b);
        case KeyClass::Date:
            return ksp_compare_dates(a, b);
        case KeyClass::String: {
            const int c = collator.numericMode() ? ksp_natural_compare(a.toString(), b.toString(), collator)
                                                 : collator.compare(a.toString(), b.toString());
            return c < 0 ? -1 : (c > 0 ? 1 : 0);
        }
    }
    return 0;
}

bool pinned_order(bool left_pinned, bool right_pinned, Qt::SortOrder order, bool* less) {
    if (left_pinned == right_pinned)
        return false;
    // Ascending: a pinned row is the largest; descending (Qt swaps the arguments): the
    // smallest — so it lands at the bottom both ways.
    if (less)
        *less = order == Qt::DescendingOrder ? left_pinned : right_pinned;
    return true;
}

bool less_than(const QVariant& a, const QVariant& b, Qt::SortOrder order, const QCollator& collator,
               const Options& options) {
    const bool a_null = classify(a, options) == KeyClass::Null;
    const bool b_null = classify(b, options) == KeyClass::Null;
    if (a_null || b_null) {
        if (a_null && b_null)
            return false;
        bool less = false;
        pinned_order(a_null, b_null, order, &less); // a null key behaves like a pinned row
        return less;
    }
    return compare_keys(a, b, collator, options) < 0;
}

QStringList filter_terms(const QString& text) {
    return text.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
}

bool matches_terms(const QStringList& cells, const QStringList& terms) {
    for (const QString& term : terms) {
        bool found = false;
        for (const QString& cell : cells)
            if (cell.contains(term, Qt::CaseInsensitive)) {
                found = true;
                break;
            }
        if (!found)
            return false;
    }
    return true;
}

} // namespace key_sort

// ── KeySortProxy ─────────────────────────────────────────────────────────────

KeySortProxy::KeySortProxy(QObject* parent) : QSortFilterProxyModel(parent) {
    collator_.setCaseSensitivity(Qt::CaseInsensitive);
}

void KeySortProxy::set_key_provider(KeyFn fn) {
    key_fn_ = std::move(fn);
    invalidate();
}

void KeySortProxy::set_case_sensitivity(Qt::CaseSensitivity cs) {
    if (collator_.caseSensitivity() == cs)
        return;
    collator_.setCaseSensitivity(cs);
    invalidate();
}

void KeySortProxy::set_natural_numbers(bool on) {
    if (collator_.numericMode() == on)
        return;
    collator_.setNumericMode(on);
    invalidate();
}

void KeySortProxy::set_collation_locale(const QLocale& locale) {
    if (collator_.locale() == locale)
        return;
    collator_.setLocale(locale);
    invalidate();
}

void KeySortProxy::set_empty_string_is_null(bool on) {
    if (options_.empty_string_is_null == on)
        return;
    options_.empty_string_is_null = on;
    invalidate();
}

void KeySortProxy::set_pinned_role(int role) {
    if (pinned_role_ == role)
        return;
    pinned_role_ = role;
    invalidate();
}

void KeySortProxy::set_filter_text(const QString& text) {
    filter_text_ = text;
    QStringList terms = key_sort::filter_terms(text);
    if (terms == terms_)
        return; // only whitespace changed
    terms_ = std::move(terms);
    invalidateRowsFilter();
}

void KeySortProxy::set_filter_columns(const QList<int>& columns) {
    if (filter_columns_ == columns)
        return;
    filter_columns_ = columns;
    if (!terms_.isEmpty())
        invalidateRowsFilter();
}

void KeySortProxy::set_row_filter(RowFilterFn fn) {
    row_filter_ = std::move(fn);
    invalidateRowsFilter();
}

void KeySortProxy::refilter() {
    invalidateRowsFilter();
}

void KeySortProxy::resort() {
    // sort() with the current column / order is a no-op while dynamic sorting is on.
    invalidate();
}

bool KeySortProxy::is_filtering() const {
    return !terms_.isEmpty() || static_cast<bool>(row_filter_);
}

QVariant KeySortProxy::key_of(const QModelIndex& source_index) const {
    if (key_fn_)
        return key_fn_(source_index.row(), source_index.column(), source_index.parent());
    return source_index.data(sortRole());
}

bool KeySortProxy::is_pinned(int source_row, const QModelIndex& source_parent) const {
    if (pinned_role_ < 0 || !sourceModel())
        return false;
    return sourceModel()->index(source_row, 0, source_parent).data(pinned_role_).toBool();
}

bool KeySortProxy::lessThan(const QModelIndex& left, const QModelIndex& right) const {
    if (pinned_role_ >= 0) {
        bool less = false;
        if (key_sort::pinned_order(is_pinned(left.row(), left.parent()), is_pinned(right.row(), right.parent()),
                                   sortOrder(), &less))
            return less;
    }
    return key_sort::less_than(key_of(left), key_of(right), sortOrder(), collator_, options_);
}

bool KeySortProxy::filterAcceptsRow(int source_row, const QModelIndex& source_parent) const {
    if (is_pinned(source_row, source_parent))
        return true;
    if (row_filter_ && !row_filter_(source_row, source_parent))
        return false;
    if (!QSortFilterProxyModel::filterAcceptsRow(source_row, source_parent))
        return false;
    if (terms_.isEmpty() || !sourceModel())
        return true;
    const QAbstractItemModel* m = sourceModel();
    const int columns = m->columnCount(source_parent);
    QStringList cells;
    if (filter_columns_.isEmpty()) {
        cells.reserve(columns);
        for (int c = 0; c < columns; ++c)
            cells.append(m->index(source_row, c, source_parent).data(filterRole()).toString());
    } else {
        cells.reserve(filter_columns_.size());
        for (int c : filter_columns_)
            if (c >= 0 && c < columns)
                cells.append(m->index(source_row, c, source_parent).data(filterRole()).toString());
    }
    return key_sort::matches_terms(cells, terms_);
}

} // namespace fincept::ui
