// src/ui/widgets/algo/LogicConnectorWidget.h
#pragma once
#include <QPushButton>
#include <QWidget>

namespace fincept::ui::algo {

class LogicConnectorWidget : public QWidget {
    Q_OBJECT
public:
    explicit LogicConnectorWidget(QWidget* parent = nullptr);

    QString logic() const;
    void set_logic(const QString& logic);

signals:
    void logic_changed(const QString& logic);

private:
    QPushButton* and_btn_ = nullptr;
    QPushButton* or_btn_ = nullptr;
    void update_style();
};

} // namespace fincept::ui::algo
