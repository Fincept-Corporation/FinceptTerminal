// Zerodha KiteTicker WebSocket Adapter
//
// Implements Zerodha's KiteTicker WebSocket API for real-time market data
// Binary protocol for high-performance data streaming
// Supports: LTP, Quote, Full depth modes

use super::WebSocketAdapter;
use crate::websocket::types::*;
use async_trait::async_trait;
use futures_util::{SinkExt, StreamExt};
use serde_json::{json, Value};
use std::collections::HashMap;
use std::sync::atomic::{AtomicBool, AtomicU64, Ordering};
use std::sync::Arc;
use std::time::{Duration, SystemTime, UNIX_EPOCH};
use tokio::sync::RwLock;
use tokio_tungstenite::{connect_async, tungstenite::Message};

// Zerodha WebSocket URL format
const ZERODHA_WS_BASE_URL: &str = "wss://ws.kite.trade";

// Subscription modes
const MODE_LTP: &str = "ltp";
const MODE_QUOTE: &str = "quote";
const MODE_FULL: &str = "full";

// Packet sizes for different modes
const PACKET_SIZE_LTP: usize = 8;
const PACKET_SIZE_QUOTE: usize = 44;
const PACKET_SIZE_FULL: usize = 184;

// Batching settings
const MAX_TOKENS_PER_SUBSCRIBE: usize = 200;
const SUBSCRIPTION_DELAY_MS: u64 = 2000;

/// Callback type for connection errors (used to trigger reconnection)
type ErrorCallback = Box<dyn Fn(String) + Send + Sync>;

pub struct ZerodhaAdapter {
    config: ProviderConfig,
    api_key: String,
    access_token: String,
    ws: Option<Arc<RwLock<tokio_tungstenite::WebSocketStream<tokio_tungstenite::MaybeTlsStream<tokio::net::TcpStream>>>>>,
    message_callback: Option<Arc<Box<dyn Fn(MarketMessage) + Send + Sync>>>,
    error_callback: Option<Arc<ErrorCallback>>,
    connected: Arc<AtomicBool>,
    last_message_time: Arc<RwLock<u64>>,
    // Token tracking
    subscribed_tokens: Arc<RwLock<HashMap<i64, String>>>, // token -> mode
    token_symbol_map: Arc<RwLock<HashMap<i64, (String, String)>>>, // token -> (symbol, exchange)
    // Statistics
    message_count: Arc<AtomicU64>,
    tick_count: Arc<AtomicU64>,
}

impl ZerodhaAdapter {
    pub fn new(config: ProviderConfig) -> Self {
        let api_key = config.api_key.clone().unwrap_or_default();
        let access_token = config.api_secret.clone().unwrap_or_default(); // access_token stored in api_secret

        Self {
            config,
            api_key,
            access_token,
            ws: None,
            message_callback: None,
            error_callback: None,
            connected: Arc::new(AtomicBool::new(false)),
            last_message_time: Arc::new(RwLock::new(0)),
            subscribed_tokens: Arc::new(RwLock::new(HashMap::new())),
            token_symbol_map: Arc::new(RwLock::new(HashMap::new())),
            message_count: Arc::new(AtomicU64::new(0)),
            tick_count: Arc::new(AtomicU64::new(0)),
        }
    }

    /// Create with explicit credentials
    pub fn new_with_credentials(api_key: String, access_token: String) -> Self {
        let config = ProviderConfig {
            name: "zerodha".to_string(),
            url: ZERODHA_WS_BASE_URL.to_string(),
            api_key: Some(api_key.clone()),
            api_secret: Some(access_token.clone()),
            enabled: true,
            ..Default::default()
        };

        Self {
            config,
            api_key,
            access_token,
            ws: None,
            message_callback: None,
            error_callback: None,
            connected: Arc::new(AtomicBool::new(false)),
            last_message_time: Arc::new(RwLock::new(0)),
            subscribed_tokens: Arc::new(RwLock::new(HashMap::new())),
            token_symbol_map: Arc::new(RwLock::new(HashMap::new())),
            message_count: Arc::new(AtomicU64::new(0)),
            tick_count: Arc::new(AtomicU64::new(0)),
        }
    }

    fn now() -> u64 {
        SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .unwrap_or_else(|_| Duration::from_secs(0))
            .as_millis() as u64
    }

    /// Set error callback for connection errors
    pub fn set_error_callback(&mut self, callback: ErrorCallback) {
        self.error_callback = Some(Arc::new(callback));
    }

    /// Set token to symbol mapping
    pub fn set_token_symbol_map(&self, token: i64, symbol: String, exchange: String) {
        if let Ok(mut map) = self.token_symbol_map.try_write() {
            map.insert(token, (symbol, exchange));
        }
    }

    /// Build WebSocket URL with authentication
    fn build_ws_url(&self) -> String {
        format!(
            "{}?api_key={}&access_token={}",
            ZERODHA_WS_BASE_URL, self.api_key, self.access_token
        )
    }

    /// Subscribe to tokens with a specific mode
    pub async fn subscribe_tokens(&self, tokens: Vec<i64>, mode: &str) -> anyhow::Result<()> {
        if !self.connected.load(Ordering::SeqCst) {
            return Err(anyhow::anyhow!("Not connected"));
        }

        let ws = self.ws.as_ref().ok_or_else(|| anyhow::anyhow!("WebSocket not initialized"))?;

        // Process in batches
        for chunk in tokens.chunks(MAX_TOKENS_PER_SUBSCRIBE) {
            let chunk_vec: Vec<i64> = chunk.to_vec();

            // Send subscribe message
            let sub_msg = json!({
                "a": "subscribe",
                "v": chunk_vec
            });

            {
                let mut ws_lock = ws.write().await;
                ws_lock.send(Message::Text(sub_msg.to_string())).await?;
            }

            eprintln!("[Zerodha] Subscribed to {} tokens", chunk_vec.len());

            // Wait a bit for subscription to process
            tokio::time::sleep(Duration::from_secs(1)).await;

            // Set mode
            let mode_msg = json!({
                "a": "mode",
                "v": [mode, chunk_vec]
            });

            {
                let mut ws_lock = ws.write().await;
                ws_lock.send(Message::Text(mode_msg.to_string())).await?;
            }

            eprintln!("[Zerodha] Set mode {} for {} tokens", mode, chunk_vec.len());

            // Update subscribed tokens
            {
                let mut subscribed = self.subscribed_tokens.write().await;
                for token in chunk_vec {
                    subscribed.insert(token, mode.to_string());
                }
            }

            // Delay between batches
            if chunk.len() == MAX_TOKENS_PER_SUBSCRIBE {
                tokio::time::sleep(Duration::from_millis(SUBSCRIPTION_DELAY_MS)).await;
            }
        }

        Ok(())
    }

    /// Unsubscribe from tokens
    pub async fn unsubscribe_tokens(&self, tokens: Vec<i64>) -> anyhow::Result<()> {
        if !self.connected.load(Ordering::SeqCst) {
            return Err(anyhow::anyhow!("Not connected"));
        }

        let ws = self.ws.as_ref().ok_or_else(|| anyhow::anyhow!("WebSocket not initialized"))?;

        let unsub_msg = json!({
            "a": "unsubscribe",
            "v": tokens
        });

        {
            let mut ws_lock = ws.write().await;
            ws_lock.send(Message::Text(unsub_msg.to_string())).await?;
        }

        // Update subscribed tokens
        {
            let mut subscribed = self.subscribed_tokens.write().await;
            for token in &tokens {
                subscribed.remove(token);
            }
        }

        eprintln!("[Zerodha] Unsubscribed from {} tokens", tokens.len());

        Ok(())
    }

    /// Message receiver loop
    async fn receive_loop(
        ws: Arc<RwLock<tokio_tungstenite::WebSocketStream<tokio_tungstenite::MaybeTlsStream<tokio::net::TcpStream>>>>,
        callback: Arc<Box<dyn Fn(MarketMessage) + Send + Sync>>,
        error_callback: Option<Arc<ErrorCallback>>,
        connected: Arc<AtomicBool>,
        last_message_time: Arc<RwLock<u64>>,
        token_symbol_map: Arc<RwLock<HashMap<i64, (String, String)>>>,
        message_count: Arc<AtomicU64>,
        tick_count: Arc<AtomicU64>,
    ) {
        loop {
            let msg_result = {
                let mut ws_lock = ws.write().await;
                ws_lock.next().await
            };

            match msg_result {
                Some(Ok(msg)) => {
                    // Update last message time
                    *last_message_time.write().await = Self::now();
                    message_count.fetch_add(1, Ordering::Relaxed);

                    match msg {
                        Message::Binary(data) => {
                            // Handle binary market data
                            if data.len() == 1 {
                                // Zerodha heartbeat - 1 byte message
                                continue;
                            }

                            // Parse binary data
                            let ticks = Self::parse_binary_message(&data, &token_symbol_map).await;
                            tick_count.fetch_add(ticks.len() as u64, Ordering::Relaxed);

                            for tick in ticks {
                                callback(MarketMessage::Ticker(tick));
                            }
                        }
                        Message::Text(text) => {
                            // Handle JSON messages (errors, order updates)
                            if let Ok(data) = serde_json::from_str::<Value>(&text) {
                                if let Some(msg_type) = data.get("type").and_then(|t| t.as_str()) {
                                    if msg_type == "error" {
                                        let error_msg = data.get("data")
                                            .and_then(|d| d.as_str())
                                            .unwrap_or("Unknown error")
                                            .to_string();
                                        eprintln!("[Zerodha] Error: {}", error_msg);

                                        callback(MarketMessage::Status(StatusData {
                                            provider: "zerodha".to_string(),
                                            status: ConnectionStatus::Error,
                                            message: Some(error_msg),
                                            timestamp: Self::now(),
                                        }));
                                    }
                                }
                            }
                        }
                        Message::Ping(data) => {
                            // Respond to ping with pong
                            let mut ws_lock = ws.write().await;
                            let _ = ws_lock.send(Message::Pong(data)).await;
                        }
                        Message::Close(_) => {
                            eprintln!("[Zerodha] Connection closed by server");
                            break;
                        }
                        _ => {}
                    }
                }
                Some(Err(e)) => {
                    let error_msg = format!("WebSocket error: {}", e);
                    eprintln!("[Zerodha] {}", error_msg);
                    connected.store(false, Ordering::SeqCst);

                    callback(MarketMessage::Status(StatusData {
                        provider: "zerodha".to_string(),
                        status: ConnectionStatus::Error,
                        message: Some(error_msg.clone()),
                        timestamp: Self::now(),
                    }));

                    if let Some(err_cb) = &error_callback {
                        err_cb(error_msg);
                    }
                    break;
                }
                None => {
                    let error_msg = "WebSocket connection closed unexpectedly".to_string();
                    eprintln!("[Zerodha] {}", error_msg);
                    connected.store(false, Ordering::SeqCst);

                    callback(MarketMessage::Status(StatusData {
                        provider: "zerodha".to_string(),
                        status: ConnectionStatus::Disconnected,
                        message: Some(error_msg.clone()),
                        timestamp: Self::now(),
                    }));

                    if let Some(err_cb) = &error_callback {
                        err_cb(error_msg);
                    }
                    break;
                }
            }
        }
    }

    /// Parse binary message according to Zerodha specification
    async fn parse_binary_message(
        data: &[u8],
        token_symbol_map: &Arc<RwLock<HashMap<i64, (String, String)>>>,
    ) -> Vec<TickerData> {
        let mut ticks = Vec::new();

        if data.len() < 4 {
            return ticks;
        }

        // Parse header: first 2 bytes = number of packets
        let num_packets = u16::from_be_bytes([data[0], data[1]]) as usize;
        let mut offset = 2;

        for _ in 0..num_packets {
            if offset + 2 > data.len() {
                break;
            }

            // Next 2 bytes: packet length
            let packet_length = u16::from_be_bytes([data[offset], data[offset + 1]]) as usize;
            offset += 2;

            if offset + packet_length > data.len() {
                break;
            }

            // Extract and parse packet
            let packet_data = &data[offset..offset + packet_length];
            if let Some(tick) = Self::parse_packet(packet_data, token_symbol_map).await {
                ticks.push(tick);
            }

            offset += packet_length;
        }

        ticks
    }

    /// Parse individual packet
    async fn parse_packet(
        packet: &[u8],
        token_symbol_map: &Arc<RwLock<HashMap<i64, (String, String)>>>,
    ) -> Option<TickerData> {
        if packet.len() < PACKET_SIZE_LTP {
            return None;
        }

        // Extract instrument token (first 4 bytes, big-endian)
        let instrument_token = i32::from_be_bytes([packet[0], packet[1], packet[2], packet[3]]) as i64;

        // Extract last price (next 4 bytes, big-endian, in paise)
        let last_price_paise = i32::from_be_bytes([packet[4], packet[5], packet[6], packet[7]]);
        let last_price = last_price_paise as f64 / 100.0;

        // Get symbol and exchange from map
        let (symbol, exchange) = {
            let map = token_symbol_map.read().await;
            map.get(&instrument_token)
                .cloned()
                .unwrap_or_else(|| (instrument_token.to_string(), "NSE".to_string()))
        };

        let mut tick = TickerData {
            provider: "zerodha".to_string(),
            symbol: format!("{}:{}", exchange, symbol),
            price: last_price,
            bid: None,
            ask: None,
            bid_size: None,
            ask_size: None,
            volume: None,
            high: None,
            low: None,
            open: None,
            close: None,
            change: None,
            change_percent: None,
            timestamp: Self::now(),
        };

        // Parse Quote mode fields (44 bytes)
        if packet.len() >= PACKET_SIZE_QUOTE {
            // Fields: token(4), ltp(4), qty(4), avg(4), vol(4), buy_qty(4), sell_qty(4), open(4), high(4), low(4), close(4)
            let last_qty = i32::from_be_bytes([packet[8], packet[9], packet[10], packet[11]]);
            let avg_price = i32::from_be_bytes([packet[12], packet[13], packet[14], packet[15]]) as f64 / 100.0;
            let volume = i32::from_be_bytes([packet[16], packet[17], packet[18], packet[19]]) as u64;
            let buy_qty = i32::from_be_bytes([packet[20], packet[21], packet[22], packet[23]]);
            let sell_qty = i32::from_be_bytes([packet[24], packet[25], packet[26], packet[27]]);
            let open = i32::from_be_bytes([packet[28], packet[29], packet[30], packet[31]]) as f64 / 100.0;
            let high = i32::from_be_bytes([packet[32], packet[33], packet[34], packet[35]]) as f64 / 100.0;
            let low = i32::from_be_bytes([packet[36], packet[37], packet[38], packet[39]]) as f64 / 100.0;
            let close = i32::from_be_bytes([packet[40], packet[41], packet[42], packet[43]]) as f64 / 100.0;

            tick.volume = Some(volume as f64);
            tick.open = Some(open);
            tick.high = Some(high);
            tick.low = Some(low);
            tick.close = Some(close);
            tick.bid_size = Some(buy_qty as f64);
            tick.ask_size = Some(sell_qty as f64);

            // Calculate change
            if close > 0.0 {
                tick.change = Some(last_price - close);
                tick.change_percent = Some(((last_price - close) / close) * 100.0);
            }
        }

        // Parse Full mode fields (184+ bytes) - includes depth
        if packet.len() >= PACKET_SIZE_FULL {
            // Additional fields at offset 44+
            // last_trade_time(4), oi(4), oi_day_high(4), oi_day_low(4), exchange_timestamp(4)
            // Then market depth: 5 buy + 5 sell levels (each: qty(4), price(4), orders(2))

            // For now, we'll just note that depth is available
            // Full depth parsing can be added if needed
        }

        Some(tick)
    }

    /// Get connection statistics
    pub fn get_statistics(&self) -> (u64, u64, usize) {
        let msg_count = self.message_count.load(Ordering::Relaxed);
        let tick_count = self.tick_count.load(Ordering::Relaxed);
        let sub_count = self.subscribed_tokens.try_read()
            .map(|t| t.len())
            .unwrap_or(0);
        (msg_count, tick_count, sub_count)
    }
}

#[async_trait]
impl WebSocketAdapter for ZerodhaAdapter {
    async fn connect(&mut self) -> anyhow::Result<()> {
        if self.connected.load(Ordering::SeqCst) {
            return Ok(());
        }

        let url = self.build_ws_url();
        eprintln!("[Zerodha] Connecting to WebSocket...");

        // Connect with timeout
        let (ws_stream, _response) = tokio::time::timeout(
            Duration::from_secs(10),
            connect_async(&url),
        )
        .await
        .map_err(|_| anyhow::anyhow!("Connection timeout"))?
        .map_err(|e| anyhow::anyhow!("Connection failed: {}", e))?;

        eprintln!("[Zerodha] WebSocket connected successfully");

        let ws = Arc::new(RwLock::new(ws_stream));
        self.ws = Some(ws.clone());
        self.connected.store(true, Ordering::SeqCst);
        *self.last_message_time.write().await = Self::now();

        // Emit connected status
        if let Some(ref callback) = self.message_callback {
            callback(MarketMessage::Status(StatusData {
                provider: "zerodha".to_string(),
                status: ConnectionStatus::Connected,
                message: Some("Connected to Zerodha KiteTicker".to_string()),
                timestamp: Self::now(),
            }));
        }

        // Start receive loop
        if let Some(callback) = &self.message_callback {
            let callback = callback.clone();
            let error_callback = self.error_callback.clone();
            let connected = self.connected.clone();
            let last_message_time = self.last_message_time.clone();
            let token_symbol_map = self.token_symbol_map.clone();
            let message_count = self.message_count.clone();
            let tick_count = self.tick_count.clone();

            tokio::spawn(async move {
                Self::receive_loop(
                    ws,
                    callback,
                    error_callback,
                    connected,
                    last_message_time,
                    token_symbol_map,
                    message_count,
                    tick_count,
                )
                .await;
            });
        }

        Ok(())
    }

    async fn disconnect(&mut self) -> anyhow::Result<()> {
        self.connected.store(false, Ordering::SeqCst);

        if let Some(ws) = self.ws.take() {
            let mut ws_lock = ws.write().await;
            let _ = ws_lock.close(None).await;
        }

        // Emit disconnected status
        if let Some(ref callback) = self.message_callback {
            callback(MarketMessage::Status(StatusData {
                provider: "zerodha".to_string(),
                status: ConnectionStatus::Disconnected,
                message: Some("Disconnected from Zerodha".to_string()),
                timestamp: Self::now(),
            }));
        }

        eprintln!("[Zerodha] WebSocket disconnected");
        Ok(())
    }

    async fn subscribe(
        &mut self,
        symbol: &str,
        channel: &str,
        params: Option<Value>,
    ) -> anyhow::Result<()> {
        // Extract token from params
        let token = params
            .as_ref()
            .and_then(|p| p.get("token"))
            .and_then(|t| t.as_i64())
            .ok_or_else(|| anyhow::anyhow!("Token required in params"))?;

        // Map channel to Zerodha mode
        let mode = match channel {
            "ticker" | "ltp" => MODE_LTP,
            "quote" => MODE_QUOTE,
            "book" | "depth" | "full" => MODE_FULL,
            _ => MODE_QUOTE,
        };

        // Extract exchange from params or default
        let exchange = params
            .as_ref()
            .and_then(|p| p.get("exchange"))
            .and_then(|e| e.as_str())
            .unwrap_or("NSE")
            .to_string();

        // Store symbol mapping
        self.set_token_symbol_map(token, symbol.to_string(), exchange);

        // Subscribe
        self.subscribe_tokens(vec![token], mode).await?;

        Ok(())
    }

    async fn unsubscribe(&mut self, symbol: &str, channel: &str) -> anyhow::Result<()> {
        // Find token for symbol
        let token = {
            let map = self.token_symbol_map.read().await;
            map.iter()
                .find(|(_, (s, _))| s == symbol)
                .map(|(t, _)| *t)
        };

        if let Some(token) = token {
            self.unsubscribe_tokens(vec![token]).await?;

            // Remove from symbol map
            let mut map = self.token_symbol_map.write().await;
            map.remove(&token);
        }

        Ok(())
    }

    fn set_message_callback(&mut self, callback: Box<dyn Fn(MarketMessage) + Send + Sync>) {
        self.message_callback = Some(Arc::new(callback));
    }

    fn provider_name(&self) -> &str {
        "zerodha"
    }

    fn is_connected(&self) -> bool {
        self.connected.load(Ordering::SeqCst)
    }
}
