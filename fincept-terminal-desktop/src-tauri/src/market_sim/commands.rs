use serde::{Deserialize, Serialize};
use std::sync::Mutex;
use std::time::Duration;
use once_cell::sync::Lazy;
use crate::market_sim::types::*;
use crate::market_sim::exchange::{Exchange, create_default_simulation};
use crate::market_sim::analytics::AnalyticsSummary;

/// Global simulation state
static SIM_STATE: Lazy<Mutex<Option<Exchange>>> = Lazy::new(|| Mutex::new(None));

/// Helper to acquire lock with retry to prevent UI freezing
fn acquire_state_lock() -> Result<std::sync::MutexGuard<'static, Option<Exchange>>, String> {
    // Try to acquire lock immediately first
    match SIM_STATE.try_lock() {
        Ok(guard) => return Ok(guard),
        Err(std::sync::TryLockError::WouldBlock) => {
            // Lock is held, retry a few times with small delays
            for _ in 0..10 {
                std::thread::sleep(Duration::from_millis(10));
                if let Ok(guard) = SIM_STATE.try_lock() {
                    return Ok(guard);
                }
            }
            Err("Simulation is busy. Please try again.".to_string())
        }
        Err(std::sync::TryLockError::Poisoned(e)) => {
            Err(format!("Lock poisoned: {}", e))
        }
    }
}

#[derive(Debug, Serialize, Deserialize)]
pub struct SimStartRequest {
    pub name: Option<String>,
    pub instruments: Option<Vec<InstrumentConfig>>,
    pub seed: Option<u64>,
    pub tick_interval_ms: Option<u64>,
    pub enable_circuit_breakers: Option<bool>,
    pub enable_auctions: Option<bool>,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct InstrumentConfig {
    pub symbol: String,
    pub reference_price: f64,
    pub tick_size: f64,
    pub volatility: Option<f64>,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct SimResponse<T: Serialize> {
    pub success: bool,
    pub data: Option<T>,
    pub error: Option<String>,
}

impl<T: Serialize> SimResponse<T> {
    pub fn ok(data: T) -> Self {
        Self { success: true, data: Some(data), error: None }
    }

    pub fn err(msg: impl Into<String>) -> Self {
        Self { success: false, data: None, error: Some(msg.into()) }
    }
}

/// Start a new market simulation
#[tauri::command]
pub fn market_sim_start(request: SimStartRequest) -> SimResponse<SimulationSnapshot> {
    let mut state = match acquire_state_lock() {
        Ok(s) => s,
        Err(e) => return SimResponse::err(e),
    };

    // Create exchange with default config if no custom instruments
    let exchange = if request.instruments.is_some() && !request.instruments.as_ref().unwrap().is_empty() {
        let instruments: Vec<Instrument> = request.instruments.unwrap().iter().enumerate().map(|(idx, ic)| {
            Instrument {
                id: (idx + 1) as InstrumentId,
                symbol: ic.symbol.clone(),
                tick_size: (ic.tick_size * 100.0) as Price,
                lot_size: 1,
                min_quantity: 1,
                max_quantity: 1_000_000,
                price_band_pct: 0.20,
                reference_price: (ic.reference_price * 100.0) as Price,
                maker_fee_bps: -0.2,
                taker_fee_bps: 0.3,
                short_sell_allowed: true,
                tick_size_table: vec![],
            }
        }).collect();

        let config = SimulationConfig {
            name: request.name.unwrap_or_else(|| "Custom Simulation".to_string()),
            instruments,
            random_seed: request.seed.unwrap_or(42),
            tick_interval_nanos: request.tick_interval_ms.unwrap_or(1) * 1_000_000,
            enable_circuit_breakers: request.enable_circuit_breakers.unwrap_or(true),
            enable_auctions: request.enable_auctions.unwrap_or(true),
            ..Default::default()
        };

        Exchange::new(config)
    } else {
        create_default_simulation()
    };

    let snapshot = exchange.snapshot();
    *state = Some(exchange);

    SimResponse::ok(snapshot)
}

/// Step the simulation forward by N ticks
#[tauri::command]
pub fn market_sim_step(ticks: u64) -> SimResponse<SimulationSnapshot> {
    let mut state = match acquire_state_lock() {
        Ok(s) => s,
        Err(e) => return SimResponse::err(e),
    };

    match state.as_mut() {
        Some(exchange) => {
            let snapshot = exchange.step(ticks);
            SimResponse::ok(snapshot)
        }
        None => SimResponse::err("No simulation running. Call market_sim_start first."),
    }
}

/// Run the full simulation to completion
#[tauri::command]
pub fn market_sim_run() -> SimResponse<SimulationSnapshot> {
    let mut state = match acquire_state_lock() {
        Ok(s) => s,
        Err(e) => return SimResponse::err(e),
    };

    match state.as_mut() {
        Some(exchange) => {
            let snapshot = exchange.run();
            SimResponse::ok(snapshot)
        }
        None => SimResponse::err("No simulation running. Call market_sim_start first."),
    }
}

/// Get current simulation snapshot
#[tauri::command]
pub fn market_sim_snapshot() -> SimResponse<SimulationSnapshot> {
    let state = match acquire_state_lock() {
        Ok(s) => s,
        Err(e) => return SimResponse::err(e),
    };

    match state.as_ref() {
        Some(exchange) => SimResponse::ok(exchange.snapshot()),
        None => SimResponse::err("No simulation running."),
    }
}

/// Get detailed analytics
#[tauri::command]
pub fn market_sim_analytics() -> SimResponse<AnalyticsSummary> {
    let state = match acquire_state_lock() {
        Ok(s) => s,
        Err(e) => return SimResponse::err(e),
    };

    match state.as_ref() {
        Some(exchange) => SimResponse::ok(exchange.analytics.summary()),
        None => SimResponse::err("No simulation running."),
    }
}

/// Get L2 order book depth for an instrument
#[tauri::command]
pub fn market_sim_orderbook(instrument_id: u32, depth: usize) -> SimResponse<L2Snapshot> {
    let state = match acquire_state_lock() {
        Ok(s) => s,
        Err(e) => return SimResponse::err(e),
    };

    match state.as_ref() {
        Some(exchange) => {
            match exchange.matching_engine.get_book(instrument_id) {
                Some(book) => {
                    let snapshot = book.l2_snapshot(depth, exchange.current_time);
                    SimResponse::ok(snapshot)
                }
                None => SimResponse::err(format!("Instrument {} not found", instrument_id)),
            }
        }
        None => SimResponse::err("No simulation running."),
    }
}

/// Inject a news event into the simulation
#[tauri::command]
pub fn market_sim_inject_news(
    headline: String,
    sentiment: f64,
    magnitude: f64,
    instrument_ids: Vec<u32>,
) -> SimResponse<String> {
    let mut state = match acquire_state_lock() {
        Ok(s) => s,
        Err(e) => return SimResponse::err(e),
    };

    match state.as_mut() {
        Some(exchange) => {
            exchange.inject_news(headline.clone(), sentiment, magnitude, instrument_ids);
            SimResponse::ok(format!("News injected: {}", headline))
        }
        None => SimResponse::err("No simulation running."),
    }
}

/// Stop and reset the simulation
#[tauri::command]
pub fn market_sim_stop() -> SimResponse<String> {
    let mut state = match acquire_state_lock() {
        Ok(s) => s,
        Err(e) => return SimResponse::err(e),
    };

    *state = None;
    SimResponse::ok("Simulation stopped".to_string())
}

/// Get event log summary (last N events)
#[tauri::command]
pub fn market_sim_events(count: usize) -> SimResponse<Vec<String>> {
    let state = match acquire_state_lock() {
        Ok(s) => s,
        Err(e) => return SimResponse::err(e),
    };

    match state.as_ref() {
        Some(exchange) => {
            let events: Vec<String> = exchange.event_log.events()
                .iter()
                .rev()
                .take(count)
                .map(|e| format!("{:?}", e))
                .collect();
            SimResponse::ok(events)
        }
        None => SimResponse::err("No simulation running."),
    }
}
