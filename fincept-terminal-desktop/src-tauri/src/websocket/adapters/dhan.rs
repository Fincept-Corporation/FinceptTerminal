#![allow(dead_code)]
// Dhan WebSocket Adapter (Binary Protocol)
//
// Dhan uses binary-encoded market data over WebSocket:
// - 5-level depth: wss://api-feed.dhan.co
// - 20-level depth: wss://depth-api-feed.dhan.co/twentydepth
// - Authentication via URL parameters (token, clientId)
// - Binary packet format with struct-based parsing

use super::WebSocketAdapter;
use crate::websocket::types::{
    MarketMessage, TickerData, OrderBookData, OrderBookLevel, ProviderConfig,
};
use async_trait::async_trait;
use futures_util::{SinkExt, StreamExt};
use serde::{Deserialize, Serialize};
use serde_json::Value;
use std::collections::HashMap;
use std::sync::Arc;
use tokio::net::TcpStream;
use tokio::sync::RwLock;
use tokio_tungstenite::{connect_async, tungstenite::Message, MaybeTlsStream, WebSocketStream};

// ============================================================================
// DHAN CONFIGURATION
// ============================================================================

const DHAN_WS_URL_5DEPTH: &str = "wss://api-feed.dhan.co";
const DHAN_WS_URL_20DEPTH: &str = "wss://depth-api-feed.dhan.co/twentydepth";

// Feed response codes
const FEED_TICKER: u8 = 2;
const FEED_QUOTE: u8 = 4;
const FEED_OI: u8 = 5;
const FEED_PREV_CLOSE: u8 = 6;
const FEED_FULL: u8 = 8;
const FEED_DEPTH_20_BID: u8 = 41;
const FEED_DEPTH_20_ASK: u8 = 51;
const FEED_DISCONNECT: u8 = 50;

// Request codes
const REQUEST_SUBSCRIBE_TICKER: u8 = 15;
const REQUEST_SUBSCRIBE_QUOTE: u8 = 17;
const REQUEST_SUBSCRIBE_FULL: u8 = 21;
const REQUEST_SUBSCRIBE_20_DEPTH: u8 = 23;
const REQUEST_DISCONNECT: u8 = 12;

// ============================================================================
// DHAN STRUCTURES
// ============================================================================

#[derive(Debug, Clone, PartialEq)]
pub enum DhanMode {
    Ticker,
    Quote,
    Full,
    Depth20,
}

impl DhanMode {
    fn request_code(&self) -> u8 {
        match self {
            DhanMode::Ticker => REQUEST_SUBSCRIBE_TICKER,
            DhanMode::Quote => REQUEST_SUBSCRIBE_QUOTE,
            DhanMode::Full => REQUEST_SUBSCRIBE_FULL,
            DhanMode::Depth20 => REQUEST_SUBSCRIBE_20_DEPTH,
        }
    }
}

#[derive(Debug, Serialize)]
struct SubscriptionMessage {
    #[serde(rename = "RequestCode")]
    request_code: u8,
    #[serde(rename = "InstrumentCount")]
    instrument_count: usize,
    #[serde(rename = "InstrumentList")]
    instrument_list: Vec<DhanInstrument>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DhanInstrument {
    #[serde(rename = "ExchangeSegment")]
    pub exchange_segment: String,
    #[serde(rename = "SecurityId")]
    pub security_id: String,
}

#[derive(Debug, Clone)]
pub struct DhanTick {
    pub exchange_segment: u8,
    pub security_id: u32,
    pub ltp: f32,
    pub ltq: u16,
    pub ltt: u32,
    pub atp: f32,
    pub volume: u32,
    pub total_sell_qty: u32,
    pub total_buy_qty: u32,
    pub open: f32,
    pub close: f32,
    pub high: f32,
    pub low: f32,
    pub oi: u32,
    pub depth: Option<DhanDepth>,
}

#[derive(Debug, Clone)]
pub struct DhanDepth {
    pub buy: Vec<DhanDepthLevel>,
    pub sell: Vec<DhanDepthLevel>,
}

#[derive(Debug, Clone)]
pub struct DhanDepthLevel {
    pub price: f32,
    pub quantity: u32,
    pub orders: u16,
}

// ============================================================================
// DHAN ADAPTER
// ============================================================================

pub struct DhanAdapter {
    config: ProviderConfig,
    client_id: String,
    access_token: String,
    is_20_depth: bool,
    ws_stream: Arc<RwLock<Option<WebSocketStream<MaybeTlsStream<TcpStream>>>>>,
    message_callback: Option<Box<dyn Fn(MarketMessage) + Send + Sync>>,
    subscriptions: Arc<RwLock<HashMap<String, DhanMode>>>,
    is_connected: Arc<RwLock<bool>>,
    security_symbol_map: Arc<RwLock<HashMap<u32, String>>>,
}

impl DhanAdapter {
    pub fn new(config: ProviderConfig) -> Self {
        let client_id = config.client_id.clone().unwrap_or_default();
        let access_token = config.api_key.clone().unwrap_or_default();
        let is_20_depth = config.extra.as_ref()
            .and_then(|e| e.get("is_20_depth"))
            .and_then(|v| v.as_bool())
            .unwrap_or(false);

        Self {
            config,
            client_id,
            access_token,
            is_20_depth,
            ws_stream: Arc::new(RwLock::new(None)),
            message_callback: None,
            subscriptions: Arc::new(RwLock::new(HashMap::new())),
            is_connected: Arc::new(RwLock::new(false)),
            security_symbol_map: Arc::new(RwLock::new(HashMap::new())),
        }
    }

    /// Build WebSocket URL with authentication
    fn build_ws_url(&self) -> String {
        let base_url = if self.is_20_depth {
            DHAN_WS_URL_20DEPTH
        } else {
            DHAN_WS_URL_5DEPTH
        };

        if self.is_20_depth {
            format!(
                "{}?token={}&clientId={}&authType=2",
                base_url, self.access_token, self.client_id
            )
        } else {
            format!(
                "{}?version=2&token={}&clientId={}&authType=2",
                base_url, self.access_token, self.client_id
            )
        }
    }

    /// Parse binary message header
    fn parse_header(&self, data: &[u8]) -> Option<(u8, u16, u8, u32)> {
        if data.len() < 8 {
            return None;
        }

        let feed_response_code = data[0];
        let message_length = u16::from_le_bytes([data[1], data[2]]);
        let exchange_segment = data[3];
        let security_id = u32::from_le_bytes([data[4], data[5], data[6], data[7]]);

        Some((feed_response_code, message_length, exchange_segment, security_id))
    }

    /// Parse ticker packet (LTP and LTT)
    fn parse_ticker(&self, payload: &[u8], exchange_segment: u8, security_id: u32) -> Option<DhanTick> {
        if payload.len() < 8 {
            return None;
        }

        let ltp = f32::from_le_bytes([payload[0], payload[1], payload[2], payload[3]]);
        let ltt = u32::from_le_bytes([payload[4], payload[5], payload[6], payload[7]]);

        Some(DhanTick {
            exchange_segment,
            security_id,
            ltp,
            ltq: 0,
            ltt,
            atp: 0.0,
            volume: 0,
            total_sell_qty: 0,
            total_buy_qty: 0,
            open: 0.0,
            close: 0.0,
            high: 0.0,
            low: 0.0,
            oi: 0,
            depth: None,
        })
    }

    /// Parse quote packet
    fn parse_quote(&self, payload: &[u8], exchange_segment: u8, security_id: u32) -> Option<DhanTick> {
        if payload.len() < 42 {
            return None;
        }

        Some(DhanTick {
            exchange_segment,
            security_id,
            ltp: f32::from_le_bytes([payload[0], payload[1], payload[2], payload[3]]),
            ltq: u16::from_le_bytes([payload[4], payload[5]]),
            ltt: u32::from_le_bytes([payload[6], payload[7], payload[8], payload[9]]),
            atp: f32::from_le_bytes([payload[10], payload[11], payload[12], payload[13]]),
            volume: u32::from_le_bytes([payload[14], payload[15], payload[16], payload[17]]),
            total_sell_qty: u32::from_le_bytes([payload[18], payload[19], payload[20], payload[21]]),
            total_buy_qty: u32::from_le_bytes([payload[22], payload[23], payload[24], payload[25]]),
            open: f32::from_le_bytes([payload[26], payload[27], payload[28], payload[29]]),
            close: f32::from_le_bytes([payload[30], payload[31], payload[32], payload[33]]),
            high: f32::from_le_bytes([payload[34], payload[35], payload[36], payload[37]]),
            low: f32::from_le_bytes([payload[38], payload[39], payload[40], payload[41]]),
            oi: 0,
            depth: None,
        })
    }

    /// Parse full packet with 5-level depth
    fn parse_full(&self, payload: &[u8], exchange_segment: u8, security_id: u32) -> Option<DhanTick> {
        if payload.len() < 154 {
            tracing::warn!("Full packet payload too short: {} bytes", payload.len());
            return None;
        }

        let mut tick = DhanTick {
            exchange_segment,
            security_id,
            ltp: f32::from_le_bytes([payload[0], payload[1], payload[2], payload[3]]),
            ltq: u16::from_le_bytes([payload[4], payload[5]]),
            ltt: u32::from_le_bytes([payload[6], payload[7], payload[8], payload[9]]),
            atp: f32::from_le_bytes([payload[10], payload[11], payload[12], payload[13]]),
            volume: u32::from_le_bytes([payload[14], payload[15], payload[16], payload[17]]),
            total_sell_qty: u32::from_le_bytes([payload[18], payload[19], payload[20], payload[21]]),
            total_buy_qty: u32::from_le_bytes([payload[22], payload[23], payload[24], payload[25]]),
            oi: u32::from_le_bytes([payload[26], payload[27], payload[28], payload[29]]),
            open: f32::from_le_bytes([payload[38], payload[39], payload[40], payload[41]]),
            close: f32::from_le_bytes([payload[42], payload[43], payload[44], payload[45]]),
            high: f32::from_le_bytes([payload[46], payload[47], payload[48], payload[49]]),
            low: f32::from_le_bytes([payload[50], payload[51], payload[52], payload[53]]),
            depth: None,
        };

        // Parse 5-level depth starting at offset 54
        let mut buy_levels = Vec::new();
        let mut sell_levels = Vec::new();

        for i in 0..5 {
            let offset = 54 + (i * 20);
            if offset + 20 > payload.len() {
                break;
            }

            let bid_qty = u32::from_le_bytes([
                payload[offset], payload[offset + 1], payload[offset + 2], payload[offset + 3]
            ]);
            let ask_qty = u32::from_le_bytes([
                payload[offset + 4], payload[offset + 5], payload[offset + 6], payload[offset + 7]
            ]);
            let bid_orders = u16::from_le_bytes([payload[offset + 8], payload[offset + 9]]);
            let ask_orders = u16::from_le_bytes([payload[offset + 10], payload[offset + 11]]);
            let bid_price = f32::from_le_bytes([
                payload[offset + 12], payload[offset + 13], payload[offset + 14], payload[offset + 15]
            ]);
            let ask_price = f32::from_le_bytes([
                payload[offset + 16], payload[offset + 17], payload[offset + 18], payload[offset + 19]
            ]);

            buy_levels.push(DhanDepthLevel {
                price: bid_price,
                quantity: bid_qty,
                orders: bid_orders,
            });
            sell_levels.push(DhanDepthLevel {
                price: ask_price,
                quantity: ask_qty,
                orders: ask_orders,
            });
        }

        tick.depth = Some(DhanDepth {
            buy: buy_levels,
            sell: sell_levels,
        });

        Some(tick)
    }

    /// Parse binary message
    fn parse_message(&self, data: &[u8]) -> Vec<DhanTick> {
        let mut ticks = Vec::new();
        let mut offset = 0;

        while offset < data.len() {
            if offset + 8 > data.len() {
                break;
            }

            let feed_response_code = data[offset];
            let message_length = u16::from_le_bytes([data[offset + 1], data[offset + 2]]) as usize;
            let exchange_segment = data[offset + 3];
            let security_id = u32::from_le_bytes([
                data[offset + 4], data[offset + 5], data[offset + 6], data[offset + 7]
            ]);

            let payload_start = offset + 8;
            let payload_end = offset + message_length;

            if payload_end > data.len() {
                tracing::warn!("Incomplete message received");
                break;
            }

            let payload = &data[payload_start..payload_end];

            let tick = match feed_response_code {
                FEED_TICKER => self.parse_ticker(payload, exchange_segment, security_id),
                FEED_QUOTE => self.parse_quote(payload, exchange_segment, security_id),
                FEED_FULL => self.parse_full(payload, exchange_segment, security_id),
                FEED_DISCONNECT => {
                    if payload.len() >= 2 {
                        let disconnect_code = u16::from_le_bytes([payload[0], payload[1]]);
                        tracing::warn!("Received disconnect packet with code: {}", disconnect_code);
                    }
                    None
                }
                _ => {
                    tracing::debug!("Unknown feed response code: {}", feed_response_code);
                    None
                }
            };

            if let Some(t) = tick {
                ticks.push(t);
            }

            offset = payload_end;
        }

        ticks
    }

    /// Convert tick to MarketMessage
    fn tick_to_message(&self, tick: DhanTick) -> MarketMessage {
        let symbol = self.security_symbol_map
            .try_read()
            .ok()
            .and_then(|map| map.get(&tick.security_id).cloned())
            .unwrap_or_else(|| format!("{}:{}", tick.exchange_segment, tick.security_id));

        if let Some(depth) = tick.depth {
            MarketMessage::OrderBook(OrderBookData {
                provider: "dhan".to_string(),
                symbol,
                bids: depth.buy.iter().map(|l| OrderBookLevel {
                    price: l.price as f64,
                    quantity: l.quantity as f64,
                    count: Some(l.orders as u32),
                }).collect(),
                asks: depth.sell.iter().map(|l| OrderBookLevel {
                    price: l.price as f64,
                    quantity: l.quantity as f64,
                    count: Some(l.orders as u32),
                }).collect(),
                timestamp: tick.ltt as u64,
                is_snapshot: true,
            })
        } else {
            MarketMessage::Ticker(TickerData {
                provider: "dhan".to_string(),
                symbol,
                price: tick.ltp as f64,
                bid: None,
                ask: None,
                bid_size: None,
                ask_size: None,
                volume: Some(tick.volume as f64),
                high: Some(tick.high as f64),
                low: Some(tick.low as f64),
                open: Some(tick.open as f64),
                close: Some(tick.close as f64),
                change: None,
                change_percent: None,
                quote_volume: None,
                timestamp: tick.ltt as u64,
            })
        }
    }
}

#[async_trait]
impl WebSocketAdapter for DhanAdapter {
    async fn connect(&mut self) -> anyhow::Result<()> {
        let ws_url = self.build_ws_url();
        tracing::info!("Connecting to Dhan WebSocket ({})", if self.is_20_depth { "20-depth" } else { "5-depth" });

        let (ws_stream, _) = connect_async(&ws_url).await?;
        tracing::info!("Connected to Dhan WebSocket");

        *self.ws_stream.write().await = Some(ws_stream);
        *self.is_connected.write().await = true;

        // Start message handler
        let ws_stream = self.ws_stream.clone();
        let is_connected = self.is_connected.clone();
        let _security_symbol_map = self.security_symbol_map.clone();

        tokio::spawn(async move {
            loop {
                let mut stream = ws_stream.write().await;
                if let Some(ref mut ws) = *stream {
                    match ws.next().await {
                        Some(Ok(Message::Binary(data))) => {
                            tracing::debug!("Received binary message: {} bytes", data.len());
                            // Parse and emit via callback
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

        Ok(())
    }

    async fn disconnect(&mut self) -> anyhow::Result<()> {
        // Send disconnect message
        let disconnect_msg = serde_json::json!({
            "RequestCode": REQUEST_DISCONNECT
        });

        if let Some(ref mut ws) = *self.ws_stream.write().await {
            let _ = ws.send(Message::Text(disconnect_msg.to_string())).await;
            ws.close(None).await?;
        }

        *self.ws_stream.write().await = None;
        *self.is_connected.write().await = false;
        tracing::info!("Disconnected from Dhan WebSocket");
        Ok(())
    }

    async fn subscribe(
        &mut self,
        symbol: &str,
        channel: &str,
        _params: Option<Value>,
    ) -> anyhow::Result<()> {
        let mode = match channel {
            "full" | "depth" => DhanMode::Full,
            "quote" => DhanMode::Quote,
            "20depth" => DhanMode::Depth20,
            _ => DhanMode::Ticker,
        };

        // Parse symbol: format is "ExchangeSegment:SecurityId" (e.g., "NSE_EQ:1594")
        let parts: Vec<&str> = symbol.split(':').collect();
        if parts.len() != 2 {
            return Err(anyhow::anyhow!("Invalid symbol format. Expected 'ExchangeSegment:SecurityId'"));
        }

        let instrument = DhanInstrument {
            exchange_segment: parts[0].to_string(),
            security_id: parts[1].to_string(),
        };

        // Store symbol mapping
        if let Ok(security_id) = parts[1].parse::<u32>() {
            self.security_symbol_map.write().await.insert(security_id, symbol.to_string());
        }

        let message = SubscriptionMessage {
            request_code: mode.request_code(),
            instrument_count: 1,
            instrument_list: vec![instrument],
        };

        if let Some(ref mut ws) = *self.ws_stream.write().await {
            ws.send(Message::Text(serde_json::to_string(&message)?)).await?;
            self.subscriptions.write().await.insert(symbol.to_string(), mode);
            tracing::info!("Subscribed to {} on channel {}", symbol, channel);
        }

        Ok(())
    }

    async fn unsubscribe(&mut self, symbol: &str, _channel: &str) -> anyhow::Result<()> {
        self.subscriptions.write().await.remove(symbol);
        tracing::info!("Unsubscribed from {}", symbol);
        Ok(())
    }

    fn set_message_callback(&mut self, callback: Box<dyn Fn(MarketMessage) + Send + Sync>) {
        self.message_callback = Some(callback);
    }

    fn provider_name(&self) -> &str {
        "dhan"
    }

    fn is_connected(&self) -> bool {
        self.is_connected.try_read().map(|g| *g).unwrap_or(false)
    }
}
