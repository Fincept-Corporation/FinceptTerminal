#pragma once
#include <QCoreApplication>
#include <QJsonObject>
#include <QMap>
#include <QString>
#include <QVector>

#include <functional>

class QWidget;

namespace fincept::screens::widgets {
class BaseWidget;
}

namespace fincept::screens {

/// Factory receives the tile's persisted config (empty object for fresh widgets)
/// and the tile it lives in. Widgets that don't need per-instance config can
/// ignore it. The widget MUST be created with `parent` as its parent: a widget
/// built parentless and re-parented into the dashboard is re-styled on the move
/// (the dashboard's style sheets cascade into it), which cost ~0.4 s per tile.
using WidgetFactory = std::function<widgets::BaseWidget*(const QJsonObject&, QWidget* parent)>;

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
    Q_DECLARE_TR_FUNCTIONS(WidgetRegistry)
  public:
    static WidgetRegistry& instance();

    void register_widget(WidgetMeta meta);
    const WidgetMeta* find(const QString& type_id) const;
    QVector<WidgetMeta> all() const;
    QVector<WidgetMeta> by_category(const QString& category) const;

    /// Translated display strings. Storage stays English so equality checks
    /// (accent_for_category, category filter) keep working. Display sites
    /// must call these instead of reading meta.display_name etc. directly.
    static QString display_name_tr(const WidgetMeta& m);
    static QString description_tr(const WidgetMeta& m);
    static QString category_tr(const QString& category);

  private:
    WidgetRegistry();
    QMap<QString, WidgetMeta> registry_;
};

} // namespace fincept::screens
