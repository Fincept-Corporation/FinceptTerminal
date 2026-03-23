// src/screens/portfolio/views/AnalyticsSectorsView.h
#pragma once
#include "screens/portfolio/PortfolioTypes.h"

#include <QChartView>
#include <QLabel>
#include <QTabWidget>
#include <QTableWidget>
#include <QWidget>

namespace fincept::screens {

/// Analytics & Sectors detail view with 3 sub-tabs: Overview, Analytics, Correlation.
class AnalyticsSectorsView : public QWidget {
    Q_OBJECT
  public:
    explicit AnalyticsSectorsView(QWidget* parent = nullptr);

    void set_data(const portfolio::PortfolioSummary& summary, const QString& currency);

  private:
    void build_ui();
    void update_overview();
    void update_analytics();
    void update_correlation();

    QTabWidget* tabs_ = nullptr;

    // Overview tab
    QChartView*   sector_chart_    = nullptr;
    QTableWidget* sector_table_    = nullptr;

    // Analytics tab
    QWidget*      analytics_panel_ = nullptr;

    // Correlation tab
    QWidget*      corr_panel_      = nullptr;

    // Data
    portfolio::PortfolioSummary summary_;
    QString currency_;

    // Sector helpers
    struct SectorInfo {
        QString name;
        double  weight = 0;
        double  pnl    = 0;
        int     count  = 0;
        QVector<portfolio::HoldingWithQuote> holdings;
    };
    QVector<SectorInfo> compute_sectors() const;
    static QString infer_sector(const QString& symbol);
    static QColor sector_color(int index);
};

} // namespace fincept::screens
