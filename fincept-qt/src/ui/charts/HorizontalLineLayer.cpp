#include "ui/charts/HorizontalLineLayer.h"

#include <QChart>
#include <QGraphicsLineItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QPen>

namespace fincept::ui {

HorizontalLineLayer::HorizontalLineLayer(const QString& id, const QString& name, QObject* parent)
    : OverlayLayer(parent), id_(id), name_(name) {}

void HorizontalLineLayer::compute(const QVector<CandleData>&) {
    // Levels are set externally via set_levels() — nothing to compute.
}

void HorizontalLineLayer::set_levels(const QVector<HorizontalLevel>& levels) {
    levels_ = levels;
    if (scene_ && chart_) {
        clear_items();
        rebuild_items();
        reposition(chart_);
    }
}

void HorizontalLineLayer::attach(QGraphicsScene* scene, QChart* chart) {
    scene_ = scene;
    chart_ = chart;
    rebuild_items();
    reposition(chart);
}

void HorizontalLineLayer::detach(QGraphicsScene*, QChart*) {
    clear_items();
    scene_ = nullptr;
    chart_ = nullptr;
}

void HorizontalLineLayer::reposition(QChart* chart) {
    if (!chart || items_.isEmpty())
        return;

    const QRectF plot = chart->plotArea();
    constexpr qreal pad_x = 4, pad_y = 1;

    for (int i = 0; i < items_.size() && i < levels_.size(); ++i) {
        const auto& level = levels_[i];
        const QPointF pos = chart->mapToPosition(QPointF(0, level.price));
        const qreal y = pos.y();

        const bool in_view = y >= plot.top() && y <= plot.bottom();

        items_[i].line->setLine(plot.left(), y, plot.right(), y);
        items_[i].line->setVisible(in_view && visible());

        if (items_[i].tag_txt && items_[i].tag_bg) {
            const QRectF tb = items_[i].tag_txt->boundingRect();
            const qreal tag_w = tb.width() + 2 * pad_x;
            const qreal tag_h = tb.height() + 2 * pad_y;
            const qreal tag_x = plot.right() + 2;
            const qreal tag_y = y - tag_h / 2.0;

            items_[i].tag_bg->setRect(QRectF(tag_x, tag_y, tag_w, tag_h));
            items_[i].tag_txt->setPos(tag_x + pad_x, tag_y + pad_y);
            items_[i].tag_bg->setVisible(in_view && visible());
            items_[i].tag_txt->setVisible(in_view && visible());
        }
    }
}

void HorizontalLineLayer::rebuild_items() {
    if (!scene_)
        return;

    QFont tag_font;
    tag_font.setFamily(QStringLiteral("Consolas"));
    tag_font.setPixelSize(10);
    tag_font.setWeight(QFont::DemiBold);

    for (const auto& level : levels_) {
        LineItem item;
        QPen pen(level.color);
        pen.setStyle(level.style);
        pen.setWidth(1);
        item.line = scene_->addLine(QLineF(), pen);
        item.line->setZValue(5);

        if (!level.label.isEmpty()) {
            QColor bg_color = level.color;
            bg_color.setAlpha(200);
            item.tag_bg = scene_->addRect(QRectF(), QPen(Qt::NoPen), QBrush(bg_color));
            item.tag_bg->setZValue(21);

            item.tag_txt = scene_->addSimpleText(level.label);
            item.tag_txt->setBrush(QBrush(QColor("#080808")));
            item.tag_txt->setFont(tag_font);
            item.tag_txt->setZValue(22);
        }
        items_.append(item);
    }
}

void HorizontalLineLayer::clear_items() {
    for (auto& item : items_) {
        if (item.line && scene_) {
            scene_->removeItem(item.line);
            delete item.line;
        }
        if (item.tag_bg && scene_) {
            scene_->removeItem(item.tag_bg);
            delete item.tag_bg;
        }
        if (item.tag_txt && scene_) {
            scene_->removeItem(item.tag_txt);
            delete item.tag_txt;
        }
    }
    items_.clear();
}

} // namespace fincept::ui
