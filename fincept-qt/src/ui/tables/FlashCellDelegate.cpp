#include "ui/tables/FlashCellDelegate.h"

#include "ui/theme/Theme.h"

#include <QApplication>
#include <QFontMetrics>
#include <QPainter>

#include <algorithm>
#include <cmath>

namespace fincept::ui {

FlashCellDelegate::FlashCellDelegate(int flash_role, QObject* parent)
    : QStyledItemDelegate(parent), flash_role_(flash_role) {}

void FlashCellDelegate::paint(QPainter* p, const QStyleOptionViewItem& option, const QModelIndex& idx) const {
    QStyleOptionViewItem opt(option);
    initStyleOption(&opt, idx);
    const QString text = opt.text;
    opt.text.clear();
    QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &opt, p, opt.widget); // background, selection, focus

    const bool group = group_role_ >= 0 && idx.data(group_role_).toBool();
    p->save();
    if (group)
        p->fillRect(option.rect, colors::overlay_color(0.05));

    // Tick flash: the direction colour fading out over ~1 s, under the text.
    const double f = idx.data(flash_role_).toDouble();
    if (f != 0.0 && std::isfinite(f)) {
        QColor c(f > 0 ? colors::POSITIVE() : colors::NEGATIVE());
        c.setAlphaF(static_cast<float>(std::clamp(std::abs(f), 0.0, 1.0) * 0.32));
        p->fillRect(option.rect, c);
    }

    // A group's label (column 0) is left to the view (Watchlist paints it in drawRow so
    // it can run on over the empty cells beside it instead of eliding to the column).
    const bool group_label = group && idx.column() == 0;
    if (!text.isEmpty() && !group_label) {
        QFont font = opt.font;
        if (group)
            font.setBold(true);
        p->setFont(font);
        p->setPen(opt.palette.color(QPalette::Text));
        const QRect r = option.rect.adjusted(6, 0, -6, 0);
        const QString shown = QFontMetrics(font).elidedText(text, Qt::ElideRight, r.width());
        p->drawText(r, static_cast<int>(opt.displayAlignment) | Qt::TextSingleLine, shown);
    }
    p->restore();
}

} // namespace fincept::ui
