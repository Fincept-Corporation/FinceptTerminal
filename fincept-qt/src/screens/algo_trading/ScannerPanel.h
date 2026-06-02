// src/screens/algo_trading/ScannerPanel.h
#pragma once
#include "services/algo_trading/AlgoTradingTypes.h"

#include <QComboBox>
#include <QEvent>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

/// Market scanner — find symbols matching condition rules.
class ScannerPanel : public QWidget {
    Q_OBJECT
  public:
    explicit ScannerPanel(QWidget* parent = nullptr);

  private slots:
    void on_scan();
    void on_scan_result(const QJsonObject& data);
    void on_error(const QString& context, const QString& msg);

  protected:
    void changeEvent(QEvent* event) override;

  private:
    void build_ui();
    void connect_service();
    void apply_preset(int index);
    void retranslateUi();

    QVBoxLayout*  conditions_layout_ = nullptr;
    QTextEdit*    symbols_edit_      = nullptr;
    QComboBox*    timeframe_combo_   = nullptr;
    QSpinBox*     lookback_spin_     = nullptr;
    QComboBox*    preset_combo_      = nullptr;
    QComboBox*    logic_combo_       = nullptr;
    QComboBox*    data_source_combo_ = nullptr;
    QComboBox*    account_combo_     = nullptr;
    QTableWidget* results_table_     = nullptr;
    QLabel*       status_label_      = nullptr;

    // Static text-bearing widgets cached for retranslateUi.
    QLabel*      cond_title_   = nullptr;
    QLabel*      preset_lbl_   = nullptr;
    QLabel*      logic_lbl_    = nullptr;
    QPushButton* add_cond_btn_ = nullptr;
    QLabel*      sym_title_    = nullptr;
    QLabel*      sym_lbl_      = nullptr;
    QLabel*      tf_lbl_       = nullptr;
    QLabel*      lb_lbl_       = nullptr;
    QLabel*      ds_lbl_       = nullptr;
    QLabel*      acct_lbl_     = nullptr;
    QPushButton* scan_btn_     = nullptr;
    QLabel*      results_title_ = nullptr;
};

} // namespace fincept::screens
