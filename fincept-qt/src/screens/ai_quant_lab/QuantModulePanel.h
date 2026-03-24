// src/screens/ai_quant_lab/QuantModulePanel.h
#pragma once
#include "services/ai_quant_lab/AIQuantLabTypes.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QHash>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

/// Dynamic panel for a single AI Quant Lab module.
/// Builds input forms and result display based on module type.
class QuantModulePanel : public QWidget {
    Q_OBJECT
  public:
    explicit QuantModulePanel(const fincept::services::quant::QuantModule& mod, QWidget* parent = nullptr);

  private slots:
    void on_result(const QString& module_id, const QString& command, const QJsonObject& data);
    void on_error(const QString& module_id, const QString& message);

  private:
    void build_ui();
    void connect_service();

    // Module-specific builders
    QWidget* build_generic_panel();
    QWidget* build_gs_quant_panel();
    QWidget* build_cfa_quant_panel();
    QWidget* build_backtesting_panel();
    QWidget* build_rl_trading_panel();
    QWidget* build_advanced_models_panel();

    // Helpers
    QWidget* build_input_row(const QString& label, QWidget* input, QWidget* parent);
    QDoubleSpinBox* make_double_spin(double min, double max, double val, int decimals, const QString& suffix,
                                     QWidget* parent);
    QPushButton* make_run_button(const QString& text, QWidget* parent);
    void display_result(const QJsonObject& data);
    void display_error(const QString& msg);
    void clear_results();

    fincept::services::quant::QuantModule module_;
    QVBoxLayout* results_layout_ = nullptr;
    QLabel* status_label_ = nullptr;

    QHash<QString, QDoubleSpinBox*> double_inputs_;
    QHash<QString, QLineEdit*> text_inputs_;
    QHash<QString, QComboBox*> combo_inputs_;
    QHash<QString, QSpinBox*> int_inputs_;
};

} // namespace fincept::screens
