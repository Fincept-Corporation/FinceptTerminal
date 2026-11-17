#pragma once
// SearchDelegate — paints the popup's rows from the live theme (no widget per row):
//   heading   GROUP NAME · count
//   result    [CHIP] lead title (matched letters emphasised)            [Chart][Trade][Watch] / hint
//                  what Enter does, in muted words
//   loading   [░░░] Searching symbols…
// Sizes come from font metrics (base font 9 / 14 / 24); right-to-left mirrored.
// action_at() hit-tests the hover buttons of a result row.

#include "ui/navigation/search/SearchRow.h"

#include <QStyledItemDelegate>

namespace fincept::ui::search {

class SearchDelegate : public QStyledItemDelegate {
    Q_OBJECT
  public:
    explicit SearchDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    /// Index into SearchRow::actions of the hover button at `pos` (view coordinates), -1 = none.
    int action_at(const QStyleOptionViewItem& option, const QModelIndex& index, const QPoint& pos) const;
    /// Hover buttons painted per row (the rest stay in the Tab / right-click menu).
    static constexpr int kMaxButtons = 3;

  private:
    struct Geometry;
    Geometry layout(const QStyleOptionViewItem& option, const SearchRow& row) const;
};

} // namespace fincept::ui::search
