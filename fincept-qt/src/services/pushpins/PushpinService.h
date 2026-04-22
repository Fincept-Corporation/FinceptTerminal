#pragma once
#include "core/symbol/SymbolRef.h"

#include <QList>
#include <QObject>

namespace fincept {

/// Singleton model backing the persistent symbol chip strip (Bloomberg
/// "Pushpins" equivalent). Pins are user-scoped — they follow the user
/// across workspaces and terminal sessions, stored in QSettings under
/// "pushpins/list".
///
/// The service is deliberately thin: add/remove/list/clear plus a single
/// signal that fires after any mutation. Consumers (PushpinBar) simply
/// rebuild themselves from pins() on change.
class PushpinService : public QObject {
    Q_OBJECT
  public:
    static PushpinService& instance();

    QList<SymbolRef> pins() const { return pins_; }
    bool contains(const SymbolRef& ref) const;

    /// Idempotent — pinning the same symbol twice has no effect.
    void pin(const SymbolRef& ref);
    void unpin(const SymbolRef& ref);
    void clear();

    /// Move `ref` to a new index (for drag-reorder within the bar).
    /// No-op if ref is not pinned or new_index is out of range.
    void move(const SymbolRef& ref, int new_index);

  signals:
    void pins_changed();

  private:
    PushpinService();
    void load();
    void save() const;

    QList<SymbolRef> pins_;
};

} // namespace fincept
