// src/ui/widgets/algo/ConditionBlock.cpp
#include "ui/widgets/algo/ConditionBlock.h"

#include "services/algo_trading/AlgoTradingTypes.h"

#include <QHBoxLayout>
#include <QVBoxLayout>

using fincept::services::algo::algo_operators;

namespace fincept::ui::algo {

ConditionBlock::ConditionBlock(bool is_entry, QWidget* parent)
    : QFrame(parent), is_entry_(is_entry) {
    setObjectName(is_entry ? QStringLiteral("conditionBlockEntry") : QStringLiteral("conditionBlockExit"));
    setFrameShape(QFrame::StyledPanel);
    build_ui();
}

void ConditionBlock::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QFrame::changeEvent(event);
}

void ConditionBlock::retranslateUi() {
    if (remove_btn_) remove_btn_->setToolTip(tr("Remove condition"));
    if (between_and_) between_and_->setText(tr("and"));
    // Re-apply the translatable operator labels in place (preserving each item's
    // stored operator data and the current selection). The first five entries are
    // raw comparison symbols (data, not translated); only the named operators do.
    if (operator_combo_) {
        const QStringList labels = {QStringLiteral(">"),  QStringLiteral("<"),
                                    QStringLiteral(">="), QStringLiteral("<="),
                                    QStringLiteral("=="), tr("Crosses Above"),
                                    tr("Crosses Below"),  tr("Rising"),
                                    tr("Falling")};
        for (int i = 0; i < operator_combo_->count() && i < labels.size(); ++i)
            operator_combo_->setItemText(i, labels[i]);
    }
}

void ConditionBlock::build_ui() {
    auto* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(8, 6, 8, 6);
    main_layout->setSpacing(4);

    // ── Line 1: left operand ────────────────────────────────────────────────
    auto* line1 = new QHBoxLayout();
    line1->setSpacing(6);
    lhs_ = new OperandEditor(/*allow_value=*/false, this);
    line1->addWidget(lhs_, 1);

    remove_btn_ = new QPushButton(QStringLiteral("✕"), this);
    remove_btn_->setObjectName(QStringLiteral("condBlockRemove"));
    remove_btn_->setFixedSize(24, 24);
    remove_btn_->setToolTip(tr("Remove condition"));
    // Icon/close button: never take keyboard focus. Otherwise clicking it focuses
    // the button, and when this block is deleted Qt hands focus to the next widget —
    // the neighbouring row's editable indicator combo — which then shows a text
    // caret, making "close" look like it opened a text field instead of removing.
    remove_btn_->setFocusPolicy(Qt::NoFocus);
    line1->addWidget(remove_btn_);
    main_layout->addLayout(line1);

    // ── Line 2: operator + right operand ────────────────────────────────────
    auto* line2 = new QHBoxLayout();
    line2->setSpacing(6);
    line2->setContentsMargins(16, 0, 0, 0); // indent to read as "…compared to…"

    operator_combo_ = new QComboBox(this);
    operator_combo_->setObjectName(QStringLiteral("condBlockOperator"));
    operator_combo_->setMinimumWidth(120);
    populate_operators();
    line2->addWidget(operator_combo_);

    rhs_ = new OperandEditor(/*allow_value=*/true, this);
    rhs_->set_value_mode(true); // default to a constant on the right
    line2->addWidget(rhs_, 1);

    // `between` bounds (hidden unless the operator is "between").
    between_low_ = new QDoubleSpinBox(this);
    between_low_->setObjectName(QStringLiteral("condBlockBetweenLow"));
    between_low_->setRange(-1e9, 1e9);
    between_low_->setDecimals(4);
    between_and_ = new QLabel(tr("and"), this);
    between_high_ = new QDoubleSpinBox(this);
    between_high_->setObjectName(QStringLiteral("condBlockBetweenHigh"));
    between_high_->setRange(-1e9, 1e9);
    between_high_->setDecimals(4);
    between_high_->setValue(100);
    line2->addWidget(between_low_);
    line2->addWidget(between_and_);
    line2->addWidget(between_high_);
    main_layout->addLayout(line2);

    connect(between_low_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this](double) { emit changed(); });
    connect(between_high_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this](double) { emit changed(); });
    connect(lhs_, &OperandEditor::changed, this, &ConditionBlock::changed);
    connect(rhs_, &OperandEditor::changed, this, &ConditionBlock::changed);
    connect(operator_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) { on_operator_changed(); });
    connect(remove_btn_, &QPushButton::clicked, this, &ConditionBlock::remove_requested);

    on_operator_changed();
}

void ConditionBlock::populate_operators() {
    const auto ops = algo_operators();
    const QStringList labels = {">", "<", ">=", "<=", "==",
                                tr("Crosses Above"), tr("Crosses Below"),
                                tr("Rising"), tr("Falling")};
    for (int i = 0; i < ops.size() && i < labels.size(); ++i)
        operator_combo_->addItem(labels[i], ops[i]);
}

void ConditionBlock::on_operator_changed() {
    const QString op = operator_combo_->currentData().toString();
    const bool unary = (op == "rising" || op == "falling");
    const bool is_between = (op == "between");
    // Rising/Falling describe the LHS series alone; Between uses two constants.
    rhs_->setVisible(!unary && !is_between);
    between_low_->setVisible(is_between);
    between_and_->setVisible(is_between);
    between_high_->setVisible(is_between);
    emit changed();
}

QJsonObject ConditionBlock::to_json() const {
    QJsonObject obj;
    obj["indicator"] = lhs_->indicator_id();
    obj["params"] = lhs_->params();
    obj["field"] = lhs_->field();
    obj["offset"] = lhs_->offset();

    const QString op = operator_combo_->currentData().toString();
    obj["operator"] = op;

    const bool unary = (op == "rising" || op == "falling");
    if (op == "between") {
        obj["compare_mode"] = QStringLiteral("value");
        obj["value"] = between_low_->value();
        obj["value2"] = between_high_->value();
    } else if (unary) {
        obj["compare_mode"] = QStringLiteral("value");
        obj["value"] = 0;
    } else if (rhs_->is_value_mode()) {
        obj["compare_mode"] = QStringLiteral("value");
        obj["value"] = rhs_->value();
    } else {
        obj["compare_mode"] = QStringLiteral("indicator");
        obj["compare_indicator"] = rhs_->indicator_id();
        obj["compare_params"] = rhs_->params();
        obj["compare_field"] = rhs_->field();
        obj["compare_offset"] = rhs_->offset();
    }
    return obj;
}

void ConditionBlock::from_json(const QJsonObject& obj) {
    lhs_->set_indicator(obj.value("indicator").toString());
    lhs_->set_params(obj.value("params").toObject());
    lhs_->set_field(obj.value("field").toString("value"));
    lhs_->set_offset(obj.value("offset").toInt(0));

    const QString op = obj.value("operator").toString(">");
    for (int i = 0; i < operator_combo_->count(); ++i) {
        if (operator_combo_->itemData(i).toString() == op) {
            operator_combo_->setCurrentIndex(i);
            break;
        }
    }

    if (op == "between") {
        between_low_->setValue(obj.value("value").toDouble(0));
        between_high_->setValue(obj.value("value2").toDouble(0));
    } else {
        const QString cmp = obj.value("compare_mode").toString("value");
        if (cmp == "indicator") {
            rhs_->set_value_mode(false);
            rhs_->set_indicator(obj.value("compare_indicator").toString());
            rhs_->set_params(obj.value("compare_params").toObject());
            rhs_->set_field(obj.value("compare_field").toString("value"));
            rhs_->set_offset(obj.value("compare_offset").toInt(0));
        } else {
            rhs_->set_value_mode(true);
            rhs_->set_value(obj.value("value").toDouble(0));
        }
    }
    on_operator_changed();
}

} // namespace fincept::ui::algo
