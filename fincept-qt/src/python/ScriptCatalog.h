#pragma once
// Phase 11 scaffolding — registry over the 1,423 scripts under `scripts/`.
//
// Today, ~10+ services hardcode literal paths:
//   PythonRunner::run("yfinance_data.py", args)
//   PythonRunner::run("agents/finagent_core/main.py", args)
//
// Renaming a script breaks compilation across the codebase. `ScriptCatalog`
// resolves logical names to (rel_path, venv) tuples. Callers reference scripts
// by name; the catalog is the single source of truth for file location and
// runtime requirements.
//
// The catalog seeds itself with the small set of hot-path scripts. New entries
// register from anywhere via `register_entry`. A future build step can
// auto-generate `scripts/catalog.json` and load it here.

#include <QHash>
#include <QString>
#include <QStringList>

#include <optional>

namespace fincept::python {

/// Which managed venv a script needs.
/// Mirrors `PythonSetupManager`'s two venvs.
enum class VenvId {
    Numpy2, ///< Default — pyportfolioopt-free, modern NumPy.
    Numpy1, ///< Legacy — vectorbt, gluonts, financepy, ffn, functime.
};

struct CatalogEntry {
    QString name;        ///< Canonical logical name (e.g. "market.yfinance").
    QString relpath;     ///< Path under fincept-qt/scripts/ (forward slashes).
    VenvId  venv = VenvId::Numpy2;
};

/// Process-global script catalog.
///
/// First call seeds with hot-path entries. Additional entries register at
/// any time (typically once at startup by the owning service).
class ScriptCatalog {
  public:
    static ScriptCatalog& instance();

    /// Register or replace an entry. Idempotent.
    void register_entry(CatalogEntry entry);

    /// Look up by logical name. Returns std::nullopt if not registered.
    std::optional<CatalogEntry> resolve(const QString& name) const;

    /// All registered logical names.
    QStringList names() const;

  private:
    ScriptCatalog();
    void seed_builtins();

    QHash<QString, CatalogEntry> entries_;
};

} // namespace fincept::python
