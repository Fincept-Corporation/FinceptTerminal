// Polymarket prediction markets data commands
use crate::python;
use crate::database::pool::get_pool;
use rusqlite::params;
use serde::{Deserialize, Serialize};

#[tauri::command]
pub async fn fetch_polymarket_markets(app: tauri::AppHandle, limit: Option<u32>) -> Result<String, String> {
    let limit_str = limit.unwrap_or(10).to_string();
    python::execute(&app, "polymarket.py", vec!["get_markets".to_string(), limit_str]).await
}

// ── Bot Persistence Commands ───────────────────────────────────────────────────

#[derive(Debug, Serialize, Deserialize)]
pub struct PolymarketBotRow {
    pub id: String,
    pub name: String,
    pub status: String,
    pub strategy_json: String,
    pub risk_config_json: String,
    pub llm_config_json: String,
    pub stats_json: String,
    pub last_error: Option<String>,
    pub created_at: String,
    pub updated_at: String,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct PolymarketDecisionRow {
    pub id: String,
    pub bot_id: String,
    pub timestamp: String,
    pub market_id: String,
    pub market_question: String,
    pub action: String,
    pub outcome: Option<String>,
    pub token_id: Option<String>,
    pub confidence: f64,
    pub reasoning: Option<String>,
    pub risk_factors: Option<String>,
    pub price_at_decision: f64,
    pub assessed_probability: f64,
    pub recommended_size_usdc: f64,
    pub executed: bool,
    pub pending_approval: bool,
    pub approved: Option<bool>,
    pub order_id: Option<String>,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct PolymarketPositionRow {
    pub id: String,
    pub bot_id: String,
    pub market_id: String,
    pub market_question: String,
    pub outcome: String,
    pub token_id: String,
    pub entry_price: f64,
    pub current_price: f64,
    pub size_usdc: f64,
    pub order_id: Option<String>,
    pub opened_at: String,
    pub closed_at: Option<String>,
    pub close_reason: Option<String>,
    pub realized_pnl_usdc: Option<f64>,
    pub unrealized_pnl_pct: f64,
}

/// Upsert a bot (create or update)
#[tauri::command]
pub async fn pm_bot_upsert(bot: PolymarketBotRow) -> Result<(), String> {
    let pool = get_pool().map_err(|e| e.to_string())?;
    let conn = pool.get().map_err(|e| e.to_string())?;
    conn.execute(
        "INSERT INTO polymarket_bots
            (id, name, status, strategy_json, risk_config_json, llm_config_json, stats_json, last_error, created_at, updated_at)
         VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, CURRENT_TIMESTAMP)
         ON CONFLICT(id) DO UPDATE SET
            name = excluded.name,
            status = excluded.status,
            strategy_json = excluded.strategy_json,
            risk_config_json = excluded.risk_config_json,
            llm_config_json = excluded.llm_config_json,
            stats_json = excluded.stats_json,
            last_error = excluded.last_error,
            updated_at = CURRENT_TIMESTAMP",
        params![
            bot.id, bot.name, bot.status,
            bot.strategy_json, bot.risk_config_json, bot.llm_config_json,
            bot.stats_json, bot.last_error, bot.created_at,
        ],
    ).map_err(|e| e.to_string())?;
    Ok(())
}

/// Get all bots
#[tauri::command]
pub async fn pm_bot_get_all() -> Result<Vec<PolymarketBotRow>, String> {
    let pool = get_pool().map_err(|e| e.to_string())?;
    let conn = pool.get().map_err(|e| e.to_string())?;
    let mut stmt = conn.prepare(
        "SELECT id, name, status, strategy_json, risk_config_json, llm_config_json, stats_json, last_error, created_at, updated_at
         FROM polymarket_bots ORDER BY created_at DESC"
    ).map_err(|e| e.to_string())?;
    let rows = stmt.query_map([], |row| {
        Ok(PolymarketBotRow {
            id: row.get(0)?,
            name: row.get(1)?,
            status: row.get(2)?,
            strategy_json: row.get(3)?,
            risk_config_json: row.get(4)?,
            llm_config_json: row.get(5)?,
            stats_json: row.get(6)?,
            last_error: row.get(7)?,
            created_at: row.get(8)?,
            updated_at: row.get(9)?,
        })
    }).map_err(|e| e.to_string())?;
    rows.collect::<Result<Vec<_>, _>>().map_err(|e| e.to_string())
}

/// Delete a bot and all its decisions/positions (CASCADE)
#[tauri::command]
pub async fn pm_bot_delete(bot_id: String) -> Result<(), String> {
    let pool = get_pool().map_err(|e| e.to_string())?;
    let conn = pool.get().map_err(|e| e.to_string())?;
    conn.execute("DELETE FROM polymarket_bots WHERE id = ?1", params![bot_id])
        .map_err(|e| e.to_string())?;
    Ok(())
}

/// Insert a decision record
#[tauri::command]
pub async fn pm_decision_insert(d: PolymarketDecisionRow) -> Result<(), String> {
    let pool = get_pool().map_err(|e| e.to_string())?;
    let conn = pool.get().map_err(|e| e.to_string())?;
    conn.execute(
        "INSERT OR IGNORE INTO polymarket_bot_decisions
            (id, bot_id, timestamp, market_id, market_question, action, outcome, token_id,
             confidence, reasoning, risk_factors, price_at_decision, assessed_probability,
             recommended_size_usdc, executed, pending_approval, approved, order_id)
         VALUES (?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13,?14,?15,?16,?17,?18)",
        params![
            d.id, d.bot_id, d.timestamp, d.market_id, d.market_question,
            d.action, d.outcome, d.token_id, d.confidence, d.reasoning,
            d.risk_factors, d.price_at_decision, d.assessed_probability,
            d.recommended_size_usdc,
            d.executed as i32, d.pending_approval as i32,
            d.approved.map(|b| b as i32),
            d.order_id,
        ],
    ).map_err(|e| e.to_string())?;
    Ok(())
}

/// Update decision approval status
#[tauri::command]
pub async fn pm_decision_update_approval(decision_id: String, approved: bool, order_id: Option<String>) -> Result<(), String> {
    let pool = get_pool().map_err(|e| e.to_string())?;
    let conn = pool.get().map_err(|e| e.to_string())?;
    conn.execute(
        "UPDATE polymarket_bot_decisions
         SET approved = ?1, pending_approval = 0, executed = ?2, order_id = COALESCE(?3, order_id)
         WHERE id = ?4",
        params![approved as i32, approved as i32, order_id, decision_id],
    ).map_err(|e| e.to_string())?;
    Ok(())
}

/// Get decisions for a bot (most recent first, with optional limit)
#[tauri::command]
pub async fn pm_decisions_get(bot_id: String, limit: Option<u32>) -> Result<Vec<PolymarketDecisionRow>, String> {
    let pool = get_pool().map_err(|e| e.to_string())?;
    let conn = pool.get().map_err(|e| e.to_string())?;
    let lim = limit.unwrap_or(50) as i64;
    let mut stmt = conn.prepare(
        "SELECT id, bot_id, timestamp, market_id, market_question, action, outcome, token_id,
                confidence, reasoning, risk_factors, price_at_decision, assessed_probability,
                recommended_size_usdc, executed, pending_approval, approved, order_id
         FROM polymarket_bot_decisions WHERE bot_id = ?1 ORDER BY timestamp DESC LIMIT ?2"
    ).map_err(|e| e.to_string())?;
    let rows = stmt.query_map(params![bot_id, lim], |row| {
        Ok(PolymarketDecisionRow {
            id: row.get(0)?,
            bot_id: row.get(1)?,
            timestamp: row.get(2)?,
            market_id: row.get(3)?,
            market_question: row.get(4)?,
            action: row.get(5)?,
            outcome: row.get(6)?,
            token_id: row.get(7)?,
            confidence: row.get(8)?,
            reasoning: row.get(9)?,
            risk_factors: row.get(10)?,
            price_at_decision: row.get(11)?,
            assessed_probability: row.get(12)?,
            recommended_size_usdc: row.get(13)?,
            executed: row.get::<_, i32>(14)? != 0,
            pending_approval: row.get::<_, i32>(15)? != 0,
            approved: row.get::<_, Option<i32>>(16)?.map(|v| v != 0),
            order_id: row.get(17)?,
        })
    }).map_err(|e| e.to_string())?;
    rows.collect::<Result<Vec<_>, _>>().map_err(|e| e.to_string())
}

/// Get pending approval decisions across all bots
#[tauri::command]
pub async fn pm_decisions_pending() -> Result<Vec<PolymarketDecisionRow>, String> {
    let pool = get_pool().map_err(|e| e.to_string())?;
    let conn = pool.get().map_err(|e| e.to_string())?;
    let mut stmt = conn.prepare(
        "SELECT id, bot_id, timestamp, market_id, market_question, action, outcome, token_id,
                confidence, reasoning, risk_factors, price_at_decision, assessed_probability,
                recommended_size_usdc, executed, pending_approval, approved, order_id
         FROM polymarket_bot_decisions WHERE pending_approval = 1 ORDER BY timestamp ASC"
    ).map_err(|e| e.to_string())?;
    let rows = stmt.query_map([], |row| {
        Ok(PolymarketDecisionRow {
            id: row.get(0)?,
            bot_id: row.get(1)?,
            timestamp: row.get(2)?,
            market_id: row.get(3)?,
            market_question: row.get(4)?,
            action: row.get(5)?,
            outcome: row.get(6)?,
            token_id: row.get(7)?,
            confidence: row.get(8)?,
            reasoning: row.get(9)?,
            risk_factors: row.get(10)?,
            price_at_decision: row.get(11)?,
            assessed_probability: row.get(12)?,
            recommended_size_usdc: row.get(13)?,
            executed: row.get::<_, i32>(14)? != 0,
            pending_approval: row.get::<_, i32>(15)? != 0,
            approved: row.get::<_, Option<i32>>(16)?.map(|v| v != 0),
            order_id: row.get(17)?,
        })
    }).map_err(|e| e.to_string())?;
    rows.collect::<Result<Vec<_>, _>>().map_err(|e| e.to_string())
}

/// Upsert a position record
#[tauri::command]
pub async fn pm_position_upsert(p: PolymarketPositionRow) -> Result<(), String> {
    let pool = get_pool().map_err(|e| e.to_string())?;
    let conn = pool.get().map_err(|e| e.to_string())?;
    conn.execute(
        "INSERT INTO polymarket_bot_positions
            (id, bot_id, market_id, market_question, outcome, token_id, entry_price, current_price,
             size_usdc, order_id, opened_at, closed_at, close_reason, realized_pnl_usdc, unrealized_pnl_pct)
         VALUES (?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13,?14,?15)
         ON CONFLICT(id) DO UPDATE SET
            current_price = excluded.current_price,
            closed_at = excluded.closed_at,
            close_reason = excluded.close_reason,
            realized_pnl_usdc = excluded.realized_pnl_usdc,
            unrealized_pnl_pct = excluded.unrealized_pnl_pct",
        params![
            p.id, p.bot_id, p.market_id, p.market_question, p.outcome, p.token_id,
            p.entry_price, p.current_price, p.size_usdc, p.order_id, p.opened_at,
            p.closed_at, p.close_reason, p.realized_pnl_usdc, p.unrealized_pnl_pct,
        ],
    ).map_err(|e| e.to_string())?;
    Ok(())
}

/// Get open positions for a bot
#[tauri::command]
pub async fn pm_positions_get(bot_id: String, open_only: Option<bool>) -> Result<Vec<PolymarketPositionRow>, String> {
    let pool = get_pool().map_err(|e| e.to_string())?;
    let conn = pool.get().map_err(|e| e.to_string())?;
    let sql = if open_only.unwrap_or(false) {
        "SELECT id, bot_id, market_id, market_question, outcome, token_id, entry_price, current_price,
                size_usdc, order_id, opened_at, closed_at, close_reason, realized_pnl_usdc, unrealized_pnl_pct
         FROM polymarket_bot_positions WHERE bot_id = ?1 AND closed_at IS NULL ORDER BY opened_at DESC"
    } else {
        "SELECT id, bot_id, market_id, market_question, outcome, token_id, entry_price, current_price,
                size_usdc, order_id, opened_at, closed_at, close_reason, realized_pnl_usdc, unrealized_pnl_pct
         FROM polymarket_bot_positions WHERE bot_id = ?1 ORDER BY opened_at DESC"
    };
    let mut stmt = conn.prepare(sql).map_err(|e| e.to_string())?;
    let rows = stmt.query_map(params![bot_id], |row| {
        Ok(PolymarketPositionRow {
            id: row.get(0)?,
            bot_id: row.get(1)?,
            market_id: row.get(2)?,
            market_question: row.get(3)?,
            outcome: row.get(4)?,
            token_id: row.get(5)?,
            entry_price: row.get(6)?,
            current_price: row.get(7)?,
            size_usdc: row.get(8)?,
            order_id: row.get(9)?,
            opened_at: row.get(10)?,
            closed_at: row.get(11)?,
            close_reason: row.get(12)?,
            realized_pnl_usdc: row.get(13)?,
            unrealized_pnl_pct: row.get(14)?,
        })
    }).map_err(|e| e.to_string())?;
    rows.collect::<Result<Vec<_>, _>>().map_err(|e| e.to_string())
}
