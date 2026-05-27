#pragma once

#include <QDialog>

class QSpinBox;
class QDoubleSpinBox;
class QComboBox;

namespace fincept::ui {

class IndicatorParamDialog : public QDialog {
    Q_OBJECT
public:
    explicit IndicatorParamDialog(const QString& indicator_id, QWidget* parent = nullptr);

    int period() const;
    double std_dev() const;
    QString pivot_type() const;

private:
    void setup_ema_ui();
    void setup_bollinger_ui();
    void setup_pivot_ui();

    QString id_;
    QSpinBox* period_spin_ = nullptr;
    QDoubleSpinBox* std_spin_ = nullptr;
    QComboBox* pivot_combo_ = nullptr;
};

} // namespace fincept::ui
