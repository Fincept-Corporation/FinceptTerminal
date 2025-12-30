// Kraken WebSocket Adapter
//
// Implements Kraken WebSocket API v2
// Supports: ticker, book, trade, ohlc channels

use super::WebSocketAdapter;
use crate::websocket::types::*;
use async_trait::async_trait;
use futures_util::{SinkExt, StreamExt};
use serde::{Deserialize, Serialize};
use serde_json::Value;
use std::sync::Arc;
use std::time::{SystemTime, UNIX_EPOCH};
use tokio::sync::RwLock;
use tokio_tungstenite::{connect_async, tungstenite::Message};

const KRAKEN_WS_URL: &str = "wss://ws.kraken.com/v2";

pub struct KrakenAdapter {
    config: ProviderConfig,
    ws: Option<Arc<RwLock<tokio_tungstenite::WebSocketStream<tokio_tungstenite::MaybeTlsStream<tokio::net::TcpStream>>>>>,
    message_callback: Option<Arc<Box<dyn Fn(MarketMessage) + Send + Sync>>>,
    connected: Arc<RwLock<bool>>,
}

impl KrakenAdapter {
    pub fn new(config: ProviderConfig) -> Self {
        Self {
            config,
            ws: None,
            message_callback: None,
            connected: Arc::new(RwLock::new(false)),
        }
    }

    fn now() -> u64 {
        SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .unwrap()
            .as_millis() as u64
    }

    /// Normalize Kraken symbol (XBT -> BTC)
    fn normalize_kraken_symbol(symbol: &str) -> String {
        symbol.replace("XBT", "BTC").replace('/', "")
    }

    /// Parse Kraken ticker message
    fn parse_ticker(&self, data: &Value) -> Option<TickerData> {
        let symbol = data.get("symbol")?.as_str()?;
        let ticker_data = data.get("data")?.as_array()?.get(0)?;

        let last = ticker_data.get("last")?.as_str()?.parse::<f64>().ok()?;
        let bid = ticker_data.get("bid")?.as_str()?.parse::<f64>().ok();
        let ask = ticker_data.get("ask")?.as_str()?.parse::<f64>().ok();
        let volume = ticker_data.get("volume")?.as_str()?.parse::<f64>().ok();
        let high = ticker_data.get("high")?.as_str()?.parse::<f64>().ok();
        let low = ticker_data.get("low")?.as_str()?.parse::<f64>().ok();

        Some(TickerData {
            provider: "kraken".to_string(),
            symbol: Self::normalize_kraken_symbol(symbol),
            price: last,
            bid,
            ask,
            bid_size: None,
            ask_size: None,
            volume,
            high,
            low,
            open: None,
            close: None,
            change: None,
            change_percent: None,
            timestamp: Self::now(),
        })
    }

    /// Parse Kraken order book message
    fn parse_orderbook(&self, data: &Value, is_snapshot: bool) -> Option<OrderBookData> {
        let symbol = data.get("symbol")?.as_str()?;
        let book_data = data.get("data")?.as_array()?.get(0)?;

        let parse_levels = |levels: &Value| -> Vec<OrderBookLevel> {
            levels.as_array()
                .map(|arr| {
                    arr.iter()
                        .filter_map(|level| {
                            Some(OrderBookLevel {
                                price: level.get("price")?.as_str()?.parse().ok()?,
                                quantity: level.get("qty")?.as_str()?.parse().ok()?,
                                count: None,
                            })
                        })
                        .collect()
                })
                .unwrap_or_default()
        };

        let bids = if let Some(b) = book_data.get("bids") {
            parse_levels(b)
        } else {
            vec![]
        };

        let asks = if let Some(a) = book_data.get("asks") {
            parse_levels(a)
        } else {
            vec![]
        };

        Some(OrderBookData {
            provider: "kraken".to_string(),
            symbol: Self::normalize_kraken_symbol(symbol),
            bids,
            asks,
            timestamp: Self::now(),
            is_snapshot,
        })
    }

    /// Parse Kraken trade message
    fn parse_trade(&self, data: &Value) -> Option<Vec<TradeData>> {
        let symbol = data.get("symbol")?.as_str()?;
        let trades = data.get("data")?.as_array()?;

        Some(
            trades
                .iter()
                .filter_map(|trade| {
                    let price = trade.get("price")?.as_str()?.parse().ok()?;
                    let qty = trade.get("qty")?.as_str()?.parse().ok()?;
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
                .collect(),
        )
    }

    /// Parse Kraken candle message
    fn parse_candle(&self, data: &Value) -> Option<CandleData> {
        let symbol = data.get("symbol")?.as_str()?;
        let candle_data = data.get("data")?.as_array()?.get(0)?;

        Some(CandleData {
            provider: "kraken".to_string(),
            symbol: Self::normalize_kraken_symbol(symbol),
            open: candle_data.get("open")?.as_str()?.parse().ok()?,
            high: candle_data.get("high")?.as_str()?.parse().ok()?,
            low: candle_data.get("low")?.as_str()?.parse().ok()?,
            close: candle_data.get("close")?.as_str()?.parse().ok()?,
            volume: candle_data.get("volume")?.as_str()?.parse().ok()?,
            timestamp: Self::now(),
            interval: candle_data.get("interval")?.as_str()?.to_string(),
        })
    }

    /// Handle incoming message
    async fn handle_message(&self, msg: Message) {
        if let Message::Text(text) = msg {
            if let Ok(data) = serde_json::from_str::<Value>(&text) {
                // Check message type
                if let Some(channel) = data.get("channel").and_then(|c| c.as_str()) {
                    let market_msg = match channel {
                        "ticker" => {
                            self.parse_ticker(&data).map(MarketMessage::Ticker)
                        }
                        "book" => {
                            // Check if snapshot or update
                            let is_snapshot = data.get("type")
                                .and_then(|t| t.as_str())
                                .map(|t| t == "snapshot")
                                .unwrap_or(false);

                            self.parse_orderbook(&data, is_snapshot).map(MarketMessage::OrderBook)
                        }
                        "trade" => {
                            if let Some(trades) = self.parse_trade(&data) {
                                // Emit each trade separately
                                if let Some(callback) = &self.message_callback {
                                    for trade in trades {
                                        callback(MarketMessage::Trade(trade));
                                    }
                                }
                                return;
                            }
                            None
                        }
                        "ohlc" => {
                            self.parse_candle(&data).map(MarketMessage::Candle)
                        }
                        _ => None,
                    };

                    if let Some(msg) = market_msg {
                        if let Some(callback) = &self.message_callback {
                            callback(msg);
                        }
                    }
                }
            }
        }
    }

    /// Message receiver loop
    async fn receive_loop(
        ws: Arc<RwLock<tokio_tungstenite::WebSocketStream<tokio_tungstenite::MaybeTlsStream<tokio::net::TcpStream>>>>,
        callback: Arc<Box<dyn Fn(MarketMessage) + Send + Sync>>,
        connected: Arc<RwLock<bool>>,
    ) {
        loop {
            let msg_result = {
                let mut ws_lock = ws.write().await;
                ws_lock.next().await
            };

            match msg_result {
                Some(Ok(msg)) => {
                    // Handle message in separate task to avoid blocking
                    let adapter = KrakenAdapter {
                        config: ProviderConfig::default(),
                        ws: None,
                        message_callback: Some(callback.clone()),
                        connected: connected.clone(),
                    };
                    tokio::spawn(async move {
                        adapter.handle_message(msg).await;
                    });
                }
                Some(Err(e)) => {
                    eprintln!("[Kraken] WebSocket error: {}", e);
                    *connected.write().await = false;
                    break;
                }
                None => {
                    eprintln!("[Kraken] WebSocket connection closed");
                    *connected.write().await = false;
                    break;
                }
            }
        }
    }
}

#[async_trait]
impl WebSocketAdapter for KrakenAdapter {
    async fn connect(&mut self) -> anyhow::Result<()> {
        let url = self.config.url.clone();
        let (ws_stream, _) = connect_async(if url.is_empty() { KRAKEN_WS_URL } else { &url }).await?;

        let ws = Arc::new(RwLock::new(ws_stream));
        self.ws = Some(ws.clone());
        *self.connected.write().await = true;

        // Start receive loop
        if let Some(callback) = self.message_callback.clone() {
            let connected = self.connected.clone();
            tokio::spawn(async move {
                Self::receive_loop(ws, callback, connected).await;
            });
        }

        Ok(())
    }

    async fn disconnect(&mut self) -> anyhow::Result<()> {
        *self.connected.write().await = false;

        if let Some(ws) = &self.ws {
            ws.write().await.close(None).await?;
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
        let ws = self.ws.as_ref()
            .ok_or_else(|| anyhow::anyhow!("Not connected"))?;

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
        ws.write().await.send(Message::Text(msg_str)).await?;

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
        // Use tokio runtime instead of futures executor
        tokio::task::block_in_place(|| {
            tokio::runtime::Handle::current().block_on(async {
                *self.connected.read().await
            })
        })
    }
}
