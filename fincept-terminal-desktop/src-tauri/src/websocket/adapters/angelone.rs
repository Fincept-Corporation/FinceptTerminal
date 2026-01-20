// Angel One (SmartAPI) WebSocket Adapter
//
// Angel One uses SmartWebSocketV2 protocol:
// - WebSocket URL: wss://smartapisocket.angelone.in/smart-stream
// - Authentication via headers: Authorization, x-api-key, x-client-code, x-feed-token
// - Binary message format with subscription modes: LTP(1), Quote(2), SnapQuote(3), Depth(4)

use super::WebSocketAdapter;
use crate::websocket::types::*;
use async_trait::async_trait;
use futures_util::{SinkExt, StreamExt};
use serde::{Deserialize, Serialize};
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
// ANGEL ONE CONFIGURATION
// ============================================================================

const ANGEL_WS_URL: &str = "wss://smartapisocket.angelone.in/smart-stream";
const HEARTBEAT_INTERVAL: u64 = 10; // seconds

// Subscription modes
const MODE_LTP: u8 = 1;
const MODE_QUOTE: u8 = 2;
const MODE_SNAP_QUOTE: u8 = 3;
const MODE_DEPTH: u8 = 4;

// Exchange types
const NSE_CM: u8 = 1;
const NSE_FO: u8 = 2;
const BSE_CM: u8 = 3;
const BSE_FO: u8 = 4;
const MCX_FO: u8 = 5;

// Actions
const ACTION_SUBSCRIBE: u8 = 1;
const ACTION_UNSUBSCRIBE: u8 = 0;

// ============================================================================
// ANGEL ONE STRUCTURES
// ============================================================================

#[derive(Debug, Clone, PartialEq)]
pub enum AngelMode {
    Ltp,
    Quote,
    SnapQuote,
    Depth,
}

impl AngelMode {
    fn to_code(&self) -> u8 {
        match self {
            AngelMode::Ltp => MODE_LTP,
            AngelMode::Quote => MODE_QUOTE,
            AngelMode::SnapQuote => MODE_SNAP_QUOTE,
            AngelMode::Depth => MODE_DEPTH,
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
    pub ltq: i64,
    pub atp: f64,
    pub volume: i64,
    pub total_buy_qty: f64,
    pub total_sell_qty: f64,
    pub open: f64,
    pub high: f64,
    pub low: f64,
    pub close: f64,
    pub last_traded_timestamp: i64,
    pub oi: i64,
    pub oi_day_high: i64,
    pub oi_day_low: i64,
    pub depth: Option<AngelDepth>,
}

#[derive(Debug, Clone)]
pub struct AngelDepth {
    pub buy: Vec<AngelDepthLevel>,
    pub sell: Vec<AngelDepthLevel>,
}

#[derive(Debug, Clone)]
pub struct AngelDepthLevel {
    pub price: f64,
    pub quantity: i64,
    pub orders: i16,
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
    message_callback: Option<Box<dyn Fn(MarketMessage) + Send + Sync>>,
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
            message_callback: None,
            subscriptions: Arc::new(RwLock::new(HashMap::new())),
            is_connected: Arc::new(RwLock::new(false)),
            token_symbol_map: Arc::new(RwLock::new(HashMap::new())),
        }
    }

    /// Parse binary market data from Angel One
    fn parse_binary_data(&self, data: &[u8]) -> Option<AngelTick> {
        if data.len() < 27 {
            return None;
        }

        // Parse header
        let subscription_mode = data[0];
        let exchange_type = data[1];

        // Parse token (25 bytes, null-terminated string)
        let token_bytes = &data[2..27];
        let token = String::from_utf8_lossy(token_bytes)
            .trim_end_matches('\0')
            .to_string();

        let mut offset = 27;

        // Parse sequence number (8 bytes)
        if offset + 8 > data.len() {
            return None;
        }
        let sequence_number = i64::from_le_bytes([
            data[offset], data[offset+1], data[offset+2], data[offset+3],
            data[offset+4], data[offset+5], data[offset+6], data[offset+7],
        ]);
        offset += 8;

        // Parse exchange timestamp (8 bytes)
        if offset + 8 > data.len() {
            return None;
        }
        let exchange_timestamp = i64::from_le_bytes([
            data[offset], data[offset+1], data[offset+2], data[offset+3],
            data[offset+4], data[offset+5], data[offset+6], data[offset+7],
        ]);
        offset += 8;

        // Parse LTP (8 bytes, divide by 100)
        if offset + 8 > data.len() {
            return None;
        }
        let ltp_raw = i64::from_le_bytes([
            data[offset], data[offset+1], data[offset+2], data[offset+3],
            data[offset+4], data[offset+5], data[offset+6], data[offset+7],
        ]);
        let ltp = ltp_raw as f64 / 100.0;
        offset += 8;

        let mut tick = AngelTick {
            subscription_mode,
            exchange_type,
            token,
            sequence_number,
            exchange_timestamp,
            ltp,
            ltq: 0,
            atp: 0.0,
            volume: 0,
            total_buy_qty: 0.0,
            total_sell_qty: 0.0,
            open: 0.0,
            high: 0.0,
            low: 0.0,
            close: 0.0,
            last_traded_timestamp: 0,
            oi: 0,
            oi_day_high: 0,
            oi_day_low: 0,
            depth: None,
        };

        // Parse additional fields for Quote mode
        if subscription_mode >= MODE_QUOTE && offset + 56 <= data.len() {
            tick.ltq = i64::from_le_bytes([
                data[offset], data[offset+1], data[offset+2], data[offset+3],
                data[offset+4], data[offset+5], data[offset+6], data[offset+7],
            ]);
            offset += 8;

            let atp_raw = i64::from_le_bytes([
                data[offset], data[offset+1], data[offset+2], data[offset+3],
                data[offset+4], data[offset+5], data[offset+6], data[offset+7],
            ]);
            tick.atp = atp_raw as f64 / 100.0;
            offset += 8;

            tick.volume = i64::from_le_bytes([
                data[offset], data[offset+1], data[offset+2], data[offset+3],
                data[offset+4], data[offset+5], data[offset+6], data[offset+7],
            ]);
            offset += 8;

            let buy_raw = i64::from_le_bytes([
                data[offset], data[offset+1], data[offset+2], data[offset+3],
                data[offset+4], data[offset+5], data[offset+6], data[offset+7],
            ]) as f64;
            tick.total_buy_qty = buy_raw;
            offset += 8;

            let sell_raw = i64::from_le_bytes([
                data[offset], data[offset+1], data[offset+2], data[offset+3],
                data[offset+4], data[offset+5], data[offset+6], data[offset+7],
            ]) as f64;
            tick.total_sell_qty = sell_raw;
            offset += 8;

            // OHLC
            let open_raw = i64::from_le_bytes([
                data[offset], data[offset+1], data[offset+2], data[offset+3],
                data[offset+4], data[offset+5], data[offset+6], data[offset+7],
            ]);
            tick.open = open_raw as f64 / 100.0;
            offset += 8;

            let high_raw = i64::from_le_bytes([
                data[offset], data[offset+1], data[offset+2], data[offset+3],
                data[offset+4], data[offset+5], data[offset+6], data[offset+7],
            ]);
            tick.high = high_raw as f64 / 100.0;
            offset += 8;

            let low_raw = i64::from_le_bytes([
                data[offset], data[offset+1], data[offset+2], data[offset+3],
                data[offset+4], data[offset+5], data[offset+6], data[offset+7],
            ]);
            tick.low = low_raw as f64 / 100.0;
            offset += 8;

            let close_raw = i64::from_le_bytes([
                data[offset], data[offset+1], data[offset+2], data[offset+3],
                data[offset+4], data[offset+5], data[offset+6], data[offset+7],
            ]);
            tick.close = close_raw as f64 / 100.0;
        }

        Some(tick)
    }

    /// Map exchange type to string
    fn exchange_type_to_string(exchange_type: u8) -> &'static str {
        match exchange_type {
            1 => "NSE",
            2 => "NFO",
            3 => "BSE",
            4 => "BFO",
            5 => "MCX",
            _ => "UNKNOWN",
        }
    }

    /// Parse symbol format (exchange:token) to exchange type
    fn parse_exchange_type(exchange: &str) -> u8 {
        match exchange.to_uppercase().as_str() {
            "NSE" | "NSE_CM" => NSE_CM,
            "NFO" | "NSE_FO" => NSE_FO,
            "BSE" | "BSE_CM" => BSE_CM,
            "BFO" | "BSE_FO" => BSE_FO,
            "MCX" | "MCX_FO" => MCX_FO,
            _ => NSE_CM,
        }
    }
}

#[async_trait]
impl WebSocketAdapter for AngelOneAdapter {
    async fn connect(&mut self) -> anyhow::Result<()> {
        tracing::info!("Connecting to Angel One WebSocket");

        // Build request with headers
        let uri: Uri = ANGEL_WS_URL.parse()?;
        let request = Request::builder()
            .uri(&uri)
            .header("Host", uri.host().unwrap_or("smartapisocket.angelone.in"))
            .header("Authorization", &self.auth_token)
            .header("x-api-key", &self.api_key)
            .header("x-client-code", &self.client_code)
            .header("x-feed-token", &self.feed_token)
            .header("Connection", "Upgrade")
            .header("Upgrade", "websocket")
            .header("Sec-WebSocket-Version", "13")
            .header("Sec-WebSocket-Key", tokio_tungstenite::tungstenite::handshake::client::generate_key())
            .body(())?;

        let (ws_stream, _) = connect_async_with_config(request, None, false).await?;
        tracing::info!("Connected to Angel One WebSocket");

        *self.ws_stream.write().await = Some(ws_stream);
        *self.is_connected.write().await = true;

        // Start message handler
        let ws_stream = self.ws_stream.clone();
        let is_connected = self.is_connected.clone();

        tokio::spawn(async move {
            loop {
                let mut stream = ws_stream.write().await;
                if let Some(ref mut ws) = *stream {
                    match ws.next().await {
                        Some(Ok(Message::Binary(data))) => {
                            tracing::debug!("Received binary message: {} bytes", data.len());
                        }
                        Some(Ok(Message::Text(text))) => {
                            if text == "pong" {
                                tracing::debug!("Received pong");
                            } else {
                                tracing::debug!("Received text: {}", text);
                            }
                        }
                        Some(Ok(Message::Ping(data))) => {
                            if let Err(e) = ws.send(Message::Pong(data)).await {
                                tracing::error!("Failed to send pong: {}", e);
                            }
                        }
                        Some(Err(e)) => {
                            tracing::error!("WebSocket error: {}", e);
                            *is_connected.write().await = false;
                            break;
                        }
                        None => {
                            tracing::info!("WebSocket closed");
                            *is_connected.write().await = false;
                            break;
                        }
                        _ => {}
                    }
                } else {
                    break;
                }
                drop(stream);
                tokio::time::sleep(tokio::time::Duration::from_millis(1)).await;
            }
        });

        // Start heartbeat
        let ws_stream_hb = self.ws_stream.clone();
        let is_connected_hb = self.is_connected.clone();
        tokio::spawn(async move {
            loop {
                tokio::time::sleep(tokio::time::Duration::from_secs(HEARTBEAT_INTERVAL)).await;
                if !*is_connected_hb.read().await {
                    break;
                }
                if let Some(ref mut ws) = *ws_stream_hb.write().await {
                    if let Err(e) = ws.send(Message::Text("ping".to_string())).await {
                        tracing::error!("Failed to send heartbeat: {}", e);
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
        tracing::info!("Disconnected from Angel One WebSocket");
        Ok(())
    }

    async fn subscribe(
        &mut self,
        symbol: &str,
        channel: &str,
        params: Option<Value>,
    ) -> anyhow::Result<()> {
        let mode = match channel {
            "depth" => AngelMode::Depth,
            "quote" => AngelMode::Quote,
            "snap" | "snap_quote" => AngelMode::SnapQuote,
            _ => AngelMode::Ltp,
        };

        // Parse symbol format: "NSE:12345" or "NFO:56789"
        let parts: Vec<&str> = symbol.split(':').collect();
        if parts.len() != 2 {
            return Err(anyhow::anyhow!("Invalid symbol format. Expected 'Exchange:Token'"));
        }

        let exchange = parts[0];
        let token = parts[1].to_string();
        let exchange_type = Self::parse_exchange_type(exchange);

        // Store mapping
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

        if let Some(ref mut ws) = *self.ws_stream.write().await {
            ws.send(Message::Text(serde_json::to_string(&request)?)).await?;
            self.subscriptions.write().await.insert(symbol.to_string(), mode);
            tracing::info!("Subscribed to {} on channel {}", symbol, channel);
        }

        Ok(())
    }

    async fn unsubscribe(&mut self, symbol: &str, channel: &str) -> anyhow::Result<()> {
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
                tracing::info!("Unsubscribed from {}", symbol);
            }
        }

        Ok(())
    }

    fn set_message_callback(&mut self, callback: Box<dyn Fn(MarketMessage) + Send + Sync>) {
        self.message_callback = Some(callback);
    }

    fn provider_name(&self) -> &str {
        "angelone"
    }

    fn is_connected(&self) -> bool {
        self.is_connected.try_read().map(|g| *g).unwrap_or(false)
    }
}
