#include "core/components/ComponentCatalog.h"

#include "core/components/PopularityTracker.h"
#include "core/logging/Logger.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>

namespace fincept {

ComponentCatalog& ComponentCatalog::instance() {
    static ComponentCatalog s;
    return s;
}

int ComponentCatalog::load_from_file(const QString& path) {
    entries_.clear();
    index_by_id_.clear();
    categories_in_order_.clear();

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        LOG_WARN("ComponentCatalog", QString("Could not open %1").arg(path));
        return 0;
    }
    const QByteArray blob = f.readAll();
    f.close();

    const QJsonDocument doc = QJsonDocument::fromJson(blob);
    if (!doc.isObject()) {
        LOG_WARN("ComponentCatalog", QString("Invalid JSON in %1").arg(path));
        return 0;
    }
    const QJsonArray arr = doc.object().value("components").toArray();
    for (const QJsonValue& v : arr) {
        if (!v.isObject())
            continue;
        ComponentMeta m = ComponentMeta::from_json(v.toObject());
        if (!m.is_valid())
            continue;
        if (index_by_id_.contains(m.id)) {
            LOG_WARN("ComponentCatalog", QString("Duplicate id: %1").arg(m.id));
            continue;
        }
        index_by_id_[m.id] = static_cast<int>(entries_.size());
        entries_.append(m);
        if (!m.category.isEmpty() && !categories_in_order_.contains(m.category))
            categories_in_order_.append(m.category);
    }

    LOG_INFO("ComponentCatalog", QString("Loaded %1 components from %2").arg(entries_.size()).arg(path));
    emit reloaded();
    return static_cast<int>(entries_.size());
}

int ComponentCatalog::load_with_fallbacks(const QStringList& candidates) {
    for (const QString& p : candidates) {
        if (QFileInfo::exists(p))
            return load_from_file(p);
    }
    LOG_WARN("ComponentCatalog", "No catalogue file found in any candidate path");
    return 0;
}

QList<ComponentMeta> ComponentCatalog::all() const {
    return entries_;
}

QList<ComponentMeta> ComponentCatalog::list_for_ui() const {
    QList<ComponentMeta> out = entries_;
    auto& tracker = PopularityTracker::instance();
    for (ComponentMeta& m : out)
        m.popularity = tracker.use_count(m.id);
    std::sort(out.begin(), out.end(), [](const ComponentMeta& a, const ComponentMeta& b) {
        if (a.popularity != b.popularity)
            return a.popularity > b.popularity;
        return a.title.localeAwareCompare(b.title) < 0;
    });
    return out;
}

QList<ComponentMeta> ComponentCatalog::by_category(const QString& category) const {
    QList<ComponentMeta> out;
    for (const ComponentMeta& m : list_for_ui()) {
        if (m.category == category)
            out.append(m);
    }
    return out;
}

QStringList ComponentCatalog::categories() const {
    return categories_in_order_;
}

ComponentMeta ComponentCatalog::by_id(const QString& id) const {
    const int idx = index_by_id_.value(id, -1);
    if (idx < 0 || idx >= entries_.size())
        return {};
    return entries_.at(idx);
}

} // namespace fincept
