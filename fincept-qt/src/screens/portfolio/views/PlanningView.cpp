// src/screens/portfolio/views/PlanningView.cpp
#include "screens/portfolio/views/PlanningView.h"

#include "storage/repositories/SettingsRepository.h"
#include "ui/theme/Theme.h"

#include <QAbstractItemView>
#include <QColor>
#include <QEvent>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QTabBar>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>

namespace fincept::screens {

PlanningView::PlanningView(QWidget* parent) : QWidget(parent) {
    build_ui();
}

void PlanningView::build_ui() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    tabs_ = new QTabWidget;
    tabs_->setDocumentMode(true);
    // Tabs must show their full label ("RETIREMENT", not "RETIRE…"). Size each
    // tab to its text and never elide; scroll buttons appear only if the bar
    // genuinely can't fit (it can, for these three short labels).
    tabs_->tabBar()->setElideMode(Qt::ElideNone);
    tabs_->tabBar()->setExpanding(false);
    tabs_->tabBar()->setUsesScrollButtons(false);
    tabs_->setStyleSheet(QString("QTabWidget::pane { border:0; background:%1; }"
                                 "QTabBar::tab { background:%2; color:%3; padding:6px 16px; border:0;"
                                 "  border-bottom:2px solid transparent; font-size:9px; font-weight:700;"
                                 "  letter-spacing:0.5px; min-width:72px; }"
                                 "QTabBar::tab:selected { color:%4; border-bottom:2px solid %4; }"
                                 "QTabBar::tab:hover { color:%5; }")
                             .arg(ui::colors::BG_BASE(), ui::colors::BG_SURFACE(), ui::colors::TEXT_SECONDARY(),
                                  ui::colors::AMBER(), ui::colors::TEXT_PRIMARY()));

    retirement_tab_index_ = tabs_->addTab(build_retirement_tab(), tr("RETIREMENT"));
    goals_tab_index_ = tabs_->addTab(build_goals_tab(), tr("GOALS"));
    savings_tab_index_ = tabs_->addTab(build_savings_tab(), tr("SAVINGS"));

    layout->addWidget(tabs_);
}

QWidget* PlanningView::build_retirement_tab() {
    auto* w = new QWidget(this);
    auto* layout = new QHBoxLayout(w);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(24);

    // Left: inputs
    auto* input_panel = new QWidget(this);
    input_panel->setFixedWidth(320);
    input_panel->setStyleSheet(
        QString("background:%1; border:1px solid %2;").arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    auto* input_layout = new QVBoxLayout(input_panel);
    input_layout->setContentsMargins(16, 12, 16, 12);
    input_layout->setSpacing(8);

    input_title_ = new QLabel(tr("RETIREMENT CALCULATOR"));
    input_title_->setStyleSheet(
        QString("color:%1; font-size:11px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER()));
    input_layout->addWidget(input_title_);

    retire_assump_note_ = new QLabel;
    retire_assump_note_->setWordWrap(true);
    retire_assump_note_->setStyleSheet(QString("color:%1; font-size:9px; padding:2px 0 4px 0;").arg(ui::colors::CYAN()));
    input_layout->addWidget(retire_assump_note_);

    auto* form = new QFormLayout;
    form->setSpacing(8);
    form->setLabelAlignment(Qt::AlignRight);

    auto style_spin = [](QDoubleSpinBox* sb) {
        sb->setFixedHeight(26);
        sb->setStyleSheet(
            QString("QDoubleSpinBox { background:%1; color:%2; border:1px solid %3;"
                    "  padding:0 8px; font-size:11px; }"
                    "QDoubleSpinBox:focus { border-color:%4; }")
                .arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(), ui::colors::AMBER()));
    };
    auto label_style = QString("color:%1; font-size:10px;").arg(ui::colors::TEXT_SECONDARY());

    current_age_ = new QDoubleSpinBox;
    current_age_->setRange(18, 100);
    current_age_->setValue(30);
    current_age_->setDecimals(0);
    style_spin(current_age_);
    l_current_age_ = new QLabel(tr("Current Age:"));
    l_current_age_->setStyleSheet(label_style);
    form->addRow(l_current_age_, current_age_);

    retire_age_ = new QDoubleSpinBox;
    retire_age_->setRange(30, 100);
    retire_age_->setValue(65);
    retire_age_->setDecimals(0);
    style_spin(retire_age_);
    l_retire_age_ = new QLabel(tr("Retire Age:"));
    l_retire_age_->setStyleSheet(label_style);
    form->addRow(l_retire_age_, retire_age_);

    annual_expense_ = new QDoubleSpinBox;
    annual_expense_->setRange(0, 10000000);
    annual_expense_->setValue(60000);
    annual_expense_->setPrefix("$ ");
    annual_expense_->setDecimals(0);
    style_spin(annual_expense_);
    l_annual_expense_ = new QLabel(tr("Annual Expense:"));
    l_annual_expense_->setStyleSheet(label_style);
    form->addRow(l_annual_expense_, annual_expense_);

    monthly_contrib_ = new QDoubleSpinBox;
    monthly_contrib_->setRange(0, 1000000);
    monthly_contrib_->setValue(2000);
    monthly_contrib_->setPrefix("$ ");
    monthly_contrib_->setDecimals(0);
    style_spin(monthly_contrib_);
    l_monthly_contrib_ = new QLabel(tr("Monthly Savings:"));
    l_monthly_contrib_->setStyleSheet(label_style);
    form->addRow(l_monthly_contrib_, monthly_contrib_);

    expected_return_ = new QDoubleSpinBox;
    expected_return_->setRange(0, 30);
    expected_return_->setValue(8);
    expected_return_->setSuffix(" %");
    expected_return_->setDecimals(1);
    style_spin(expected_return_);
    l_expected_return_ = new QLabel(tr("Exp. Return:"));
    l_expected_return_->setStyleSheet(label_style);
    form->addRow(l_expected_return_, expected_return_);

    inflation_ = new QDoubleSpinBox;
    inflation_->setRange(0, 20);
    inflation_->setValue(3);
    inflation_->setSuffix(" %");
    inflation_->setDecimals(1);
    style_spin(inflation_);
    l_inflation_ = new QLabel(tr("Inflation:"));
    l_inflation_->setStyleSheet(label_style);
    form->addRow(l_inflation_, inflation_);

    withdrawal_rate_ = new QDoubleSpinBox;
    withdrawal_rate_->setRange(2, 8);
    withdrawal_rate_->setValue(4);
    withdrawal_rate_->setSuffix(" %");
    withdrawal_rate_->setDecimals(1);
    withdrawal_rate_->setSingleStep(0.5);
    style_spin(withdrawal_rate_);
    l_withdrawal_rate_ = new QLabel(tr("Withdrawal Rate:"));
    l_withdrawal_rate_->setStyleSheet(label_style);
    form->addRow(l_withdrawal_rate_, withdrawal_rate_);

    input_layout->addLayout(form);

    calc_btn_ = new QPushButton(tr("CALCULATE"));
    calc_btn_->setFixedHeight(28);
    calc_btn_->setCursor(Qt::PointingHandCursor);
    calc_btn_->setStyleSheet(
        QString("QPushButton { background:%1; color:#000; border:none; font-size:10px; font-weight:700; }"
                "QPushButton:hover { background:%2; }")
            .arg(ui::colors::AMBER(), ui::colors::WARNING()));
    connect(calc_btn_, &QPushButton::clicked, this, &PlanningView::recalculate);
    input_layout->addWidget(calc_btn_);
    input_layout->addStretch();

    layout->addWidget(input_panel);

    // Right: results
    auto* results = new QWidget(this);
    auto* res_layout = new QVBoxLayout(results);
    res_layout->setContentsMargins(0, 0, 0, 0);
    res_layout->setSpacing(12);

    res_title_ = new QLabel(tr("PROJECTION RESULTS"));
    res_title_->setStyleSheet(
        QString("color:%1; font-size:12px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER()));
    res_layout->addWidget(res_title_);

    auto* grid = new QGridLayout;
    grid->setSpacing(8);

    auto add_card = [&](int r, int c, const QString& label, QLabel*& val_label, QLabel*& title_label,
                        const char* color) {
        auto* card = new QWidget(this);
        card->setStyleSheet(
            QString("background:%1; border:1px solid %2;").arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
        auto* cl = new QVBoxLayout(card);
        cl->setContentsMargins(12, 8, 12, 8);
        cl->setSpacing(2);

        title_label = new QLabel(label);
        title_label->setStyleSheet(
            QString("color:%1; font-size:8px; font-weight:700; letter-spacing:0.5px;").arg(ui::colors::TEXT_TERTIARY()));
        cl->addWidget(title_label);

        val_label = new QLabel("--");
        val_label->setStyleSheet(QString("color:%1; font-size:18px; font-weight:700;").arg(color));
        cl->addWidget(val_label);

        grid->addWidget(card, r, c);
    };

    add_card(0, 0, tr("YEARS TO RETIREMENT"), years_label_, years_card_label_, ui::colors::CYAN);
    add_card(0, 1, tr("TARGET NEST EGG"), target_label_, target_card_label_, ui::colors::WARNING);
    add_card(1, 0, tr("PROJECTED VALUE"), projected_label_, projected_card_label_, ui::colors::POSITIVE);
    add_card(1, 1, tr("SURPLUS / GAP"), gap_label_, gap_card_label_, ui::colors::TEXT_PRIMARY);

    res_layout->addLayout(grid);

    // Monte Carlo outcome (probability + percentile band).
    retire_mc_label_ = new QLabel;
    retire_mc_label_->setWordWrap(true);
    retire_mc_label_->setStyleSheet(QString("color:%1; font-size:11px; padding:8px 12px 0 12px; background:transparent;")
                                        .arg(ui::colors::TEXT_PRIMARY()));
    res_layout->addWidget(retire_mc_label_);

    // Drawdown/VaR stress line (real metrics).
    retire_stress_label_ = new QLabel;
    retire_stress_label_->setWordWrap(true);
    retire_stress_label_->setStyleSheet(QString("color:%1; font-size:11px; padding:2px 12px 0 12px; background:transparent;")
                                            .arg(ui::colors::NEGATIVE()));
    res_layout->addWidget(retire_stress_label_);

    status_label_ = new QLabel;
    status_label_->setWordWrap(true);
    status_label_->setStyleSheet(QString("color:%1; font-size:12px; padding:12px;").arg(ui::colors::TEXT_SECONDARY()));
    res_layout->addWidget(status_label_);

    // Cross-link: optimize the portfolio for the return this plan needs.
    retire_optimize_btn_ = new QPushButton(tr("⚙  OPTIMIZE PORTFOLIO FOR THIS GOAL"));
    retire_optimize_btn_->setCursor(Qt::PointingHandCursor);
    retire_optimize_btn_->setFixedHeight(28);
    retire_optimize_btn_->setStyleSheet(
        QString("QPushButton { background:transparent; color:%1; border:1px solid %1; font-size:10px;"
                "  font-weight:700; letter-spacing:1px; } QPushButton:hover { background:%2; color:#000; }")
            .arg(ui::colors::CYAN(), ui::colors::CYAN()));
    connect(retire_optimize_btn_, &QPushButton::clicked, this, [this]() {
        // Annualised return the plan needs to exactly hit the target; hand it to
        // the Optimization tab so the user can build a portfolio that targets it.
        const double years = retire_age_->value() - current_age_->value();
        const double target = (withdrawal_rate_->value() > 1e-6)
                                  ? (annual_expense_->value() / (withdrawal_rate_->value() / 100.0))
                                  : annual_expense_->value() * 25.0;
        const double pv = summary_.total_market_value;
        const double contrib = monthly_contrib_->value() * 12.0;
        // Solve required real return r s.t. FV(pv, contrib, years, r) = target (bisection).
        double lo = -0.10, hi = 0.40, req = 0.08;
        for (int it = 0; it < 60; ++it) {
            req = 0.5 * (lo + hi);
            double bal = pv;
            for (int y = 0; y < static_cast<int>(years); ++y)
                bal = bal * (1.0 + req) + contrib;
            (bal < target ? lo : hi) = req;
        }
        emit optimize_for_return(std::max(0.0, req));
    });
    res_layout->addWidget(retire_optimize_btn_);

    res_layout->addStretch();
    layout->addWidget(results, 1);

    return w;
}

QWidget* PlanningView::build_goals_tab() {
    auto* w = new QWidget(this);
    auto* layout = new QHBoxLayout(w);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(24);

    auto style_spin = [](QDoubleSpinBox* sb) {
        sb->setFixedHeight(26);
        sb->setStyleSheet(
            QString("QDoubleSpinBox { background:%1; color:%2; border:1px solid %3;"
                    "  padding:0 8px; font-size:11px; }"
                    "QDoubleSpinBox:focus { border-color:%4; }")
                .arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(), ui::colors::AMBER()));
    };
    const QString label_style = QString("color:%1; font-size:10px;").arg(ui::colors::TEXT_SECONDARY());

    // ── Left: inputs ──────────────────────────────────────────────────────────
    auto* input_panel = new QWidget(this);
    input_panel->setFixedWidth(320);
    input_panel->setStyleSheet(
        QString("background:%1; border:1px solid %2;").arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    auto* input_layout = new QVBoxLayout(input_panel);
    input_layout->setContentsMargins(16, 12, 16, 12);
    input_layout->setSpacing(8);

    goals_title_ = new QLabel(tr("GOAL-BASED PLANNING"));
    goals_title_->setStyleSheet(
        QString("color:%1; font-size:11px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER()));
    input_layout->addWidget(goals_title_);

    auto* form = new QFormLayout;
    form->setSpacing(8);
    form->setLabelAlignment(Qt::AlignRight);

    goal_name_ = new QLineEdit(tr("House Down Payment"));
    goal_name_->setFixedHeight(26);
    goal_name_->setStyleSheet(
        QString("QLineEdit { background:%1; color:%2; border:1px solid %3; padding:0 8px; font-size:11px; }"
                "QLineEdit:focus { border-color:%4; }")
            .arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(), ui::colors::AMBER()));
    l_goal_name_ = new QLabel(tr("Goal:"));
    l_goal_name_->setStyleSheet(label_style);
    form->addRow(l_goal_name_, goal_name_);

    goal_target_ = new QDoubleSpinBox;
    goal_target_->setRange(0, 1000000000);
    goal_target_->setValue(100000);
    goal_target_->setPrefix("$ ");
    goal_target_->setDecimals(0);
    style_spin(goal_target_);
    l_goal_target_ = new QLabel(tr("Target Amount:"));
    l_goal_target_->setStyleSheet(label_style);
    form->addRow(l_goal_target_, goal_target_);

    goal_years_ = new QDoubleSpinBox;
    goal_years_->setRange(1, 60);
    goal_years_->setValue(5);
    goal_years_->setDecimals(0);
    style_spin(goal_years_);
    l_goal_years_ = new QLabel(tr("Years to Goal:"));
    l_goal_years_->setStyleSheet(label_style);
    form->addRow(l_goal_years_, goal_years_);

    goal_return_ = new QDoubleSpinBox;
    goal_return_->setRange(0, 30);
    goal_return_->setValue(6);
    goal_return_->setSuffix(" %");
    goal_return_->setDecimals(1);
    style_spin(goal_return_);
    l_goal_return_ = new QLabel(tr("Exp. Return:"));
    l_goal_return_->setStyleSheet(label_style);
    form->addRow(l_goal_return_, goal_return_);

    goal_monthly_ = new QDoubleSpinBox;
    goal_monthly_->setRange(0, 1000000);
    goal_monthly_->setValue(1000);
    goal_monthly_->setPrefix("$ ");
    goal_monthly_->setDecimals(0);
    style_spin(goal_monthly_);
    l_goal_monthly_ = new QLabel(tr("Monthly Saving:"));
    l_goal_monthly_->setStyleSheet(label_style);
    form->addRow(l_goal_monthly_, goal_monthly_);

    goal_alloc_pct_ = new QDoubleSpinBox;
    goal_alloc_pct_->setRange(0, 100);
    goal_alloc_pct_->setValue(0);
    goal_alloc_pct_->setSuffix(" %");
    goal_alloc_pct_->setDecimals(0);
    style_spin(goal_alloc_pct_);
    l_goal_alloc_ = new QLabel(tr("% Portfolio Allocated:"));
    l_goal_alloc_->setStyleSheet(label_style);
    form->addRow(l_goal_alloc_, goal_alloc_pct_);

    input_layout->addLayout(form);

    goals_calc_btn_ = new QPushButton(tr("ADD / UPDATE GOAL"));
    goals_calc_btn_->setFixedHeight(28);
    goals_calc_btn_->setCursor(Qt::PointingHandCursor);
    goals_calc_btn_->setStyleSheet(
        QString("QPushButton { background:%1; color:#000; border:none; font-size:10px; font-weight:700; }"
                "QPushButton:hover { background:%2; }")
            .arg(ui::colors::AMBER(), ui::colors::WARNING()));
    connect(goals_calc_btn_, &QPushButton::clicked, this, [this]() {
        recalculate_goals(); // refresh the projection cards for the typed goal
        add_or_update_goal(); // and persist it into the saved list
    });
    input_layout->addWidget(goals_calc_btn_);
    input_layout->addStretch();

    layout->addWidget(input_panel);

    // ── Right: results ──────────────────────────────────────────────────────────
    auto* results = new QWidget(this);
    auto* res_layout = new QVBoxLayout(results);
    res_layout->setContentsMargins(0, 0, 0, 0);
    res_layout->setSpacing(12);

    goals_res_title_ = new QLabel(tr("GOAL PROJECTION"));
    goals_res_title_->setStyleSheet(
        QString("color:%1; font-size:12px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER()));
    res_layout->addWidget(goals_res_title_);

    auto* grid = new QGridLayout;
    grid->setSpacing(8);
    auto add_card = [&](int r, int c, const QString& label, QLabel*& val_label, QLabel*& title_label,
                        const char* color) {
        auto* card = new QWidget(this);
        card->setStyleSheet(
            QString("background:%1; border:1px solid %2;").arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
        auto* cl = new QVBoxLayout(card);
        cl->setContentsMargins(12, 8, 12, 8);
        cl->setSpacing(2);
        title_label = new QLabel(label);
        title_label->setStyleSheet(
            QString("color:%1; font-size:8px; font-weight:700; letter-spacing:0.5px;").arg(ui::colors::TEXT_TERTIARY()));
        cl->addWidget(title_label);
        val_label = new QLabel("--");
        val_label->setStyleSheet(QString("color:%1; font-size:18px; font-weight:700;").arg(color));
        cl->addWidget(val_label);
        grid->addWidget(card, r, c);
    };
    add_card(0, 0, tr("TARGET AMOUNT"), goal_target_value_label_, goal_target_card_label_, ui::colors::WARNING);
    add_card(0, 1, tr("PROJECTED VALUE"), goal_projected_label_, goal_projected_card_label_, ui::colors::POSITIVE);
    add_card(1, 0, tr("REQUIRED MONTHLY"), goal_required_label_, goal_required_card_label_, ui::colors::CYAN);
    add_card(1, 1, tr("PROGRESS"), goal_progress_label_, goal_progress_card_label_, ui::colors::TEXT_PRIMARY);
    res_layout->addLayout(grid);

    goal_mc_label_ = new QLabel;
    goal_mc_label_->setWordWrap(true);
    goal_mc_label_->setTextFormat(Qt::RichText);
    goal_mc_label_->setStyleSheet(QString("color:%1; font-size:11px; padding:0 12px;").arg(ui::colors::TEXT_PRIMARY()));
    res_layout->addWidget(goal_mc_label_);

    goal_status_label_ = new QLabel;
    goal_status_label_->setWordWrap(true);
    goal_status_label_->setStyleSheet(QString("color:%1; font-size:12px; padding:12px;").arg(ui::colors::TEXT_SECONDARY()));
    res_layout->addWidget(goal_status_label_);

    // ── Saved goals list (per-portfolio, with funding %) ─────────────────────
    auto* saved_title = new QLabel(tr("SAVED GOALS"));
    saved_title->setStyleSheet(
        QString("color:%1; font-size:10px; font-weight:700; letter-spacing:1px; padding:0 0 0 12px;")
            .arg(ui::colors::TEXT_TERTIARY()));
    res_layout->addWidget(saved_title);

    goals_list_ = new QTableWidget;
    goals_list_->setColumnCount(4);
    goals_list_->setHorizontalHeaderLabels({tr("GOAL"), tr("TARGET"), tr("FUNDED NOW"), tr("ON TRACK")});
    goals_list_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    goals_list_->verticalHeader()->setVisible(false);
    goals_list_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    goals_list_->setSelectionBehavior(QAbstractItemView::SelectRows);
    goals_list_->setSelectionMode(QAbstractItemView::SingleSelection);
    goals_list_->setStyleSheet(
        QString("QTableWidget { background:%1; color:%2; border:1px solid %3; gridline-color:%3; font-size:11px; }"
                "QHeaderView::section { background:%4; color:%5; border:0; padding:5px; font-size:9px;"
                "  font-weight:700; letter-spacing:0.5px; }")
            .arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(), ui::colors::BG_SURFACE(),
                 ui::colors::TEXT_SECONDARY()));
    // Clicking a saved goal loads it back into the editor.
    connect(goals_list_, &QTableWidget::cellClicked, this, [this](int row, int) {
        if (row < 0 || row >= goals_.size())
            return;
        const Goal& g = goals_[row];
        if (goal_name_) goal_name_->setText(g.name);
        if (goal_target_) goal_target_->setValue(g.target);
        if (goal_years_) goal_years_->setValue(g.years);
        if (goal_monthly_) goal_monthly_->setValue(g.monthly);
        if (goal_alloc_pct_) goal_alloc_pct_->setValue(g.alloc_pct);
        recalculate_goals();
    });
    res_layout->addWidget(goals_list_, 1);

    goal_remove_btn_ = new QPushButton(tr("REMOVE SELECTED GOAL"));
    goal_remove_btn_->setFixedHeight(24);
    goal_remove_btn_->setCursor(Qt::PointingHandCursor);
    goal_remove_btn_->setStyleSheet(
        QString("QPushButton { background:transparent; color:%1; border:1px solid %2; font-size:9px;"
                "  font-weight:700; } QPushButton:hover { color:%3; border-color:%3; }")
            .arg(ui::colors::TEXT_TERTIARY(), ui::colors::BORDER_DIM(), ui::colors::NEGATIVE()));
    connect(goal_remove_btn_, &QPushButton::clicked, this, &PlanningView::remove_selected_goal);
    res_layout->addWidget(goal_remove_btn_);

    layout->addWidget(results, 1);
    return w;
}

QWidget* PlanningView::build_savings_tab() {
    auto* w = new QWidget(this);
    auto* layout = new QHBoxLayout(w);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(24);

    auto style_spin = [](QDoubleSpinBox* sb) {
        sb->setFixedHeight(26);
        sb->setStyleSheet(
            QString("QDoubleSpinBox { background:%1; color:%2; border:1px solid %3;"
                    "  padding:0 8px; font-size:11px; }"
                    "QDoubleSpinBox:focus { border-color:%4; }")
                .arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(), ui::colors::AMBER()));
    };
    const QString label_style = QString("color:%1; font-size:10px;").arg(ui::colors::TEXT_SECONDARY());

    // ── Left: inputs ──────────────────────────────────────────────────────────
    auto* input_panel = new QWidget(this);
    input_panel->setFixedWidth(320);
    input_panel->setStyleSheet(
        QString("background:%1; border:1px solid %2;").arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    auto* input_layout = new QVBoxLayout(input_panel);
    input_layout->setContentsMargins(16, 12, 16, 12);
    input_layout->setSpacing(8);

    savings_title_ = new QLabel(tr("SAVINGS RATE ANALYSIS"));
    savings_title_->setStyleSheet(
        QString("color:%1; font-size:11px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER()));
    input_layout->addWidget(savings_title_);

    savings_desc_ = new QLabel(tr("Projects wealth accumulation across a range of savings rates, "
                                  "starting from your current portfolio value."));
    savings_desc_->setWordWrap(true);
    savings_desc_->setStyleSheet(QString("color:%1; font-size:10px;").arg(ui::colors::TEXT_TERTIARY()));
    input_layout->addWidget(savings_desc_);

    sav_assump_note_ = new QLabel;
    sav_assump_note_->setWordWrap(true);
    sav_assump_note_->setStyleSheet(QString("color:%1; font-size:9px; padding:2px 0;").arg(ui::colors::CYAN()));
    input_layout->addWidget(sav_assump_note_);

    auto* form = new QFormLayout;
    form->setSpacing(8);
    form->setLabelAlignment(Qt::AlignRight);

    sav_income_ = new QDoubleSpinBox;
    sav_income_->setRange(0, 100000000);
    sav_income_->setValue(120000);
    sav_income_->setPrefix("$ ");
    sav_income_->setDecimals(0);
    style_spin(sav_income_);
    l_sav_income_ = new QLabel(tr("Annual Income:"));
    l_sav_income_->setStyleSheet(label_style);
    form->addRow(l_sav_income_, sav_income_);

    sav_rate_ = new QDoubleSpinBox;
    sav_rate_->setRange(0, 90);
    sav_rate_->setValue(20);
    sav_rate_->setSuffix(" %");
    sav_rate_->setDecimals(0);
    style_spin(sav_rate_);
    l_sav_rate_ = new QLabel(tr("Savings Rate:"));
    l_sav_rate_->setStyleSheet(label_style);
    form->addRow(l_sav_rate_, sav_rate_);

    sav_return_ = new QDoubleSpinBox;
    sav_return_->setRange(0, 30);
    sav_return_->setValue(7);
    sav_return_->setSuffix(" %");
    sav_return_->setDecimals(1);
    style_spin(sav_return_);
    l_sav_return_ = new QLabel(tr("Exp. Return:"));
    l_sav_return_->setStyleSheet(label_style);
    form->addRow(l_sav_return_, sav_return_);

    sav_years_ = new QDoubleSpinBox;
    sav_years_->setRange(1, 60);
    sav_years_->setValue(20);
    sav_years_->setDecimals(0);
    style_spin(sav_years_);
    l_sav_years_ = new QLabel(tr("Years:"));
    l_sav_years_->setStyleSheet(label_style);
    form->addRow(l_sav_years_, sav_years_);

    input_layout->addLayout(form);

    savings_calc_btn_ = new QPushButton(tr("CALCULATE"));
    savings_calc_btn_->setFixedHeight(28);
    savings_calc_btn_->setCursor(Qt::PointingHandCursor);
    savings_calc_btn_->setStyleSheet(
        QString("QPushButton { background:%1; color:#000; border:none; font-size:10px; font-weight:700; }"
                "QPushButton:hover { background:%2; }")
            .arg(ui::colors::AMBER(), ui::colors::WARNING()));
    connect(savings_calc_btn_, &QPushButton::clicked, this, &PlanningView::recalculate_savings);
    input_layout->addWidget(savings_calc_btn_);
    input_layout->addStretch();

    layout->addWidget(input_panel);

    // ── Right: comparison table ─────────────────────────────────────────────────
    auto* results = new QWidget(this);
    auto* res_layout = new QVBoxLayout(results);
    res_layout->setContentsMargins(0, 0, 0, 0);
    res_layout->setSpacing(12);

    savings_res_title_ = new QLabel(tr("WEALTH BY SAVINGS RATE"));
    savings_res_title_->setStyleSheet(
        QString("color:%1; font-size:12px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER()));
    res_layout->addWidget(savings_res_title_);

    savings_table_ = new QTableWidget;
    savings_table_->setColumnCount(4);
    savings_table_->setHorizontalHeaderLabels(
        {tr("SAVINGS RATE"), tr("ANNUAL SAVED"), tr("PROJECTED WEALTH"), tr("VS CURRENT RATE")});
    savings_table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    savings_table_->verticalHeader()->setVisible(false);
    savings_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    savings_table_->setSelectionMode(QAbstractItemView::NoSelection);
    savings_table_->setStyleSheet(
        QString("QTableWidget { background:%1; color:%2; border:1px solid %3; gridline-color:%3; font-size:11px; }"
                "QHeaderView::section { background:%4; color:%5; border:0; padding:6px; font-size:9px;"
                "  font-weight:700; letter-spacing:0.5px; }")
            .arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(), ui::colors::BG_SURFACE(),
                 ui::colors::TEXT_SECONDARY()));
    res_layout->addWidget(savings_table_, 1);

    savings_status_label_ = new QLabel;
    savings_status_label_->setWordWrap(true);
    savings_status_label_->setStyleSheet(
        QString("color:%1; font-size:12px; padding:8px;").arg(ui::colors::TEXT_SECONDARY()));
    res_layout->addWidget(savings_status_label_);

    layout->addWidget(results, 1);
    return w;
}

void PlanningView::recalculate_goals() {
    if (!goal_target_)
        return;
    const double target = goal_target_->value();
    const int years = static_cast<int>(goal_years_->value());
    const double ret = goal_return_->value() / 100.0;
    const double monthly = goal_monthly_->value();
    const double alloc_pct = goal_alloc_pct_->value() / 100.0;

    // Starting capital = the slice of the current portfolio earmarked for this goal.
    const double start_capital = summary_.total_market_value * alloc_pct;

    const double monthly_rate = ret / 12.0;
    const int months = years * 12;
    const double growth = std::pow(1.0 + monthly_rate, months);
    const double fv_start = start_capital * growth;
    const double annuity_factor = (monthly_rate > 1e-9) ? ((growth - 1.0) / monthly_rate) : months;
    const double fv_contrib = monthly * annuity_factor;
    const double projected = fv_start + fv_contrib;

    // Required monthly contribution to exactly hit the target from start_capital.
    const double remaining = std::max(0.0, target - fv_start);
    const double required_monthly = (annuity_factor > 1e-9) ? (remaining / annuity_factor) : 0.0;

    const double progress = (target > 1e-6) ? (projected / target * 100.0) : 0.0;

    goal_target_value_label_->setText(QString("%1 %2").arg(currency_, QString::number(target, 'f', 0)));
    goal_projected_label_->setText(QString("%1 %2").arg(currency_, QString::number(projected, 'f', 0)));
    goal_required_label_->setText(QString("%1 %2").arg(currency_, QString::number(required_monthly, 'f', 0)));
    goal_progress_label_->setText(QString("%1%").arg(QString::number(std::min(progress, 999.0), 'f', 0)));

    const bool on_track = projected >= target;
    const char* col = on_track ? ui::colors::POSITIVE : ui::colors::WARNING;
    goal_progress_label_->setStyleSheet(QString("color:%1; font-size:18px; font-weight:700;").arg(col));

    const QString name = goal_name_->text().trimmed().isEmpty() ? tr("this goal") : goal_name_->text().trimmed();
    if (on_track) {
        goal_status_label_->setText(tr("✓ On track for %1. Saving %2 %3/month reaches %2 %4 in %5 years — "
                                       "%6% of the %2 %7 target.")
                                        .arg(name)
                                        .arg(currency_)
                                        .arg(QString::number(monthly, 'f', 0))
                                        .arg(QString::number(projected, 'f', 0))
                                        .arg(years)
                                        .arg(QString::number(progress, 'f', 0))
                                        .arg(QString::number(target, 'f', 0)));
        goal_status_label_->setStyleSheet(QString("color:%1; font-size:12px; padding:12px;").arg(ui::colors::POSITIVE()));
    } else {
        goal_status_label_->setText(tr("⚠ Short of %1. At %2 %3/month you reach %2 %4 (%5%). "
                                       "Save %2 %6/month to hit the %2 %7 target in %8 years.")
                                        .arg(name)
                                        .arg(currency_)
                                        .arg(QString::number(monthly, 'f', 0))
                                        .arg(QString::number(projected, 'f', 0))
                                        .arg(QString::number(progress, 'f', 0))
                                        .arg(QString::number(required_monthly, 'f', 0))
                                        .arg(QString::number(target, 'f', 0))
                                        .arg(years));
        goal_status_label_->setStyleSheet(QString("color:%1; font-size:12px; padding:12px;").arg(ui::colors::WARNING()));
    }

    // Monte Carlo probability of hitting this goal, calibrated to portfolio vol.
    if (goal_mc_label_) {
        const double vol = have_history_ ? hist_vol_ : 0.15;
        const McResult mc = monte_carlo(start_capital, monthly, years, ret, vol, target);
        if (mc.valid) {
            const char* pc = mc.success_prob >= 0.75   ? ui::colors::POSITIVE
                             : mc.success_prob >= 0.5   ? ui::colors::WARNING
                                                        : ui::colors::NEGATIVE;
            goal_mc_label_->setText(tr("Monte Carlo: <b><span style='color:%1'>%2%% chance</span></b> of reaching "
                                       "%3 %4 — likely %3 %5 – %3 %6.")
                                        .arg(pc)
                                        .arg(QString::number(mc.success_prob * 100, 'f', 0))
                                        .arg(currency_)
                                        .arg(QString::number(target, 'f', 0))
                                        .arg(QString::number(mc.p5, 'f', 0))
                                        .arg(QString::number(mc.p95, 'f', 0)));
        }
    }

    // Keep the saved-goals funding column in sync with the current return input.
    refresh_goals_list();
}

void PlanningView::recalculate_savings() {
    if (!savings_table_)
        return;
    const double income = sav_income_->value();
    const double base_rate = sav_rate_->value();
    const double ret = sav_return_->value() / 100.0;
    const int years = static_cast<int>(sav_years_->value());
    const double start_capital = summary_.total_market_value;

    // Compare the user's rate against a band of nearby rates.
    QVector<double> rates;
    for (double delta : {-10.0, -5.0, 0.0, 5.0, 10.0}) {
        const double r = base_rate + delta;
        if (r >= 0.0 && r <= 100.0)
            rates.append(r);
    }
    std::sort(rates.begin(), rates.end());
    rates.erase(std::unique(rates.begin(), rates.end()), rates.end());

    auto project = [&](double rate_pct) {
        const double annual_saved = income * rate_pct / 100.0;
        const double growth = std::pow(1.0 + ret, years);
        const double fv_start = start_capital * growth;
        const double fv_saved = (ret > 1e-9) ? annual_saved * ((growth - 1.0) / ret) : annual_saved * years;
        return std::make_pair(annual_saved, fv_start + fv_saved);
    };

    const double base_wealth = project(base_rate).second;

    savings_table_->setRowCount(rates.size());
    for (int i = 0; i < rates.size(); ++i) {
        const double rate = rates[i];
        const auto [annual_saved, wealth] = project(rate);
        const bool is_base = std::abs(rate - base_rate) < 0.01;
        const double diff = wealth - base_wealth;

        auto make_item = [&](const QString& text, const char* color = nullptr) {
            auto* it = new QTableWidgetItem(text);
            it->setTextAlignment(Qt::AlignCenter);
            if (color)
                it->setForeground(QColor(QString(color)));
            if (is_base)
                it->setBackground(QColor(ui::colors::BG_SURFACE()));
            return it;
        };

        savings_table_->setItem(i, 0, make_item(QString("%1%%2").arg(QString::number(rate, 'f', 0),
                                                                     is_base ? tr("  (current)") : QString())));
        savings_table_->setItem(i, 1, make_item(QString("%1 %2").arg(currency_, QString::number(annual_saved, 'f', 0))));
        savings_table_->setItem(i, 2, make_item(QString("%1 %2").arg(currency_, QString::number(wealth, 'f', 0))));
        const char* dcol = diff > 0 ? ui::colors::POSITIVE : diff < 0 ? ui::colors::NEGATIVE : ui::colors::TEXT_TERTIARY();
        savings_table_->setItem(
            i, 3,
            make_item(is_base ? QStringLiteral("—")
                              : QString("%1%2 %3").arg(diff >= 0 ? "+" : "-", currency_,
                                                       QString::number(std::abs(diff), 'f', 0)),
                      dcol));
    }

    const auto [base_annual, base_proj] = project(base_rate);
    savings_status_label_->setText(tr("At a %1% savings rate you set aside %2 %3/year and project %2 %4 "
                                      "after %5 years (%6% annual return, starting from %2 %7).")
                                       .arg(QString::number(base_rate, 'f', 0))
                                       .arg(currency_)
                                       .arg(QString::number(base_annual, 'f', 0))
                                       .arg(QString::number(base_proj, 'f', 0))
                                       .arg(years)
                                       .arg(QString::number(sav_return_->value(), 'f', 1))
                                       .arg(QString::number(start_capital, 'f', 0)));
}

void PlanningView::set_data(const portfolio::PortfolioSummary& summary, const QString& currency) {
    summary_ = summary;
    currency_ = currency;
    has_data_ = true;

    // Money inputs must reflect the portfolio's own currency, not a hardcoded
    // "$". Result labels in this view already use currency_ (e.g. "INR 60000"),
    // so prefix the spin boxes with the same currency code to stay consistent.
    const QString prefix = (currency_.isEmpty() ? QStringLiteral("USD") : currency_) + " ";
    for (QDoubleSpinBox* sb : {annual_expense_, monthly_contrib_, goal_target_, goal_monthly_, sav_income_}) {
        if (sb)
            sb->setPrefix(prefix);
    }

    load_goals();
    recalculate();
    recalculate_goals();
    recalculate_savings();
}

void PlanningView::set_snapshots(const QVector<portfolio::PortfolioSnapshot>& snapshots) {
    snapshots_ = snapshots;
    recompute_assumptions();
    if (has_data_) {
        recalculate();
        recalculate_goals();
        recalculate_savings();
    }
}

void PlanningView::set_metrics(const portfolio::ComputedMetrics& metrics) {
    metrics_ = metrics;
    if (has_data_) {
        recalculate();
        recalculate_goals();
    }
}

void PlanningView::recompute_assumptions() {
    have_history_ = false;
    hist_cagr_ = 0.0;
    hist_vol_ = 0.0;

    auto snaps = snapshots_;
    std::sort(snaps.begin(), snaps.end(),
              [](const auto& a, const auto& b) { return a.snapshot_date < b.snapshot_date; });
    QVector<double> rets;
    for (int i = 1; i < snaps.size(); ++i) {
        const double p = snaps[i - 1].total_value;
        const double c = snaps[i].total_value;
        if (p > 1.0)
            rets.append((c - p) / p);
    }
    if (rets.size() >= 20) {
        const int n = rets.size();
        const double mean = std::accumulate(rets.begin(), rets.end(), 0.0) / n;
        double var = 0.0;
        for (double r : rets)
            var += (r - mean) * (r - mean);
        const double daily_vol = std::sqrt(var / (n - 1));
        hist_cagr_ = std::pow(1.0 + mean, 252.0) - 1.0; // annualise daily mean
        hist_vol_ = daily_vol * std::sqrt(252.0);
        // Clamp to sane planning bounds (a 30-day sample can be extreme).
        hist_cagr_ = std::max(-0.5, std::min(0.35, hist_cagr_));
        hist_vol_ = std::max(0.01, std::min(1.0, hist_vol_));
        have_history_ = true;
    }

    const QString note =
        have_history_
            ? tr("◉ Calibrated to your portfolio: %1% expected return, %2% volatility "
                 "(from %3 days of history). Edit any field to override.")
                  .arg(QString::number(hist_cagr_ * 100, 'f', 1))
                  .arg(QString::number(hist_vol_ * 100, 'f', 1))
                  .arg(rets.size())
            : tr("○ Not enough portfolio history yet to calibrate — using your typed assumptions. "
                 "Estimates improve as daily snapshots accumulate.");
    if (retire_assump_note_)
        retire_assump_note_->setText(note);
    if (sav_assump_note_)
        sav_assump_note_->setText(note);

    // Seed the return inputs from the real CAGR (user can still override).
    if (have_history_) {
        const double pct = hist_cagr_ * 100.0;
        if (expected_return_)
            expected_return_->setValue(pct);
        if (goal_return_)
            goal_return_->setValue(pct);
        if (sav_return_)
            sav_return_->setValue(pct);
    }
}

PlanningView::McResult PlanningView::monte_carlo(double start, double monthly, int years, double mean_annual,
                                                double vol_annual, double target) const {
    McResult r;
    if (years <= 0)
        return r;
    constexpr int kSims = 2000;
    std::mt19937 gen(12345u); // fixed seed → stable, reproducible projections
    std::normal_distribution<double> nd(mean_annual, std::max(vol_annual, 1e-6));
    const double annual_contrib = monthly * 12.0;
    QVector<double> ends;
    ends.reserve(kSims);
    int success = 0;
    for (int s = 0; s < kSims; ++s) {
        double bal = start;
        for (int y = 0; y < years; ++y) {
            bal = bal * (1.0 + nd(gen)) + annual_contrib;
            if (bal < 0.0)
                bal = 0.0;
        }
        ends.append(bal);
        if (bal >= target)
            ++success;
    }
    std::sort(ends.begin(), ends.end());
    auto pct = [&](double p) { return ends[std::clamp(static_cast<int>(p * kSims), 0, kSims - 1)]; };
    r.p5 = pct(0.05);
    r.p50 = pct(0.50);
    r.p95 = pct(0.95);
    r.success_prob = static_cast<double>(success) / kSims;
    r.valid = true;
    return r;
}

// ── Goals persistence (per-portfolio) ─────────────────────────────────────────

void PlanningView::load_goals() {
    goals_.clear();
    const QString pid = summary_.portfolio.id;
    if (!pid.isEmpty()) {
        auto res = SettingsRepository::instance().get(QString("pi.goals.%1").arg(pid));
        if (res.is_ok() && !res.value().isEmpty()) {
            const auto doc = QJsonDocument::fromJson(res.value().toUtf8());
            for (const auto v : doc.array()) {
                const auto o = v.toObject();
                Goal g;
                g.name = o["name"].toString();
                g.target = o["target"].toDouble();
                g.years = o["years"].toInt();
                g.monthly = o["monthly"].toDouble();
                g.alloc_pct = o["alloc"].toDouble();
                goals_.append(g);
            }
        }
    }
    refresh_goals_list();
}

void PlanningView::save_goals() {
    const QString pid = summary_.portfolio.id;
    if (pid.isEmpty())
        return;
    QJsonArray arr;
    for (const auto& g : goals_) {
        QJsonObject o;
        o["name"] = g.name;
        o["target"] = g.target;
        o["years"] = g.years;
        o["monthly"] = g.monthly;
        o["alloc"] = g.alloc_pct;
        arr.append(o);
    }
    SettingsRepository::instance().set(QString("pi.goals.%1").arg(pid),
                                       QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)),
                                       "portfolio_insights");
}

void PlanningView::refresh_goals_list() {
    if (!goals_list_)
        return;
    const double mean = (expected_return_ ? expected_return_->value() : 8.0) / 100.0;
    goals_list_->setRowCount(goals_.size());
    for (int i = 0; i < goals_.size(); ++i) {
        const Goal& g = goals_[i];
        const double start = summary_.total_market_value * g.alloc_pct / 100.0;
        const double growth = std::pow(1.0 + mean / 12.0, g.years * 12);
        const double annuity = (mean > 1e-9) ? ((growth - 1.0) / (mean / 12.0)) : g.years * 12;
        const double projected = start * growth + g.monthly * annuity;
        const double funded_now = (g.target > 1e-6) ? (start / g.target * 100.0) : 0.0;
        const double proj_pct = (g.target > 1e-6) ? (projected / g.target * 100.0) : 0.0;
        const bool on_track = projected >= g.target;

        auto item = [&](const QString& t, const char* color = nullptr, int align = Qt::AlignLeft) {
            auto* it = new QTableWidgetItem(t);
            it->setTextAlignment(static_cast<Qt::Alignment>(align) | Qt::AlignVCenter);
            if (color)
                it->setForeground(QColor(QString(color)));
            return it;
        };
        goals_list_->setItem(i, 0, item(g.name.isEmpty() ? tr("(unnamed)") : g.name, ui::colors::TEXT_PRIMARY));
        goals_list_->setItem(i, 1, item(QString("%1 %2").arg(currency_, QString::number(g.target, 'f', 0)),
                                        ui::colors::TEXT_SECONDARY, Qt::AlignRight));
        goals_list_->setItem(i, 2, item(QString("%1%").arg(QString::number(funded_now, 'f', 0)),
                                        ui::colors::CYAN, Qt::AlignRight));
        goals_list_->setItem(i, 3,
                             item(QString("%1%").arg(QString::number(std::min(proj_pct, 999.0), 'f', 0)),
                                  on_track ? ui::colors::POSITIVE : ui::colors::WARNING, Qt::AlignRight));
    }
}

void PlanningView::add_or_update_goal() {
    if (!goal_name_)
        return;
    Goal g;
    g.name = goal_name_->text().trimmed();
    g.target = goal_target_ ? goal_target_->value() : 0;
    g.years = goal_years_ ? static_cast<int>(goal_years_->value()) : 0;
    g.monthly = goal_monthly_ ? goal_monthly_->value() : 0;
    g.alloc_pct = goal_alloc_pct_ ? goal_alloc_pct_->value() : 0;
    if (g.name.isEmpty() || g.target <= 0)
        return;
    // Update existing goal with the same name, else append.
    bool updated = false;
    for (auto& existing : goals_) {
        if (existing.name.compare(g.name, Qt::CaseInsensitive) == 0) {
            existing = g;
            updated = true;
            break;
        }
    }
    if (!updated)
        goals_.append(g);
    save_goals();
    refresh_goals_list();
    recalculate_goals();
}

void PlanningView::remove_selected_goal() {
    if (!goals_list_)
        return;
    const int row = goals_list_->currentRow();
    if (row < 0 || row >= goals_.size())
        return;
    goals_.removeAt(row);
    save_goals();
    refresh_goals_list();
}

void PlanningView::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void PlanningView::retranslateUi() {
    if (tabs_) {
        if (retirement_tab_index_ >= 0) tabs_->setTabText(retirement_tab_index_, tr("RETIREMENT"));
        if (goals_tab_index_ >= 0)      tabs_->setTabText(goals_tab_index_, tr("GOALS"));
        if (savings_tab_index_ >= 0)    tabs_->setTabText(savings_tab_index_, tr("SAVINGS"));
    }
    if (input_title_)        input_title_->setText(tr("RETIREMENT CALCULATOR"));
    if (l_current_age_)      l_current_age_->setText(tr("Current Age:"));
    if (l_retire_age_)       l_retire_age_->setText(tr("Retire Age:"));
    if (l_annual_expense_)   l_annual_expense_->setText(tr("Annual Expense:"));
    if (l_monthly_contrib_)  l_monthly_contrib_->setText(tr("Monthly Savings:"));
    if (l_expected_return_)  l_expected_return_->setText(tr("Exp. Return:"));
    if (l_inflation_)        l_inflation_->setText(tr("Inflation:"));
    if (l_withdrawal_rate_)  l_withdrawal_rate_->setText(tr("Withdrawal Rate:"));
    if (calc_btn_)           calc_btn_->setText(tr("CALCULATE"));
    if (res_title_)          res_title_->setText(tr("PROJECTION RESULTS"));
    if (years_card_label_)     years_card_label_->setText(tr("YEARS TO RETIREMENT"));
    if (target_card_label_)    target_card_label_->setText(tr("TARGET NEST EGG"));
    if (projected_card_label_) projected_card_label_->setText(tr("PROJECTED VALUE"));
    if (gap_card_label_)       gap_card_label_->setText(tr("SURPLUS / GAP"));
    if (goals_title_)   goals_title_->setText(tr("GOAL-BASED PLANNING"));
    if (l_goal_name_)    l_goal_name_->setText(tr("Goal:"));
    if (l_goal_target_)  l_goal_target_->setText(tr("Target Amount:"));
    if (l_goal_years_)   l_goal_years_->setText(tr("Years to Goal:"));
    if (l_goal_return_)  l_goal_return_->setText(tr("Exp. Return:"));
    if (l_goal_monthly_) l_goal_monthly_->setText(tr("Monthly Saving:"));
    if (l_goal_alloc_)   l_goal_alloc_->setText(tr("% Portfolio Allocated:"));
    if (goals_calc_btn_) goals_calc_btn_->setText(tr("CALCULATE"));
    if (goals_res_title_) goals_res_title_->setText(tr("GOAL PROJECTION"));
    if (goal_target_card_label_)    goal_target_card_label_->setText(tr("TARGET AMOUNT"));
    if (goal_projected_card_label_) goal_projected_card_label_->setText(tr("PROJECTED VALUE"));
    if (goal_required_card_label_)  goal_required_card_label_->setText(tr("REQUIRED MONTHLY"));
    if (goal_progress_card_label_)  goal_progress_card_label_->setText(tr("PROGRESS"));

    if (savings_title_) savings_title_->setText(tr("SAVINGS RATE ANALYSIS"));
    if (savings_desc_)  savings_desc_->setText(tr("Projects wealth accumulation across a range of savings rates, "
                                                  "starting from your current portfolio value."));
    if (l_sav_income_)  l_sav_income_->setText(tr("Annual Income:"));
    if (l_sav_rate_)    l_sav_rate_->setText(tr("Savings Rate:"));
    if (l_sav_return_)  l_sav_return_->setText(tr("Exp. Return:"));
    if (l_sav_years_)   l_sav_years_->setText(tr("Years:"));
    if (savings_calc_btn_) savings_calc_btn_->setText(tr("CALCULATE"));
    if (savings_res_title_) savings_res_title_->setText(tr("WEALTH BY SAVINGS RATE"));
    if (savings_table_)
        savings_table_->setHorizontalHeaderLabels(
            {tr("SAVINGS RATE"), tr("ANNUAL SAVED"), tr("PROJECTED WEALTH"), tr("VS CURRENT RATE")});

    // re-run so status_label_ tr() strings pick up new locale
    if (has_data_) {
        recalculate();
        recalculate_goals();
        recalculate_savings();
    }
}

void PlanningView::recalculate() {
    double current_value = summary_.total_market_value;
    double age = current_age_->value();
    double retire = retire_age_->value();
    double years = retire - age;
    double annual_exp = annual_expense_->value();
    double monthly = monthly_contrib_->value();
    double ret = expected_return_->value() / 100.0;
    double inf = inflation_->value() / 100.0;
    double real_ret = ret - inf;

    // Withdrawal rule target: corpus = annual_expense / withdrawal_rate
    double wr = withdrawal_rate_->value() / 100.0;
    double target = (wr > 1e-6) ? (annual_exp / wr) : annual_exp * 25.0;

    // Future value: current portfolio + monthly contributions
    // FV = PV * (1+r)^n + PMT * [((1+r)^n - 1) / r]
    double monthly_rate = real_ret / 12.0;
    int months = static_cast<int>(years * 12);
    double fv_portfolio = current_value * std::pow(1.0 + monthly_rate, months);
    double fv_contrib = (monthly_rate > 0.0001)
                            ? monthly * ((std::pow(1.0 + monthly_rate, months) - 1.0) / monthly_rate)
                            : monthly * months;
    double projected = fv_portfolio + fv_contrib;
    double gap = projected - target;

    years_label_->setText(QString::number(years, 'f', 0));
    target_label_->setText(QString("%1 %2").arg(currency_, QString::number(target, 'f', 0)));
    projected_label_->setText(QString("%1 %2").arg(currency_, QString::number(projected, 'f', 0)));

    const char* gap_color = gap >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE;
    gap_label_->setText(
        QString("%1%2 %3").arg(gap >= 0 ? "+" : "").arg(currency_).arg(QString::number(std::abs(gap), 'f', 0)));
    gap_label_->setStyleSheet(QString("color:%1; font-size:18px; font-weight:700;").arg(gap_color));

    if (gap >= 0) {
        status_label_->setText(tr("\u2713 On track! Your projected retirement fund of %1 %2 "
                                  "exceeds your target of %1 %3 by %1 %4.")
                                   .arg(currency_)
                                   .arg(QString::number(projected, 'f', 0))
                                   .arg(QString::number(target, 'f', 0))
                                   .arg(QString::number(gap, 'f', 0)));
        status_label_->setStyleSheet(QString("color:%1; font-size:12px; padding:12px;").arg(ui::colors::POSITIVE()));
    } else {
        double needed_monthly = (-gap) / ((std::pow(1.0 + monthly_rate, months) - 1.0) / monthly_rate);
        status_label_->setText(tr("\u26A0 Shortfall of %1 %2. Consider increasing monthly savings "
                                  "by %1 %3 to close the gap.")
                                   .arg(currency_)
                                   .arg(QString::number(std::abs(gap), 'f', 0))
                                   .arg(QString::number(needed_monthly, 'f', 0)));
        status_label_->setStyleSheet(QString("color:%1; font-size:12px; padding:12px;").arg(ui::colors::WARNING()));
    }

    // \u2500\u2500 Monte Carlo (probabilistic, calibrated to portfolio vol) \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500
    if (retire_mc_label_) {
        const double vol = have_history_ ? hist_vol_ : 0.15; // default 15% if uncalibrated
        const McResult mc = monte_carlo(current_value, monthly, static_cast<int>(years), real_ret, vol, target);
        if (mc.valid) {
            const char* pc = mc.success_prob >= 0.75   ? ui::colors::POSITIVE
                             : mc.success_prob >= 0.5   ? ui::colors::WARNING
                                                        : ui::colors::NEGATIVE;
            retire_mc_label_->setText(
                tr("Monte Carlo (2000 runs, %1% vol): <b><span style='color:%2'>%3%% chance</span></b> "
                   "of reaching your target. Likely range %4 %5 \u2013 %6 %7 (median %4 %8).")
                    .arg(QString::number(vol * 100, 'f', 0))
                    .arg(pc)
                    .arg(QString::number(mc.success_prob * 100, 'f', 0))
                    .arg(currency_)
                    .arg(QString::number(mc.p5, 'f', 0))
                    .arg(currency_)
                    .arg(QString::number(mc.p95, 'f', 0))
                    .arg(QString::number(mc.p50, 'f', 0)));
            retire_mc_label_->setTextFormat(Qt::RichText);
        }
    }

    // \u2500\u2500 Drawdown stress (real metric) \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500
    if (retire_stress_label_) {
        const double dd_pct = metrics_.max_drawdown ? std::abs(*metrics_.max_drawdown) : 30.0;
        const double shocked = projected * (1.0 - dd_pct / 100.0);
        const double shocked_funded = (target > 1e-6) ? (shocked / target * 100.0) : 0.0;
        retire_stress_label_->setText(
            tr("Stress: a %1%% drawdown (your historical max) near retirement would cut the projection to "
               "%2 %3 \u2014 %4%% of target.")
                .arg(QString::number(dd_pct, 'f', 0))
                .arg(currency_)
                .arg(QString::number(shocked, 'f', 0))
                .arg(QString::number(shocked_funded, 'f', 0)));
    }
}

} // namespace fincept::screens
