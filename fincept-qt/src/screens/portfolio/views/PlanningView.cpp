// src/screens/portfolio/views/PlanningView.cpp
#include "screens/portfolio/views/PlanningView.h"

#include "ui/theme/Theme.h"

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
                             .arg(ui::colors::BG_BASE, ui::colors::BG_SURFACE, ui::colors::TEXT_SECONDARY,
                                  ui::colors::AMBER, ui::colors::TEXT_PRIMARY));

    tabs_->addTab(build_retirement_tab(), "RETIREMENT");
    tabs_->addTab(build_goals_tab(), "GOALS");
    tabs_->addTab(build_savings_tab(), "SAVINGS");

    layout->addWidget(tabs_);
}

QWidget* PlanningView::build_retirement_tab() {
    auto* w = new QWidget;
    auto* layout = new QHBoxLayout(w);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(24);

    // Left: inputs
    auto* input_panel = new QWidget;
    input_panel->setFixedWidth(320);
    input_panel->setStyleSheet(
        QString("background:%1; border:1px solid %2;").arg(ui::colors::BG_RAISED, ui::colors::BORDER_DIM));
    auto* input_layout = new QVBoxLayout(input_panel);
    input_layout->setContentsMargins(16, 12, 16, 12);
    input_layout->setSpacing(8);

    auto* input_title = new QLabel("RETIREMENT CALCULATOR");
    input_title->setStyleSheet(
        QString("color:%1; font-size:11px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER));
    input_layout->addWidget(input_title);

    auto* form = new QFormLayout;
    form->setSpacing(8);
    form->setLabelAlignment(Qt::AlignRight);

    auto style_spin = [](QDoubleSpinBox* sb) {
        sb->setFixedHeight(26);
        sb->setStyleSheet(
            QString("QDoubleSpinBox { background:%1; color:%2; border:1px solid %3;"
                    "  padding:0 8px; font-size:11px; }"
                    "QDoubleSpinBox:focus { border-color:%4; }")
                .arg(ui::colors::BG_BASE, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_DIM, ui::colors::AMBER));
    };
    auto label_style = QString("color:%1; font-size:10px;").arg(ui::colors::TEXT_SECONDARY);

    current_age_ = new QDoubleSpinBox;
    current_age_->setRange(18, 100);
    current_age_->setValue(30);
    current_age_->setDecimals(0);
    style_spin(current_age_);
    auto* l1 = new QLabel("Current Age:");
    l1->setStyleSheet(label_style);
    form->addRow(l1, current_age_);

    retire_age_ = new QDoubleSpinBox;
    retire_age_->setRange(30, 100);
    retire_age_->setValue(65);
    retire_age_->setDecimals(0);
    style_spin(retire_age_);
    auto* l2 = new QLabel("Retire Age:");
    l2->setStyleSheet(label_style);
    form->addRow(l2, retire_age_);

    annual_expense_ = new QDoubleSpinBox;
    annual_expense_->setRange(0, 10000000);
    annual_expense_->setValue(60000);
    annual_expense_->setPrefix("$ ");
    annual_expense_->setDecimals(0);
    style_spin(annual_expense_);
    auto* l3 = new QLabel("Annual Expense:");
    l3->setStyleSheet(label_style);
    form->addRow(l3, annual_expense_);

    monthly_contrib_ = new QDoubleSpinBox;
    monthly_contrib_->setRange(0, 1000000);
    monthly_contrib_->setValue(2000);
    monthly_contrib_->setPrefix("$ ");
    monthly_contrib_->setDecimals(0);
    style_spin(monthly_contrib_);
    auto* l4 = new QLabel("Monthly Savings:");
    l4->setStyleSheet(label_style);
    form->addRow(l4, monthly_contrib_);

    expected_return_ = new QDoubleSpinBox;
    expected_return_->setRange(0, 30);
    expected_return_->setValue(8);
    expected_return_->setSuffix(" %");
    expected_return_->setDecimals(1);
    style_spin(expected_return_);
    auto* l5 = new QLabel("Exp. Return:");
    l5->setStyleSheet(label_style);
    form->addRow(l5, expected_return_);

    inflation_ = new QDoubleSpinBox;
    inflation_->setRange(0, 20);
    inflation_->setValue(3);
    inflation_->setSuffix(" %");
    inflation_->setDecimals(1);
    style_spin(inflation_);
    auto* l6 = new QLabel("Inflation:");
    l6->setStyleSheet(label_style);
    form->addRow(l6, inflation_);

    input_layout->addLayout(form);

    auto* calc_btn = new QPushButton("CALCULATE");
    calc_btn->setFixedHeight(28);
    calc_btn->setCursor(Qt::PointingHandCursor);
    calc_btn->setStyleSheet(
        QString("QPushButton { background:%1; color:#000; border:none; font-size:10px; font-weight:700; }"
                "QPushButton:hover { background:%2; }")
            .arg(ui::colors::AMBER, ui::colors::WARNING));
    connect(calc_btn, &QPushButton::clicked, this, &PlanningView::recalculate);
    input_layout->addWidget(calc_btn);
    input_layout->addStretch();

    layout->addWidget(input_panel);

    // Right: results
    auto* results = new QWidget;
    auto* res_layout = new QVBoxLayout(results);
    res_layout->setContentsMargins(0, 0, 0, 0);
    res_layout->setSpacing(12);

    auto* res_title = new QLabel("PROJECTION RESULTS");
    res_title->setStyleSheet(
        QString("color:%1; font-size:12px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER));
    res_layout->addWidget(res_title);

    auto* grid = new QGridLayout;
    grid->setSpacing(8);

    auto add_card = [&](int r, int c, const QString& label, QLabel*& val_label, const char* color) {
        auto* card = new QWidget;
        card->setStyleSheet(
            QString("background:%1; border:1px solid %2;").arg(ui::colors::BG_RAISED, ui::colors::BORDER_DIM));
        auto* cl = new QVBoxLayout(card);
        cl->setContentsMargins(12, 8, 12, 8);
        cl->setSpacing(2);

        auto* lbl = new QLabel(label);
        lbl->setStyleSheet(
            QString("color:%1; font-size:8px; font-weight:700; letter-spacing:0.5px;").arg(ui::colors::TEXT_TERTIARY));
        cl->addWidget(lbl);

        val_label = new QLabel("--");
        val_label->setStyleSheet(QString("color:%1; font-size:18px; font-weight:700;").arg(color));
        cl->addWidget(val_label);

        grid->addWidget(card, r, c);
    };

    add_card(0, 0, "YEARS TO RETIREMENT", years_label_, ui::colors::CYAN);
    add_card(0, 1, "TARGET NEST EGG", target_label_, ui::colors::WARNING);
    add_card(1, 0, "PROJECTED VALUE", projected_label_, ui::colors::POSITIVE);
    add_card(1, 1, "SURPLUS / GAP", gap_label_, ui::colors::TEXT_PRIMARY);

    res_layout->addLayout(grid);

    status_label_ = new QLabel;
    status_label_->setWordWrap(true);
    status_label_->setStyleSheet(QString("color:%1; font-size:12px; padding:12px;").arg(ui::colors::TEXT_SECONDARY));
    res_layout->addWidget(status_label_);

    res_layout->addStretch();
    layout->addWidget(results, 1);

    return w;
}

QWidget* PlanningView::build_goals_tab() {
    auto* w = new QWidget;
    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setAlignment(Qt::AlignCenter);

    auto* title = new QLabel("GOAL-BASED PLANNING");
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(
        QString("color:%1; font-size:12px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER));
    layout->addWidget(title);

    auto* desc = new QLabel("Define financial goals (house, education, emergency fund)\n"
                            "and track your progress toward each target.");
    desc->setAlignment(Qt::AlignCenter);
    desc->setWordWrap(true);
    desc->setStyleSheet(QString("color:%1; font-size:11px;").arg(ui::colors::TEXT_TERTIARY));
    layout->addWidget(desc);

    return w;
}

QWidget* PlanningView::build_savings_tab() {
    auto* w = new QWidget;
    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setAlignment(Qt::AlignCenter);

    auto* title = new QLabel("SAVINGS RATE ANALYSIS");
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(
        QString("color:%1; font-size:12px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER));
    layout->addWidget(title);

    auto* desc = new QLabel("Analyze how different savings rates and contribution schedules\n"
                            "affect your long-term wealth accumulation.");
    desc->setAlignment(Qt::AlignCenter);
    desc->setWordWrap(true);
    desc->setStyleSheet(QString("color:%1; font-size:11px;").arg(ui::colors::TEXT_TERTIARY));
    layout->addWidget(desc);

    return w;
}

void PlanningView::set_data(const portfolio::PortfolioSummary& summary, const QString& currency) {
    summary_ = summary;
    currency_ = currency;
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

    // 4% rule target
    double target = annual_exp * 25.0;

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
        status_label_->setText(QString("\u2713 On track! Your projected retirement fund of %1 %2 "
                                       "exceeds your target of %1 %3 by %1 %4.")
                                   .arg(currency_)
                                   .arg(QString::number(projected, 'f', 0))
                                   .arg(QString::number(target, 'f', 0))
                                   .arg(QString::number(gap, 'f', 0)));
        status_label_->setStyleSheet(QString("color:%1; font-size:12px; padding:12px;").arg(ui::colors::POSITIVE));
    } else {
        double needed_monthly = (-gap) / ((std::pow(1.0 + monthly_rate, months) - 1.0) / monthly_rate);
        status_label_->setText(QString("\u26A0 Shortfall of %1 %2. Consider increasing monthly savings "
                                       "by %1 %3 to close the gap.")
                                   .arg(currency_)
                                   .arg(QString::number(std::abs(gap), 'f', 0))
                                   .arg(QString::number(needed_monthly, 'f', 0)));
        status_label_->setStyleSheet(QString("color:%1; font-size:12px; padding:12px;").arg(ui::colors::WARNING));
    }
}

} // namespace fincept::screens
