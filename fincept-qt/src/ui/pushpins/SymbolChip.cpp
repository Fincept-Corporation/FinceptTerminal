#include "ui/pushpins/SymbolChip.h"

#include "core/symbol/SymbolContext.h"
#include "core/symbol/SymbolDragSource.h"
#include "core/symbol/SymbolGroupRegistry.h"
#include "services/pushpins/PushpinService.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QFontMetrics>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>

namespace fincept::ui {

SymbolChip::SymbolChip(SymbolRef ref, QWidget* parent) : QWidget(parent), ref_(std::move(ref)) {
    setCursor(Qt::PointingHandCursor);
    setToolTip(ref_.display() + "\nLeft-click: broadcast to Group A\nDrag: send to any panel\nRight-click: menu");
    setAttribute(Qt::WA_Hover);
}

QSize SymbolChip::sizeHint() const {
    QFontMetrics fm(font());
    return QSize(fm.horizontalAdvance(ref_.symbol) + 18, fm.height() + 8);
}

void SymbolChip::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        press_pos_ = e->pos();
        pressed_ = true;
        e->accept();
        return;
    }
    QWidget::mousePressEvent(e);
}

void SymbolChip::mouseMoveEvent(QMouseEvent* e) {
    if (!pressed_ || !(e->buttons() & Qt::LeftButton))
        return;
    if ((e->pos() - press_pos_).manhattanLength() < QApplication::startDragDistance())
        return;
    // Threshold crossed — hand off to the drag machinery. Clear pressed_
    // so the subsequent mouseReleaseEvent doesn't also fire the click.
    pressed_ = false;
    symbol_dnd::startSymbolDrag(this, ref_);
}

void SymbolChip::mouseReleaseEvent(QMouseEvent* e) {
    const bool was_click = pressed_ && e->button() == Qt::LeftButton;
    pressed_ = false;
    if (was_click)
        broadcast_to_group(SymbolGroup::A);
    QWidget::mouseReleaseEvent(e);
}

void SymbolChip::contextMenuEvent(QContextMenuEvent* e) {
    QMenu menu(this);

    auto* copy = menu.addAction("Copy Ticker");
    connect(copy, &QAction::triggered, this, [this]() {
        QApplication::clipboard()->setText(ref_.symbol);
    });

    auto* remove = menu.addAction("Remove Pin");
    connect(remove, &QAction::triggered, this, [this]() {
        emit remove_requested(ref_);
        PushpinService::instance().unpin(ref_);
    });

    menu.addSeparator();
    auto& registry = SymbolGroupRegistry::instance();
    for (SymbolGroup g : registry.enabled_groups()) {
        auto* act = menu.addAction(QStringLiteral("Broadcast to %1  (%2)")
                                       .arg(registry.name(g))
                                       .arg(symbol_group_letter(g)));
        connect(act, &QAction::triggered, this, [this, g]() { broadcast_to_group(g); });
    }
    menu.exec(e->globalPos());
}

void SymbolChip::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);

    QPainterPath path;
    path.addRoundedRect(r, r.height() / 2.0, r.height() / 2.0);
    p.fillPath(path, QColor("#1f2937")); // slate-800
    p.setPen(QColor("#d97706")); // amber border — echoes Bloomberg's amber
    p.drawPath(path);

    p.setPen(QColor("#f3f4f6"));
    QFont f = p.font();
    f.setBold(true);
    p.setFont(f);
    p.drawText(r, Qt::AlignCenter, ref_.symbol);
}

void SymbolChip::broadcast_to_group(SymbolGroup g) {
    if (g == SymbolGroup::None)
        return;
    SymbolContext::instance().set_group_symbol(g, ref_, this);
}

} // namespace fincept::ui
