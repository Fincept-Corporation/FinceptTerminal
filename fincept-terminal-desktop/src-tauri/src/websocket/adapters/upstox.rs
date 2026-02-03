// Upstox WebSocket Adapter (V3 API with Protobuf)
//
// Upstox uses protobuf-encoded market data over WebSocket:
// - Authorization: GET https://api.upstox.com/v3/feed/market-data-feed/authorize
// - Returns authorized_redirect_uri for WebSocket connection
// - Messages: Binary protobuf (FeedResponse)
// - Subscription: JSON message with instrument keys

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
use uuid::Uuid;

// ============================================================================
// UPSTOX CONFIGURATION
// ============================================================================

const UPSTOX_AUTH_URL: &str = "https://api.upstox.com/v3/feed/market-data-feed/authorize";

// Subscription modes
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
#[serde(rename_all = "lowercase")]
pub enum UpstoxMode {
    Ltpc,   // LTP + change
    Full,   // Full market depth
}

// ============================================================================
// UPSTOX STRUCTURES
// ============================================================================

#[derive(Debug, Serialize)]
struct SubscriptionMessage {
    guid: String,
    method: String,
    data: SubscriptionData,
}

#[derive(Debug, Serialize)]
struct SubscriptionData {
    #[serde(rename = "instrumentKeys")]
    instrument_keys: Vec<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    mode: Option<String>,
}

#[derive(Debug, Deserialize)]
struct AuthResponse {
    data: Option<AuthData>,
}

#[derive(Debug, Deserialize)]
struct AuthData {
    authorized_redirect_uri: Option<String>,
}

// Simplified protobuf-like structure for tick data
// Note: In production, use proper protobuf parsing with prost
#[derive(Debug, Clone)]
pub struct UpstoxTick {
    pub instrument_key: String,
    pub ltp: f64,
    pub close: f64,
    pub change: f64,
    pub volume: i64,
    pub oi: i64,
    pub timestamp: u64,
}

// ============================================================================
// UPSTOX ADAPTER
// ============================================================================

pub struct UpstoxAdapter {
    config: ProviderConfig,
    access_token: String,
    ws_stream: Arc<RwLock<Option<WebSocketStream<MaybeTlsStream<TcpStream>>>>>,
    message_callback: Option<Box<dyn Fn(MarketMessage) + Send + Sync>>,
    subscriptions: Arc<RwLock<HashMap<String, UpstoxMode>>>,
    is_connected: Arc<RwLock<bool>>,
}

impl UpstoxAdapter {
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

    /// Get authorized WebSocket URL from Upstox API
    async fn get_websocket_url(&self) -> anyhow::Result<String> {
        let client = reqwest::Client::new();
        let response = client
            .get(UPSTOX_AUTH_URL)
            .header("Accept", "application/json")
            .header("Authorization", format!("Bearer {}", self.access_token))
            .send()
            .await?;

        if !response.status().is_success() {
            return Err(anyhow::anyhow!(
                "Failed to get WebSocket URL: {}",
                response.status()
            ));
        }

        let auth_response: AuthResponse = response.json().await?;
        auth_response
            .data
            .and_then(|d| d.authorized_redirect_uri)
            .ok_or_else(|| anyhow::anyhow!("No WebSocket URL in auth response"))
    }

    /// Parse protobuf market data
    /// Note: This is a simplified parser. In production, use prost with proper .proto files
    fn parse_protobuf_data(&self, data: &[u8]) -> Option<Vec<UpstoxTick>> {
        // Upstox uses protobuf FeedResponse format
        // For now, we'll do basic parsing - in production use prost
        if data.len() < 10 {
            return None;
        }

        // Simple protobuf field parsing
        // Field 1 (feeds) is a map of instrument_key -> FeedData
        let ticks = Vec::new();

        // This is a placeholder - actual implementation would use prost
        // to properly decode the FeedResponse protobuf message
        tracing::debug!("Received {} bytes of protobuf data", data.len());

        // For now, return empty - proper implementation needs protobuf definitions
        Some(ticks)
    }

    /// Create subscription message
    fn create_subscription_message(
        &self,
        instrument_keys: Vec<String>,
        mode: Option<UpstoxMode>,
        method: &str,
    ) -> String {
        let guid = Uuid::new_v4().to_string().replace("-", "")[..20].to_string();

        let message = SubscriptionMessage {
            guid,
            method: method.to_string(),
            data: SubscriptionData {
                instrument_keys,
                mode: mode.map(|m| match m {
                    UpstoxMode::Ltpc => "ltpc".to_string(),
                    UpstoxMode::Full => "full".to_string(),
                }),
            },
        };

        serde_json::to_string(&message).unwrap_or_default()
    }

    /// Convert tick to MarketMessage
    fn tick_to_message(&self, tick: UpstoxTick) -> MarketMessage {
        MarketMessage::Ticker(TickerData {
            provider: "upstox".to_string(),
            symbol: tick.instrument_key,
            price: tick.ltp,
            bid: None,
            ask: None,
            bid_size: None,
            ask_size: None,
            volume: Some(tick.volume as f64),
            high: None,
            low: None,
            open: None,
            close: Some(tick.close),
            change: Some(tick.change),
            change_percent: None,
            timestamp: tick.timestamp,
        })
    }

    /// Process incoming binary message
    async fn process_message(&self, data: &[u8]) {
        if let Some(ticks) = self.parse_protobuf_data(data) {
            for tick in ticks {
                let market_message = self.tick_to_message(tick);
                if let Some(ref callback) = self.message_callback {
                    callback(market_message);
                }
            }
        }
    }
}

#[async_trait]
impl WebSocketAdapter for UpstoxAdapter {
    async fn connect(&mut self) -> anyhow::Result<()> {
        // Get authorized WebSocket URL
        let ws_url = self.get_websocket_url().await?;
        tracing::info!("Connecting to Upstox WebSocket: {}", ws_url);

        // Connect to WebSocket
        let (ws_stream, _) = connect_async(&ws_url).await?;
        tracing::info!("Connected to Upstox WebSocket");

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
                            // Process binary protobuf message
                            tracing::debug!("Received binary message: {} bytes", data.len());
                            // Parse and emit via callback
                        }
                        Some(Ok(Message::Text(text))) => {
                            // Process JSON message (subscription confirmations, errors)
                            tracing::debug!("Received text message: {}", text);
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
        if let Some(ref mut ws) = *self.ws_stream.write().await {
            ws.close(None).await?;
        }
        *self.ws_stream.write().await = None;
        *self.is_connected.write().await = false;
        tracing::info!("Disconnected from Upstox WebSocket");
        Ok(())
    }

    async fn subscribe(
        &mut self,
        symbol: &str,
        channel: &str,
        _params: Option<Value>,
    ) -> anyhow::Result<()> {
        let mode = match channel {
            "full" | "depth" => UpstoxMode::Full,
            _ => UpstoxMode::Ltpc,
        };

        // Format instrument key (e.g., NSE_EQ|RELIANCE, NSE_FO|NIFTY23OCTFUT)
        let instrument_key = symbol.to_string();

        let message = self.create_subscription_message(
            vec![instrument_key.clone()],
            Some(mode.clone()),
            "sub",
        );

        if let Some(ref mut ws) = *self.ws_stream.write().await {
            ws.send(Message::Text(message)).await?;
            self.subscriptions
                .write()
                .await
                .insert(instrument_key, mode);
            tracing::info!("Subscribed to {} on channel {}", symbol, channel);
        }

        Ok(())
    }

    async fn unsubscribe(&mut self, symbol: &str, _channel: &str) -> anyhow::Result<()> {
        let instrument_key = symbol.to_string();
        let message = self.create_subscription_message(vec![instrument_key.clone()], None, "unsub");

        if let Some(ref mut ws) = *self.ws_stream.write().await {
            ws.send(Message::Text(message)).await?;
            self.subscriptions.write().await.remove(&instrument_key);
            tracing::info!("Unsubscribed from {}", symbol);
        }

        Ok(())
    }

    fn set_message_callback(&mut self, callback: Box<dyn Fn(MarketMessage) + Send + Sync>) {
        self.message_callback = Some(callback);
    }

    fn provider_name(&self) -> &str {
        "upstox"
    }

    fn is_connected(&self) -> bool {
        // Use try_read to avoid blocking
        self.is_connected
            .try_read()
            .map(|guard| *guard)
            .unwrap_or(false)
    }
}
