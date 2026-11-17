// src/screens/equity_research/EquityOverviewTab.h
//
// Equity Research → Overview: company block (sector, industry, country, exchange, quote vs
// reporting currency, next earnings, ex-dividend), return tiles vs the home benchmark,
// an interactive price chart (range presets 1D…MAX incl. intraday and YTD, log scale,
// indicator overlays and panes, rebased comparison against the benchmark or any symbol,
// earnings / dividend / split markers, line or candles), 52-week and analyst-target range
// bars, and the key-statistics panels — every amount labelled with its currency.
#pragma once

#include "services/equity/EquityResearchModels.h"
#include "services/equity/EquityTechnicalMath.h"

#include <QHash>
#include <QLabel>
#include <QMap>
#include <QPushButton>
#include <QSet>
#include <QStringList>
#include <QWidget>

class QGridLayout;
class QStackedWidget;
class QVBoxLayout;

namespace fincept::ui {
class CandleChart;
class KpiStrip;
class LoadingOverlay;
class RangeBar;
class TimeSeriesChart;
} // namespace fincept::ui

namespace fincept::services::equity {
struct LoadFailure;
}

namespace fincept::screens {

namespace er_ui {
class StatusStrip;
class Segmented;
} // namespace er_ui

class EquityOverviewTab : public QWidget {
    Q_OBJECT
  public:
    explicit EquityOverviewTab(QWidget* parent = nullptr);
    void set_symbol(const QString& symbol);

  protected:
    void changeEvent(QEvent* event) override;

  private slots:
    void on_info_loaded(services::equity::StockInfo info);
    void on_historical_loaded(QString symbol, QString period, QVector<services::equity::Candle> candles);
    void on_quote_loaded(services::equity::QuoteData quote);
    void on_events_loaded(services::equity::EquityEvents events);

  private:
    // Layout
    void build_ui();
    QWidget* build_company_block();
    QWidget* build_chart_panel();
    QWidget* build_side_panels();
    void retranslateUi();
    QLabel* add_stat(QGridLayout* grid, int row, const char* key);

    // Data
    void on_load_failed(const services::equity::LoadFailure& f);
    void request_all();
    void render_info();
    void render_quote();
    void render_company();
    void render_ranges();
    void render_returns();

    // Chart
    void rebuild_chart();
    /// Coalesce the rebuilds data arrival triggers (info, events, history, benchmark land
    /// within a few hundred ms of each other): one rebuild + first paint instead of four.
    void schedule_rebuild();
    bool rebuild_pending_ = false;
    void load_intraday(const QString& preset);
    void on_preset(const QString& preset);
    void show_indicator_menu();
    void show_compare_menu();
    void add_comparison(const QString& symbol);
    QString fmt_quote_price(double v) const;

    // State
    QString current_symbol_;
    services::equity::StockInfo info_;
    services::equity::QuoteData quote_;
    services::equity::EquityEvents events_;
    bool info_loaded_ = false;
    bool quote_loaded_ = false;
    bool history_loaded_ = false;
    bool events_loaded_ = false;
    QVector<services::equity::Candle> daily_;     ///< full daily history ("max")
    QVector<services::equity::Candle> intraday_;  ///< 1D / 5D bars while an intraday preset is shown
    QString intraday_preset_;                     ///< "1D" / "5D" when intraday_ is on the chart
    QString pending_preset_ = QStringLiteral("1Y");
    QMap<QString, QVector<services::equity::Candle>> compare_data_; ///< symbol → daily history
    QStringList compare_symbols_;
    QSet<QString> indicators_ = {QStringLiteral("sma50"), QStringLiteral("sma200"), QStringLiteral("volume")};
    bool show_events_ = true;
    bool candles_mode_ = false;
    QMap<QString, QString> leg_errors_;

    // Widgets
    er_ui::StatusStrip* status_ = nullptr;
    QLabel* name_lbl_ = nullptr;
    QLabel* meta_lbl_ = nullptr;
    QLabel* ccy_lbl_ = nullptr;
    QLabel* dates_lbl_ = nullptr;
    QLabel* rec_badge_ = nullptr;
    ui::KpiStrip* returns_ = nullptr;
    ui::TimeSeriesChart* chart_ = nullptr;
    ui::CandleChart* candles_ = nullptr;
    QStackedWidget* chart_stack_ = nullptr;
    QPushButton* indicators_btn_ = nullptr;
    QPushButton* compare_btn_ = nullptr;
    QPushButton* events_btn_ = nullptr;
    er_ui::Segmented* style_seg_ = nullptr;
    ui::RangeBar* range52_ = nullptr;
    ui::RangeBar* targets_ = nullptr;
    QLabel* range52_title_ = nullptr;
    QLabel* targets_title_ = nullptr;
    QLabel* targets_note_ = nullptr;
    QLabel* desc_lbl_ = nullptr;
    QHash<QString, QLabel*> stats_; ///< stat key → value label
    QHash<QLabel*, const char*> i18n_; ///< static label → source text
    ui::LoadingOverlay* loading_overlay_ = nullptr;
};

} // namespace fincept::screens
