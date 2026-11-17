#include "screens/markets/MarketPanelStore.h"

#include "core/logging/Logger.h"
#include "screens/markets/MarketColumns.h"
#include "screens/markets/MarketsLogic.h"
#include "services/markets/MarketDataService.h"
#include "storage/repositories/SettingsRepository.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace fincept::screens {

namespace {
constexpr const char* kPanelStoreTag = "MarketPanels";

/// Set when load() could not *read* the stored panel set (as opposed to
/// reading a genuinely absent one). While it is set, save() refuses to write:
/// the panels in memory are the built-in defaults, and MarketsScreen persists
/// the whole set on every panel edit, so one transient DB error would otherwise
/// replace the user's panels wholesale. Cleared by the next successful load,
/// and by reset_to_defaults() (explicit user intent to overwrite).
bool g_panel_load_failed = false;

// One-time rewrite of tickers Yahoo stopped serving, in panels saved before the
// defaults moved off them. Saved panel sets are kept forever, so without this
// those rows stay dead. Exactly these two symbols, nothing else.
constexpr const char* kPanelSymbolMigrationKey = "markets_panel_symbols_migrated_v1";
struct PanelSymbolRename {
    const char* from;
    const char* to;
};
constexpr PanelSymbolRename kPanelSymbolRenames[] = {
    {"LBS=F", "LBR=F"},         // lumber
    {"UNI-USD", "UNI7083-USD"}, // Uniswap
};

/// Rewrites the dead symbols in the stored panel array (its objects' "symbols");
/// returns true if anything changed. A panel that already lists the replacement
/// just loses the old row (no duplicate). Idempotent: a second pass finds nothing.
bool panel_store_migrate_dead_symbols(QJsonArray& stored) {
    bool changed = false;
    for (qsizetype i = 0; i < stored.size(); ++i) {
        QJsonObject obj = stored.at(i).toObject();
        const QJsonArray syms_json = obj.value(QStringLiteral("symbols")).toArray();
        QStringList syms;
        for (const auto& s : syms_json)
            syms << s.toString();
        bool panel_changed = false;
        for (const auto& rename : kPanelSymbolRenames) {
            const QString from = QString::fromLatin1(rename.from);
            const QString to = QString::fromLatin1(rename.to);
            const qsizetype at = syms.indexOf(from);
            if (at < 0)
                continue;
            if (!syms.contains(to))
                syms[at] = to;    // same row position
            syms.removeAll(from); // and no copy of the old ticker left
            panel_changed = true;
        }
        if (panel_changed) {
            obj[QStringLiteral("symbols")] = QJsonArray::fromStringList(syms);
            stored[i] = obj;
            changed = true;
        }
    }
    return changed;
}

// Second one-time pass (v2): every stored symbol in its canonical market-data form
// (core::instrument::canonical_symbol — case, USD-base FX "USDJPY=X" -> "JPY=X", class
// shares, HK padding, NSE series), duplicates within a panel dropped keeping the first
// position. A symbol canonical_symbol cannot place is kept exactly as stored.
constexpr const char* kPanelSymbolMigrationV2Key = "markets_panel_symbols_migrated_v2";

bool panel_store_canonicalise_symbols(QJsonArray& stored) {
    bool changed = false;
    for (qsizetype i = 0; i < stored.size(); ++i) {
        QJsonObject obj = stored.at(i).toObject();
        QStringList syms;
        for (const auto& s : obj.value(QStringLiteral("symbols")).toArray())
            syms << s.toString();
        bool panel_changed = false;
        const QStringList canon = markets_logic::canonical_panel_symbols(syms, &panel_changed);
        if (!panel_changed)
            continue;
        obj[QStringLiteral("symbols")] = QJsonArray::fromStringList(canon);
        stored[i] = obj;
        changed = true;
    }
    return changed;
}

/// Runs one stored-JSON migration once: `migrate` rewrites `stored` in place; the
/// result is written back and the flag `key` set only after a successful write, so
/// a failed write retries on the next load.
template <typename Fn>
void panel_store_run_migration(QJsonArray& stored, const char* key, const char* settings_key, const char* category,
                               Fn migrate, const QString& what) {
    auto& settings = SettingsRepository::instance();
    const auto done = settings.get(QString::fromLatin1(key));
    if (!done.is_ok() || !done.value().isEmpty())
        return;
    bool written = true;
    if (migrate(stored)) {
        written = settings
                      .set(QString::fromLatin1(settings_key),
                           QString::fromUtf8(QJsonDocument(stored).toJson(QJsonDocument::Compact)),
                           QString::fromLatin1(category))
                      .is_ok();
        LOG_INFO(kPanelStoreTag, what + (written ? QString() : QStringLiteral(" (not stored; will retry)")));
    }
    if (written)
        settings.set(QString::fromLatin1(key), QStringLiteral("1"), QString::fromLatin1(category));
}
} // namespace

bool MarketPanelStore::load_failed() const {
    return g_panel_load_failed;
}

MarketPanelStore& MarketPanelStore::instance() {
    static MarketPanelStore inst;
    return inst;
}

QVector<MarketPanelConfig> MarketPanelStore::load() {
    auto result = SettingsRepository::instance().get(kSettingsKey);
    if (result.is_err()) {
        LOG_ERROR(kPanelStoreTag, QString("settings read failed for '%1' — showing the built-in panels read-only, "
                                          "leaving the stored set untouched: %2")
                                      .arg(QString::fromLatin1(kSettingsKey), QString::fromStdString(result.error())));
        g_panel_load_failed = true;
        return build_defaults();
    }
    g_panel_load_failed = false;
    if (result.value().isEmpty())
        return build_defaults();

    auto doc = QJsonDocument::fromJson(result.value().toUtf8());
    if (!doc.isArray())
        return build_defaults();
    QJsonArray stored = doc.array();

    // One-time dead-ticker rewrite (see kPanelSymbolRenames), applied to the
    // stored JSON itself so nothing but those symbols changes. Marked done only
    // once the rewritten set is written, so a failed write retries on the next
    // load; a symbol the user re-adds by hand afterwards is left alone.
    auto& settings = SettingsRepository::instance();
    const auto migrated = settings.get(QString::fromLatin1(kPanelSymbolMigrationKey));
    if (migrated.is_ok() && migrated.value().isEmpty()) {
        bool written = true;
        if (panel_store_migrate_dead_symbols(stored)) {
            written = settings
                          .set(kSettingsKey, QString::fromUtf8(QJsonDocument(stored).toJson(QJsonDocument::Compact)),
                               kCategory)
                          .is_ok();
            LOG_INFO(kPanelStoreTag, QString("Replaced retired tickers (LBS=F -> LBR=F, UNI-USD -> UNI7083-USD) in "
                                             "saved panels%1")
                                         .arg(written ? QString() : QStringLiteral(" (not stored; will retry)")));
        }
        if (written)
            settings.set(QString::fromLatin1(kPanelSymbolMigrationKey), QStringLiteral("1"), kCategory);
    }

    // v2: canonical symbol forms (after the v1 renames above), same write-then-flag rule.
    panel_store_run_migration(stored, kPanelSymbolMigrationV2Key, kSettingsKey, kCategory,
                              panel_store_canonicalise_symbols,
                              QStringLiteral("Canonicalised saved panel symbols (e.g. USDJPY=X -> JPY=X)"));

    QVector<MarketPanelConfig> panels;
    for (const auto& v : stored) {
        QJsonObject obj = v.toObject();
        MarketPanelConfig cfg;
        cfg.id = obj["id"].toString();
        cfg.title = obj["title"].toString();
        cfg.show_name = obj["show_name"].toBool(false);
        QStringList stored_symbols;
        for (const auto& s : obj["symbols"].toArray())
            stored_symbols << s.toString();
        // Canonical in memory on every load (idempotent; the v2 pass above only
        // decides what is written back once).
        cfg.symbols = markets_logic::canonical_panel_symbols(stored_symbols);
        if (obj.contains("column_order")) {
            QStringList cols;
            for (const auto& col : obj["column_order"].toArray())
                cols << col.toString();
            if (!cols.isEmpty())
                cfg.column_order = cols;
        }
        // Columns: the new id list when present; otherwise the legacy codes mapped
        // (the old default list becomes the new default; any other list is kept as
        // mapped, in order — BID / ASK / NAME dropped).
        QStringList ids;
        for (const auto& id : obj.value(QStringLiteral("columns")).toArray())
            ids << id.toString();
        cfg.columns = ids.isEmpty() ? mcols::ids_from_legacy(cfg.column_order) : mcols::sanitize_ids(ids);
        if (cfg.column_order.isEmpty())
            cfg.column_order = mcols::legacy_from_ids(cfg.columns);
        cfg.sort_id = obj.value(QStringLiteral("sort_id")).toString();
        if (!cfg.sort_id.isEmpty() && mcols::index_of(cfg.sort_id) < 0)
            cfg.sort_id.clear();
        cfg.sort_desc = obj.value(QStringLiteral("sort_desc")).toBool(false);
        const QJsonObject widths = obj.value(QStringLiteral("widths")).toObject();
        for (auto it = widths.begin(); it != widths.end(); ++it) {
            const int w = it.value().toInt(0);
            if (mcols::index_of(it.key()) >= 0 && w >= 20 && w <= 2000)
                cfg.widths.insert(it.key(), w);
        }
        cfg.column_index = obj["column_index"].toInt(0);
        cfg.splitter_index = obj["splitter_index"].toInt(0);
        if (!cfg.id.isEmpty() && !cfg.title.isEmpty())
            panels.append(cfg);
    }
    if (panels.isEmpty())
        return build_defaults();

    // Migration: if all panels have column_index==0 (old save format), redistribute evenly
    bool all_zero = true;
    for (const auto& p : panels) {
        if (p.column_index != 0) {
            all_zero = false;
            break;
        }
    }
    if (all_zero && panels.size() > 1) {
        for (int i = 0; i < panels.size(); ++i)
            panels[i].column_index = i % 3;
    }

    return panels;
}

void MarketPanelStore::save(const QVector<MarketPanelConfig>& panels) {
    if (g_panel_load_failed) {
        // The read failure itself was logged (ERROR) when the store was loaded.
        LOG_WARN_RL(kPanelStoreTag, QStringLiteral("save_skipped"), 60000,
                    logf("Market panels not saved", {{"panels", panels.size()},
                                                     {"reason", "the stored panel set could not be read this session; "
                                                                "the on-screen defaults must not overwrite it"}}));
        return;
    }

    QJsonArray arr;
    for (const auto& cfg : panels) {
        QJsonObject obj;
        obj["id"] = cfg.id;
        obj["title"] = cfg.title;
        obj["show_name"] = cfg.show_name;
        QJsonArray syms;
        for (const auto& s : cfg.symbols)
            syms.append(s);
        obj["symbols"] = syms;
        // Legacy codes still written, so the previous build can read a newer save.
        const QStringList ids = cfg.columns.isEmpty() ? default_market_columns() : cfg.columns;
        obj["column_order"] = QJsonArray::fromStringList(mcols::legacy_from_ids(ids));
        obj["columns"] = QJsonArray::fromStringList(ids);
        if (!cfg.sort_id.isEmpty()) {
            obj["sort_id"] = cfg.sort_id;
            obj["sort_desc"] = cfg.sort_desc;
        }
        if (!cfg.widths.isEmpty()) {
            QJsonObject widths;
            for (auto it = cfg.widths.cbegin(); it != cfg.widths.cend(); ++it)
                widths.insert(it.key(), it.value());
            obj["widths"] = widths;
        }
        obj["column_index"] = cfg.column_index;
        obj["splitter_index"] = cfg.splitter_index;
        arr.append(obj);
    }
    SettingsRepository::instance().set(kSettingsKey,
                                       QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)), kCategory);
}

void MarketPanelStore::reset_to_defaults() {
    // Explicit user intent to overwrite the stored set with the defaults —
    // the only path allowed to lift the read-failure guard.
    g_panel_load_failed = false;
    save(build_defaults());
}

QVector<MarketPanelConfig> MarketPanelStore::build_defaults() {
    QVector<MarketPanelConfig> panels;

    // Every default goes through core::instrument::canonical_symbol (the original is
    // kept when it returns nothing) and is de-duplicated, so the defaults are in the
    // same form the editor and the v2 migration store.
    // Global markets
    for (const auto& cat : services::MarketDataService::default_global_markets())
        panels.append(MarketPanelConfig::make(cat.category, markets_logic::canonical_panel_symbols(cat.tickers), false));

    // Regional markets — flatten TickerDef → symbol strings
    for (const auto& reg : services::MarketDataService::default_regional_markets()) {
        QStringList syms;
        for (const auto& t : reg.tickers)
            syms << t.symbol;
        panels.append(MarketPanelConfig::make(reg.region, markets_logic::canonical_panel_symbols(syms), true));
    }

    // Distribute panels evenly across 3 columns
    for (int i = 0; i < panels.size(); ++i)
        panels[i].column_index = i % 3;

    return panels;
}

} // namespace fincept::screens
