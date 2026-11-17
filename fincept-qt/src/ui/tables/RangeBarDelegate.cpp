#include "ui/tables/RangeBarDelegate.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QHelpEvent>
#include <QPainter>
#include <QToolTip>

#include <algorithm>
#include <cmath>

namespace fincept::ui {

RangeBarDelegate::RangeBarDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

RangeBarData RangeBarDelegate::data_for(const QModelIndex& index, bool* ok) {
    RangeBarData d;
    bool valid = false;
    const QVariant packed = index.data(DataRole);
    if (packed.metaType() == QMetaType::fromType<RangeBarData>()) {
        d = packed.value<RangeBarData>();
        valid = true;
    } else {
        bool ok_lo = false;
        bool ok_hi = false;
        d.low = index.data(LowRole).toDouble(&ok_lo);
        d.high = index.data(HighRole).toDouble(&ok_hi);
        valid = ok_lo && ok_hi;
        bool ok_v = false;
        const double v = index.data(ValueRole).toDouble(&ok_v);
        if (valid && ok_v && std::isfinite(v))
            d.markers.append(RangeMarker{v, QString(), RangeMarker::Role::Current});
    }
    valid = valid && std::isfinite(d.low) && std::isfinite(d.high) && d.high > d.low;
    if (ok)
        *ok = valid;
    return d;
}

void RangeBarDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    opt.text.clear(); // background / selection only
    QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

    bool ok = false;
    const RangeBarData d = data_for(index, &ok);
    if (!ok)
        return;
    const QRectF cell = QRectF(option.rect).adjusted(4, 1, -4, -1);
    RangeBar::paint_range(*painter, cell, d, formatter_, show_labels_);
}

QSize RangeBarDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const {
    QSize s = QStyledItemDelegate::sizeHint(option, index);
    s.setWidth(std::max(s.width(), show_labels_ ? 200 : 100));
    return s;
}

bool RangeBarDelegate::helpEvent(QHelpEvent* event, QAbstractItemView* view, const QStyleOptionViewItem& option,
                                 const QModelIndex& index) {
    if (event && event->type() == QEvent::ToolTip) {
        bool ok = false;
        const RangeBarData d = data_for(index, &ok);
        if (ok) {
            QToolTip::showText(event->globalPos(), RangeBar::tooltip_for(d, formatter_), view, option.rect);
            return true;
        }
    }
    return QStyledItemDelegate::helpEvent(event, view, option, index);
}

} // namespace fincept::ui
