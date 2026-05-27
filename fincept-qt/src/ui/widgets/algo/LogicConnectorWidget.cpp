// src/ui/widgets/algo/LogicConnectorWidget.cpp
#include "ui/widgets/algo/LogicConnectorWidget.h"

#include <QHBoxLayout>
#include <QFrame>

namespace fincept::ui::algo {

LogicConnectorWidget::LogicConnectorWidget(QWidget* parent)
    : QWidget(parent) {
    setObjectName(QStringLiteral("logicConnector"));
    setFixedHeight(28);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(20, 0, 20, 0);
    layout->setSpacing(0);

    auto* left_line = new QFrame(this);
    left_line->setObjectName(QStringLiteral("logicLine"));
    left_line->setFrameShape(QFrame::HLine);
    left_line->setFixedHeight(1);

    and_btn_ = new QPushButton(tr("AND"), this);
    and_btn_->setObjectName(QStringLiteral("logicBtnAnd"));
    and_btn_->setCheckable(true);
    and_btn_->setChecked(true);
    and_btn_->setFixedSize(48, 22);

    or_btn_ = new QPushButton(tr("OR"), this);
    or_btn_->setObjectName(QStringLiteral("logicBtnOr"));
    or_btn_->setCheckable(true);
    or_btn_->setFixedSize(36, 22);

    auto* right_line = new QFrame(this);
    right_line->setObjectName(QStringLiteral("logicLine"));
    right_line->setFrameShape(QFrame::HLine);
    right_line->setFixedHeight(1);

    layout->addWidget(left_line, 1);
    layout->addWidget(and_btn_);
    layout->addWidget(or_btn_);
    layout->addWidget(right_line, 1);

    connect(and_btn_, &QPushButton::clicked, this, [this]() {
        and_btn_->setChecked(true);
        or_btn_->setChecked(false);
        update_style();
        emit logic_changed(QStringLiteral("AND"));
    });
    connect(or_btn_, &QPushButton::clicked, this, [this]() {
        or_btn_->setChecked(true);
        and_btn_->setChecked(false);
        update_style();
        emit logic_changed(QStringLiteral("OR"));
    });
    update_style();
}

QString LogicConnectorWidget::logic() const {
    return and_btn_->isChecked() ? QStringLiteral("AND") : QStringLiteral("OR");
}

void LogicConnectorWidget::set_logic(const QString& logic) {
    bool is_and = (logic.toUpper() == "AND");
    and_btn_->setChecked(is_and);
    or_btn_->setChecked(!is_and);
    update_style();
}

void LogicConnectorWidget::update_style() {
    and_btn_->setProperty("active", and_btn_->isChecked());
    or_btn_->setProperty("active", or_btn_->isChecked());
    and_btn_->style()->unpolish(and_btn_);
    and_btn_->style()->polish(and_btn_);
    or_btn_->style()->unpolish(or_btn_);
    or_btn_->style()->polish(or_btn_);
}

} // namespace fincept::ui::algo
