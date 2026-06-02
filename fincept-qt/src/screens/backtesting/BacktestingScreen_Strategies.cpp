// src/screens/backtesting/BacktestingScreen_Strategies.cpp
//
// Strategy selection + dynamic parameter form rebuild. populate_strategies()
// fills the strategy combo from the active provider's catalog and triggers
// rebuild_strategy_params(), which rebuilds the per-parameter spinboxes
// (plus optimize-only MIN/MAX/STEP sub-rows).
//
// Part of the partial-class split of BacktestingScreen.cpp; helpers shared
// across split files live in BacktestingScreen_internal.h.

#include "screens/backtesting/BacktestingScreen.h"
#include "screens/backtesting/BacktestingScreen_internal.h"

#include "ui/theme/Theme.h"

#include <QGridLayout>

namespace fincept::screens {

using namespace fincept::services::backtest;
using fincept::screens::backtesting_internal::clear_layout;

void BacktestingScreen::on_strategy_changed(int index) {
    Q_UNUSED(index)
    rebuild_strategy_params();
}

void BacktestingScreen::populate_strategies() {
    strategy_combo_->blockSignals(true);
    strategy_combo_->clear();
    auto cat = strategy_category_combo_->currentText();
    for (const auto& s : strategies_) {
        if (s.category == cat)
            strategy_combo_->addItem(s.name, s.id);
    }
    strategy_combo_->blockSignals(false);

    if (strategy_combo_->count() > 0) {
        strategy_combo_->setCurrentIndex(0);
        rebuild_strategy_params();
    } else {
        clear_layout(strategy_params_layout_);
        param_spinboxes_.clear();
    }
}

void BacktestingScreen::rebuild_strategy_params() {
    clear_layout(strategy_params_layout_);
    param_spinboxes_.clear();
    param_min_spinboxes_.clear();
    param_max_spinboxes_.clear();
    param_step_spinboxes_.clear();
    param_range_rows_.clear();

    auto strategy_id = strategy_combo_->currentData().toString();
    if (strategy_id.isEmpty())
        return;

    const Strategy* strat = nullptr;
    for (const auto& s : strategies_) {
        if (s.id == strategy_id) {
            strat = &s;
            break;
        }
    }

    // Custom combo strategy: build the combo builder UI instead of param spinboxes
    if (strategy_id == "custom_combo") {
        auto label_s = QString("color:%1; font-size:%2px; font-weight:700; font-family:%3; letter-spacing:1px;")
                           .arg(ui::colors::TEXT_TERTIARY())
                           .arg(ui::fonts::TINY)
                           .arg(ui::fonts::DATA_FAMILY);
        auto combo_s = QString("QComboBox { background:%1; color:%2; border:1px solid %3;"
                               "font-family:%4; font-size:%5px; padding:4px 6px; }"
                               "QComboBox::drop-down { border:none; }"
                               "QComboBox QAbstractItemView { background:%1; color:%2; border:1px solid %3;"
                               "selection-background-color:rgba(217,119,6,0.15); }")
                           .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED())
                           .arg(ui::fonts::DATA_FAMILY)
                           .arg(ui::fonts::SMALL);
        auto input_s = QString("QSpinBox, QDoubleSpinBox { background:%1; color:%2; border:1px solid %3;"
                               "font-family:%4; font-size:%5px; padding:3px 4px; }")
                           .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED())
                           .arg(ui::fonts::DATA_FAMILY)
                           .arg(ui::fonts::SMALL);
        QStringList ind_types = {"rsi", "sma", "ema", "macd_hist", "bbands_pctb", "stochastic", "atr"};
        QStringList cond_types = {"below", "above", "cross_up", "cross_down"};

        auto build_ind_row = [&](const QString& title, ComboIndicatorRow& row) {
            auto* t = new QLabel(title, strategy_params_container_);
            t->setStyleSheet(label_s);
            strategy_params_layout_->addWidget(t);

            auto* tl = new QLabel(tr("TYPE"), strategy_params_container_);
            tl->setStyleSheet(label_s);
            strategy_params_layout_->addWidget(tl);
            row.type = new QComboBox(strategy_params_container_);
            row.type->setStyleSheet(combo_s);
            for (const auto& it : ind_types) row.type->addItem(it);
            strategy_params_layout_->addWidget(row.type);

            auto* pl = new QLabel(tr("PERIOD"), strategy_params_container_);
            pl->setStyleSheet(label_s);
            strategy_params_layout_->addWidget(pl);
            row.period = new QSpinBox(strategy_params_container_);
            row.period->setRange(2, 200);
            row.period->setValue(14);
            row.period->setStyleSheet(input_s);
            strategy_params_layout_->addWidget(row.period);

            auto* cl = new QLabel(tr("CONDITION"), strategy_params_container_);
            cl->setStyleSheet(label_s);
            strategy_params_layout_->addWidget(cl);
            row.cond = new QComboBox(strategy_params_container_);
            row.cond->setStyleSheet(combo_s);
            for (const auto& c : cond_types) row.cond->addItem(c);
            strategy_params_layout_->addWidget(row.cond);

            auto* vl = new QLabel(tr("THRESHOLD"), strategy_params_container_);
            vl->setStyleSheet(label_s);
            strategy_params_layout_->addWidget(vl);
            row.val = new QDoubleSpinBox(strategy_params_container_);
            row.val->setRange(-1000, 1000);
            row.val->setValue(30);
            row.val->setDecimals(2);
            row.val->setStyleSheet(input_s);
            strategy_params_layout_->addWidget(row.val);
        };

        build_ind_row(tr("INDICATOR 1"), combo_rows_[0]);
        build_ind_row(tr("INDICATOR 2"), combo_rows_[1]);

        // Defaults for indicator 2
        combo_rows_[1].type->setCurrentText("sma");
        combo_rows_[1].cond->setCurrentText("above");
        combo_rows_[1].val->setValue(0);
        combo_rows_[1].period->setValue(20);

        // Indicator 3 (optional, hidden by default)
        auto check_s = QString("QCheckBox { color:%1; font-family:%2; font-size:%3px; spacing:6px; }"
                               "QCheckBox::indicator { width:14px; height:14px; border:1px solid %4; background:%5; }"
                               "QCheckBox::indicator:checked { background:%6; border-color:%6; }")
                           .arg(ui::colors::TEXT_SECONDARY(), ui::fonts::DATA_FAMILY)
                           .arg(ui::fonts::SMALL)
                           .arg(ui::colors::BORDER_MED(), ui::colors::BG_RAISED(), ui::colors::AMBER());
        combo_ind3_enabled_ = new QCheckBox(tr("Enable Indicator 3"), strategy_params_container_);
        combo_ind3_enabled_->setStyleSheet(check_s);
        strategy_params_layout_->addWidget(combo_ind3_enabled_);

        // Build row 3 widgets but wrap them so we can toggle visibility
        auto* ind3_container = new QWidget(strategy_params_container_);
        auto* ind3_layout_save = strategy_params_layout_;
        auto* ind3_inner = new QVBoxLayout(ind3_container);
        ind3_inner->setContentsMargins(0, 0, 0, 0);
        ind3_inner->setSpacing(4);
        strategy_params_layout_ = ind3_inner;
        build_ind_row(tr("INDICATOR 3"), combo_rows_[2]);
        combo_rows_[2].type->setCurrentText("macd_hist");
        combo_rows_[2].cond->setCurrentText("above");
        combo_rows_[2].val->setValue(0);
        combo_rows_[2].period->setValue(26);
        strategy_params_layout_ = ind3_layout_save;
        ind3_container->setVisible(false);
        strategy_params_layout_->addWidget(ind3_container);
        connect(combo_ind3_enabled_, &QCheckBox::toggled, ind3_container, &QWidget::setVisible);

        auto* logic_lbl = new QLabel(tr("COMBINE LOGIC"), strategy_params_container_);
        logic_lbl->setStyleSheet(label_s);
        strategy_params_layout_->addWidget(logic_lbl);
        combo_logic_ = new QComboBox(strategy_params_container_);
        combo_logic_->setStyleSheet(combo_s);
        combo_logic_->addItem("and");
        combo_logic_->addItem("or");
        strategy_params_layout_->addWidget(combo_logic_);
        // Guard dynamically created widgets against accidental wheel changes
        auto* wg = property("_wheel_guard").value<QObject*>();
        if (wg) backtesting_internal::guard_all_inputs(strategy_params_container_, wg);
        return;
    }

    if (!strat || strat->params.isEmpty())
        return;

    auto input_style = QString("QDoubleSpinBox { background:%1; color:%2; border:1px solid %3;"
                               "font-family:%4; font-size:%5px; padding:3px 4px; }"
                               "QDoubleSpinBox:focus { border-color:%6; }")
                           .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED())
                           .arg(ui::fonts::DATA_FAMILY)
                           .arg(ui::fonts::SMALL)
                           .arg(ui::colors::BORDER_BRIGHT());

    auto sublabel_style = QString("color:%1; font-size:%2px; font-family:%3;")
                              .arg(ui::colors::TEXT_DIM())
                              .arg(ui::fonts::TINY)
                              .arg(ui::fonts::DATA_FAMILY);

    auto label_style = QString("color:%1; font-size:%2px; font-family:%3;")
                           .arg(ui::colors::TEXT_TERTIARY())
                           .arg(ui::fonts::TINY)
                           .arg(ui::fonts::DATA_FAMILY);

    const bool is_optimize = (active_command_ >= 0
                              && active_command_ < commands_.size()
                              && commands_[active_command_].id == "optimize");

    for (const auto& p : strat->params) {
        auto* lbl = new QLabel(p.label.toUpper(), strategy_params_container_);
        lbl->setStyleSheet(label_style);
        strategy_params_layout_->addWidget(lbl);

        auto* spin = new QDoubleSpinBox(strategy_params_container_);
        spin->setRange(p.min_val, p.max_val);
        spin->setValue(p.default_val);
        spin->setSingleStep(p.step);
        spin->setDecimals(p.step < 1.0 ? 2 : 0);
        spin->setStyleSheet(input_style);
        spin->setProperty("param_name", p.name);
        strategy_params_layout_->addWidget(spin);
        param_spinboxes_.append(spin);

        // ── Optimize-only: explicit MIN / MAX / STEP range editor ──
        // Built up front so we can flip visibility in update_section_visibility()
        // without having to rebuild the whole strategy form on command change.
        auto* range_row = new QWidget(strategy_params_container_);
        auto* hl = new QGridLayout(range_row);
        hl->setContentsMargins(0, 0, 0, 4);
        hl->setHorizontalSpacing(4);
        hl->setVerticalSpacing(2);

        auto build_subspin = [&](const QString& sub_label, double default_val, int col) -> QDoubleSpinBox* {
            auto* sl = new QLabel(sub_label, range_row);
            sl->setStyleSheet(sublabel_style);
            hl->addWidget(sl, 0, col);
            auto* s = new QDoubleSpinBox(range_row);
            s->setRange(p.min_val, p.max_val);
            s->setValue(default_val);
            s->setSingleStep(p.step);
            s->setDecimals(p.step < 1.0 ? 2 : 0);
            s->setStyleSheet(input_style);
            hl->addWidget(s, 1, col);
            return s;
        };

        // Default range: seed value ± a couple of steps, snapped to param bounds.
        const double span = qMax(p.step, (p.max_val - p.min_val) * 0.1);
        const double min_default = qMax(p.min_val, p.default_val - span);
        const double max_default = qMin(p.max_val, p.default_val + span);
        auto* min_spin  = build_subspin(tr("MIN"),  min_default,    0);
        auto* max_spin  = build_subspin(tr("MAX"),  max_default,    1);
        auto* step_spin = build_subspin(tr("STEP"), p.step,         2);

        strategy_params_layout_->addWidget(range_row);
        range_row->setVisible(is_optimize);

        param_min_spinboxes_.append(min_spin);
        param_max_spinboxes_.append(max_spin);
        param_step_spinboxes_.append(step_spin);
        param_range_rows_.append(range_row);
    }

    auto* wg = property("_wheel_guard").value<QObject*>();
    if (wg) backtesting_internal::guard_all_inputs(strategy_params_container_, wg);
}

} // namespace fincept::screens
