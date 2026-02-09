#![allow(dead_code)]
// Candle Aggregator Service — builds OHLCV candles from tick stream
//
// Subscribes to the ticker broadcast channel and aggregates ticks into
// candles of various timeframes. When a candle closes (new tick falls
// into the next window), the closed candle is written to the
// `candle_cache` table and an `algo_candle_closed` Tauri event is emitted.

use crate::database::pool::get_pool;
use crate::websocket::types::TickerData;
use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use std::sync::Arc;
use tokio::sync::RwLock;
use tauri::Emitter;

// ============================================================================
// TYPES
// ============================================================================

/// In-memory state for a single open candle
#[derive(Debug, Clone)]
struct CandleState {
    symbol: String,
    provider: String,
    timeframe: String,
    open_time: u64, // epoch seconds, start of candle window
    o: f64,
    h: f64,
    l: f64,
    c: f64,
    volume: f64,
    tick_count: u64,
}

/// A subscription request: track this symbol+timeframe pair
#[derive(Debug, Clone, Hash, Eq, PartialEq)]
pub struct AggregationKey {
    pub symbol: String,
    pub timeframe: String,
}

/// Closed candle emitted as Tauri event
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ClosedCandle {
    pub symbol: String,
    pub provider: String,
    pub timeframe: String,
    pub open_time: u64,
    pub o: f64,
    pub h: f64,
    pub l: f64,
    pub c: f64,
    pub volume: f64,
}

// ============================================================================
// CANDLE AGGREGATOR SERVICE
// ============================================================================

pub struct CandleAggregatorService {
    /// Active aggregation subscriptions: (symbol, timeframe) -> CandleState
    candles: Arc<RwLock<HashMap<AggregationKey, CandleState>>>,
    /// Set of active subscriptions (which symbol+timeframe pairs to aggregate)
    subscriptions: Arc<RwLock<HashMap<AggregationKey, ()>>>,
    app_handle: Option<tauri::AppHandle>,
}

impl Default for CandleAggregatorService {
    fn default() -> Self {
        Self::new()
    }
}

impl CandleAggregatorService {
    pub fn new() -> Self {
        Self {
            candles: Arc::new(RwLock::new(HashMap::new())),
            subscriptions: Arc::new(RwLock::new(HashMap::new())),
            app_handle: None,
        }
    }

    pub fn set_app_handle(&mut self, app_handle: tauri::AppHandle) {
        self.app_handle = Some(app_handle);
    }

    /// Register a symbol+timeframe pair for aggregation
    pub async fn add_subscription(&self, symbol: String, timeframe: String) {
        let key = AggregationKey {
            symbol: symbol.clone(),
            timeframe: timeframe.clone(),
        };
        self.subscriptions.write().await.insert(key, ());
        println!(
            "[CandleAggregator] Subscribed: {} @ {}",
            symbol, timeframe
        );
    }

    /// Unregister a symbol+timeframe pair
    pub async fn remove_subscription(&self, symbol: String, timeframe: String) {
        let key = AggregationKey {
            symbol: symbol.clone(),
            timeframe: timeframe.clone(),
        };
        self.subscriptions.write().await.remove(&key);
        self.candles.write().await.remove(&key);
        println!(
            "[CandleAggregator] Unsubscribed: {} @ {}",
            symbol, timeframe
        );
    }

    /// Start listening for ticker data from the broadcast channel
    pub fn start_aggregation(
        &self,
        mut ticker_rx: tokio::sync::broadcast::Receiver<TickerData>,
    ) {
        let candles = self.candles.clone();
        let subscriptions = self.subscriptions.clone();
        let app_handle = self.app_handle.clone();

        println!("[CandleAggregator] Starting tick aggregation loop...");

        tokio::spawn(async move {
            loop {
                match ticker_rx.recv().await {
                    Ok(ticker) => {
                        Self::process_tick(
                            &candles,
                            &subscriptions,
                            &app_handle,
                            &ticker,
                        )
                        .await;
                    }
                    Err(tokio::sync::broadcast::error::RecvError::Lagged(n)) => {
                        println!(
                            "[CandleAggregator] Channel lagged, skipped {} messages",
                            n
                        );
                        continue;
                    }
                    Err(_) => {
                        println!("[CandleAggregator] Channel closed — stopping aggregation");
                        break;
                    }
                }
            }
            println!("[CandleAggregator] Aggregation loop exited");
        });
    }

    /// Process a single tick: update in-memory candle or close+create new
    async fn process_tick(
        candles: &Arc<RwLock<HashMap<AggregationKey, CandleState>>>,
        subscriptions: &Arc<RwLock<HashMap<AggregationKey, ()>>>,
        app_handle: &Option<tauri::AppHandle>,
        ticker: &TickerData,
    ) {
        let subs = subscriptions.read().await;
        if subs.is_empty() {
            return;
        }

        // Find all subscriptions matching this symbol
        let matching_keys: Vec<AggregationKey> = subs
            .keys()
            .filter(|k| k.symbol == ticker.symbol)
            .cloned()
            .collect();
        drop(subs);

        if matching_keys.is_empty() {
            return;
        }

        let price = ticker.price;
        let volume = ticker.volume.unwrap_or(0.0);
        let tick_ts = ticker.timestamp; // epoch seconds

        for key in matching_keys {
            let tf_seconds = timeframe_to_seconds(&key.timeframe);
            if tf_seconds == 0 {
                continue;
            }

            let candle_open_time = (tick_ts / tf_seconds) * tf_seconds;

            let mut candle_map = candles.write().await;

            if let Some(existing) = candle_map.get_mut(&key) {
                if candle_open_time == existing.open_time {
                    // Same candle window — update OHLCV
                    if price > existing.h {
                        existing.h = price;
                    }
                    if price < existing.l {
                        existing.l = price;
                    }
                    existing.c = price;
                    existing.volume += volume;
                    existing.tick_count += 1;
                } else {
                    // New candle window — close the old one
                    let closed = ClosedCandle {
                        symbol: existing.symbol.clone(),
                        provider: existing.provider.clone(),
                        timeframe: existing.timeframe.clone(),
                        open_time: existing.open_time,
                        o: existing.o,
                        h: existing.h,
                        l: existing.l,
                        c: existing.c,
                        volume: existing.volume,
                    };

                    // Write to DB
                    Self::write_candle_to_db(&closed);

                    // Emit event to frontend
                    if let Some(ref handle) = app_handle {
                        let _ = handle.emit("algo_candle_closed", &closed);
                    }

                    // Start new candle
                    *existing = CandleState {
                        symbol: ticker.symbol.clone(),
                        provider: ticker.provider.clone(),
                        timeframe: key.timeframe.clone(),
                        open_time: candle_open_time,
                        o: price,
                        h: price,
                        l: price,
                        c: price,
                        volume,
                        tick_count: 1,
                    };
                }
            } else {
                // First tick for this key — start new candle
                candle_map.insert(
                    key.clone(),
                    CandleState {
                        symbol: ticker.symbol.clone(),
                        provider: ticker.provider.clone(),
                        timeframe: key.timeframe.clone(),
                        open_time: candle_open_time,
                        o: price,
                        h: price,
                        l: price,
                        c: price,
                        volume,
                        tick_count: 1,
                    },
                );
            }
        }
    }

    /// Write a closed candle to the candle_cache table
    fn write_candle_to_db(candle: &ClosedCandle) {
        let candle = candle.clone();
        // Spawn blocking task to avoid blocking the async runtime
        tokio::task::spawn_blocking(move || {
            let pool = match get_pool() {
                Ok(p) => p,
                Err(e) => {
                    eprintln!("[CandleAggregator] DB pool error: {}", e);
                    return;
                }
            };
            let conn = match pool.get() {
                Ok(c) => c,
                Err(e) => {
                    eprintln!("[CandleAggregator] DB conn error: {}", e);
                    return;
                }
            };

            if let Err(e) = conn.execute(
                "INSERT OR REPLACE INTO candle_cache
                    (symbol, provider, timeframe, open_time, o, h, l, c, volume, is_closed)
                 VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, 1)",
                rusqlite::params![
                    candle.symbol,
                    candle.provider,
                    candle.timeframe,
                    candle.open_time,
                    candle.o,
                    candle.h,
                    candle.l,
                    candle.c,
                    candle.volume,
                ],
            ) {
                eprintln!("[CandleAggregator] DB write error: {}", e);
            }
        });
    }
}

// ============================================================================
// HELPERS
// ============================================================================

/// Convert timeframe string to seconds
fn timeframe_to_seconds(tf: &str) -> u64 {
    match tf {
        "1m" => 60,
        "3m" => 180,
        "5m" => 300,
        "15m" => 900,
        "30m" => 1800,
        "1h" => 3600,
        "4h" => 14400,
        "1d" => 86400,
        _ => {
            // Try parsing numeric suffix
            if let Some(num_str) = tf.strip_suffix('m') {
                num_str.parse::<u64>().unwrap_or(0) * 60
            } else if let Some(num_str) = tf.strip_suffix('h') {
                num_str.parse::<u64>().unwrap_or(0) * 3600
            } else if let Some(num_str) = tf.strip_suffix('d') {
                num_str.parse::<u64>().unwrap_or(0) * 86400
            } else {
                0
            }
        }
    }
}
