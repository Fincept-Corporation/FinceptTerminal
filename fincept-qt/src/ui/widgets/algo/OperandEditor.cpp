// src/ui/widgets/algo/OperandEditor.cpp
#include "ui/widgets/algo/OperandEditor.h"

#include "ui/theme/Theme.h"

#include <QAbstractItemView>
#include <QCompleter>
#include <QLineEdit>
#include <QVBoxLayout>

using fincept::services::algo::algo_indicators;
using fincept::services::algo::IndicatorDef;
using fincept::services::algo::ParamSpec;

namespace fincept::ui::algo {

OperandEditor::OperandEditor(bool allow_value, QWidget* parent)
    : QWidget(parent), allow_value_(allow_value) {
    setObjectName(QStringLiteral("operandEditor"));
    build_ui();
}

void OperandEditor::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void OperandEditor::retranslateUi() {
    // Mode combo — re-apply the two fixed labels in place (data is preserved).
    if (mode_combo_) {
        for (int i = 0; i < mode_combo_->count(); ++i) {
            const QString data = mode_combo_->itemData(i).toString();
            if (data == "indicator")
                mode_combo_->setItemText(i, tr("Indicator"));
            else if (data == "value")
                mode_combo_->setItemText(i, tr("Value"));
        }
    }
    if (indicator_combo_ && indicator_combo_->lineEdit())
        indicator_combo_->lineEdit()->setPlaceholderText(tr("Search indicator…"));
    if (offset_btn_) {
        offset_btn_->setText(tr("⋯ bar"));
        offset_btn_->setToolTip(tr("Advanced: compare this against an earlier bar (e.g. 1 bar ago)"));
    }
    // Offset combo — re-derive each label from its stored bar-count (covers both
    // the preset rows and any custom offset added by set_offset()).
    if (offset_combo_) {
        for (int i = 0; i < offset_combo_->count(); ++i) {
            const int bars = offset_combo_->itemData(i).toInt();
            if (bars == 0)
                offset_combo_->setItemText(i, tr("Current bar"));
            else if (bars == 1)
                offset_combo_->setItemText(i, tr("1 bar ago"));
            else
                offset_combo_->setItemText(i, tr("%1 bars ago").arg(bars));
        }
    }
}

void OperandEditor::build_ui() {
    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(6);

    if (allow_value_) {
        mode_combo_ = new QComboBox(this);
        mode_combo_->setObjectName(QStringLiteral("operandMode"));
        mode_combo_->addItem(tr("Indicator"), QStringLiteral("indicator"));
        mode_combo_->addItem(tr("Value"), QStringLiteral("value"));
        row->addWidget(mode_combo_);
    }

    indicator_combo_ = new QComboBox(this);
    indicator_combo_->setObjectName(QStringLiteral("operandIndicator"));
    indicator_combo_->setMinimumWidth(130);
    populate_indicators();
    // Type-to-search across the indicator catalog (contains match, case-insensitive).
    indicator_combo_->setEditable(true);
    indicator_combo_->setInsertPolicy(QComboBox::NoInsert);
    indicator_combo_->lineEdit()->setPlaceholderText(tr("Search indicator…"));
    // The drop-down view and the type-to-search completer popup are separate
    // top-level list widgets the global stylesheet never reaches, so by default
    // their rows render with near-invisible, vertically-clipped text. Style both
    // to the dark theme explicitly: opaque background, readable row height, and
    // clear hover/selection states.
    const QString list_qss =
        QString("QAbstractItemView { background:%1; color:%2; border:1px solid %3; "
                "outline:0; selection-background-color:%4; selection-color:%5; }"
                "QAbstractItemView::item { min-height:22px; padding:3px 8px; border:0; }"
                "QAbstractItemView::item:hover { background:%6; }")
            .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED())
            .arg(ui::colors::ACCENT_BG(), ui::colors::TEXT_PRIMARY(), ui::colors::BG_HOVER());
    if (auto* view = indicator_combo_->view())
        view->setStyleSheet(list_qss);
    if (auto* c = indicator_combo_->completer()) {
        c->setCompletionMode(QCompleter::PopupCompletion);
        c->setFilterMode(Qt::MatchContains);
        c->setCaseSensitivity(Qt::CaseInsensitive);
        if (auto* popup = c->popup())
            popup->setStyleSheet(list_qss);
    }
    row->addWidget(indicator_combo_);

    params_container_ = new QWidget(this);
    params_layout_ = new QHBoxLayout(params_container_);
    params_layout_->setContentsMargins(0, 0, 0, 0);
    params_layout_->setSpacing(4);
    row->addWidget(params_container_);

    field_combo_ = new QComboBox(this);
    field_combo_->setObjectName(QStringLiteral("operandField"));
    field_combo_->setMinimumWidth(80);
    row->addWidget(field_combo_);

    // Advanced (opt-in): compare against an earlier bar. Hidden behind a toggle so
    // the common case (current bar) stays clutter-free.
    offset_btn_ = new QPushButton(tr("⋯ bar"), this);
    offset_btn_->setObjectName(QStringLiteral("operandOffsetToggle"));
    offset_btn_->setCheckable(true);
    offset_btn_->setFixedHeight(22);
    offset_btn_->setToolTip(tr("Advanced: compare this against an earlier bar (e.g. 1 bar ago)"));
    offset_combo_ = new QComboBox(this);
    offset_combo_->setObjectName(QStringLiteral("operandOffset"));
    offset_combo_->addItem(tr("Current bar"), 0);
    offset_combo_->addItem(tr("1 bar ago"), 1);
    offset_combo_->addItem(tr("2 bars ago"), 2);
    offset_combo_->addItem(tr("3 bars ago"), 3);
    offset_combo_->addItem(tr("5 bars ago"), 5);
    offset_combo_->addItem(tr("10 bars ago"), 10);
    offset_combo_->addItem(tr("20 bars ago"), 20);
    offset_combo_->setVisible(false);
    row->addWidget(offset_btn_);
    row->addWidget(offset_combo_);

    value_spin_ = new QDoubleSpinBox(this);
    value_spin_->setObjectName(QStringLiteral("operandValue"));
    value_spin_->setRange(-1e9, 1e9);
    value_spin_->setDecimals(4);
    row->addWidget(value_spin_);

    if (mode_combo_)
        connect(mode_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &OperandEditor::on_mode_changed);
    connect(indicator_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &OperandEditor::on_indicator_changed);
    connect(field_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { emit changed(); });
    connect(offset_btn_, &QPushButton::toggled, this, [this](bool on) {
        offset_combo_->setVisible(on && !is_value_mode());
        if (!on)
            offset_combo_->setCurrentIndex(0); // collapse back to "Current bar"
        emit changed();
    });
    connect(offset_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { emit changed(); });
    connect(value_spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double) { emit changed(); });

    // Land on the first real indicator (skip the leading category header).
    if (indicator_combo_->count() > 1)
        indicator_combo_->setCurrentIndex(1);
    on_indicator_changed();
    apply_mode_visibility();
}

void OperandEditor::populate_indicators() {
    QString last_cat;
    const auto inds = algo_indicators();
    for (const auto& ind : inds) {
        if (ind.category != last_cat) {
            last_cat = ind.category;
            indicator_combo_->addItem(QStringLiteral("-- %1 --").arg(ind.category.toUpper()), QString());
            // Make the header row non-selectable.
            const int hidx = indicator_combo_->count() - 1;
            indicator_combo_->model()->setData(
                indicator_combo_->model()->index(hidx, 0), false, Qt::UserRole - 1);
        }
        indicator_combo_->addItem(ind.label, ind.id);
    }
}

IndicatorDef OperandEditor::current_def() const {
    const QString id = indicator_combo_->currentData().toString();
    for (const auto& ind : algo_indicators())
        if (ind.id == id)
            return ind;
    return {};
}

void OperandEditor::rebuild_params() {
    // Tear down existing param spins.
    QLayoutItem* item;
    while ((item = params_layout_->takeAt(0)) != nullptr) {
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
    param_spins_.clear();
    param_names_.clear();

    const auto def = current_def();
    for (const auto& p : def.params) {
        auto* lbl = new QLabel(p.name + ":", params_container_);
        lbl->setObjectName(QStringLiteral("operandParamLabel"));
        auto* spin = new QDoubleSpinBox(params_container_);
        spin->setObjectName(QStringLiteral("operandParam"));
        spin->setRange(p.min, p.max);
        spin->setDecimals(p.decimals);
        spin->setSingleStep(p.step);
        spin->setValue(p.def);
        spin->setMaximumWidth(70);
        connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this](double) { emit changed(); });
        params_layout_->addWidget(lbl);
        params_layout_->addWidget(spin);
        param_spins_.append(spin);
        param_names_.append(p.name);
    }
    params_container_->setVisible(!def.params.isEmpty());
}

void OperandEditor::on_indicator_changed() {
    const QString id = indicator_combo_->currentData().toString();
    if (id.isEmpty()) { // landed on a category header — skip forward
        const int next = indicator_combo_->currentIndex() + 1;
        if (next < indicator_combo_->count())
            indicator_combo_->setCurrentIndex(next);
        return;
    }

    rebuild_params();

    const auto def = current_def();
    field_combo_->clear();
    field_combo_->addItems(def.fields.isEmpty() ? QStringList{"value"} : def.fields);
    // Output/field selector only matters for multi-output indicators.
    field_combo_->setVisible(field_combo_->count() > 1);

    emit changed();
}

void OperandEditor::on_mode_changed() {
    apply_mode_visibility();
    emit changed();
}

void OperandEditor::apply_mode_visibility() {
    const bool value_mode = is_value_mode();
    indicator_combo_->setVisible(!value_mode);
    params_container_->setVisible(!value_mode && !current_def().params.isEmpty());
    field_combo_->setVisible(!value_mode && field_combo_->count() > 1);
    offset_btn_->setVisible(!value_mode);
    offset_combo_->setVisible(!value_mode && offset_btn_->isChecked());
    value_spin_->setVisible(value_mode);
}

// ── State accessors ─────────────────────────────────────────────────────────

bool OperandEditor::is_value_mode() const {
    return mode_combo_ && mode_combo_->currentData().toString() == "value";
}

double OperandEditor::value() const { return value_spin_->value(); }

QString OperandEditor::indicator_id() const { return indicator_combo_->currentData().toString(); }

QJsonObject OperandEditor::params() const {
    QJsonObject obj;
    for (int i = 0; i < param_spins_.size(); ++i) {
        const double v = param_spins_[i]->value();
        // Integer-valued params serialize as ints; fractional (multiplier/std_dev) stay double.
        if (param_spins_[i]->decimals() == 0)
            obj[param_names_[i]] = static_cast<int>(v);
        else
            obj[param_names_[i]] = v;
    }
    return obj;
}

QString OperandEditor::field() const {
    return field_combo_->count() > 0 ? field_combo_->currentText() : QStringLiteral("value");
}

int OperandEditor::offset() const {
    return (offset_btn_ && offset_btn_->isChecked()) ? offset_combo_->currentData().toInt() : 0;
}

// ── State setters ───────────────────────────────────────────────────────────

void OperandEditor::set_value_mode(bool on) {
    if (mode_combo_)
        mode_combo_->setCurrentIndex(on ? 1 : 0);
    apply_mode_visibility();
}

void OperandEditor::set_value(double v) { value_spin_->setValue(v); }

void OperandEditor::set_indicator(const QString& id) {
    for (int i = 0; i < indicator_combo_->count(); ++i) {
        if (indicator_combo_->itemData(i).toString() == id) {
            indicator_combo_->setCurrentIndex(i);
            return;
        }
    }
}

void OperandEditor::set_params(const QJsonObject& p) {
    for (int i = 0; i < param_spins_.size(); ++i)
        if (p.contains(param_names_[i]))
            param_spins_[i]->setValue(p.value(param_names_[i]).toDouble());
}

void OperandEditor::set_field(const QString& f) {
    const int idx = field_combo_->findText(f);
    if (idx >= 0)
        field_combo_->setCurrentIndex(idx);
}

void OperandEditor::set_offset(int o) {
    if (o <= 0) {
        offset_btn_->setChecked(false);
        offset_combo_->setCurrentIndex(0);
        return;
    }
    int idx = offset_combo_->findData(o);
    if (idx < 0) { // a saved offset not in the preset list — add it
        offset_combo_->addItem(tr("%1 bars ago").arg(o), o);
        idx = offset_combo_->count() - 1;
    }
    offset_combo_->setCurrentIndex(idx);
    offset_btn_->setChecked(true); // reveals the picker (and emits changed)
}

} // namespace fincept::ui::algo
