// Monitoring Service - Real-time market data monitoring and alerting
//
// Monitors WebSocket data streams against user-defined conditions
// and triggers alerts when conditions are met.

use crate::websocket::types::*;
use anyhow::Result;
use rusqlite::{params, Connection};
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

pub struct MonitoringService {
    conditions: Arc<RwLock<Vec<MonitorCondition>>>,
    db_path: String,
    app_handle: Option<tauri::AppHandle>,
}

impl MonitoringService {
    pub fn new(db_path: String) -> Self {
        Self {
            conditions: Arc::new(RwLock::new(Vec::new())),
            db_path,
            app_handle: None,
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
        let db_path = self.db_path.clone();
        let app_handle = self.app_handle.clone();

        tokio::spawn(async move {
            loop {
                match ticker_rx.recv().await {
                    Ok(ticker) => {
                        // Create temporary service to check conditions
                        let service = MonitoringService {
                            conditions: conditions.clone(),
                            db_path: db_path.clone(),
                            app_handle: app_handle.clone(),
                        };

                        let alerts = service.check_ticker(&ticker).await;

                        // Emit alerts to frontend
                        if !alerts.is_empty() && app_handle.is_some() {
                            for alert in alerts {
                                let _ = app_handle.as_ref().unwrap().emit("monitor_alert", &alert);
                            }
                        }
                    }
                    Err(tokio::sync::broadcast::error::RecvError::Lagged(_)) => {
                        // Channel lagged, continue
                        continue;
                    }
                    Err(_) => {
                        // Channel closed
                        break;
                    }
                }
            }
        });
    }

    /// Load all enabled conditions from database
    pub async fn load_conditions(&self) -> Result<()> {
        let db_path = self.db_path.clone();

        // Use spawn_blocking for SQLite operations
        let conditions = tokio::task::spawn_blocking(move || -> Result<Vec<MonitorCondition>> {
            let conn = Connection::open(&db_path)?;

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

            Ok(conditions)
        })
        .await
        .map_err(|e| anyhow::anyhow!("Join error: {}", e))??;

        *self.conditions.write().await = conditions;
        Ok(())
    }

    /// Check ticker data against all conditions
    pub async fn check_ticker(&self, ticker: &TickerData) -> Vec<MonitorAlert> {
        let conditions = self.conditions.read().await;
        let mut alerts = Vec::new();

        for condition in conditions.iter() {
            // Filter by provider and symbol
            if condition.provider != ticker.provider || condition.symbol != ticker.symbol {
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
                if self.check_condition(value, condition) {
                    // Condition matched - create alert
                    alerts.push(MonitorAlert {
                        id: None,
                        condition_id: condition.id.unwrap(),
                        provider: ticker.provider.clone(),
                        symbol: ticker.symbol.clone(),
                        field: condition.field.clone(),
                        triggered_value: value,
                        triggered_at: Self::now(),
                    });
                }
            }
        }

        // Save alerts to database
        if !alerts.is_empty() {
            let _ = self.save_alerts(&alerts).await;
        }

        alerts
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

    /// Save alerts to database
    async fn save_alerts(&self, alerts: &[MonitorAlert]) -> Result<()> {
        let conn = Connection::open(&self.db_path)?;

        for alert in alerts {
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
        Self::new("fincept_terminal.db".to_string())
    }
}
