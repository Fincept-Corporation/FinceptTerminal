// src/screens/portfolio/PortfolioSectorPanel.h
#pragma once
#include "screens/portfolio/PortfolioTypes.h"

#include <QChartView>
#include <QHash>
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
    /// Feed real pairwise Pearson correlation matrix (key = "SYM1|SYM2").
    void set_correlation(const QHash<QString, double>& matrix);

    // Sector utilities — public so callers can map symbols to sectors
    static QColor sector_color(int index);
    static QString infer_sector(const QString& symbol);

  signals:
    /// Emitted when the user clicks a pie slice. Empty string = clear filter.
    void sector_selected(QString sector);

  protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

  private:
    void build_ui();
    void update_donut();
    void update_correlation();
    void on_sector_legend_clicked(const QString& sector);

    // Donut chart
    QChartView* donut_view_ = nullptr;
    QWidget* legend_widget_ = nullptr;

    // Correlation matrix
    QWidget* corr_widget_ = nullptr;

    // Data
    QVector<portfolio::HoldingWithQuote> holdings_;
    QString currency_ = "USD";
    QString selected_sector_;            // empty = no filter active
    QHash<QString, double> corr_matrix_; // real Pearson matrix, key="SYM1|SYM2"
};

} // namespace fincept::screens
