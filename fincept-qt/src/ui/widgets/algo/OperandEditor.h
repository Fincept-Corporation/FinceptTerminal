// src/ui/widgets/algo/OperandEditor.h
#pragma once
#include "services/algo_trading/AlgoTradingTypes.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QVector>
#include <QWidget>

namespace fincept::ui::algo {

/// One side of a comparison. Symmetric: the same widget builds the left operand
/// and the right operand of a condition. An operand is either an indicator
/// reference (id + per-indicator params + named output field + bar offset) or,
/// when `allow_value` is set and the user picks "Value", a plain constant.
///
/// Parameter spin-boxes and the output/field dropdown are built dynamically from
/// the selected indicator's catalog entry (algo_indicators()); the field combo
/// is shown only for multi-output indicators.
class OperandEditor : public QWidget {
    Q_OBJECT
public:
    explicit OperandEditor(bool allow_value, QWidget* parent = nullptr);

    bool is_value_mode() const;
    double value() const;
    QString indicator_id() const;
    QJsonObject params() const;
    QString field() const;
    int offset() const;

    void set_value_mode(bool on);
    void set_value(double v);
    void set_indicator(const QString& id);
    void set_params(const QJsonObject& p);
    void set_field(const QString& f);
    void set_offset(int o);

signals:
    void changed();

protected:
    void changeEvent(QEvent* event) override;

private:
    void build_ui();
    void populate_indicators();
    void on_mode_changed();
    void on_indicator_changed();
    void rebuild_params();
    services::algo::IndicatorDef current_def() const;
    void apply_mode_visibility();

    /// Re-apply tr() lookups to every widget whose text we keep a handle to.
    /// Called from changeEvent() on QEvent::LanguageChange.
    void retranslateUi();

    bool allow_value_;

    QComboBox* mode_combo_ = nullptr; // only present when allow_value_
    QComboBox* indicator_combo_ = nullptr;
    QWidget* params_container_ = nullptr;
    QHBoxLayout* params_layout_ = nullptr;
    QVector<QDoubleSpinBox*> param_spins_;
    QVector<QString> param_names_;
    QComboBox* field_combo_ = nullptr;
    // Bar-offset is an advanced, opt-in control: hidden until the user clicks the
    // small "⋯ bar" toggle, then a plain-language picker ("Current bar / 1 bar ago").
    QPushButton* offset_btn_ = nullptr;
    QComboBox* offset_combo_ = nullptr;
    QDoubleSpinBox* value_spin_ = nullptr;
};

} // namespace fincept::ui::algo
