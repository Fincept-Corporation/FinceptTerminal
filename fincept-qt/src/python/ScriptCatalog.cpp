#include "python/ScriptCatalog.h"

namespace fincept::python {

ScriptCatalog& ScriptCatalog::instance() {
    static ScriptCatalog s;
    return s;
}

ScriptCatalog::ScriptCatalog() {
    seed_builtins();
}

void ScriptCatalog::register_entry(CatalogEntry entry) {
    const QString name = entry.name;
    entries_.insert(name, std::move(entry));
}

std::optional<CatalogEntry> ScriptCatalog::resolve(const QString& name) const {
    auto it = entries_.find(name);
    if (it == entries_.end())
        return std::nullopt;
    return it.value();
}

QStringList ScriptCatalog::names() const {
    return entries_.keys();
}

void ScriptCatalog::seed_builtins() {
    // Hot-path scripts that multiple services hardcode today. Each migration
    // PR moves a few more entries here and replaces the literal string at the
    // callsite with `ScriptCatalog::instance().resolve(name)->relpath`.
    register_entry({QStringLiteral("market.yfinance"),
                    QStringLiteral("yfinance_data.py"),
                    VenvId::Numpy2});
    register_entry({QStringLiteral("agent.finagent_core"),
                    QStringLiteral("agents/finagent_core/main.py"),
                    VenvId::Numpy2});
    register_entry({QStringLiteral("market.asia_china"),
                    QStringLiteral("china_market_data_service.py"),
                    VenvId::Numpy2});
    // Numpy1-only — vectorbt, gluonts, financepy backtests live here.
    register_entry({QStringLiteral("backtest.vectorbt"),
                    QStringLiteral("Analytics/backtesting/vectorbt/vectorbt_provider.py"),
                    VenvId::Numpy1});
}

} // namespace fincept::python
