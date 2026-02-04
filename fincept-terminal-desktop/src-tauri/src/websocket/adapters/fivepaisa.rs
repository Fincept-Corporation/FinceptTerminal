#![allow(dead_code)]
// 5Paisa WebSocket Adapter
//
// 5Paisa uses WebSocket for real-time market data:
// - WebSocket URL: wss://openfeed.5paisa.com/Feeds/api/chat
// - Authentication via JWT token
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
// 5PAISA CONFIGURATION
// ============================================================================

const FIVEPAISA_WS_URL: &str = "wss://openfeed.5paisa.com/Feeds/api/chat";

// Subscription methods
const METHOD_MARKET_FEED: i32 = 1;
const METHOD_MARKET_DEPTH: i32 = 2;
const METHOD_INDEX_FEED: i32 = 3;
const METHOD_OPEN_INTEREST: i32 = 4;

// ============================================================================
// 5PAISA STRUCTURES
// ============================================================================

#[derive(Debug, Clone, PartialEq)]
pub enum FivePaisaMode {
    MarketFeed,
    MarketDepth,
    IndexFeed,
    OpenInterest,
}

impl FivePaisaMode {
    fn method_code(&self) -> i32 {
        match self {
            FivePaisaMode::MarketFeed => METHOD_MARKET_FEED,
            FivePaisaMode::MarketDepth => METHOD_MARKET_DEPTH,
            FivePaisaMode::IndexFeed => METHOD_INDEX_FEED,
            FivePaisaMode::OpenInterest => METHOD_OPEN_INTEREST,
        }
    }
}

#[derive(Debug, Serialize)]
struct SubscribeRequest {
    #[serde(rename = "Method")]
    method: String,
    #[serde(rename = "Operation")]
    operation: String,
    #[serde(rename = "ClientCode")]
    client_code: String,
    #[serde(rename = "MarketFeedData")]
    market_feed_data: Vec<FivePaisaScrip>,
}

#[derive(Debug, Serialize, Clone)]
struct FivePaisaScrip {
    #[serde(rename = "Exch")]
    exch: String,
    #[serde(rename = "ExchType")]
    exch_type: String,
    #[serde(rename = "ScripCode")]
    scrip_code: i32,
}

#[derive(Debug, Deserialize)]
struct TickResponse {
    #[serde(rename = "Token")]
    token: Option<i32>,
    #[serde(rename = "Exch")]
    exch: Option<String>,
    #[serde(rename = "ExchType")]
    exch_type: Option<String>,
    #[serde(rename = "LastRate")]
    last_rate: Option<f64>,
    #[serde(rename = "TotalQty")]
    total_qty: Option<i64>,
    #[serde(rename = "High")]
    high: Option<f64>,
    #[serde(rename = "Low")]
    low: Option<f64>,
    #[serde(rename = "Open")]
    open: Option<f64>,
    #[serde(rename = "PClose")]
    prev_close: Option<f64>,
    #[serde(rename = "ChgPcnt")]
    change_percent: Option<f64>,
}

#[derive(Debug, Clone)]
pub struct FivePaisaTick {
    pub token: i32,
    pub exchange: String,
    pub exchange_type: String,
    pub ltp: f64,
    pub volume: i64,
    pub high: f64,
    pub low: f64,
    pub open: f64,
    pub prev_close: f64,
    pub change_percent: f64,
}

// ============================================================================
// 5PAISA ADAPTER
// ============================================================================

pub struct FivePaisaAdapter {
    config: ProviderConfig,
    client_code: String,
    jwt_token: String,
    ws_stream: Arc<RwLock<Option<WebSocketStream<MaybeTlsStream<TcpStream>>>>>,
    message_callback: Option<Box<dyn Fn(MarketMessage) + Send + Sync>>,
    subscriptions: Arc<RwLock<HashMap<String, FivePaisaMode>>>,
    is_connected: Arc<RwLock<bool>>,
}

impl FivePaisaAdapter {
    pub fn new(config: ProviderConfig) -> Self {
        let client_code = config.client_id.clone().unwrap_or_default();
        let jwt_token = config.api_key.clone().unwrap_or_default();

        Self {
            config,
            client_code,
            jwt_token,
            ws_stream: Arc::new(RwLock::new(None)),
            message_callback: None,
            subscriptions: Arc::new(RwLock::new(HashMap::new())),
            is_connected: Arc::new(RwLock::new(false)),
        }
    }

    fn build_ws_url(&self) -> String {
        format!("{}?Value={}", FIVEPAISA_WS_URL, self.jwt_token)
    }

    fn parse_tick(&self, data: &str) -> Option<FivePaisaTick> {
        let tick: TickResponse = serde_json::from_str(data).ok()?;

        Some(FivePaisaTick {
            token: tick.token.unwrap_or(0),
            exchange: tick.exch.unwrap_or_default(),
            exchange_type: tick.exch_type.unwrap_or_default(),
            ltp: tick.last_rate.unwrap_or(0.0),
            volume: tick.total_qty.unwrap_or(0),
            high: tick.high.unwrap_or(0.0),
            low: tick.low.unwrap_or(0.0),
            open: tick.open.unwrap_or(0.0),
            prev_close: tick.prev_close.unwrap_or(0.0),
            change_percent: tick.change_percent.unwrap_or(0.0),
        })
    }
}

#[async_trait]
impl WebSocketAdapter for FivePaisaAdapter {
    async fn connect(&mut self) -> anyhow::Result<()> {
        let ws_url = self.build_ws_url();
        tracing::info!("Connecting to 5Paisa WebSocket");

        let (ws_stream, _) = connect_async(&ws_url).await?;
        tracing::info!("Connected to 5Paisa WebSocket");

        *self.ws_stream.write().await = Some(ws_stream);
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
        tracing::info!("Disconnected from 5Paisa WebSocket");
        Ok(())
    }

    async fn subscribe(
        &mut self,
        symbol: &str,
        channel: &str,
        _params: Option<Value>,
    ) -> anyhow::Result<()> {
        let mode = match channel {
            "depth" => FivePaisaMode::MarketDepth,
            "index" => FivePaisaMode::IndexFeed,
            "oi" => FivePaisaMode::OpenInterest,
            _ => FivePaisaMode::MarketFeed,
        };

        // Parse symbol: "N:C:12345" (Exchange:ExchType:ScripCode)
        let parts: Vec<&str> = symbol.split(':').collect();
        if parts.len() != 3 {
            return Err(anyhow::anyhow!("Invalid symbol format. Expected 'Exch:ExchType:ScripCode'"));
        }

        let scrip = FivePaisaScrip {
            exch: parts[0].to_string(),
            exch_type: parts[1].to_string(),
            scrip_code: parts[2].parse().map_err(|_| anyhow::anyhow!("Invalid scrip code"))?,
        };

        let method_name = match mode {
            FivePaisaMode::MarketFeed => "MarketFeedV3",
            FivePaisaMode::MarketDepth => "MarketDepthService",
            FivePaisaMode::IndexFeed => "IndexDetailV2",
            FivePaisaMode::OpenInterest => "OpenInterestFeed",
        };

        let subscribe_req = SubscribeRequest {
            method: method_name.to_string(),
            operation: "Subscribe".to_string(),
            client_code: self.client_code.clone(),
            market_feed_data: vec![scrip],
        };

        if let Some(ref mut ws) = *self.ws_stream.write().await {
            ws.send(Message::Text(serde_json::to_string(&subscribe_req)?)).await?;
            self.subscriptions.write().await.insert(symbol.to_string(), mode);
            tracing::info!("Subscribed to {}", symbol);
        }

        Ok(())
    }

    async fn unsubscribe(&mut self, symbol: &str, _channel: &str) -> anyhow::Result<()> {
        let parts: Vec<&str> = symbol.split(':').collect();
        if parts.len() != 3 {
            return Err(anyhow::anyhow!("Invalid symbol format"));
        }

        let scrip = FivePaisaScrip {
            exch: parts[0].to_string(),
            exch_type: parts[1].to_string(),
            scrip_code: parts[2].parse().map_err(|_| anyhow::anyhow!("Invalid scrip code"))?,
        };

        let unsubscribe_req = SubscribeRequest {
            method: "MarketFeedV3".to_string(),
            operation: "Unsubscribe".to_string(),
            client_code: self.client_code.clone(),
            market_feed_data: vec![scrip],
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
        "fivepaisa"
    }

    fn is_connected(&self) -> bool {
        self.is_connected.try_read().map(|g| *g).unwrap_or(false)
    }
}
