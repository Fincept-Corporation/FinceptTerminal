#pragma once
#include "screens/dashboard/widgets/BaseWidget.h"
#include "services/markets/MarketDataService.h"

#include <QLabel>

namespace fincept::screens::widgets {

/// Risk Metrics Widget — displays VIX (fear gauge), estimated beta for key stocks,
/// 52-week range positions, and correlation proxies. All data from yfinance.
class RiskMetricsWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit RiskMetricsWidget(QWidget* parent = nullptr);

  protected:
    void on_theme_changed() override;

  private:
    void apply_styles();
    void refresh_data();
    void populate(const QVector<services::QuoteData>& quotes);

    // VIX section
    QWidget* vix_card_ = nullptr;
    QLabel* vix_header_lbl_ = nullptr;
    QLabel* vix_value_ = nullptr;
    QLabel* vix_regime_ = nullptr;
    QLabel* vix_bar_fill_ = nullptr;
    QVector<QFrame*> vix_bar_segments_; // 5 gradient segments

    // Separators and section headers
    QFrame* sep1_ = nullptr;
    QFrame* sep2_ = nullptr;
    QLabel* stocks_hdr_ = nullptr;
    QLabel* corr_hdr_ = nullptr;

    // Volatility cluster labels [6 stocks]
    struct StockRisk {
        QLabel* symbol = nullptr;
        QLabel* chg_pct = nullptr;
        QLabel* hi_lo = nullptr;
    };
    QVector<StockRisk> stock_rows_;

    // Spread row labels (the left-hand descriptors)
    QVector<QLabel*> spread_labels_;
    // Correlation proxies
    QLabel* spy_qqq_spread_ = nullptr;
    QLabel* spy_iwm_spread_ = nullptr;
    QLabel* equity_bond_lbl_ = nullptr; // SPY vs TLT proxy
};

} // namespace fincept::screens::widgets
