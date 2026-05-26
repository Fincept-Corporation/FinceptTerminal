// src/screens/portfolio/views/PlanningView.cpp
#include "screens/portfolio/views/PlanningView.h"

#include "ui/theme/Theme.h"

#include <QEvent>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include <cmath>

namespace fincept::screens {

PlanningView::PlanningView(QWidget* parent) : QWidget(parent) {
    build_ui();
}

void PlanningView::build_ui() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    tabs_ = new QTabWidget;
    tabs_->setDocumentMode(true);
    tabs_->setStyleSheet(QString("QTabWidget::pane { border:0; background:%1; }"
                                 "QTabBar::tab { background:%2; color:%3; padding:6px 14px; border:0;"
                                 "  border-bottom:2px solid transparent; font-size:9px; font-weight:700;"
                                 "  letter-spacing:0.5px; }"
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

    status_label_ = new QLabel;
    status_label_->setWordWrap(true);
    status_label_->setStyleSheet(QString("color:%1; font-size:12px; padding:12px;").arg(ui::colors::TEXT_SECONDARY()));
    res_layout->addWidget(status_label_);

    res_layout->addStretch();
    layout->addWidget(results, 1);

    return w;
}

QWidget* PlanningView::build_goals_tab() {
    auto* w = new QWidget(this);
    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setAlignment(Qt::AlignCenter);

    goals_title_ = new QLabel(tr("GOAL-BASED PLANNING"));
    goals_title_->setAlignment(Qt::AlignCenter);
    goals_title_->setStyleSheet(
        QString("color:%1; font-size:12px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER()));
    layout->addWidget(goals_title_);

    goals_desc_ = new QLabel(tr("Define financial goals (house, education, emergency fund)\n"
                                "and track your progress toward each target."));
    goals_desc_->setAlignment(Qt::AlignCenter);
    goals_desc_->setWordWrap(true);
    goals_desc_->setStyleSheet(QString("color:%1; font-size:11px;").arg(ui::colors::TEXT_TERTIARY()));
    layout->addWidget(goals_desc_);

    return w;
}

QWidget* PlanningView::build_savings_tab() {
    auto* w = new QWidget(this);
    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setAlignment(Qt::AlignCenter);

    savings_title_ = new QLabel(tr("SAVINGS RATE ANALYSIS"));
    savings_title_->setAlignment(Qt::AlignCenter);
    savings_title_->setStyleSheet(
        QString("color:%1; font-size:12px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER()));
    layout->addWidget(savings_title_);

    savings_desc_ = new QLabel(tr("Analyze how different savings rates and contribution schedules\n"
                                  "affect your long-term wealth accumulation."));
    savings_desc_->setAlignment(Qt::AlignCenter);
    savings_desc_->setWordWrap(true);
    savings_desc_->setStyleSheet(QString("color:%1; font-size:11px;").arg(ui::colors::TEXT_TERTIARY()));
    layout->addWidget(savings_desc_);

    return w;
}

void PlanningView::set_data(const portfolio::PortfolioSummary& summary, const QString& currency) {
    summary_ = summary;
    currency_ = currency;
    has_data_ = true;
    recalculate();
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
    if (goals_desc_)    goals_desc_->setText(tr("Define financial goals (house, education, emergency fund)\n"
                                                "and track your progress toward each target."));
    if (savings_title_) savings_title_->setText(tr("SAVINGS RATE ANALYSIS"));
    if (savings_desc_)  savings_desc_->setText(tr("Analyze how different savings rates and contribution schedules\n"
                                                  "affect your long-term wealth accumulation."));

    // re-run so status_label_ tr() strings pick up new locale
    if (has_data_)
        recalculate();
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
}

} // namespace fincept::screens
