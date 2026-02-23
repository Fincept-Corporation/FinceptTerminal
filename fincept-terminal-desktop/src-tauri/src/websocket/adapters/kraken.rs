#![allow(dead_code)]
// Kraken WebSocket Adapter v2
//
// Implements Kraken WebSocket API v2 with proper real-time price streaming
// Reference: https://docs.kraken.com/api/docs/websocket-v2/ticker
// Supports: ticker, book, trade, ohlc channels

use super::WebSocketAdapter;
use crate::websocket::types::*;
use async_trait::async_trait;
use futures_util::stream::SplitSink;
use futures_util::{SinkExt, StreamExt};
use serde_json::Value;
use std::sync::atomic::{AtomicBool, AtomicU64, Ordering};
use std::sync::Arc;
use std::time::{Duration, SystemTime, UNIX_EPOCH};
use tokio::sync::Mutex;
use tokio_tungstenite::{connect_async, tungstenite::Message};

// Kraken WebSocket v2 public endpoint
const KRAKEN_WS_URL: &str = "wss://ws.kraken.com/v2";

// Heartbeat interval (30 seconds - Kraken sends heartbeats)
const HEARTBEAT_TIMEOUT_MS: u64 = 45_000;

type WsStream = tokio_tungstenite::WebSocketStream<tokio_tungstenite::MaybeTlsStream<tokio::net::TcpStream>>;
type WsSink = SplitSink<WsStream, Message>;

/// Callback type for connection errors (used to trigger reconnection)
type ErrorCallback = Box<dyn Fn(String) + Send + Sync>;

/// Kraken WebSocket Adapter - implements proper v2 API
pub struct KrakenAdapter {
    config: ProviderConfig,
    // Write half of the split stream — used by subscribe/unsubscribe/disconnect
    ws_sink: Option<Arc<Mutex<WsSink>>>,
    message_callback: Option<Arc<Box<dyn Fn(MarketMessage) + Send + Sync>>>,
    error_callback: Option<Arc<ErrorCallback>>,
    connected: Arc<AtomicBool>,
    last_message_time: Arc<AtomicU64>,
    // Shutdown signal for background tasks
    shutdown: Arc<AtomicBool>,
}

impl KrakenAdapter {
    pub fn new(config: ProviderConfig) -> Self {
        Self {
            config,
            ws_sink: None,
            message_callback: None,
            error_callback: None,
            connected: Arc::new(AtomicBool::new(false)),
            last_message_time: Arc::new(AtomicU64::new(0)),
            shutdown: Arc::new(AtomicBool::new(false)),
        }
    }

    fn now() -> u64 {
        SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .unwrap_or_else(|_| Duration::from_secs(0))
            .as_millis() as u64
    }

    /// Set error callback for connection errors (to trigger reconnection)
    pub fn set_error_callback(&mut self, callback: ErrorCallback) {
        self.error_callback = Some(Arc::new(callback));
    }

    /// Normalize Kraken symbol to standard format
    /// Kraken API v2 uses standard format like "BTC/USD"
    /// We normalize XBT -> BTC and remove slashes for internal matching
    fn normalize_symbol(symbol: &str) -> String {
        symbol
            .replace("XBT", "BTC")
            .replace("XDG", "DOGE")
            .replace('/', "")
    }

    /// Convert standard symbol to Kraken format for subscription
    /// Input: "BTC/USD" or "BTCUSD" -> Output: "BTC/USD" (Kraken v2 format)
    fn to_kraken_symbol(symbol: &str) -> String {
        // If already has slash, use as-is
        if symbol.contains('/') {
            return symbol.to_string();
        }

        // Otherwise try to detect the quote currency and add slash
        let upper = symbol.to_uppercase();
        for quote in ["USD", "USDT", "EUR", "GBP", "JPY", "CAD", "AUD", "CHF", "BTC", "ETH"] {
            if let Some(base) = upper.strip_suffix(quote) {
                return format!("{}/{}", base, quote);
            }
        }

        // Fallback: return as-is
        symbol.to_string()
    }

    /// Message receiver loop — only holds the read half, never blocks writes
    async fn receive_loop(
        mut read_stream: futures_util::stream::SplitStream<WsStream>,
        write_sink: Arc<Mutex<WsSink>>,
        callback: Arc<Box<dyn Fn(MarketMessage) + Send + Sync>>,
        error_callback: Option<Arc<ErrorCallback>>,
        connected: Arc<AtomicBool>,
        last_message_time: Arc<AtomicU64>,
        shutdown: Arc<AtomicBool>,
    ) {
        eprintln!("[Kraken] Receive loop started");

        loop {
            // Check shutdown signal
            if shutdown.load(Ordering::SeqCst) {
                eprintln!("[Kraken] Shutdown signal received, exiting receive loop");
                break;
            }

            let msg_result = read_stream.next().await;

            match msg_result {
                Some(Ok(msg)) => {
                    // Update last message time
                    last_message_time.store(Self::now(), Ordering::SeqCst);

                    match &msg {
                        Message::Text(text) => {
                            Self::handle_text_message(text, &callback);
                        }
                        Message::Ping(data) => {
                            // Respond to ping with pong
                            let sink = write_sink.clone();
                            let pong_data = data.clone();
                            tokio::spawn(async move {
                                let _ = sink.lock().await.send(Message::Pong(pong_data)).await;
                            });
                        }
                        Message::Pong(_) => {
                            // Pong received - heartbeat acknowledged
                        }
                        Message::Close(frame) => {
                            eprintln!("[Kraken] Close frame received: {:?}", frame);
                            connected.store(false, Ordering::SeqCst);
                            callback(MarketMessage::Status(StatusData {
                                provider: "kraken".to_string(),
                                status: ConnectionStatus::Disconnected,
                                message: Some(format!("Server closed: {:?}", frame)),
                                timestamp: Self::now(),
                            }));
                            break;
                        }
                        _ => {}
                    }
                }
                Some(Err(e)) => {
                    let error_msg = format!("WebSocket error: {}", e);
                    eprintln!("[Kraken] {}", error_msg);
                    connected.store(false, Ordering::SeqCst);

                    callback(MarketMessage::Status(StatusData {
                        provider: "kraken".to_string(),
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
                    let error_msg = "WebSocket connection closed".to_string();
                    eprintln!("[Kraken] {}", error_msg);
                    connected.store(false, Ordering::SeqCst);

                    callback(MarketMessage::Status(StatusData {
                        provider: "kraken".to_string(),
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

        eprintln!("[Kraken] Receive loop ended");
    }

    /// Handle incoming text message
    fn handle_text_message(text: &str, callback: &Arc<Box<dyn Fn(MarketMessage) + Send + Sync>>) {
        // Debug: log first few raw messages
        static RAW_MSG_COUNT: AtomicU64 = AtomicU64::new(0);
        let count = RAW_MSG_COUNT.fetch_add(1, Ordering::Relaxed);
        if count < 10 {
            let preview = &text[..std::cmp::min(300, text.len())];
            eprintln!("[Kraken] Raw msg #{}: {}", count, preview);
        }

        let cb = callback.clone();
        let text_clone = text.to_string();

        // Parse JSON and process
        if let Ok(data) = serde_json::from_str::<Value>(&text_clone) {
            Self::process_message(&data, &cb);
        }
    }

    /// Process a parsed message and route to callback
    fn process_message(data: &Value, callback: &Arc<Box<dyn Fn(MarketMessage) + Send + Sync>>) {
        // Get channel type
        let channel = match data.get("channel").and_then(|c| c.as_str()) {
            Some(c) => c,
            None => return, // Not a data message
        };

        // Get message type (snapshot, update, etc.)
        let msg_type = data.get("type").and_then(|t| t.as_str()).unwrap_or("");

        match channel {
            "ticker" => {
                if let Some(ticker) = Self::parse_ticker(data) {
                    // Debug log
                    static TICKER_COUNT: AtomicU64 = AtomicU64::new(0);
                    let count = TICKER_COUNT.fetch_add(1, Ordering::Relaxed);
                    if count < 5 || count % 50 == 0 {
                        eprintln!("[Kraken] Ticker #{}: {} @ {:.2}", count, ticker.symbol, ticker.price);
                    }
                    callback(MarketMessage::Ticker(ticker));
                }
            }
            "book" => {
                let is_snapshot = msg_type == "snapshot";
                if let Some(book) = Self::parse_orderbook(data, is_snapshot) {
                    callback(MarketMessage::OrderBook(book));
                }
            }
            "trade" => {
                if let Some(trades) = Self::parse_trades(data) {
                    for trade in trades {
                        callback(MarketMessage::Trade(trade));
                    }
                }
            }
            "ohlc" => {
                if let Some(candle) = Self::parse_candle(data) {
                    callback(MarketMessage::Candle(candle));
                }
            }
            "heartbeat" => {
                // Heartbeat received - connection is alive
                // eprintln!("[Kraken] Heartbeat received");
            }
            "status" => {
                // System status message
                if let Some(status) = data.get("data").and_then(|d| d.as_array()).and_then(|a| a.first()) {
                    let system = status.get("system").and_then(|s| s.as_str()).unwrap_or("unknown");
                    let version = status.get("version").and_then(|v| v.as_str()).unwrap_or("unknown");
                    eprintln!("[Kraken] System status: {} v{}", system, version);
                }
            }
            _ => {
                // Unknown channel - log for debugging
                static UNKNOWN_COUNT: AtomicU64 = AtomicU64::new(0);
                let count = UNKNOWN_COUNT.fetch_add(1, Ordering::Relaxed);
                if count < 5 {
                    eprintln!("[Kraken] Unknown channel: {}", channel);
                }
            }
        }
    }

    /// Parse ticker data from Kraken v2 format
    /// Example response:
    /// {
    ///   "channel": "ticker",
    ///   "type": "snapshot",
    ///   "data": [{
    ///     "symbol": "BTC/USD",
    ///     "bid": 42000.0,
    ///     "bid_qty": 0.5,
    ///     "ask": 42010.0,
    ///     "ask_qty": 0.3,
    ///     "last": 42005.0,
    ///     "volume": 1234.5,
    ///     "vwap": 41950.0,
    ///     "low": 41000.0,
    ///     "high": 43000.0,
    ///     "change": 500.0,
    ///     "change_pct": 1.2
    ///   }]
    /// }
    fn parse_ticker(data: &Value) -> Option<TickerData> {
        let ticker_arr = data.get("data")?.as_array()?;
        let ticker = ticker_arr.first()?;

        let symbol = ticker.get("symbol")?.as_str()?;
        let last = ticker.get("last")?.as_f64()?;

        Some(TickerData {
            provider: "kraken".to_string(),
            symbol: Self::normalize_symbol(symbol),
            price: last,
            bid: ticker.get("bid").and_then(|v| v.as_f64()),
            ask: ticker.get("ask").and_then(|v| v.as_f64()),
            bid_size: ticker.get("bid_qty").and_then(|v| v.as_f64()),
            ask_size: ticker.get("ask_qty").and_then(|v| v.as_f64()),
            volume: ticker.get("volume").and_then(|v| v.as_f64()),
            high: ticker.get("high").and_then(|v| v.as_f64()),
            low: ticker.get("low").and_then(|v| v.as_f64()),
            open: None, // Kraken v2 doesn't provide open in ticker
            close: Some(last),
            change: ticker.get("change").and_then(|v| v.as_f64()),
            change_percent: ticker.get("change_pct").and_then(|v| v.as_f64()),
            quote_volume: None,
            timestamp: Self::now(),
        })
    }

    /// Parse order book data
    /// Example response:
    /// {
    ///   "channel": "book",
    ///   "type": "snapshot",
    ///   "data": [{
    ///     "symbol": "BTC/USD",
    ///     "bids": [{"price": 42000.0, "qty": 0.5}, ...],
    ///     "asks": [{"price": 42010.0, "qty": 0.3}, ...]
    ///   }]
    /// }
    fn parse_orderbook(data: &Value, is_snapshot: bool) -> Option<OrderBookData> {
        let book_arr = data.get("data")?.as_array()?;
        let book = book_arr.first()?;

        let symbol = book.get("symbol")?.as_str()?;

        let parse_levels = |levels: &Value| -> Vec<OrderBookLevel> {
            levels.as_array()
                .map(|arr| {
                    arr.iter()
                        .filter_map(|level| {
                            Some(OrderBookLevel {
                                price: level.get("price")?.as_f64()?,
                                quantity: level.get("qty")?.as_f64()?,
                                count: None,
                            })
                        })
                        .collect()
                })
                .unwrap_or_default()
        };

        Some(OrderBookData {
            provider: "kraken".to_string(),
            symbol: Self::normalize_symbol(symbol),
            bids: book.get("bids").map(parse_levels).unwrap_or_default(),
            asks: book.get("asks").map(parse_levels).unwrap_or_default(),
            timestamp: Self::now(),
            is_snapshot,
        })
    }

    /// Parse trade data
    /// Example response:
    /// {
    ///   "channel": "trade",
    ///   "type": "update",
    ///   "data": [{
    ///     "symbol": "BTC/USD",
    ///     "side": "buy",
    ///     "price": 42005.0,
    ///     "qty": 0.1,
    ///     "ord_type": "limit",
    ///     "trade_id": 12345,
    ///     "timestamp": "2024-01-01T00:00:00.000Z"
    ///   }]
    /// }
    fn parse_trades(data: &Value) -> Option<Vec<TradeData>> {
        let trades_arr = data.get("data")?.as_array()?;

        let trades: Vec<TradeData> = trades_arr.iter()
            .filter_map(|trade| {
                let symbol = trade.get("symbol")?.as_str()?;
                let price = trade.get("price")?.as_f64()?;
                let qty = trade.get("qty")?.as_f64()?;
                let side_str = trade.get("side")?.as_str()?;

                let side = match side_str {
                    "buy" => TradeSide::Buy,
                    "sell" => TradeSide::Sell,
                    _ => TradeSide::Unknown,
                };

                // Parse trade_id (can be number or string)
                let trade_id = trade.get("trade_id")
                    .map(|v| {
                        if let Some(n) = v.as_u64() {
                            n.to_string()
                        } else if let Some(s) = v.as_str() {
                            s.to_string()
                        } else {
                            String::new()
                        }
                    })
                    .filter(|s| !s.is_empty());

                Some(TradeData {
                    provider: "kraken".to_string(),
                    symbol: Self::normalize_symbol(symbol),
                    trade_id,
                    price,
                    quantity: qty,
                    side,
                    timestamp: Self::now(),
                })
            })
            .collect();

        if trades.is_empty() { None } else { Some(trades) }
    }

    /// Parse OHLC/candle data
    fn parse_candle(data: &Value) -> Option<CandleData> {
        let candle_arr = data.get("data")?.as_array()?;
        let candle = candle_arr.first()?;

        let symbol = candle.get("symbol")?.as_str()?;

        Some(CandleData {
            provider: "kraken".to_string(),
            symbol: Self::normalize_symbol(symbol),
            open: candle.get("open")?.as_f64()?,
            high: candle.get("high")?.as_f64()?,
            low: candle.get("low")?.as_f64()?,
            close: candle.get("close")?.as_f64()?,
            volume: candle.get("volume").and_then(|v| v.as_f64()).unwrap_or(0.0),
            timestamp: Self::now(),
            interval: candle.get("interval")
                .and_then(|v| v.as_i64())
                .map(|i| format!("{}m", i))
                .unwrap_or_else(|| "1m".to_string()),
        })
    }
}

#[async_trait]
impl WebSocketAdapter for KrakenAdapter {
    async fn connect(&mut self) -> anyhow::Result<()> {
        // Reset shutdown flag
        self.shutdown.store(false, Ordering::SeqCst);

        let url = if self.config.url.is_empty() {
            KRAKEN_WS_URL
        } else {
            &self.config.url
        };

        eprintln!("[Kraken] Connecting to {}", url);

        let (ws_stream, response) = connect_async(url).await
            .map_err(|e| {
                eprintln!("[Kraken] Connection failed: {}", e);
                e
            })?;

        eprintln!("[Kraken] Connected! Response: {:?}", response.status());

        // Split into independent read/write halves
        let (write_half, read_half) = ws_stream.split();
        let sink = Arc::new(Mutex::new(write_half));
        self.ws_sink = Some(sink.clone());
        self.connected.store(true, Ordering::SeqCst);
        self.last_message_time.store(Self::now(), Ordering::SeqCst);

        // Start receive loop
        if let Some(callback) = self.message_callback.clone() {
            let connected = self.connected.clone();
            let error_callback = self.error_callback.clone();
            let last_message_time = self.last_message_time.clone();
            let shutdown = self.shutdown.clone();

            tokio::spawn(async move {
                Self::receive_loop(
                    read_half,
                    sink,
                    callback,
                    error_callback,
                    connected,
                    last_message_time,
                    shutdown,
                ).await;
            });
        } else {
            eprintln!("[Kraken] Warning: No message callback set");
        }

        // Emit connected status
        if let Some(ref cb) = self.message_callback {
            cb(MarketMessage::Status(StatusData {
                provider: "kraken".to_string(),
                status: ConnectionStatus::Connected,
                message: Some("Connected to Kraken WebSocket v2".to_string()),
                timestamp: Self::now(),
            }));
        }

        Ok(())
    }

    async fn disconnect(&mut self) -> anyhow::Result<()> {
        eprintln!("[Kraken] Disconnecting...");

        // Signal shutdown
        self.shutdown.store(true, Ordering::SeqCst);
        self.connected.store(false, Ordering::SeqCst);

        // Close WebSocket
        if let Some(sink) = &self.ws_sink {
            let _ = sink.lock().await.close().await;
        }

        self.ws_sink = None;
        eprintln!("[Kraken] Disconnected");
        Ok(())
    }

    async fn subscribe(
        &mut self,
        symbol: &str,
        channel: &str,
        params: Option<Value>,
    ) -> anyhow::Result<()> {
        if !self.connected.load(Ordering::SeqCst) {
            return Err(anyhow::anyhow!("Not connected"));
        }

        let sink = self.ws_sink.as_ref()
            .ok_or_else(|| anyhow::anyhow!("WebSocket sink not available"))?;

        // Convert symbol to Kraken format
        let kraken_symbol = Self::to_kraken_symbol(symbol);

        // Build Kraken v2 subscription message
        let mut sub_msg = serde_json::json!({
            "method": "subscribe",
            "params": {
                "channel": channel,
                "symbol": [kraken_symbol],
            }
        });

        // Add optional params (depth for book, interval for ohlc)
        if let Some(p) = params {
            if let Some(params_obj) = sub_msg.get_mut("params").and_then(|p| p.as_object_mut()) {
                if let Some(p_obj) = p.as_object() {
                    for (k, v) in p_obj {
                        params_obj.insert(k.clone(), v.clone());
                    }
                }
            }
        }

        // Add default depth for book channel
        if channel == "book" {
            if let Some(params_obj) = sub_msg.get_mut("params").and_then(|p| p.as_object_mut()) {
                if !params_obj.contains_key("depth") {
                    params_obj.insert("depth".to_string(), serde_json::json!(25));
                }
            }
        }

        let msg_str = serde_json::to_string(&sub_msg)?;
        eprintln!("[Kraken] Subscribing: {}", msg_str);

        match sink.lock().await.send(Message::Text(msg_str)).await {
            Ok(_) => {
                eprintln!("[Kraken] Subscription sent for {} {}", kraken_symbol, channel);
                Ok(())
            }
            Err(e) => {
                eprintln!("[Kraken] Subscription failed: {}", e);
                self.connected.store(false, Ordering::SeqCst);
                Err(anyhow::anyhow!("Failed to subscribe: {}", e))
            }
        }
    }

    async fn unsubscribe(&mut self, symbol: &str, channel: &str) -> anyhow::Result<()> {
        let sink = self.ws_sink.as_ref()
            .ok_or_else(|| anyhow::anyhow!("Not connected"))?;

        let kraken_symbol = Self::to_kraken_symbol(symbol);

        let unsub_msg = serde_json::json!({
            "method": "unsubscribe",
            "params": {
                "channel": channel,
                "symbol": [kraken_symbol],
            }
        });

        let msg_str = serde_json::to_string(&unsub_msg)?;
        eprintln!("[Kraken] Unsubscribing: {}", msg_str);

        sink.lock().await.send(Message::Text(msg_str)).await?;
        Ok(())
    }

    fn set_message_callback(&mut self, callback: Box<dyn Fn(MarketMessage) + Send + Sync>) {
        self.message_callback = Some(Arc::new(callback));
    }

    fn provider_name(&self) -> &str {
        "kraken"
    }

    fn is_connected(&self) -> bool {
        self.connected.load(Ordering::SeqCst)
    }
}
