// src/screens/algo_trading/ScannerPanel.h
#pragma once
#include "services/algo_trading/AlgoTradingTypes.h"

#include <QComboBox>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
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

  private:
    void build_ui();
    void connect_service();
    void apply_preset(int index);

    QVBoxLayout* conditions_layout_ = nullptr;
    QTextEdit* symbols_edit_ = nullptr;
    QComboBox* timeframe_combo_ = nullptr;
    QSpinBox* lookback_spin_ = nullptr;
    QComboBox* preset_combo_ = nullptr;
    QVBoxLayout* results_layout_ = nullptr;
    QLabel* status_label_ = nullptr;
};

} // namespace fincept::screens
