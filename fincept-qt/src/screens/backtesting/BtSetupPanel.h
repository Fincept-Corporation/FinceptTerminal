// src/screens/backtesting/BtSetupPanel.h
//
// The inputs of the Backtesting screen's ONE setup (the same for every analysis):
//   BtInputStrip   the band under the command bar: symbols (chips + ui::SymbolSearchBox), period
//                  (start / end), interval, capital (account currency), benchmark, and a slot on the
//                  right for the screen's run state;
//   BtSetupPanel   the collapsible left panel: STRATEGY (picker, description, parameters generated from
//                  the engine's catalogue with live validation, the custom-combo builder), SIZING & RISK,
//                  COSTS & EXECUTION (cost preset, commission, slippage, fill timing, risk-free source and
//                  rate), DATA (synthetic-data switch, adjustment, source) — inputs an engine does not apply
//                  are disabled with the reason;
//   BtStrategyPicker  the F4 popup: search over every strategy of the engine, unsupported ones listed with
//                  their reason (nothing hidden), engine counts ("418 listed · 189 supported · 185 runnable
//                  here — install keras to enable …").
// Everything is remembered through ui::InputMemory("backtesting", ...): the shared "setup" document (owned
// by the screen), "engine/<slug>" (last strategy) and "params/<slug>/<strategy>" (parameter values).
// Styling: the screen's root stylesheet (object names #btSetup*, #btField*, #btSection*; no setStyleSheet).
#pragma once

#include "services/backtesting/BacktestRequest.h"
#include "services/backtesting/BacktestingTypes.h"

#include <QDate>
#include <QFrame>
#include <QHash>
#include <QJsonObject>
#include <QPointer>
#include <QScrollArea>
#include <QStringList>
#include <QVector>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QDateEdit;
class QDoubleSpinBox;
class QGridLayout;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QListView;
class QPushButton;
class QSortFilterProxyModel;
class QSpinBox;
class QStandardItemModel;
class QToolButton;
class QVBoxLayout;

namespace fincept::ui {
class InputMemory;
class SymbolSearchBox;
} // namespace fincept::ui

namespace fincept::screens::bt {

/// The strategy picker popup.
class BtStrategyPicker : public QFrame {
    Q_OBJECT
  public:
    explicit BtStrategyPicker(QWidget* parent = nullptr);
    void set_catalog(const QString& engine_label, const services::backtest::StrategyCatalog& catalog,
                     const QString& current_id);
    /// Show under `anchor` (or centred over the window), search box focused.
    void popup_at(QWidget* anchor);

  signals:
    void picked(const QString& strategy_id);

  protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

  private:
    void apply_filter(const QString& text);
    void pick_current();

    QLineEdit* search_ = nullptr;
    QLabel* counts_ = nullptr;
    QListView* list_ = nullptr;
    QStandardItemModel* model_ = nullptr;
    QSortFilterProxyModel* proxy_ = nullptr;
};

/// The band under the command bar.
class BtInputStrip : public QWidget {
    Q_OBJECT
  public:
    explicit BtInputStrip(ui::InputMemory* memory, QWidget* parent = nullptr);

    QStringList symbols() const;
    void set_symbols(const QStringList& symbols);
    QDate start() const;
    QDate end() const;
    void set_period(const QDate& start, const QDate& end);
    /// "1Y" | "3Y" | "5Y" | "10Y" | "MAX": end = yesterday, start from it.
    void apply_range_preset(const QString& preset);
    QString range_preset() const { return preset_; }
    QString interval() const;
    void set_interval(const QString& interval);
    double capital() const;
    void set_capital(double capital);
    QString benchmark() const;
    /// `user_set` = the user chose it (regional defaults never overwrite it then).
    void set_benchmark(const QString& symbol, bool user_set);
    bool benchmark_user_set() const { return benchmark_user_set_; }
    /// Account currency shown on the capital field.
    void set_currency(const QString& ccy);
    /// Engines that test the first symbol only: later chips are dimmed with the reason.
    void set_multi_symbol(bool supported, const QString& engine_label);
    /// Intervals other than 1d disabled with the reason when the engine has no intraday support.
    void set_intraday(bool supported, const QString& engine_label);
    /// A slot at the right end for the run state (QHBoxLayout).
    QWidget* state_area() const { return state_area_; }
    void focus_symbols();
    void focus_benchmark();
    /// Start >= end: both date edits are marked and the reason is returned (empty when fine).
    QString period_error() const;

  signals:
    void changed();
    void symbols_changed();
    void range_preset_changed(const QString& preset);
    void unresolved(const QString& message);

  private:
    void add_symbol(const QString& symbol);
    void remove_symbol(const QString& symbol);
    void rebuild_chips();
    void persist_symbols();
    void mark_period();

    ui::InputMemory* memory_ = nullptr;
    QWidget* chips_box_ = nullptr;
    QHBoxLayout* chips_layout_ = nullptr;
    ui::SymbolSearchBox* symbol_box_ = nullptr;
    QDateEdit* start_edit_ = nullptr;
    QDateEdit* end_edit_ = nullptr;
    QComboBox* interval_combo_ = nullptr;
    QDoubleSpinBox* capital_spin_ = nullptr;
    QLabel* capital_ccy_ = nullptr;
    ui::SymbolSearchBox* benchmark_box_ = nullptr;
    QWidget* state_area_ = nullptr;
    QStringList symbols_;
    QString preset_ = QStringLiteral("5Y");
    QString ccy_;
    QString engine_label_;
    bool multi_symbol_ = true;
    bool benchmark_user_set_ = false;
    bool updating_ = false;
};

/// The collapsible left panel.
class BtSetupPanel : public QScrollArea {
    Q_OBJECT
  public:
    explicit BtSetupPanel(ui::InputMemory* memory, QWidget* parent = nullptr);
    ~BtSetupPanel() override;

    /// Select the engine: gating of unsupported inputs; the strategy list follows set_catalog().
    void set_engine(const QString& slug, const QString& display_name);
    QString engine() const { return engine_; }
    /// The current engine's get_capabilities reply (empty = the built-in table): re-gates the inputs.
    void set_engine_capabilities(const QJsonObject& caps);
    /// The catalogue of the current engine arrived: the remembered (or first runnable) strategy is selected.
    void set_catalog(const services::backtest::StrategyCatalog& catalog);
    bool catalog_ready() const { return catalog_ready_; }
    const services::backtest::StrategyCatalog& catalog() const { return catalog_; }
    /// Select a strategy of the current catalogue (remembered per engine).
    void select_strategy(const QString& id);
    const services::backtest::Strategy* strategy() const;
    QVector<services::backtest::StrategyParam> params_schema() const;
    /// The current parameter values (typed).
    QJsonObject param_values() const;

    /// Write the panel's part of the setup (strategy, params, sizing, costs, execution, synthetic).
    void fill_setup(services::backtest::BtSetup& setup) const;
    /// LOAD INPUTS of a saved run: costs, execution, sizing, strategy + its parameters.
    void apply_setup(const services::backtest::BtSetup& setup);
    /// APPLY of an optimised parameter set.
    void apply_params(const QJsonObject& params);
    /// The venue's default cost preset, unless the user picked one.
    void set_venue_cost_default(const QString& preset_id);

    /// Empty when the setup can run; else the first problem (parameter rule, unsupported strategy).
    QString validation_error() const;
    QString strategy_label() const;
    QString cost_short() const;
    QString fill_short() const;
    QString risk_free_short() const;
    QString sizing_short() const;
    /// Inputs of the panel that differ from their defaults.
    int changed_count() const;

    void open_picker(QWidget* anchor);
    /// Expand and scroll to a section: "strategy" | "sizing" | "costs" | "data".
    void reveal_section(const QString& id);

  signals:
    void changed();
    void strategy_changed();

  private:
    struct ParamEditor {
        services::backtest::StrategyParam param;
        QWidget* row = nullptr;
        QLabel* label = nullptr;
        QWidget* editor = nullptr;
        QToolButton* reset = nullptr;
    };
    struct ComboRow {
        QComboBox* type = nullptr;
        QSpinBox* period = nullptr;
        QComboBox* cond = nullptr;
        QDoubleSpinBox* value = nullptr;
    };
    QWidget* add_section(const QString& id, const QString& title, QVBoxLayout** body);
    QWidget* field_row(QWidget* parent, const QString& label, QWidget* editor, QLabel** label_out = nullptr);
    void rebuild_params();
    void build_combo_builder(QVBoxLayout* into);
    double param_value(const ParamEditor& e) const;
    void set_param_value(const ParamEditor& e, const QJsonValue& v);
    void update_param_marks();
    void update_cost_fields();
    void update_support();
    void update_strategy_header();
    void emit_changed();

    ui::InputMemory* memory_ = nullptr;             ///< the screen's "setup" document
    ui::InputMemory* engine_memory_ = nullptr;      ///< "engine/<slug>"
    ui::InputMemory* params_memory_ = nullptr;      ///< "params/<slug>/<strategy>"
    QString engine_;
    QString engine_label_;
    QJsonObject engine_caps_;                       ///< get_capabilities of engine_ (empty = built-in table)
    services::backtest::StrategyCatalog catalog_;
    bool catalog_ready_ = false;
    QString strategy_id_;
    QJsonObject pending_params_;                    ///< applied once the strategy's editors exist

    QWidget* body_ = nullptr;
    QVBoxLayout* body_layout_ = nullptr;
    QHash<QString, QWidget*> section_bodies_;
    QHash<QString, QToolButton*> section_heads_;
    // STRATEGY
    QPushButton* strategy_btn_ = nullptr;
    QLabel* strategy_desc_ = nullptr;
    QLabel* strategy_note_ = nullptr;   ///< unsupported reason / catalogue counts
    QWidget* params_box_ = nullptr;
    QVBoxLayout* params_layout_ = nullptr;
    QLabel* params_error_ = nullptr;
    QPushButton* params_reset_ = nullptr;
    QVector<ParamEditor> editors_;
    ComboRow combo_rows_[3];
    QComboBox* combo_logic_ = nullptr;
    QCheckBox* combo_third_ = nullptr;
    BtStrategyPicker* picker_ = nullptr;
    // SIZING & RISK
    QComboBox* sizing_combo_ = nullptr;
    QDoubleSpinBox* sizing_value_ = nullptr;
    QLabel* sizing_value_label_ = nullptr;
    QDoubleSpinBox* leverage_spin_ = nullptr;
    QCheckBox* short_check_ = nullptr;
    QDoubleSpinBox* stop_spin_ = nullptr;
    QDoubleSpinBox* take_spin_ = nullptr;
    QLabel* sizing_note_ = nullptr;
    // COSTS & EXECUTION
    QComboBox* cost_combo_ = nullptr;
    QDoubleSpinBox* commission_spin_ = nullptr;
    QDoubleSpinBox* slippage_spin_ = nullptr;
    QLabel* cost_detail_ = nullptr;
    QComboBox* fill_combo_ = nullptr;
    QLabel* fill_detail_ = nullptr;
    QComboBox* rf_source_combo_ = nullptr;
    QDoubleSpinBox* rf_spin_ = nullptr;
    QLabel* costs_note_ = nullptr;
    // DATA
    QCheckBox* synthetic_check_ = nullptr;
    QLabel* data_note_ = nullptr;
    // footer
    QLabel* changed_label_ = nullptr;
    QPushButton* reset_all_ = nullptr;
    bool cost_user_set_ = false;
    bool updating_ = false;
    QObject* wheel_guard_ = nullptr; ///< unfocused spin boxes / combos do not take the wheel
};

} // namespace fincept::screens::bt
