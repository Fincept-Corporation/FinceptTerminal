// src/ui/widgets/algo/ConditionSection.h
#pragma once
#include "ui/widgets/algo/ConditionBlock.h"
#include "ui/widgets/algo/GroupBlock.h"

#include <QEvent>
#include <QJsonArray>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::ui::algo {

/// Root of an entry/exit rule: a single AND/OR combinator over a list of
/// children, each a ConditionBlock (leaf) or a GroupBlock (nested group). The
/// section-level toggle is the real combinator — `combined_logic()` returns it
/// (no more "only the first connector counts"). Serializes its children to the
/// array the ConditionEvaluator consumes, with nested groups expressed as
/// `{type:"group", ...}` elements.
class ConditionSection : public QWidget {
    Q_OBJECT
public:
    enum class Type { Entry, Exit };

    explicit ConditionSection(Type type, QWidget* parent = nullptr);

    QJsonArray conditions() const;
    QString combined_logic() const;
    void set_conditions(const QJsonArray& conditions, const QString& logic);
    void clear_all();

signals:
    void conditions_changed();

public slots:
    void add_condition();

protected:
    void changeEvent(QEvent* event) override;

private:
    void add_group();
    void attach_node(QWidget* node);
    void remove_node(QWidget* node);
    void rebuild_layout();
    void set_logic(const QString& logic);
    void update_logic_buttons();

    /// Re-apply tr() lookups to every widget whose text we keep a handle to.
    /// Called from changeEvent() on QEvent::LanguageChange.
    void retranslateUi();

    Type type_;
    QString logic_ = QStringLiteral("AND");

    QLabel* title_ = nullptr;
    QLabel* match_label_ = nullptr;
    QPushButton* and_btn_ = nullptr;
    QPushButton* or_btn_ = nullptr;
    QPushButton* add_cond_btn_ = nullptr;
    QPushButton* add_group_btn_ = nullptr;
    QVBoxLayout* nodes_layout_ = nullptr;
    QVector<QWidget*> nodes_; // ConditionBlock* or GroupBlock*
};

} // namespace fincept::ui::algo
