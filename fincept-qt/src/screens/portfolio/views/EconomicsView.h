// src/screens/portfolio/views/EconomicsView.h
#pragma once
#include "screens/portfolio/PortfolioTypes.h"

#include <QHash>
#include <QLabel>
#include <QString>
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
    void fetch_macro();        // pull live FRED macro levels (async)
    void update_macro_table(); // render macro_values_ into macro_table_

    // Section titles/notes
    QLabel* ind_title_ = nullptr;
    QLabel* ind_note_ = nullptr;
    QLabel* sens_title_ = nullptr;
    QLabel* sens_note_ = nullptr;

    // Macro indicators table
    QTableWidget* indicators_table_ = nullptr;

    // Sensitivity matrix
    QTableWidget* sensitivity_table_ = nullptr;

    // Live macro conditions (FRED)
    QLabel* macro_title_ = nullptr;
    QLabel* macro_note_ = nullptr;
    QPushButton* macro_set_key_btn_ = nullptr; // shown when no FRED key configured
    QTableWidget* macro_table_ = nullptr;
    QHash<QString, double> macro_values_;     // series_id -> latest value
    QHash<QString, QString> macro_dates_;      // series_id -> as-of date
    QString macro_status_;                     // "" ok, else error/hint
    bool macro_loading_ = false;
    bool macro_loaded_ = false;

    portfolio::PortfolioSummary summary_;
    QString currency_;
    bool has_data_ = false;
};

} // namespace fincept::screens
