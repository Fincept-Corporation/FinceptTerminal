// src/ui/widgets/algo/ConditionSection.h
#pragma once
#include "ui/widgets/algo/ConditionBlock.h"
#include "ui/widgets/algo/LogicConnectorWidget.h"

#include <QJsonArray>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::ui::algo {

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

private:
    void rebuild_layout();
    void on_block_removed(ConditionBlock* block);

    Type type_;
    QVector<ConditionBlock*> blocks_;
    QVector<LogicConnectorWidget*> connectors_;
    QVBoxLayout* blocks_layout_ = nullptr;
    QPushButton* add_btn_ = nullptr;
};

} // namespace fincept::ui::algo
