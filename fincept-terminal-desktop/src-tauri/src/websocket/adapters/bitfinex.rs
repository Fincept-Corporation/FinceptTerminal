#![allow(dead_code)]
// Bitfinex WebSocket Adapter
//
// Implements Bitfinex WebSocket API v2
// Supports: ticker, book, trades channels

use super::WebSocketAdapter;
use crate::websocket::types::*;
use async_trait::async_trait;
use futures_util::{SinkExt, StreamExt};
use serde_json::{json, Value};
use std::collections::HashMap;
use std::sync::Arc;
use std::time::{SystemTime, UNIX_EPOCH};
use tokio::sync::RwLock;
use tokio_tungstenite::{connect_async, tungstenite::Message};

const BITFINEX_WS_URL: &str = "wss://api-pub.bitfinex.com/ws/2";

pub struct BitfinexAdapter {
    config: ProviderConfig,
    ws: Option<Arc<RwLock<tokio_tungstenite::WebSocketStream<tokio_tungstenite::MaybeTlsStream<tokio::net::TcpStream>>>>>,
    message_callback: Option<Arc<Box<dyn Fn(MarketMessage) + Send + Sync>>>,
    connected: Arc<RwLock<bool>>,
    // Map channel ID to (symbol, channel_type)
    channel_map: Arc<RwLock<HashMap<u64, (String, String)>>>,
}

impl BitfinexAdapter {
    pub fn new(config: ProviderConfig) -> Self {
        Self {
            config,
            ws: None,
            message_callback: None,
            connected: Arc::new(RwLock::new(false)),
            channel_map: Arc::new(RwLock::new(HashMap::new())),
        }
    }

    fn now() -> u64 {
        SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .unwrap_or_else(|_| std::time::Duration::from_secs(0))
            .as_millis() as u64
    }

    /// Normalize Bitfinex symbol (tBTCUSD -> BTC/USD)
    fn normalize_bitfinex_symbol(symbol: &str) -> String {
        let s = symbol.strip_prefix('t').unwrap_or(symbol);
        // Handle pairs like BTCUSD, ETHBTC, etc.
        if s.len() >= 6 {
            // Check for 3-char quote currencies
            for quote in ["USD", "UST", "BTC", "ETH", "EUR", "GBP", "JPY"] {
                if s.ends_with(quote) {
                    let base = &s[..s.len() - quote.len()];
                    return format!("{}/{}", base, quote);
                }
            }
        }
        s.to_string()
    }

    /// Convert symbol to Bitfinex format (BTC/USD -> tBTCUSD)
    fn to_bitfinex_symbol(symbol: &str) -> String {
        format!("t{}", symbol.replace('/', ""))
    }

    /// Parse Bitfinex ticker (from array format)
    fn parse_ticker(&self, data: &Value, symbol: &str) -> Option<TickerData> {
        let arr = data.as_array()?;
        if arr.len() < 10 {
            return None;
        }

        let bid = arr.get(0)?.as_f64()?;
        let bid_size = arr.get(1)?.as_f64();
        let ask = arr.get(2)?.as_f64()?;
        let ask_size = arr.get(3)?.as_f64();
        let daily_change = arr.get(4)?.as_f64();
        let daily_change_perc = arr.get(5)?.as_f64().map(|p| p * 100.0);
        let last_price = arr.get(6)?.as_f64()?;
        let volume = arr.get(7)?.as_f64();
        let high = arr.get(8)?.as_f64();
        let low = arr.get(9)?.as_f64();

        Some(TickerData {
            provider: "bitfinex".to_string(),
            symbol: Self::normalize_bitfinex_symbol(symbol),
            price: last_price,
            bid: Some(bid),
            ask: Some(ask),
            bid_size,
            ask_size,
            volume,
            high,
            low,
            open: None,
            close: Some(last_price),
            change: daily_change,
            change_percent: daily_change_perc,
            quote_volume: None,
            timestamp: Self::now(),
        })
    }

    /// Parse Bitfinex order book (snapshot or update)
    fn parse_orderbook(&self, data: &Value, symbol: &str, is_snapshot: bool) -> Option<OrderBookData> {
        let mut bids = Vec::new();
        let mut asks = Vec::new();

        if is_snapshot {
            // Snapshot: array of [price, count, amount]
            if let Some(arr) = data.as_array() {
                for entry in arr {
                    if let Some(entry_arr) = entry.as_array() {
                        if entry_arr.len() >= 3 {
                            let price = entry_arr.get(0)?.as_f64()?;
                            let count = entry_arr.get(1)?.as_i64()? as u32;
                            let amount = entry_arr.get(2)?.as_f64()?;

                            let level = OrderBookLevel {
                                price,
                                quantity: amount.abs(),
                                count: Some(count),
                            };

                            if amount > 0.0 {
                                bids.push(level);
                            } else {
                                asks.push(level);
                            }
                        }
                    }
                }
            }
        } else {
            // Update: single [price, count, amount]
            if let Some(arr) = data.as_array() {
                if arr.len() >= 3 {
                    let price = arr.get(0)?.as_f64()?;
                    let count = arr.get(1)?.as_i64()? as u32;
                    let amount = arr.get(2)?.as_f64()?;

                    let level = OrderBookLevel {
                        price,
                        quantity: amount.abs(),
                        count: Some(count),
                    };

                    if amount > 0.0 {
                        bids.push(level);
                    } else {
                        asks.push(level);
                    }
                }
            }
        }

        Some(OrderBookData {
            provider: "bitfinex".to_string(),
            symbol: Self::normalize_bitfinex_symbol(symbol),
            bids,
            asks,
            timestamp: Self::now(),
            is_snapshot,
        })
    }

    /// Parse Bitfinex trade
    fn parse_trade(&self, data: &Value, symbol: &str) -> Option<TradeData> {
        let arr = data.as_array()?;
        if arr.len() < 4 {
            return None;
        }

        // Format: [ID, MTS, AMOUNT, PRICE] or [ID, MTS, AMOUNT, PRICE] in tu
        let trade_id = arr.get(0)?.as_u64().map(|i| i.to_string());
        let timestamp = arr.get(1)?.as_u64().unwrap_or_else(Self::now);
        let amount = arr.get(2)?.as_f64()?;
        let price = arr.get(3)?.as_f64()?;

        Some(TradeData {
            provider: "bitfinex".to_string(),
            symbol: Self::normalize_bitfinex_symbol(symbol),
            trade_id,
            price,
            quantity: amount.abs(),
            side: if amount > 0.0 { TradeSide::Buy } else { TradeSide::Sell },
            timestamp,
        })
    }
}

#[async_trait]
impl WebSocketAdapter for BitfinexAdapter {
    async fn connect(&mut self) -> anyhow::Result<()> {
        let url = if self.config.url.is_empty() {
            BITFINEX_WS_URL
        } else {
            &self.config.url
        };

        let (ws_stream, _) = connect_async(url).await?;
        let ws = Arc::new(RwLock::new(ws_stream));
        self.ws = Some(ws.clone());
        *self.connected.write().await = true;

        let callback = self.message_callback.clone();
        let connected = self.connected.clone();
        let channel_map = self.channel_map.clone();

        tokio::spawn(async move {
            let mut ws_lock = ws.write().await;

            while let Some(msg) = ws_lock.next().await {
                match msg {
                    Ok(Message::Text(text)) => {
                        if let Ok(data) = serde_json::from_str::<Value>(&text) {
                            // Handle info/subscribed events
                            if let Some(event) = data.get("event").and_then(|e| e.as_str()) {
                                if event == "subscribed" {
                                    let chan_id = data.get("chanId").and_then(|c| c.as_u64()).unwrap_or(0);
                                    let symbol = data.get("symbol").or_else(|| data.get("pair"))
                                        .and_then(|s| s.as_str()).unwrap_or("").to_string();
                                    let channel = data.get("channel").and_then(|c| c.as_str()).unwrap_or("").to_string();

                                    let mut map = channel_map.write().await;
                                    map.insert(chan_id, (symbol, channel));
                                }
                                continue;
                            }

                            // Handle data messages (array format)
                            if let Some(arr) = data.as_array() {
                                if arr.len() >= 2 {
                                    let chan_id = arr.get(0).and_then(|c| c.as_u64()).unwrap_or(0);

                                    // Skip heartbeat
                                    if arr.get(1).and_then(|h| h.as_str()) == Some("hb") {
                                        continue;
                                    }

                                    if let Some(ref cb) = callback {
                                        let map = channel_map.read().await;
                                        if let Some((symbol, channel)) = map.get(&chan_id) {
                                            let adapter = BitfinexAdapter::new(ProviderConfig::default());
                                            let payload = &arr[1];

                                            let message = match channel.as_str() {
                                                "ticker" => adapter.parse_ticker(payload, symbol).map(MarketMessage::Ticker),
                                                "book" => {
                                                    let is_snapshot = payload.as_array()
                                                        .map(|a| a.first().and_then(|f| f.as_array()).is_some())
                                                        .unwrap_or(false);
                                                    adapter.parse_orderbook(payload, symbol, is_snapshot).map(MarketMessage::OrderBook)
                                                }
                                                "trades" => {
                                                    // Handle both snapshot and updates
                                                    if let Some(trade_data) = payload.as_array() {
                                                        // Check if it's a snapshot (array of trades) or single trade
                                                        if trade_data.first().and_then(|f| f.as_array()).is_some() {
                                                            // Snapshot - emit each trade
                                                            for trade in trade_data {
                                                                if let Some(t) = adapter.parse_trade(trade, symbol) {
                                                                    cb(MarketMessage::Trade(t));
                                                                }
                                                            }
                                                            None
                                                        } else {
                                                            adapter.parse_trade(payload, symbol).map(MarketMessage::Trade)
                                                        }
                                                    } else {
                                                        None
                                                    }
                                                }
                                                _ => None,
                                            };

                                            if let Some(msg) = message {
                                                cb(msg);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    Ok(Message::Close(_)) => {
                        *connected.write().await = false;
                        break;
                    }
                    Err(_) => {
                        *connected.write().await = false;
                        break;
                    }
                    _ => {}
                }
            }
        });

        Ok(())
    }

    async fn disconnect(&mut self) -> anyhow::Result<()> {
        if let Some(ws) = &self.ws {
            let mut ws_lock = ws.write().await;
            ws_lock.close(None).await?;
        }
        *self.connected.write().await = false;
        self.ws = None;
        self.channel_map.write().await.clear();
        Ok(())
    }

    async fn subscribe(
        &mut self,
        symbol: &str,
        channel: &str,
        _params: Option<Value>,
    ) -> anyhow::Result<()> {
        let ws = self.ws.as_ref().ok_or_else(|| anyhow::anyhow!("Not connected"))?;
        let bitfinex_symbol = Self::to_bitfinex_symbol(symbol);

        let bitfinex_channel = match channel {
            "ticker" => "ticker",
            "book" | "depth" => "book",
            "trade" => "trades",
            _ => return Err(anyhow::anyhow!("Unsupported channel: {}", channel)),
        };

        let mut subscribe_msg = json!({
            "event": "subscribe",
            "channel": bitfinex_channel,
            "symbol": bitfinex_symbol
        });

        // Add precision for orderbook
        if bitfinex_channel == "book" {
            subscribe_msg["prec"] = json!("P0");
            subscribe_msg["len"] = json!("25");
        }

        let mut ws_lock = ws.write().await;
        ws_lock.send(Message::Text(subscribe_msg.to_string())).await?;

        Ok(())
    }

    async fn unsubscribe(&mut self, symbol: &str, channel: &str) -> anyhow::Result<()> {
        let ws = self.ws.as_ref().ok_or_else(|| anyhow::anyhow!("Not connected"))?;
        let bitfinex_symbol = Self::to_bitfinex_symbol(symbol);

        // Find channel ID
        let map = self.channel_map.read().await;
        let chan_id = map.iter()
            .find(|(_, (s, c))| {
                s == &bitfinex_symbol && match channel {
                    "ticker" => c == "ticker",
                    "book" | "depth" => c == "book",
                    "trade" => c == "trades",
                    _ => false,
                }
            })
            .map(|(id, _)| *id);
        drop(map);

        if let Some(id) = chan_id {
            let unsubscribe_msg = json!({
                "event": "unsubscribe",
                "chanId": id
            });

            let mut ws_lock = ws.write().await;
            ws_lock.send(Message::Text(unsubscribe_msg.to_string())).await?;

            self.channel_map.write().await.remove(&id);
        }

        Ok(())
    }

    fn set_message_callback(&mut self, callback: Box<dyn Fn(MarketMessage) + Send + Sync>) {
        self.message_callback = Some(Arc::new(callback));
    }

    fn provider_name(&self) -> &str {
        "bitfinex"
    }

    fn is_connected(&self) -> bool {
        *self.connected.blocking_read()
    }
}
