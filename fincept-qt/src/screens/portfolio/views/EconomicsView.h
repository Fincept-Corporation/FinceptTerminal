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

  protected:
    void changeEvent(QEvent* event) override;

  private:
    void build_ui();
    void retranslateUi();
    void update_indicators();
    void update_sensitivity();

    // Section titles/notes
    QLabel* ind_title_ = nullptr;
    QLabel* ind_note_ = nullptr;
    QLabel* sens_title_ = nullptr;
    QLabel* sens_note_ = nullptr;

    // Macro indicators table
    QTableWidget* indicators_table_ = nullptr;

    // Sensitivity matrix
    QTableWidget* sensitivity_table_ = nullptr;

    portfolio::PortfolioSummary summary_;
    QString currency_;
    bool has_data_ = false;
};

} // namespace fincept::screens
