#pragma once
#include "services/options/StrategyTemplates.h"
#include <QComboBox>
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

private slots:
    void on_use_clicked();

private:
    QComboBox* template_combo_ = nullptr;
    QSpinBox* width_spin_ = nullptr;
    QSpinBox* shift_spin_ = nullptr;
    QSpinBox* lots_spin_ = nullptr;
};

} // namespace fincept::screens::fno
