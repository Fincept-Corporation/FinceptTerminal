#pragma once

#include <QDialog>
#include <QEvent>

class QSpinBox;
class QDoubleSpinBox;
class QComboBox;
class QLabel;

namespace fincept::ui {

class IndicatorParamDialog : public QDialog {
    Q_OBJECT
public:
    explicit IndicatorParamDialog(const QString& indicator_id, QWidget* parent = nullptr);

    int period() const;
    double std_dev() const;
    QString pivot_type() const;

protected:
    void changeEvent(QEvent* event) override;

private:
    void setup_ema_ui();
    void setup_bollinger_ui();
    void setup_pivot_ui();

    /// Re-apply tr() lookups to every widget whose text we keep a handle to.
    /// Called from changeEvent() on QEvent::LanguageChange.
    void retranslateUi();

    QString id_;
    QSpinBox* period_spin_ = nullptr;
    QDoubleSpinBox* std_spin_ = nullptr;
    QComboBox* pivot_combo_ = nullptr;

    // Form labels kept so language switch can re-apply them.
    QLabel* period_label_ = nullptr;
    QLabel* std_label_ = nullptr;
    QLabel* pivot_label_ = nullptr;
};

} // namespace fincept::ui
