// src/ui/widgets/algo/GroupBlock.cpp
#include "ui/widgets/algo/GroupBlock.h"

#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QStyle>

namespace fincept::ui::algo {

GroupBlock::GroupBlock(bool is_entry, QWidget* parent)
    : QFrame(parent), is_entry_(is_entry) {
    setObjectName(QStringLiteral("conditionGroupBlock"));
    setFrameShape(QFrame::StyledPanel);
    build_ui();
}

void GroupBlock::build_ui() {
    auto* main = new QVBoxLayout(this);
    main->setContentsMargins(8, 6, 8, 8);
    main->setSpacing(4);

    // ── Header: GROUP · AND/OR · +Condition · +Group · remove ───────────────
    auto* header = new QHBoxLayout();
    header->setSpacing(6);

    auto* tag = new QLabel(tr("GROUP"), this);
    tag->setObjectName(QStringLiteral("groupTag"));
    header->addWidget(tag);

    and_btn_ = new QPushButton(tr("AND"), this);
    and_btn_->setObjectName(QStringLiteral("logicBtnAnd"));
    and_btn_->setCheckable(true);
    and_btn_->setFixedHeight(22);
    or_btn_ = new QPushButton(tr("OR"), this);
    or_btn_->setObjectName(QStringLiteral("logicBtnOr"));
    or_btn_->setCheckable(true);
    or_btn_->setFixedHeight(22);
    header->addWidget(and_btn_);
    header->addWidget(or_btn_);
    header->addStretch();

    auto* add_cond = new QPushButton(tr("+ Condition"), this);
    add_cond->setObjectName(QStringLiteral("condSectionAddBtn"));
    auto* add_grp = new QPushButton(tr("+ Group"), this);
    add_grp->setObjectName(QStringLiteral("condSectionAddGroupBtn"));
    auto* remove = new QPushButton(QStringLiteral("✕"), this);
    remove->setObjectName(QStringLiteral("condBlockRemove"));
    remove->setFixedSize(24, 22);
    remove->setToolTip(tr("Remove group"));
    header->addWidget(add_cond);
    header->addWidget(add_grp);
    header->addWidget(remove);
    main->addLayout(header);

    nodes_layout_ = new QVBoxLayout();
    nodes_layout_->setContentsMargins(8, 0, 0, 0);
    nodes_layout_->setSpacing(2);
    main->addLayout(nodes_layout_);

    connect(and_btn_, &QPushButton::clicked, this, [this]() { set_logic(QStringLiteral("AND")); emit changed(); });
    connect(or_btn_, &QPushButton::clicked, this, [this]() { set_logic(QStringLiteral("OR")); emit changed(); });
    connect(add_cond, &QPushButton::clicked, this, &GroupBlock::add_condition);
    connect(add_grp, &QPushButton::clicked, this, &GroupBlock::add_group);
    connect(remove, &QPushButton::clicked, this, &GroupBlock::remove_requested);

    update_logic_buttons();
    add_condition(); // a fresh group starts with one condition
}

void GroupBlock::set_logic(const QString& logic) {
    logic_ = (logic.toUpper() == "OR") ? QStringLiteral("OR") : QStringLiteral("AND");
    update_logic_buttons();
}

void GroupBlock::update_logic_buttons() {
    const bool is_and = (logic_ == "AND");
    and_btn_->setChecked(is_and);
    or_btn_->setChecked(!is_and);
    for (QPushButton* b : {and_btn_, or_btn_}) {
        b->setProperty("active", b->isChecked());
        b->style()->unpolish(b);
        b->style()->polish(b);
    }
}

void GroupBlock::attach_node(QWidget* node) {
    if (auto* c = qobject_cast<ConditionBlock*>(node)) {
        connect(c, &ConditionBlock::remove_requested, this, [this, node]() { remove_node(node); });
        connect(c, &ConditionBlock::changed, this, &GroupBlock::changed);
    } else if (auto* g = qobject_cast<GroupBlock*>(node)) {
        connect(g, &GroupBlock::remove_requested, this, [this, node]() { remove_node(node); });
        connect(g, &GroupBlock::changed, this, &GroupBlock::changed);
    }
    nodes_.append(node);
}

void GroupBlock::add_condition() {
    attach_node(new ConditionBlock(is_entry_, this));
    rebuild_layout();
    emit changed();
}

void GroupBlock::add_group() {
    attach_node(new GroupBlock(is_entry_, this));
    rebuild_layout();
    emit changed();
}

void GroupBlock::remove_node(QWidget* node) {
    const int idx = nodes_.indexOf(node);
    if (idx < 0) return;
    nodes_.removeAt(idx);
    node->deleteLater();
    rebuild_layout();
    emit changed();
}

void GroupBlock::rebuild_layout() {
    // Detach connector labels but keep node widgets alive.
    while (nodes_layout_->count() > 0) {
        QLayoutItem* item = nodes_layout_->takeAt(0);
        if (item->widget() && item->widget()->objectName() == "groupConnectorLabel")
            item->widget()->deleteLater();
        delete item;
    }
    for (int i = 0; i < nodes_.size(); ++i) {
        if (i > 0) {
            auto* lbl = new QLabel(logic_, this);
            lbl->setObjectName(QStringLiteral("groupConnectorLabel"));
            nodes_layout_->addWidget(lbl);
        }
        nodes_layout_->addWidget(nodes_[i]);
    }
}

QJsonObject GroupBlock::to_json() const {
    QJsonObject obj;
    obj["type"] = QStringLiteral("group");
    obj["logic"] = logic_;
    QJsonArray children;
    for (QWidget* n : nodes_) {
        if (auto* c = qobject_cast<ConditionBlock*>(n))
            children.append(c->to_json());
        else if (auto* g = qobject_cast<GroupBlock*>(n))
            children.append(g->to_json());
    }
    obj["children"] = children;
    return obj;
}

void GroupBlock::from_json(const QJsonObject& obj) {
    set_logic(obj.value("logic").toString(obj.value("op").toString("AND")));

    for (QWidget* n : nodes_)
        n->deleteLater();
    nodes_.clear();

    for (const auto& v : obj.value("children").toArray()) {
        const QJsonObject node = v.toObject();
        if (node.contains("children")) {
            auto* g = new GroupBlock(is_entry_, this);
            attach_node(g);
            g->from_json(node);
        } else {
            auto* c = new ConditionBlock(is_entry_, this);
            attach_node(c);
            c->from_json(node);
        }
    }
    rebuild_layout();
}

} // namespace fincept::ui::algo
