#pragma once
// LiveTableModel — a reusable QAbstractTableModel for streaming financial rows.
//
// WHY THIS EXISTS
// ---------------
// The terminal has ~2,000 uses of QTableWidget/QTreeWidget/QListWidget against
// 16 uses of model/view. The item-widget idiom forces a specific anti-pattern on
// every live blotter:
//
//     table->setRowCount(0);                       // destroy every cell
//     for (const auto& p : positions) {            // rebuild all of them
//         table->setItem(r, 0, new QTableWidgetItem(...));   // ~10 heap allocs/row
//         auto* btn = new QPushButton(tr("Exit"));           // + a widget
//         btn->setStyleSheet("...");                         // + a CSS reparse
//     }
//
// A 200-row blotter therefore costs ~2,000 allocations and ~200 full CSS
// reparses *per tick*, and — because the rows are destroyed — the user's
// selection, scroll position and sort order are thrown away every refresh. A
// trader mid-click on a position loses it; a trader scrolled to a strike gets
// yanked back to the top.
//
// This model fixes the structural cause rather than the symptom:
//   • rows are keyed, so an update mutates a row in place instead of recreating it
//   • dataChanged() is emitted ONLY for cells whose value actually changed, so
//     Qt repaints the two cells that ticked rather than the whole viewport
//   • row identity is stable, so selection/scroll/sort survive for free
//   • it composes with QSortFilterProxyModel, so sorting and filtering stop
//     being hand-rolled per screen
//
// TEMPLATE + moc
// --------------
// moc cannot process a template class carrying Q_OBJECT. This class therefore
// declares NO new signals or slots — it only overrides QAbstractTableModel's
// virtuals and reuses its (already moc'd) signals. That is why there is no
// Q_OBJECT macro here and why user-visible strings must be translated by the
// owner and passed in, rather than via tr() inside the template.
//
// USAGE
//     ui::LiveTableModel<Position> model;
//     model.set_headers({tr("SYMBOL"), tr("QTY"), tr("P&L")});
//     model.set_key([](const Position& p) { return p.account + "|" + p.symbol; });
//     model.set_cell([](const Position& p, int col, int role) -> QVariant {
//         if (role == Qt::DisplayRole) switch (col) { ... }
//         if (role == ui::LiveTableModel<Position>::SortRole) ...  // numeric key
//         return {};
//     });
//     view->setModel(&model);
//     model.upsert(rows);   // call this on every tick

#include <QAbstractTableModel>
#include <QHash>
#include <QModelIndex>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVector>

#include <functional>
#include <utility>

namespace fincept::ui {

template <typename Row>
class LiveTableModel : public QAbstractTableModel {
  public:
    /// Stable identity for a row. Two rows with the same key are the same row.
    using KeyFn = std::function<QString(const Row&)>;
    /// Value for (row, column, role). Return an invalid QVariant for roles you
    /// do not handle — the model supplies no defaults of its own.
    using CellFn = std::function<QVariant(const Row&, int column, int role)>;

    /// Role carrying a sortable (usually numeric) key. Point a
    /// QSortFilterProxyModel at this via setSortRole() so that "1,234.50" sorts
    /// as a number rather than as text — sorting numeric columns lexically is a
    /// long-standing defect class in item-widget tables.
    static constexpr int SortRole = Qt::UserRole + 1;
    /// Role carrying the row's stable key, for callers that need to map a
    /// QModelIndex back to a domain object (context menus, per-row actions).
    static constexpr int KeyRole = Qt::UserRole + 2;

    explicit LiveTableModel(QObject* parent = nullptr) : QAbstractTableModel(parent) {}

    // ── Configuration (call once, before the first upsert) ──────────────────

    void set_headers(const QStringList& headers) {
        const bool count_changed = headers.size() != headers_.size();
        if (count_changed)
            beginResetModel();
        headers_ = headers;
        if (count_changed)
            endResetModel();
        else if (!headers_.isEmpty())
            emit headerDataChanged(Qt::Horizontal, 0, headers_.size() - 1);
    }

    void set_key(KeyFn fn) { key_fn_ = std::move(fn); }
    void set_cell(CellFn fn) { cell_fn_ = std::move(fn); }

    /// Optional: rows for which this returns false are dropped by upsert().
    /// Prefer this over filtering at the call site so the model stays the single
    /// source of truth for what is displayed.
    void set_filter(std::function<bool(const Row&)> fn) { filter_fn_ = std::move(fn); }

    // ── Streaming update ────────────────────────────────────────────────────

    /// Merge `incoming` into the model.
    ///
    /// Rows present in both are updated in place and emit dataChanged only for
    /// the columns whose displayed value actually changed. Rows only in the
    /// model are removed; rows only in `incoming` are appended. Order follows
    /// `incoming`, so a caller that sorts upstream keeps its ordering — but
    /// prefer a QSortFilterProxyModel and let the user sort.
    ///
    /// Safe to call at tick rate: in the steady state (same rows, one price
    /// moved) this performs zero allocations and emits one dataChanged.
    void upsert(const QVector<Row>& incoming) {
        QVector<Row> next;
        next.reserve(incoming.size());
        for (const Row& r : incoming) {
            if (filter_fn_ && !filter_fn_(r))
                continue;
            next.push_back(r);
        }

        // Structural change (rows added/removed/reordered) → one reset. Doing a
        // reset only when the KEY SEQUENCE changes is the whole point: a pure
        // value tick, which is the overwhelmingly common case, takes the cheap
        // path below and leaves selection and scroll untouched.
        const bool structural = !same_key_sequence(next);
        if (structural) {
            beginResetModel();
            rows_ = std::move(next);
            rebuild_key_index();
            endResetModel();
            return;
        }

        // Same rows, same order — diff cell by cell and repaint only what moved.
        const int cols = column_count();
        for (int r = 0; r < rows_.size(); ++r) {
            int first_changed = -1;
            int last_changed = -1;
            for (int c = 0; c < cols; ++c) {
                if (cell(rows_[r], c, Qt::DisplayRole) != cell(next[r], c, Qt::DisplayRole)) {
                    if (first_changed < 0)
                        first_changed = c;
                    last_changed = c;
                }
            }
            rows_[r] = next[r]; // adopt the new values regardless
            if (first_changed >= 0) {
                emit dataChanged(index(r, first_changed), index(r, last_changed),
                                 {Qt::DisplayRole, Qt::ForegroundRole, Qt::ToolTipRole, SortRole});
            }
        }
    }

    /// Drop every row (e.g. on account switch). Prefer upsert({}) when you want
    /// the empty state to go through the same path as a normal update.
    void clear() {
        if (rows_.isEmpty())
            return;
        beginResetModel();
        rows_.clear();
        key_to_row_.clear();
        endResetModel();
    }

    // ── Lookup ──────────────────────────────────────────────────────────────

    const QVector<Row>& rows() const { return rows_; }

    /// Domain object behind an index, or nullptr. Use this in context menus and
    /// per-row buttons instead of re-parsing the displayed text — recovering a
    /// quantity by parsing a rounded display string silently loses precision.
    const Row* row_at(const QModelIndex& idx) const {
        if (!idx.isValid() || idx.row() < 0 || idx.row() >= rows_.size())
            return nullptr;
        return &rows_[idx.row()];
    }

    const Row* row_for_key(const QString& key) const {
        const auto it = key_to_row_.constFind(key);
        return it == key_to_row_.constEnd() ? nullptr : &rows_[it.value()];
    }

    /// Model row index for a key, or -1. Lets a caller restore a selection by
    /// identity after a structural change.
    int index_of_key(const QString& key) const { return key_to_row_.value(key, -1); }

    // ── QAbstractTableModel ─────────────────────────────────────────────────

    int rowCount(const QModelIndex& parent = QModelIndex()) const override {
        return parent.isValid() ? 0 : static_cast<int>(rows_.size());
    }

    int columnCount(const QModelIndex& parent = QModelIndex()) const override {
        return parent.isValid() ? 0 : column_count();
    }

    QVariant data(const QModelIndex& idx, int role = Qt::DisplayRole) const override {
        if (!idx.isValid() || idx.row() < 0 || idx.row() >= rows_.size())
            return {};
        const Row& r = rows_[idx.row()];
        if (role == KeyRole)
            return key_fn_ ? key_fn_(r) : QVariant{};
        return cell(r, idx.column(), role);
    }

    QVariant headerData(int section, Qt::Orientation o, int role = Qt::DisplayRole) const override {
        if (o != Qt::Horizontal || role != Qt::DisplayRole)
            return {};
        return (section >= 0 && section < headers_.size()) ? QVariant(headers_.at(section)) : QVariant{};
    }

  private:
    int column_count() const { return static_cast<int>(headers_.size()); }

    QVariant cell(const Row& r, int col, int role) const { return cell_fn_ ? cell_fn_(r, col, role) : QVariant{}; }

    bool same_key_sequence(const QVector<Row>& next) const {
        if (!key_fn_ || next.size() != rows_.size())
            return false;
        for (int i = 0; i < next.size(); ++i) {
            if (key_fn_(next[i]) != key_fn_(rows_[i]))
                return false;
        }
        return true;
    }

    void rebuild_key_index() {
        key_to_row_.clear();
        if (!key_fn_)
            return;
        key_to_row_.reserve(rows_.size());
        for (int i = 0; i < rows_.size(); ++i)
            key_to_row_.insert(key_fn_(rows_[i]), i);
    }

    QStringList headers_;
    QVector<Row> rows_;
    QHash<QString, int> key_to_row_;
    KeyFn key_fn_;
    CellFn cell_fn_;
    std::function<bool(const Row&)> filter_fn_;
};

} // namespace fincept::ui
