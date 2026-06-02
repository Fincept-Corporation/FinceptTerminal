// src/ui/widgets/algo/ConditionBlock.h
#pragma once
#include "services/algo_trading/AlgoTradingTypes.h"
#include "ui/widgets/algo/OperandEditor.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFrame>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>

namespace fincept::ui::algo {

/// A single comparison leaf: `LHS  <operator>  RHS`.
/// LHS is always an indicator operand; RHS is an indicator-or-value operand,
/// and is hidden for the unary `rising` / `falling` operators. Serializes to the
/// flat comparison schema the ConditionEvaluator understands (indicator/params/
/// field/offset on the left; compare_* / value on the right).
class ConditionBlock : public QFrame {
    Q_OBJECT
public:
    explicit ConditionBlock(bool is_entry, QWidget* parent = nullptr);

    QJsonObject to_json() const;
    void from_json(const QJsonObject& obj);

signals:
    void remove_requested();
    void changed();

protected:
    void changeEvent(QEvent* event) override;

private:
    void build_ui();
    void populate_operators();
    void on_operator_changed();

    /// Re-apply tr() lookups to every widget whose text we keep a handle to.
    /// Called from changeEvent() on QEvent::LanguageChange.
    void retranslateUi();

    bool is_entry_;

    OperandEditor* lhs_ = nullptr;
    QComboBox* operator_combo_ = nullptr;
    OperandEditor* rhs_ = nullptr;
    // `between` uses two constants (low..high) instead of the RHS operand.
    QDoubleSpinBox* between_low_ = nullptr;
    QLabel* between_and_ = nullptr;
    QDoubleSpinBox* between_high_ = nullptr;
    QPushButton* remove_btn_ = nullptr;
};

} // namespace fincept::ui::algo
