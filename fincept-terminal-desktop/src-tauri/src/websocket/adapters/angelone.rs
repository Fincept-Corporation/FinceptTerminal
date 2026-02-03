// Angel One (SmartAPI) WebSocket Adapter v2
//
// Official spec: https://smartapi.angelone.in/docs/WebSocket2
// - WebSocket URL: wss://smartapisocket.angelone.in/smart-stream
// - Auth via headers: Authorization, x-api-key, x-client-code, x-feed-token
// - Binary response format with Little Endian byte order
// - Modes: LTP(1)=51 bytes, Quote(2)=123 bytes, SnapQuote(3)=379 bytes

use super::WebSocketAdapter;
use crate::websocket::types::*;
use async_trait::async_trait;
use futures_util::{SinkExt, StreamExt};
use serde::Serialize;
use serde_json::Value;
use std::collections::HashMap;
use std::sync::Arc;
use tokio::net::TcpStream;
use tokio::sync::RwLock;
use tokio_tungstenite::{
    connect_async_with_config,
    tungstenite::{
        http::{Request, Uri},
        Message,
    },
    MaybeTlsStream, WebSocketStream,
};

// ============================================================================
// ANGEL ONE CONFIGURATION (per official spec)
// ============================================================================

const ANGEL_WS_URL: &str = "wss://smartapisocket.angelone.in/smart-stream";
const HEARTBEAT_INTERVAL: u64 = 30; // per official spec

// Subscription modes (official spec)
const MODE_LTP: u8 = 1;      // packet size = 51 bytes
const MODE_QUOTE: u8 = 2;    // packet size = 123 bytes
const MODE_SNAP_QUOTE: u8 = 3; // packet size = 379 bytes (includes depth)

// Exchange types (official spec)
const NSE_CM: u8 = 1;
const NSE_FO: u8 = 2;
const BSE_CM: u8 = 3;
const BSE_FO: u8 = 4;
const MCX_FO: u8 = 5;
const NCX_FO: u8 = 7;
const CDE_FO: u8 = 13;

// Actions
const ACTION_SUBSCRIBE: u8 = 1;
const ACTION_UNSUBSCRIBE: u8 = 0;

// ============================================================================
// STRUCTURES
// ============================================================================

#[derive(Debug, Clone, PartialEq)]
pub enum AngelMode {
    Ltp,
    Quote,
    SnapQuote,
}

impl AngelMode {
    fn to_code(&self) -> u8 {
        match self {
            AngelMode::Ltp => MODE_LTP,
            AngelMode::Quote => MODE_QUOTE,
            AngelMode::SnapQuote => MODE_SNAP_QUOTE,
        }
    }
}

#[derive(Debug, Serialize)]
struct SubscriptionRequest {
    #[serde(rename = "correlationID")]
    correlation_id: String,
    action: u8,
    params: SubscriptionParams,
}

#[derive(Debug, Serialize)]
struct SubscriptionParams {
    mode: u8,
    #[serde(rename = "tokenList")]
    token_list: Vec<TokenListItem>,
}

#[derive(Debug, Serialize, Clone)]
struct TokenListItem {
    #[serde(rename = "exchangeType")]
    exchange_type: u8,
    tokens: Vec<String>,
}

#[derive(Debug, Clone)]
pub struct AngelTick {
    pub subscription_mode: u8,
    pub exchange_type: u8,
    pub token: String,
    pub sequence_number: i64,
    pub exchange_timestamp: i64,
    pub ltp: f64,
    // Quote mode fields
    pub ltq: i64,
    pub atp: f64,
    pub volume: i64,
    pub total_buy_qty: f64,
    pub total_sell_qty: f64,
    pub open: f64,
    pub high: f64,
    pub low: f64,
    pub close: f64,
    // Snap Quote fields
    pub last_traded_timestamp: i64,
    pub oi: i64,
    pub oi_change_pct: f64,
    // Best five data (in Snap Quote mode)
    pub best_five: Option<Vec<BestFiveEntry>>,
    // Circuit limits & 52-week (Snap Quote)
    pub upper_circuit: f64,
    pub lower_circuit: f64,
    pub high_52w: f64,
    pub low_52w: f64,
}

#[derive(Debug, Clone)]
pub struct BestFiveEntry {
    pub is_buy: bool,
    pub quantity: i64,
    pub price: f64,
    pub orders: i16,
}

// ============================================================================
// HELPER: read little-endian integers from byte slice
// ============================================================================

fn read_i64_le(data: &[u8], offset: usize) -> i64 {
    i64::from_le_bytes([
        data[offset], data[offset+1], data[offset+2], data[offset+3],
        data[offset+4], data[offset+5], data[offset+6], data[offset+7],
    ])
}

fn read_i32_le(data: &[u8], offset: usize) -> i32 {
    i32::from_le_bytes([
        data[offset], data[offset+1], data[offset+2], data[offset+3],
    ])
}

fn read_i16_le(data: &[u8], offset: usize) -> i16 {
    i16::from_le_bytes([data[offset], data[offset+1]])
}

fn read_f64_le(data: &[u8], offset: usize) -> f64 {
    f64::from_le_bytes([
        data[offset], data[offset+1], data[offset+2], data[offset+3],
        data[offset+4], data[offset+5], data[offset+6], data[offset+7],
    ])
}

/// Convert raw price to actual price (paise to rupees)
fn price_from_paise(raw: i64) -> f64 {
    raw as f64 / 100.0
}

// ============================================================================
// ANGEL ONE ADAPTER
// ============================================================================

pub struct AngelOneAdapter {
    config: ProviderConfig,
    auth_token: String,
    api_key: String,
    client_code: String,
    feed_token: String,
    ws_stream: Arc<RwLock<Option<WebSocketStream<MaybeTlsStream<TcpStream>>>>>,
    message_callback: Arc<RwLock<Option<Box<dyn Fn(MarketMessage) + Send + Sync>>>>,
    subscriptions: Arc<RwLock<HashMap<String, AngelMode>>>,
    is_connected: Arc<RwLock<bool>>,
    token_symbol_map: Arc<RwLock<HashMap<String, String>>>,
}

impl AngelOneAdapter {
    pub fn new(config: ProviderConfig) -> Self {
        let auth_token = config.api_key.clone().unwrap_or_default();
        let api_key = config.api_secret.clone().unwrap_or_default();
        let client_code = config.client_id.clone().unwrap_or_default();
        let feed_token = config.extra.as_ref()
            .and_then(|e| e.get("feed_token"))
            .and_then(|v| v.as_str())
            .unwrap_or("")
            .to_string();

        Self {
            config,
            auth_token,
            api_key,
            client_code,
            feed_token,
            ws_stream: Arc::new(RwLock::new(None)),
            message_callback: Arc::new(RwLock::new(None)),
            subscriptions: Arc::new(RwLock::new(HashMap::new())),
            is_connected: Arc::new(RwLock::new(false)),
            token_symbol_map: Arc::new(RwLock::new(HashMap::new())),
        }
    }

    /// Parse binary market data per official AngelOne WebSocket v2 spec.
    ///
    /// Byte layout:
    /// - [0]      subscription_mode (1 byte)
    /// - [1]      exchange_type (1 byte)
    /// - [2..27]  token (25 bytes, null-terminated UTF-8)
    /// - [27..35] sequence_number (int64)
    /// - [35..43] exchange_timestamp (int64, epoch ms)
    /// - [43..51] LTP (int64, paise) -- NOTE: spec says int32 but Angel API actually sends int64
    /// -- LTP mode ends here (51 bytes) --
    /// - [51..59] last_traded_quantity (int64)
    /// - [59..67] average_traded_price (int64, paise)
    /// - [67..75] volume_traded_today (int64)
    /// - [75..83] total_buy_qty (double/f64)
    /// - [83..91] total_sell_qty (double/f64)
    /// - [91..99] open_price (int64, paise)
    /// - [99..107] high_price (int64, paise)
    /// - [107..115] low_price (int64, paise)
    /// - [115..123] close_price (int64, paise)
    /// -- Quote mode ends here (123 bytes) --
    /// - [123..131] last_traded_timestamp (int64)
    /// - [131..139] open_interest (int64)
    /// - [139..147] oi_change_pct (double, dummy/garbage)
    /// - [147..347] best_five_data (10 packets x 20 bytes each)
    ///              Each: flag(2) + qty(8) + price(8) + orders(2)
    /// - [347..355] upper_circuit (int64, paise)
    /// - [355..363] lower_circuit (int64, paise)
    /// - [363..371] 52w_high (int64, paise)
    /// - [371..379] 52w_low (int64, paise)
    /// -- Snap Quote mode ends here (379 bytes) --
    fn parse_binary_data(data: &[u8]) -> Option<AngelTick> {
        // Minimum LTP packet = 51 bytes
        if data.len() < 51 {
            return None;
        }

        let subscription_mode = data[0];
        let exchange_type = data[1];

        // Token: 25 bytes null-terminated
        let token = String::from_utf8_lossy(&data[2..27])
            .trim_end_matches('\0')
            .to_string();

        let sequence_number = read_i64_le(data, 27);
        let exchange_timestamp = read_i64_le(data, 35);

        // LTP: int64 at offset 43 (paise)
        let ltp_raw = read_i64_le(data, 43);
        let ltp = price_from_paise(ltp_raw);

        let mut tick = AngelTick {
            subscription_mode,
            exchange_type,
            token,
            sequence_number,
            exchange_timestamp,
            ltp,
            ltq: 0, atp: 0.0, volume: 0,
            total_buy_qty: 0.0, total_sell_qty: 0.0,
            open: 0.0, high: 0.0, low: 0.0, close: 0.0,
            last_traded_timestamp: 0, oi: 0, oi_change_pct: 0.0,
            best_five: None,
            upper_circuit: 0.0, lower_circuit: 0.0,
            high_52w: 0.0, low_52w: 0.0,
        };

        // Quote mode: need at least 123 bytes
        if subscription_mode >= MODE_QUOTE && data.len() >= 123 {
            tick.ltq = read_i64_le(data, 51);
            tick.atp = price_from_paise(read_i64_le(data, 59));
            tick.volume = read_i64_le(data, 67);
            tick.total_buy_qty = read_f64_le(data, 75);
            tick.total_sell_qty = read_f64_le(data, 83);
            tick.open = price_from_paise(read_i64_le(data, 91));
            tick.high = price_from_paise(read_i64_le(data, 99));
            tick.low = price_from_paise(read_i64_le(data, 107));
            tick.close = price_from_paise(read_i64_le(data, 115));
        }

        // Snap Quote mode: need at least 379 bytes
        if subscription_mode >= MODE_SNAP_QUOTE && data.len() >= 379 {
            tick.last_traded_timestamp = read_i64_le(data, 123);
            tick.oi = read_i64_le(data, 131);
            tick.oi_change_pct = read_f64_le(data, 139); // dummy field per spec

            // Best Five Data: 10 packets of 20 bytes each starting at offset 147
            // Each: flag(i16,2) + qty(i64,8) + price(i64,8) + orders(i16,2) = 20 bytes
            // First 5 are buy, next 5 are sell (indicated by flag)
            let mut best_five = Vec::with_capacity(10);
            let mut off = 147;
            for _ in 0..10 {
                if off + 20 > data.len() { break; }
                let flag = read_i16_le(data, off);
                let qty = read_i64_le(data, off + 2);
                let price = price_from_paise(read_i64_le(data, off + 10));
                let orders = read_i16_le(data, off + 18);
                best_five.push(BestFiveEntry {
                    is_buy: flag == 1,
                    quantity: qty,
                    price,
                    orders,
                });
                off += 20;
            }
            tick.best_five = Some(best_five);

            // Circuit limits and 52-week
            tick.upper_circuit = price_from_paise(read_i64_le(data, 347));
            tick.lower_circuit = price_from_paise(read_i64_le(data, 355));
            tick.high_52w = price_from_paise(read_i64_le(data, 363));
            tick.low_52w = price_from_paise(read_i64_le(data, 371));
        }

        Some(tick)
    }

    /// Convert AngelTick to MarketMessage for the Tauri event callback
    fn tick_to_market_message(tick: &AngelTick, symbol_map: &HashMap<String, String>) -> Vec<MarketMessage> {
        let exchange_str = Self::exchange_type_to_string(tick.exchange_type);
        let symbol_key = format!("{}:{}", exchange_str, tick.token);
        let symbol = symbol_map.get(&tick.token)
            .or_else(|| symbol_map.get(&symbol_key))
            .cloned()
            .unwrap_or(symbol_key);

        let change = if tick.close > 0.0 { tick.ltp - tick.close } else { 0.0 };
        let change_pct = if tick.close > 0.0 { (change / tick.close) * 100.0 } else { 0.0 };
        let now_ms = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap_or_default()
            .as_millis() as u64;

        let mut messages = Vec::new();

        // Extract best bid/ask from depth data if available
        let (best_bid, best_bid_qty, best_ask, best_ask_qty) = if let Some(ref entries) = tick.best_five {
            let best_bid_entry = entries.iter().filter(|e| e.is_buy).max_by(|a, b|
                a.price.partial_cmp(&b.price).unwrap_or(std::cmp::Ordering::Equal)
            );
            let best_ask_entry = entries.iter().filter(|e| !e.is_buy).min_by(|a, b|
                a.price.partial_cmp(&b.price).unwrap_or(std::cmp::Ordering::Equal)
            );
            (
                best_bid_entry.map(|e| e.price),
                best_bid_entry.map(|e| e.quantity as f64),
                best_ask_entry.map(|e| e.price),
                best_ask_entry.map(|e| e.quantity as f64),
            )
        } else {
            (None, None, None, None)
        };

        // Always emit a Ticker message
        messages.push(MarketMessage::Ticker(TickerData {
            provider: "angelone".to_string(),
            symbol: symbol.clone(),
            price: tick.ltp,
            bid: best_bid,
            ask: best_ask,
            bid_size: best_bid_qty.or(if tick.total_buy_qty > 0.0 { Some(tick.total_buy_qty) } else { None }),
            ask_size: best_ask_qty.or(if tick.total_sell_qty > 0.0 { Some(tick.total_sell_qty) } else { None }),
            volume: if tick.volume > 0 { Some(tick.volume as f64) } else { None },
            high: if tick.high > 0.0 { Some(tick.high) } else { None },
            low: if tick.low > 0.0 { Some(tick.low) } else { None },
            open: if tick.open > 0.0 { Some(tick.open) } else { None },
            close: if tick.close > 0.0 { Some(tick.close) } else { None },
            change: if change != 0.0 { Some(change) } else { None },
            change_percent: if change_pct != 0.0 { Some(change_pct) } else { None },
            timestamp: now_ms,
        }));

        // If best five data exists, also emit OrderBook
        if let Some(ref entries) = tick.best_five {
            let bids: Vec<OrderBookLevel> = entries.iter()
                .filter(|e| e.is_buy)
                .map(|e| OrderBookLevel {
                    price: e.price,
                    quantity: e.quantity as f64,
                    count: Some(e.orders as u32),
                })
                .collect();
            let asks: Vec<OrderBookLevel> = entries.iter()
                .filter(|e| !e.is_buy)
                .map(|e| OrderBookLevel {
                    price: e.price,
                    quantity: e.quantity as f64,
                    count: Some(e.orders as u32),
                })
                .collect();

            if !bids.is_empty() || !asks.is_empty() {
                messages.push(MarketMessage::OrderBook(OrderBookData {
                    provider: "angelone".to_string(),
                    symbol,
                    bids,
                    asks,
                    timestamp: now_ms,
                    is_snapshot: true,
                }));
            }
        }

        messages
    }

    fn exchange_type_to_string(exchange_type: u8) -> &'static str {
        match exchange_type {
            1 => "NSE",
            2 => "NFO",
            3 => "BSE",
            4 => "BFO",
            5 => "MCX",
            7 => "NCX",
            13 => "CDE",
            _ => "UNKNOWN",
        }
    }

    fn parse_exchange_type(exchange: &str) -> u8 {
        match exchange.to_uppercase().as_str() {
            "NSE" | "NSE_CM" => NSE_CM,
            "NFO" | "NSE_FO" => NSE_FO,
            "BSE" | "BSE_CM" => BSE_CM,
            "BFO" | "BSE_FO" => BSE_FO,
            "MCX" | "MCX_FO" => MCX_FO,
            "NCX" | "NCX_FO" => NCX_FO,
            "CDE" | "CDE_FO" => CDE_FO,
            _ => NSE_CM,
        }
    }
}

#[async_trait]
impl WebSocketAdapter for AngelOneAdapter {
    async fn connect(&mut self) -> anyhow::Result<()> {
        let uri: Uri = ANGEL_WS_URL.parse()?;
        // NOTE: Angel One WebSocket does NOT use "Bearer" prefix for Authorization header
        // It expects just the raw JWT token (authToken from login response)
        let request = Request::builder()
            .uri(&uri)
            .header("Host", uri.host().unwrap_or("smartapisocket.angelone.in"))
            .header("Authorization", &self.auth_token)  // NO "Bearer" prefix!
            .header("x-api-key", &self.api_key)
            .header("x-client-code", &self.client_code)
            .header("x-feed-token", &self.feed_token)
            .header("Connection", "Upgrade")
            .header("Upgrade", "websocket")
            .header("Sec-WebSocket-Version", "13")
            .header("Sec-WebSocket-Key", tokio_tungstenite::tungstenite::handshake::client::generate_key())
            .body(())?;

        let (ws_stream, _response) = connect_async_with_config(request, None, false).await?;

        *self.ws_stream.write().await = Some(ws_stream);
        *self.is_connected.write().await = true;

        // Clone Arcs for spawned message handler task
        let ws_stream_clone = self.ws_stream.clone();
        let is_connected_clone = self.is_connected.clone();
        let callback_clone = self.message_callback.clone();
        let token_map_clone = self.token_symbol_map.clone();

        tokio::spawn(async move {
            let mut _msg_count: u64 = 0;
            loop {
                // Use a short timeout to avoid holding the lock indefinitely
                // This allows subscribe() to acquire the lock between reads
                let message = {
                    let mut stream = ws_stream_clone.write().await;
                    if let Some(ref mut ws) = *stream {
                        // Use tokio::time::timeout to avoid blocking forever
                        match tokio::time::timeout(
                            tokio::time::Duration::from_millis(100),
                            ws.next()
                        ).await {
                            Ok(Some(msg)) => Some(msg),
                            Ok(None) => {
                                *is_connected_clone.write().await = false;
                                break;
                            }
                            Err(_) => None, // Timeout - no message, continue loop
                        }
                    } else {
                        break;
                    }
                }; // Lock is released here

                // Process message outside the lock
                if let Some(msg_result) = message {
                    match msg_result {
                        Ok(Message::Binary(data)) => {
                            _msg_count += 1;

                            if let Some(tick) = AngelOneAdapter::parse_binary_data(&data) {
                                let map = token_map_clone.read().await;
                                let messages = AngelOneAdapter::tick_to_market_message(&tick, &map);
                                drop(map);

                                let cb = callback_clone.read().await;
                                if let Some(ref callback) = *cb {
                                    for msg in messages {
                                        callback(msg);
                                    }
                                }
                            }
                        }
                        Ok(Message::Text(text)) => {
                            if text == "pong" {
                                // heartbeat response
                            } else {
                            }
                        }
                        Ok(Message::Ping(data)) => {
                            // Send pong - need to reacquire lock
                            if let Some(ref mut ws) = *ws_stream_clone.write().await {
                                if let Err(_e) = ws.send(Message::Pong(data)).await {
                                }
                            }
                        }
                        Ok(Message::Pong(_)) => {
                            // pong response to our ping
                        }
                        Err(_e) => {
                            *is_connected_clone.write().await = false;
                            break;
                        }
                        _ => {}
                    }
                }
            }
        });

        // Heartbeat: send "ping" every 30 seconds per spec
        let ws_stream_hb = self.ws_stream.clone();
        let is_connected_hb = self.is_connected.clone();
        tokio::spawn(async move {
            loop {
                tokio::time::sleep(tokio::time::Duration::from_secs(HEARTBEAT_INTERVAL)).await;
                if !*is_connected_hb.read().await {
                    break;
                }
                if let Some(ref mut ws) = *ws_stream_hb.write().await {
                    if let Err(_e) = ws.send(Message::Text("ping".to_string())).await {
                        break;
                    }
                }
            }
        });

        Ok(())
    }

    async fn disconnect(&mut self) -> anyhow::Result<()> {
        if let Some(ref mut ws) = *self.ws_stream.write().await {
            ws.close(None).await?;
        }
        *self.ws_stream.write().await = None;
        *self.is_connected.write().await = false;
        Ok(())
    }

    async fn subscribe(
        &mut self,
        symbol: &str,
        channel: &str,
        _params: Option<Value>,
    ) -> anyhow::Result<()> {

        let mode = match channel {
            "depth" | "snap" | "snap_quote" | "full" => AngelMode::SnapQuote, // depth is part of SnapQuote in v2
            "quote" => AngelMode::Quote,
            _ => AngelMode::Ltp,
        };

        // Parse symbol: "NSE:12345" or "NFO:56789"
        let parts: Vec<&str> = symbol.split(':').collect();
        if parts.len() != 2 {
            return Err(anyhow::anyhow!("Invalid symbol format '{}'. Expected 'EXCHANGE:TOKEN' (e.g. 'NSE:2885')", symbol));
        }

        let exchange = parts[0];
        let token = parts[1].to_string();
        let exchange_type = Self::parse_exchange_type(exchange);

        // Store token → symbol mapping for resolving in tick messages
        self.token_symbol_map.write().await.insert(token.clone(), symbol.to_string());

        let request = SubscriptionRequest {
            correlation_id: uuid::Uuid::new_v4().to_string()[..10].to_string(),
            action: ACTION_SUBSCRIBE,
            params: SubscriptionParams {
                mode: mode.to_code(),
                token_list: vec![TokenListItem {
                    exchange_type,
                    tokens: vec![token],
                }],
            },
        };

        let request_json = serde_json::to_string(&request)?;

        if let Some(ref mut ws) = *self.ws_stream.write().await {
            ws.send(Message::Text(request_json)).await?;
            self.subscriptions.write().await.insert(symbol.to_string(), mode);
        } else {
            return Err(anyhow::anyhow!("WebSocket not connected"));
        }

        Ok(())
    }

    async fn unsubscribe(&mut self, symbol: &str, _channel: &str) -> anyhow::Result<()> {
        let mode = self.subscriptions.read().await.get(symbol).cloned();

        if let Some(m) = mode {
            let parts: Vec<&str> = symbol.split(':').collect();
            if parts.len() != 2 {
                return Err(anyhow::anyhow!("Invalid symbol format"));
            }

            let exchange_type = Self::parse_exchange_type(parts[0]);
            let token = parts[1].to_string();

            let request = SubscriptionRequest {
                correlation_id: uuid::Uuid::new_v4().to_string()[..10].to_string(),
                action: ACTION_UNSUBSCRIBE,
                params: SubscriptionParams {
                    mode: m.to_code(),
                    token_list: vec![TokenListItem {
                        exchange_type,
                        tokens: vec![token],
                    }],
                },
            };

            if let Some(ref mut ws) = *self.ws_stream.write().await {
                ws.send(Message::Text(serde_json::to_string(&request)?)).await?;
                self.subscriptions.write().await.remove(symbol);
            }
        }

        Ok(())
    }

    fn set_message_callback(&mut self, callback: Box<dyn Fn(MarketMessage) + Send + Sync>) {
        // Use try_write — callback is set before connect(), so no contention
        if let Ok(mut guard) = self.message_callback.try_write() {
            *guard = Some(callback);
        } else {
            let cb_arc = self.message_callback.clone();
            tokio::spawn(async move {
                *cb_arc.write().await = Some(callback);
            });
        }
    }

    fn provider_name(&self) -> &str {
        "angelone"
    }

    fn is_connected(&self) -> bool {
        self.is_connected.try_read().map(|g| *g).unwrap_or(false)
    }
}
