#pragma once
// RangeBarDelegate — paints a RangeBar (low…high track with the current value,
// targets, bands) inside a table cell, with a hover tooltip. Not a Q_OBJECT.
//
// The model supplies the data either as one RangeBarData:
//     item->setData(QVariant::fromValue(data), ui::RangeBarDelegate::DataRole);
// or as three numbers (the 52-week-range case):
//     LowRole, HighRole, ValueRole (the Current marker).
//
// USAGE
//     auto* d = new ui::RangeBarDelegate(view);
//     d->set_formatter([](double v) { return QString::number(v, 'f', 2); });
//     view->setItemDelegateForColumn(kRangeColumn, d);

#include "ui/charts/RangeBar.h"

#include <QStyledItemDelegate>

#include <functional>

namespace fincept::ui {

class RangeBarDelegate : public QStyledItemDelegate {
  public:
    enum Role {
        DataRole = Qt::UserRole + 0x5201, // QVariant::fromValue<RangeBarData>
        LowRole,                          // double
        HighRole,                         // double
        ValueRole,                        // double — drawn as the Current marker
    };

    explicit RangeBarDelegate(QObject* parent = nullptr);

    /// Formats low / high / marker values. Default: compact number.
    void set_formatter(std::function<QString(double)> formatter) { formatter_ = std::move(formatter); }
    /// Low / high values beside the track when the cell is wide enough. Default true.
    void set_show_labels(bool show) { show_labels_ = show; }

    /// The RangeBarData for `index` (from DataRole, else Low/High/ValueRole);
    /// `ok` is false when the cell has no usable range.
    static RangeBarData data_for(const QModelIndex& index, bool* ok = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    bool helpEvent(QHelpEvent* event, QAbstractItemView* view, const QStyleOptionViewItem& option,
                   const QModelIndex& index) override;

  private:
    std::function<QString(double)> formatter_;
    bool show_labels_ = true;
};

} // namespace fincept::ui
