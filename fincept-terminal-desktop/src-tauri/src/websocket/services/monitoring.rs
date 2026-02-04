#![allow(dead_code)]
// Monitoring Service - Real-time market data monitoring and alerting
//
// Monitors WebSocket data streams against user-defined conditions
// and triggers alerts when conditions are met.

use crate::database::pool::get_pool;
use crate::websocket::types::*;
use anyhow::Result;
use rusqlite::params;
use serde::{Deserialize, Serialize};
use std::sync::Arc;
use std::time::{SystemTime, UNIX_EPOCH};
use tokio::sync::RwLock;
use tauri::Emitter;

// ============================================================================
// TYPES
// ============================================================================

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MonitorCondition {
    pub id: Option<i64>,
    pub provider: String,
    pub symbol: String,
    pub field: MonitorField,
    pub operator: MonitorOperator,
    pub value: f64,
    pub value2: Option<f64>, // For 'between' operator
    pub enabled: bool,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "snake_case")]
pub enum MonitorField {
    Price,
    Volume,
    ChangePercent,
    Spread,
}

impl MonitorField {
    pub fn as_str(&self) -> &str {
        match self {
            Self::Price => "price",
            Self::Volume => "volume",
            Self::ChangePercent => "change_percent",
            Self::Spread => "spread",
        }
    }

    pub fn from_str(s: &str) -> Option<Self> {
        match s {
            "price" => Some(Self::Price),
            "volume" => Some(Self::Volume),
            "change_percent" => Some(Self::ChangePercent),
            "spread" => Some(Self::Spread),
            _ => None,
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub enum MonitorOperator {
    #[serde(rename = ">")]
    GreaterThan,
    #[serde(rename = "<")]
    LessThan,
    #[serde(rename = ">=")]
    GreaterThanOrEqual,
    #[serde(rename = "<=")]
    LessThanOrEqual,
    #[serde(rename = "==")]
    Equal,
    #[serde(rename = "between")]
    Between,
}

impl MonitorOperator {
    pub fn as_str(&self) -> &str {
        match self {
            Self::GreaterThan => ">",
            Self::LessThan => "<",
            Self::GreaterThanOrEqual => ">=",
            Self::LessThanOrEqual => "<=",
            Self::Equal => "==",
            Self::Between => "between",
        }
    }

    pub fn from_str(s: &str) -> Option<Self> {
        match s {
            ">" => Some(Self::GreaterThan),
            "<" => Some(Self::LessThan),
            ">=" => Some(Self::GreaterThanOrEqual),
            "<=" => Some(Self::LessThanOrEqual),
            "==" => Some(Self::Equal),
            "between" => Some(Self::Between),
            _ => None,
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MonitorAlert {
    pub id: Option<i64>,
    pub condition_id: i64,
    pub provider: String,
    pub symbol: String,
    pub field: MonitorField,
    pub triggered_value: f64,
    pub triggered_at: u64,
}

// ============================================================================
// MONITORING SERVICE
// ============================================================================

/// Per-condition state for crossing detection
#[derive(Debug, Clone)]
struct ConditionState {
    /// Last observed value for this condition (to detect crossing)
    prev_value: Option<f64>,
    /// Whether the condition was matching on the previous tick
    was_matching: bool,
    /// Whether the condition has already fired (fire-once)
    fired: bool,
}

pub struct MonitoringService {
    conditions: Arc<RwLock<Vec<MonitorCondition>>>,
    app_handle: Option<tauri::AppHandle>,
    /// Per-condition crossing state: condition_id → ConditionState
    condition_states: Arc<RwLock<std::collections::HashMap<i64, ConditionState>>>,
}

impl MonitoringService {
    pub fn new() -> Self {
        Self {
            conditions: Arc::new(RwLock::new(Vec::new())),
            app_handle: None,
            condition_states: Arc::new(RwLock::new(std::collections::HashMap::new())),
        }
    }

    /// Set app handle for emitting alerts to frontend
    pub fn set_app_handle(&mut self, app_handle: tauri::AppHandle) {
        self.app_handle = Some(app_handle);
    }

    /// Start monitoring ticker stream from router
    pub fn start_monitoring(
        &self,
        mut ticker_rx: tokio::sync::broadcast::Receiver<TickerData>,
    ) {
        let conditions = self.conditions.clone();
        let app_handle = self.app_handle.clone();
        let condition_states = self.condition_states.clone();

        println!("[MonitoringService] start_monitoring called — waiting for WebSocket ticker data...");

        tokio::spawn(async move {
            println!("[MonitoringService] Monitoring task spawned, listening on broadcast channel");
            loop {
                match ticker_rx.recv().await {
                    Ok(ticker) => {
                        // Create temporary service to check conditions
                        let service = MonitoringService {
                            conditions: conditions.clone(),
                            app_handle: app_handle.clone(),
                            condition_states: condition_states.clone(),
                        };

                        let alerts = service.check_ticker(&ticker).await;

                        if !alerts.is_empty() {
                            println!(
                                "[MonitoringService] {} alert(s) triggered — provider={} symbol={} price={}",
                                alerts.len(), ticker.provider, ticker.symbol, ticker.price
                            );
                            if app_handle.is_some() {
                                for alert in alerts {
                                    let _ = app_handle.as_ref().unwrap().emit("monitor_alert", &alert);
                                }
                            }
                        }
                    }
                    Err(tokio::sync::broadcast::error::RecvError::Lagged(n)) => {
                        println!("[MonitoringService] Channel lagged, skipped {} messages", n);
                        continue;
                    }
                    Err(e) => {
                        println!("[MonitoringService] Channel closed: {:?} — stopping monitoring", e);
                        break;
                    }
                }
            }
            println!("[MonitoringService] Monitoring loop exited");
        });
    }

    /// Load all enabled conditions from database (uses shared connection pool)
    pub async fn load_conditions(&self) -> Result<()> {
        let conditions = tokio::task::spawn_blocking(move || -> Result<Vec<MonitorCondition>> {
            let pool = get_pool()?;
            let conn = pool.get().map_err(|e| anyhow::anyhow!("Pool error: {}", e))?;

            let mut stmt = conn.prepare(
                "SELECT id, provider, symbol, field, operator, value, value2, enabled
                 FROM monitor_conditions
                 WHERE enabled = 1"
            )?;

            let conditions = stmt
                .query_map([], |row| {
                    Ok(MonitorCondition {
                        id: Some(row.get(0)?),
                        provider: row.get(1)?,
                        symbol: row.get(2)?,
                        field: MonitorField::from_str(&row.get::<_, String>(3)?).unwrap(),
                        operator: MonitorOperator::from_str(&row.get::<_, String>(4)?).unwrap(),
                        value: row.get(5)?,
                        value2: row.get(6)?,
                        enabled: row.get::<_, i32>(7)? == 1,
                    })
                })?
                .collect::<std::result::Result<Vec<_>, _>>()?;

            println!("[MonitoringService] Loaded {} enabled conditions from DB", conditions.len());
            Ok(conditions)
        })
        .await
        .map_err(|e| anyhow::anyhow!("Join error: {}", e))??;

        *self.conditions.write().await = conditions;
        // Clear crossing state so reloaded/new conditions start fresh
        self.condition_states.write().await.clear();
        Ok(())
    }

    /// Check ticker data against all conditions using crossing detection.
    ///
    /// Crossing logic:
    /// - On the FIRST tick for a condition, we record whether it matches or not (no alert).
    /// - If the price is already on the "matching" side, we wait for it to go to the
    ///   "non-matching" side first, then cross back → trigger.
    /// - Once triggered, the condition is marked `fired` and never fires again.
    pub async fn check_ticker(&self, ticker: &TickerData) -> Vec<MonitorAlert> {
        let conditions = self.conditions.read().await;
        let mut states = self.condition_states.write().await;
        let mut alerts = Vec::new();

        // Capture the exact time NOW before any processing
        let trigger_time = Self::now();

        for condition in conditions.iter() {
            let cond_id = match condition.id {
                Some(id) => id,
                None => continue,
            };

            // Filter by provider and symbol
            if condition.provider != ticker.provider || condition.symbol != ticker.symbol {
                continue;
            }

            // Get or create state for this condition
            let state = states.entry(cond_id).or_insert(ConditionState {
                prev_value: None,
                was_matching: false,
                fired: false,
            });

            // Already fired — skip forever
            if state.fired {
                continue;
            }

            // Extract field value
            let field_value = match condition.field {
                MonitorField::Price => Some(ticker.price),
                MonitorField::Volume => ticker.volume,
                MonitorField::ChangePercent => ticker.change_percent,
                MonitorField::Spread => {
                    if let (Some(bid), Some(ask)) = (ticker.bid, ticker.ask) {
                        Some(ask - bid)
                    } else {
                        None
                    }
                }
            };

            if let Some(value) = field_value {
                let is_matching = self.check_condition(value, condition);

                if state.prev_value.is_none() {
                    // First tick — just record state, don't trigger
                    // If already matching, we wait for it to go non-matching first
                    state.prev_value = Some(value);
                    state.was_matching = is_matching;
                    continue;
                }

                // Crossing detection: was NOT matching → now IS matching
                if !state.was_matching && is_matching {
                    state.fired = true;
                    alerts.push(MonitorAlert {
                        id: None,
                        condition_id: cond_id,
                        provider: ticker.provider.clone(),
                        symbol: ticker.symbol.clone(),
                        field: condition.field.clone(),
                        triggered_value: value,
                        triggered_at: trigger_time,
                    });
                }

                // Update state
                state.prev_value = Some(value);
                state.was_matching = is_matching;
            }
        }

        // Save alerts and delete triggered conditions
        if !alerts.is_empty() {
            let _ = self.save_alerts(&alerts).await;

            // Collect triggered condition IDs
            let fired_ids: Vec<i64> = alerts.iter().map(|a| a.condition_id).collect();

            // Delete triggered conditions from DB
            let _ = Self::delete_conditions_from_db(&fired_ids).await;

            // Remove from in-memory list (need write lock on conditions)
            drop(conditions); // release read lock
            let mut conds = self.conditions.write().await;
            conds.retain(|c| c.id.map_or(true, |id| !fired_ids.contains(&id)));
        }

        alerts
    }

    /// Delete conditions from the database by their IDs
    async fn delete_conditions_from_db(ids: &[i64]) -> Result<()> {
        let ids_owned: Vec<i64> = ids.to_vec();
        tokio::task::spawn_blocking(move || -> Result<()> {
            let pool = get_pool()?;
            let conn = pool.get().map_err(|e| anyhow::anyhow!("Pool error: {}", e))?;
            for id in &ids_owned {
                conn.execute("DELETE FROM monitor_conditions WHERE id = ?1", params![id])?;
            }
            Ok(())
        })
        .await
        .map_err(|e| anyhow::anyhow!("Join error: {}", e))?
    }

    /// Check if a value matches a condition
    fn check_condition(&self, value: f64, condition: &MonitorCondition) -> bool {
        match condition.operator {
            MonitorOperator::GreaterThan => value > condition.value,
            MonitorOperator::LessThan => value < condition.value,
            MonitorOperator::GreaterThanOrEqual => value >= condition.value,
            MonitorOperator::LessThanOrEqual => value <= condition.value,
            MonitorOperator::Equal => (value - condition.value).abs() < f64::EPSILON,
            MonitorOperator::Between => {
                if let Some(value2) = condition.value2 {
                    value >= condition.value && value <= value2
                } else {
                    false
                }
            }
        }
    }

    /// Save alerts to database (uses shared connection pool)
    async fn save_alerts(&self, alerts: &[MonitorAlert]) -> Result<()> {
        let alerts_owned: Vec<MonitorAlert> = alerts.to_vec();
        tokio::task::spawn_blocking(move || -> Result<()> {
            let pool = get_pool()?;
            let conn = pool.get().map_err(|e| anyhow::anyhow!("Pool error: {}", e))?;

            for alert in &alerts_owned {
                conn.execute(
                    "INSERT INTO monitor_alerts (condition_id, provider, symbol, field, triggered_value, triggered_at)
                     VALUES (?1, ?2, ?3, ?4, ?5, ?6)",
                    params![
                        alert.condition_id,
                        &alert.provider,
                        &alert.symbol,
                        alert.field.as_str(),
                        alert.triggered_value,
                        alert.triggered_at as i64,
                    ],
                )?;
            }

            Ok(())
        })
        .await
        .map_err(|e| anyhow::anyhow!("Join error: {}", e))??;

        Ok(())
    }

    fn now() -> u64 {
        SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .unwrap()
            .as_millis() as u64
    }
}

impl Default for MonitoringService {
    fn default() -> Self {
        Self::new()
    }
}
