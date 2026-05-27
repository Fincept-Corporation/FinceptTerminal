// src/ui/widgets/algo/ConditionSection.cpp
#include "ui/widgets/algo/ConditionSection.h"

#include <QLabel>
#include <QVBoxLayout>

namespace fincept::ui::algo {

ConditionSection::ConditionSection(Type type, QWidget* parent)
    : QWidget(parent), type_(type) {
    setObjectName(type == Type::Entry ? QStringLiteral("conditionSectionEntry")
                                      : QStringLiteral("conditionSectionExit"));

    auto* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(0, 0, 0, 0);
    main_layout->setSpacing(4);

    // Section header
    auto* header = new QLabel(
        type == Type::Entry ? tr("ENTRY CONDITIONS") : tr("EXIT CONDITIONS"), this);
    header->setObjectName(type == Type::Entry ? QStringLiteral("condSectionHeaderEntry")
                                               : QStringLiteral("condSectionHeaderExit"));
    main_layout->addWidget(header);

    // Condition blocks container
    blocks_layout_ = new QVBoxLayout();
    blocks_layout_->setSpacing(0);
    blocks_layout_->setContentsMargins(0, 0, 0, 0);
    main_layout->addLayout(blocks_layout_);

    // Add button
    add_btn_ = new QPushButton(tr("+ Add Condition"), this);
    add_btn_->setObjectName(QStringLiteral("condSectionAddBtn"));
    connect(add_btn_, &QPushButton::clicked, this, &ConditionSection::add_condition);
    main_layout->addWidget(add_btn_);

    main_layout->addStretch();

    add_condition();
}

void ConditionSection::add_condition() {
    auto* block = new ConditionBlock(type_ == Type::Entry, this);
    connect(block, &ConditionBlock::remove_requested, this, [this, block]() {
        on_block_removed(block);
    });
    connect(block, &ConditionBlock::changed, this, &ConditionSection::conditions_changed);

    blocks_.append(block);
    rebuild_layout();
    emit conditions_changed();
}

void ConditionSection::on_block_removed(ConditionBlock* block) {
    int idx = blocks_.indexOf(block);
    if (idx < 0) return;
    if (blocks_.size() <= 1) return;

    blocks_.removeAt(idx);
    block->deleteLater();
    rebuild_layout();
    emit conditions_changed();
}

void ConditionSection::rebuild_layout() {
    // Clear layout
    while (blocks_layout_->count() > 0) {
        auto* item = blocks_layout_->takeAt(0);
        if (item->widget() && qobject_cast<LogicConnectorWidget*>(item->widget())) {
            item->widget()->deleteLater();
        }
        delete item;
    }
    connectors_.clear();

    for (int i = 0; i < blocks_.size(); ++i) {
        if (i > 0) {
            auto* connector = new LogicConnectorWidget(this);
            connectors_.append(connector);
            connect(connector, &LogicConnectorWidget::logic_changed,
                    this, &ConditionSection::conditions_changed);
            blocks_layout_->addWidget(connector);
        }
        blocks_layout_->addWidget(blocks_[i]);
    }
}

QJsonArray ConditionSection::conditions() const {
    QJsonArray arr;
    for (const auto* block : blocks_)
        arr.append(block->to_json());
    return arr;
}

QString ConditionSection::combined_logic() const {
    if (connectors_.isEmpty()) return QStringLiteral("AND");
    return connectors_.first()->logic();
}

void ConditionSection::set_conditions(const QJsonArray& conditions, const QString& logic) {
    clear_all();
    for (const auto& val : conditions) {
        auto* block = new ConditionBlock(type_ == Type::Entry, this);
        block->from_json(val.toObject());
        connect(block, &ConditionBlock::remove_requested, this, [this, block]() {
            on_block_removed(block);
        });
        connect(block, &ConditionBlock::changed, this, &ConditionSection::conditions_changed);
        blocks_.append(block);
    }
    rebuild_layout();
    for (auto* c : connectors_)
        c->set_logic(logic);
}

void ConditionSection::clear_all() {
    for (auto* b : blocks_)
        b->deleteLater();
    blocks_.clear();
    rebuild_layout();
}

} // namespace fincept::ui::algo
