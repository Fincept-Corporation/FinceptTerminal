// src/screens/equity_research/EquityTechnicalsTab.h
//
// Equity Research → Technicals. A price chart with moving-average / Bollinger overlays and
// RSI / MACD / volume panes on top; the technical rating split into MOVING AVERAGES
// (price above / below each average), OSCILLATORS (zone + direction votes) and a SUMMARY,
// where only indicators that vote count; a timeframe selector (1H / D / W / M); and a
// performance and risk block: returns vs the home benchmark, realised volatility,
// drawdowns, beta and the 52-week position. Widgets are built once and refilled.
#pragma once

#include "services/equity/EquityResearchModels.h"
#include "services/equity/EquityTechnicalMath.h"

#include <QLabel>
#include <QPushButton>
#include <QWidget>

class QTableWidget;

namespace fincept::ui {
class Gauge;
class LoadingOverlay;
class RangeBar;
class TimeSeriesChart;
} // namespace fincept::ui

namespace fincept::services::equity {
struct LoadFailure;
}

namespace fincept::screens {

namespace er_ui {
class Segmented;
class StatusStrip;
} // namespace er_ui

class EquityTechnicalsTab : public QWidget {
    Q_OBJECT
  public:
    explicit EquityTechnicalsTab(QWidget* parent = nullptr);
    void set_symbol(const QString& symbol);

  protected:
    void changeEvent(QEvent* event) override;

  private:
    enum class Frame { Hourly, Daily, Weekly, Monthly };

    void build_ui();
    void retranslateUi();
    void request(bool force);
    void on_history(const QString& symbol, const QString& period, const QVector<services::equity::Candle>& candles);
    void on_load_failed(const services::equity::LoadFailure& f);
    void load_hourly();
    /// The bars of the selected timeframe.
    services::equity::tmath::Bars frame_bars() const;
    void render();
    void render_chart(const services::equity::tmath::Bars& b);
    void render_rating(const services::equity::tmath::Bars& b);
    void render_performance();
    QString signal_text(services::equity::TechSignal s) const;
    QString fmt_price(double v) const;

    QString current_symbol_;
    QString currency_;
    Frame frame_ = Frame::Daily;
    QVector<services::equity::Candle> daily_;
    QVector<services::equity::Candle> hourly_;
    QVector<services::equity::Candle> bench_;
    bool daily_loaded_ = false;

    er_ui::Segmented* frame_seg_ = nullptr;
    QPushButton* refresh_btn_ = nullptr;
    er_ui::StatusStrip* status_ = nullptr;
    ui::TimeSeriesChart* chart_ = nullptr;
    QWidget* chart_host_ = nullptr;
    ui::Gauge* gauge_osc_ = nullptr;
    ui::Gauge* gauge_sum_ = nullptr;
    ui::Gauge* gauge_ma_ = nullptr;
    QLabel* gauge_title_[3] = {nullptr, nullptr, nullptr};
    QLabel* gauge_counts_[3] = {nullptr, nullptr, nullptr};
    QTableWidget* ma_table_ = nullptr;
    QTableWidget* osc_table_ = nullptr;
    QTableWidget* perf_table_ = nullptr;
    QTableWidget* risk_table_ = nullptr;
    ui::RangeBar* range52_ = nullptr;
    QLabel* method_note_ = nullptr;
    QLabel* ma_title_ = nullptr;
    QLabel* osc_title_ = nullptr;
    QLabel* perf_title_ = nullptr;
    QLabel* risk_title_ = nullptr;
    QLabel* range_title_ = nullptr;
    ui::LoadingOverlay* loading_overlay_ = nullptr;
};

} // namespace fincept::screens
