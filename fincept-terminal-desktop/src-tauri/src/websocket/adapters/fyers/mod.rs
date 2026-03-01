#![allow(dead_code)]
// Fyers WebSocket Adapter
//
// Implements Fyers HSM (High-Speed Market) binary protocol for real-time market data
// Based on official Fyers library analysis and OpenAlgo implementation
//
// Connection URL: wss://socket.fyers.in/hsm/v1-5/prod
// Access token format: "appid:accesstoken"
// Protocol: Binary (not JSON!)

use crate::websocket::types::*;
use std::collections::HashMap;
use std::sync::Arc;
use tokio::net::TcpStream;
use tokio::sync::RwLock;
use tokio_tungstenite::{MaybeTlsStream, WebSocketStream};

mod impl_core;
mod impl_parser;
mod impl_adapter;
mod impl_batch;

// Fyers symbol-token API endpoint
pub(super) const FYERS_SYMBOL_TOKEN_API: &str = "https://api-t1.fyers.in/data/symbol-token";

// ============================================================================
// FYERS WEBSOCKET CONFIGURATION
// ============================================================================

pub(super) const FYERS_HSM_WS_URL: &str = "wss://socket.fyers.in/hsm/v1-5/prod";
pub(super) const SOURCE_IDENTIFIER: &str = "FinceptTerminal-HSM";
pub(super) const MODE_PRODUCTION: &str = "P";

// ============================================================================
// FYERS HSM DATA FIELD MAPPINGS (from official library)
// ============================================================================

/// Data fields for scrip/equity/options (sf| prefix)
pub(super) const DATA_FIELDS: &[&str] = &[
    "ltp", "vol_traded_today", "last_traded_time", "exch_feed_time",
    "bid_size", "ask_size", "bid_price", "ask_price", "last_traded_qty",
    "tot_buy_qty", "tot_sell_qty", "avg_trade_price", "oi", "low_price",
    "high_price", "yhigh", "ylow", "lower_ckt", "upper_ckt", "open_price",
    "prev_close_price", "type", "symbol"
];

/// Data fields for index (if| prefix)
pub(super) const INDEX_FIELDS: &[&str] = &[
    "ltp", "prev_close_price", "exch_feed_time", "high_price", "low_price",
    "open_price", "type", "symbol"
];

/// Data fields for depth (dp| prefix)
pub(super) const DEPTH_FIELDS: &[&str] = &[
    "bid_price1", "bid_price2", "bid_price3", "bid_price4", "bid_price5",
    "ask_price1", "ask_price2", "ask_price3", "ask_price4", "ask_price5",
    "bid_size1", "bid_size2", "bid_size3", "bid_size4", "bid_size5",
    "ask_size1", "ask_size2", "ask_size3", "ask_size4", "ask_size5",
    "bid_order1", "bid_order2", "bid_order3", "bid_order4", "bid_order5",
    "ask_order1", "ask_order2", "ask_order3", "ask_order4", "ask_order5",
    "type", "symbol"
];

// ============================================================================
// FYERS DATA TYPES
// ============================================================================

/// Subscription mode
#[derive(Debug, Clone, PartialEq)]
pub enum FyersDataType {
    SymbolUpdate,  // sf| - scrip/equity data
    DepthUpdate,   // dp| - market depth
    IndexUpdate,   // if| - index data
}

/// Parsed scrip data
#[derive(Debug, Clone, Default)]
pub struct FyersScripData {
    pub symbol: String,
    pub exchange: String,
    pub exchange_token: String,
    pub ltp: i32,
    pub volume: i32,
    pub last_traded_time: i32,
    pub exch_feed_time: i32,
    pub bid_size: i32,
    pub ask_size: i32,
    pub bid_price: i32,
    pub ask_price: i32,
    pub last_traded_qty: i32,
    pub tot_buy_qty: i32,
    pub tot_sell_qty: i32,
    pub avg_trade_price: i32,
    pub oi: i32,
    pub low_price: i32,
    pub high_price: i32,
    pub open_price: i32,
    pub prev_close_price: i32,
    pub multiplier: u16,
    pub precision: u8,
    pub hsm_token: String,
}

// ============================================================================
// FYERS ADAPTER
// ============================================================================

pub struct FyersAdapter {
    pub(super) config: ProviderConfig,
    pub(super) access_token: String,
    pub(super) hsm_key: Option<String>,
    pub(super) ws_stream: Arc<RwLock<Option<WebSocketStream<MaybeTlsStream<TcpStream>>>>>,
    pub(super) message_callback: Arc<std::sync::RwLock<Option<Box<dyn Fn(MarketMessage) + Send + Sync>>>>,
    pub(super) subscriptions: Arc<RwLock<HashMap<String, FyersDataType>>>,
    pub(super) is_connected: Arc<RwLock<bool>>,
    pub(super) is_authenticated: Arc<RwLock<bool>>,
    // Topic ID -> Topic Name mapping (for update messages)
    pub(super) topic_mappings: Arc<RwLock<HashMap<u16, String>>>,
    // Topic ID -> Scrip data cache (for incremental updates)
    pub(super) scrips_cache: Arc<RwLock<HashMap<u16, HashMap<String, i32>>>>,
    // HSM token -> Original symbol mapping
    pub(super) symbol_mappings: Arc<RwLock<HashMap<String, String>>>,
    // Symbol -> Fytoken mapping (from API)
    pub(super) fytoken_cache: Arc<RwLock<HashMap<String, String>>>,
}
