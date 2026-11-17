// src/ui/widgets/KeySortProxy.h
#pragma once
// ui::KeySortProxy — a QSortFilterProxyModel that sorts by TYPED sort keys and filters by
// text, the shared copy of the per-screen proxies (PortfolioBlotter, Screener, Markets,
// Backtesting trades, chat run list):
//   * sort key = data(sortRole()) of the sorted column (setSortRole(MyModel::SortKeyRole)),
//     or a host key provider that reads rows directly (no data() call per comparison —
//     large tables);
//   * numbers (all integer types, double, float, bool) compare numerically, dates /
//     date-times / times chronologically, strings with a locale-aware collator
//     (case-insensitive by default, optional natural numbers "A2" < "A10");
//   * nulls LAST in both orders: invalid QVariant, null variant, NaN, invalid date, and an
//     empty string (unless set_empty_string_is_null(false)); mixed classes keep a fixed
//     order (numbers, dates, strings);
//   * optional "pinned" role: rows whose pinned role is true stay at the bottom in both
//     orders and are never filtered out (a TOTAL row);
//   * optional text filter: whitespace-separated terms, every term must appear
//     (case-insensitive) in at least one of the chosen columns (filterRole(), default
//     DisplayRole); plus an optional host row predicate; plus QSortFilterProxyModel's
//     own filter (setFilterRegularExpression etc.) — all must pass.
// Ties keep the source order (QSortFilterProxyModel sorts stably).
//
//   auto* proxy = new fincept::ui::KeySortProxy(this);
//   proxy->setSourceModel(model);  proxy->setSortRole(HoldingsModel::SortKeyRole);
//   proxy->set_filter_columns({kSymbol, kName});  proxy->set_filter_text(search->text());

#include <QCollator>
#include <QList>
#include <QSortFilterProxyModel>
#include <QStringList>
#include <QVariant>

#include <functional>

namespace fincept::ui {

namespace key_sort {

enum class KeyClass { Null, Number, Date, String };

struct Options {
    bool empty_string_is_null = true;
};

/// Which class a key belongs to (Null for invalid / null / NaN / invalid date / "" when
/// empty strings are null).
KeyClass classify(const QVariant& key, const Options& options = {});

/// Three-way compare of two keys: <0, 0, >0. Nulls compare after everything (two nulls
/// are equal); different classes compare by class (Number < Date < String).
int compare_keys(const QVariant& a, const QVariant& b, const QCollator& collator, const Options& options = {});

/// QSortFilterProxyModel::lessThan for `order` with nulls last in BOTH orders (Qt sorts
/// descending by calling lessThan with the arguments swapped).
bool less_than(const QVariant& a, const QVariant& b, Qt::SortOrder order, const QCollator& collator,
               const Options& options = {});

/// Same rule for a "pinned" flag (pinned rows last in both orders). Returns true and sets
/// *less when the pin decides the order; false when both rows have the same pin state.
bool pinned_order(bool left_pinned, bool right_pinned, Qt::SortOrder order, bool* less);

/// Filter terms: the text split on whitespace, empty parts dropped.
QStringList filter_terms(const QString& text);

/// true when every term appears (case-insensitive) in at least one cell; no terms = true.
bool matches_terms(const QStringList& cells, const QStringList& terms);

} // namespace key_sort

class KeySortProxy : public QSortFilterProxyModel {
  public:
    /// Key of (source_row, column) under source_parent — used instead of data(sortRole()).
    using KeyFn = std::function<QVariant(int source_row, int column, const QModelIndex& source_parent)>;
    /// Extra row predicate (AND with the text filter); e.g. a per-row accept vector.
    using RowFilterFn = std::function<bool(int source_row, const QModelIndex& source_parent)>;

    explicit KeySortProxy(QObject* parent = nullptr);

    // ── sorting ─────────────────────────────────────────────────────────────
    /// Read keys from the rows directly (no data() per comparison); empty = data(sortRole()).
    void set_key_provider(KeyFn fn);
    /// Strings: case sensitivity (default Qt::CaseInsensitive).
    void set_case_sensitivity(Qt::CaseSensitivity cs);
    /// Strings: digits compare by value ("A2" < "A10"); default off.
    void set_natural_numbers(bool on);
    /// Strings: collation locale (default QLocale(), the application locale at construction).
    void set_collation_locale(const QLocale& locale);
    const QCollator& collator() const { return collator_; }
    /// Default true: "" is a null key (sorts last).
    void set_empty_string_is_null(bool on);
    /// Rows whose `role` data is true stay last in both orders and always pass the filter;
    /// -1 = none (default).
    void set_pinned_role(int role);

    // ── filtering ───────────────────────────────────────────────────────────
    /// Whitespace-separated terms; each must appear in one of the filter columns.
    void set_filter_text(const QString& text);
    QString filter_text() const { return filter_text_; }
    /// Columns searched by the text filter (empty = every column, the default).
    void set_filter_columns(const QList<int>& columns);
    void set_row_filter(RowFilterFn fn);
    /// Re-run the filter after the host's predicate inputs changed.
    void refilter();
    /// Re-sort after the host's keys changed without dataChanged (key provider).
    void resort();
    /// true while a text filter or a row predicate is set.
    bool is_filtering() const;

  protected:
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;
    bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const override;

  private:
    QVariant key_of(const QModelIndex& source_index) const;
    bool is_pinned(int source_row, const QModelIndex& source_parent) const;

    KeyFn key_fn_;
    RowFilterFn row_filter_;
    QCollator collator_;
    key_sort::Options options_;
    int pinned_role_ = -1;
    QString filter_text_;
    QStringList terms_;
    QList<int> filter_columns_;
};

} // namespace fincept::ui
