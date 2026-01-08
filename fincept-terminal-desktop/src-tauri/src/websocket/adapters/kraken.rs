// Kraken WebSocket Adapter
//
// Implements Kraken WebSocket API v2
// Supports: ticker, book, trade, ohlc channels

use super::WebSocketAdapter;
use crate::websocket::types::*;
use async_trait::async_trait;
use futures_util::{SinkExt, StreamExt};
use serde_json::Value;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use std::time::{Duration, SystemTime, UNIX_EPOCH};
use tokio::sync::RwLock;
use tokio_tungstenite::{connect_async, tungstenite::Message};

const KRAKEN_WS_URL: &str = "wss://ws.kraken.com/v2";

/// Callback type for connection errors (used to trigger reconnection)
type ErrorCallback = Box<dyn Fn(String) + Send + Sync>;

pub struct KrakenAdapter {
    config: ProviderConfig,
    ws: Option<Arc<RwLock<tokio_tungstenite::WebSocketStream<tokio_tungstenite::MaybeTlsStream<tokio::net::TcpStream>>>>>,
    message_callback: Option<Arc<Box<dyn Fn(MarketMessage) + Send + Sync>>>,
    error_callback: Option<Arc<ErrorCallback>>,
    connected: Arc<AtomicBool>,
    last_message_time: Arc<RwLock<u64>>,
}

impl KrakenAdapter {
    pub fn new(config: ProviderConfig) -> Self {
        Self {
            config,
            ws: None,
            message_callback: None,
            error_callback: None,
            connected: Arc::new(AtomicBool::new(false)),
            last_message_time: Arc::new(RwLock::new(0)),
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

    /// Normalize Kraken symbol (XBT -> BTC)
    fn normalize_kraken_symbol(symbol: &str) -> String {
        symbol.replace("XBT", "BTC").replace('/', "")
    }

    /// Message receiver loop with error propagation
    async fn receive_loop(
        ws: Arc<RwLock<tokio_tungstenite::WebSocketStream<tokio_tungstenite::MaybeTlsStream<tokio::net::TcpStream>>>>,
        callback: Arc<Box<dyn Fn(MarketMessage) + Send + Sync>>,
        error_callback: Option<Arc<ErrorCallback>>,
        connected: Arc<AtomicBool>,
        last_message_time: Arc<RwLock<u64>>,
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

                    // Handle message in separate task to avoid blocking
                    let cb = callback.clone();
                    tokio::spawn(async move {
                        if let Message::Text(text) = msg {
                            if let Ok(data) = serde_json::from_str::<Value>(&text) {
                                Self::process_message(&data, &cb);
                            }
                        }
                    });
                }
                Some(Err(e)) => {
                    let error_msg = format!("WebSocket error: {}", e);
                    eprintln!("[Kraken] {}", error_msg);
                    connected.store(false, Ordering::SeqCst);

                    // Emit error status via message callback
                    callback(MarketMessage::Status(StatusData {
                        provider: "kraken".to_string(),
                        status: ConnectionStatus::Error,
                        message: Some(error_msg.clone()),
                        timestamp: Self::now(),
                    }));

                    // Notify error callback for reconnection handling
                    if let Some(err_cb) = &error_callback {
                        err_cb(error_msg);
                    }
                    break;
                }
                None => {
                    let error_msg = "WebSocket connection closed unexpectedly".to_string();
                    eprintln!("[Kraken] {}", error_msg);
                    connected.store(false, Ordering::SeqCst);

                    // Emit disconnected status
                    callback(MarketMessage::Status(StatusData {
                        provider: "kraken".to_string(),
                        status: ConnectionStatus::Disconnected,
                        message: Some(error_msg.clone()),
                        timestamp: Self::now(),
                    }));

                    // Notify error callback for reconnection handling
                    if let Some(err_cb) = &error_callback {
                        err_cb(error_msg);
                    }
                    break;
                }
            }
        }
    }

    /// Process a parsed message and route to callback
    fn process_message(data: &Value, callback: &Arc<Box<dyn Fn(MarketMessage) + Send + Sync>>) {
        if let Some(channel) = data.get("channel").and_then(|c| c.as_str()) {
            match channel {
                "ticker" => {
                    if let Some(ticker) = Self::parse_ticker_static(data) {
                        callback(MarketMessage::Ticker(ticker));
                    }
                }
                "book" => {
                    let is_snapshot = data.get("type")
                        .and_then(|t| t.as_str())
                        .map(|t| t == "snapshot")
                        .unwrap_or(false);

                    if let Some(book) = Self::parse_orderbook_static(data, is_snapshot) {
                        callback(MarketMessage::OrderBook(book));
                    }
                }
                "trade" => {
                    if let Some(trades) = Self::parse_trade_static(data) {
                        for trade in trades {
                            callback(MarketMessage::Trade(trade));
                        }
                    }
                }
                "ohlc" => {
                    if let Some(candle) = Self::parse_candle_static(data) {
                        callback(MarketMessage::Candle(candle));
                    }
                }
                "heartbeat" | "status" => {
                    // Heartbeat and status messages - ignore silently
                }
                _ => {}
            }
        }
    }

    // Static parsing methods (don't need &self)
    fn parse_ticker_static(data: &Value) -> Option<TickerData> {
        let ticker_data = data.get("data")?.as_array()?.get(0)?;
        let symbol = ticker_data.get("symbol")?.as_str()?;
        let last = ticker_data.get("last")?.as_f64()?;

        Some(TickerData {
            provider: "kraken".to_string(),
            symbol: Self::normalize_kraken_symbol(symbol),
            price: last,
            bid: ticker_data.get("bid").and_then(|v| v.as_f64()),
            ask: ticker_data.get("ask").and_then(|v| v.as_f64()),
            bid_size: ticker_data.get("bid_qty").and_then(|v| v.as_f64()),
            ask_size: ticker_data.get("ask_qty").and_then(|v| v.as_f64()),
            volume: ticker_data.get("volume").and_then(|v| v.as_f64()),
            high: ticker_data.get("high").and_then(|v| v.as_f64()),
            low: ticker_data.get("low").and_then(|v| v.as_f64()),
            open: ticker_data.get("vwap").and_then(|v| v.as_f64()),
            close: Some(last),
            change: None,
            change_percent: None,
            timestamp: Self::now(),
        })
    }

    fn parse_orderbook_static(data: &Value, is_snapshot: bool) -> Option<OrderBookData> {
        let book_data = data.get("data")?.as_array()?.get(0)?;
        let symbol = book_data.get("symbol")?.as_str()?;

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
            symbol: Self::normalize_kraken_symbol(symbol),
            bids: book_data.get("bids").map(parse_levels).unwrap_or_default(),
            asks: book_data.get("asks").map(parse_levels).unwrap_or_default(),
            timestamp: Self::now(),
            is_snapshot,
        })
    }

    fn parse_trade_static(data: &Value) -> Option<Vec<TradeData>> {
        let trade_data = data.get("data")?.as_array()?.get(0)?;
        let symbol = trade_data.get("symbol")?.as_str()?;

        // Kraken v2 format: data array contains trade objects
        let trades = if let Some(trades_array) = data.get("data").and_then(|d| d.as_array()) {
            trades_array.iter()
                .filter_map(|trade| {
                    let price = trade.get("price")?.as_f64()?;
                    let qty = trade.get("qty")?.as_f64()?;
                    let side_str = trade.get("side")?.as_str()?;

                    let side = match side_str {
                        "buy" => TradeSide::Buy,
                        "sell" => TradeSide::Sell,
                        _ => TradeSide::Unknown,
                    };

                    Some(TradeData {
                        provider: "kraken".to_string(),
                        symbol: Self::normalize_kraken_symbol(symbol),
                        trade_id: trade.get("trade_id").and_then(|v| v.as_str()).map(String::from),
                        price,
                        quantity: qty,
                        side,
                        timestamp: Self::now(),
                    })
                })
                .collect()
        } else {
            vec![]
        };

        if trades.is_empty() { None } else { Some(trades) }
    }

    fn parse_candle_static(data: &Value) -> Option<CandleData> {
        let candle_array = data.get("data")?.as_array()?;
        let candle_data = candle_array.get(0)?;
        let symbol = candle_data.get("symbol")?.as_str()?;

        Some(CandleData {
            provider: "kraken".to_string(),
            symbol: Self::normalize_kraken_symbol(symbol),
            open: candle_data.get("open")?.as_f64()?,
            high: candle_data.get("high")?.as_f64()?,
            low: candle_data.get("low")?.as_f64()?,
            close: candle_data.get("close")?.as_f64()?,
            volume: candle_data.get("volume")?.as_f64().unwrap_or(0.0),
            timestamp: Self::now(),
            interval: candle_data.get("interval")
                .and_then(|v| v.as_str())
                .unwrap_or("1m")
                .to_string(),
        })
    }
}

#[async_trait]
impl WebSocketAdapter for KrakenAdapter {
    async fn connect(&mut self) -> anyhow::Result<()> {
        let url = self.config.url.clone();
        let connect_url = if url.is_empty() { KRAKEN_WS_URL } else { &url };

        eprintln!("[Kraken::connect] Connecting to: {}", connect_url);

        let (ws_stream, _) = connect_async(connect_url).await
            .map_err(|e| {
                eprintln!("[Kraken::connect] ✗ Failed to connect: {}", e);
                e
            })?;

        eprintln!("[Kraken::connect] ✓ WebSocket connection established");

        let ws = Arc::new(RwLock::new(ws_stream));
        self.ws = Some(ws.clone());
        self.connected.store(true, Ordering::SeqCst);

        eprintln!("[Kraken::connect] Starting receive loop...");

        // Start receive loop with error callback
        if let Some(callback) = self.message_callback.clone() {
            let connected = self.connected.clone();
            let error_callback = self.error_callback.clone();
            let last_message_time = self.last_message_time.clone();
            tokio::spawn(async move {
                Self::receive_loop(ws, callback, error_callback, connected, last_message_time).await;
            });
            eprintln!("[Kraken::connect] ✓ Receive loop started");
        } else {
            eprintln!("[Kraken::connect] ⚠ No message callback set!");
        }

        eprintln!("[Kraken::connect] ✓ Connection complete");
        Ok(())
    }

    async fn disconnect(&mut self) -> anyhow::Result<()> {
        self.connected.store(false, Ordering::SeqCst);

        if let Some(ws) = &self.ws {
            let _ = ws.write().await.close(None).await;
        }

        self.ws = None;
        Ok(())
    }

    async fn subscribe(
        &mut self,
        symbol: &str,
        channel: &str,
        params: Option<Value>,
    ) -> anyhow::Result<()> {
        eprintln!("[Kraken::subscribe] symbol={}, channel={}", symbol, channel);

        let ws = self.ws.as_ref()
            .ok_or_else(|| {
                eprintln!("[Kraken::subscribe] ✗ Not connected");
                anyhow::anyhow!("Not connected")
            })?;

        // Build subscription message
        let mut sub_msg = serde_json::json!({
            "method": "subscribe",
            "params": {
                "channel": channel,
                "symbol": [symbol],
            }
        });

        // Add optional params (depth, snapshot, etc.)
        if let Some(p) = params {
            if let Some(obj) = sub_msg.get_mut("params") {
                if let Some(params_obj) = obj.as_object_mut() {
                    if let Some(p_obj) = p.as_object() {
                        for (k, v) in p_obj {
                            params_obj.insert(k.clone(), v.clone());
                        }
                    }
                }
            }
        }

        let msg_str = serde_json::to_string(&sub_msg)?;
        eprintln!("[Kraken::subscribe] Sending subscription message: {}", msg_str);

        ws.write().await.send(Message::Text(msg_str)).await
            .map_err(|e| {
                eprintln!("[Kraken::subscribe] ✗ Failed to send: {}", e);
                anyhow::anyhow!("Failed to send subscription: {}", e)
            })?;

        eprintln!("[Kraken::subscribe] ✓ Subscription message sent");
        Ok(())
    }

    async fn unsubscribe(&mut self, symbol: &str, channel: &str) -> anyhow::Result<()> {
        let ws = self.ws.as_ref()
            .ok_or_else(|| anyhow::anyhow!("Not connected"))?;

        let unsub_msg = serde_json::json!({
            "method": "unsubscribe",
            "params": {
                "channel": channel,
                "symbol": [symbol],
            }
        });

        let msg_str = serde_json::to_string(&unsub_msg)?;
        ws.write().await.send(Message::Text(msg_str)).await?;

        Ok(())
    }

    fn set_message_callback(&mut self, callback: Box<dyn Fn(MarketMessage) + Send + Sync>) {
        self.message_callback = Some(Arc::new(callback));
    }

    fn provider_name(&self) -> &str {
        "kraken"
    }

    fn is_connected(&self) -> bool {
        // Use AtomicBool for lock-free, non-blocking check
        self.connected.load(Ordering::SeqCst)
    }
}
