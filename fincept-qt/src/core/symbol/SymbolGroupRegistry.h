#pragma once
#include "core/symbol/SymbolGroup.h"

#include <QColor>
#include <QHash>
#include <QList>
#include <QObject>
#include <QString>

namespace fincept {

/// Per-user, per-slot metadata for symbol link groups (A..J).
///
/// Each slot owns three pieces of state:
///   - `name`: human-readable label ("Group A", "Tech Watch", …). Editable.
///   - `color`: badge fill colour. Editable.
///   - `enabled`: whether the slot appears in cycle/menus. A..G start
///                enabled; H..I..J start disabled.
///
/// The enum identity itself (the letter) is fixed and immutable — workspace
/// state, dock layouts, IGroupLinked persistence all reference the letter.
/// Renaming "A" to "Tech Watch" does not migrate any saved state; existing
/// links keep working under the new label.
///
/// Storage: QSettings under `symbol_groups/<letter>/{name,color,enabled}`,
/// global per user (not per workspace). Settings are written immediately on
/// each mutation — no batched flush.
class SymbolGroupRegistry : public QObject {
    Q_OBJECT
  public:
    static SymbolGroupRegistry& instance();

    QString name(SymbolGroup g) const;
    QColor color(SymbolGroup g) const;
    bool enabled(SymbolGroup g) const;

    void set_name(SymbolGroup g, const QString& name);
    void set_color(SymbolGroup g, const QColor& color);
    void set_enabled(SymbolGroup g, bool enabled);

    /// Slots in canonical letter order, filtered to those with `enabled=true`.
    QList<SymbolGroup> enabled_groups() const;

    /// Reset a single slot back to factory defaults (name + colour + enabled).
    void reset_to_default(SymbolGroup g);

    /// Cycle to the next *enabled* group, wrapping through None.
    /// Order: None → first-enabled → next-enabled → … → None. If no slots
    /// are enabled, returns None unconditionally.
    SymbolGroup next_enabled(SymbolGroup current) const;

    /// Default factory metadata — used by the registry on first run and by
    /// reset_to_default(). Exposed so tests / settings UI can show defaults
    /// alongside user overrides.
    static QString default_name(SymbolGroup g);
    static QColor default_color(SymbolGroup g);
    static bool default_enabled(SymbolGroup g);

  signals:
    /// Fires after any mutation to a slot. Subscribers re-render badges,
    /// menu labels, title-bar text, etc.
    void group_metadata_changed(fincept::SymbolGroup g);

  private:
    SymbolGroupRegistry();

    struct Slot {
        QString name;
        QColor color;
        bool enabled;
    };

    void load();
    void save_slot(SymbolGroup g) const;

    QHash<SymbolGroup, Slot> slots_;
};

} // namespace fincept
