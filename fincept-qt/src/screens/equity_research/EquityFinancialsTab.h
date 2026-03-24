// src/screens/equity_research/EquityFinancialsTab.h
#pragma once
#include "services/equity/EquityResearchModels.h"
#include "ui/widgets/LoadingOverlay.h"

#include <QChartView>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QWidget>

namespace fincept::screens {

class EquityFinancialsTab : public QWidget {
    Q_OBJECT
  public:
    explicit EquityFinancialsTab(QWidget* parent = nullptr);
    void set_symbol(const QString& symbol);

  private slots:
    void on_financials_loaded(services::equity::FinancialsData data);

  private:
    void build_ui();

    // Statement view builders
    QWidget* build_income_view();
    QWidget* build_balance_view();
    QWidget* build_cashflow_view();

    // Section builders
    QWidget* build_key_metrics_section();
    QWidget* build_profitability_section();
    QWidget* build_dupont_section();
    QWidget* build_liquidity_section();
    QWidget* build_balance_section();
    QWidget* build_cashflow_detail_section();
    QWidget* build_raw_table_section(QTableWidget*& tbl_out);

    // Chart builders
    QChartView* build_revenue_trend_chart();
    QChartView* build_margin_trend_chart();
    QChartView* build_balance_trend_chart();
    QChartView* build_cashflow_trend_chart();
    QChartView* build_return_trend_chart();

    // Populate helpers
    void populate_income_view(const services::equity::FinancialsData& d);
    void populate_balance_view(const services::equity::FinancialsData& d);
    void populate_cashflow_view(const services::equity::FinancialsData& d);
    void populate_table(QTableWidget* tbl, const QVector<QPair<QString, QJsonObject>>& stmt);
    void rebuild_revenue_chart(const services::equity::FinancialsData& d);
    void rebuild_margin_chart(const services::equity::FinancialsData& d);
    void rebuild_balance_chart(const services::equity::FinancialsData& d);
    void rebuild_cashflow_chart(const services::equity::FinancialsData& d);
    void rebuild_return_chart(const services::equity::FinancialsData& d);

    // Helpers
    static QFrame* make_section(const QString& title, const QString& color);
    static QLabel* make_metric_card(QWidget* parent, const QString& label, const QString& value, const QString& color,
                                    const QString& subtitle = {});
    static void set_metric(QLabel* lbl, const QString& val);
    static double get_val(const QJsonObject& o, const QStringList& keys);
    static QString fmt_large(double v);
    static QString fmt_pct(double v);
    static QString fmt_ratio(double v);
    static QColor growth_color(double v);

    QString current_symbol_;
    bool loaded_ = false;

    // Statement selector buttons
    QPushButton* btn_income_ = nullptr;
    QPushButton* btn_balance_ = nullptr;
    QPushButton* btn_cashflow_ = nullptr;

    QStackedWidget* stack_ = nullptr;

    // ── Income statement widgets ───────────────────────────────────────────────
    QLabel* inc_revenue_val_ = nullptr;
    QLabel* inc_revenue_sub_ = nullptr;
    QLabel* inc_gross_val_ = nullptr;
    QLabel* inc_gross_sub_ = nullptr;
    QLabel* inc_opincome_val_ = nullptr;
    QLabel* inc_opincome_sub_ = nullptr;
    QLabel* inc_netincome_val_ = nullptr;
    QLabel* inc_netincome_sub_ = nullptr;
    QLabel* inc_ebitda_val_ = nullptr;
    QLabel* inc_ebitda_sub_ = nullptr;
    QLabel* inc_gross_margin_ = nullptr;
    QLabel* inc_op_margin_ = nullptr;
    QLabel* inc_net_margin_ = nullptr;
    QLabel* inc_ebitda_margin_ = nullptr;
    QChartView* inc_revenue_chart_ = nullptr;
    QChartView* inc_margin_chart_ = nullptr;
    QTableWidget* inc_table_ = nullptr;

    // DuPont labels (income view)
    QLabel* dupont_net_margin_ = nullptr;
    QLabel* dupont_asset_turn_ = nullptr;
    QLabel* dupont_eq_mult_ = nullptr;
    QLabel* dupont_roe_result_ = nullptr;

    // Return metrics
    QLabel* ret_roe_val_ = nullptr;
    QLabel* ret_roa_val_ = nullptr;
    QLabel* ret_roic_val_ = nullptr;
    QLabel* ret_roce_val_ = nullptr;
    QChartView* ret_chart_ = nullptr;

    // ── Balance sheet widgets ──────────────────────────────────────────────────
    QLabel* bal_assets_val_ = nullptr;
    QLabel* bal_liabilities_val_ = nullptr;
    QLabel* bal_equity_val_ = nullptr;
    QLabel* bal_debt_val_ = nullptr;
    QLabel* bal_cash_val_ = nullptr;
    QLabel* bal_current_ratio_ = nullptr;
    QLabel* bal_quick_ratio_ = nullptr;
    QLabel* bal_debt_equity_ = nullptr;
    QLabel* bal_debt_assets_ = nullptr;
    QLabel* bal_int_coverage_ = nullptr;
    QLabel* bal_working_cap_ = nullptr;
    QChartView* bal_chart_ = nullptr;
    QTableWidget* bal_table_ = nullptr;

    // ── Cash flow widgets ──────────────────────────────────────────────────────
    QLabel* cf_operating_val_ = nullptr;
    QLabel* cf_investing_val_ = nullptr;
    QLabel* cf_financing_val_ = nullptr;
    QLabel* cf_capex_val_ = nullptr;
    QLabel* cf_fcf_val_ = nullptr;
    QLabel* cf_fcf_sub_ = nullptr;
    QLabel* cf_dividends_val_ = nullptr;
    QLabel* cf_buybacks_val_ = nullptr;
    QLabel* cf_fcf_margin_ = nullptr;
    QLabel* cf_capex_rev_ = nullptr;
    QChartView* cf_chart_ = nullptr;
    QTableWidget* cf_table_ = nullptr;

    services::equity::FinancialsData cached_data_;

    ui::LoadingOverlay* loading_overlay_ = nullptr;
};

} // namespace fincept::screens
