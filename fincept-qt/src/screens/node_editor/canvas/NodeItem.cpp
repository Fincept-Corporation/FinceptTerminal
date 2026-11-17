#include "screens/node_editor/canvas/NodeItem.h"
#include "ui/theme/Theme.h"

#include "screens/node_editor/canvas/EdgeItem.h"
#include "screens/node_editor/canvas/PortItem.h"
#include "ui/theme/ThemeManager.h"

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

namespace {

// The user's font at `offset` from the base size, capped at `max_px`: node
// geometry is fixed in scene units (the canvas zoom scales it), so text never
// grows past the row it is drawn in.
QFont node_font(int offset, int max_px, int weight = QFont::Normal) {
    QFont f = ui::fonts::font(offset, weight);
    if (f.pixelSize() > max_px)
        f.setPixelSize(max_px);
    return f;
}

} // namespace

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

    // The paint cache holds theme colours and fonts.
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this,
            [this](const ui::ThemeTokens&) { invalidate_cache(); });
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
    if (state != QLatin1String("invalid"))
        setToolTip(QString());
    invalidate_cache();
}

void NodeItem::set_validation_error(const QString& message) {
    execution_state_ = QStringLiteral("invalid");
    setToolTip(message);
    invalidate_cache();
}

void NodeItem::set_disabled(bool disabled) {
    if (def_.disabled == disabled)
        return;
    def_.disabled = disabled;
    invalidate_cache(); // border + dim overlay are disabled-dependent
}

void NodeItem::set_continue_on_fail(bool value) {
    def_.continue_on_fail = value; // no visual, but must reach serialize()
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
    // Ports are owned by scene via parent — just clear the lists
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

    // The cache is allocated at 2x device pixels, so QPixmap::size() is 2x the
    // logical rect. Comparing it against the *logical* size was therefore always
    // unequal — the "cache" was rebuilt (allocate + full repaint into a pixmap +
    // blit) on EVERY paint event, for every node, making it strictly slower than
    // painting directly. Compare against the same 2x size we allocate.
    if (cache_dirty_ || cache_.size() != rect.size().toSize() * 2) {
        cache_ = QPixmap(rect.size().toSize() * 2); // 2x for retina
        cache_.setDevicePixelRatio(2.0);
        cache_.fill(Qt::transparent);

        QPainter cp(&cache_);
        cp.setRenderHint(QPainter::Antialiasing, false);

        const auto& t = ui::ThemeManager::instance().tokens();

        // ── Node body ──────────────────────────────────────────────
        cp.setPen(Qt::NoPen);
        cp.setBrush(QColor(t.bg_surface));
        cp.drawRect(rect);

        // ── Header ─────────────────────────────────────────────────
        QRectF header(0, 0, kWidth, kHeaderHeight);
        cp.setBrush(QColor(t.bg_raised));
        cp.drawRect(header);

        // Accent bar (left edge of header). Node-type colours are chosen for the
        // dark canvas; legible_color() keeps them readable on the light one.
        const QColor accent = type_def_.accent_color.isEmpty()
                                  ? QColor(t.accent)
                                  : ui::colors::legible_color(QColor(type_def_.accent_color));
        cp.setPen(Qt::NoPen);
        cp.setBrush(accent);
        cp.drawRect(QRectF(0, 0, 3, kHeaderHeight));

        // Icon text
        cp.setFont(node_font(-3, 13, QFont::Bold));
        cp.setPen(accent);
        cp.drawText(QRectF(8, 0, 24, kHeaderHeight), Qt::AlignVCenter, type_def_.icon_text);

        // Node name
        const QFont name_font = node_font(-2, static_cast<int>(kHeaderHeight) - 10, QFont::Bold);
        cp.setFont(name_font);
        cp.setPen(QColor(t.text_primary));
        QRectF name_rect(34, 0, kWidth - 44, kHeaderHeight);
        QString elided = QFontMetrics(name_font).elidedText(def_.name, Qt::ElideRight, (int)name_rect.width());
        cp.drawText(name_rect, Qt::AlignVCenter, elided);

        // ── Port labels ────────────────────────────────────────────
        const QFont port_font = node_font(-3, static_cast<int>(kPortSpacing) - 8);
        const QFontMetrics port_fm(port_font);
        const qreal label_w = kWidth / 2 - 20;
        cp.setFont(port_font);
        cp.setPen(QColor(t.text_secondary));

        for (int i = 0; i < type_def_.inputs.size(); ++i) {
            qreal y = kHeaderHeight + kPortPadding + i * kPortSpacing;
            cp.drawText(QRectF(14, y - kPortSpacing / 2, label_w, kPortSpacing), Qt::AlignVCenter | Qt::AlignLeft,
                        port_fm.elidedText(type_def_.inputs[i].label, Qt::ElideRight, (int)label_w));
        }

        for (int i = 0; i < type_def_.outputs.size(); ++i) {
            qreal y = kHeaderHeight + kPortPadding + i * kPortSpacing;
            cp.drawText(QRectF(kWidth / 2 + 6, y - kPortSpacing / 2, label_w, kPortSpacing),
                        Qt::AlignVCenter | Qt::AlignRight,
                        port_fm.elidedText(type_def_.outputs[i].label, Qt::ElideRight, (int)label_w));
        }

        // ── Execution state indicator ──────────────────────────────
        if (execution_state_ != "idle") {
            QColor state_color;
            if (execution_state_ == "running")
                state_color = QColor(t.accent);
            else if (execution_state_ == "completed")
                state_color = QColor(t.success);
            else if (execution_state_ == "error" || execution_state_ == "invalid")
                state_color = QColor(t.danger);
            else
                state_color = QColor(t.text_tertiary); // skipped

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
    const auto& t2 = ui::ThemeManager::instance().tokens();
    QColor border_color;
    if (def_.disabled)
        border_color = QColor(t2.border_med);
    else if (isSelected())
        border_color = QColor(t2.accent);
    else if (execution_state_ == "error" || execution_state_ == "invalid")
        border_color = QColor(t2.danger);
    else if (hovered_)
        border_color = QColor(t2.border_bright);
    else
        border_color = QColor(t2.border_dim);

    painter->setPen(QPen(border_color, 1.0));
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(rect.adjusted(0.5, 0.5, -0.5, -0.5));

    // Disabled overlay: the node fades into the canvas background.
    if (def_.disabled) {
        QColor veil(t2.bg_base);
        veil.setAlpha(150);
        painter->setPen(Qt::NoPen);
        painter->setBrush(veil);
        painter->drawRect(rect);
    }
}

QVariant NodeItem::itemChange(GraphicsItemChange change, const QVariant& value) {
    if (change == ItemPositionHasChanged) {
        QPointF pos = value.toPointF();
        def_.x = pos.x();
        def_.y = pos.y();
        emit node_moved(def_.id, pos.x(), pos.y());

        // Edges adjust in NodeScene via signal — no per-edge action needed here
    }

    if (change == ItemSelectedHasChanged) {
        if (value.toBool())
            emit node_selected(def_.id);
    }

    return QGraphicsObject::itemChange(change, value);
}

void NodeItem::keyPressEvent(QKeyEvent* event) {
    // Delete / Backspace are deliberately NOT handled here. A focused node used to
    // swallow the key and delete only itself - even when it was not part of the
    // current selection - so a multi-selection could never be deleted. Ignoring it
    // lets the event reach NodeEditorScreen, which deletes the whole selection.
    QGraphicsObject::keyPressEvent(event);
}

void NodeItem::mousePressEvent(QGraphicsSceneMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        dragging_ = true;
        emit move_started();
    }
    QGraphicsObject::mousePressEvent(event);
}

void NodeItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event) {
    QGraphicsObject::mouseReleaseEvent(event);
    if (dragging_ && event->button() == Qt::LeftButton) {
        dragging_ = false;
        emit move_finished();
    }
}

void NodeItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) {
    // Reserved for future inline editing
    QGraphicsObject::mouseDoubleClickEvent(event);
}

void NodeItem::contextMenuEvent(QGraphicsSceneContextMenuEvent* event) {
    const auto& tm = ui::ThemeManager::instance().tokens();
    QMenu menu;
    menu.setStyleSheet(QString("QMenu { background: %1; color: %2; border: 1px solid %3;"
                               "  font-size: " + fincept::ui::fonts::px(-2) + "px; }"
                               "QMenu::item { padding: 4px 20px; }"
                               "QMenu::item:selected { background: %4; color: %5; }"
                               "QMenu::separator { background: %3; height: 1px; margin: 2px 6px; }")
                           .arg(tm.bg_raised, tm.text_primary, tm.border_med, tm.accent, tm.text_on_accent));

    auto* duplicate_action = menu.addAction(tr("Duplicate"));
    auto* disable_action = menu.addAction(def_.disabled ? tr("Enable") : tr("Disable"));
    menu.addSeparator();
    auto* exec_from_action = menu.addAction(tr("Execute From Here"));
    exec_from_action->setProperty("fincept_agent_risk", "order");
    menu.addSeparator();
    auto* delete_action = menu.addAction(tr("Delete"));
    delete_action->setShortcut(QKeySequence::Delete);

    auto* chosen = menu.exec(event->screenPos());
    if (!chosen)
        return;

    if (chosen == duplicate_action) {
        emit duplicate_requested(def_.id);
    } else if (chosen == disable_action) {
        set_disabled(!def_.disabled);
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
