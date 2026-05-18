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
        auto* min_spin  = build_subspin("MIN",  min_default,    0);
        auto* max_spin  = build_subspin("MAX",  max_default,    1);
        auto* step_spin = build_subspin("STEP", p.step,         2);

        strategy_params_layout_->addWidget(range_row);
        range_row->setVisible(is_optimize);

        param_min_spinboxes_.append(min_spin);
        param_max_spinboxes_.append(max_spin);
        param_step_spinboxes_.append(step_spin);
        param_range_rows_.append(range_row);
    }
}

} // namespace fincept::screens
