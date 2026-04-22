#pragma once
#include "core/components/ComponentMeta.h"

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

namespace fincept {

/// Catalogue of all user-discoverable screens (components). The data lives
/// in resources/component_catalog.json and is loaded once at startup.
///
/// This is *purely* a UI discovery layer — the authoritative screen registry
/// is still DockScreenRouter. A catalogue entry with no matching
/// DockScreenRouter factory is harmless (the browser will gracefully fail
/// to navigate). A DockScreenRouter factory with no catalogue entry is
/// invisible to the browser; that's also fine for screens we intentionally
/// hide (auth, setup, lock, info, payment).
class ComponentCatalog : public QObject {
    Q_OBJECT
  public:
    static ComponentCatalog& instance();

    /// Load the catalogue from the given file. Clears any previous state.
    /// Returns the number of entries loaded (0 on failure). Safe to call
    /// multiple times — the browser can reload the catalogue without
    /// restarting.
    int load_from_file(const QString& path);

    /// Convenience: look in a list of candidate paths and use the first one
    /// that exists. Used at startup to try the installed resource dir and
    /// the build-dir fallback without forcing a Qt resource system round-trip.
    int load_with_fallbacks(const QStringList& candidates);

    /// All entries, unsorted. Popularity is NOT filled — use list_for_ui.
    QList<ComponentMeta> all() const;

    /// All entries with popularity populated from PopularityTracker and
    /// sorted by popularity desc then title asc. Intended for the browser.
    QList<ComponentMeta> list_for_ui() const;

    /// Entries in a single category, popularity-sorted.
    QList<ComponentMeta> by_category(const QString& category) const;

    /// Distinct category names in insertion order of first appearance.
    QStringList categories() const;

    /// Lookup by id; empty `id` returns an invalid ComponentMeta.
    ComponentMeta by_id(const QString& id) const;

  signals:
    void reloaded();

  private:
    ComponentCatalog() = default;
    QList<ComponentMeta> entries_;
    QHash<QString, int> index_by_id_; // id → position in entries_
    QStringList categories_in_order_;
};

} // namespace fincept
