// src/ui/widgets/algo/ConditionBlock.h
#pragma once
#include "services/algo_trading/AlgoTradingTypes.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QWidget>

namespace fincept::ui::algo {

class ConditionBlock : public QFrame {
    Q_OBJECT
public:
    explicit ConditionBlock(bool is_entry, QWidget* parent = nullptr);

    QJsonObject to_json() const;
    void from_json(const QJsonObject& obj);

signals:
    void remove_requested();
    void changed();

private:
    void build_ui();
    void on_indicator_changed(int index);
    void on_compare_mode_changed(int index);
    void populate_indicators();
    void populate_fields(QComboBox* combo, const QString& indicator);
    void populate_operators();
    void update_param_visibility();

    bool is_entry_;

    QComboBox* indicator_combo_ = nullptr;
    QComboBox* field_combo_ = nullptr;
    QComboBox* operator_combo_ = nullptr;
    QComboBox* compare_mode_combo_ = nullptr;
    QDoubleSpinBox* value_spin_ = nullptr;
    QComboBox* compare_indicator_combo_ = nullptr;
    QComboBox* compare_field_combo_ = nullptr;

    // Indicator parameters
    QWidget* params_container_ = nullptr;
    QDoubleSpinBox* param1_spin_ = nullptr;
    QDoubleSpinBox* param2_spin_ = nullptr;
    QDoubleSpinBox* param3_spin_ = nullptr;
    QLabel* param1_label_ = nullptr;
    QLabel* param2_label_ = nullptr;
    QLabel* param3_label_ = nullptr;

    QPushButton* remove_btn_ = nullptr;
};

} // namespace fincept::ui::algo
