// src/screens/portfolio/PortfolioSectorPanel.h
#pragma once
#include "screens/portfolio/PortfolioTypes.h"

#include <QChartView>
#include <QLabel>
#include <QWidget>

namespace fincept::screens {

/// Sector allocation donut chart + NxN correlation matrix.
class PortfolioSectorPanel : public QWidget {
    Q_OBJECT
  public:
    explicit PortfolioSectorPanel(QWidget* parent = nullptr);

    void set_holdings(const QVector<portfolio::HoldingWithQuote>& holdings);
    void set_currency(const QString& currency);

  private:
    void build_ui();
    void update_donut();
    void update_correlation();

    // Donut chart
    QChartView* donut_view_ = nullptr;
    QWidget* legend_widget_ = nullptr;

    // Correlation matrix
    QWidget* corr_widget_ = nullptr;

    // Data
    QVector<portfolio::HoldingWithQuote> holdings_;
    QString currency_ = "USD";

    // Sector color palette
    static QColor sector_color(int index);
    static QString infer_sector(const QString& symbol);
};

} // namespace fincept::screens
