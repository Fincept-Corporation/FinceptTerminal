// Kotak Securities WebSocket Adapter
//
// Kotak uses Neo API WebSocket for market data streaming:
// - WebSocket URL: wss://livefeeds.kotaksecurities.com
// - Authentication via token in connection params
// - Binary message format with specific packet structures

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
use tokio_tungstenite::{connect_async, tungstenite::Message, MaybeTlsStream, WebSocketStream};

// ============================================================================
// KOTAK CONFIGURATION
// ============================================================================

const KOTAK_WS_URL: &str = "wss://livefeeds.kotaksecurities.com";

// Subscription modes
#[derive(Debug, Clone, PartialEq)]
pub enum KotakMode {
    Ltp,
    Quote,
    Depth,
}

// ============================================================================
// KOTAK STRUCTURES
// ============================================================================

#[derive(Debug, Serialize)]
struct KotakSubscription {
    #[serde(rename = "type")]
    msg_type: String,
    scrips: Vec<KotakScrip>,
}

#[derive(Debug, Serialize, Clone)]
struct KotakScrip {
    exchange: String,
    token: String,
}

#[derive(Debug, Clone)]
pub struct KotakTick {
    pub exchange: String,
    pub token: String,
    pub ltp: f64,
    pub volume: i64,
    pub open: f64,
    pub high: f64,
    pub low: f64,
    pub close: f64,
    pub timestamp: u64,
}

// ============================================================================
// KOTAK ADAPTER
// ============================================================================

pub struct KotakAdapter {
    config: ProviderConfig,
    access_token: String,
    consumer_key: String,
    ws_stream: Arc<RwLock<Option<WebSocketStream<MaybeTlsStream<TcpStream>>>>>,
    message_callback: Option<Box<dyn Fn(MarketMessage) + Send + Sync>>,
    subscriptions: Arc<RwLock<HashMap<String, KotakMode>>>,
    is_connected: Arc<RwLock<bool>>,
}

impl KotakAdapter {
    pub fn new(config: ProviderConfig) -> Self {
        let access_token = config.api_key.clone().unwrap_or_default();
        let consumer_key = config.api_secret.clone().unwrap_or_default();

        Self {
            config,
            access_token,
            consumer_key,
            ws_stream: Arc::new(RwLock::new(None)),
            message_callback: None,
            subscriptions: Arc::new(RwLock::new(HashMap::new())),
            is_connected: Arc::new(RwLock::new(false)),
        }
    }

    fn build_ws_url(&self) -> String {
        format!(
            "{}?Authorization=Bearer {}&consumerKey={}",
            KOTAK_WS_URL, self.access_token, self.consumer_key
        )
    }

    fn parse_binary_data(&self, data: &[u8]) -> Option<KotakTick> {
        if data.len() < 40 {
            return None;
        }

        // Kotak binary format parsing
        // This is a simplified implementation
        Some(KotakTick {
            exchange: "NSE".to_string(),
            token: "0".to_string(),
            ltp: 0.0,
            volume: 0,
            open: 0.0,
            high: 0.0,
            low: 0.0,
            close: 0.0,
            timestamp: 0,
        })
    }
}

#[async_trait]
impl WebSocketAdapter for KotakAdapter {
    async fn connect(&mut self) -> anyhow::Result<()> {
        let ws_url = self.build_ws_url();
        tracing::info!("Connecting to Kotak WebSocket");

        let (ws_stream, _) = connect_async(&ws_url).await?;
        tracing::info!("Connected to Kotak WebSocket");

        *self.ws_stream.write().await = Some(ws_stream);
        *self.is_connected.write().await = true;

        let ws_stream = self.ws_stream.clone();
        let is_connected = self.is_connected.clone();

        tokio::spawn(async move {
            loop {
                let mut stream = ws_stream.write().await;
                if let Some(ref mut ws) = *stream {
                    match ws.next().await {
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
        tracing::info!("Disconnected from Kotak WebSocket");
        Ok(())
    }

    async fn subscribe(
        &mut self,
        symbol: &str,
        channel: &str,
        _params: Option<Value>,
    ) -> anyhow::Result<()> {
        let mode = match channel {
            "depth" => KotakMode::Depth,
            "quote" => KotakMode::Quote,
            _ => KotakMode::Ltp,
        };

        let parts: Vec<&str> = symbol.split(':').collect();
        if parts.len() != 2 {
            return Err(anyhow::anyhow!("Invalid symbol format"));
        }

        let subscription = KotakSubscription {
            msg_type: "subscribe".to_string(),
            scrips: vec![KotakScrip {
                exchange: parts[0].to_string(),
                token: parts[1].to_string(),
            }],
        };

        if let Some(ref mut ws) = *self.ws_stream.write().await {
            ws.send(Message::Text(serde_json::to_string(&subscription)?)).await?;
            self.subscriptions.write().await.insert(symbol.to_string(), mode);
            tracing::info!("Subscribed to {}", symbol);
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
        "kotak"
    }

    fn is_connected(&self) -> bool {
        self.is_connected.try_read().map(|g| *g).unwrap_or(false)
    }
}
