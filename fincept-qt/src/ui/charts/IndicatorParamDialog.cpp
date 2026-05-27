#include "ui/charts/IndicatorParamDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>

namespace fincept::ui {

IndicatorParamDialog::IndicatorParamDialog(const QString& indicator_id, QWidget* parent)
    : QDialog(parent), id_(indicator_id) {
    setObjectName("indicatorParamDialog");
    setWindowTitle("Indicator Parameters");
    setFixedWidth(280);

    auto* layout = new QVBoxLayout(this);

    if (id_.startsWith("ema"))
        setup_ema_ui();
    else if (id_.startsWith("bb"))
        setup_bollinger_ui();
    else if (id_.startsWith("pivot"))
        setup_pivot_ui();

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

void IndicatorParamDialog::setup_ema_ui() {
    auto* form = new QFormLayout();
    period_spin_ = new QSpinBox(this);
    period_spin_->setRange(2, 500);
    period_spin_->setValue(21);
    form->addRow("Period:", period_spin_);
    qobject_cast<QVBoxLayout*>(layout())->insertLayout(0, form);
}

void IndicatorParamDialog::setup_bollinger_ui() {
    auto* form = new QFormLayout();
    period_spin_ = new QSpinBox(this);
    period_spin_->setRange(2, 200);
    period_spin_->setValue(20);
    form->addRow("Period:", period_spin_);

    std_spin_ = new QDoubleSpinBox(this);
    std_spin_->setRange(0.5, 5.0);
    std_spin_->setValue(2.0);
    std_spin_->setSingleStep(0.5);
    form->addRow("Std Dev:", std_spin_);
    qobject_cast<QVBoxLayout*>(layout())->insertLayout(0, form);
}

void IndicatorParamDialog::setup_pivot_ui() {
    auto* form = new QFormLayout();
    pivot_combo_ = new QComboBox(this);
    pivot_combo_->addItems({"Standard", "Camarilla", "Woodie", "Fibonacci"});
    form->addRow("Type:", pivot_combo_);
    qobject_cast<QVBoxLayout*>(layout())->insertLayout(0, form);
}

int IndicatorParamDialog::period() const {
    return period_spin_ ? period_spin_->value() : 21;
}

double IndicatorParamDialog::std_dev() const {
    return std_spin_ ? std_spin_->value() : 2.0;
}

QString IndicatorParamDialog::pivot_type() const {
    return pivot_combo_ ? pivot_combo_->currentText() : "Standard";
}

} // namespace fincept::ui
