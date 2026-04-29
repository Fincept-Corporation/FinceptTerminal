#include "ui/widgets/GroupBadge.h"

#include "core/symbol/SymbolGroupRegistry.h"

#include <QAction>
#include <QColorDialog>
#include <QContextMenuEvent>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>

namespace fincept::ui {

GroupBadge::GroupBadge(QWidget* parent) : QWidget(parent) {
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_Hover);

    // Repaint + refresh tooltip when any registry mutation lands. Cheap —
    // the slot count is at most 10 and we only repaint our own widget.
    connect(&SymbolGroupRegistry::instance(),
            &SymbolGroupRegistry::group_metadata_changed, this,
            [this](SymbolGroup) { set_group_silent(group_); });

    set_group_silent(SymbolGroup::None); // seeds the tooltip
}

QColor GroupBadge::color_for_group(SymbolGroup g) {
    if (g == SymbolGroup::None)
        return QColor("#4b5563");
    return SymbolGroupRegistry::instance().color(g);
}

void GroupBadge::set_group_silent(SymbolGroup g) {
    group_ = g;
    const QString state = (g == SymbolGroup::None)
                              ? QStringLiteral("Unlinked")
                              : SymbolGroupRegistry::instance().name(g);
    setToolTip(QStringLiteral("Symbol link group: %1\nClick to cycle • Right-click for menu").arg(state));
    update();
}

void GroupBadge::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        emit group_change_requested(SymbolGroupRegistry::instance().next_enabled(group_));
        e->accept();
        return;
    }
    QWidget::mousePressEvent(e);
}

void GroupBadge::contextMenuEvent(QContextMenuEvent* e) {
    auto& registry = SymbolGroupRegistry::instance();
    QMenu menu(this);

    auto* unlink = menu.addAction("Unlink");
    unlink->setEnabled(group_ != SymbolGroup::None);
    connect(unlink, &QAction::triggered, this,
            [this]() { emit group_change_requested(SymbolGroup::None); });

    menu.addSeparator();

    // Link-to-<group> entries. All ten slots are listed; disabled slots are
    // shown greyed out so users can see the full inventory but can't cycle
    // into them. Names come from the registry so renames show up immediately.
    for (SymbolGroup g : all_symbol_groups()) {
        const bool en = registry.enabled(g);
        QString label = QStringLiteral("%1  (%2)").arg(registry.name(g)).arg(symbol_group_letter(g));
        if (!en)
            label += QStringLiteral("  — disabled");
        auto* act = menu.addAction(label);
        act->setCheckable(true);
        act->setChecked(g == group_);
        act->setEnabled(en);
        connect(act, &QAction::triggered, this, [this, g]() { emit group_change_requested(g); });
    }

    menu.addSeparator();

    // Customisation actions — only meaningful for a concrete slot.
    const SymbolGroup target = group_ == SymbolGroup::None ? SymbolGroup::A : group_;

    auto* rename = menu.addAction(QStringLiteral("Rename %1…").arg(registry.name(target)));
    QPointer<GroupBadge> self = this;
    connect(rename, &QAction::triggered, this, [self, target]() {
        if (!self) return;
        bool ok = false;
        const QString current = SymbolGroupRegistry::instance().name(target);
        const QString name = QInputDialog::getText(self, QStringLiteral("Rename Group"),
                                                   QStringLiteral("New name for %1:")
                                                       .arg(symbol_group_letter(target)),
                                                   QLineEdit::Normal, current, &ok);
        if (ok)
            SymbolGroupRegistry::instance().set_name(target, name);
    });

    auto* recolor = menu.addAction(QStringLiteral("Change Colour for %1…").arg(registry.name(target)));
    connect(recolor, &QAction::triggered, this, [self, target]() {
        if (!self) return;
        const QColor current = SymbolGroupRegistry::instance().color(target);
        const QColor picked = QColorDialog::getColor(current, self,
                                                     QStringLiteral("Group %1 Colour")
                                                         .arg(symbol_group_letter(target)));
        if (picked.isValid())
            SymbolGroupRegistry::instance().set_color(target, picked);
    });

    auto* toggle = menu.addAction(registry.enabled(target) ? QStringLiteral("Disable Slot %1")
                                                                  .arg(symbol_group_letter(target))
                                                            : QStringLiteral("Enable Slot %1")
                                                                  .arg(symbol_group_letter(target)));
    connect(toggle, &QAction::triggered, this, [target]() {
        auto& r = SymbolGroupRegistry::instance();
        r.set_enabled(target, !r.enabled(target));
    });

    auto* reset = menu.addAction(QStringLiteral("Reset %1 to Defaults").arg(symbol_group_letter(target)));
    connect(reset, &QAction::triggered, this,
            [target]() { SymbolGroupRegistry::instance().reset_to_default(target); });

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
