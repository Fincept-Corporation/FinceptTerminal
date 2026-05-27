// src/ui/widgets/algo/RiskManagementPanel.h
#pragma once
#include <QDoubleSpinBox>
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

    void set_values(double sl, double tp, double ts, double qty, double mov);

signals:
    void values_changed();

private:
    struct SliderRow {
        QLabel* label = nullptr;
        QSlider* slider = nullptr;
        QDoubleSpinBox* spin = nullptr;
    };

    SliderRow create_row(const QString& label_text, double min_val, double max_val,
                         double default_val, int decimals, QWidget* parent);

    SliderRow stop_loss_;
    SliderRow take_profit_;
    SliderRow trailing_stop_;
    QDoubleSpinBox* quantity_spin_ = nullptr;
    QDoubleSpinBox* max_order_spin_ = nullptr;
};

} // namespace fincept::ui::algo
