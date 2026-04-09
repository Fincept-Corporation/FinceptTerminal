#include "screens/polymarket/PolymarketPriceChart.h"

#include "ui/charts/ChartFactory.h"
#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QVBoxLayout>

namespace fincept::screens::polymarket {

using namespace fincept::ui;
using namespace fincept::services::polymarket;

static const QStringList INTERVAL_LABELS = {"1H", "6H", "1D", "1W", "1M", "ALL"};
static const QStringList INTERVAL_VALUES = {"1h", "6h", "1d", "1w", "1m", "max"};

PolymarketPriceChart::PolymarketPriceChart(QWidget* parent) : QWidget(parent) {
    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(8, 8, 8, 8);
    vl->setSpacing(4);

    auto* toolbar = new QHBoxLayout;
    toolbar->setSpacing(6);

    auto* lbl = new QLabel("INTERVAL");
    lbl->setStyleSheet(
        QString("color: %1; font-size: 9px; font-weight: 700; background: transparent;").arg(colors::TEXT_SECONDARY));
    toolbar->addWidget(lbl);

    interval_combo_ = new QComboBox;
    interval_combo_->addItems(INTERVAL_LABELS);
    interval_combo_->setCurrentIndex(2); // 1D default
    interval_combo_->setFixedWidth(70);
    connect(interval_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (idx >= 0 && idx < INTERVAL_VALUES.size())
            emit interval_changed(INTERVAL_VALUES[idx]);
    });
    toolbar->addWidget(interval_combo_);

    toolbar->addSpacing(12);

    auto* olbl = new QLabel("OUTCOME");
    olbl->setStyleSheet(lbl->styleSheet());
    toolbar->addWidget(olbl);

    outcome_combo_ = new QComboBox;
    outcome_combo_->addItems({"YES", "NO"});
    outcome_combo_->setFixedWidth(70);
    connect(outcome_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &PolymarketPriceChart::outcome_changed);
    toolbar->addWidget(outcome_combo_);

    toolbar->addStretch(1);

    price_label_ = new QLabel;
    price_label_->setStyleSheet(
        QString("color: %1; font-size: 13px; font-weight: 700; background: transparent;").arg(colors::AMBER));
    toolbar->addWidget(price_label_);

    vl->addLayout(toolbar);

    chart_container_ = new QWidget(this);
    auto* ccl = new QVBoxLayout(chart_container_);
    ccl->setContentsMargins(0, 0, 0, 0);
    auto* empty = new QLabel("Select a market to view chart");
    empty->setStyleSheet(QString("color: %1; font-size: 13px; background: transparent;").arg(colors::TEXT_DIM));
    empty->setAlignment(Qt::AlignCenter);
    ccl->addWidget(empty);
    vl->addWidget(chart_container_, 1);
}

void PolymarketPriceChart::set_price_history(const PriceHistory& history) {
    auto* layout = chart_container_->layout();
    while (layout->count() > 0) {
        auto* item = layout->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    if (history.points.isEmpty()) {
        auto* empty = new QLabel("No price history available");
        empty->setStyleSheet(QString("color: %1; font-size: 13px; background: transparent;").arg(colors::TEXT_DIM));
        empty->setAlignment(Qt::AlignCenter);
        layout->addWidget(empty);
        return;
    }

    QVector<ChartFactory::DataPoint> data;
    data.reserve(history.points.size());
    for (const auto& pt : history.points) {
        data.append({static_cast<double>(pt.timestamp), pt.price * 100.0}); // scale to cents
    }

    auto* chart_view = ChartFactory::line_chart("PROBABILITY", data, colors::AMBER);
    layout->addWidget(chart_view);
}

void PolymarketPriceChart::set_current_price(double price) {
    price_label_->setText(QString("%1c").arg(qRound(price * 100)));
}

void PolymarketPriceChart::set_token_labels(const QStringList& labels) {
    outcome_combo_->clear();
    outcome_combo_->addItems(labels);
}

} // namespace fincept::screens::polymarket
