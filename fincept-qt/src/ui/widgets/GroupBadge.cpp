#include "ui/widgets/GroupBadge.h"

#include <QAction>
#include <QContextMenuEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

namespace fincept::ui {

GroupBadge::GroupBadge(QWidget* parent) : QWidget(parent) {
    setCursor(Qt::PointingHandCursor);
    setToolTip("Symbol link group — click to cycle, right-click for menu");
    setAttribute(Qt::WA_Hover);
}

QColor GroupBadge::color_for_group(SymbolGroup g) {
    // Bloomberg-ish six-colour palette, tuned to remain readable on both
    // dark and light themes.
    switch (g) {
        case SymbolGroup::A: return QColor("#d97706"); // amber
        case SymbolGroup::B: return QColor("#0891b2"); // cyan
        case SymbolGroup::C: return QColor("#c026d3"); // magenta
        case SymbolGroup::D: return QColor("#16a34a"); // green
        case SymbolGroup::E: return QColor("#7c3aed"); // purple
        case SymbolGroup::F: return QColor("#dc2626"); // red
        case SymbolGroup::None: return QColor("#4b5563"); // slate (unlinked)
    }
    return QColor("#4b5563");
}

void GroupBadge::set_group_silent(SymbolGroup g) {
    if (group_ == g)
        return;
    group_ = g;
    update();
}

void GroupBadge::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        emit group_change_requested(next_symbol_group(group_));
        e->accept();
        return;
    }
    QWidget::mousePressEvent(e);
}

void GroupBadge::contextMenuEvent(QContextMenuEvent* e) {
    QMenu menu(this);
    auto* unlink = menu.addAction("Unlink");
    unlink->setEnabled(group_ != SymbolGroup::None);
    connect(unlink, &QAction::triggered, this,
            [this]() { emit group_change_requested(SymbolGroup::None); });
    menu.addSeparator();
    for (SymbolGroup g : all_symbol_groups()) {
        auto* act = menu.addAction(QString("Link to Group %1").arg(symbol_group_letter(g)));
        act->setCheckable(true);
        act->setChecked(g == group_);
        connect(act, &QAction::triggered, this, [this, g]() { emit group_change_requested(g); });
    }
    menu.exec(e->globalPos());
}

void GroupBadge::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QColor fill = color_for_group(group_);
    const QRectF r = QRectF(rect()).adjusted(1.0, 1.0, -1.0, -1.0);

    QPainterPath path;
    path.addRoundedRect(r, 3.0, 3.0);
    p.fillPath(path, fill);

    // Letter
    p.setPen(QColor("#ffffff"));
    QFont f = p.font();
    f.setPointSizeF(f.pointSizeF() - 1.0);
    f.setBold(true);
    p.setFont(f);
    const QString label = group_ == SymbolGroup::None
                              ? QStringLiteral("·") // middle dot for unlinked
                              : QString(symbol_group_letter(group_));
    p.drawText(r, Qt::AlignCenter, label);
}

QSize GroupBadge::sizeHint() const {
    return QSize(18, 18);
}

} // namespace fincept::ui
