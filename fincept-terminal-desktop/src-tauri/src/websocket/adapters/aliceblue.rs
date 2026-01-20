// Alice Blue WebSocket Adapter
//
// Alice Blue (ANT) uses WebSocket for market data:
// - WebSocket URL: wss://ws1.aliceblueonline.com/NorenWS/
// - Authentication via user session token
// - JSON-based subscription protocol

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
use tokio_tungstenite::{connect_async, tungstenite::Message, MaybeTlsStream, WebSocketStream};

// ============================================================================
// ALICE BLUE CONFIGURATION
// ============================================================================

const ALICEBLUE_WS_URL: &str = "wss://ws1.aliceblueonline.com/NorenWS/";

// ============================================================================
// ALICE BLUE STRUCTURES
// ============================================================================

#[derive(Debug, Clone, PartialEq)]
pub enum AliceBlueMode {
    Ltp,
    Quote,
    Depth,
}

#[derive(Debug, Serialize)]
struct ConnectRequest {
    t: String,  // "c" for connect
    uid: String,
    actid: String,
    susertoken: String,
    source: String,
}

#[derive(Debug, Serialize)]
struct SubscribeRequest {
    t: String,  // "t" for subscribe, "u" for unsubscribe, "d" for depth
    k: String,  // scrips in format "exchange|token"
}

#[derive(Debug, Deserialize)]
struct TickData {
    t: Option<String>,
    tk: Option<String>,  // Token
    e: Option<String>,   // Exchange
    lp: Option<String>,  // LTP
    v: Option<String>,   // Volume
    o: Option<String>,   // Open
    h: Option<String>,   // High
    l: Option<String>,   // Low
    c: Option<String>,   // Close
    bp1: Option<String>, // Best bid price 1
    bq1: Option<String>, // Best bid qty 1
    sp1: Option<String>, // Best ask price 1
    sq1: Option<String>, // Best ask qty 1
}

#[derive(Debug, Clone)]
pub struct AliceBlueTick {
    pub token: String,
    pub exchange: String,
    pub ltp: f64,
    pub volume: i64,
    pub open: f64,
    pub high: f64,
    pub low: f64,
    pub close: f64,
    pub bid_price: f64,
    pub bid_qty: i64,
    pub ask_price: f64,
    pub ask_qty: i64,
}

// ============================================================================
// ALICE BLUE ADAPTER
// ============================================================================

pub struct AliceBlueAdapter {
    config: ProviderConfig,
    user_id: String,
    session_token: String,
    ws_stream: Arc<RwLock<Option<WebSocketStream<MaybeTlsStream<TcpStream>>>>>,
    message_callback: Option<Box<dyn Fn(MarketMessage) + Send + Sync>>,
    subscriptions: Arc<RwLock<HashMap<String, AliceBlueMode>>>,
    is_connected: Arc<RwLock<bool>>,
}

impl AliceBlueAdapter {
    pub fn new(config: ProviderConfig) -> Self {
        let user_id = config.client_id.clone().unwrap_or_default();
        let session_token = config.api_key.clone().unwrap_or_default();

        Self {
            config,
            user_id,
            session_token,
            ws_stream: Arc::new(RwLock::new(None)),
            message_callback: None,
            subscriptions: Arc::new(RwLock::new(HashMap::new())),
            is_connected: Arc::new(RwLock::new(false)),
        }
    }

    fn parse_tick_data(&self, data: &str) -> Option<AliceBlueTick> {
        let tick: TickData = serde_json::from_str(data).ok()?;

        // Skip non-tick messages
        if tick.t.as_deref() != Some("tk") && tick.t.as_deref() != Some("tf") && tick.t.as_deref() != Some("dk") {
            return None;
        }

        Some(AliceBlueTick {
            token: tick.tk.unwrap_or_default(),
            exchange: tick.e.unwrap_or_default(),
            ltp: tick.lp.and_then(|s| s.parse().ok()).unwrap_or(0.0),
            volume: tick.v.and_then(|s| s.parse().ok()).unwrap_or(0),
            open: tick.o.and_then(|s| s.parse().ok()).unwrap_or(0.0),
            high: tick.h.and_then(|s| s.parse().ok()).unwrap_or(0.0),
            low: tick.l.and_then(|s| s.parse().ok()).unwrap_or(0.0),
            close: tick.c.and_then(|s| s.parse().ok()).unwrap_or(0.0),
            bid_price: tick.bp1.and_then(|s| s.parse().ok()).unwrap_or(0.0),
            bid_qty: tick.bq1.and_then(|s| s.parse().ok()).unwrap_or(0),
            ask_price: tick.sp1.and_then(|s| s.parse().ok()).unwrap_or(0.0),
            ask_qty: tick.sq1.and_then(|s| s.parse().ok()).unwrap_or(0),
        })
    }
}

#[async_trait]
impl WebSocketAdapter for AliceBlueAdapter {
    async fn connect(&mut self) -> anyhow::Result<()> {
        tracing::info!("Connecting to Alice Blue WebSocket");

        let (ws_stream, _) = connect_async(ALICEBLUE_WS_URL).await?;
        tracing::info!("Connected to Alice Blue WebSocket");

        *self.ws_stream.write().await = Some(ws_stream);

        // Send connect request
        let connect_req = ConnectRequest {
            t: "c".to_string(),
            uid: self.user_id.clone(),
            actid: self.user_id.clone(),
            susertoken: self.session_token.clone(),
            source: "API".to_string(),
        };

        if let Some(ref mut ws) = *self.ws_stream.write().await {
            ws.send(Message::Text(serde_json::to_string(&connect_req)?)).await?;
        }

        *self.is_connected.write().await = true;

        let ws_stream = self.ws_stream.clone();
        let is_connected = self.is_connected.clone();

        tokio::spawn(async move {
            loop {
                let mut stream = ws_stream.write().await;
                if let Some(ref mut ws) = *stream {
                    match ws.next().await {
                        Some(Ok(Message::Text(text))) => {
                            tracing::debug!("Received: {}", text);
                        }
                        Some(Ok(Message::Ping(data))) => {
                            let _ = ws.send(Message::Pong(data)).await;
                        }
                        Some(Err(e)) => {
                            tracing::error!("WebSocket error: {}", e);
                            *is_connected.write().await = false;
                            break;
                        }
                        None => {
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
        if let Some(ref mut ws) = *self.ws_stream.write().await {
            ws.close(None).await?;
        }
        *self.ws_stream.write().await = None;
        *self.is_connected.write().await = false;
        tracing::info!("Disconnected from Alice Blue WebSocket");
        Ok(())
    }

    async fn subscribe(
        &mut self,
        symbol: &str,
        channel: &str,
        _params: Option<Value>,
    ) -> anyhow::Result<()> {
        let mode = match channel {
            "depth" => AliceBlueMode::Depth,
            "quote" => AliceBlueMode::Quote,
            _ => AliceBlueMode::Ltp,
        };

        // Symbol format: "NSE|12345" or "NFO|67890"
        let msg_type = match mode {
            AliceBlueMode::Depth => "d",
            _ => "t",
        };

        let subscribe_req = SubscribeRequest {
            t: msg_type.to_string(),
            k: symbol.to_string(),
        };

        if let Some(ref mut ws) = *self.ws_stream.write().await {
            ws.send(Message::Text(serde_json::to_string(&subscribe_req)?)).await?;
            self.subscriptions.write().await.insert(symbol.to_string(), mode);
            tracing::info!("Subscribed to {}", symbol);
        }

        Ok(())
    }

    async fn unsubscribe(&mut self, symbol: &str, _channel: &str) -> anyhow::Result<()> {
        let unsubscribe_req = SubscribeRequest {
            t: "u".to_string(),
            k: symbol.to_string(),
        };

        if let Some(ref mut ws) = *self.ws_stream.write().await {
            ws.send(Message::Text(serde_json::to_string(&unsubscribe_req)?)).await?;
            self.subscriptions.write().await.remove(symbol);
            tracing::info!("Unsubscribed from {}", symbol);
        }

        Ok(())
    }

    fn set_message_callback(&mut self, callback: Box<dyn Fn(MarketMessage) + Send + Sync>) {
        self.message_callback = Some(callback);
    }

    fn provider_name(&self) -> &str {
        "aliceblue"
    }

    fn is_connected(&self) -> bool {
        self.is_connected.try_read().map(|g| *g).unwrap_or(false)
    }
}
