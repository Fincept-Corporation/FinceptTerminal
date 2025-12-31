/// Tauri commands for frontend integration
use crate::barter_integration::*;
use tauri::State;
use std::sync::Arc;
use tokio::sync::Mutex;

/// Global state for barter integration
pub struct BarterState {
    pub market_data: Arc<Mutex<market_data::MarketDataManager>>,
    pub execution: Arc<Mutex<execution::ExecutionManager>>,
    pub strategy_registry: Arc<Mutex<strategy::StrategyRegistry>>,
}

impl BarterState {
    pub fn new(mode: TradingMode) -> Self {
        Self {
            market_data: Arc::new(Mutex::new(market_data::MarketDataManager::new())),
            execution: Arc::new(Mutex::new(execution::ExecutionManager::new(mode))),
            strategy_registry: Arc::new(Mutex::new(strategy::StrategyRegistry::default())),
        }
    }
}

// ====================
// Market Data Commands
// ====================

#[tauri::command]
pub async fn start_market_stream(
    config: MarketStreamConfig,
    state: State<'_, BarterState>,
) -> Result<String, String> {
    state
        .market_data
        .lock()
        .await
        .start_stream(config)
        .await
        .map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn stop_market_stream(
    stream_id: String,
    state: State<'_, BarterState>,
) -> Result<(), String> {
    state
        .market_data
        .lock()
        .await
        .stop_stream(&stream_id)
        .await
        .map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn get_active_streams(state: State<'_, BarterState>) -> Result<Vec<String>, String> {
    Ok(state.market_data.lock().await.get_active_streams())
}

#[tauri::command]
pub async fn subscribe_symbols(
    stream_id: String,
    symbols: Vec<String>,
    state: State<'_, BarterState>,
) -> Result<(), String> {
    state
        .market_data
        .lock()
        .await
        .subscribe(&stream_id, symbols)
        .await
        .map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn unsubscribe_symbols(
    stream_id: String,
    symbols: Vec<String>,
    state: State<'_, BarterState>,
) -> Result<(), String> {
    state
        .market_data
        .lock()
        .await
        .unsubscribe(&stream_id, symbols)
        .await
        .map_err(|e| e.to_string())
}

// ====================
// Execution Commands
// ====================

#[tauri::command]
pub async fn submit_order(
    request: OrderRequest,
    state: State<'_, BarterState>,
) -> Result<OrderResponse, String> {
    state
        .execution
        .lock()
        .await
        .submit_order(request)
        .await
        .map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn cancel_order(
    order_id: String,
    state: State<'_, BarterState>,
) -> Result<OrderResponse, String> {
    state
        .execution
        .lock()
        .await
        .cancel_order(&order_id)
        .await
        .map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn get_order(order_id: String, state: State<'_, BarterState>) -> Result<OrderResponse, String> {
    state
        .execution
        .lock()
        .await
        .get_order(&order_id)
        .map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn get_all_orders(state: State<'_, BarterState>) -> Result<Vec<OrderResponse>, String> {
    Ok(state.execution.lock().await.get_all_orders())
}

#[tauri::command]
pub async fn get_open_orders(state: State<'_, BarterState>) -> Result<Vec<OrderResponse>, String> {
    Ok(state.execution.lock().await.get_open_orders())
}

#[tauri::command]
pub async fn get_positions(state: State<'_, BarterState>) -> Result<Vec<Position>, String> {
    Ok(state.execution.lock().await.get_positions())
}

#[tauri::command]
pub async fn get_position(
    exchange: Exchange,
    symbol: String,
    state: State<'_, BarterState>,
) -> Result<Option<Position>, String> {
    Ok(state.execution.lock().await.get_position(&exchange, &symbol))
}

#[tauri::command]
pub async fn get_balances(state: State<'_, BarterState>) -> Result<Vec<Balance>, String> {
    Ok(state.execution.lock().await.get_balances())
}

#[tauri::command]
pub async fn close_all_positions(
    state: State<'_, BarterState>,
) -> Result<Vec<OrderResponse>, String> {
    state
        .execution
        .lock()
        .await
        .close_all_positions()
        .await
        .map_err(|e| e.to_string())
}

// ====================
// Strategy Commands
// ====================

#[tauri::command]
pub async fn list_strategies(state: State<'_, BarterState>) -> Result<Vec<String>, String> {
    Ok(state.strategy_registry.lock().await.list_strategies())
}

#[tauri::command]
pub async fn get_strategy_parameters(
    strategy_name: String,
    state: State<'_, BarterState>,
) -> Result<std::collections::HashMap<String, serde_json::Value>, String> {
    let registry = state.strategy_registry.lock().await;
    registry
        .get(&strategy_name)
        .map(|s| s.get_parameters())
        .ok_or_else(|| format!("Strategy not found: {}", strategy_name))
}

#[tauri::command]
pub async fn update_strategy_parameters(
    strategy_name: String,
    parameters: std::collections::HashMap<String, serde_json::Value>,
    state: State<'_, BarterState>,
) -> Result<(), String> {
    let mut registry = state.strategy_registry.lock().await;
    let strategy = registry
        .get_mut(&strategy_name)
        .ok_or_else(|| format!("Strategy not found: {}", strategy_name))?;
    strategy.as_mut().update_parameters(parameters)
        .map_err(|e| e.to_string())
}

// ====================
// Backtest Commands
// ====================

#[tauri::command]
pub async fn run_backtest(
    config: BacktestConfig,
    candle_data: std::collections::HashMap<String, Vec<Candle>>,
    _state: State<'_, BarterState>,
) -> Result<BacktestResult, String> {
    // Create a temporary strategy instance
    let mut strategy = strategy::SmaStrategy::new();

    backtest::BacktestRunner::run_backtest(config, &mut strategy, candle_data)
        .await
        .map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn optimize_strategy(
    config: BacktestConfig,
    parameter_grid: std::collections::HashMap<String, Vec<serde_json::Value>>,
    candle_data: std::collections::HashMap<String, Vec<Candle>>,
) -> Result<Vec<(std::collections::HashMap<String, serde_json::Value>, BacktestResult)>, String> {
    backtest::BacktestRunner::optimize_parameters(
        config,
        "SMA Crossover",
        parameter_grid,
        candle_data,
    )
    .await
    .map_err(|e| e.to_string())
}

// ====================
// Utility Commands
// ====================

#[tauri::command]
pub fn get_supported_exchanges() -> Vec<String> {
    vec![
        "binance".to_string(),
        "bybit".to_string(),
        "coinbase".to_string(),
        "kraken".to_string(),
        "okx".to_string(),
        "bitfinex".to_string(),
        "bitmex".to_string(),
        "gateio".to_string(),
    ]
}

#[tauri::command]
pub fn get_trading_modes() -> Vec<String> {
    vec![
        "live".to_string(),
        "paper".to_string(),
        "backtest".to_string(),
    ]
}

// ====================
// Data Mapping Commands
// ====================

#[tauri::command]
pub async fn parse_databento_dataset(
    file_path: String,
) -> Result<std::collections::HashMap<String, Vec<Candle>>, String> {
    data_mapper::parse_databento_file(&file_path)
        .map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn map_custom_dataset(
    mapping: data_mapper::DatasetMapping,
    rows: Vec<std::collections::HashMap<String, String>>,
    symbol: String,
) -> Result<Vec<Candle>, String> {
    let mapper = data_mapper::DataMapper::new(mapping);
    let data_rows: Vec<data_mapper::DataRow> = rows
        .into_iter()
        .map(|fields| data_mapper::DataRow { fields })
        .collect();

    mapper.map_ohlcv_data(data_rows, &symbol)
        .map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn map_tick_dataset(
    mapping: data_mapper::DatasetMapping,
    rows: Vec<std::collections::HashMap<String, String>>,
) -> Result<std::collections::HashMap<String, Vec<Candle>>, String> {
    let mapper = data_mapper::DataMapper::new(mapping);
    let data_rows: Vec<data_mapper::DataRow> = rows
        .into_iter()
        .map(|fields| data_mapper::DataRow { fields })
        .collect();

    mapper.map_tick_data(data_rows)
        .map_err(|e| e.to_string())
}
