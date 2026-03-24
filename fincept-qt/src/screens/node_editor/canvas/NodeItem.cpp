#include "screens/node_editor/canvas/NodeItem.h"

#include "screens/node_editor/canvas/EdgeItem.h"
#include "screens/node_editor/canvas/PortItem.h"

#include <QAction>
#include <QFont>
#include <QFontMetrics>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QPainter>

#include <algorithm>

namespace fincept::workflow {

NodeItem::NodeItem(const NodeDef& def, const NodeTypeDef& type_def, QGraphicsItem* parent)
    : QGraphicsObject(parent), def_(def), type_def_(type_def) {
    setFlag(ItemIsMovable);
    setFlag(ItemIsSelectable);
    setFlag(ItemSendsGeometryChanges);
    setFlag(ItemIsFocusable);
    setAcceptHoverEvents(true);
    setZValue(1);

    setPos(def_.x, def_.y);

    rebuild_layout();
}

void NodeItem::set_name(const QString& name) {
    def_.name = name;
    invalidate_cache();
}

void NodeItem::set_parameter(const QString& key, const QJsonValue& value) {
    def_.parameters[key] = value;
    invalidate_cache();
}

void NodeItem::set_execution_state(const QString& state) {
    execution_state_ = state;
    invalidate_cache();
}

PortItem* NodeItem::find_port(const QString& port_id) const {
    for (auto* p : input_ports_)
        if (p->def().id == port_id)
            return p;
    for (auto* p : output_ports_)
        if (p->def().id == port_id)
            return p;
    return nullptr;
}

void NodeItem::rebuild_layout() {
    // Clear existing ports
    for (auto* p : input_ports_) { /* owned by scene via parent */
    }
    input_ports_.clear();
    output_ports_.clear();

    // Create input ports
    for (int i = 0; i < type_def_.inputs.size(); ++i) {
        auto* port = new PortItem(type_def_.inputs[i], this);
        qreal y = kHeaderHeight + kPortPadding + i * kPortSpacing;
        port->setPos(0, y);
        input_ports_.append(port);
    }

    // Create output ports
    for (int i = 0; i < type_def_.outputs.size(); ++i) {
        auto* port = new PortItem(type_def_.outputs[i], this);
        qreal y = kHeaderHeight + kPortPadding + i * kPortSpacing;
        port->setPos(kWidth, y);
        output_ports_.append(port);
    }

    invalidate_cache();
}

void NodeItem::invalidate_cache() {
    cache_dirty_ = true;
    update();
}

QRectF NodeItem::boundingRect() const {
    int max_ports = std::max(type_def_.inputs.size(), type_def_.outputs.size());
    qreal body_h = std::max(kMinBodyHeight, max_ports * kPortSpacing + kPortPadding * 2);
    return QRectF(0, 0, kWidth, kHeaderHeight + body_h);
}

void NodeItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) {
    QRectF rect = boundingRect();

    if (cache_dirty_ || cache_.size() != rect.size().toSize()) {
        cache_ = QPixmap(rect.size().toSize() * 2); // 2x for retina
        cache_.setDevicePixelRatio(2.0);
        cache_.fill(Qt::transparent);

        QPainter cp(&cache_);
        cp.setRenderHint(QPainter::Antialiasing, false);

        QFont mono("Consolas", 10);
        mono.setStyleHint(QFont::Monospace);

        // ── Node body ──────────────────────────────────────────────
        cp.setPen(Qt::NoPen);
        cp.setBrush(QColor("#181818"));
        cp.drawRect(rect);

        // ── Header ─────────────────────────────────────────────────
        QRectF header(0, 0, kWidth, kHeaderHeight);
        cp.setBrush(QColor("#1e1e1e"));
        cp.drawRect(header);

        // Accent bar (left edge of header)
        QColor accent(type_def_.accent_color.isEmpty() ? "#d97706" : type_def_.accent_color);
        cp.setPen(Qt::NoPen);
        cp.setBrush(accent);
        cp.drawRect(QRectF(0, 0, 3, kHeaderHeight));

        // Icon text
        QFont icon_font("Consolas", 8);
        icon_font.setBold(true);
        cp.setFont(icon_font);
        cp.setPen(accent);
        cp.drawText(QRectF(8, 0, 24, kHeaderHeight), Qt::AlignVCenter, type_def_.icon_text);

        // Node name
        mono.setPointSize(9);
        mono.setBold(true);
        cp.setFont(mono);
        cp.setPen(QColor("#e5e5e5"));
        QRectF name_rect(34, 0, kWidth - 44, kHeaderHeight);
        QString elided = QFontMetrics(mono).elidedText(def_.name, Qt::ElideRight, (int)name_rect.width());
        cp.drawText(name_rect, Qt::AlignVCenter, elided);

        // ── Port labels ────────────────────────────────────────────
        mono.setPointSize(8);
        mono.setBold(false);
        cp.setFont(mono);
        cp.setPen(QColor("#808080"));

        for (int i = 0; i < type_def_.inputs.size(); ++i) {
            qreal y = kHeaderHeight + kPortPadding + i * kPortSpacing;
            cp.drawText(QRectF(14, y - 8, kWidth / 2 - 20, 16), Qt::AlignVCenter | Qt::AlignLeft,
                        type_def_.inputs[i].label);
        }

        for (int i = 0; i < type_def_.outputs.size(); ++i) {
            qreal y = kHeaderHeight + kPortPadding + i * kPortSpacing;
            cp.drawText(QRectF(kWidth / 2 + 6, y - 8, kWidth / 2 - 20, 16), Qt::AlignVCenter | Qt::AlignRight,
                        type_def_.outputs[i].label);
        }

        // ── Execution state indicator ──────────────────────────────
        if (execution_state_ != "idle") {
            QColor state_color;
            if (execution_state_ == "running")
                state_color = QColor("#d97706");
            else if (execution_state_ == "completed")
                state_color = QColor("#16a34a");
            else if (execution_state_ == "error")
                state_color = QColor("#dc2626");

            cp.setPen(Qt::NoPen);
            cp.setBrush(state_color);
            cp.drawRect(QRectF(kWidth - 8, 4, 4, 4));
        }

        cp.end();
        cache_dirty_ = false;
    }

    // Blit cached pixmap
    painter->drawPixmap(0, 0, cache_);

    // ── Border (not cached — changes with selection/hover) ─────────
    QColor border_color;
    if (def_.disabled)
        border_color = QColor("#4a4a4a");
    else if (isSelected())
        border_color = QColor("#d97706");
    else if (execution_state_ == "error")
        border_color = QColor("#dc2626");
    else if (hovered_)
        border_color = QColor("#4a4a4a");
    else
        border_color = QColor("#2a2a2a");

    painter->setPen(QPen(border_color, 1.0));
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(rect.adjusted(0.5, 0.5, -0.5, -0.5));

    // Disabled overlay
    if (def_.disabled) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(0, 0, 0, 140));
        painter->drawRect(rect);
    }
}

QVariant NodeItem::itemChange(GraphicsItemChange change, const QVariant& value) {
    if (change == ItemPositionHasChanged) {
        QPointF pos = value.toPointF();
        def_.x = pos.x();
        def_.y = pos.y();
        emit node_moved(def_.id, pos.x(), pos.y());

        // Update connected edges
        for (auto* p : input_ports_)
            for (auto* e : p->edges())
                if (auto* edge = dynamic_cast<class EdgeItem*>(static_cast<QGraphicsItem*>(e)))
                    ; // edges adjust in NodeScene via signal
        for (auto* p : output_ports_)
            for (auto* e : p->edges())
                if (auto* edge = dynamic_cast<class EdgeItem*>(static_cast<QGraphicsItem*>(e)))
                    ;
    }

    if (change == ItemSelectedHasChanged) {
        if (value.toBool())
            emit node_selected(def_.id);
        else
            emit node_deselected(def_.id);
    }

    return QGraphicsObject::itemChange(change, value);
}

void NodeItem::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        emit delete_requested(def_.id);
        event->accept();
    } else {
        QGraphicsObject::keyPressEvent(event);
    }
}

void NodeItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) {
    // Reserved for future inline editing
    QGraphicsObject::mouseDoubleClickEvent(event);
}

void NodeItem::contextMenuEvent(QGraphicsSceneContextMenuEvent* event) {
    QMenu menu;
    menu.setStyleSheet("QMenu { background: #1e1e1e; color: #e5e5e5; border: 1px solid #2a2a2a;"
                       "  font-family: Consolas; font-size: 12px; }"
                       "QMenu::item { padding: 4px 20px; }"
                       "QMenu::item:selected { background: #d97706; color: #080808; }"
                       "QMenu::separator { background: #2a2a2a; height: 1px; margin: 2px 6px; }");

    auto* duplicate_action = menu.addAction("Duplicate");
    auto* disable_action = menu.addAction(def_.disabled ? "Enable" : "Disable");
    menu.addSeparator();
    auto* exec_from_action = menu.addAction("Execute From Here");
    menu.addSeparator();
    auto* delete_action = menu.addAction("Delete");
    delete_action->setShortcut(QKeySequence::Delete);

    auto* chosen = menu.exec(event->screenPos());
    if (!chosen)
        return;

    if (chosen == duplicate_action) {
        emit duplicate_requested(def_.id);
    } else if (chosen == disable_action) {
        def_.disabled = !def_.disabled;
        invalidate_cache();
    } else if (chosen == exec_from_action) {
        emit execute_from_requested(def_.id);
    } else if (chosen == delete_action) {
        emit delete_requested(def_.id);
    }
}

void NodeItem::hoverEnterEvent(QGraphicsSceneHoverEvent* event) {
    hovered_ = true;
    update();
    QGraphicsObject::hoverEnterEvent(event);
}

void NodeItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event) {
    hovered_ = false;
    update();
    QGraphicsObject::hoverLeaveEvent(event);
}

} // namespace fincept::workflow
