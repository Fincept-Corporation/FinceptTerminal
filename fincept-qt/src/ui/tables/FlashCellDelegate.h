#pragma once
// FlashCellDelegate — the default cell painter of the live quote tables
// (Watchlist, Markets): style background / selection / hover, an optional
// "group row" overlay, the tick flash (the direction colour fading out under the
// text) and the cell text elided in the model's colour and alignment. Not a
// QObject.
//
// The model supplies, per cell:
//   flash_role  double -1..1 — tick flash strength; the sign is the direction
//               (up = positive colour, down = negative). 0 / absent = no flash.
//   group_role  bool (optional, set_group_role) — rows flagged by it get a faint
//               overlay and bold text.
//
// USAGE
//     view->setItemDelegate(new ui::FlashCellDelegate(MyModel::FlashRole, view));

#include <QStyledItemDelegate>

namespace fincept::ui {

class FlashCellDelegate : public QStyledItemDelegate {
  public:
    explicit FlashCellDelegate(int flash_role, QObject* parent = nullptr);

    /// Rows whose `role` data is true get the group overlay and bold text. -1 (default) = none.
    void set_group_role(int role) { group_role_ = role; }
    int flash_role() const { return flash_role_; }
    int group_role() const { return group_role_; }

    void paint(QPainter* p, const QStyleOptionViewItem& option, const QModelIndex& idx) const override;

  private:
    int flash_role_;
    int group_role_ = -1;
};

} // namespace fincept::ui
