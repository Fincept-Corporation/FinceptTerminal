#pragma once
// WatchlistView — the Watchlist quote table: a QTreeView over
// WatchlistProxyModel with
//   * per-column delegates (range bars, sparklines, heat cells, data bars, and
//     a default cell delegate that paints the tick flash and group rows),
//   * an id-keyed column layout (WatchlistLayout) — header right-click is the
//     column chooser, sections can be dragged to reorder, sort indicator
//     cycles ascending / descending / manual order,
//   * row drags that carry the shared symbol MIME (drop on any panel) plus the
//     watchlist rows MIME (drop inside the table to reorder / regroup),
//   * multi-selection.
// WatchlistFooter paints aggregate values under the table, aligned with the
// view's header sections.

#include "screens/watchlist/WatchlistColumnPrefs.h"
#include "screens/watchlist/WatchlistModel.h"
#include "ui/tables/FlashCellDelegate.h"

#include <QHash>
#include <QPointer>
#include <QStyledItemDelegate>
#include <QTreeView>
#include <QWidget>

namespace fincept::screens {

/// Default cell painter: tick flash, group-row emphasis, elided text in the
/// model's colour — the shared ui::FlashCellDelegate on the Watchlist roles. Not a QObject.
class WatchlistCellDelegate : public ui::FlashCellDelegate {
  public:
    explicit WatchlistCellDelegate(QObject* parent = nullptr) : FlashCellDelegate(WatchlistModel::FlashRole, parent) {
        set_group_role(WatchlistModel::IsGroupRole);
    }
};

class WatchlistView : public QTreeView {
    Q_OBJECT
  public:
    explicit WatchlistView(QWidget* parent = nullptr);

    void set_models(WatchlistModel* source, WatchlistProxyModel* proxy);
    WatchlistModel* source_model() const { return source_; }
    WatchlistProxyModel* proxy_model() const { return proxy_; }

    // ── Layout ──────────────────────────────────────────────────────────────
    /// Visible columns, their order, widths and the sort.
    void apply_layout(const WatchlistLayout& layout);
    /// Fill `layout`'s column fields (visible, widths, sort) from the view.
    void capture_layout(WatchlistLayout& layout) const;
    QStringList visible_column_ids() const;
    void set_column_visible(const QString& id, bool visible);
    /// Default width of a registry column at the current font.
    int default_width(int column) const;
    /// Clear the sort (back to the list's manual order).
    void clear_sort();
    bool is_sorted() const;

    // ── Rows ────────────────────────────────────────────────────────────────
    /// Selected symbols in display order.
    QStringList selected_symbols() const;
    /// Symbol of the current (focused) row, or empty.
    QString current_symbol() const;
    /// Select `symbols` (those shown) and make `current` the current row,
    /// without emitting user-selection signals to the caller's handlers.
    void select_symbols(const QStringList& symbols, const QString& current);
    /// Symbols of the rows in or near the viewport (`margin` rows above / below).
    QStringList visible_symbols(int margin = 5) const;
    /// Every shown symbol row in display order (members of collapsed groups included).
    QStringList display_order() const;
    /// Group keys currently collapsed / expand everything except `collapsed`.
    QStringList collapsed_groups() const;
    void restore_expansion(const QStringList& collapsed);

  signals:
    /// The user resized, moved, hid / showed or sorted columns.
    void layout_edited();
    /// The set of visible columns changed (data needs may differ).
    void visible_columns_changed();
    /// Scrolled or resized: a different set of rows is on screen.
    void viewport_rows_changed();
    /// A group row was collapsed or expanded by the user.
    void expansion_edited();

  protected:
    void startDrag(Qt::DropActions supported) override;
    /// Group rows: the label (in the symbol column) runs on over the empty cells to its right.
    void drawRow(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    void scrollContentsBy(int dx, int dy) override;
    void resizeEvent(QResizeEvent* e) override;

  private:
    void install_delegates();
    void show_header_menu(const QPoint& pos);
    void on_section_moved(int logical, int old_visual, int new_visual);

    WatchlistModel* source_ = nullptr;
    WatchlistProxyModel* proxy_ = nullptr;
    bool applying_ = false;
};

/// One line of aggregates under the table, aligned with its columns.
class WatchlistFooter : public QWidget {
    Q_OBJECT
  public:
    struct Cell {
        QString text;
        QColor color; ///< invalid = secondary text colour
        QString tooltip;
    };

    explicit WatchlistFooter(QTreeView* view, QWidget* parent = nullptr);
    /// Cells keyed by model (logical) column.
    void set_cells(const QHash<int, Cell>& cells);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return sizeHint(); }

  protected:
    void paintEvent(QPaintEvent* e) override;
    bool event(QEvent* e) override;

  private:
    int column_at(int x) const;

    QPointer<QTreeView> view_;
    QHash<int, Cell> cells_;
};

} // namespace fincept::screens
