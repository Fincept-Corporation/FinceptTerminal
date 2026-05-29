// src/screens/portfolio/views/AnalyticsSectorsView.h
#pragma once
#include "screens/portfolio/PortfolioTypes.h"

#include <QColor>
#include <QHash>
#include <QWidget>

class QChartView;
class QGridLayout;
class QLabel;
class QTableWidget;
class QTabWidget;
class QVBoxLayout;

namespace fincept::screens {

/// Analytics & Sectors detail view.
///
/// Two sub-tabs:
///   - OVERVIEW: KPI strip, donut + sector table, 4 performer cards, HHI +
///               top-3 concentration badges. Everything the user needs to
///               reason about sector exposure on one page.
///   - CORRELATION: symbol×symbol Pearson correlation matrix computed from
///                  real trailing-30-day daily returns (PortfolioService).
class AnalyticsSectorsView : public QWidget {
    Q_OBJECT
  public:
    explicit AnalyticsSectorsView(QWidget* parent = nullptr);

    void set_data(const portfolio::PortfolioSummary& summary, const QString& currency);
    /// Real Pearson correlation matrix keyed "SYMBOL_A|SYMBOL_B", supplied by
    /// PortfolioService::correlation_computed. Triggers a redraw of the matrix.
    void set_correlation(const QHash<QString, double>& matrix);

  protected:
    void changeEvent(QEvent* event) override;

  signals:
    /// Fired when the user clicks a sector row / donut slice / performer card.
    /// Empty string = clear filter.
    void sector_selected(QString sector);

  private:
    struct SectorInfo {
        QString name;
        double weight = 0;        // percent of portfolio (0..100)
        double market_value = 0;  // in portfolio currency
        double cost_basis = 0;
        double pnl = 0;
        double pnl_percent = 0;
        int count = 0;
        QVector<portfolio::HoldingWithQuote> holdings;
    };

    void build_ui();
    void retranslateUi();
    QWidget* build_overview_tab();
    QWidget* build_correlation_tab();
    void update_overview();
    void update_correlation();
    void update_kpi_strip(const QVector<SectorInfo>& sectors);
    void update_donut(const QVector<SectorInfo>& sectors);
    void update_sector_table(const QVector<SectorInfo>& sectors);
    void update_performers(const QVector<SectorInfo>& sectors);
    void update_concentration(const QVector<SectorInfo>& sectors);

    QVector<SectorInfo> compute_sectors() const;
    static QColor sector_color(int index);
    QString format_money(double v) const;
    static QString format_pct(double v, bool signed_ = false);

    // Layout
    QTabWidget* tabs_ = nullptr;
    int overview_tab_index_ = -1;
    int correlation_tab_index_ = -1;

    // KPI strip
    QLabel* kpi_sectors_ = nullptr;
    QLabel* kpi_positions_ = nullptr;
    QLabel* kpi_market_value_ = nullptr;
    QLabel* kpi_pnl_ = nullptr;
    QLabel* kpi_sectors_label_ = nullptr;
    QLabel* kpi_positions_label_ = nullptr;
    QLabel* kpi_market_value_label_ = nullptr;
    QLabel* kpi_pnl_label_ = nullptr;

    // Section titles
    QLabel* donut_title_ = nullptr;
    QLabel* table_title_ = nullptr;
    QLabel* corr_title_ = nullptr;

    // Donut + legend
    QChartView* donut_view_ = nullptr;
    QLabel* donut_center_ = nullptr;
    QWidget* legend_container_ = nullptr;

    // Sector table
    QTableWidget* sector_table_ = nullptr;

    // Performer cards
    QWidget* performers_row_ = nullptr;

    // Concentration
    QWidget* concentration_row_ = nullptr;

    // Correlation
    QWidget* corr_panel_ = nullptr;
    QGridLayout* corr_grid_ = nullptr;
    QLabel* corr_note_ = nullptr;

    // Data
    portfolio::PortfolioSummary summary_;
    QString currency_;
    QHash<QString, double> correlation_matrix_; // keyed "SYM_A|SYM_B"
    bool has_data_ = false;
};

} // namespace fincept::screens
