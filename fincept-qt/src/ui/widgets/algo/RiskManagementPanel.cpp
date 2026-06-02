// src/ui/widgets/algo/RiskManagementPanel.cpp
#include "ui/widgets/algo/RiskManagementPanel.h"

#include <QGridLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace fincept::ui::algo {

RiskManagementPanel::RiskManagementPanel(QWidget* parent)
    : QWidget(parent) {
    setObjectName(QStringLiteral("riskManagementPanel"));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    header_ = new QLabel(tr("RISK MANAGEMENT"), this);
    header_->setObjectName(QStringLiteral("riskPanelHeader"));
    layout->addWidget(header_);

    auto* grid = new QGridLayout();
    grid->setSpacing(6);

    stop_loss_ = create_row(tr("Stop Loss %"), 0, 50, 2.0, 1, this);
    take_profit_ = create_row(tr("Take Profit %"), 0, 100, 5.0, 1, this);
    trailing_stop_ = create_row(tr("Trailing Stop %"), 0, 50, 0, 1, this);
    capital_pct_ = create_row(tr("Capital Alloc %"), 1, 100, 100, 0, this);

    grid->addWidget(stop_loss_.label, 0, 0);
    grid->addWidget(stop_loss_.slider, 0, 1);
    grid->addWidget(stop_loss_.spin, 0, 2);
    grid->addWidget(take_profit_.label, 1, 0);
    grid->addWidget(take_profit_.slider, 1, 1);
    grid->addWidget(take_profit_.spin, 1, 2);
    grid->addWidget(trailing_stop_.label, 2, 0);
    grid->addWidget(trailing_stop_.slider, 2, 1);
    grid->addWidget(trailing_stop_.spin, 2, 2);
    grid->addWidget(capital_pct_.label, 3, 0);
    grid->addWidget(capital_pct_.slider, 3, 1);
    grid->addWidget(capital_pct_.spin, 3, 2);

    // Quantity
    quantity_label_ = new QLabel(tr("Quantity"), this);
    quantity_spin_ = new QDoubleSpinBox(this);
    quantity_spin_->setObjectName(QStringLiteral("riskQtySpin"));
    quantity_spin_->setRange(1, 100000);
    quantity_spin_->setDecimals(0);
    quantity_spin_->setValue(1);
    grid->addWidget(quantity_label_, 4, 0);
    grid->addWidget(quantity_spin_, 4, 1, 1, 2);

    // Max order value
    max_order_label_ = new QLabel(tr("Max Order Value"), this);
    max_order_spin_ = new QDoubleSpinBox(this);
    max_order_spin_->setObjectName(QStringLiteral("riskMaxOrderSpin"));
    max_order_spin_->setRange(0, 10000000);
    max_order_spin_->setDecimals(0);
    max_order_spin_->setValue(0);
    max_order_spin_->setSpecialValueText(tr("No Limit"));
    grid->addWidget(max_order_label_, 5, 0);
    grid->addWidget(max_order_spin_, 5, 1, 1, 2);

    layout->addLayout(grid);

    connect(quantity_spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &RiskManagementPanel::values_changed);
    connect(max_order_spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &RiskManagementPanel::values_changed);
}

void RiskManagementPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void RiskManagementPanel::retranslateUi() {
    if (header_) header_->setText(tr("RISK MANAGEMENT"));
    if (stop_loss_.label) stop_loss_.label->setText(tr("Stop Loss %"));
    if (take_profit_.label) take_profit_.label->setText(tr("Take Profit %"));
    if (trailing_stop_.label) trailing_stop_.label->setText(tr("Trailing Stop %"));
    if (capital_pct_.label) capital_pct_.label->setText(tr("Capital Alloc %"));
    if (quantity_label_) quantity_label_->setText(tr("Quantity"));
    if (max_order_label_) max_order_label_->setText(tr("Max Order Value"));
    if (max_order_spin_) max_order_spin_->setSpecialValueText(tr("No Limit"));
}

RiskManagementPanel::SliderRow RiskManagementPanel::create_row(
    const QString& label_text, double min_val, double max_val,
    double default_val, int decimals, QWidget* parent) {

    SliderRow row;
    row.label = new QLabel(label_text, parent);
    row.slider = new QSlider(Qt::Horizontal, parent);
    row.slider->setRange(static_cast<int>(min_val * 10), static_cast<int>(max_val * 10));
    row.slider->setValue(static_cast<int>(default_val * 10));

    row.spin = new QDoubleSpinBox(parent);
    row.spin->setRange(min_val, max_val);
    row.spin->setDecimals(decimals);
    row.spin->setValue(default_val);

    connect(row.slider, &QSlider::valueChanged, parent, [row](int val) {
        row.spin->setValue(val / 10.0);
    });
    connect(row.spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            parent, [row, this](double val) {
                row.slider->setValue(static_cast<int>(val * 10));
                emit values_changed();
            });
    return row;
}

double RiskManagementPanel::stop_loss() const { return stop_loss_.spin->value(); }
double RiskManagementPanel::take_profit() const { return take_profit_.spin->value(); }
double RiskManagementPanel::trailing_stop() const { return trailing_stop_.spin->value(); }
double RiskManagementPanel::quantity() const { return quantity_spin_->value(); }
double RiskManagementPanel::max_order_value() const { return max_order_spin_->value(); }
double RiskManagementPanel::capital_pct() const { return capital_pct_.spin->value(); }

void RiskManagementPanel::set_values(double sl, double tp, double ts, double qty, double mov, double capital_pct) {
    stop_loss_.spin->setValue(sl);
    take_profit_.spin->setValue(tp);
    trailing_stop_.spin->setValue(ts);
    quantity_spin_->setValue(qty);
    max_order_spin_->setValue(mov);
    capital_pct_.spin->setValue(capital_pct);
}

} // namespace fincept::ui::algo
