#pragma once
#include <QMap>
#include <QString>
#include <QVector>

#include <functional>

namespace fincept::screens::widgets {
class BaseWidget;
}

namespace fincept::screens {

using WidgetFactory = std::function<widgets::BaseWidget*()>;

struct WidgetMeta {
    QString type_id;
    QString display_name;
    QString category;
    QString description;
    int default_w = 4;
    int default_h = 3;
    int min_w = 2;
    int min_h = 2;
    WidgetFactory factory;
};

class WidgetRegistry {
  public:
    static WidgetRegistry& instance();

    void register_widget(WidgetMeta meta);
    const WidgetMeta* find(const QString& type_id) const;
    QVector<WidgetMeta> all() const;
    QVector<WidgetMeta> by_category(const QString& category) const;

  private:
    WidgetRegistry();
    QMap<QString, WidgetMeta> registry_;
};

} // namespace fincept::screens
