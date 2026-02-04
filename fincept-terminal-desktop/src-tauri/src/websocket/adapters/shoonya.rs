#![allow(dead_code)]
// Shoonya (Finvasia) WebSocket Adapter
//
// Shoonya uses NorenOMS WebSocket for market data:
// - WebSocket URL: wss://api.shoonya.com/NorenWSTP/
// - Authentication via session token from login
// - JSON message format with touch line and depth subscriptions

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
// SHOONYA CONFIGURATION
// ============================================================================

const SHOONYA_WS_URL: &str = "wss://api.shoonya.com/NorenWSTP/";

// ============================================================================
// SHOONYA STRUCTURES
// ============================================================================

#[derive(Debug, Clone, PartialEq)]
pub enum ShoonyaMode {
    TouchLine,  // LTP + OHLC
    Depth,      // Full market depth
}

#[derive(Debug, Serialize)]
struct ConnectRequest {
    t: String,           // "c" for connect
    uid: String,         // User ID
    actid: String,       // Account ID
    susertoken: String,  // Session token
    source: String,      // "API"
}

#[derive(Debug, Serialize)]
struct TouchLineSubscribe {
    t: String,  // "t" for touchline subscribe
    k: String,  // Scrip tokens (e.g., "NSE|26000#NSE|26009")
}

#[derive(Debug, Serialize)]
struct DepthSubscribe {
    t: String,  // "d" for depth subscribe
    k: String,  // Scrip tokens
}

#[derive(Debug, Serialize)]
struct Unsubscribe {
    t: String,  // "u" for unsubscribe
    k: String,  // Scrip tokens
}

#[derive(Debug, Deserialize)]
struct TickResponse {
    t: Option<String>,    // Message type
    e: Option<String>,    // Exchange
    tk: Option<String>,   // Token
    lp: Option<String>,   // LTP
    pc: Option<String>,   // Percent change
    v: Option<String>,    // Volume
    o: Option<String>,    // Open
    h: Option<String>,    // High
    l: Option<String>,    // Low
    c: Option<String>,    // Close
    ap: Option<String>,   // Average price
    oi: Option<String>,   // Open interest
    bp1: Option<String>,  // Best bid price 1
    bq1: Option<String>,  // Best bid qty 1
    sp1: Option<String>,  // Best ask price 1
    sq1: Option<String>,  // Best ask qty 1
}

#[derive(Debug, Clone)]
pub struct ShoonyaTick {
    pub exchange: String,
    pub token: String,
    pub ltp: f64,
    pub change_percent: f64,
    pub volume: i64,
    pub open: f64,
    pub high: f64,
    pub low: f64,
    pub close: f64,
    pub avg_price: f64,
    pub oi: i64,
    pub bid_price: f64,
    pub bid_qty: i64,
    pub ask_price: f64,
    pub ask_qty: i64,
}

// ============================================================================
// SHOONYA ADAPTER
// ============================================================================

pub struct ShoonyaAdapter {
    config: ProviderConfig,
    user_id: String,
    session_token: String,
    ws_stream: Arc<RwLock<Option<WebSocketStream<MaybeTlsStream<TcpStream>>>>>,
    message_callback: Option<Box<dyn Fn(MarketMessage) + Send + Sync>>,
    subscriptions: Arc<RwLock<HashMap<String, ShoonyaMode>>>,
    is_connected: Arc<RwLock<bool>>,
}

impl ShoonyaAdapter {
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

    fn parse_tick(&self, data: &str) -> Option<ShoonyaTick> {
        let tick: TickResponse = serde_json::from_str(data).ok()?;

        // Skip non-tick messages (like connection acknowledgment)
        if tick.t.as_deref() != Some("tf") && tick.t.as_deref() != Some("tk") && tick.t.as_deref() != Some("dk") {
            return None;
        }

        Some(ShoonyaTick {
            exchange: tick.e.unwrap_or_default(),
            token: tick.tk.unwrap_or_default(),
            ltp: tick.lp.and_then(|s| s.parse().ok()).unwrap_or(0.0),
            change_percent: tick.pc.and_then(|s| s.parse().ok()).unwrap_or(0.0),
            volume: tick.v.and_then(|s| s.parse().ok()).unwrap_or(0),
            open: tick.o.and_then(|s| s.parse().ok()).unwrap_or(0.0),
            high: tick.h.and_then(|s| s.parse().ok()).unwrap_or(0.0),
            low: tick.l.and_then(|s| s.parse().ok()).unwrap_or(0.0),
            close: tick.c.and_then(|s| s.parse().ok()).unwrap_or(0.0),
            avg_price: tick.ap.and_then(|s| s.parse().ok()).unwrap_or(0.0),
            oi: tick.oi.and_then(|s| s.parse().ok()).unwrap_or(0),
            bid_price: tick.bp1.and_then(|s| s.parse().ok()).unwrap_or(0.0),
            bid_qty: tick.bq1.and_then(|s| s.parse().ok()).unwrap_or(0),
            ask_price: tick.sp1.and_then(|s| s.parse().ok()).unwrap_or(0.0),
            ask_qty: tick.sq1.and_then(|s| s.parse().ok()).unwrap_or(0),
        })
    }
}

#[async_trait]
impl WebSocketAdapter for ShoonyaAdapter {
    async fn connect(&mut self) -> anyhow::Result<()> {
        tracing::info!("Connecting to Shoonya WebSocket");

        let (ws_stream, _) = connect_async(SHOONYA_WS_URL).await?;
        tracing::info!("Connected to Shoonya WebSocket");

        *self.ws_stream.write().await = Some(ws_stream);

        // Send connect/auth message
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

        // Start heartbeat (send "h" every 30 seconds)
        let ws_stream_hb = self.ws_stream.clone();
        let is_connected_hb = self.is_connected.clone();
        tokio::spawn(async move {
            loop {
                tokio::time::sleep(tokio::time::Duration::from_secs(30)).await;
                if !*is_connected_hb.read().await {
                    break;
                }
                if let Some(ref mut ws) = *ws_stream_hb.write().await {
                    let hb = serde_json::json!({"t": "h"});
                    if let Err(e) = ws.send(Message::Text(hb.to_string())).await {
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
        tracing::info!("Disconnected from Shoonya WebSocket");
        Ok(())
    }

    async fn subscribe(
        &mut self,
        symbol: &str,
        channel: &str,
        _params: Option<Value>,
    ) -> anyhow::Result<()> {
        let mode = match channel {
            "depth" => ShoonyaMode::Depth,
            _ => ShoonyaMode::TouchLine,
        };

        // Symbol format: "NSE|26000" (Exchange|Token)
        let subscribe_msg = match mode {
            ShoonyaMode::TouchLine => {
                serde_json::to_string(&TouchLineSubscribe {
                    t: "t".to_string(),
                    k: symbol.to_string(),
                })?
            }
            ShoonyaMode::Depth => {
                serde_json::to_string(&DepthSubscribe {
                    t: "d".to_string(),
                    k: symbol.to_string(),
                })?
            }
        };

        if let Some(ref mut ws) = *self.ws_stream.write().await {
            ws.send(Message::Text(subscribe_msg)).await?;
            self.subscriptions.write().await.insert(symbol.to_string(), mode);
            tracing::info!("Subscribed to {}", symbol);
        }

        Ok(())
    }

    async fn unsubscribe(&mut self, symbol: &str, _channel: &str) -> anyhow::Result<()> {
        let unsubscribe_msg = serde_json::to_string(&Unsubscribe {
            t: "u".to_string(),
            k: symbol.to_string(),
        })?;

        if let Some(ref mut ws) = *self.ws_stream.write().await {
            ws.send(Message::Text(unsubscribe_msg)).await?;
            self.subscriptions.write().await.remove(symbol);
            tracing::info!("Unsubscribed from {}", symbol);
        }

        Ok(())
    }

    fn set_message_callback(&mut self, callback: Box<dyn Fn(MarketMessage) + Send + Sync>) {
        self.message_callback = Some(callback);
    }

    fn provider_name(&self) -> &str {
        "shoonya"
    }

    fn is_connected(&self) -> bool {
        self.is_connected.try_read().map(|g| *g).unwrap_or(false)
    }
}
