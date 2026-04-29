// src/screens/ai_quant_lab/QuantModulePanel.h
#pragma once
#include "services/ai_quant_lab/AIQuantLabTypes.h"
#include "storage/repositories/LlmProfileRepository.h"

#include <QComboBox>
#include <QDateEdit>
#include <QDoubleSpinBox>
#include <QHash>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
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
    QWidget* build_deep_agent_panel();
    QWidget* build_rd_agent_tab(QComboBox* llm_combo);
    QWidget* build_generic_panel();
    QWidget* build_gs_quant_panel();
    QWidget* build_cfa_quant_panel();
    QWidget* build_backtesting_panel();
    QWidget* build_rl_trading_panel();
    QWidget* build_advanced_models_panel();
    QWidget* build_factor_discovery_panel();
    QWidget* build_model_library_panel();
    QWidget* build_live_signals_panel();
    QWidget* build_online_learning_panel();
    QWidget* build_meta_learning_panel();
    QWidget* build_hft_panel();
    QWidget* build_rolling_retraining_panel();
    QWidget* build_feature_engineering_panel();
    QWidget* build_portfolio_opt_panel();
    QWidget* build_factor_evaluation_panel();
    QWidget* build_strategy_builder_panel();
    QWidget* build_data_processors_panel();
    QWidget* build_quant_reporting_panel();

    // Helpers
    QWidget* build_input_row(const QString& label, QWidget* input, QWidget* parent);
    QDoubleSpinBox* make_double_spin(double min, double max, double val, int decimals, const QString& suffix,
                                     QWidget* parent);
    QPushButton* make_run_button(const QString& text, QWidget* parent);
    QWidget* build_llm_picker(QWidget* parent, QComboBox** out_combo);
    QJsonObject llm_config_from_combo(QComboBox* combo) const;
    void display_result(const QJsonObject& data);
    void display_backtest_result(const QJsonObject& data);
    void display_cfa_result(const QString& command, const QJsonObject& data);
    void display_error(const QString& msg);
    void clear_results();

    void refresh_theme();

    fincept::services::quant::QuantModule module_;
    QVBoxLayout* results_layout_ = nullptr;
    QLabel* status_label_ = nullptr;
    QWidget* panel_header_ = nullptr; // the fixed header bar inside build_ui()
    QLabel* header_title_ = nullptr;
    QLabel* header_cat_ = nullptr;
    QTextEdit* agent_output_ = nullptr;     // deep_agent LangGraph tab
    QTextEdit* rd_agent_output_ = nullptr;  // rd_agent tab output
    QTableWidget* rd_task_table_ = nullptr; // rd_agent task monitor

    // RL Trading subtab — live progress UI
    class QProgressBar* rl_progress_bar_ = nullptr;
    QLabel* rl_progress_stats_ = nullptr;
    class QPlainTextEdit* rl_log_console_ = nullptr;
    QPushButton* rl_train_button_ = nullptr;

    QHash<QString, QDoubleSpinBox*> double_inputs_;
    QHash<QString, QLineEdit*> text_inputs_;
    QHash<QString, QComboBox*> combo_inputs_;
    QHash<QString, QSpinBox*> int_inputs_;
    QHash<QString, QDateEdit*> date_inputs_;
};

} // namespace fincept::screens
