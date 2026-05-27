#include "ui/charts/IndicatorPicker.h"

#include "ui/charts/ChartOverlayManager.h"
#include "ui/charts/layers/BollingerLayer.h"
#include "ui/charts/layers/EmaLayer.h"
#include "ui/charts/layers/VwapLayer.h"

#include <QHBoxLayout>
#include <QMenu>
#include <QPushButton>

namespace fincept::ui {

IndicatorPicker::IndicatorPicker(ChartOverlayManager* mgr, QWidget* parent)
    : QWidget(parent), mgr_(mgr) {
    setObjectName("indicatorPicker");
    setFixedHeight(28);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 0, 8, 0);
    layout->setSpacing(4);

    add_btn_ = new QPushButton("+ Indicator");
    add_btn_->setObjectName("indicatorAddBtn");
    add_btn_->setFixedHeight(22);
    add_btn_->setCursor(Qt::PointingHandCursor);
    connect(add_btn_, &QPushButton::clicked, this, &IndicatorPicker::build_menu);
    layout->addWidget(add_btn_);

    chip_container_ = new QWidget(this);
    chip_container_->setObjectName("indicatorChipContainer");
    chip_layout_ = new QHBoxLayout(chip_container_);
    chip_layout_->setContentsMargins(0, 0, 0, 0);
    chip_layout_->setSpacing(4);
    layout->addWidget(chip_container_);

    layout->addStretch();

    connect(mgr_, &ChartOverlayManager::layer_added, this, &IndicatorPicker::refresh_chips);
    connect(mgr_, &ChartOverlayManager::layer_removed, this, &IndicatorPicker::refresh_chips);
}

void IndicatorPicker::build_menu() {
    auto* menu = new QMenu(this);
    menu->setObjectName("indicatorMenu");

    const auto defs = available_indicators();
    QString last_cat;
    for (const auto& def : defs) {
        if (def.category != last_cat) {
            if (!last_cat.isEmpty())
                menu->addSeparator();
            menu->addSection(def.category);
            last_cat = def.category;
        }

        auto* action = menu->addAction(def.name);
        action->setEnabled(!mgr_->has_layer(def.id));
        connect(action, &QAction::triggered, this, [this, id = def.id]() {
            emit indicator_requested(id);
        });
    }
    menu->popup(add_btn_->mapToGlobal(QPoint(0, add_btn_->height())));
}

void IndicatorPicker::add_chip(const QString& id, const QString& name) {
    auto* chip = new QPushButton(name + QStringLiteral("  ×"), chip_container_);
    chip->setObjectName("indicatorChip");
    chip->setFixedHeight(20);
    chip->setCursor(Qt::PointingHandCursor);
    chip->setProperty("layerId", id);
    connect(chip, &QPushButton::clicked, this, [this, id]() {
        emit indicator_removed(id);
    });
    chip_layout_->addWidget(chip);
}

void IndicatorPicker::remove_chip(const QString& id) {
    for (int i = chip_layout_->count() - 1; i >= 0; --i) {
        auto* item = chip_layout_->itemAt(i);
        if (auto* w = item->widget()) {
            if (w->property("layerId").toString() == id) {
                chip_layout_->removeWidget(w);
                w->deleteLater();
                return;
            }
        }
    }
}

void IndicatorPicker::refresh_chips() {
    while (chip_layout_->count() > 0) {
        auto* item = chip_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
    for (auto* layer : mgr_->layers())
        add_chip(layer->id(), layer->display_name());
}

QVector<IndicatorDef> IndicatorPicker::available_indicators() {
    return {
        {"ema_9",     "EMA (9)",         "Trend"},
        {"ema_21",    "EMA (21)",        "Trend"},
        {"ema_50",    "EMA (50)",        "Trend"},
        {"ema_200",   "EMA (200)",       "Trend"},
        {"vwap",      "VWAP",            "Volume"},
        {"bb_20_2.0", "Bollinger (20,2)","Volatility"},
        {"sr_auto",   "Auto S/R",        "Levels"},
        {"pivot_std", "Pivot Points",    "Levels"},
    };
}

} // namespace fincept::ui
