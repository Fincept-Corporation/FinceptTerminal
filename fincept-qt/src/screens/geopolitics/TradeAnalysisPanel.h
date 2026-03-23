// src/screens/geopolitics/TradeAnalysisPanel.h
#pragma once
#include <QDoubleSpinBox>
#include <QJsonObject>
#include <QLabel>
#include <QTabWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

/// Trade geopolitics analysis — benefits/costs, restrictions, sanctions impact.
class TradeAnalysisPanel : public QWidget {
    Q_OBJECT
  public:
    explicit TradeAnalysisPanel(QWidget* parent = nullptr);

  private slots:
    void on_trade_result(const QString& context, const QJsonObject& data);
    void on_error(const QString& context, const QString& message);

  private:
    void build_ui();
    void connect_service();
    void display_result(const QJsonObject& data);

    QTabWidget* tabs_ = nullptr;
    QVBoxLayout* results_layout_ = nullptr;
    QWidget* results_container_ = nullptr;
    QLabel* status_label_ = nullptr;
};

} // namespace fincept::screens
