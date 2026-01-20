// Groww WebSocket Adapter (NATS Protocol)
//
// Groww uses NATS protocol over WebSocket:
// - WebSocket URL: wss://socket-api.groww.in
// - Requires socket token from API: POST /v1/api/apex/v1/socket/token/create/
// - NATS protocol with nkey authentication
// - Protobuf-encoded market data

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
// GROWW CONFIGURATION
// ============================================================================

const GROWW_WS_URL: &str = "wss://socket-api.groww.in";
const GROWW_TOKEN_URL: &str = "https://api.groww.in/v1/api/apex/v1/socket/token/create/";

// ============================================================================
// GROWW STRUCTURES
// ============================================================================

#[derive(Debug, Clone, PartialEq)]
pub enum GrowwMode {
    Ltp,
    Depth,
}

#[derive(Debug, Serialize)]
struct TokenRequest {
    #[serde(rename = "socketKey")]
    socket_key: String,
}

#[derive(Debug, Deserialize)]
struct TokenResponse {
    token: Option<String>,
    #[serde(rename = "subscriptionId")]
    subscription_id: Option<String>,
}

#[derive(Debug, Clone)]
pub struct GrowwTick {
    pub symbol: String,
    pub exchange: String,
    pub ltp: f64,
    pub change: f64,
    pub change_percent: f64,
    pub volume: i64,
    pub timestamp: u64,
}

// ============================================================================
// GROWW ADAPTER
// ============================================================================

pub struct GrowwAdapter {
    config: ProviderConfig,
    auth_token: String,
    socket_token: Option<String>,
    subscription_id: Option<String>,
    ws_stream: Arc<RwLock<Option<WebSocketStream<MaybeTlsStream<TcpStream>>>>>,
    message_callback: Option<Box<dyn Fn(MarketMessage) + Send + Sync>>,
    subscriptions: Arc<RwLock<HashMap<String, GrowwMode>>>,
    is_connected: Arc<RwLock<bool>>,
    nats_sid_counter: Arc<RwLock<u32>>,
}

impl GrowwAdapter {
    pub fn new(config: ProviderConfig) -> Self {
        let auth_token = config.api_key.clone().unwrap_or_default();

        Self {
            config,
            auth_token,
            socket_token: None,
            subscription_id: None,
            ws_stream: Arc::new(RwLock::new(None)),
            message_callback: None,
            subscriptions: Arc::new(RwLock::new(HashMap::new())),
            is_connected: Arc::new(RwLock::new(false)),
            nats_sid_counter: Arc::new(RwLock::new(1)),
        }
    }

    /// Generate socket token from Groww API
    async fn generate_socket_token(&mut self) -> anyhow::Result<()> {
        let client = reqwest::Client::new();

        // Generate a simple socket key (in production, use proper nkey)
        let socket_key = format!("UFINCEPT{}", uuid::Uuid::new_v4().to_string().replace("-", "")[..16].to_string());

        let response = client
            .post(GROWW_TOKEN_URL)
            .header("Authorization", format!("Bearer {}", self.auth_token))
            .header("Content-Type", "application/json")
            .header("x-client-id", "growwapi")
            .json(&TokenRequest { socket_key })
            .send()
            .await?;

        if response.status().is_success() {
            let token_response: TokenResponse = response.json().await?;
            self.socket_token = token_response.token;
            self.subscription_id = token_response.subscription_id;
            tracing::info!("Generated Groww socket token");
        } else {
            // Fallback to using auth token directly
            tracing::warn!("Failed to generate socket token, using auth token as fallback");
            self.socket_token = Some(self.auth_token.clone());
            self.subscription_id = Some("direct_auth".to_string());
        }

        Ok(())
    }

    /// Create NATS CONNECT message
    fn create_nats_connect(&self) -> String {
        let connect_opts = serde_json::json!({
            "jwt": self.socket_token.as_ref().unwrap_or(&self.auth_token),
            "lang": "rust",
            "version": "1.0.0",
            "protocol": 1,
            "verbose": false,
            "pedantic": false
        });
        format!("CONNECT {}\r\n", connect_opts)
    }

    /// Create NATS SUB message
    fn create_nats_subscribe(&self, topic: &str, sid: u32) -> String {
        format!("SUB {} {}\r\n", topic, sid)
    }

    /// Create NATS UNSUB message
    fn create_nats_unsubscribe(&self, sid: u32) -> String {
        format!("UNSUB {}\r\n", sid)
    }

    /// Format topic for Groww
    fn format_topic(&self, exchange: &str, segment: &str, token: &str, mode: &GrowwMode) -> String {
        let mode_str = match mode {
            GrowwMode::Ltp => "price",
            GrowwMode::Depth => "book",
        };

        // Format: /ld/eq/nse/price.{token} or /ld/fo/nse/book.{token}
        let segment_code = match segment.to_uppercase().as_str() {
            "FNO" | "FO" => "fo",
            _ => "eq",
        };

        format!("/ld/{}/{}/{}.{}", segment_code, exchange.to_lowercase(), mode_str, token)
    }

    /// Parse protobuf market data (simplified)
    fn parse_protobuf_data(&self, data: &[u8]) -> Option<GrowwTick> {
        // Simplified protobuf parsing
        // In production, use proper protobuf definitions with prost
        if data.len() < 10 {
            return None;
        }

        Some(GrowwTick {
            symbol: String::new(),
            exchange: String::new(),
            ltp: 0.0,
            change: 0.0,
            change_percent: 0.0,
            volume: 0,
            timestamp: 0,
        })
    }
}

#[async_trait]
impl WebSocketAdapter for GrowwAdapter {
    async fn connect(&mut self) -> anyhow::Result<()> {
        // Generate socket token first
        self.generate_socket_token().await?;

        tracing::info!("Connecting to Groww WebSocket (NATS)");

        let (ws_stream, _) = connect_async(GROWW_WS_URL).await?;
        tracing::info!("Connected to Groww WebSocket");

        *self.ws_stream.write().await = Some(ws_stream);

        // Send NATS CONNECT
        if let Some(ref mut ws) = *self.ws_stream.write().await {
            let connect_msg = self.create_nats_connect();
            ws.send(Message::Text(connect_msg)).await?;
            ws.send(Message::Text("PING\r\n".to_string())).await?;
        }

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
                        Some(Ok(Message::Text(text))) => {
                            if text.starts_with("PING") {
                                let _ = ws.send(Message::Text("PONG\r\n".to_string())).await;
                            } else if text.starts_with("INFO") {
                                tracing::debug!("Received NATS INFO");
                            } else if text.starts_with("+OK") {
                                tracing::debug!("Received NATS +OK");
                            }
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
        tracing::info!("Disconnected from Groww WebSocket");
        Ok(())
    }

    async fn subscribe(
        &mut self,
        symbol: &str,
        channel: &str,
        _params: Option<Value>,
    ) -> anyhow::Result<()> {
        let mode = match channel {
            "depth" => GrowwMode::Depth,
            _ => GrowwMode::Ltp,
        };

        // Parse symbol: "NSE:CASH:1594" or "NSE:FNO:12345"
        let parts: Vec<&str> = symbol.split(':').collect();
        if parts.len() < 3 {
            return Err(anyhow::anyhow!("Invalid symbol format. Expected 'Exchange:Segment:Token'"));
        }

        let exchange = parts[0];
        let segment = parts[1];
        let token = parts[2];

        let topic = self.format_topic(exchange, segment, token, &mode);

        let mut sid_counter = self.nats_sid_counter.write().await;
        let sid = *sid_counter;
        *sid_counter += 1;
        drop(sid_counter);

        let sub_msg = self.create_nats_subscribe(&topic, sid);

        if let Some(ref mut ws) = *self.ws_stream.write().await {
            ws.send(Message::Text(sub_msg)).await?;
            ws.send(Message::Text("PING\r\n".to_string())).await?;
            self.subscriptions.write().await.insert(symbol.to_string(), mode);
            tracing::info!("Subscribed to {} (topic: {})", symbol, topic);
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
        "groww"
    }

    fn is_connected(&self) -> bool {
        self.is_connected.try_read().map(|g| *g).unwrap_or(false)
    }
}
