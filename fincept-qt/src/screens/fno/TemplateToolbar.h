#pragma once
#include "services/options/StrategyTemplates.h"
#include <QComboBox>
#include <QEvent>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QWidget>

namespace fincept::screens::fno {

class TemplateToolbar : public QWidget {
    Q_OBJECT
public:
    explicit TemplateToolbar(QWidget* parent = nullptr);

signals:
    void template_chosen(const QString& template_id,
                         const fincept::services::options::StrategyInstantiationOptions& opts);
    void add_leg_requested();

protected:
    void changeEvent(QEvent* event) override;

private slots:
    void on_use_clicked();

private:
    void retranslateUi();

    QLabel* template_label_ = nullptr;
    QLabel* width_label_ = nullptr;
    QLabel* shift_label_ = nullptr;
    QLabel* lots_label_ = nullptr;
    QPushButton* add_btn_ = nullptr;
    QPushButton* use_btn_ = nullptr;
    QComboBox* template_combo_ = nullptr;
    QSpinBox* width_spin_ = nullptr;
    QSpinBox* shift_spin_ = nullptr;
    QSpinBox* lots_spin_ = nullptr;
};

} // namespace fincept::screens::fno
