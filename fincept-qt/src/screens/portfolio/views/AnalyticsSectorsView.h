// src/screens/portfolio/views/AnalyticsSectorsView.h
#pragma once
#include "screens/portfolio/PortfolioTypes.h"
#include "services/portfolio/PortfolioAllocation.h"

#include <QHash>
#include <QWidget>

class QComboBox;
class QLabel;
class QPushButton;
class QTableWidget;
class QTabWidget;

namespace fincept::ui {
class DonutChart;
class KpiStrip;
class MatrixHeatmap;
class Treemap;
} // namespace fincept::ui

namespace fincept::screens::pfmt {
class PortfolioNoticeBar;
}

namespace fincept::screens {

/// ANALYTICS — allocation and concentration.
///
///   ALLOCATION: a dimension switch (sector, industry, asset class, country,
///     region, currency, exchange, market cap) driving a donut, a sortable table
///     with weight bars and a treemap of the holdings grouped by that dimension;
///     concentration tiles (positions, effective N, HHI, top-1/3/5/10).
///   CORRELATION: the clustered correlation matrix of EVERY holding, coloured for
///     diversification, with the average / most / least correlated pair.
class AnalyticsSectorsView : public QWidget {
    Q_OBJECT
  public:
    explicit AnalyticsSectorsView(QWidget* parent = nullptr);

    void set_data(const portfolio::PortfolioSummary& summary, const QString& currency);
    /// Pearson correlation keyed "SYMBOL_A|SYMBOL_B".
    void set_correlation(const QHash<QString, double>& matrix);

  protected:
    void changeEvent(QEvent* event) override;

  signals:
    /// Double-click on a sector row: filter the positions table by it.
    void sector_selected(QString sector);

  private:
    void build_ui();
    void apply_styles();
    void recompute();
    void render_allocation();
    void render_concentration();
    void render_correlation();
    void export_csv();
    portfolio::AllocationDimension dimension() const;

    pfmt::PortfolioNoticeBar* notice_ = nullptr;
    QComboBox* dim_cb_ = nullptr;
    QPushButton* export_btn_ = nullptr;
    ui::KpiStrip* kpis_ = nullptr;
    QTabWidget* tabs_ = nullptr;
    ui::DonutChart* donut_ = nullptr;
    QTableWidget* table_ = nullptr;
    ui::Treemap* treemap_ = nullptr;
    ui::MatrixHeatmap* corr_ = nullptr;
    QLabel* corr_summary_ = nullptr;

    portfolio::PortfolioSummary summary_;
    portfolio::AllocationReport report_;
    QString currency_;
    QHash<QString, double> correlation_;
    QString corr_error_;     ///< last analytics failure for this book ("" = none)
    QString corr_shown_sig_; ///< what the matrix / its state currently shows (skip identical re-renders)
    bool has_data_ = false;
};

} // namespace fincept::screens
