#include "screens/polymarket/PolymarketPriceChart.h"

#include "ui/charts/ChartFactory.h"
#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QVBoxLayout>

namespace fincept::screens::polymarket {

using namespace fincept::ui;
using namespace fincept::services::prediction;

static const QStringList INTERVAL_LABELS = {"1H", "6H", "1D", "1W", "1M", "ALL"};
static const QStringList INTERVAL_VALUES = {"1h", "6h", "1d", "1w", "1m", "max"};

PolymarketPriceChart::PolymarketPriceChart(QWidget* parent) : QWidget(parent) {
    setStyleSheet(QString("background: %1;").arg(colors::BG_BASE()));
    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // ── Toolbar ───────────────────────────────────────────────────────────
    auto* toolbar_widget = new QWidget(this);
    toolbar_widget->setFixedHeight(36);
    toolbar_widget->setStyleSheet(
        QString("background: %1; border-bottom: 1px solid %2;")
            .arg(colors::BG_RAISED(), colors::BORDER_DIM()));
    auto* toolbar = new QHBoxLayout(toolbar_widget);
    toolbar->setContentsMargins(12, 0, 12, 0);
    toolbar->setSpacing(8);

    const QString combo_css =
        QString("QComboBox { background: %1; color: %2; border: 1px solid %3; "
                "  padding: 2px 6px; font-size: 9px; font-weight: 600; }"
                "QComboBox::drop-down { border: none; width: 12px; }"
                "QComboBox QAbstractItemView { background: %1; color: %2; border: 1px solid %3; }")
            .arg(colors::BG_BASE(), colors::TEXT_PRIMARY(), colors::BORDER_MED());

    auto* lbl = new QLabel("INTERVAL");
    lbl->setStyleSheet(
        QString("color: %1; font-size: 8px; font-weight: 700; letter-spacing: 0.5px; "
                "background: transparent;")
            .arg(colors::TEXT_SECONDARY()));
    toolbar->addWidget(lbl);

    interval_combo_ = new QComboBox;
    interval_combo_->addItems(INTERVAL_LABELS);
    interval_combo_->setCurrentIndex(2);
    interval_combo_->setFixedSize(62, 24);
    interval_combo_->setStyleSheet(combo_css);
    connect(interval_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (idx >= 0 && idx < INTERVAL_VALUES.size())
            emit interval_changed(INTERVAL_VALUES[idx]);
    });
    toolbar->addWidget(interval_combo_);

    toolbar->addSpacing(10);

    auto* olbl = new QLabel("OUTCOME");
    olbl->setStyleSheet(lbl->styleSheet());
    toolbar->addWidget(olbl);

    outcome_combo_ = new QComboBox;
    outcome_combo_->setFixedSize(100, 24);
    outcome_combo_->setStyleSheet(combo_css);
    connect(outcome_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &PolymarketPriceChart::outcome_changed);
    toolbar->addWidget(outcome_combo_);

    toolbar->addStretch(1);

    price_label_ = new QLabel;
    price_label_->setStyleSheet(
        QString("color: %1; font-size: 13px; font-weight: 700; background: transparent;")
            .arg(colors::AMBER()));
    toolbar->addWidget(price_label_);

    vl->addWidget(toolbar_widget);

    chart_container_ = new QWidget(this);
    chart_container_->setStyleSheet(QString("background: %1;").arg(colors::BG_BASE()));
    auto* ccl = new QVBoxLayout(chart_container_);
    ccl->setContentsMargins(8, 8, 8, 8);
    auto* empty = new QLabel("Select a market to view its price chart");
    empty->setStyleSheet(
        QString("color: %1; font-size: 12px; background: transparent;").arg(colors::TEXT_DIM()));
    empty->setAlignment(Qt::AlignCenter);
    ccl->addWidget(empty);
    vl->addWidget(chart_container_, 1);
}

void PolymarketPriceChart::set_price_history(const PriceHistory& history) {
    last_history_ = history;
    has_last_history_ = true;

    auto* layout = chart_container_->layout();
    while (layout->count() > 0) {
        auto* item = layout->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    if (history.points.isEmpty()) {
        auto* empty = new QLabel("No price history available");
        empty->setStyleSheet(QString("color: %1; font-size: 13px; background: transparent;").arg(colors::TEXT_DIM()));
        empty->setAlignment(Qt::AlignCenter);
        layout->addWidget(empty);
        return;
    }

    QVector<ChartFactory::DataPoint> points;
    points.reserve(history.points.size());
    for (const auto& pt : history.points) {
        // prediction::PricePoint is always in ts_ms + 0..1 probability space
        // (Kalshi cents are divided by 100 in the adapter type map). Y-axis
        // is rendered as 0–100 cents regardless so the chart scale is
        // consistent; the axis *label* comes from the presentation.
        points.append({static_cast<double>(pt.ts_ms), pt.price * 100.0});
    }

    // Accent-colored series ties the chart back to the active exchange.
    auto* chart_view = ChartFactory::line_chart(presentation_.chart_y_label, points,
                                                presentation_.accent.name());
    layout->addWidget(chart_view);
}

void PolymarketPriceChart::set_current_price(double price) {
    price_label_->setText(presentation_.format_price(price));
}

void PolymarketPriceChart::set_presentation(const ExchangePresentation& p) {
    presentation_ = p;
    // Price readout tracks the exchange accent so the header colour doesn't
    // stay amber after a swap to Kalshi (teal) / vice versa.
    if (price_label_) {
        price_label_->setStyleSheet(
            QString("color: %1; font-size: 13px; font-weight: 700; background: transparent;")
                .arg(presentation_.accent.name()));
    }
    if (has_last_history_) set_price_history(last_history_);
}

void PolymarketPriceChart::set_outcome_labels(const QStringList& labels) {
    const QSignalBlocker block(outcome_combo_);
    outcome_combo_->clear();
    outcome_combo_->addItems(labels);
    if (!labels.isEmpty())
        outcome_combo_->setCurrentIndex(0);
}

} // namespace fincept::screens::polymarket
