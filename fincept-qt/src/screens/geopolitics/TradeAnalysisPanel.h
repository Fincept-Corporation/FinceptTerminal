// src/screens/geopolitics/TradeAnalysisPanel.h
#pragma once
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QHash>
#include <QJsonObject>
#include <QLabel>
#include <QList>
#include <QPushButton>
#include <QTabWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QVector>
#include <QWidget>

namespace fincept::screens {

/// Trade geopolitics analysis — benefits/costs, restrictions, sanctions impact.
class TradeAnalysisPanel : public QWidget {
    Q_OBJECT
  public:
    explicit TradeAnalysisPanel(QWidget* parent = nullptr);

  protected:
    void changeEvent(QEvent* event) override;

  private slots:
    void on_trade_result(const QString& context, const QJsonObject& data);
    void on_error(const QString& context, const QString& message);

  private:
    void build_ui();
    void connect_service();
    void display_result(const QJsonObject& data);
    void retranslateUi();

    QTabWidget* tabs_ = nullptr;
    QVBoxLayout* results_layout_ = nullptr;
    QWidget* results_container_ = nullptr;
    QLabel* status_label_ = nullptr;

    // Static text widgets (cached for retranslateUi).
    QLabel* title_lbl_ = nullptr;
    QComboBox* type_combo_ = nullptr;
    QList<QPushButton*> run_buttons_;
    // Page-level hint labels paired with their English source string so
    // retranslateUi can re-apply them. (Per-field captions built via make_field
    // are translated at construction time.) Filled during build_ui().
    QVector<QPair<QLabel*, QString>> i18n_labels_;
};

} // namespace fincept::screens
