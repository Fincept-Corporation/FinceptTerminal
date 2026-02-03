// Fyers WebSocket Adapter
//
// Implements Fyers HSM (High-Speed Market) binary protocol for real-time market data
// Based on official Fyers library analysis and OpenAlgo implementation
//
// Connection URL: wss://socket.fyers.in/hsm/v1-5/prod
// Access token format: "appid:accesstoken"
// Protocol: Binary (not JSON!)

use super::WebSocketAdapter;
use crate::websocket::types::*;
use async_trait::async_trait;
use base64::{engine::general_purpose::URL_SAFE_NO_PAD, Engine};
use futures_util::{SinkExt, StreamExt};
use serde_json::Value;
use std::collections::HashMap;
use std::sync::Arc;
use tokio::net::TcpStream;
use tokio::sync::RwLock;
use tokio_tungstenite::{connect_async, tungstenite::Message, MaybeTlsStream, WebSocketStream};

// Fyers symbol-token API endpoint
const FYERS_SYMBOL_TOKEN_API: &str = "https://api-t1.fyers.in/data/symbol-token";

// ============================================================================
// FYERS WEBSOCKET CONFIGURATION
// ============================================================================

const FYERS_HSM_WS_URL: &str = "wss://socket.fyers.in/hsm/v1-5/prod";
const SOURCE_IDENTIFIER: &str = "FinceptTerminal-HSM";
const MODE_PRODUCTION: &str = "P";

// ============================================================================
// FYERS HSM DATA FIELD MAPPINGS (from official library)
// ============================================================================

/// Data fields for scrip/equity/options (sf| prefix)
const DATA_FIELDS: &[&str] = &[
    "ltp", "vol_traded_today", "last_traded_time", "exch_feed_time",
    "bid_size", "ask_size", "bid_price", "ask_price", "last_traded_qty",
    "tot_buy_qty", "tot_sell_qty", "avg_trade_price", "oi", "low_price",
    "high_price", "yhigh", "ylow", "lower_ckt", "upper_ckt", "open_price",
    "prev_close_price", "type", "symbol"
];

/// Data fields for index (if| prefix)
const INDEX_FIELDS: &[&str] = &[
    "ltp", "prev_close_price", "exch_feed_time", "high_price", "low_price",
    "open_price", "type", "symbol"
];

/// Data fields for depth (dp| prefix)
const DEPTH_FIELDS: &[&str] = &[
    "bid_price1", "bid_price2", "bid_price3", "bid_price4", "bid_price5",
    "ask_price1", "ask_price2", "ask_price3", "ask_price4", "ask_price5",
    "bid_size1", "bid_size2", "bid_size3", "bid_size4", "bid_size5",
    "ask_size1", "ask_size2", "ask_size3", "ask_size4", "ask_size5",
    "bid_order1", "bid_order2", "bid_order3", "bid_order4", "bid_order5",
    "ask_order1", "ask_order2", "ask_order3", "ask_order4", "ask_order5",
    "type", "symbol"
];

/// Exchange segment mapping (fytoken prefix -> segment name)
#[allow(dead_code)]
fn get_exchange_segment(segment_code: &str) -> &'static str {
    match segment_code {
        "1010" => "nse_cm",    // NSE Cash
        "1011" => "nse_fo",    // NSE F&O
        "1120" => "mcx_fo",    // MCX F&O
        "1210" => "bse_cm",    // BSE Cash
        "1211" => "bse_fo",    // BSE F&O
        "1212" => "bcs_fo",    // BSE Currency
        "1012" => "cde_fo",    // CDE F&O
        "1020" => "nse_com",   // NSE Commodity
        _ => "unknown",
    }
}

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
    config: ProviderConfig,
    access_token: String,
    hsm_key: Option<String>,
    ws_stream: Arc<RwLock<Option<WebSocketStream<MaybeTlsStream<TcpStream>>>>>,
    message_callback: Arc<std::sync::RwLock<Option<Box<dyn Fn(MarketMessage) + Send + Sync>>>>,
    subscriptions: Arc<RwLock<HashMap<String, FyersDataType>>>,
    is_connected: Arc<RwLock<bool>>,
    is_authenticated: Arc<RwLock<bool>>,
    // Topic ID -> Topic Name mapping (for update messages)
    topic_mappings: Arc<RwLock<HashMap<u16, String>>>,
    // Topic ID -> Scrip data cache (for incremental updates)
    scrips_cache: Arc<RwLock<HashMap<u16, HashMap<String, i32>>>>,
    // HSM token -> Original symbol mapping
    symbol_mappings: Arc<RwLock<HashMap<String, String>>>,
    // Symbol -> Fytoken mapping (from API)
    fytoken_cache: Arc<RwLock<HashMap<String, String>>>,
}

impl FyersAdapter {
    pub fn new(config: ProviderConfig) -> Self {
        let access_token = config.api_key.clone().unwrap_or_default();
        Self {
            config,
            access_token,
            hsm_key: None,
            ws_stream: Arc::new(RwLock::new(None)),
            message_callback: Arc::new(std::sync::RwLock::new(None)),
            subscriptions: Arc::new(RwLock::new(HashMap::new())),
            is_connected: Arc::new(RwLock::new(false)),
            is_authenticated: Arc::new(RwLock::new(false)),
            topic_mappings: Arc::new(RwLock::new(HashMap::new())),
            scrips_cache: Arc::new(RwLock::new(HashMap::new())),
            symbol_mappings: Arc::new(RwLock::new(HashMap::new())),
            fytoken_cache: Arc::new(RwLock::new(HashMap::new())),
        }
    }

    /// Fetch fytokens from Fyers API for symbol-to-token conversion
    /// This is critical for proper HSM subscription
    async fn fetch_fytokens(&self, symbols: &[String]) -> HashMap<String, String> {
        let mut result = HashMap::new();

        if symbols.is_empty() {
            return result;
        }


        let client = reqwest::Client::new();
        let body = serde_json::json!({
            "symbols": symbols
        });

        match client
            .post(FYERS_SYMBOL_TOKEN_API)
            .header("Authorization", &self.access_token)
            .header("Content-Type", "application/json")
            .json(&body)
            .send()
            .await
        {
            Ok(response) => {
                if let Ok(json) = response.json::<Value>().await {

                    if json.get("s").and_then(|s| s.as_str()) == Some("ok") {
                        if let Some(valid_symbols) = json.get("validSymbol").and_then(|v| v.as_object()) {
                            for (symbol, fytoken) in valid_symbols {
                                if let Some(fytoken_str) = fytoken.as_str() {
                                    result.insert(symbol.clone(), fytoken_str.to_string());
                                }
                            }
                        }
                        if let Some(invalid) = json.get("invalidSymbol").and_then(|v| v.as_array()) {
                            for _sym in invalid {
                            }
                        }
                    } else {
                        let _msg = json.get("message").and_then(|m| m.as_str()).unwrap_or("Unknown error");
                    }
                }
            }
            Err(_e) => {
            }
        }

        result
    }

    /// Convert fytoken to HSM token format
    /// fytoken format: "10100000002885" (14 digits)
    /// - First 4 digits: exchange segment (1010 = NSE CM)
    /// - Last 10 digits: token suffix for HSM
    fn fytoken_to_hsm_token(fytoken: &str, data_type: &FyersDataType, is_index: bool) -> Option<String> {
        if fytoken.len() < 10 {
            return None;
        }

        let ex_sg = &fytoken[..4];
        let segment = match ex_sg {
            "1010" => "nse_cm",    // NSE Cash
            "1011" => "nse_fo",    // NSE F&O
            "1120" => "mcx_fo",    // MCX F&O
            "1210" => "bse_cm",    // BSE Cash
            "1211" => "bse_fo",    // BSE F&O
            "1212" => "bcs_fo",    // BSE Currency
            "1012" => "cde_fo",    // CDE F&O
            "1020" => "nse_com",   // NSE Commodity
            _ => {
                return None;
            }
        };

        // For indices, always use "if" prefix regardless of data type
        if is_index {
            // For indices, use the full token_suffix from position 10
            let token_suffix = &fytoken[10..];
            return Some(format!("if|{}|{}", segment, token_suffix));
        }

        let prefix = match data_type {
            FyersDataType::SymbolUpdate => "sf",
            FyersDataType::DepthUpdate => "dp",
            FyersDataType::IndexUpdate => "if",
        };

        // Extract token suffix (last part after first 10 chars, or from position 10)
        let token_suffix = &fytoken[10..];

        Some(format!("{}|{}|{}", prefix, segment, token_suffix))
    }

    /// Extract HSM key from JWT access token
    fn extract_hsm_key(access_token: &str) -> Option<String> {
        // Remove app_id prefix if present (format: "appid:token")
        let token = if access_token.contains(':') {
            access_token.split(':').nth(1)?
        } else {
            access_token
        };

        // JWT format: header.payload.signature
        let parts: Vec<&str> = token.split('.').collect();
        if parts.len() != 3 {
            return None;
        }

        // Decode payload (second part)
        let payload_b64 = parts[1];

        // Base64 URL-safe decode
        let decoded = URL_SAFE_NO_PAD.decode(payload_b64).ok()?;
        let payload_str = String::from_utf8(decoded).ok()?;

        // Parse JSON
        let payload: Value = serde_json::from_str(&payload_str).ok()?;

        // Extract HSM key
        let hsm_key = payload.get("hsm_key")?.as_str()?.to_string();

        // Check expiration
        if let Some(exp) = payload.get("exp").and_then(|e| e.as_i64()) {
            let now = chrono::Utc::now().timestamp();
            if exp < now {
                return None;
            }
        }

        Some(hsm_key)
    }

    /// Create binary authentication message
    fn create_auth_message(hsm_key: &str) -> Vec<u8> {
        let source = SOURCE_IDENTIFIER;
        let mode = MODE_PRODUCTION;

        // Calculate buffer size: 18 + hsm_key length + source length
        let buffer_size = 18 + hsm_key.len() + source.len();

        let mut buffer = Vec::with_capacity(buffer_size);

        // Data length (buffer_size - 2) as big-endian u16
        buffer.extend_from_slice(&((buffer_size - 2) as u16).to_be_bytes());

        // Request type = 1 (authentication)
        buffer.push(1);

        // Field count = 4
        buffer.push(4);

        // Field-1: AuthToken (HSM key)
        buffer.push(1); // Field ID
        buffer.extend_from_slice(&(hsm_key.len() as u16).to_be_bytes());
        buffer.extend_from_slice(hsm_key.as_bytes());

        // Field-2: Mode
        buffer.push(2); // Field ID
        buffer.extend_from_slice(&(1u16).to_be_bytes());
        buffer.extend_from_slice(mode.as_bytes());

        // Field-3: Unknown flag (always 1)
        buffer.push(3); // Field ID
        buffer.extend_from_slice(&(1u16).to_be_bytes());
        buffer.push(1);

        // Field-4: Source
        buffer.push(4); // Field ID
        buffer.extend_from_slice(&(source.len() as u16).to_be_bytes());
        buffer.extend_from_slice(source.as_bytes());

        buffer
    }

    /// Create binary subscription message
    fn create_subscription_message(hsm_symbols: &[String], channel: u8) -> Vec<u8> {
        // Create scrips data
        let mut scrips_data = Vec::new();

        // Symbol count as big-endian u16
        let count = hsm_symbols.len() as u16;
        scrips_data.push((count >> 8) as u8);
        scrips_data.push((count & 0xFF) as u8);

        // Each symbol: length byte + symbol bytes
        for symbol in hsm_symbols {
            let symbol_bytes = symbol.as_bytes();
            scrips_data.push(symbol_bytes.len() as u8);
            scrips_data.extend_from_slice(symbol_bytes);
        }

        // Build complete message
        let data_len = 6 + scrips_data.len();

        let mut buffer = Vec::with_capacity(data_len + 2);

        // Data length as big-endian u16
        buffer.extend_from_slice(&(data_len as u16).to_be_bytes());

        // Request type = 4 (subscription)
        buffer.push(4);

        // Field count = 2
        buffer.push(2);

        // Field-1: Symbols
        buffer.push(1); // Field ID
        buffer.extend_from_slice(&(scrips_data.len() as u16).to_be_bytes());
        buffer.extend_from_slice(&scrips_data);

        // Field-2: Channel
        buffer.push(2); // Field ID
        buffer.extend_from_slice(&(1u16).to_be_bytes());
        buffer.push(channel);

        buffer
    }

    /// Convert Fyers symbol to HSM token format
    /// Input: "NSE:RELIANCE-EQ" or "NSE:NIFTY50-INDEX"
    /// Output: "sf|nse_cm|{token}" or "if|nse_cm|Nifty 50"
    pub fn symbol_to_hsm_token(symbol: &str, data_type: &FyersDataType) -> String {
        let prefix = match data_type {
            FyersDataType::SymbolUpdate => "sf",
            FyersDataType::DepthUpdate => "dp",
            FyersDataType::IndexUpdate => "if",
        };

        // Parse symbol format "EXCHANGE:SYMBOL-TYPE"
        let parts: Vec<&str> = symbol.split(':').collect();
        if parts.len() != 2 {
            // Return as-is if already in HSM format
            if symbol.contains('|') {
                return symbol.to_string();
            }
            return format!("{}|nse_cm|{}", prefix, symbol);
        }

        let exchange = parts[0].to_lowercase();
        let symbol_part = parts[1];

        // Determine segment
        let segment = if symbol_part.ends_with("-INDEX") {
            match exchange.as_str() {
                "nse" => "nse_cm",
                "bse" => "bse_cm",
                _ => "nse_cm",
            }
        } else if symbol_part.contains("-FUT") || symbol_part.contains("-OPT") {
            match exchange.as_str() {
                "nse" | "nfo" => "nse_fo",
                "bse" | "bfo" => "bse_fo",
                "mcx" => "mcx_fo",
                _ => "nse_fo",
            }
        } else {
            // Cash market
            match exchange.as_str() {
                "nse" => "nse_cm",
                "bse" => "bse_cm",
                _ => "nse_cm",
            }
        };

        // For indices, use "if" prefix
        let final_prefix = if symbol_part.ends_with("-INDEX") { "if" } else { prefix };

        // Extract token from symbol (remove -EQ, -INDEX suffix)
        let token = symbol_part
            .replace("-EQ", "")
            .replace("-INDEX", "")
            .replace("-FUT", "")
            .replace("-OPT", "");

        format!("{}|{}|{}", final_prefix, segment, token)
    }

    /// Parse binary message from HSM WebSocket
    fn parse_binary_message(
        data: &[u8],
        topic_mappings: &mut HashMap<u16, String>,
        scrips_cache: &mut HashMap<u16, HashMap<String, i32>>,
        symbol_mappings: &HashMap<String, String>,
        callback: &Arc<std::sync::RwLock<Option<Box<dyn Fn(MarketMessage) + Send + Sync>>>>,
    ) {
        if data.len() < 3 {
            return;
        }

        let msg_type = data[2];

        match msg_type {
            1 => {
                // Authentication response
            }
            4 => {
                // Subscription acknowledgment
            }
            6 => {
                // Data feed message
                Self::parse_data_feed(data, topic_mappings, scrips_cache, symbol_mappings, callback);
            }
            13 => {
                // Master data (usually large message on connect)
            }
            _ => {
            }
        }
    }

    /// Parse data feed message (message type 6)
    fn parse_data_feed(
        data: &[u8],
        topic_mappings: &mut HashMap<u16, String>,
        scrips_cache: &mut HashMap<u16, HashMap<String, i32>>,
        symbol_mappings: &HashMap<String, String>,
        callback: &Arc<std::sync::RwLock<Option<Box<dyn Fn(MarketMessage) + Send + Sync>>>>,
    ) {
        if data.len() < 9 {
            return;
        }

        // Get scrip count at bytes 7-8
        let scrip_count = u16::from_be_bytes([data[7], data[8]]) as usize;
        let mut offset = 9;

        for _ in 0..scrip_count {
            if offset >= data.len() {
                break;
            }

            // Get data type
            let data_type = data[offset];
            offset += 1;

            match data_type {
                83 => {
                    // Snapshot data (0x53 = 'S')
                    offset = Self::parse_snapshot_data(data, offset, topic_mappings, scrips_cache, symbol_mappings, callback);
                }
                85 => {
                    // Update data (0x55 = 'U')
                    offset = Self::parse_update_data(data, offset, topic_mappings, scrips_cache, symbol_mappings, callback);
                }
                _ => {
                    break;
                }
            }
        }
    }

    /// Parse snapshot data (data_type = 83)
    fn parse_snapshot_data(
        data: &[u8],
        mut offset: usize,
        topic_mappings: &mut HashMap<u16, String>,
        scrips_cache: &mut HashMap<u16, HashMap<String, i32>>,
        symbol_mappings: &HashMap<String, String>,
        callback: &Arc<std::sync::RwLock<Option<Box<dyn Fn(MarketMessage) + Send + Sync>>>>,
    ) -> usize {
        if offset + 3 > data.len() {
            return offset;
        }

        // Get topic ID (2 bytes, little-endian in Fyers)
        let topic_id = u16::from_le_bytes([data[offset], data[offset + 1]]);
        offset += 2;

        // Get topic name length
        let topic_name_len = data[offset] as usize;
        offset += 1;

        if offset + topic_name_len > data.len() {
            return offset;
        }

        // Get topic name (HSM token like "sf|nse_cm|2885")
        let topic_name = String::from_utf8_lossy(&data[offset..offset + topic_name_len]).to_string();
        offset += topic_name_len;

        // Store topic mapping
        topic_mappings.insert(topic_id, topic_name.clone());

        // Parse based on topic type
        if topic_name.starts_with("sf|") {
            offset = Self::parse_scrip_snapshot(data, offset, topic_id, &topic_name, scrips_cache, symbol_mappings, callback);
        } else if topic_name.starts_with("if|") {
            offset = Self::parse_index_snapshot(data, offset, topic_id, &topic_name, symbol_mappings, callback);
        } else if topic_name.starts_with("dp|") {
            offset = Self::parse_depth_snapshot(data, offset, topic_id, &topic_name, scrips_cache, symbol_mappings, callback);
        }

        offset
    }

    /// Parse scrip snapshot data
    fn parse_scrip_snapshot(
        data: &[u8],
        mut offset: usize,
        topic_id: u16,
        topic_name: &str,
        scrips_cache: &mut HashMap<u16, HashMap<String, i32>>,
        symbol_mappings: &HashMap<String, String>,
        callback: &Arc<std::sync::RwLock<Option<Box<dyn Fn(MarketMessage) + Send + Sync>>>>,
    ) -> usize {
        if offset + 1 > data.len() {
            return offset;
        }

        let field_count = data[offset] as usize;
        offset += 1;

        let mut scrip_data: HashMap<String, i32> = HashMap::new();

        // Parse field values (4 bytes each, big-endian)
        for index in 0..field_count {
            if offset + 4 > data.len() {
                break;
            }

            let value = i32::from_be_bytes([data[offset], data[offset + 1], data[offset + 2], data[offset + 3]]);
            offset += 4;

            // -2147483648 means null/no value
            if value != i32::MIN && index < DATA_FIELDS.len() {
                scrip_data.insert(DATA_FIELDS[index].to_string(), value);
            }
        }

        // Skip 2 bytes
        offset += 2;

        if offset + 3 > data.len() {
            return offset;
        }

        // Get multiplier and precision
        let multiplier = u16::from_be_bytes([data[offset], data[offset + 1]]);
        offset += 2;
        let _precision = data[offset];
        offset += 1;

        // Parse string fields: exchange, exchange_token, symbol
        let mut exchange = String::new();
        let mut _exchange_token = String::new();
        let mut symbol = String::new();

        for field_name in &["exchange", "exchange_token", "symbol"] {
            if offset + 1 > data.len() {
                break;
            }

            let string_len = data[offset] as usize;
            offset += 1;

            if offset + string_len > data.len() {
                break;
            }

            let string_value = String::from_utf8_lossy(&data[offset..offset + string_len]).to_string();
            offset += string_len;

            match *field_name {
                "exchange" => exchange = string_value,
                "exchange_token" => _exchange_token = string_value,
                "symbol" => symbol = string_value,
                _ => {}
            }
        }

        // Store in cache
        scrips_cache.insert(topic_id, scrip_data.clone());

        // Get original symbol from mapping or use parsed symbol
        let original_symbol = symbol_mappings.get(topic_name)
            .cloned()
            .unwrap_or_else(|| format!("{}:{}", exchange, symbol));

        // Convert to TickerData and emit
        let divisor = if multiplier > 0 { multiplier as f64 * 100.0 } else { 100.0 };

        let tick = TickerData {
            provider: "fyers".to_string(),
            symbol: original_symbol.clone(),
            price: scrip_data.get("ltp").map(|v| *v as f64 / divisor).unwrap_or(0.0),
            bid: scrip_data.get("bid_price").map(|v| *v as f64 / divisor),
            ask: scrip_data.get("ask_price").map(|v| *v as f64 / divisor),
            bid_size: scrip_data.get("bid_size").map(|v| *v as f64),
            ask_size: scrip_data.get("ask_size").map(|v| *v as f64),
            volume: scrip_data.get("vol_traded_today").map(|v| *v as f64),
            high: scrip_data.get("high_price").map(|v| *v as f64 / divisor),
            low: scrip_data.get("low_price").map(|v| *v as f64 / divisor),
            open: scrip_data.get("open_price").map(|v| *v as f64 / divisor),
            close: scrip_data.get("prev_close_price").map(|v| *v as f64 / divisor),
            change: None, // Calculate from ltp - prev_close
            change_percent: None,
            timestamp: scrip_data.get("exch_feed_time").map(|v| *v as u64).unwrap_or_else(|| chrono::Utc::now().timestamp() as u64),
        };

        // Calculate change
        let tick = {
            let mut t = tick;
            if let (Some(_ltp), Some(close)) = (t.price.into(), t.close) {
                let change = t.price - close;
                let change_pct = if close > 0.0 { (change / close) * 100.0 } else { 0.0 };
                t.change = Some(change);
                t.change_percent = Some(change_pct);
            }
            t
        };

        // Emit to callback
        if let Ok(cb_lock) = callback.read() {
            if let Some(cb) = cb_lock.as_ref() {
                cb(MarketMessage::Ticker(tick));
            }
        }

        offset
    }

    /// Parse index snapshot data
    fn parse_index_snapshot(
        data: &[u8],
        mut offset: usize,
        _topic_id: u16,
        topic_name: &str,
        symbol_mappings: &HashMap<String, String>,
        callback: &Arc<std::sync::RwLock<Option<Box<dyn Fn(MarketMessage) + Send + Sync>>>>,
    ) -> usize {
        if offset + 1 > data.len() {
            return offset;
        }

        let field_count = data[offset] as usize;
        offset += 1;

        let mut index_data: HashMap<String, i32> = HashMap::new();

        // Parse field values
        for index in 0..field_count {
            if offset + 4 > data.len() {
                break;
            }

            let value = i32::from_be_bytes([data[offset], data[offset + 1], data[offset + 2], data[offset + 3]]);
            offset += 4;

            if value != i32::MIN && index < INDEX_FIELDS.len() {
                index_data.insert(INDEX_FIELDS[index].to_string(), value);
            }
        }

        // Get original symbol
        let original_symbol = symbol_mappings.get(topic_name)
            .cloned()
            .unwrap_or_else(|| topic_name.to_string());

        // Index values are already in proper format (no division needed)
        let divisor = 100.0;

        let tick = TickerData {
            provider: "fyers".to_string(),
            symbol: original_symbol,
            price: index_data.get("ltp").map(|v| *v as f64 / divisor).unwrap_or(0.0),
            bid: None,
            ask: None,
            bid_size: None,
            ask_size: None,
            volume: None,
            high: index_data.get("high_price").map(|v| *v as f64 / divisor),
            low: index_data.get("low_price").map(|v| *v as f64 / divisor),
            open: index_data.get("open_price").map(|v| *v as f64 / divisor),
            close: index_data.get("prev_close_price").map(|v| *v as f64 / divisor),
            change: None,
            change_percent: None,
            timestamp: index_data.get("exch_feed_time").map(|v| *v as u64).unwrap_or_else(|| chrono::Utc::now().timestamp() as u64),
        };

        // Calculate change
        let tick = {
            let mut t = tick;
            if let Some(close) = t.close {
                let change = t.price - close;
                let change_pct = if close > 0.0 { (change / close) * 100.0 } else { 0.0 };
                t.change = Some(change);
                t.change_percent = Some(change_pct);
            }
            t
        };

        if let Ok(cb_lock) = callback.read() {
            if let Some(cb) = cb_lock.as_ref() {
                cb(MarketMessage::Ticker(tick));
            }
        }

        offset
    }

    /// Parse depth snapshot data
    fn parse_depth_snapshot(
        data: &[u8],
        mut offset: usize,
        topic_id: u16,
        topic_name: &str,
        scrips_cache: &mut HashMap<u16, HashMap<String, i32>>,
        symbol_mappings: &HashMap<String, String>,
        callback: &Arc<std::sync::RwLock<Option<Box<dyn Fn(MarketMessage) + Send + Sync>>>>,
    ) -> usize {
        if offset + 1 > data.len() {
            return offset;
        }

        let field_count = data[offset] as usize;
        offset += 1;

        let mut depth_data: HashMap<String, i32> = HashMap::new();

        // Parse field values
        for index in 0..field_count {
            if offset + 4 > data.len() {
                break;
            }

            let value = i32::from_be_bytes([data[offset], data[offset + 1], data[offset + 2], data[offset + 3]]);
            offset += 4;

            if value != i32::MIN && index < DEPTH_FIELDS.len() {
                depth_data.insert(DEPTH_FIELDS[index].to_string(), value);
            }
        }

        // Skip 2 bytes
        offset += 2;

        if offset + 3 > data.len() {
            return offset;
        }

        // Get multiplier and precision
        let multiplier = u16::from_be_bytes([data[offset], data[offset + 1]]);
        offset += 2;
        let _precision = data[offset];
        offset += 1;

        // Parse string fields
        let mut exchange = String::new();
        let mut symbol = String::new();

        for field_name in &["exchange", "exchange_token", "symbol"] {
            if offset + 1 > data.len() {
                break;
            }

            let string_len = data[offset] as usize;
            offset += 1;

            if offset + string_len > data.len() {
                break;
            }

            let string_value = String::from_utf8_lossy(&data[offset..offset + string_len]).to_string();
            offset += string_len;

            match *field_name {
                "exchange" => exchange = string_value,
                "symbol" => symbol = string_value,
                _ => {}
            }
        }

        // Store in cache
        scrips_cache.insert(topic_id, depth_data.clone());

        // Get original symbol
        let original_symbol = symbol_mappings.get(topic_name)
            .cloned()
            .unwrap_or_else(|| format!("{}:{}", exchange, symbol));

        let divisor = if multiplier > 0 { multiplier as f64 * 100.0 } else { 100.0 };

        // Build order book levels
        let mut bids = Vec::new();
        let mut asks = Vec::new();

        for i in 1..=5 {
            let bid_price_key = format!("bid_price{}", i);
            let bid_size_key = format!("bid_size{}", i);
            let bid_order_key = format!("bid_order{}", i);
            let ask_price_key = format!("ask_price{}", i);
            let ask_size_key = format!("ask_size{}", i);
            let ask_order_key = format!("ask_order{}", i);

            if let Some(&price) = depth_data.get(&bid_price_key) {
                let size = depth_data.get(&bid_size_key).copied().unwrap_or(0);
                let count = depth_data.get(&bid_order_key).map(|&c| c as u32);
                bids.push(OrderBookLevel {
                    price: price as f64 / divisor,
                    quantity: size as f64,
                    count,
                });
            }

            if let Some(&price) = depth_data.get(&ask_price_key) {
                let size = depth_data.get(&ask_size_key).copied().unwrap_or(0);
                let count = depth_data.get(&ask_order_key).map(|&c| c as u32);
                asks.push(OrderBookLevel {
                    price: price as f64 / divisor,
                    quantity: size as f64,
                    count,
                });
            }
        }

        let orderbook = OrderBookData {
            provider: "fyers".to_string(),
            symbol: original_symbol,
            bids,
            asks,
            timestamp: chrono::Utc::now().timestamp() as u64,
            is_snapshot: true,
        };

        if let Ok(cb_lock) = callback.read() {
            if let Some(cb) = cb_lock.as_ref() {
                cb(MarketMessage::OrderBook(orderbook));
            }
        }

        offset
    }

    /// Parse update data (data_type = 85) - incremental updates
    fn parse_update_data(
        data: &[u8],
        mut offset: usize,
        topic_mappings: &HashMap<u16, String>,
        scrips_cache: &mut HashMap<u16, HashMap<String, i32>>,
        symbol_mappings: &HashMap<String, String>,
        callback: &Arc<std::sync::RwLock<Option<Box<dyn Fn(MarketMessage) + Send + Sync>>>>,
    ) -> usize {
        if offset + 3 > data.len() {
            return offset;
        }

        // Get topic ID
        let topic_id = u16::from_le_bytes([data[offset], data[offset + 1]]);
        offset += 2;

        // Get field count
        let field_count = data[offset] as usize;
        offset += 1;

        // Get topic name from mapping
        let topic_name = match topic_mappings.get(&topic_id) {
            Some(name) => name.clone(),
            None => {
                // Skip unknown topic
                offset += field_count * 4;
                return offset;
            }
        };

        // Determine field mapping based on topic type
        let fields: &[&str] = if topic_name.starts_with("sf|") {
            DATA_FIELDS
        } else if topic_name.starts_with("if|") {
            INDEX_FIELDS
        } else if topic_name.starts_with("dp|") {
            DEPTH_FIELDS
        } else {
            offset += field_count * 4;
            return offset;
        };

        // Get or create cache entry
        let cache_entry = scrips_cache.entry(topic_id).or_insert_with(HashMap::new);

        // Update fields
        let mut updated = false;
        for index in 0..field_count {
            if offset + 4 > data.len() {
                break;
            }

            let value = i32::from_be_bytes([data[offset], data[offset + 1], data[offset + 2], data[offset + 3]]);
            offset += 4;

            if value != i32::MIN && index < fields.len() {
                let old_value = cache_entry.get(fields[index]).copied();
                if old_value != Some(value) {
                    cache_entry.insert(fields[index].to_string(), value);
                    updated = true;
                }
            }
        }

        // Emit update if changed
        if updated {
            let original_symbol = symbol_mappings.get(&topic_name)
                .cloned()
                .unwrap_or_else(|| topic_name.clone());

            if topic_name.starts_with("sf|") || topic_name.starts_with("if|") {
                let divisor = 100.0; // Default divisor

                let tick = TickerData {
                    provider: "fyers".to_string(),
                    symbol: original_symbol,
                    price: cache_entry.get("ltp").map(|v| *v as f64 / divisor).unwrap_or(0.0),
                    bid: cache_entry.get("bid_price").map(|v| *v as f64 / divisor),
                    ask: cache_entry.get("ask_price").map(|v| *v as f64 / divisor),
                    bid_size: cache_entry.get("bid_size").map(|v| *v as f64),
                    ask_size: cache_entry.get("ask_size").map(|v| *v as f64),
                    volume: cache_entry.get("vol_traded_today").map(|v| *v as f64),
                    high: cache_entry.get("high_price").map(|v| *v as f64 / divisor),
                    low: cache_entry.get("low_price").map(|v| *v as f64 / divisor),
                    open: cache_entry.get("open_price").map(|v| *v as f64 / divisor),
                    close: cache_entry.get("prev_close_price").map(|v| *v as f64 / divisor),
                    change: None,
                    change_percent: None,
                    timestamp: cache_entry.get("exch_feed_time").map(|v| *v as u64).unwrap_or_else(|| chrono::Utc::now().timestamp() as u64),
                };

                // Calculate change
                let tick = {
                    let mut t = tick;
                    if let Some(close) = t.close {
                        let change = t.price - close;
                        let change_pct = if close > 0.0 { (change / close) * 100.0 } else { 0.0 };
                        t.change = Some(change);
                        t.change_percent = Some(change_pct);
                    }
                    t
                };

                if let Ok(cb_lock) = callback.read() {
                    if let Some(cb) = cb_lock.as_ref() {
                        cb(MarketMessage::Ticker(tick));
                    }
                }
            } else if topic_name.starts_with("dp|") {
                // Emit depth update
                let divisor = 100.0;
                let mut bids = Vec::new();
                let mut asks = Vec::new();

                for i in 1..=5 {
                    if let Some(&price) = cache_entry.get(&format!("bid_price{}", i)) {
                        let size = cache_entry.get(&format!("bid_size{}", i)).copied().unwrap_or(0);
                        let count = cache_entry.get(&format!("bid_order{}", i)).map(|&c| c as u32);
                        bids.push(OrderBookLevel {
                            price: price as f64 / divisor,
                            quantity: size as f64,
                            count,
                        });
                    }
                    if let Some(&price) = cache_entry.get(&format!("ask_price{}", i)) {
                        let size = cache_entry.get(&format!("ask_size{}", i)).copied().unwrap_or(0);
                        let count = cache_entry.get(&format!("ask_order{}", i)).map(|&c| c as u32);
                        asks.push(OrderBookLevel {
                            price: price as f64 / divisor,
                            quantity: size as f64,
                            count,
                        });
                    }
                }

                let orderbook = OrderBookData {
                    provider: "fyers".to_string(),
                    symbol: original_symbol,
                    bids,
                    asks,
                    timestamp: chrono::Utc::now().timestamp() as u64,
                    is_snapshot: false,
                };

                if let Ok(cb_lock) = callback.read() {
                    if let Some(cb) = cb_lock.as_ref() {
                        cb(MarketMessage::OrderBook(orderbook));
                    }
                }
            }
        }

        offset
    }

    /// Start message processing loop
    async fn start_message_loop(
        ws_stream: Arc<RwLock<Option<WebSocketStream<MaybeTlsStream<TcpStream>>>>>,
        is_connected: Arc<RwLock<bool>>,
        is_authenticated: Arc<RwLock<bool>>,
        topic_mappings: Arc<RwLock<HashMap<u16, String>>>,
        scrips_cache: Arc<RwLock<HashMap<u16, HashMap<String, i32>>>>,
        symbol_mappings: Arc<RwLock<HashMap<String, String>>>,
        callback: Arc<std::sync::RwLock<Option<Box<dyn Fn(MarketMessage) + Send + Sync>>>>,
    ) {
        let mut msg_count: u64 = 0;

        loop {
            let msg = {
                let mut stream_lock = ws_stream.write().await;
                if let Some(stream) = stream_lock.as_mut() {
                    stream.next().await
                } else {
                    break;
                }
            };

            msg_count += 1;

            match msg {
                Some(Ok(Message::Binary(data))) => {
                    // HSM protocol uses binary messages
                    let mut topics = topic_mappings.write().await;
                    let mut cache = scrips_cache.write().await;
                    let symbols = symbol_mappings.read().await;

                    Self::parse_binary_message(
                        &data,
                        &mut topics,
                        &mut cache,
                        &symbols,
                        &callback,
                    );

                    // Check if authenticated after first message
                    if msg_count == 1 && data.len() >= 3 && data[2] == 1 {
                        let mut auth = is_authenticated.write().await;
                        *auth = true;
                    }
                }
                Some(Ok(Message::Text(_text))) => {
                    // Unexpected text message
                }
                Some(Ok(Message::Ping(data))) => {
                    let mut stream_lock = ws_stream.write().await;
                    if let Some(stream) = stream_lock.as_mut() {
                        let _ = stream.send(Message::Pong(data)).await;
                    }
                }
                Some(Ok(Message::Pong(_))) => {
                    // Pong received, connection is alive
                }
                Some(Ok(Message::Close(_frame))) => {
                    break;
                }
                Some(Ok(Message::Frame(_))) => {
                    // Raw frame, ignore
                }
                Some(Err(_e)) => {
                    break;
                }
                None => {
                    break;
                }
            }
        }

        // Mark as disconnected
        let mut connected = is_connected.write().await;
        *connected = false;
        let mut auth = is_authenticated.write().await;
        *auth = false;
    }
}

#[async_trait]
impl WebSocketAdapter for FyersAdapter {
    async fn connect(&mut self) -> anyhow::Result<()> {
        if self.access_token.is_empty() {
            return Err(anyhow::anyhow!("Access token not configured. Format should be 'appid:accesstoken'"));
        }

        // Extract HSM key from JWT
        self.hsm_key = Self::extract_hsm_key(&self.access_token);
        if self.hsm_key.is_none() {
            return Err(anyhow::anyhow!("Failed to extract HSM key from access token"));
        }


        // Connect to WebSocket with Authorization header
        let url = url::Url::parse(FYERS_HSM_WS_URL)?;

        let request = tokio_tungstenite::tungstenite::http::Request::builder()
            .uri(FYERS_HSM_WS_URL)
            .header("Authorization", &self.access_token)
            .header("User-Agent", format!("{}/1.0", SOURCE_IDENTIFIER))
            .header("Host", url.host_str().unwrap_or("socket.fyers.in"))
            .header("Connection", "Upgrade")
            .header("Upgrade", "websocket")
            .header("Sec-WebSocket-Version", "13")
            .header("Sec-WebSocket-Key", tokio_tungstenite::tungstenite::handshake::client::generate_key())
            .body(())?;

        let (ws_stream, _response) = connect_async(request).await
            .map_err(|e| anyhow::anyhow!("WebSocket connection failed: {}", e))?;


        // Store stream
        {
            let mut stream_lock = self.ws_stream.write().await;
            *stream_lock = Some(ws_stream);
        }

        // Mark as connected
        {
            let mut connected = self.is_connected.write().await;
            *connected = true;
        }

        // Send authentication message
        let hsm_key = self.hsm_key.as_ref().unwrap();
        let auth_msg = Self::create_auth_message(hsm_key);

        {
            let mut stream_lock = self.ws_stream.write().await;
            if let Some(stream) = stream_lock.as_mut() {
                stream.send(Message::Binary(auth_msg)).await
                    .map_err(|e| anyhow::anyhow!("Failed to send auth message: {}", e))?;
            }
        }

        // Start message processing loop
        let ws_stream_clone = Arc::clone(&self.ws_stream);
        let is_connected_clone = Arc::clone(&self.is_connected);
        let is_authenticated_clone = Arc::clone(&self.is_authenticated);
        let topic_mappings_clone = Arc::clone(&self.topic_mappings);
        let scrips_cache_clone = Arc::clone(&self.scrips_cache);
        let symbol_mappings_clone = Arc::clone(&self.symbol_mappings);
        let callback_clone = Arc::clone(&self.message_callback);

        tokio::spawn(async move {
            Self::start_message_loop(
                ws_stream_clone,
                is_connected_clone,
                is_authenticated_clone,
                topic_mappings_clone,
                scrips_cache_clone,
                symbol_mappings_clone,
                callback_clone,
            ).await;
        });

        // Wait briefly for authentication response (reduced from 500ms)
        tokio::time::sleep(tokio::time::Duration::from_millis(100)).await;

        Ok(())
    }

    async fn disconnect(&mut self) -> anyhow::Result<()> {
        let mut stream_lock = self.ws_stream.write().await;
        if let Some(mut stream) = stream_lock.take() {
            let _ = stream.close(None).await;
        }

        let mut connected = self.is_connected.write().await;
        *connected = false;

        let mut auth = self.is_authenticated.write().await;
        *auth = false;

        // Clear caches
        let mut subs = self.subscriptions.write().await;
        subs.clear();

        let mut topics = self.topic_mappings.write().await;
        topics.clear();

        let mut cache = self.scrips_cache.write().await;
        cache.clear();

        let mut symbols = self.symbol_mappings.write().await;
        symbols.clear();

        Ok(())
    }

    async fn subscribe(
        &mut self,
        symbol: &str,
        channel: &str,
        _params: Option<Value>,
    ) -> anyhow::Result<()> {

        // Determine data type from channel
        let data_type = match channel {
            "depth" | "market_depth" | "DepthUpdate" => FyersDataType::DepthUpdate,
            "index" | "IndexUpdate" => FyersDataType::IndexUpdate,
            _ => FyersDataType::SymbolUpdate,
        };

        // Check if we already have the fytoken cached
        let fytoken = {
            let cache = self.fytoken_cache.read().await;
            cache.get(symbol).cloned()
        };

        let fytoken_str = if let Some(fytoken) = fytoken {
            fytoken
        } else {
            // Fetch fytoken from API
            let fytokens = self.fetch_fytokens(&[symbol.to_string()]).await;

            if let Some(fytoken) = fytokens.get(symbol) {
                // Cache the fytoken
                {
                    let mut cache = self.fytoken_cache.write().await;
                    cache.insert(symbol.to_string(), fytoken.clone());
                }
                fytoken.clone()
            } else {
                // No fytoken available
                return Err(anyhow::anyhow!("Failed to get fytoken for symbol"));
            }
        };

        let is_index = symbol.contains("-INDEX");

        // Build list of HSM tokens to subscribe
        let mut hsm_tokens = Vec::new();

        // Always subscribe to symbol feed (sf|) for LTP/quote data
        if let Some(sf_token) = Self::fytoken_to_hsm_token(&fytoken_str, &FyersDataType::SymbolUpdate, is_index) {
            hsm_tokens.push(sf_token);
        }

        // If depth requested, also subscribe to depth feed (dp|)
        if data_type == FyersDataType::DepthUpdate && !is_index {
            if let Some(dp_token) = Self::fytoken_to_hsm_token(&fytoken_str, &FyersDataType::DepthUpdate, false) {
                hsm_tokens.push(dp_token);
            }
        }

        if hsm_tokens.is_empty() {
            return Err(anyhow::anyhow!("Failed to convert fytoken"));
        }


        // Store symbol mappings (HSM token -> original symbol)
        {
            let mut symbols = self.symbol_mappings.write().await;
            for token in &hsm_tokens {
                symbols.insert(token.clone(), symbol.to_string());
            }
        }

        // Create and send subscription message for all tokens
        let sub_msg = Self::create_subscription_message(&hsm_tokens, 11);

        let mut stream_lock = self.ws_stream.write().await;
        if let Some(stream) = stream_lock.as_mut() {
            stream.send(Message::Binary(sub_msg)).await
                .map_err(|e| anyhow::anyhow!("Failed to send subscription: {}", e))?;

            drop(stream_lock);

            // Store subscription
            let mut subs = self.subscriptions.write().await;
            subs.insert(symbol.to_string(), data_type);

            Ok(())
        } else {
            Err(anyhow::anyhow!("Not connected to WebSocket"))
        }
    }

    async fn unsubscribe(&mut self, symbol: &str, _channel: &str) -> anyhow::Result<()> {
        // Note: Fyers HSM doesn't support selective unsubscription
        // We just remove from our tracking
        let mut subs = self.subscriptions.write().await;
        subs.remove(symbol);

        let mut symbols = self.symbol_mappings.write().await;
        // Remove all mappings for this symbol
        symbols.retain(|_, v| v != symbol);

        Ok(())
    }

    fn set_message_callback(&mut self, callback: Box<dyn Fn(MarketMessage) + Send + Sync>) {
        if let Ok(mut cb_lock) = self.message_callback.write() {
            *cb_lock = Some(callback);
        }
    }

    fn provider_name(&self) -> &str {
        "fyers"
    }

    fn is_connected(&self) -> bool {
        futures::executor::block_on(async { *self.is_connected.read().await })
    }
}

/// Subscribe to multiple symbols at once (batch subscription) - FAST VERSION
/// Pre-fetches all fytokens in ONE API call before subscribing
impl FyersAdapter {
    pub async fn subscribe_batch(
        &mut self,
        symbols: &[String],
        data_type: FyersDataType,
    ) -> anyhow::Result<()> {
        if symbols.is_empty() {
            return Ok(());
        }


        let _start_time = std::time::Instant::now();

        // Step 1: Batch fetch ALL fytokens in one API call (FAST!)
        let fytokens = self.fetch_fytokens(symbols).await;

        if fytokens.is_empty() {
            return Err(anyhow::anyhow!("Failed to fetch any fytokens from API"));
        }

        // Step 2: Convert fytokens to HSM tokens
        let mut hsm_tokens: Vec<String> = Vec::new();
        let mut symbol_to_hsm: Vec<(String, String)> = Vec::new();

        for symbol in symbols {
            let is_index = symbol.contains("-INDEX");

            if let Some(fytoken) = fytokens.get(symbol) {
                // Cache the fytoken
                {
                    let mut cache = self.fytoken_cache.write().await;
                    cache.insert(symbol.clone(), fytoken.clone());
                }

                // Always add sf| token for LTP
                if let Some(sf_token) = Self::fytoken_to_hsm_token(fytoken, &FyersDataType::SymbolUpdate, is_index) {
                    hsm_tokens.push(sf_token.clone());
                    symbol_to_hsm.push((sf_token, symbol.clone()));
                }

                // Also add dp| token if depth requested and not index
                if data_type == FyersDataType::DepthUpdate && !is_index {
                    if let Some(dp_token) = Self::fytoken_to_hsm_token(fytoken, &FyersDataType::DepthUpdate, false) {
                        hsm_tokens.push(dp_token.clone());
                        symbol_to_hsm.push((dp_token, symbol.clone()));
                    }
                }
            } else {
            }
        }


        // Step 3: Store symbol mappings
        {
            let mut symbol_map = self.symbol_mappings.write().await;
            for (hsm_token, symbol) in &symbol_to_hsm {
                symbol_map.insert(hsm_token.clone(), symbol.clone());
            }
        }

        // Step 4: Subscribe in batches (Fyers supports up to 5000, batch in 100)
        const BATCH_SIZE: usize = 100;

        for chunk in hsm_tokens.chunks(BATCH_SIZE) {
            let sub_msg = Self::create_subscription_message(&chunk.to_vec(), 11);

            let mut stream_lock = self.ws_stream.write().await;
            if let Some(stream) = stream_lock.as_mut() {
                stream.send(Message::Binary(sub_msg)).await
                    .map_err(|e| anyhow::anyhow!("Failed to send batch subscription: {}", e))?;

            } else {
                return Err(anyhow::anyhow!("Not connected to WebSocket"));
            }
            drop(stream_lock);

            // Minimal delay between batches
            if chunk.len() == BATCH_SIZE {
                tokio::time::sleep(tokio::time::Duration::from_millis(50)).await;
            }
        }

        // Store subscriptions
        {
            let mut subs = self.subscriptions.write().await;
            for symbol in symbols {
                subs.insert(symbol.clone(), data_type.clone());
            }
        }

        Ok(())
    }

    pub async fn unsubscribe_batch(&mut self, symbols: &[String]) -> anyhow::Result<()> {
        // Note: HSM doesn't support selective unsubscription
        // Just remove from tracking
        let mut subs = self.subscriptions.write().await;
        let mut symbol_map = self.symbol_mappings.write().await;

        for symbol in symbols {
            subs.remove(symbol);
            symbol_map.retain(|_, v| v != symbol);
        }

        Ok(())
    }

    /// Get current subscription count
    pub async fn subscription_count(&self) -> usize {
        let subs = self.subscriptions.read().await;
        subs.len()
    }

    /// Check if authenticated
    pub async fn is_authenticated(&self) -> bool {
        *self.is_authenticated.read().await
    }
}
