// src/screens/equity_research/EquityOverviewTab.h
#pragma once
#include "services/equity/EquityResearchModels.h"
#include "ui/widgets/LoadingOverlay.h"

#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QWidget>

namespace fincept::screens {

// ── Custom QPainter candlestick canvas ───────────────────────────────────────
class ResearchCandleCanvas : public QWidget {
    Q_OBJECT
  public:
    explicit ResearchCandleCanvas(QWidget* parent = nullptr);
    void set_candles(const QVector<services::equity::Candle>& candles, const QString& currency_sym);
    void clear();

  protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;

  private:
    void rebuild_cache();

    QVector<services::equity::Candle> candles_;
    QPixmap cache_;
    bool dirty_ = true;
    QString currency_sym_ = "$";

    static constexpr int MAX_VISIBLE = 260;
    static constexpr int PRICE_AXIS_W = 80;
    static constexpr int TIME_AXIS_H = 24;
    static constexpr int LABEL_STEP = 20;
};

// ── Overview tab ─────────────────────────────────────────────────────────────
class EquityOverviewTab : public QWidget {
    Q_OBJECT
  public:
    explicit EquityOverviewTab(QWidget* parent = nullptr);
    void set_symbol(const QString& symbol);

    static QString currency_symbol(const QString& currency_code);

  private slots:
    void on_info_loaded(services::equity::StockInfo info);
    void on_historical_loaded(QString symbol, QVector<services::equity::Candle> candles);
    void on_quote_loaded(services::equity::QuoteData quote);

  private:
    void build_ui();

    QWidget* build_col1();
    QWidget* build_chart_panel();
    QWidget* build_col4();

    QWidget* build_trading_panel();
    QWidget* build_valuation_panel();
    QWidget* build_share_stats_panel();
    QWidget* build_analyst_panel();
    QWidget* build_52w_panel();
    QWidget* build_profitability_panel();
    QWidget* build_growth_panel();

    QWidget* build_bottom_row();
    QWidget* build_company_desc_panel();
    QWidget* build_company_info_panel();
    QWidget* build_financial_health_panel();

    void rebuild_chart(const QVector<services::equity::Candle>& candles);
    void switch_period(QPushButton* btn, const QString& period);

    static QString fmt_large(double v);
    static QString fmt_pct(double v);
    QString fmt_price(double v) const;

    QString current_symbol_;
    QString current_period_ = "1y";
    QString current_currency_;

    // Trading panel
    QLabel* open_val_ = nullptr;
    QLabel* high_val_ = nullptr;
    QLabel* low_val_ = nullptr;
    QLabel* prev_close_val_ = nullptr;
    QLabel* vol_val_ = nullptr;

    // Valuation panel
    QLabel* mktcap_val_ = nullptr;
    QLabel* pe_val_ = nullptr;
    QLabel* fwd_pe_val_ = nullptr;
    QLabel* peg_val_ = nullptr;
    QLabel* pb_val_ = nullptr;
    QLabel* div_val_ = nullptr;
    QLabel* beta_val_ = nullptr;

    // Share stats panel
    QLabel* shares_out_val_ = nullptr;
    QLabel* float_val_ = nullptr;
    QLabel* insiders_val_ = nullptr;
    QLabel* institutions_val_ = nullptr;
    QLabel* short_pct_val_ = nullptr;

    // Chart
    ResearchCandleCanvas* candle_canvas_ = nullptr;
    QPushButton* btn_1m_ = nullptr;
    QPushButton* btn_3m_ = nullptr;
    QPushButton* btn_6m_ = nullptr;
    QPushButton* btn_1y_ = nullptr;
    QPushButton* btn_5y_ = nullptr;
    QPushButton* active_period_btn_ = nullptr;

    // Analyst panel
    QLabel* target_high_val_ = nullptr;
    QLabel* target_mean_val_ = nullptr;
    QLabel* target_low_val_ = nullptr;
    QLabel* analyst_count_val_ = nullptr;
    QLabel* rec_key_label_ = nullptr;

    // 52 Week Range
    QLabel* w52h_val_ = nullptr;
    QLabel* w52l_val_ = nullptr;
    QLabel* avg_vol_val_ = nullptr;

    // Profitability
    QLabel* gross_margin_val_ = nullptr;
    QLabel* op_margin_val_ = nullptr;
    QLabel* profit_margin_val_ = nullptr;
    QLabel* roa_val_ = nullptr;
    QLabel* roe_val_ = nullptr;

    // Growth
    QLabel* rev_growth_val_ = nullptr;
    QLabel* earnings_growth_val_ = nullptr;

    // Company description
    QLabel* company_desc_ = nullptr;

    // Company info
    QLabel* company_emp_ = nullptr;
    QLabel* company_web_ = nullptr;
    QLabel* company_currency_ = nullptr;

    // Financial health
    QLabel* cash_val_ = nullptr;
    QLabel* debt_val_ = nullptr;
    QLabel* free_cf_val_ = nullptr;

    // Cached data
    services::equity::StockInfo cached_info_;
    services::equity::QuoteData cached_quote_;
    QVector<services::equity::Candle> cached_candles_;

    ui::LoadingOverlay* loading_overlay_ = nullptr;
    bool info_loaded_ = false;
    bool quote_loaded_ = false;
    bool historical_loaded_ = false;
};

} // namespace fincept::screens
