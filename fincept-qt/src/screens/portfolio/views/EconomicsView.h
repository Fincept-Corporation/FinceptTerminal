// src/screens/portfolio/views/EconomicsView.h
#pragma once
#include "screens/portfolio/PortfolioTypes.h"

#include <QLabel>
#include <QTableWidget>
#include <QWidget>

namespace fincept::screens {

/// Economics view showing macro factor exposure and portfolio correlation to economic indicators.
class EconomicsView : public QWidget {
    Q_OBJECT
  public:
    explicit EconomicsView(QWidget* parent = nullptr);

    void set_data(const portfolio::PortfolioSummary& summary, const QString& currency);

  private:
    void build_ui();
    void update_indicators();
    void update_sensitivity();

    // Macro indicators table
    QTableWidget* indicators_table_ = nullptr;

    // Sensitivity matrix
    QTableWidget* sensitivity_table_ = nullptr;

    portfolio::PortfolioSummary summary_;
    QString currency_;
};

} // namespace fincept::screens
