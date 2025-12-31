use serde::{Deserialize, Serialize};
use rust_decimal::Decimal;
use chrono::{DateTime, Utc};
use std::collections::HashMap;

/// Trading mode
#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "lowercase")]
pub enum TradingMode {
    Live,
    Paper,
    Backtest,
}

/// Order side
#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "lowercase")]
pub enum OrderSide {
    Buy,
    Sell,
}

/// Order type
#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "snake_case")]
pub enum OrderType {
    Market,
    Limit,
    StopLoss,
    StopLimit,
}

/// Order status
#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "snake_case")]
pub enum OrderStatus {
    Pending,
    Open,
    PartiallyFilled,
    Filled,
    Cancelled,
    Rejected,
    Expired,
}

/// Exchange identifier
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq, Hash)]
#[serde(rename_all = "lowercase")]
pub enum Exchange {
    Binance,
    Bybit,
    Coinbase,
    Kraken,
    Okx,
    Bitfinex,
    Bitmex,
    Gateio,
}

/// Market data candlestick
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Candle {
    pub timestamp: DateTime<Utc>,
    pub open: Decimal,
    pub high: Decimal,
    pub low: Decimal,
    pub close: Decimal,
    pub volume: Decimal,
}

/// Order book level
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct OrderBookLevel {
    pub price: Decimal,
    pub quantity: Decimal,
}

/// Order book snapshot
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct OrderBook {
    pub timestamp: DateTime<Utc>,
    pub bids: Vec<OrderBookLevel>,
    pub asks: Vec<OrderBookLevel>,
}

/// Trade data
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Trade {
    pub id: String,
    pub timestamp: DateTime<Utc>,
    pub price: Decimal,
    pub quantity: Decimal,
    pub side: OrderSide,
}

/// Order request from frontend
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct OrderRequest {
    pub exchange: Exchange,
    pub symbol: String,
    pub side: OrderSide,
    pub order_type: OrderType,
    pub quantity: Decimal,
    pub price: Option<Decimal>,
    pub stop_price: Option<Decimal>,
    pub time_in_force: Option<String>,
}

/// Order response
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct OrderResponse {
    pub order_id: String,
    pub client_order_id: String,
    pub exchange: Exchange,
    pub symbol: String,
    pub side: OrderSide,
    pub order_type: OrderType,
    pub status: OrderStatus,
    pub quantity: Decimal,
    pub filled_quantity: Decimal,
    pub price: Option<Decimal>,
    pub average_price: Option<Decimal>,
    pub created_at: DateTime<Utc>,
    pub updated_at: DateTime<Utc>,
}

/// Position information
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Position {
    pub exchange: Exchange,
    pub symbol: String,
    pub side: OrderSide,
    pub quantity: Decimal,
    pub entry_price: Decimal,
    pub current_price: Decimal,
    pub unrealized_pnl: Decimal,
    pub realized_pnl: Decimal,
}

/// Account balance
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Balance {
    pub asset: String,
    pub free: Decimal,
    pub locked: Decimal,
    pub total: Decimal,
}

/// Strategy configuration
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct StrategyConfig {
    pub name: String,
    pub mode: TradingMode,
    pub exchanges: Vec<Exchange>,
    pub symbols: Vec<String>,
    pub parameters: HashMap<String, serde_json::Value>,
}

/// Backtest configuration
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BacktestConfig {
    pub strategy: StrategyConfig,
    pub start_date: DateTime<Utc>,
    pub end_date: DateTime<Utc>,
    pub initial_capital: Decimal,
    pub commission_rate: Decimal,
}

/// Backtest result
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BacktestResult {
    pub total_return: Decimal,
    pub sharpe_ratio: Decimal,
    pub sortino_ratio: Decimal,
    pub max_drawdown: Decimal,
    pub win_rate: Decimal,
    pub total_trades: u64,
    pub winning_trades: u64,
    pub losing_trades: u64,
    pub average_win: Decimal,
    pub average_loss: Decimal,
    pub profit_factor: Decimal,
    pub final_capital: Decimal,
    pub equity_curve: Vec<(DateTime<Utc>, Decimal)>,
    pub trades: Vec<TradeRecord>,
}

/// Trade record for backtest
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TradeRecord {
    pub entry_time: DateTime<Utc>,
    pub exit_time: DateTime<Utc>,
    pub symbol: String,
    pub side: OrderSide,
    pub entry_price: Decimal,
    pub exit_price: Decimal,
    pub quantity: Decimal,
    pub pnl: Decimal,
    pub return_pct: Decimal,
}

/// Market stream configuration
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MarketStreamConfig {
    pub exchange: Exchange,
    pub symbols: Vec<String>,
    pub subscribe_trades: bool,
    pub subscribe_orderbook: bool,
    pub subscribe_candles: bool,
    pub candle_interval: Option<String>,
}

/// Error type for barter operations
#[derive(Debug, thiserror::Error)]
pub enum BarterError {
    #[error("Exchange error: {0}")]
    Exchange(String),

    #[error("Order error: {0}")]
    Order(String),

    #[error("Strategy error: {0}")]
    Strategy(String),

    #[error("Market data error: {0}")]
    MarketData(String),

    #[error("Backtest error: {0}")]
    Backtest(String),

    #[error("Configuration error: {0}")]
    Config(String),

    #[error("Internal error: {0}")]
    Internal(String),
}

impl Serialize for BarterError {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde::Serializer,
    {
        serializer.serialize_str(&self.to_string())
    }
}

pub type BarterResult<T> = Result<T, BarterError>;
