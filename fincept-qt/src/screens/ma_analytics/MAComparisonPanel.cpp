// MAComparisonPanel.cpp — deal comparison & league-table sub-tab builder for MAModulePanel.
// Method definition split from MAModulePanel.cpp.

#include "screens/ma_analytics/MAModulePanel.h"

#include "ui/theme/Theme.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
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

    sub_tabs_ = new QTabWidget(w);
    apply_tab_stylesheet();

    // ── Compare ──
    auto* cmp = new QWidget(this);
    auto* cmp_vl = new QVBoxLayout(cmp);
    cmp_vl->setContentsMargins(12, 12, 12, 12);
    cmp_vl->setSpacing(8);

    auto* cmp_hint = new QLabel("Enter deal data as JSON array. Each deal: "
                                "{\"acquirer\":\"...\",\"target\":\"...\",\"deal_value\":N,\"premium\":N,"
                                "\"ev_revenue\":N,\"ev_ebitda\":N}",
                                cmp);
    cmp_hint->setWordWrap(true);
    cmp_hint->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                                .arg(ui::colors::TEXT_SECONDARY())
                                .arg(ui::fonts::SMALL)
                                .arg(ui::fonts::DATA_FAMILY));
    cmp_vl->addWidget(cmp_hint);

    auto* cmp_text = new QTextEdit(cmp);
    cmp_text->setPlaceholderText("[{\"acquirer\":\"MSFT\",\"target\":\"ATVI\",\"deal_value\":68700,\"premium\":45.3,"
                                 "\"ev_revenue\":8.7,\"ev_ebitda\":23.1}]");
    cmp_text->setMaximumHeight(120);
    cmp_text->setStyleSheet(QString("QTextEdit { background:%1; color:%2; border:1px solid %3;"
                                    "font-family:%4; font-size:%5px; padding:8px; }")
                                .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED())
                                .arg(ui::fonts::DATA_FAMILY)
                                .arg(ui::fonts::SMALL));
    cmp_vl->addWidget(cmp_text);

    auto* cmp_run = make_run_button("COMPARE DEALS", cmp);
    connect(cmp_run, &QPushButton::clicked, this, [this, cmp_text]() {
        status_label_->setText("Comparing Deals...");
        auto doc = QJsonDocument::fromJson(cmp_text->toPlainText().toUtf8());
        QJsonObject params;
        params["deals"] = doc.array();
        MAAnalyticsService::instance().compare_deals(params);
    });
    cmp_vl->addWidget(cmp_run);
    cmp_vl->addStretch();
    sub_tabs_->addTab(cmp, "Compare");

    // ── Rank ──
    auto* rank = new QWidget(this);
    auto* rank_vl = new QVBoxLayout(rank);
    rank_vl->setContentsMargins(12, 12, 12, 12);
    rank_vl->setSpacing(8);

    auto* rank_criteria = new QComboBox(rank);
    rank_criteria->addItems({"premium", "deal_value", "ev_revenue", "ev_ebitda", "synergies"});
    rank_criteria->setStyleSheet(QString("QComboBox { background:%1; color:%2; border:1px solid %3;"
                                         "font-family:%4; font-size:%5px; padding:4px 6px; }")
                                     .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED())
                                     .arg(ui::fonts::DATA_FAMILY)
                                     .arg(ui::fonts::SMALL));
    combo_inputs_["rank_criteria"] = rank_criteria;
    rank_vl->addWidget(build_input_row("Rank By", rank_criteria, rank));

    auto* rank_text = new QTextEdit(rank);
    rank_text->setPlaceholderText("Same JSON array format as Compare tab...");
    rank_text->setMaximumHeight(120);
    rank_text->setStyleSheet(cmp_text->styleSheet());
    rank_vl->addWidget(rank_text);

    auto* rank_run = make_run_button("RANK DEALS", rank);
    connect(rank_run, &QPushButton::clicked, this, [this, rank_text]() {
        status_label_->setText("Ranking Deals...");
        auto doc = QJsonDocument::fromJson(rank_text->toPlainText().toUtf8());
        QJsonObject params;
        params["deals"] = doc.array();
        params["criteria"] = combo_inputs_["rank_criteria"]->currentText();
        MAAnalyticsService::instance().rank_deals(params);
    });
    rank_vl->addWidget(rank_run);
    rank_vl->addStretch();
    sub_tabs_->addTab(rank, "Rank");

    // ── Benchmark ──
    auto* bench = new QWidget(this);
    auto* bench_vl = new QVBoxLayout(bench);
    bench_vl->setContentsMargins(12, 12, 12, 12);
    bench_vl->setSpacing(8);

    auto* bench_premium = make_double_spin(0, 200, 30, 1, "%", bench);
    double_inputs_["bench_premium"] = bench_premium;
    bench_vl->addWidget(build_input_row("Target Premium %", bench_premium, bench));

    auto* bench_text = new QTextEdit(bench);
    bench_text->setPlaceholderText("Comparable deals JSON array...");
    bench_text->setMaximumHeight(120);
    bench_text->setStyleSheet(cmp_text->styleSheet());
    bench_vl->addWidget(bench_text);

    auto* bench_run = make_run_button("BENCHMARK PREMIUM", bench);
    connect(bench_run, &QPushButton::clicked, this, [this, bench_text]() {
        status_label_->setText("Benchmarking Premium...");
        auto doc = QJsonDocument::fromJson(bench_text->toPlainText().toUtf8());
        QJsonObject params;
        params["target_premium"] = double_inputs_["bench_premium"]->value() / 100.0;
        params["comparables"] = doc.array();
        MAAnalyticsService::instance().benchmark_deal_premium(params);
    });
    bench_vl->addWidget(bench_run);
    bench_vl->addStretch();
    sub_tabs_->addTab(bench, "Benchmark");

    // ── Payment Structure ──
    auto* pay = new QWidget(this);
    auto* pay_vl = new QVBoxLayout(pay);
    pay_vl->setContentsMargins(12, 12, 12, 12);
    pay_vl->setSpacing(8);

    auto* pay_text = new QTextEdit(pay);
    pay_text->setPlaceholderText("Deals JSON with cash_pct and stock_pct fields...");
    pay_text->setMaximumHeight(120);
    pay_text->setStyleSheet(cmp_text->styleSheet());
    pay_vl->addWidget(pay_text);

    auto* pay_run = make_run_button("ANALYZE PAYMENT STRUCTURES", pay);
    connect(pay_run, &QPushButton::clicked, this, [this, pay_text]() {
        status_label_->setText("Analyzing Payment Structures...");
        auto doc = QJsonDocument::fromJson(pay_text->toPlainText().toUtf8());
        QJsonObject params;
        params["deals"] = doc.array();
        MAAnalyticsService::instance().analyze_payment_structures(params);
    });
    pay_vl->addWidget(pay_run);
    pay_vl->addStretch();
    sub_tabs_->addTab(pay, "Payment");

    // ── Industry ──
    auto* ind = new QWidget(this);
    auto* ind_vl = new QVBoxLayout(ind);
    ind_vl->setContentsMargins(12, 12, 12, 12);
    ind_vl->setSpacing(8);

    auto* ind_text = new QTextEdit(ind);
    ind_text->setPlaceholderText("Deals JSON with industry field...");
    ind_text->setMaximumHeight(120);
    ind_text->setStyleSheet(cmp_text->styleSheet());
    ind_vl->addWidget(ind_text);

    auto* ind_run = make_run_button("ANALYZE BY INDUSTRY", ind);
    connect(ind_run, &QPushButton::clicked, this, [this, ind_text]() {
        status_label_->setText("Analyzing Industry Deals...");
        auto doc = QJsonDocument::fromJson(ind_text->toPlainText().toUtf8());
        QJsonObject params;
        params["deals"] = doc.array();
        MAAnalyticsService::instance().analyze_industry_deals(params);
    });
    ind_vl->addWidget(ind_run);
    ind_vl->addStretch();
    sub_tabs_->addTab(ind, "Industry");

    vl->addWidget(sub_tabs_);

    results_container_ = new QWidget(w);
    results_layout_ = new QVBoxLayout(results_container_);
    results_layout_->setContentsMargins(0, 8, 0, 0);
    results_layout_->setSpacing(8);
    vl->addWidget(results_container_);

    return w;
}

} // namespace fincept::screens
