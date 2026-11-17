#include "screens/watchlist/WatchlistColumnPrefs.h"

#include "core/logging/Logger.h"
#include "screens/watchlist/WatchlistColumns.h"
#include "storage/repositories/SettingsRepository.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <QtConcurrent/QtConcurrent>

namespace fincept::screens {

namespace wcols = fincept::screens::watchlist_columns;

namespace {

constexpr int kWlPrefsMinWidth = 20;
constexpr int kWlPrefsMaxWidth = 2000;

QJsonObject wl_prefs_read_object(const QString& key) {
    auto r = fincept::SettingsRepository::instance().get(key);
    if (r.is_err() || r.value().isEmpty())
        return {};
    const QJsonDocument doc = QJsonDocument::fromJson(r.value().toUtf8());
    return doc.isObject() ? doc.object() : QJsonObject{};
}

void wl_prefs_write_async(const QStringList& keys, const QString& value) {
    (void)QtConcurrent::run([keys, value]() {
        for (const QString& key : keys) {
            auto r = fincept::SettingsRepository::instance().set(key, value, WatchlistColumnPrefs::kCategory);
            if (r.is_err())
                LOG_WARN("WatchlistColumns",
                         "Could not save the watchlist layout: " + QString::fromStdString(r.error()));
        }
    });
}

/// `visible` with "symbol" first, known ids only, no duplicates.
QStringList wl_prefs_normalise(const QStringList& ids) {
    QStringList out{QString::fromLatin1(wcols::kSymbol)};
    QSet<QString> seen{out.first()};
    for (const QString& id : ids) {
        if (seen.contains(id) || wcols::index_of(id) < 0)
            continue;
        seen.insert(id);
        out << id;
    }
    return out;
}

} // namespace

// ── WatchlistLayout ─────────────────────────────────────────────────────────

QJsonObject WatchlistLayout::to_json() const {
    QJsonObject o;
    o.insert(QStringLiteral("v"), 2);
    o.insert(QStringLiteral("visible"), QJsonArray::fromStringList(visible));
    QJsonObject w;
    for (auto it = widths.constBegin(); it != widths.constEnd(); ++it)
        w.insert(it.key(), it.value());
    o.insert(QStringLiteral("widths"), w);
    o.insert(QStringLiteral("sort_id"), sort_id);
    o.insert(QStringLiteral("sort_desc"), sort_order == Qt::DescendingOrder);
    o.insert(QStringLiteral("grouping"), grouping);
    o.insert(QStringLiteral("collapsed"), QJsonArray::fromStringList(collapsed));
    o.insert(QStringLiteral("view"), view);
    o.insert(QStringLiteral("detail"), detail_visible);
    o.insert(QStringLiteral("map_size"), map_size);
    o.insert(QStringLiteral("map_horizon"), map_horizon);
    return o;
}

WatchlistLayout WatchlistLayout::from_json(const QJsonObject& o) {
    WatchlistLayout l;
    if (o.isEmpty())
        return l;
    QStringList ids;
    for (const auto& v : o.value(QStringLiteral("visible")).toArray())
        ids << v.toString();
    if (ids.isEmpty())
        return l; // not a v2 layout
    l.visible = wl_prefs_normalise(ids);
    const QJsonObject w = o.value(QStringLiteral("widths")).toObject();
    for (auto it = w.constBegin(); it != w.constEnd(); ++it) {
        const int px = it.value().toInt();
        if (wcols::index_of(it.key()) >= 0 && px >= kWlPrefsMinWidth && px <= kWlPrefsMaxWidth)
            l.widths.insert(it.key(), px);
    }
    l.sort_id = o.value(QStringLiteral("sort_id")).toString();
    if (wcols::index_of(l.sort_id) < 0)
        l.sort_id.clear();
    l.sort_order = o.value(QStringLiteral("sort_desc")).toBool() ? Qt::DescendingOrder : Qt::AscendingOrder;
    l.grouping = o.value(QStringLiteral("grouping")).toString(QStringLiteral("none"));
    for (const auto& v : o.value(QStringLiteral("collapsed")).toArray())
        l.collapsed << v.toString();
    l.view = o.value(QStringLiteral("view")).toString(QStringLiteral("table"));
    l.detail_visible = o.value(QStringLiteral("detail")).toBool(true);
    l.map_size = o.value(QStringLiteral("map_size")).toString(QStringLiteral("cap"));
    l.map_horizon = o.value(QStringLiteral("map_horizon")).toString(QStringLiteral("1d"));
    return l;
}

// ── WatchlistColumnPrefs ────────────────────────────────────────────────────

QString WatchlistColumnPrefs::list_key(const QString& list_id) {
    return QStringLiteral("watchlist.layout.") + list_id;
}

WatchlistLayout WatchlistColumnPrefs::builtin_default() {
    WatchlistLayout l;
    l.visible = wl_prefs_normalise(wcols::default_visible_ids());
    return l;
}

WatchlistLayout WatchlistColumnPrefs::from_legacy(const QJsonObject& legacy) {
    WatchlistLayout l = builtin_default();
    if (legacy.isEmpty())
        return l;
    const QStringList old_ids = wcols::legacy_ids();

    QSet<int> hidden;
    for (const auto& v : legacy.value(QStringLiteral("hidden")).toArray()) {
        const int c = v.toInt(-1);
        if (c > 0 && c < old_ids.size()) // column 0 (symbol) could never be hidden
            hidden.insert(c);
    }
    // Old columns: hidden ones leave the default set, visible ones are kept —
    // inserted after the previous old column when the default set lacks them.
    for (int c = 0; c < old_ids.size(); ++c) {
        const QString& id = old_ids[c];
        if (hidden.contains(c)) {
            l.visible.removeAll(id);
            continue;
        }
        if (l.visible.contains(id))
            continue;
        int at = static_cast<int>(l.visible.size());
        for (int p = c - 1; p >= 0; --p) {
            const qsizetype prev = l.visible.indexOf(old_ids[p]);
            if (prev >= 0) {
                at = static_cast<int>(prev) + 1;
                break;
            }
        }
        l.visible.insert(at, id);
    }
    const QJsonObject widths = legacy.value(QStringLiteral("widths")).toObject();
    for (auto it = widths.constBegin(); it != widths.constEnd(); ++it) {
        bool ok = false;
        const int c = it.key().toInt(&ok);
        const int px = it.value().toInt();
        if (ok && c >= 0 && c < old_ids.size() && px >= kWlPrefsMinWidth && px <= kWlPrefsMaxWidth)
            l.widths.insert(old_ids[c], px);
    }
    const int sort_col = legacy.value(QStringLiteral("sort_col")).toInt(-1);
    if (sort_col >= 0 && sort_col < old_ids.size()) {
        l.sort_id = old_ids[sort_col];
        l.sort_order = legacy.value(QStringLiteral("sort_order")).toInt(0) == 1 ? Qt::DescendingOrder
                                                                                   : Qt::AscendingOrder;
    }
    return l;
}

WatchlistLayout WatchlistColumnPrefs::load(const QString& list_id) {
    if (!list_id.isEmpty()) {
        const WatchlistLayout own = WatchlistLayout::from_json(wl_prefs_read_object(list_key(list_id)));
        if (own.is_valid())
            return own;
    }
    const WatchlistLayout def = WatchlistLayout::from_json(wl_prefs_read_object(QString::fromLatin1(kDefaultKey)));
    if (def.is_valid())
        return def;

    // First run of the v2 table: carry the old index-keyed preferences over and
    // store the result as the default (the old key is left untouched).
    const QJsonObject legacy = wl_prefs_read_object(QString::fromLatin1(kLegacyKey));
    if (!legacy.isEmpty()) {
        const WatchlistLayout migrated = from_legacy(legacy);
        wl_prefs_write_async({QString::fromLatin1(kDefaultKey)},
                             QString::fromUtf8(QJsonDocument(migrated.to_json()).toJson(QJsonDocument::Compact)));
        LOG_INFO("WatchlistColumns", "Migrated the column preferences of the previous watchlist table");
        return migrated;
    }
    return builtin_default();
}

void WatchlistColumnPrefs::save(const QString& list_id, const WatchlistLayout& layout) {
    if (!layout.is_valid())
        return;
    const QString value = QString::fromUtf8(QJsonDocument(layout.to_json()).toJson(QJsonDocument::Compact));
    QStringList keys{QString::fromLatin1(kDefaultKey)};
    if (!list_id.isEmpty())
        keys.prepend(list_key(list_id));
    wl_prefs_write_async(keys, value);
}

void WatchlistColumnPrefs::forget(const QString& list_id) {
    if (list_id.isEmpty())
        return;
    const QString key = list_key(list_id);
    (void)QtConcurrent::run([key]() { (void)fincept::SettingsRepository::instance().remove(key); });
}

} // namespace fincept::screens
