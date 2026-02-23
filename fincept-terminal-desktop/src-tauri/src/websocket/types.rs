#![allow(dead_code)]
// WebSocket Type Definitions
// Normalized message formats for all providers

use serde::{Deserialize, Serialize};
use std::collections::HashMap;

// ============================================================================
// CORE MESSAGE TYPES
// ============================================================================

/// Normalized market message - all providers convert to this format
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(tag = "type", rename_all = "lowercase")]
pub enum MarketMessage {
    Ticker(TickerData),
    OrderBook(OrderBookData),
    Trade(TradeData),
    Candle(CandleData),
    Status(StatusData),
}

/// Ticker/Quote data
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TickerData {
    pub provider: String,
    pub symbol: String,
    pub price: f64,
    pub bid: Option<f64>,
    pub ask: Option<f64>,
    pub bid_size: Option<f64>,
    pub ask_size: Option<f64>,
    pub volume: Option<f64>,
    pub high: Option<f64>,
    pub low: Option<f64>,
    pub open: Option<f64>,
    pub close: Option<f64>,
    pub change: Option<f64>,
    pub change_percent: Option<f64>,
    #[serde(default)]
    pub quote_volume: Option<f64>,
    pub timestamp: u64,
}

/// Order book data (market depth)
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct OrderBookData {
    pub provider: String,
    pub symbol: String,
    pub bids: Vec<OrderBookLevel>,
    pub asks: Vec<OrderBookLevel>,
    pub timestamp: u64,
    pub is_snapshot: bool,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct OrderBookLevel {
    pub price: f64,
    pub quantity: f64,
    pub count: Option<u32>,
}

/// Trade execution data
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TradeData {
    pub provider: String,
    pub symbol: String,
    pub trade_id: Option<String>,
    pub price: f64,
    pub quantity: f64,
    pub side: TradeSide,
    pub timestamp: u64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "lowercase")]
pub enum TradeSide {
    Buy,
    Sell,
    Unknown,
}

/// OHLCV candle data
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CandleData {
    pub provider: String,
    pub symbol: String,
    pub open: f64,
    pub high: f64,
    pub low: f64,
    pub close: f64,
    pub volume: f64,
    pub timestamp: u64,
    pub interval: String,
}

/// Connection/status data
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct StatusData {
    pub provider: String,
    pub status: ConnectionStatus,
    pub message: Option<String>,
    pub timestamp: u64,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "lowercase")]
pub enum ConnectionStatus {
    Connected,
    Connecting,
    Disconnected,
    Reconnecting,
    Error,
}

// ============================================================================
// SUBSCRIPTION TYPES
// ============================================================================

/// Subscription request
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SubscriptionRequest {
    pub provider: String,
    pub symbol: String,
    pub channels: Vec<String>,  // ["ticker", "book", "trade", "candle"]
    pub params: Option<HashMap<String, serde_json::Value>>,
}

/// Subscription info
#[derive(Debug, Clone)]
pub struct Subscription {
    pub id: String,
    pub provider: String,
    pub symbol: String,
    pub channel: String,
    pub params: Option<HashMap<String, serde_json::Value>>,
}

// ============================================================================
// PROVIDER CONFIG
// ============================================================================

/// Provider configuration
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ProviderConfig {
    pub name: String,
    pub url: String,
    pub api_key: Option<String>,
    pub api_secret: Option<String>,
    pub client_id: Option<String>,
    pub enabled: bool,
    pub reconnect_delay_ms: u64,
    pub max_reconnect_attempts: u32,
    pub heartbeat_interval_ms: u64,
    pub extra: Option<HashMap<String, serde_json::Value>>,
}

impl Default for ProviderConfig {
    fn default() -> Self {
        Self {
            name: String::new(),
            url: String::new(),
            api_key: None,
            api_secret: None,
            client_id: None,
            enabled: true,
            reconnect_delay_ms: 5000,
            max_reconnect_attempts: 10,
            heartbeat_interval_ms: 30000,
            extra: None,
        }
    }
}

// ============================================================================
// METRICS
// ============================================================================

/// Connection metrics
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ConnectionMetrics {
    pub provider: String,
    pub status: ConnectionStatus,
    pub connected_at: Option<u64>,
    pub last_message_at: Option<u64>,
    pub messages_received: u64,
    pub messages_sent: u64,
    pub active_subscriptions: usize,
    pub reconnect_count: u32,
    pub latency_ms: Option<u64>,
}

impl Default for ConnectionMetrics {
    fn default() -> Self {
        Self {
            provider: String::new(),
            status: ConnectionStatus::Disconnected,
            connected_at: None,
            last_message_at: None,
            messages_received: 0,
            messages_sent: 0,
            active_subscriptions: 0,
            reconnect_count: 0,
            latency_ms: None,
        }
    }
}

// ============================================================================
// ERROR TYPES
// ============================================================================

#[derive(Debug, thiserror::Error)]
pub enum WebSocketError {
    #[error("Connection error: {0}")]
    ConnectionError(String),

    #[error("Parse error: {0}")]
    ParseError(String),

    #[error("Provider not found: {0}")]
    ProviderNotFound(String),

    #[error("Subscription error: {0}")]
    SubscriptionError(String),

    #[error("Already connected: {0}")]
    AlreadyConnected(String),

    #[error("Not connected: {0}")]
    NotConnected(String),
}

pub type Result<T> = std::result::Result<T, WebSocketError>;
