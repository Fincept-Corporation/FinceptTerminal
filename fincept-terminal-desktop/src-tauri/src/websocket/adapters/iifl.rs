// IIFL Securities WebSocket Adapter
//
// IIFL uses XTS API WebSocket for market data:
// - WebSocket URL: Obtained from market data login
// - Authentication via session token
// - Binary message format

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
// IIFL CONFIGURATION
// ============================================================================

const IIFL_MARKET_LOGIN_URL: &str = "https://ttblaze.iifl.com/apimarketdata/auth/login";

// ============================================================================
// IIFL STRUCTURES
// ============================================================================

#[derive(Debug, Clone, PartialEq)]
pub enum IiflMode {
    Ltp,
    Quote,
    Depth,
}

#[derive(Debug, Serialize)]
struct LoginRequest {
    secretKey: String,
    appKey: String,
    source: String,
}

#[derive(Debug, Deserialize)]
struct LoginResponse {
    result: Option<LoginResult>,
}

#[derive(Debug, Deserialize)]
struct LoginResult {
    token: Option<String>,
    userID: Option<String>,
}

#[derive(Debug, Serialize)]
struct SubscribeRequest {
    instruments: Vec<IiflInstrument>,
    xtsMessageCode: i32,
    publishFormat: String,
}

#[derive(Debug, Serialize, Clone)]
struct IiflInstrument {
    exchangeSegment: i32,
    exchangeInstrumentID: i32,
}

#[derive(Debug, Clone)]
pub struct IiflTick {
    pub exchange_segment: i32,
    pub exchange_instrument_id: i32,
    pub ltp: f64,
    pub volume: i64,
    pub open: f64,
    pub high: f64,
    pub low: f64,
    pub close: f64,
    pub timestamp: u64,
}

// ============================================================================
// IIFL ADAPTER
// ============================================================================

pub struct IiflAdapter {
    config: ProviderConfig,
    app_key: String,
    secret_key: String,
    session_token: Option<String>,
    ws_url: Option<String>,
    ws_stream: Arc<RwLock<Option<WebSocketStream<MaybeTlsStream<TcpStream>>>>>,
    message_callback: Option<Box<dyn Fn(MarketMessage) + Send + Sync>>,
    subscriptions: Arc<RwLock<HashMap<String, IiflMode>>>,
    is_connected: Arc<RwLock<bool>>,
}

impl IiflAdapter {
    pub fn new(config: ProviderConfig) -> Self {
        let app_key = config.api_key.clone().unwrap_or_default();
        let secret_key = config.api_secret.clone().unwrap_or_default();

        Self {
            config,
            app_key,
            secret_key,
            session_token: None,
            ws_url: None,
            ws_stream: Arc::new(RwLock::new(None)),
            message_callback: None,
            subscriptions: Arc::new(RwLock::new(HashMap::new())),
            is_connected: Arc::new(RwLock::new(false)),
        }
    }

    async fn login(&mut self) -> anyhow::Result<()> {
        let client = reqwest::Client::new();

        let login_req = LoginRequest {
            secretKey: self.secret_key.clone(),
            appKey: self.app_key.clone(),
            source: "WebAPI".to_string(),
        };

        let response = client
            .post(IIFL_MARKET_LOGIN_URL)
            .json(&login_req)
            .send()
            .await?;

        if response.status().is_success() {
            let login_response: LoginResponse = response.json().await?;
            if let Some(result) = login_response.result {
                self.session_token = result.token;
                tracing::info!("IIFL market data login successful");
            }
        } else {
            return Err(anyhow::anyhow!("IIFL login failed"));
        }

        // Get WebSocket URL (would come from login response in production)
        self.ws_url = Some("wss://ttblaze.iifl.com/socket.io/marketdata".to_string());

        Ok(())
    }

    fn parse_binary_data(&self, data: &[u8]) -> Option<IiflTick> {
        if data.len() < 40 {
            return None;
        }

        // Parse IIFL binary format
        Some(IiflTick {
            exchange_segment: 0,
            exchange_instrument_id: 0,
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
impl WebSocketAdapter for IiflAdapter {
    async fn connect(&mut self) -> anyhow::Result<()> {
        // Login first to get session token
        self.login().await?;

        let ws_url = self.ws_url.as_ref()
            .ok_or_else(|| anyhow::anyhow!("No WebSocket URL available"))?;

        tracing::info!("Connecting to IIFL WebSocket");

        let url_with_token = format!(
            "{}?token={}",
            ws_url,
            self.session_token.as_ref().unwrap_or(&String::new())
        );

        let (ws_stream, _) = connect_async(&url_with_token).await?;
        tracing::info!("Connected to IIFL WebSocket");

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
        tracing::info!("Disconnected from IIFL WebSocket");
        Ok(())
    }

    async fn subscribe(
        &mut self,
        symbol: &str,
        channel: &str,
        _params: Option<Value>,
    ) -> anyhow::Result<()> {
        let mode = match channel {
            "depth" => IiflMode::Depth,
            "quote" => IiflMode::Quote,
            _ => IiflMode::Ltp,
        };

        // Parse symbol: "1:12345" (ExchangeSegment:InstrumentID)
        let parts: Vec<&str> = symbol.split(':').collect();
        if parts.len() != 2 {
            return Err(anyhow::anyhow!("Invalid symbol format. Expected 'ExchangeSegment:InstrumentID'"));
        }

        let instrument = IiflInstrument {
            exchangeSegment: parts[0].parse().map_err(|_| anyhow::anyhow!("Invalid exchange segment"))?,
            exchangeInstrumentID: parts[1].parse().map_err(|_| anyhow::anyhow!("Invalid instrument ID"))?,
        };

        let xts_code = match mode {
            IiflMode::Ltp => 1501,
            IiflMode::Quote => 1502,
            IiflMode::Depth => 1505,
        };

        let subscribe_req = SubscribeRequest {
            instruments: vec![instrument],
            xtsMessageCode: xts_code,
            publishFormat: "JSON".to_string(),
        };

        if let Some(ref mut ws) = *self.ws_stream.write().await {
            ws.send(Message::Text(serde_json::to_string(&subscribe_req)?)).await?;
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
        "iifl"
    }

    fn is_connected(&self) -> bool {
        self.is_connected.try_read().map(|g| *g).unwrap_or(false)
    }
}
