#![allow(dead_code)]
// Motilal Oswal WebSocket Adapter
//
// Motilal Oswal uses WebSocket for market data streaming:
// - WebSocket URL: wss://wsapi.motilaloswal.com
// - Authentication via access token
// - JSON/Binary message format

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
// MOTILAL CONFIGURATION
// ============================================================================

const MOTILAL_WS_URL: &str = "wss://wsapi.motilaloswal.com/";

// ============================================================================
// MOTILAL STRUCTURES
// ============================================================================

#[derive(Debug, Clone, PartialEq)]
pub enum MotilalMode {
    Ltp,
    Quote,
    Depth,
}

#[derive(Debug, Serialize)]
struct AuthMessage {
    #[serde(rename = "MessageType")]
    message_type: String,
    #[serde(rename = "AccessToken")]
    access_token: String,
}

#[derive(Debug, Serialize)]
struct SubscribeMessage {
    #[serde(rename = "MessageType")]
    message_type: String,
    #[serde(rename = "Exchange")]
    exchange: String,
    #[serde(rename = "ScripCode")]
    scrip_code: String,
    #[serde(rename = "Mode")]
    mode: String,
}

#[derive(Debug, Deserialize)]
struct TickResponse {
    #[serde(rename = "Exchange")]
    exchange: Option<String>,
    #[serde(rename = "ScripCode")]
    scrip_code: Option<String>,
    #[serde(rename = "LTP")]
    ltp: Option<f64>,
    #[serde(rename = "Volume")]
    volume: Option<i64>,
    #[serde(rename = "Open")]
    open: Option<f64>,
    #[serde(rename = "High")]
    high: Option<f64>,
    #[serde(rename = "Low")]
    low: Option<f64>,
    #[serde(rename = "Close")]
    close: Option<f64>,
}

#[derive(Debug, Clone)]
pub struct MotilalTick {
    pub exchange: String,
    pub scrip_code: String,
    pub ltp: f64,
    pub volume: i64,
    pub open: f64,
    pub high: f64,
    pub low: f64,
    pub close: f64,
}

// ============================================================================
// MOTILAL ADAPTER
// ============================================================================

pub struct MotilalAdapter {
    config: ProviderConfig,
    access_token: String,
    ws_stream: Arc<RwLock<Option<WebSocketStream<MaybeTlsStream<TcpStream>>>>>,
    message_callback: Option<Box<dyn Fn(MarketMessage) + Send + Sync>>,
    subscriptions: Arc<RwLock<HashMap<String, MotilalMode>>>,
    is_connected: Arc<RwLock<bool>>,
}

impl MotilalAdapter {
    pub fn new(config: ProviderConfig) -> Self {
        let access_token = config.api_key.clone().unwrap_or_default();

        Self {
            config,
            access_token,
            ws_stream: Arc::new(RwLock::new(None)),
            message_callback: None,
            subscriptions: Arc::new(RwLock::new(HashMap::new())),
            is_connected: Arc::new(RwLock::new(false)),
        }
    }

    fn parse_tick(&self, data: &str) -> Option<MotilalTick> {
        let tick: TickResponse = serde_json::from_str(data).ok()?;

        Some(MotilalTick {
            exchange: tick.exchange.unwrap_or_default(),
            scrip_code: tick.scrip_code.unwrap_or_default(),
            ltp: tick.ltp.unwrap_or(0.0),
            volume: tick.volume.unwrap_or(0),
            open: tick.open.unwrap_or(0.0),
            high: tick.high.unwrap_or(0.0),
            low: tick.low.unwrap_or(0.0),
            close: tick.close.unwrap_or(0.0),
        })
    }
}

#[async_trait]
impl WebSocketAdapter for MotilalAdapter {
    async fn connect(&mut self) -> anyhow::Result<()> {
        tracing::info!("Connecting to Motilal Oswal WebSocket");

        let (ws_stream, _) = connect_async(MOTILAL_WS_URL).await?;
        tracing::info!("Connected to Motilal Oswal WebSocket");

        *self.ws_stream.write().await = Some(ws_stream);

        // Send authentication message
        let auth_msg = AuthMessage {
            message_type: "AUTH".to_string(),
            access_token: self.access_token.clone(),
        };

        if let Some(ref mut ws) = *self.ws_stream.write().await {
            ws.send(Message::Text(serde_json::to_string(&auth_msg)?)).await?;
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
                        Some(Ok(Message::Binary(data))) => {
                            tracing::debug!("Received binary: {} bytes", data.len());
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
        tracing::info!("Disconnected from Motilal Oswal WebSocket");
        Ok(())
    }

    async fn subscribe(
        &mut self,
        symbol: &str,
        channel: &str,
        _params: Option<Value>,
    ) -> anyhow::Result<()> {
        let mode = match channel {
            "depth" => MotilalMode::Depth,
            "quote" => MotilalMode::Quote,
            _ => MotilalMode::Ltp,
        };

        // Parse symbol: "NSE:12345"
        let parts: Vec<&str> = symbol.split(':').collect();
        if parts.len() != 2 {
            return Err(anyhow::anyhow!("Invalid symbol format. Expected 'Exchange:ScripCode'"));
        }

        let mode_str = match mode {
            MotilalMode::Ltp => "LTP",
            MotilalMode::Quote => "QUOTE",
            MotilalMode::Depth => "DEPTH",
        };

        let subscribe_msg = SubscribeMessage {
            message_type: "SUBSCRIBE".to_string(),
            exchange: parts[0].to_string(),
            scrip_code: parts[1].to_string(),
            mode: mode_str.to_string(),
        };

        if let Some(ref mut ws) = *self.ws_stream.write().await {
            ws.send(Message::Text(serde_json::to_string(&subscribe_msg)?)).await?;
            self.subscriptions.write().await.insert(symbol.to_string(), mode);
            tracing::info!("Subscribed to {}", symbol);
        }

        Ok(())
    }

    async fn unsubscribe(&mut self, symbol: &str, _channel: &str) -> anyhow::Result<()> {
        let parts: Vec<&str> = symbol.split(':').collect();
        if parts.len() != 2 {
            return Err(anyhow::anyhow!("Invalid symbol format"));
        }

        let unsubscribe_msg = serde_json::json!({
            "MessageType": "UNSUBSCRIBE",
            "Exchange": parts[0],
            "ScripCode": parts[1]
        });

        if let Some(ref mut ws) = *self.ws_stream.write().await {
            ws.send(Message::Text(unsubscribe_msg.to_string())).await?;
            self.subscriptions.write().await.remove(symbol);
            tracing::info!("Unsubscribed from {}", symbol);
        }

        Ok(())
    }

    fn set_message_callback(&mut self, callback: Box<dyn Fn(MarketMessage) + Send + Sync>) {
        self.message_callback = Some(callback);
    }

    fn provider_name(&self) -> &str {
        "motilal"
    }

    fn is_connected(&self) -> bool {
        self.is_connected.try_read().map(|g| *g).unwrap_or(false)
    }
}
