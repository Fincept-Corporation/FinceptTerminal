// src/ui/widgets/algo/GroupBlock.h
#pragma once
#include "ui/widgets/algo/ConditionBlock.h"

#include <QEvent>
#include <QFrame>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QVector>

namespace fincept::ui::algo {

/// A nested condition group: its own AND/OR combinator over a list of children,
/// where each child is a ConditionBlock (leaf) or another GroupBlock. Enables
/// `(A AND B) OR C`. Serializes to `{type:"group", logic, children:[...]}`.
class GroupBlock : public QFrame {
    Q_OBJECT
public:
    explicit GroupBlock(bool is_entry, QWidget* parent = nullptr);

    QJsonObject to_json() const;
    void from_json(const QJsonObject& obj);

signals:
    void remove_requested();
    void changed();

protected:
    void changeEvent(QEvent* event) override;

private:
    void build_ui();
    void add_condition();
    void add_group();
    void attach_node(QWidget* node); // wire signals + append to nodes_
    void remove_node(QWidget* node);
    void rebuild_layout();
    void set_logic(const QString& logic);
    void update_logic_buttons();

    /// Re-apply tr() lookups to every widget whose text we keep a handle to.
    /// Called from changeEvent() on QEvent::LanguageChange.
    void retranslateUi();

    bool is_entry_;
    QString logic_ = QStringLiteral("AND");

    QLabel* tag_ = nullptr;
    QPushButton* and_btn_ = nullptr;
    QPushButton* or_btn_ = nullptr;
    QPushButton* add_cond_btn_ = nullptr;
    QPushButton* add_group_btn_ = nullptr;
    QPushButton* remove_btn_ = nullptr;
    QVBoxLayout* nodes_layout_ = nullptr;
    QVector<QWidget*> nodes_; // ConditionBlock* or GroupBlock*
};

} // namespace fincept::ui::algo
