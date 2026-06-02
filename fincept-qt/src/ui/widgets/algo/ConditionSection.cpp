// src/ui/widgets/algo/ConditionSection.cpp
#include "ui/widgets/algo/ConditionSection.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QStyle>

namespace fincept::ui::algo {

ConditionSection::ConditionSection(Type type, QWidget* parent)
    : QWidget(parent), type_(type) {
    setObjectName(type == Type::Entry ? QStringLiteral("conditionSectionEntry")
                                      : QStringLiteral("conditionSectionExit"));

    auto* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(0, 0, 0, 0);
    main_layout->setSpacing(4);

    // ── Header: title + section AND/OR toggle ───────────────────────────────
    auto* header = new QHBoxLayout();
    header->setSpacing(6);
    title_ = new QLabel(
        type == Type::Entry ? tr("ENTRY CONDITIONS") : tr("EXIT CONDITIONS"), this);
    title_->setObjectName(type == Type::Entry ? QStringLiteral("condSectionHeaderEntry")
                                              : QStringLiteral("condSectionHeaderExit"));
    header->addWidget(title_);
    header->addStretch();

    match_label_ = new QLabel(tr("Match:"), this);
    match_label_->setObjectName(QStringLiteral("condSectionMatchLabel"));
    header->addWidget(match_label_);
    and_btn_ = new QPushButton(tr("ALL (AND)"), this);
    and_btn_->setObjectName(QStringLiteral("logicBtnAnd"));
    and_btn_->setCheckable(true);
    and_btn_->setFixedHeight(22);
    or_btn_ = new QPushButton(tr("ANY (OR)"), this);
    or_btn_->setObjectName(QStringLiteral("logicBtnOr"));
    or_btn_->setCheckable(true);
    or_btn_->setFixedHeight(22);
    header->addWidget(and_btn_);
    header->addWidget(or_btn_);
    main_layout->addLayout(header);

    // ── Children ────────────────────────────────────────────────────────────
    nodes_layout_ = new QVBoxLayout();
    nodes_layout_->setSpacing(2);
    nodes_layout_->setContentsMargins(0, 0, 0, 0);
    main_layout->addLayout(nodes_layout_);

    // ── Add buttons ─────────────────────────────────────────────────────────
    auto* add_row = new QHBoxLayout();
    add_row->setSpacing(6);
    add_cond_btn_ = new QPushButton(tr("+ Add Condition"), this);
    add_cond_btn_->setObjectName(QStringLiteral("condSectionAddBtn"));
    add_group_btn_ = new QPushButton(tr("+ Add Group"), this);
    add_group_btn_->setObjectName(QStringLiteral("condSectionAddGroupBtn"));
    add_row->addWidget(add_cond_btn_);
    add_row->addWidget(add_group_btn_);
    add_row->addStretch();
    main_layout->addLayout(add_row);

    main_layout->addStretch();

    connect(and_btn_, &QPushButton::clicked, this, [this]() { set_logic(QStringLiteral("AND")); emit conditions_changed(); });
    connect(or_btn_, &QPushButton::clicked, this, [this]() { set_logic(QStringLiteral("OR")); emit conditions_changed(); });
    connect(add_cond_btn_, &QPushButton::clicked, this, &ConditionSection::add_condition);
    connect(add_group_btn_, &QPushButton::clicked, this, &ConditionSection::add_group);

    update_logic_buttons();
    add_condition();
}

void ConditionSection::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void ConditionSection::retranslateUi() {
    if (title_)
        title_->setText(type_ == Type::Entry ? tr("ENTRY CONDITIONS") : tr("EXIT CONDITIONS"));
    if (match_label_) match_label_->setText(tr("Match:"));
    if (and_btn_) and_btn_->setText(tr("ALL (AND)"));
    if (or_btn_) or_btn_->setText(tr("ANY (OR)"));
    if (add_cond_btn_) add_cond_btn_->setText(tr("+ Add Condition"));
    if (add_group_btn_) add_group_btn_->setText(tr("+ Add Group"));
    // Connector labels between rows are built from tr("AND")/tr("OR") — rebuild
    // so they pick up the new locale too.
    rebuild_layout();
}

void ConditionSection::set_logic(const QString& logic) {
    logic_ = (logic.toUpper() == "OR") ? QStringLiteral("OR") : QStringLiteral("AND");
    update_logic_buttons();
    rebuild_layout();
}

void ConditionSection::update_logic_buttons() {
    const bool is_and = (logic_ == "AND");
    and_btn_->setChecked(is_and);
    or_btn_->setChecked(!is_and);
    for (QPushButton* b : {and_btn_, or_btn_}) {
        b->setProperty("active", b->isChecked());
        b->style()->unpolish(b);
        b->style()->polish(b);
    }
}

void ConditionSection::attach_node(QWidget* node) {
    if (auto* c = qobject_cast<ConditionBlock*>(node)) {
        connect(c, &ConditionBlock::remove_requested, this, [this, node]() { remove_node(node); });
        connect(c, &ConditionBlock::changed, this, &ConditionSection::conditions_changed);
    } else if (auto* g = qobject_cast<GroupBlock*>(node)) {
        connect(g, &GroupBlock::remove_requested, this, [this, node]() { remove_node(node); });
        connect(g, &GroupBlock::changed, this, &ConditionSection::conditions_changed);
    }
    nodes_.append(node);
}

void ConditionSection::add_condition() {
    attach_node(new ConditionBlock(type_ == Type::Entry, this));
    rebuild_layout();
    emit conditions_changed();
}

void ConditionSection::add_group() {
    attach_node(new GroupBlock(type_ == Type::Entry, this));
    rebuild_layout();
    emit conditions_changed();
}

void ConditionSection::remove_node(QWidget* node) {
    const int idx = nodes_.indexOf(node);
    if (idx < 0) return;
    if (nodes_.size() <= 1) return; // keep at least one row
    nodes_.removeAt(idx);
    node->deleteLater();
    rebuild_layout();
    emit conditions_changed();
}

void ConditionSection::rebuild_layout() {
    while (nodes_layout_->count() > 0) {
        QLayoutItem* item = nodes_layout_->takeAt(0);
        if (item->widget() && item->widget()->objectName() == "sectionConnectorLabel")
            item->widget()->deleteLater();
        delete item;
    }
    const QString conn = (logic_ == "OR") ? tr("OR") : tr("AND");
    for (int i = 0; i < nodes_.size(); ++i) {
        if (i > 0) {
            auto* lbl = new QLabel(conn, this);
            lbl->setObjectName(QStringLiteral("sectionConnectorLabel"));
            nodes_layout_->addWidget(lbl);
        }
        nodes_layout_->addWidget(nodes_[i]);
    }
}

QJsonArray ConditionSection::conditions() const {
    QJsonArray arr;
    for (QWidget* n : nodes_) {
        if (auto* c = qobject_cast<ConditionBlock*>(n))
            arr.append(c->to_json());
        else if (auto* g = qobject_cast<GroupBlock*>(n))
            arr.append(g->to_json());
    }
    return arr;
}

QString ConditionSection::combined_logic() const { return logic_; }

void ConditionSection::set_conditions(const QJsonArray& conditions, const QString& logic) {
    set_logic(logic);

    for (QWidget* n : nodes_)
        n->deleteLater();
    nodes_.clear();

    for (const auto& val : conditions) {
        const QJsonObject node = val.toObject();
        if (node.contains("children")) {
            auto* g = new GroupBlock(type_ == Type::Entry, this);
            attach_node(g);
            g->from_json(node);
        } else {
            auto* c = new ConditionBlock(type_ == Type::Entry, this);
            attach_node(c);
            c->from_json(node);
        }
    }
    if (nodes_.isEmpty())
        attach_node(new ConditionBlock(type_ == Type::Entry, this));
    rebuild_layout();
}

void ConditionSection::clear_all() {
    for (QWidget* n : nodes_)
        n->deleteLater();
    nodes_.clear();
    rebuild_layout();
}

} // namespace fincept::ui::algo
