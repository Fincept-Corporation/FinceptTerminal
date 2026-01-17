// Fyers WebSocket Adapter with Binary Protocol (HSM v1.5)
//
// Fyers uses a proprietary binary protocol over WebSocket for real-time data:
// - Connection: wss://socket.fyers.in/hsm/v1-5/prod
// - Authentication: Requires access token in connection URL
// - Protocol: Binary messages with specific packet structures
// - Subscription: Symbol tokens from master contract (e.g., NSE:RELIANCE-EQ)

use super::WebSocketAdapter;
use crate::websocket::types::*;
use async_trait::async_trait;
use futures_util::{SinkExt, StreamExt};
use serde_json::Value;
use std::collections::HashMap;
use std::sync::Arc;
use tokio::net::TcpStream;
use tokio::sync::RwLock;
use tokio_tungstenite::{connect_async, tungstenite::Message, MaybeTlsStream, WebSocketStream};

// Subscription mode for internal tracking
#[derive(Debug, Clone, PartialEq)]
enum SubscriptionMode {
    Ltp,
    Quote,
    Full,
}

// ============================================================================
// FYERS WEBSOCKET CONFIGURATION
// ============================================================================

const FYERS_WS_URL: &str = "wss://socket.fyers.in/hsm/v1-5/prod";

// Fyers data packet structure constants
const PACKET_TYPE_TICK: u8 = 31;
const PACKET_TYPE_DEPTH: u8 = 41;
const PACKET_TYPE_FULL_DEPTH: u8 = 43;
const PACKET_TYPE_CONFIRMATION: u8 = 8;

// ============================================================================
// BINARY PROTOCOL STRUCTURES
// ============================================================================

/// Fyers tick data packet (minimal binary structure)
#[derive(Debug, Clone)]
struct FyersTickPacket {
    packet_type: u8,
    token: i32,
    ltp: f64,
    volume: i64,
    change_percent: f64,
    timestamp: u64,
}

/// Fyers market depth packet
#[derive(Debug, Clone)]
struct FyersDepthPacket {
    packet_type: u8,
    token: i32,
    bids: Vec<(f64, i32)>, // (price, quantity)
    asks: Vec<(f64, i32)>,
}

// ============================================================================
// FYERS ADAPTER
// ============================================================================

pub struct FyersAdapter {
    config: ProviderConfig,
    access_token: String,
    ws_stream: Arc<RwLock<Option<WebSocketStream<MaybeTlsStream<TcpStream>>>>>,
    message_callback: Option<Box<dyn Fn(MarketMessage) + Send + Sync>>,
    subscriptions: Arc<RwLock<HashMap<String, SubscriptionMode>>>,
    is_connected: Arc<RwLock<bool>>,
    token_symbol_map: Arc<RwLock<HashMap<i32, String>>>, // Token -> Symbol mapping
}

impl FyersAdapter {
    pub fn new(config: ProviderConfig) -> Self {
        let access_token = config.api_key.clone().unwrap_or_default();
        Self {
            config,
            access_token,
            ws_stream: Arc::new(RwLock::new(None)),
            message_callback: None,
            subscriptions: Arc::new(RwLock::new(HashMap::new())),
            is_connected: Arc::new(RwLock::new(false)),
            token_symbol_map: Arc::new(RwLock::new(HashMap::new())),
        }
    }

    /// Parse binary tick data from Fyers HSM protocol
    fn parse_tick_data(&self, data: &[u8]) -> Option<FyersTickPacket> {
        if data.len() < 32 {
            return None;
        }

        let packet_type = data[0];
        if packet_type != PACKET_TYPE_TICK {
            return None;
        }

        // Parse binary fields (little-endian)
        let token = i32::from_le_bytes([data[1], data[2], data[3], data[4]]);
        let ltp = f64::from_le_bytes([
            data[5], data[6], data[7], data[8], data[9], data[10], data[11], data[12],
        ]);
        let volume = i64::from_le_bytes([
            data[13], data[14], data[15], data[16], data[17], data[18], data[19], data[20],
        ]);
        let change_percent = f64::from_le_bytes([
            data[21], data[22], data[23], data[24], data[25], data[26], data[27], data[28],
        ]);
        let timestamp = u64::from_le_bytes([
            data[29], data[30], data[31], data[32], data[33], data[34], data[35], data[36],
        ]);

        Some(FyersTickPacket {
            packet_type,
            token,
            ltp,
            volume,
            change_percent,
            timestamp,
        })
    }

    /// Parse binary market depth from Fyers
    fn parse_depth_data(&self, data: &[u8]) -> Option<FyersDepthPacket> {
        if data.len() < 10 {
            return None;
        }

        let packet_type = data[0];
        if packet_type != PACKET_TYPE_DEPTH && packet_type != PACKET_TYPE_FULL_DEPTH {
            return None;
        }

        let token = i32::from_le_bytes([data[1], data[2], data[3], data[4]]);

        // Parse 5 levels of bids and asks
        let mut bids = Vec::new();
        let mut asks = Vec::new();

        let mut offset = 5;
        for _ in 0..5 {
            if offset + 12 <= data.len() {
                let price = f64::from_le_bytes([
                    data[offset],
                    data[offset + 1],
                    data[offset + 2],
                    data[offset + 3],
                    data[offset + 4],
                    data[offset + 5],
                    data[offset + 6],
                    data[offset + 7],
                ]);
                let qty = i32::from_le_bytes([
                    data[offset + 8],
                    data[offset + 9],
                    data[offset + 10],
                    data[offset + 11],
                ]);
                bids.push((price, qty));
                offset += 12;
            }
        }

        for _ in 0..5 {
            if offset + 12 <= data.len() {
                let price = f64::from_le_bytes([
                    data[offset],
                    data[offset + 1],
                    data[offset + 2],
                    data[offset + 3],
                    data[offset + 4],
                    data[offset + 5],
                    data[offset + 6],
                    data[offset + 7],
                ]);
                let qty = i32::from_le_bytes([
                    data[offset + 8],
                    data[offset + 9],
                    data[offset + 10],
                    data[offset + 11],
                ]);
                asks.push((price, qty));
                offset += 12;
            }
        }

        Some(FyersDepthPacket {
            packet_type,
            token,
            bids,
            asks,
        })
    }

    /// Convert Fyers symbol to token (uses master contract database)
    async fn symbol_to_token(&self, symbol: &str) -> Option<i32> {
        // Parse Fyers symbol format: "EXCHANGE:SYMBOL" (e.g., "NSE:RELIANCE-EQ")
        let parts: Vec<&str> = symbol.split(':').collect();

        if parts.len() != 2 {
            eprintln!("Fyers: Invalid symbol format (expected EXCHANGE:SYMBOL): {}", symbol);
            return None;
        }

        let exchange = parts[0];
        let symbol_name = parts[1];

        // Look up token in database
        match crate::database::fyers_symbols::get_token_by_symbol(symbol_name, exchange) {
            Ok(Some(token)) => {
                eprintln!("Fyers: Mapped {}:{} -> token {}", exchange, symbol_name, token);
                Some(token)
            }
            Ok(None) => {
                eprintln!("Fyers: Symbol not found in database: {}:{}", exchange, symbol_name);
                None
            }
            Err(e) => {
                eprintln!("Fyers: Database error during token lookup: {}", e);
                None
            }
        }
    }

    /// Build subscription message for Fyers
    fn build_subscription_message(&self, tokens: Vec<i32>, mode: &str) -> Vec<u8> {
        // Fyers subscription format (binary):
        // [message_type (1 byte)][mode (1 byte)][token_count (2 bytes)][tokens...]
        let mut msg = Vec::new();

        // Message type: 1 = Subscribe, 0 = Unsubscribe
        msg.push(1);

        // Mode: 1 = LTP, 2 = Quotes, 3 = Full depth
        let mode_byte = match mode {
            "ltp" => 1,
            "quotes" => 2,
            "depth" => 3,
            _ => 1,
        };
        msg.push(mode_byte);

        // Token count
        let token_count = tokens.len() as u16;
        msg.extend_from_slice(&token_count.to_le_bytes());

        // Tokens
        for token in tokens {
            msg.extend_from_slice(&token.to_le_bytes());
        }

        msg
    }

    /// Handle incoming binary message
    async fn handle_binary_message(&self, data: Vec<u8>) {
        if data.is_empty() {
            return;
        }

        let packet_type = data[0];

        match packet_type {
            PACKET_TYPE_TICK => {
                if let Some(tick) = self.parse_tick_data(&data) {
                    self.emit_tick_message(tick).await;
                }
            }
            PACKET_TYPE_DEPTH | PACKET_TYPE_FULL_DEPTH => {
                if let Some(depth) = self.parse_depth_data(&data) {
                    self.emit_depth_message(depth).await;
                }
            }
            PACKET_TYPE_CONFIRMATION => {
                eprintln!("Fyers: Subscription confirmed");
            }
            _ => {
                eprintln!("Fyers: Unknown packet type: {}", packet_type);
            }
        }
    }

    /// Emit tick message to callback
    async fn emit_tick_message(&self, tick: FyersTickPacket) {
        if let Some(callback) = &self.message_callback {
            // Look up symbol from token
            let symbol_map = self.token_symbol_map.read().await;
            let symbol = symbol_map
                .get(&tick.token)
                .cloned()
                .unwrap_or_else(|| format!("TOKEN_{}", tick.token));

            let message = MarketMessage::Ticker(TickerData {
                provider: "fyers".to_string(),
                symbol,
                price: tick.ltp,
                bid: None,
                ask: None,
                bid_size: None,
                ask_size: None,
                volume: Some(tick.volume as f64),
                high: None,
                low: None,
                open: None,
                close: None,
                change: None,
                change_percent: Some(tick.change_percent),
                timestamp: tick.timestamp,
            });

            callback(message);
        }
    }

    /// Emit depth message to callback
    async fn emit_depth_message(&self, depth: FyersDepthPacket) {
        if let Some(callback) = &self.message_callback {
            let symbol_map = self.token_symbol_map.read().await;
            let symbol = symbol_map
                .get(&depth.token)
                .cloned()
                .unwrap_or_else(|| format!("TOKEN_{}", depth.token));

            let bids: Vec<OrderBookLevel> = depth
                .bids
                .iter()
                .map(|(price, qty)| OrderBookLevel {
                    price: *price,
                    quantity: *qty as f64,
                    count: None,
                })
                .collect();

            let asks: Vec<OrderBookLevel> = depth
                .asks
                .iter()
                .map(|(price, qty)| OrderBookLevel {
                    price: *price,
                    quantity: *qty as f64,
                    count: None,
                })
                .collect();

            let message = MarketMessage::OrderBook(OrderBookData {
                provider: "fyers".to_string(),
                symbol,
                bids,
                asks,
                timestamp: chrono::Utc::now().timestamp() as u64,
                is_snapshot: true,
            });

            callback(message);
        }
    }

    /// Start message processing loop
    async fn start_message_loop(
        ws_stream: Arc<RwLock<Option<WebSocketStream<MaybeTlsStream<TcpStream>>>>>,
        adapter: Arc<RwLock<FyersAdapter>>,
    ) {
        loop {
            let mut stream_lock = ws_stream.write().await;

            if let Some(stream) = stream_lock.as_mut() {
                match stream.next().await {
                    Some(Ok(Message::Binary(data))) => {
                        drop(stream_lock);
                        let adapter_read = adapter.read().await;
                        adapter_read.handle_binary_message(data).await;
                    }
                    Some(Ok(Message::Text(text))) => {
                        drop(stream_lock);
                        eprintln!("Fyers: Unexpected text message: {}", text);
                    }
                    Some(Ok(Message::Ping(data))) => {
                        let _ = stream.send(Message::Pong(data)).await;
                    }
                    Some(Ok(Message::Close(_))) => {
                        drop(stream_lock);
                        eprintln!("Fyers: WebSocket closed");
                        break;
                    }
                    Some(Err(e)) => {
                        drop(stream_lock);
                        eprintln!("Fyers: WebSocket error: {}", e);
                        break;
                    }
                    None => {
                        drop(stream_lock);
                        eprintln!("Fyers: Stream ended");
                        break;
                    }
                    _ => {}
                }
            } else {
                drop(stream_lock);
                break;
            }
        }

        // Mark as disconnected
        let adapter_read = adapter.read().await;
        let mut connected = adapter_read.is_connected.write().await;
        *connected = false;
    }
}

#[async_trait]
impl WebSocketAdapter for FyersAdapter {
    async fn connect(&mut self) -> anyhow::Result<()> {
        // Use access token from adapter
        if self.access_token.is_empty() {
            return Err(anyhow::anyhow!("Access token not configured"));
        }

        // Build WebSocket URL with access token
        let url = format!("{}?access_token={}", FYERS_WS_URL, self.access_token);

        // Connect to WebSocket
        let (ws_stream, _) = connect_async(&url).await?;

        eprintln!("Fyers: WebSocket connected");

        // Store stream
        let mut stream_lock = self.ws_stream.write().await;
        *stream_lock = Some(ws_stream);
        drop(stream_lock);

        // Mark as connected
        let mut connected = self.is_connected.write().await;
        *connected = true;

        // Start message processing loop
        let ws_stream_clone = Arc::clone(&self.ws_stream);
        let adapter_arc = Arc::new(RwLock::new(Self {
            config: self.config.clone(),
            access_token: self.access_token.clone(),
            ws_stream: Arc::clone(&self.ws_stream),
            message_callback: self.message_callback.take(),
            subscriptions: Arc::clone(&self.subscriptions),
            is_connected: Arc::clone(&self.is_connected),
            token_symbol_map: Arc::clone(&self.token_symbol_map),
        }));

        tokio::spawn(async move {
            Self::start_message_loop(ws_stream_clone, adapter_arc).await;
        });

        Ok(())
    }

    async fn disconnect(&mut self) -> anyhow::Result<()> {
        let mut stream_lock = self.ws_stream.write().await;
        if let Some(mut stream) = stream_lock.take() {
            stream.close(None).await?;
        }

        let mut connected = self.is_connected.write().await;
        *connected = false;

        eprintln!("Fyers: Disconnected");
        Ok(())
    }

    async fn subscribe(
        &mut self,
        symbol: &str,
        channel: &str,
        _params: Option<Value>,
    ) -> anyhow::Result<()> {
        // Convert symbol to token
        let token = self
            .symbol_to_token(symbol)
            .await
            .ok_or_else(|| anyhow::anyhow!("Could not map symbol to token: {}", symbol))?;

        // Store symbol-token mapping
        let mut symbol_map = self.token_symbol_map.write().await;
        symbol_map.insert(token, symbol.to_string());
        drop(symbol_map);

        // Determine subscription mode from channel
        let mode = match channel {
            "tick" | "ltp" => "ltp",
            "quote" | "quotes" => "quotes",
            "depth" | "market_depth" => "depth",
            _ => "ltp",
        };

        // Build subscription message
        let sub_msg = self.build_subscription_message(vec![token], mode);

        // Send subscription
        let mut stream_lock = self.ws_stream.write().await;
        if let Some(stream) = stream_lock.as_mut() {
            stream.send(Message::Binary(sub_msg)).await?;

            // Store subscription
            drop(stream_lock);
            let mut subs = self.subscriptions.write().await;
            let sub_mode = match mode {
                "ltp" => SubscriptionMode::Ltp,
                "quotes" => SubscriptionMode::Quote,
                "depth" => SubscriptionMode::Full,
                _ => SubscriptionMode::Ltp,
            };
            subs.insert(symbol.to_string(), sub_mode);

            eprintln!("Fyers: Subscribed to {} (token: {}, mode: {})", symbol, token, mode);
            Ok(())
        } else {
            Err(anyhow::anyhow!("Not connected"))
        }
    }

    async fn unsubscribe(&mut self, symbol: &str, _channel: &str) -> anyhow::Result<()> {
        // Convert symbol to token
        if let Some(token) = self.symbol_to_token(symbol).await {
            // Build unsubscribe message (same format, but message_type = 0)
            let mut msg = Vec::new();
            msg.push(0); // Unsubscribe
            msg.push(1); // Mode (doesn't matter for unsubscribe)
            msg.extend_from_slice(&1u16.to_le_bytes()); // Token count
            msg.extend_from_slice(&token.to_le_bytes()); // Token

            // Send unsubscribe
            let mut stream_lock = self.ws_stream.write().await;
            if let Some(stream) = stream_lock.as_mut() {
                stream.send(Message::Binary(msg)).await?;

                // Remove subscription
                drop(stream_lock);
                let mut subs = self.subscriptions.write().await;
                subs.remove(symbol);

                eprintln!("Fyers: Unsubscribed from {} (token: {})", symbol, token);
            }
        }

        Ok(())
    }

    fn set_message_callback(&mut self, callback: Box<dyn Fn(MarketMessage) + Send + Sync>) {
        self.message_callback = Some(callback);
    }

    fn provider_name(&self) -> &str {
        "fyers"
    }

    fn is_connected(&self) -> bool {
        // Read from Arc<RwLock<bool>> synchronously (blocking)
        futures::executor::block_on(async { *self.is_connected.read().await })
    }
}
