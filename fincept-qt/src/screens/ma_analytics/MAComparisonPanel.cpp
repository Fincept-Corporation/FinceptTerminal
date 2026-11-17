// MAComparisonPanel.cpp — Deal Comparison module builder for MAModulePanel.
// One deal grid (MaDealGrid: typed columns, paste from a spreadsheet, CSV import, load verified deals from the Deal
// Database, labelled sample deals) above the five analyses; every analysis reads the same rows. It replaced five
// separate JSON text boxes.

#include "screens/ma_analytics/MAModulePanel.h"
#include "screens/ma_analytics/MaDealGrid.h"
#include "services/ma_analytics/MAAnalyticsService.h"
#include "ui/theme/Theme.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

using namespace fincept::services::ma;

// ═══════════════════════════════════════════════════════════════════════════════
// MODULE 8: DEAL COMPARISON
// ═══════════════════════════════════════════════════════════════════════════════

QWidget* MAModulePanel::build_comparison_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    auto* hint = new QLabel(tr("The deals every analysis below compares. Values in millions, premiums and payment mix "
                               "in percent. Opens with labelled sample deals - replace them, or load the deals you "
                               "verified in the Deal Database."),
                            w);
    hint->setWordWrap(true);
    hint->setStyleSheet(QString("color:%1; font-size:%2px;").arg(ui::colors::TEXT_SECONDARY()).arg(ui::fonts::px(-2)));
    vl->addWidget(hint);
    comparison_grid_ = new MaDealGrid(MaDealGrid::Kind::Comparison, QStringLiteral("cmp_deals_grid"), w);
    vl->addWidget(comparison_grid_);

    sub_tabs_ = new QTabWidget(w);
    apply_tab_stylesheet();

    // ── Compare ──
    add_lazy_sub_tab(QT_TR_NOOP("Compare"), [this](QWidget* cmp) {
        auto* cmp_vl = new QVBoxLayout(cmp);
        cmp_vl->setContentsMargins(12, 12, 12, 12);
        cmp_vl->setSpacing(8);
        auto* cmp_run = make_run_button(tr("COMPARE DEALS"), cmp);
        connect(cmp_run, &QPushButton::clicked, this, [this]() {
            QJsonArray deals;
            if (!grid_deals(comparison_grid_, &deals, false))
                return;
            status_label_->setText(tr("Comparing Deals..."));
            QJsonObject params;
            params["deals"] = deals;
            MAAnalyticsService::instance().compare_deals(params);
        });
        cmp_vl->addWidget(cmp_run);
        cmp_vl->addStretch();
    });

    // ── Rank ──
    add_lazy_sub_tab(QT_TR_NOOP("Rank"), [this](QWidget* rank) {
        auto* rank_vl = new QVBoxLayout(rank);
        rank_vl->setContentsMargins(12, 12, 12, 12);
        rank_vl->setSpacing(8);

        auto* rank_criteria = new QComboBox(rank);
        rank_criteria->addItems({"premium", "deal_value", "ev_revenue", "ev_ebitda", "synergies"});
        rank_criteria->setStyleSheet(QString("QComboBox { background:%1; color:%2; border:1px solid %3;"
                                             "font-family:%4; font-size:%5px; padding:4px 6px; }")
                                         .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED())
                                         .arg(ui::fonts::DATA_FAMILY)
                                         .arg(ui::fonts::SMALL.get()));
        combo_inputs_["rank_criteria"] = rank_criteria;
        rank_vl->addWidget(build_input_row(tr("Rank By"), rank_criteria, rank));

        auto* rank_run = make_run_button(tr("RANK DEALS"), rank);
        connect(rank_run, &QPushButton::clicked, this, [this]() {
            QJsonArray deals;
            if (!grid_deals(comparison_grid_, &deals, false))
                return;
            status_label_->setText(tr("Ranking Deals..."));
            QJsonObject params;
            params["deals"] = deals;
            params["criteria"] = combo_inputs_["rank_criteria"]->currentText();
            MAAnalyticsService::instance().rank_deals(params);
        });
        rank_vl->addWidget(rank_run);
        rank_vl->addStretch();
    });

    // ── Benchmark ──
    add_lazy_sub_tab(QT_TR_NOOP("Benchmark"), [this](QWidget* bench) {
        auto* bench_vl = new QVBoxLayout(bench);
        bench_vl->setContentsMargins(12, 12, 12, 12);
        bench_vl->setSpacing(8);

        auto* bench_premium = make_double_spin(0, 200, 30, 1, "%", bench);
        double_inputs_["bench_premium"] = bench_premium;
        bench_vl->addWidget(build_input_row(tr("Target Premium %"), bench_premium, bench));

        auto* bench_run = make_run_button(tr("BENCHMARK PREMIUM"), bench);
        connect(bench_run, &QPushButton::clicked, this, [this]() {
            QJsonArray comparables;
            if (!grid_deals(comparison_grid_, &comparables, false))
                return;
            status_label_->setText(tr("Benchmarking Premium..."));
            QJsonObject params;
            params["target_premium"] = double_inputs_["bench_premium"]->value() / 100.0;
            params["comparables"] = comparables;
            MAAnalyticsService::instance().benchmark_deal_premium(params);
        });
        bench_vl->addWidget(bench_run);
        bench_vl->addStretch();
    });

    // ── Payment Structure ──
    add_lazy_sub_tab(QT_TR_NOOP("Payment"), [this](QWidget* pay) {
        auto* pay_vl = new QVBoxLayout(pay);
        pay_vl->setContentsMargins(12, 12, 12, 12);
        pay_vl->setSpacing(8);
        auto* pay_run = make_run_button(tr("ANALYZE PAYMENT STRUCTURES"), pay);
        connect(pay_run, &QPushButton::clicked, this, [this]() {
            QJsonArray deals;
            if (!grid_deals(comparison_grid_, &deals, false))
                return;
            status_label_->setText(tr("Analyzing Payment Structures..."));
            QJsonObject params;
            params["deals"] = deals;
            MAAnalyticsService::instance().analyze_payment_structures(params);
        });
        pay_vl->addWidget(pay_run);
        pay_vl->addStretch();
    });

    // ── Industry ──
    add_lazy_sub_tab(QT_TR_NOOP("Industry"), [this](QWidget* ind) {
        auto* ind_vl = new QVBoxLayout(ind);
        ind_vl->setContentsMargins(12, 12, 12, 12);
        ind_vl->setSpacing(8);
        auto* ind_run = make_run_button(tr("ANALYZE BY INDUSTRY"), ind);
        connect(ind_run, &QPushButton::clicked, this, [this]() {
            QJsonArray deals;
            if (!grid_deals(comparison_grid_, &deals, false))
                return;
            status_label_->setText(tr("Analyzing Industry Deals..."));
            QJsonObject params;
            params["deals"] = deals;
            MAAnalyticsService::instance().analyze_industry_deals(params);
        });
        ind_vl->addWidget(ind_run);
        ind_vl->addStretch();
    });

    vl->addWidget(sub_tabs_);

    results_container_ = new QWidget(w);
    results_layout_ = new QVBoxLayout(results_container_);
    results_layout_->setContentsMargins(0, 8, 0, 0);
    results_layout_->setSpacing(8);
    vl->addWidget(results_container_);

    return w;
}

} // namespace fincept::screens
