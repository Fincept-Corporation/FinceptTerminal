#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace fincept {

/// One screen's catalogue entry. Loaded from resources/component_catalog.json.
/// `popularity` is filled live by ComponentCatalog from PopularityTracker, not stored on disk.
struct ComponentMeta {
    QString id;           // matches DockScreenRouter screen id
    QString title;        // human-readable, e.g. "Equity Trading"
    QString category;     // "Trading", "Markets", "Research", etc.
    QString description;  // one-line summary; empty allowed
    QStringList tags;     // searchable keywords; empty allowed
    int popularity = 0;   // use count, filled by ComponentCatalog::list_for_ui

    bool is_valid() const { return !id.isEmpty() && !title.isEmpty(); }

    QJsonObject to_json() const {
        QJsonObject o;
        o["id"] = id;
        o["title"] = title;
        o["category"] = category;
        o["description"] = description;
        QJsonArray ta;
        for (const QString& t : tags)
            ta.append(t);
        o["tags"] = ta;
        return o;
    }

    static ComponentMeta from_json(const QJsonObject& o) {
        ComponentMeta m;
        m.id = o.value("id").toString();
        m.title = o.value("title").toString();
        m.category = o.value("category").toString();
        m.description = o.value("description").toString();
        for (const QJsonValue& v : o.value("tags").toArray())
            m.tags.append(v.toString());
        return m;
    }
};

} // namespace fincept
