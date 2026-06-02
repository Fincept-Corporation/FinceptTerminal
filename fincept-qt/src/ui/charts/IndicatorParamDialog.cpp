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
    setWindowTitle(tr("Indicator Parameters"));
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

void IndicatorParamDialog::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QDialog::changeEvent(event);
}

void IndicatorParamDialog::retranslateUi() {
    setWindowTitle(tr("Indicator Parameters"));
    if (period_label_) period_label_->setText(tr("Period:"));
    if (std_label_)    std_label_->setText(tr("Std Dev:"));
    if (pivot_label_)  pivot_label_->setText(tr("Type:"));
    if (pivot_combo_ && pivot_combo_->count() == 4) {
        // Preserve the current selection across the relabel.
        const int idx = pivot_combo_->currentIndex();
        pivot_combo_->setItemText(0, tr("Standard"));
        pivot_combo_->setItemText(1, tr("Camarilla"));
        pivot_combo_->setItemText(2, tr("Woodie"));
        pivot_combo_->setItemText(3, tr("Fibonacci"));
        pivot_combo_->setCurrentIndex(idx);
    }
}

void IndicatorParamDialog::setup_ema_ui() {
    auto* form = new QFormLayout();
    period_spin_ = new QSpinBox(this);
    period_spin_->setRange(2, 500);
    period_spin_->setValue(21);
    period_label_ = new QLabel(tr("Period:"), this);
    form->addRow(period_label_, period_spin_);
    qobject_cast<QVBoxLayout*>(layout())->insertLayout(0, form);
}

void IndicatorParamDialog::setup_bollinger_ui() {
    auto* form = new QFormLayout();
    period_spin_ = new QSpinBox(this);
    period_spin_->setRange(2, 200);
    period_spin_->setValue(20);
    period_label_ = new QLabel(tr("Period:"), this);
    form->addRow(period_label_, period_spin_);

    std_spin_ = new QDoubleSpinBox(this);
    std_spin_->setRange(0.5, 5.0);
    std_spin_->setValue(2.0);
    std_spin_->setSingleStep(0.5);
    std_label_ = new QLabel(tr("Std Dev:"), this);
    form->addRow(std_label_, std_spin_);
    qobject_cast<QVBoxLayout*>(layout())->insertLayout(0, form);
}

void IndicatorParamDialog::setup_pivot_ui() {
    auto* form = new QFormLayout();
    pivot_combo_ = new QComboBox(this);
    // Pivot calculation methods — fixed UI labels (not data values).
    pivot_combo_->addItems({tr("Standard"), tr("Camarilla"), tr("Woodie"), tr("Fibonacci")});
    pivot_label_ = new QLabel(tr("Type:"), this);
    form->addRow(pivot_label_, pivot_combo_);
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
