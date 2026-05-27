#pragma once

#include <QWidget>

class QHBoxLayout;
class QPushButton;

namespace fincept::ui {

class ChartOverlayManager;

struct IndicatorDef {
    QString id;
    QString name;
    QString category;
};

class IndicatorPicker : public QWidget {
    Q_OBJECT
public:
    explicit IndicatorPicker(ChartOverlayManager* mgr, QWidget* parent = nullptr);

signals:
    void indicator_requested(const QString& id);
    void indicator_removed(const QString& id);

private:
    void build_menu();
    void add_chip(const QString& id, const QString& name);
    void remove_chip(const QString& id);
    void refresh_chips();

    ChartOverlayManager* mgr_;
    QPushButton* add_btn_ = nullptr;
    QHBoxLayout* chip_layout_ = nullptr;
    QWidget* chip_container_ = nullptr;

    static QVector<IndicatorDef> available_indicators();
};

} // namespace fincept::ui
