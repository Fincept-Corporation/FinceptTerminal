// src/ui/widgets/algo/RiskManagementPanel.h
#pragma once
#include <QDoubleSpinBox>
#include <QEvent>
#include <QLabel>
#include <QSlider>
#include <QWidget>

namespace fincept::ui::algo {

class RiskManagementPanel : public QWidget {
    Q_OBJECT
public:
    explicit RiskManagementPanel(QWidget* parent = nullptr);

    double stop_loss() const;
    double take_profit() const;
    double trailing_stop() const;
    double quantity() const;
    double max_order_value() const;
    double capital_pct() const; // % of capital allocated per backtest entry

    void set_values(double sl, double tp, double ts, double qty, double mov, double capital_pct = 100.0);

signals:
    void values_changed();

protected:
    void changeEvent(QEvent* event) override;

private:
    struct SliderRow {
        QLabel* label = nullptr;
        QSlider* slider = nullptr;
        QDoubleSpinBox* spin = nullptr;
    };

    SliderRow create_row(const QString& label_text, double min_val, double max_val,
                         double default_val, int decimals, QWidget* parent);

    /// Re-apply tr() lookups to every widget whose text we keep a handle to.
    /// Called from changeEvent() on QEvent::LanguageChange.
    void retranslateUi();

    QLabel* header_ = nullptr;
    SliderRow stop_loss_;
    SliderRow take_profit_;
    SliderRow trailing_stop_;
    SliderRow capital_pct_;
    QLabel* quantity_label_ = nullptr;
    QDoubleSpinBox* quantity_spin_ = nullptr;
    QLabel* max_order_label_ = nullptr;
    QDoubleSpinBox* max_order_spin_ = nullptr;
};

} // namespace fincept::ui::algo
