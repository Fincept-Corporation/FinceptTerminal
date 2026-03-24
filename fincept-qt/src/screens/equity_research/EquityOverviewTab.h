// src/screens/equity_research/EquityOverviewTab.h
#pragma once
#include "services/equity/EquityResearchModels.h"
#include "ui/widgets/LoadingOverlay.h"

#include <QChartView>
#include <QLabel>
#include <QPushButton>
#include <QWidget>

namespace fincept::screens {

class EquityOverviewTab : public QWidget {
    Q_OBJECT
  public:
    explicit EquityOverviewTab(QWidget* parent = nullptr);
    void set_symbol(const QString& symbol);

  private slots:
    void on_info_loaded(services::equity::StockInfo info);
    void on_historical_loaded(QString symbol, QVector<services::equity::Candle> candles);
    void on_quote_loaded(services::equity::QuoteData quote);

  private:
    void build_ui();

    // Column builders
    QWidget* build_col1();        // Trading + Valuation + Share Stats
    QWidget* build_chart_panel(); // Chart (spans 2 cols)
    QWidget* build_col4();        // Analyst + 52W Range + Profitability + Growth

    // Panel builders
    QWidget* build_trading_panel();
    QWidget* build_valuation_panel();
    QWidget* build_share_stats_panel();
    QWidget* build_analyst_panel();
    QWidget* build_52w_panel();
    QWidget* build_profitability_panel();
    QWidget* build_growth_panel();

    // Bottom row
    QWidget* build_bottom_row();
    QWidget* build_company_desc_panel();
    QWidget* build_company_info_panel();
    QWidget* build_financial_health_panel();

    void rebuild_chart(const QVector<services::equity::Candle>& candles);

    static QString fmt_large(double v);
    static QString fmt_pct(double v);
    static QString fmt_price(double v);

    QString current_symbol_;
    QString current_period_ = "1y";

    // ── Trading panel ──────────────────────────────────────────────────────────
    QLabel* open_val_ = nullptr;
    QLabel* high_val_ = nullptr;
    QLabel* low_val_ = nullptr;
    QLabel* prev_close_val_ = nullptr;
    QLabel* vol_val_ = nullptr;

    // ── Valuation panel ────────────────────────────────────────────────────────
    QLabel* mktcap_val_ = nullptr;
    QLabel* pe_val_ = nullptr;
    QLabel* fwd_pe_val_ = nullptr;
    QLabel* peg_val_ = nullptr;
    QLabel* pb_val_ = nullptr;
    QLabel* div_val_ = nullptr;
    QLabel* beta_val_ = nullptr;

    // ── Share stats panel ──────────────────────────────────────────────────────
    QLabel* shares_out_val_ = nullptr;
    QLabel* float_val_ = nullptr;
    QLabel* insiders_val_ = nullptr;
    QLabel* institutions_val_ = nullptr;
    QLabel* short_pct_val_ = nullptr;

    // ── Chart ──────────────────────────────────────────────────────────────────
    QChartView* chart_view_ = nullptr;
    QPushButton* btn_1m_ = nullptr;
    QPushButton* btn_3m_ = nullptr;
    QPushButton* btn_6m_ = nullptr;
    QPushButton* btn_1y_ = nullptr;
    QPushButton* btn_5y_ = nullptr;

    // ── Analyst panel ──────────────────────────────────────────────────────────
    QLabel* target_high_val_ = nullptr;
    QLabel* target_mean_val_ = nullptr;
    QLabel* target_low_val_ = nullptr;
    QLabel* analyst_count_val_ = nullptr;
    QLabel* rec_key_label_ = nullptr;

    // ── 52 Week Range panel ────────────────────────────────────────────────────
    QLabel* w52h_val_ = nullptr;
    QLabel* w52l_val_ = nullptr;
    QLabel* avg_vol_val_ = nullptr;

    // ── Profitability panel ────────────────────────────────────────────────────
    QLabel* gross_margin_val_ = nullptr;
    QLabel* op_margin_val_ = nullptr;
    QLabel* profit_margin_val_ = nullptr;
    QLabel* roa_val_ = nullptr;
    QLabel* roe_val_ = nullptr;

    // ── Growth panel ──────────────────────────────────────────────────────────
    QLabel* rev_growth_val_ = nullptr;
    QLabel* earnings_growth_val_ = nullptr;

    // ── Company description ────────────────────────────────────────────────────
    QLabel* company_desc_ = nullptr;

    // ── Company info ───────────────────────────────────────────────────────────
    QLabel* company_emp_ = nullptr;
    QLabel* company_web_ = nullptr;
    QLabel* company_currency_ = nullptr;

    // ── Financial health ───────────────────────────────────────────────────────
    QLabel* cash_val_ = nullptr;
    QLabel* debt_val_ = nullptr;
    QLabel* free_cf_val_ = nullptr;

    // Cached data
    services::equity::StockInfo cached_info_;
    services::equity::QuoteData cached_quote_;

    ui::LoadingOverlay* loading_overlay_ = nullptr;
    bool info_loaded_ = false;
    bool quote_loaded_ = false;
    bool historical_loaded_ = false;
};

} // namespace fincept::screens
