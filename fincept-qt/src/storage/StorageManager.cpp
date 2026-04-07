#include "storage/StorageManager.h"

#include "core/config/AppPaths.h"
#include "core/logging/Logger.h"
#include "storage/cache/CacheManager.h"
#include "storage/sqlite/CacheDatabase.h"
#include "storage/sqlite/Database.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QSqlQuery>

namespace fincept {

StorageManager& StorageManager::instance() {
    static StorageManager s;
    return s;
}

StorageManager::StorageManager(QObject* parent) : QObject(parent) {}

// ── Helpers ──────────────────────────────────────────────────────────────────

int StorageManager::count_table(const QString& table, const QString& where) const {
    auto& db = Database::instance();
    if (!db.is_open()) return 0;

    QString sql = "SELECT COUNT(*) FROM " + table;
    if (!where.isEmpty())
        sql += " WHERE " + where;

    auto r = db.execute(sql, {});
    if (r.is_err()) return 0;
    auto& q = r.value();
    return q.next() ? q.value(0).toInt() : 0;
}

Result<void> StorageManager::delete_from(const QString& table, const QString& where) {
    auto& db = Database::instance();
    if (!db.is_open())
        return Result<void>::err("Database not open");

    QString sql = "DELETE FROM " + table;
    if (!where.isEmpty())
        sql += " WHERE " + where;

    return db.exec(sql);
}

// ── Category definitions ─────────────────────────────────────────────────────

QVector<StorageCategoryInfo> StorageManager::all_stats() const {
    QVector<StorageCategoryInfo> stats;

    // --- Cache (cache.db) ---
    stats.append({"cache",          "Temporary Cache",       "Cache",
                  CacheManager::instance().entry_count(), true});

    {   // tab sessions from cache.db
        int ts_count = 0;
        auto& cdb = CacheDatabase::instance();
        if (cdb.is_open()) {
            auto r = cdb.execute("SELECT COUNT(*) FROM tab_sessions", {});
            if (r.is_ok()) {
                auto& q = r.value();
                if (q.next()) ts_count = q.value(0).toInt();
            }
        }
        stats.append({"tab_sessions", "Tab Session State", "Cache", ts_count, true});
    }

    // --- AI & LLM ---
    stats.append({"chat_sessions",     "Chat Sessions",        "AI & LLM",
                  count_table("chat_sessions"), true});
    stats.append({"chat_messages",     "Chat Messages",        "AI & LLM",
                  count_table("chat_messages"), true});
    stats.append({"context_recordings","Context Recordings",   "AI & LLM",
                  count_table("recorded_contexts"), true});
    stats.append({"agent_configs",     "Agent Configurations", "AI & LLM",
                  count_table("agent_configs"), true});
    stats.append({"llm_configs",       "LLM Provider Configs", "AI & LLM",
                  count_table("llm_configs"), true});
    stats.append({"llm_model_configs", "LLM Model Configs",    "AI & LLM",
                  count_table("llm_model_configs"), true});
    stats.append({"llm_profiles",      "LLM Profiles",         "AI & LLM",
                  count_table("llm_profiles"), true});

    // --- News ---
    stats.append({"news_articles",  "News Articles",    "News",
                  count_table("news_articles"), true});
    stats.append({"news_monitors",  "News Monitors",    "News",
                  count_table("news_monitors"), true});
    stats.append({"rss_feeds",      "RSS Feeds",        "News",
                  count_table("user_rss_feeds"), true});

    // --- Notes & Reports ---
    stats.append({"notes",     "Financial Notes",  "Notes & Reports",
                  count_table("financial_notes"), true});
    stats.append({"reports",   "Reports",          "Notes & Reports",
                  count_table("reports"), true});

    // --- Portfolio & Trading ---
    stats.append({"portfolios",    "Portfolios",          "Portfolio & Trading",
                  count_table("portfolios"), true});
    stats.append({"transactions",  "Transactions",        "Portfolio & Trading",
                  count_table("portfolio_transactions"), true});
    stats.append({"watchlists",    "Watchlists",           "Portfolio & Trading",
                  count_table("watchlists"), true});
    stats.append({"custom_indices","Custom Indices",       "Portfolio & Trading",
                  count_table("custom_indices"), true});

    // --- Paper Trading ---
    stats.append({"pt_portfolios", "Paper Portfolios",     "Paper Trading",
                  count_table("pt_portfolios"), true});
    stats.append({"pt_orders",     "Paper Orders",          "Paper Trading",
                  count_table("pt_orders"), true});
    stats.append({"pt_trades",     "Paper Trades",          "Paper Trading",
                  count_table("pt_trades"), true});

    // --- Workflows ---
    stats.append({"workflows", "Workflows", "Workflows",
                  count_table("workflows"), true});

    // --- Data Sources & Integration ---
    stats.append({"data_sources",  "Data Sources",     "Data & Integration",
                  count_table("data_sources"), true});
    stats.append({"data_mappings", "Data Mappings",    "Data & Integration",
                  count_table("data_mappings"), true});
    stats.append({"mcp_servers",   "MCP Servers",      "Data & Integration",
                  count_table("mcp_servers"), true});

    // --- Dashboard ---
    stats.append({"dashboard_layouts", "Dashboard Layouts", "Dashboard",
                  count_table("dashboard_layouts"), true});

    // --- Broker ---
    stats.append({"instruments", "Broker Instruments", "Broker",
                  count_table("instruments"), true});

    // --- Settings & Credentials ---
    stats.append({"settings",       "App Settings",         "Settings & Credentials",
                  count_table("settings"), true});
    stats.append({"credentials",    "Stored Credentials",   "Settings & Credentials",
                  count_table("credentials"), true});
    stats.append({"kv_storage",     "Key-Value Storage",    "Settings & Credentials",
                  count_table("key_value_storage"), true});

    return stats;
}

int StorageManager::count_for(const QString& category_id) const {
    // Cache categories go through CacheDatabase
    if (category_id == "cache")
        return CacheManager::instance().entry_count();

    if (category_id == "tab_sessions") {
        auto& cdb = CacheDatabase::instance();
        if (!cdb.is_open()) return 0;
        auto r = cdb.execute("SELECT COUNT(*) FROM tab_sessions", {});
        if (r.is_err()) return 0;
        auto& q = r.value();
        return q.next() ? q.value(0).toInt() : 0;
    }

    // Map category_id → table name
    static const QHash<QString, QString> table_map = {
        {"chat_sessions",      "chat_sessions"},
        {"chat_messages",      "chat_messages"},
        {"context_recordings", "recorded_contexts"},
        {"agent_configs",      "agent_configs"},
        {"llm_configs",        "llm_configs"},
        {"llm_model_configs",  "llm_model_configs"},
        {"llm_profiles",       "llm_profiles"},
        {"news_articles",      "news_articles"},
        {"news_monitors",      "news_monitors"},
        {"rss_feeds",          "user_rss_feeds"},
        {"notes",              "financial_notes"},
        {"reports",            "reports"},
        {"portfolios",         "portfolios"},
        {"transactions",       "portfolio_transactions"},
        {"watchlists",         "watchlists"},
        {"custom_indices",     "custom_indices"},
        {"pt_portfolios",      "pt_portfolios"},
        {"pt_orders",          "pt_orders"},
        {"pt_trades",          "pt_trades"},
        {"workflows",          "workflows"},
        {"data_sources",       "data_sources"},
        {"data_mappings",      "data_mappings"},
        {"mcp_servers",        "mcp_servers"},
        {"dashboard_layouts",  "dashboard_layouts"},
        {"instruments",        "instruments"},
        {"settings",           "settings"},
        {"credentials",        "credentials"},
        {"kv_storage",         "key_value_storage"},
    };

    auto it = table_map.find(category_id);
    if (it == table_map.end()) return 0;
    return count_table(*it);
}

Result<void> StorageManager::clear_category(const QString& category_id) {
    LOG_INFO("StorageManager", "Clearing category: " + category_id);

    // --- Cache (cache.db) ---
    if (category_id == "cache") {
        CacheManager::instance().clear();
        emit category_cleared(category_id);
        return Result<void>::ok();
    }

    if (category_id == "tab_sessions") {
        auto& cdb = CacheDatabase::instance();
        if (!cdb.is_open())
            return Result<void>::err("Cache database not open");
        auto r = cdb.exec("DELETE FROM tab_sessions");
        if (r.is_ok()) emit category_cleared(category_id);
        return r;
    }

    // --- Chat: clear messages first (FK), then sessions ---
    if (category_id == "chat_sessions") {
        auto r1 = delete_from("chat_context_links");
        auto r2 = delete_from("chat_messages");
        auto r3 = delete_from("chat_sessions");
        if (r3.is_err()) return r3;
        emit category_cleared(category_id);
        return Result<void>::ok();
    }
    if (category_id == "chat_messages") {
        auto r = delete_from("chat_messages");
        if (r.is_ok()) emit category_cleared(category_id);
        return r;
    }

    // --- Context recordings ---
    if (category_id == "context_recordings") {
        auto r1 = delete_from("recording_sessions");
        auto r2 = delete_from("recorded_contexts");
        if (r2.is_err()) return r2;
        emit category_cleared(category_id);
        return Result<void>::ok();
    }

    // --- Agent configs ---
    if (category_id == "agent_configs") {
        auto r = delete_from("agent_configs");
        if (r.is_ok()) emit category_cleared(category_id);
        return r;
    }

    // --- LLM configs ---
    if (category_id == "llm_configs") {
        delete_from("llm_global_settings");
        auto r = delete_from("llm_configs");
        if (r.is_ok()) emit category_cleared(category_id);
        return r;
    }
    if (category_id == "llm_model_configs") {
        auto r = delete_from("llm_model_configs");
        if (r.is_ok()) emit category_cleared(category_id);
        return r;
    }

    // --- LLM profiles + assignments ---
    if (category_id == "llm_profiles") {
        auto r1 = delete_from("llm_profile_assignments");
        auto r2 = delete_from("llm_profiles");
        if (r2.is_err()) return r2;
        emit category_cleared(category_id);
        return Result<void>::ok();
    }

    // --- News ---
    if (category_id == "news_articles") {
        delete_from("news_entity_cache");
        delete_from("news_instability");
        delete_from("news_baselines");
        auto r = delete_from("news_articles");
        // FTS table clears automatically with content table
        if (r.is_ok()) emit category_cleared(category_id);
        return r;
    }
    if (category_id == "news_monitors") {
        auto r = delete_from("news_monitors");
        if (r.is_ok()) emit category_cleared(category_id);
        return r;
    }
    if (category_id == "rss_feeds") {
        auto r = delete_from("user_rss_feeds");
        if (r.is_ok()) emit category_cleared(category_id);
        return r;
    }

    // --- Notes ---
    if (category_id == "notes") {
        auto r = delete_from("financial_notes");
        if (r.is_ok()) emit category_cleared(category_id);
        return r;
    }

    // --- Reports ---
    if (category_id == "reports") {
        delete_from("report_templates");
        auto r = delete_from("reports");
        if (r.is_ok()) emit category_cleared(category_id);
        return r;
    }

    // --- Portfolios (cascade: snapshots, assets, transactions) ---
    if (category_id == "portfolios") {
        delete_from("portfolio_snapshots");
        delete_from("portfolio_transactions");
        delete_from("portfolio_assets");
        delete_from("portfolio_holdings");
        auto r = delete_from("portfolios");
        if (r.is_ok()) emit category_cleared(category_id);
        return r;
    }
    if (category_id == "transactions") {
        auto r = delete_from("portfolio_transactions");
        if (r.is_ok()) emit category_cleared(category_id);
        return r;
    }

    // --- Watchlists ---
    if (category_id == "watchlists") {
        delete_from("watchlist_stocks");
        auto r = delete_from("watchlists");
        if (r.is_ok()) emit category_cleared(category_id);
        return r;
    }

    // --- Custom indices ---
    if (category_id == "custom_indices") {
        delete_from("custom_index_values");
        auto r = delete_from("custom_indices");
        if (r.is_ok()) emit category_cleared(category_id);
        return r;
    }

    // --- Paper Trading ---
    if (category_id == "pt_portfolios") {
        delete_from("pt_margin_blocks");
        delete_from("pt_trades");
        delete_from("pt_positions");
        delete_from("pt_orders");
        auto r = delete_from("pt_portfolios");
        if (r.is_ok()) emit category_cleared(category_id);
        return r;
    }
    if (category_id == "pt_orders") {
        auto r = delete_from("pt_orders");
        if (r.is_ok()) emit category_cleared(category_id);
        return r;
    }
    if (category_id == "pt_trades") {
        auto r = delete_from("pt_trades");
        if (r.is_ok()) emit category_cleared(category_id);
        return r;
    }

    // --- Workflows (cascade: edges, nodes) ---
    if (category_id == "workflows") {
        delete_from("workflow_edges");
        delete_from("workflow_nodes");
        auto r = delete_from("workflows");
        if (r.is_ok()) emit category_cleared(category_id);
        return r;
    }

    // --- Data sources ---
    if (category_id == "data_sources") {
        delete_from("data_source_connections");
        delete_from("ws_provider_configs");
        auto r = delete_from("data_sources");
        if (r.is_ok()) emit category_cleared(category_id);
        return r;
    }
    if (category_id == "data_mappings") {
        delete_from("normalized_data");
        auto r = delete_from("data_mappings");
        if (r.is_ok()) emit category_cleared(category_id);
        return r;
    }

    // --- MCP servers ---
    if (category_id == "mcp_servers") {
        delete_from("mcp_tool_usage_logs");
        delete_from("mcp_tools");
        delete_from("internal_mcp_tool_settings");
        auto r = delete_from("mcp_servers");
        if (r.is_ok()) emit category_cleared(category_id);
        return r;
    }

    // --- Dashboard layouts ---
    if (category_id == "dashboard_layouts") {
        delete_from("dashboard_widget_instances");
        auto r = delete_from("dashboard_layouts");
        if (r.is_ok()) emit category_cleared(category_id);
        return r;
    }

    // --- Broker instruments ---
    if (category_id == "instruments") {
        auto r = delete_from("instruments");
        if (r.is_ok()) emit category_cleared(category_id);
        return r;
    }

    // --- Settings & Credentials ---
    if (category_id == "settings") {
        auto r = delete_from("settings");
        if (r.is_ok()) emit category_cleared(category_id);
        return r;
    }
    if (category_id == "credentials") {
        auto r = delete_from("credentials");
        if (r.is_ok()) emit category_cleared(category_id);
        return r;
    }
    if (category_id == "kv_storage") {
        auto r = delete_from("key_value_storage");
        if (r.is_ok()) emit category_cleared(category_id);
        return r;
    }

    return Result<void>::err("Unknown category: " + category_id.toStdString());
}

// ── File sizes ───────────────────────────────────────────────────────────────

qint64 StorageManager::main_db_size() const {
    QFileInfo fi(AppPaths::data() + "/fincept.db");
    if (!fi.exists()) return 0;
    // Include WAL + SHM files
    qint64 total = fi.size();
    QFileInfo wal(AppPaths::data() + "/fincept.db-wal");
    if (wal.exists()) total += wal.size();
    QFileInfo shm(AppPaths::data() + "/fincept.db-shm");
    if (shm.exists()) total += shm.size();
    return total;
}

qint64 StorageManager::cache_db_size() const {
    QFileInfo fi(AppPaths::data() + "/cache.db");
    if (!fi.exists()) return 0;
    qint64 total = fi.size();
    QFileInfo wal(AppPaths::data() + "/cache.db-wal");
    if (wal.exists()) total += wal.size();
    QFileInfo shm(AppPaths::data() + "/cache.db-shm");
    if (shm.exists()) total += shm.size();
    return total;
}

qint64 StorageManager::log_files_size() const {
    QDir dir(AppPaths::logs());
    if (!dir.exists()) return 0;
    qint64 total = 0;
    for (const auto& fi : dir.entryInfoList(QDir::Files))
        total += fi.size();
    return total;
}

Result<void> StorageManager::clear_log_files() {
    LOG_INFO("StorageManager", "Clearing log files");
    QDir dir(AppPaths::logs());
    if (!dir.exists())
        return Result<void>::ok();

    for (const auto& fi : dir.entryInfoList(QDir::Files)) {
        QFile f(fi.absoluteFilePath());
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
            f.close();
    }
    emit category_cleared("log_files");
    return Result<void>::ok();
}

qint64 StorageManager::workspace_files_size() const {
    QDir dir(AppPaths::workspaces());
    if (!dir.exists()) return 0;
    qint64 total = 0;
    for (const auto& fi : dir.entryInfoList({"*.fwsp"}, QDir::Files))
        total += fi.size();
    return total;
}

Result<void> StorageManager::clear_workspace_files() {
    LOG_INFO("StorageManager", "Clearing workspace files");
    QDir dir(AppPaths::workspaces());
    if (!dir.exists())
        return Result<void>::ok();

    for (const auto& fi : dir.entryInfoList({"*.fwsp"}, QDir::Files)) {
        QFile::remove(fi.absoluteFilePath());
    }
    emit category_cleared("workspaces");
    return Result<void>::ok();
}

Result<void> StorageManager::clear_qsettings() {
    LOG_INFO("StorageManager", "Clearing QSettings (window geometry, perspectives, UI state)");
    QSettings settings("Fincept", "FinceptTerminal");
    settings.clear();
    settings.sync();
    emit category_cleared("qsettings");
    return Result<void>::ok();
}

} // namespace fincept
