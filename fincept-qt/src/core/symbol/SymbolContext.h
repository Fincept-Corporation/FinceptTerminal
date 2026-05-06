#pragma once
#include "core/symbol/SymbolGroup.h"
#include "core/symbol/SymbolRef.h"

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QString>

namespace fincept {

/// Process-global registry of the active symbol per group. Persisted per layout via to/from_json.
/// `source` lets subscribers detect their own publishes and avoid feedback loops.
class SymbolContext : public QObject {
    Q_OBJECT
  public:
    static SymbolContext& instance();

    SymbolRef group_symbol(SymbolGroup g) const;
    bool has_group_symbol(SymbolGroup g) const;

    /// No-op when ref matches existing value (signals suppressed).
    void set_group_symbol(SymbolGroup g, const SymbolRef& ref, QObject* source = nullptr);

    SymbolRef active() const { return active_; }

    void clear();

    QJsonObject to_json() const;
    void from_json(const QJsonObject& o);

  signals:
    /// `source` may be nullptr. Subscribers that publish should compare `source == this` to skip re-publish.
    void group_symbol_changed(fincept::SymbolGroup g, fincept::SymbolRef ref, QObject* source);
    void active_symbol_changed(fincept::SymbolRef ref, QObject* source);

  private:
    SymbolContext();
    QHash<SymbolGroup, SymbolRef> groups_;
    SymbolRef active_;
};

} // namespace fincept
