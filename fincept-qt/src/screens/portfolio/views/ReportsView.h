// src/screens/portfolio/views/ReportsView.h
#pragma once
#include "screens/portfolio/PortfolioTypes.h"
#include "services/portfolio/PortfolioAnalyticsService.h"
#include "services/portfolio/PortfolioLots.h"

#include <QHash>
#include <QWidget>

class QButtonGroup;
class QHideEvent;
class QLabel;
class QPushButton;
class QShowEvent;
class QTableWidget;
class QTabWidget;
class QTimer;

namespace fincept::ui {
class BarChart;
class InputMemory;
class KpiStrip;
} // namespace fincept::ui

namespace fincept::screens::pfmt {
class PortfolioNoticeBar;
}

namespace fincept::screens {

/// REPORTS — what happened over a period (MTD … since inception):
///   SUMMARY       start / end NAV, net flows, investment gain, TWR / MWR vs the
///                 benchmark, realised P&L, income, fees and taxes of the period;
///   CONTRIBUTION  per holding: realised P&L booked, income received and costs in
///                 the period, unrealised P&L, today's contribution (vs the previous
///                 close NAV) and, on demand, the period price move on real prices;
///   TRANSACTIONS  every ledger row of the period (no cap).
/// Export: CSV of the tab on display, or the whole report as HTML / PDF.
///
/// The period price moves load by themselves while the view is on screen (once the
/// period's exact start is known; again when the period changes), with "…" in
/// their cells meanwhile; leaving the view cancels the download; a failure says
/// so with RETRY. The chosen period is remembered (InputMemory "portfolio/reports").
class ReportsView : public QWidget {
    Q_OBJECT
  public:
    explicit ReportsView(QWidget* parent = nullptr);

    void set_data(const portfolio::PortfolioSummary& summary, const QString& currency);

  protected:
    void changeEvent(QEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

  private:
    struct Line {
        QString symbol;
        double realised = 0;
        double income = 0;
        double costs = 0; ///< fees + taxes, base
        double unrealised = portfolio::kAbsent;
        double day_contrib_pct = portfolio::kAbsent;
        double period_move = portfolio::kAbsent; ///< price move over the period, base (fraction)
        double period_unrealised = portfolio::kAbsent; ///< estimated unrealised change in the period, base
        bool unknown_fx = false;
    };

    void build_ui();
    void apply_styles();
    void request_metrics();
    void recompute();
    /// Period transactions table from ledger_ (no replay; also the format-change re-render).
    void fill_txn_table();
    void render();
    void load_period_moves();
    /// Start the automatic load of the period moves (debounced) when the view is on
    /// screen, the period's start is known and they are not loaded / loading / failed.
    void schedule_period_moves();
    void cancel_period_moves();
    /// What the period moves on screen belong to: window | period start | portfolio.
    QString moves_key() const;
    void export_csv();
    void export_document(bool pdf);
    QString report_html() const;
    QDate period_start() const;

    pfmt::PortfolioNoticeBar* notice_ = nullptr;
    QButtonGroup* window_group_ = nullptr;
    QPushButton* moves_btn_ = nullptr;
    QPushButton* csv_btn_ = nullptr;
    QPushButton* html_btn_ = nullptr;
    QPushButton* pdf_btn_ = nullptr;
    ui::KpiStrip* kpis_ = nullptr;
    QTabWidget* tabs_ = nullptr;
    ui::BarChart* contrib_chart_ = nullptr;
    QTableWidget* contrib_table_ = nullptr;
    QTableWidget* txn_table_ = nullptr;
    QLabel* summary_note_ = nullptr;

    portfolio::PortfolioSummary summary_;
    QString currency_;
    portfolio::PerfWindow window_ = portfolio::PerfWindow::YTD;
    QHash<int, portfolio::ComputedMetrics> metrics_;
    QVector<portfolio::Transaction> ledger_;
    portfolio::LedgerResult replay_;
    QVector<Line> lines_;
    QHash<QString, double> moves_; ///< symbol -> period return (fraction), from historical_stress
    QString moves_for_;            ///< moves_key() the moves belong to
    QString moves_loading_for_;    ///< moves_key() of the download in flight
    QString moves_failed_for_;     ///< moves_key() whose download failed in this visit (RETRY reloads)
    QString moves_error_;          ///< its plain reason
    services::AnalyticsRequest moves_req_; ///< cancel token of the download in flight
    quint64 moves_seq_ = 0;                ///< download in flight (0 = none); other replies are dropped
    quint64 moves_next_seq_ = 0;
    QTimer* moves_timer_ = nullptr; ///< debounce of the automatic load
    ui::InputMemory* inputs_ = nullptr;
    double realised_ = 0, income_ = 0, fees_ = 0, taxes_ = 0;
    int unknown_fx_rows_ = 0;
    bool has_data_ = false;
};

} // namespace fincept::screens
