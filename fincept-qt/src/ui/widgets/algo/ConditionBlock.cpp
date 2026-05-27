// src/ui/widgets/algo/ConditionBlock.cpp
#include "ui/widgets/algo/ConditionBlock.h"
#include "services/algo_trading/AlgoTradingTypes.h"

#include <QHBoxLayout>
#include <QVBoxLayout>

using fincept::services::algo::algo_indicators;
using fincept::services::algo::algo_operators;

namespace fincept::ui::algo {

ConditionBlock::ConditionBlock(bool is_entry, QWidget* parent)
    : QFrame(parent), is_entry_(is_entry) {
    setObjectName(is_entry ? QStringLiteral("conditionBlockEntry") : QStringLiteral("conditionBlockExit"));
    setFrameShape(QFrame::StyledPanel);
    build_ui();
}

void ConditionBlock::build_ui() {
    auto* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(8, 6, 8, 6);
    main_layout->setSpacing(4);

    // Row 1: indicator, operator, value
    auto* row1 = new QHBoxLayout();
    row1->setSpacing(6);

    indicator_combo_ = new QComboBox(this);
    indicator_combo_->setObjectName(QStringLiteral("condBlockIndicator"));
    indicator_combo_->setMinimumWidth(120);
    populate_indicators();

    field_combo_ = new QComboBox(this);
    field_combo_->setObjectName(QStringLiteral("condBlockField"));
    field_combo_->setMinimumWidth(80);

    operator_combo_ = new QComboBox(this);
    operator_combo_->setObjectName(QStringLiteral("condBlockOperator"));
    operator_combo_->setMinimumWidth(100);
    populate_operators();

    compare_mode_combo_ = new QComboBox(this);
    compare_mode_combo_->setObjectName(QStringLiteral("condBlockCompareMode"));
    compare_mode_combo_->addItem(tr("Value"), QStringLiteral("value"));
    compare_mode_combo_->addItem(tr("Indicator"), QStringLiteral("indicator"));

    value_spin_ = new QDoubleSpinBox(this);
    value_spin_->setObjectName(QStringLiteral("condBlockValue"));
    value_spin_->setRange(-999999, 999999);
    value_spin_->setDecimals(4);

    compare_indicator_combo_ = new QComboBox(this);
    compare_indicator_combo_->setObjectName(QStringLiteral("condBlockCompareInd"));
    compare_indicator_combo_->setMinimumWidth(120);
    populate_indicators();
    for (int i = 0; i < compare_indicator_combo_->count(); ++i) {
        if (compare_indicator_combo_->itemData(i).toString().isEmpty())
            compare_indicator_combo_->model()->setData(
                compare_indicator_combo_->model()->index(i, 0), false, Qt::UserRole - 1);
    }
    compare_indicator_combo_->hide();

    compare_field_combo_ = new QComboBox(this);
    compare_field_combo_->setObjectName(QStringLiteral("condBlockCompareField"));
    compare_field_combo_->setMinimumWidth(80);
    compare_field_combo_->hide();

    remove_btn_ = new QPushButton(this);
    remove_btn_->setObjectName(QStringLiteral("condBlockRemove"));
    remove_btn_->setText(QStringLiteral("X"));
    remove_btn_->setFixedSize(24, 24);

    row1->addWidget(indicator_combo_);
    row1->addWidget(field_combo_);
    row1->addWidget(operator_combo_);
    row1->addWidget(compare_mode_combo_);
    row1->addWidget(value_spin_);
    row1->addWidget(compare_indicator_combo_);
    row1->addWidget(compare_field_combo_);
    row1->addStretch();
    row1->addWidget(remove_btn_);
    main_layout->addLayout(row1);

    // Row 2: parameters (hidden by default for stock attributes)
    params_container_ = new QWidget(this);
    params_container_->setObjectName(QStringLiteral("condBlockParams"));
    auto* params_layout = new QHBoxLayout(params_container_);
    params_layout->setContentsMargins(0, 0, 0, 0);
    params_layout->setSpacing(6);

    param1_label_ = new QLabel(this);
    param1_spin_ = new QDoubleSpinBox(this);
    param1_spin_->setRange(1, 500);
    param1_spin_->setDecimals(0);
    param1_spin_->setValue(14);

    param2_label_ = new QLabel(this);
    param2_spin_ = new QDoubleSpinBox(this);
    param2_spin_->setRange(1, 500);
    param2_spin_->setDecimals(1);
    param2_spin_->setValue(26);

    param3_label_ = new QLabel(this);
    param3_spin_ = new QDoubleSpinBox(this);
    param3_spin_->setRange(0.1, 100);
    param3_spin_->setDecimals(1);
    param3_spin_->setValue(9);

    params_layout->addWidget(param1_label_);
    params_layout->addWidget(param1_spin_);
    params_layout->addWidget(param2_label_);
    params_layout->addWidget(param2_spin_);
    params_layout->addWidget(param3_label_);
    params_layout->addWidget(param3_spin_);
    params_layout->addStretch();
    main_layout->addWidget(params_container_);

    // Connections
    connect(indicator_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ConditionBlock::on_indicator_changed);
    connect(compare_mode_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ConditionBlock::on_compare_mode_changed);
    connect(compare_indicator_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) {
                populate_fields(compare_field_combo_,
                                compare_indicator_combo_->currentData().toString());
                emit changed();
            });
    connect(remove_btn_, &QPushButton::clicked, this, &ConditionBlock::remove_requested);
    connect(value_spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this]() { emit changed(); });
    connect(operator_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this]() { emit changed(); });

    on_indicator_changed(0);
    on_compare_mode_changed(0);
}

void ConditionBlock::populate_indicators() {
    QComboBox* combo = (sender() == nullptr) ? indicator_combo_ : compare_indicator_combo_;
    if (!combo) combo = indicator_combo_;

    auto inds = algo_indicators();
    QString last_cat;
    for (const auto& ind : inds) {
        if (ind.category != last_cat) {
            last_cat = ind.category;
            QString label = ind.category.toUpper();
            combo->addItem(QStringLiteral("-- %1 --").arg(label), QString());
        }
        combo->addItem(ind.label, ind.id);
    }
    // Also populate compare_indicator_combo_ on init
    if (combo == indicator_combo_ && compare_indicator_combo_) {
        compare_indicator_combo_->clear();
        for (const auto& ind : inds) {
            if (ind.category != last_cat) {
                last_cat = ind.category;
            }
            compare_indicator_combo_->addItem(ind.label, ind.id);
        }
    }
}

void ConditionBlock::populate_fields(QComboBox* combo, const QString& indicator_id) {
    combo->clear();
    auto inds = algo_indicators();
    for (const auto& ind : inds) {
        if (ind.id == indicator_id) {
            for (const auto& f : ind.fields)
                combo->addItem(f);
            break;
        }
    }
    if (combo->count() == 0)
        combo->addItem(QStringLiteral("value"));
}

void ConditionBlock::populate_operators() {
    auto ops = algo_operators();
    QStringList labels = {">", "<", ">=", "<=", "==",
                          "Crosses Above", "Crosses Below", "Rising", "Falling"};
    for (int i = 0; i < ops.size() && i < labels.size(); ++i)
        operator_combo_->addItem(labels[i], ops[i]);
}

void ConditionBlock::on_indicator_changed(int /*index*/) {
    QString ind_id = indicator_combo_->currentData().toString();
    if (ind_id.isEmpty()) {
        indicator_combo_->setCurrentIndex(indicator_combo_->currentIndex() + 1);
        return;
    }
    populate_fields(field_combo_, ind_id);
    update_param_visibility();
    emit changed();
}

void ConditionBlock::on_compare_mode_changed(int index) {
    bool is_indicator = (index == 1);
    value_spin_->setVisible(!is_indicator);
    compare_indicator_combo_->setVisible(is_indicator);
    compare_field_combo_->setVisible(is_indicator);
    emit changed();
}

void ConditionBlock::update_param_visibility() {
    QString ind_id = indicator_combo_->currentData().toString();
    auto inds = algo_indicators();

    QStringList param_names;
    for (const auto& ind : inds) {
        if (ind.id == ind_id) {
            param_names = ind.params;
            break;
        }
    }

    param1_label_->setVisible(!param_names.isEmpty());
    param1_spin_->setVisible(!param_names.isEmpty());
    param2_label_->setVisible(param_names.size() > 1);
    param2_spin_->setVisible(param_names.size() > 1);
    param3_label_->setVisible(param_names.size() > 2);
    param3_spin_->setVisible(param_names.size() > 2);

    if (param_names.size() > 0) param1_label_->setText(param_names[0] + ":");
    if (param_names.size() > 1) param2_label_->setText(param_names[1] + ":");
    if (param_names.size() > 2) param3_label_->setText(param_names[2] + ":");

    params_container_->setVisible(!param_names.isEmpty());
}

QJsonObject ConditionBlock::to_json() const {
    QJsonObject obj;
    obj["indicator"] = indicator_combo_->currentData().toString();
    obj["field"] = field_combo_->currentText();
    obj["operator"] = operator_combo_->currentData().toString();
    obj["compare_mode"] = compare_mode_combo_->currentData().toString();
    obj["value"] = value_spin_->value();

    // Build params
    QString ind_id = indicator_combo_->currentData().toString();
    auto inds = algo_indicators();
    QJsonObject params;
    for (const auto& ind : inds) {
        if (ind.id == ind_id) {
            if (ind.params.size() > 0) params[ind.params[0]] = static_cast<int>(param1_spin_->value());
            if (ind.params.size() > 1) params[ind.params[1]] = param2_spin_->value();
            if (ind.params.size() > 2) params[ind.params[2]] = param3_spin_->value();
            break;
        }
    }
    obj["params"] = params;

    if (compare_mode_combo_->currentData().toString() == "indicator") {
        obj["compare_indicator"] = compare_indicator_combo_->currentData().toString();
        obj["compare_field"] = compare_field_combo_->currentText();
        obj["compare_params"] = QJsonObject();
    }

    return obj;
}

void ConditionBlock::from_json(const QJsonObject& obj) {
    QString ind = obj.value("indicator").toString();
    for (int i = 0; i < indicator_combo_->count(); ++i) {
        if (indicator_combo_->itemData(i).toString() == ind) {
            indicator_combo_->setCurrentIndex(i);
            break;
        }
    }

    QString field = obj.value("field").toString();
    int fi = field_combo_->findText(field);
    if (fi >= 0) field_combo_->setCurrentIndex(fi);

    QString op = obj.value("operator").toString();
    for (int i = 0; i < operator_combo_->count(); ++i) {
        if (operator_combo_->itemData(i).toString() == op) {
            operator_combo_->setCurrentIndex(i);
            break;
        }
    }

    value_spin_->setValue(obj.value("value").toDouble());

    QString cmp = obj.value("compare_mode").toString("value");
    compare_mode_combo_->setCurrentIndex(cmp == "indicator" ? 1 : 0);

    if (cmp == "indicator") {
        QString ci = obj.value("compare_indicator").toString();
        for (int i = 0; i < compare_indicator_combo_->count(); ++i) {
            if (compare_indicator_combo_->itemData(i).toString() == ci) {
                compare_indicator_combo_->setCurrentIndex(i);
                break;
            }
        }
        QString cf = obj.value("compare_field").toString();
        int cfi = compare_field_combo_->findText(cf);
        if (cfi >= 0) compare_field_combo_->setCurrentIndex(cfi);
    }

    // Restore params
    auto params = obj.value("params").toObject();
    auto inds = algo_indicators();
    for (const auto& indef : inds) {
        if (indef.id == ind) {
            if (indef.params.size() > 0 && params.contains(indef.params[0]))
                param1_spin_->setValue(params.value(indef.params[0]).toDouble());
            if (indef.params.size() > 1 && params.contains(indef.params[1]))
                param2_spin_->setValue(params.value(indef.params[1]).toDouble());
            if (indef.params.size() > 2 && params.contains(indef.params[2]))
                param3_spin_->setValue(params.value(indef.params[2]).toDouble());
            break;
        }
    }
}

} // namespace fincept::ui::algo
