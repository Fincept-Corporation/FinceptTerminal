// Specialized Query Operations - Complex queries, MCP, Backtesting, Context Recording

use crate::database::{pool::get_pool, types::*};
use anyhow::Result;
use rusqlite::params;

// ============================================================================
// MCP Server Operations
// ============================================================================

pub fn add_mcp_server(server: &MCPServer) -> Result<()> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    conn.execute(
        "INSERT OR REPLACE INTO mcp_servers
         (id, name, description, command, args, env, category, icon, enabled, auto_start, status, updated_at)
         VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, CURRENT_TIMESTAMP)",
        params![
            server.id,
            server.name,
            server.description,
            server.command,
            server.args,
            server.env,
            server.category,
            server.icon,
            if server.enabled { 1 } else { 0 },
            if server.auto_start { 1 } else { 0 },
            server.status,
        ],
    )?;

    Ok(())
}

pub fn get_mcp_servers() -> Result<Vec<MCPServer>> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let mut stmt = conn.prepare(
        "SELECT id, name, description, command, args, env, category, icon, enabled, auto_start, status, created_at, updated_at
         FROM mcp_servers ORDER BY name"
    )?;

    let servers = stmt
        .query_map([], |row| {
            Ok(MCPServer {
                id: row.get(0)?,
                name: row.get(1)?,
                description: row.get(2)?,
                command: row.get(3)?,
                args: row.get(4)?,
                env: row.get(5)?,
                category: row.get(6)?,
                icon: row.get(7)?,
                enabled: row.get::<_, i32>(8)? != 0,
                auto_start: row.get::<_, i32>(9)? != 0,
                status: row.get(10)?,
                created_at: row.get(11)?,
                updated_at: row.get(12)?,
            })
        })?
        .collect::<std::result::Result<Vec<_>, _>>()?;

    Ok(servers)
}

pub fn delete_mcp_server(id: &str) -> Result<()> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    conn.execute("DELETE FROM mcp_servers WHERE id = ?1", params![id])?;

    Ok(())
}

// ============================================================================
// Internal MCP Tool Settings Operations
// ============================================================================

pub fn get_internal_tool_settings() -> Result<Vec<InternalMCPToolSetting>> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let mut stmt = conn.prepare(
        "SELECT tool_name, category, is_enabled, updated_at
         FROM internal_mcp_tool_settings ORDER BY category, tool_name"
    )?;

    let settings = stmt
        .query_map([], |row| {
            Ok(InternalMCPToolSetting {
                tool_name: row.get(0)?,
                category: row.get(1)?,
                is_enabled: row.get::<_, i32>(2)? != 0,
                updated_at: row.get(3)?,
            })
        })?
        .collect::<std::result::Result<Vec<_>, _>>()?;

    Ok(settings)
}

pub fn set_internal_tool_enabled(tool_name: &str, category: &str, is_enabled: bool) -> Result<()> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    conn.execute(
        "INSERT OR REPLACE INTO internal_mcp_tool_settings
         (tool_name, category, is_enabled, updated_at)
         VALUES (?1, ?2, ?3, CURRENT_TIMESTAMP)",
        params![
            tool_name,
            category,
            if is_enabled { 1 } else { 0 },
        ],
    )?;

    Ok(())
}

pub fn is_internal_tool_enabled(tool_name: &str) -> Result<bool> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let result: Result<i32, rusqlite::Error> = conn.query_row(
        "SELECT is_enabled FROM internal_mcp_tool_settings WHERE tool_name = ?1",
        params![tool_name],
        |row| row.get(0),
    );

    match result {
        Ok(enabled) => Ok(enabled != 0),
        Err(rusqlite::Error::QueryReturnedNoRows) => Ok(true), // Default to enabled if not in table
        Err(e) => Err(e.into()),
    }
}

// ============================================================================
// Backtesting Operations
// ============================================================================

pub fn save_backtesting_provider(provider: &BacktestingProvider) -> Result<OperationResult> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    conn.execute(
        "INSERT OR REPLACE INTO backtesting_providers
         (id, name, adapter_type, config, enabled, is_active, updated_at)
         VALUES (?1, ?2, ?3, ?4, ?5, ?6, CURRENT_TIMESTAMP)",
        params![
            provider.id,
            provider.name,
            provider.adapter_type,
            provider.config,
            if provider.enabled { 1 } else { 0 },
            if provider.is_active { 1 } else { 0 },
        ],
    )?;

    Ok(OperationResult {
        success: true,
        message: "Backtesting provider saved successfully".to_string(),
    })
}

pub fn get_backtesting_providers() -> Result<Vec<BacktestingProvider>> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let mut stmt = conn.prepare(
        "SELECT id, name, adapter_type, config, enabled, is_active, created_at, updated_at
         FROM backtesting_providers ORDER BY name"
    )?;

    let providers = stmt
        .query_map([], |row| {
            Ok(BacktestingProvider {
                id: row.get(0)?,
                name: row.get(1)?,
                adapter_type: row.get(2)?,
                config: row.get(3)?,
                enabled: row.get::<_, i32>(4)? != 0,
                is_active: row.get::<_, i32>(5)? != 0,
                created_at: row.get(6)?,
                updated_at: row.get(7)?,
            })
        })?
        .collect::<std::result::Result<Vec<_>, _>>()?;

    Ok(providers)
}

pub fn save_backtesting_strategy(strategy: &BacktestingStrategy) -> Result<OperationResult> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    conn.execute(
        "INSERT OR REPLACE INTO backtesting_strategies
         (id, name, description, version, author, provider_type, strategy_type, strategy_definition, tags, updated_at)
         VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, CURRENT_TIMESTAMP)",
        params![
            strategy.id,
            strategy.name,
            strategy.description,
            strategy.version,
            strategy.author,
            strategy.provider_type,
            strategy.strategy_type,
            strategy.strategy_definition,
            strategy.tags,
        ],
    )?;

    Ok(OperationResult {
        success: true,
        message: "Backtesting strategy saved successfully".to_string(),
    })
}

pub fn get_backtesting_strategies() -> Result<Vec<BacktestingStrategy>> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let mut stmt = conn.prepare(
        "SELECT id, name, description, version, author, provider_type, strategy_type, strategy_definition, tags, created_at, updated_at
         FROM backtesting_strategies ORDER BY name"
    )?;

    let strategies = stmt
        .query_map([], |row| {
            Ok(BacktestingStrategy {
                id: row.get(0)?,
                name: row.get(1)?,
                description: row.get(2)?,
                version: row.get(3)?,
                author: row.get(4)?,
                provider_type: row.get(5)?,
                strategy_type: row.get(6)?,
                strategy_definition: row.get(7)?,
                tags: row.get(8)?,
                created_at: row.get(9)?,
                updated_at: row.get(10)?,
            })
        })?
        .collect::<std::result::Result<Vec<_>, _>>()?;

    Ok(strategies)
}

pub fn save_backtest_run(run: &BacktestRun) -> Result<OperationResult> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    conn.execute(
        "INSERT INTO backtest_runs
         (id, strategy_id, provider_name, config, results, status, performance_metrics, error_message)
         VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8)",
        params![
            run.id,
            run.strategy_id,
            run.provider_name,
            run.config,
            run.results,
            run.status,
            run.performance_metrics,
            run.error_message,
        ],
    )?;

    Ok(OperationResult {
        success: true,
        message: "Backtest run saved successfully".to_string(),
    })
}

pub fn get_backtest_runs(limit: Option<i64>) -> Result<Vec<BacktestRun>> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let query = if let Some(lim) = limit {
        format!(
            "SELECT id, strategy_id, provider_name, config, results, status, performance_metrics, error_message, created_at, completed_at, duration_seconds
             FROM backtest_runs ORDER BY created_at DESC LIMIT {}",
            lim
        )
    } else {
        "SELECT id, strategy_id, provider_name, config, results, status, performance_metrics, error_message, created_at, completed_at, duration_seconds
         FROM backtest_runs ORDER BY created_at DESC"
            .to_string()
    };

    let mut stmt = conn.prepare(&query)?;
    let runs = stmt
        .query_map([], |row| {
            Ok(BacktestRun {
                id: row.get(0)?,
                strategy_id: row.get(1)?,
                provider_name: row.get(2)?,
                config: row.get(3)?,
                results: row.get(4)?,
                status: row.get(5)?,
                performance_metrics: row.get(6)?,
                error_message: row.get(7)?,
                created_at: row.get(8)?,
                completed_at: row.get(9)?,
                duration_seconds: row.get(10)?,
            })
        })?
        .collect::<std::result::Result<Vec<_>, _>>()?;

    Ok(runs)
}

// ============================================================================
// Context Recording Operations
// ============================================================================

pub fn save_recorded_context(context: &RecordedContext) -> Result<()> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    conn.execute(
        "INSERT INTO recorded_contexts
         (id, tab_name, data_type, label, raw_data, metadata, data_size, tags)
         VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8)",
        params![
            context.id,
            context.tab_name,
            context.data_type,
            context.label,
            context.raw_data,
            context.metadata,
            context.data_size,
            context.tags,
        ],
    )?;

    Ok(())
}

pub fn get_recorded_contexts(tab_name: Option<&str>, limit: Option<i64>) -> Result<Vec<RecordedContext>> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let query = match (tab_name, limit) {
        (Some(_), Some(lim)) => format!(
            "SELECT id, tab_name, data_type, label, raw_data, metadata, data_size, created_at, tags
             FROM recorded_contexts WHERE tab_name = ?1 ORDER BY created_at DESC LIMIT {}",
            lim
        ),
        (Some(_), None) => {
            "SELECT id, tab_name, data_type, label, raw_data, metadata, data_size, created_at, tags
             FROM recorded_contexts WHERE tab_name = ?1 ORDER BY created_at DESC"
                .to_string()
        }
        (None, Some(lim)) => format!(
            "SELECT id, tab_name, data_type, label, raw_data, metadata, data_size, created_at, tags
             FROM recorded_contexts ORDER BY created_at DESC LIMIT {}",
            lim
        ),
        (None, None) => {
            "SELECT id, tab_name, data_type, label, raw_data, metadata, data_size, created_at, tags
             FROM recorded_contexts ORDER BY created_at DESC"
                .to_string()
        }
    };

    let mut stmt = conn.prepare(&query)?;

    let contexts = if let Some(tab) = tab_name {
        stmt.query_map(params![tab], |row| {
            Ok(RecordedContext {
                id: row.get(0)?,
                tab_name: row.get(1)?,
                data_type: row.get(2)?,
                label: row.get(3)?,
                raw_data: row.get(4)?,
                metadata: row.get(5)?,
                data_size: row.get(6)?,
                created_at: row.get(7)?,
                tags: row.get(8)?,
            })
        })?
        .collect::<std::result::Result<Vec<_>, _>>()?
    } else {
        stmt.query_map([], |row| {
            Ok(RecordedContext {
                id: row.get(0)?,
                tab_name: row.get(1)?,
                data_type: row.get(2)?,
                label: row.get(3)?,
                raw_data: row.get(4)?,
                metadata: row.get(5)?,
                data_size: row.get(6)?,
                created_at: row.get(7)?,
                tags: row.get(8)?,
            })
        })?
        .collect::<std::result::Result<Vec<_>, _>>()?
    };

    Ok(contexts)
}

pub fn delete_recorded_context(id: &str) -> Result<()> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    conn.execute("DELETE FROM recorded_contexts WHERE id = ?1", params![id])?;

    Ok(())
}

// ============================================================================
// Watchlist Operations
// ============================================================================

pub fn create_watchlist(name: &str, description: Option<&str>, color: &str) -> Result<Watchlist> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let id = uuid::Uuid::new_v4().to_string();

    conn.execute(
        "INSERT INTO watchlists (id, name, description, color) VALUES (?1, ?2, ?3, ?4)",
        params![id, name, description, color],
    )?;

    let watchlist = conn.query_row(
        "SELECT id, name, description, color, created_at, updated_at FROM watchlists WHERE id = ?1",
        params![id],
        |row| {
            Ok(Watchlist {
                id: row.get(0)?,
                name: row.get(1)?,
                description: row.get(2)?,
                color: row.get(3)?,
                created_at: row.get(4)?,
                updated_at: row.get(5)?,
            })
        },
    )?;

    Ok(watchlist)
}

pub fn get_watchlists() -> Result<Vec<Watchlist>> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let mut stmt = conn.prepare(
        "SELECT id, name, description, color, created_at, updated_at
         FROM watchlists ORDER BY created_at DESC"
    )?;

    let watchlists = stmt
        .query_map([], |row| {
            Ok(Watchlist {
                id: row.get(0)?,
                name: row.get(1)?,
                description: row.get(2)?,
                color: row.get(3)?,
                created_at: row.get(4)?,
                updated_at: row.get(5)?,
            })
        })?
        .collect::<std::result::Result<Vec<_>, _>>()?;

    Ok(watchlists)
}

pub fn add_watchlist_stock(watchlist_id: &str, symbol: &str, notes: Option<&str>) -> Result<WatchlistStock> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let id = uuid::Uuid::new_v4().to_string();

    conn.execute(
        "INSERT INTO watchlist_stocks (id, watchlist_id, symbol, notes) VALUES (?1, ?2, ?3, ?4)",
        params![id, watchlist_id, symbol.to_uppercase(), notes],
    )?;

    let stock = conn.query_row(
        "SELECT id, watchlist_id, symbol, added_at, notes FROM watchlist_stocks WHERE id = ?1",
        params![id],
        |row| {
            Ok(WatchlistStock {
                id: row.get(0)?,
                watchlist_id: row.get(1)?,
                symbol: row.get(2)?,
                added_at: row.get(3)?,
                notes: row.get(4)?,
            })
        },
    )?;

    Ok(stock)
}

pub fn get_watchlist_stocks(watchlist_id: &str) -> Result<Vec<WatchlistStock>> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let mut stmt = conn.prepare(
        "SELECT id, watchlist_id, symbol, added_at, notes
         FROM watchlist_stocks WHERE watchlist_id = ?1 ORDER BY added_at DESC"
    )?;

    let stocks = stmt
        .query_map(params![watchlist_id], |row| {
            Ok(WatchlistStock {
                id: row.get(0)?,
                watchlist_id: row.get(1)?,
                symbol: row.get(2)?,
                added_at: row.get(3)?,
                notes: row.get(4)?,
            })
        })?
        .collect::<std::result::Result<Vec<_>, _>>()?;

    Ok(stocks)
}

pub fn remove_watchlist_stock(watchlist_id: &str, symbol: &str) -> Result<()> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    conn.execute(
        "DELETE FROM watchlist_stocks WHERE watchlist_id = ?1 AND symbol = ?2",
        params![watchlist_id, symbol.to_uppercase()],
    )?;

    Ok(())
}

pub fn delete_watchlist(watchlist_id: &str) -> Result<()> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    conn.execute("DELETE FROM watchlists WHERE id = ?1", params![watchlist_id])?;

    Ok(())
}

// ============================================================================
// Agent Configuration Operations
// ============================================================================

pub fn save_agent_config(config: &AgentConfig) -> Result<OperationResult> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    conn.execute(
        "INSERT OR REPLACE INTO agent_configs
         (id, name, description, config_json, category, is_default, is_active, updated_at)
         VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, CURRENT_TIMESTAMP)",
        params![
            config.id,
            config.name,
            config.description,
            config.config_json,
            config.category,
            if config.is_default { 1 } else { 0 },
            if config.is_active { 1 } else { 0 },
        ],
    )?;

    Ok(OperationResult {
        success: true,
        message: "Agent configuration saved successfully".to_string(),
    })
}

pub fn get_agent_configs() -> Result<Vec<AgentConfig>> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let mut stmt = conn.prepare(
        "SELECT id, name, description, config_json, category, is_default, is_active, created_at, updated_at
         FROM agent_configs ORDER BY name"
    )?;

    let configs = stmt
        .query_map([], |row| {
            Ok(AgentConfig {
                id: row.get(0)?,
                name: row.get(1)?,
                description: row.get(2)?,
                config_json: row.get(3)?,
                category: row.get(4)?,
                is_default: row.get::<_, i32>(5)? != 0,
                is_active: row.get::<_, i32>(6)? != 0,
                created_at: row.get(7)?,
                updated_at: row.get(8)?,
            })
        })?
        .collect::<std::result::Result<Vec<_>, _>>()?;

    Ok(configs)
}

pub fn get_agent_config(id: &str) -> Result<Option<AgentConfig>> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let config = conn.query_row(
        "SELECT id, name, description, config_json, category, is_default, is_active, created_at, updated_at
         FROM agent_configs WHERE id = ?1",
        params![id],
        |row| {
            Ok(AgentConfig {
                id: row.get(0)?,
                name: row.get(1)?,
                description: row.get(2)?,
                config_json: row.get(3)?,
                category: row.get(4)?,
                is_default: row.get::<_, i32>(5)? != 0,
                is_active: row.get::<_, i32>(6)? != 0,
                created_at: row.get(7)?,
                updated_at: row.get(8)?,
            })
        },
    );

    match config {
        Ok(c) => Ok(Some(c)),
        Err(rusqlite::Error::QueryReturnedNoRows) => Ok(None),
        Err(e) => Err(e.into()),
    }
}

pub fn get_agent_configs_by_category(category: &str) -> Result<Vec<AgentConfig>> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let mut stmt = conn.prepare(
        "SELECT id, name, description, config_json, category, is_default, is_active, created_at, updated_at
         FROM agent_configs WHERE category = ?1 ORDER BY name"
    )?;

    let configs = stmt
        .query_map(params![category], |row| {
            Ok(AgentConfig {
                id: row.get(0)?,
                name: row.get(1)?,
                description: row.get(2)?,
                config_json: row.get(3)?,
                category: row.get(4)?,
                is_default: row.get::<_, i32>(5)? != 0,
                is_active: row.get::<_, i32>(6)? != 0,
                created_at: row.get(7)?,
                updated_at: row.get(8)?,
            })
        })?
        .collect::<std::result::Result<Vec<_>, _>>()?;

    Ok(configs)
}

pub fn delete_agent_config(id: &str) -> Result<OperationResult> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let rows_affected = conn.execute("DELETE FROM agent_configs WHERE id = ?1", params![id])?;

    if rows_affected > 0 {
        Ok(OperationResult {
            success: true,
            message: "Agent configuration deleted successfully".to_string(),
        })
    } else {
        Ok(OperationResult {
            success: false,
            message: "Agent configuration not found".to_string(),
        })
    }
}

pub fn set_active_agent_config(id: &str) -> Result<OperationResult> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    // First, deactivate all configs
    conn.execute("UPDATE agent_configs SET is_active = 0", [])?;

    // Then activate the specified one
    let rows_affected = conn.execute(
        "UPDATE agent_configs SET is_active = 1, updated_at = CURRENT_TIMESTAMP WHERE id = ?1",
        params![id],
    )?;

    if rows_affected > 0 {
        Ok(OperationResult {
            success: true,
            message: "Agent configuration activated".to_string(),
        })
    } else {
        Ok(OperationResult {
            success: false,
            message: "Agent configuration not found".to_string(),
        })
    }
}

pub fn get_active_agent_config() -> Result<Option<AgentConfig>> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let config = conn.query_row(
        "SELECT id, name, description, config_json, category, is_default, is_active, created_at, updated_at
         FROM agent_configs WHERE is_active = 1 LIMIT 1",
        [],
        |row| {
            Ok(AgentConfig {
                id: row.get(0)?,
                name: row.get(1)?,
                description: row.get(2)?,
                config_json: row.get(3)?,
                category: row.get(4)?,
                is_default: row.get::<_, i32>(5)? != 0,
                is_active: row.get::<_, i32>(6)? != 0,
                created_at: row.get(7)?,
                updated_at: row.get(8)?,
            })
        },
    );

    match config {
        Ok(c) => Ok(Some(c)),
        Err(rusqlite::Error::QueryReturnedNoRows) => Ok(None),
        Err(e) => Err(e.into()),
    }
}
