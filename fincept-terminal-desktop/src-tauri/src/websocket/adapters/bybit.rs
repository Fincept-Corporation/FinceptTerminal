#![allow(dead_code)]
// Bybit WebSocket Adapter
//
// Implements Bybit WebSocket API v5
// Supports: ticker, orderbook, trade, kline channels

use super::WebSocketAdapter;
use crate::websocket::types::*;
use async_trait::async_trait;
use futures_util::{SinkExt, StreamExt};
use serde_json::{json, Value};
use std::sync::Arc;
use std::time::{SystemTime, UNIX_EPOCH};
use tokio::sync::RwLock;
use tokio_tungstenite::{connect_async, tungstenite::Message};

const BYBIT_WS_URL: &str = "wss://stream.bybit.com/v5/public/spot";

pub struct BybitAdapter {
    config: ProviderConfig,
    ws: Option<Arc<RwLock<tokio_tungstenite::WebSocketStream<tokio_tungstenite::MaybeTlsStream<tokio::net::TcpStream>>>>>,
    message_callback: Option<Arc<Box<dyn Fn(MarketMessage) + Send + Sync>>>,
    connected: Arc<RwLock<bool>>,
}

impl BybitAdapter {
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
            .unwrap_or_else(|_| std::time::Duration::from_secs(0))
            .as_millis() as u64
    }

    /// Normalize Bybit symbol (BTCUSDT -> BTC/USDT)
    fn normalize_bybit_symbol(symbol: &str) -> String {
        for quote in ["USDT", "USDC", "USD", "BTC", "ETH"] {
            if let Some(base) = symbol.strip_suffix(quote) {
                return format!("{}/{}", base, quote);
            }
        }
        symbol.to_string()
    }

    /// Convert symbol to Bybit format (BTC/USDT -> BTCUSDT)
    fn to_bybit_symbol(symbol: &str) -> String {
        symbol.replace('/', "").to_uppercase()
    }

    /// Parse Bybit ticker
    fn parse_ticker(&self, data: &Value) -> Option<TickerData> {
        let symbol = data.get("symbol")?.as_str()?;
        let last_price = data.get("lastPrice")?.as_str()?.parse::<f64>().ok()?;
        let bid = data.get("bid1Price")?.as_str().and_then(|s| s.parse::<f64>().ok());
        let ask = data.get("ask1Price")?.as_str().and_then(|s| s.parse::<f64>().ok());
        let bid_sz = data.get("bid1Size")?.as_str().and_then(|s| s.parse::<f64>().ok());
        let ask_sz = data.get("ask1Size")?.as_str().and_then(|s| s.parse::<f64>().ok());
        let volume = data.get("volume24h")?.as_str().and_then(|s| s.parse::<f64>().ok());
        let high = data.get("highPrice24h")?.as_str().and_then(|s| s.parse::<f64>().ok());
        let low = data.get("lowPrice24h")?.as_str().and_then(|s| s.parse::<f64>().ok());
        let change_percent = data.get("price24hPcnt")?.as_str().and_then(|s| s.parse::<f64>().ok())
            .map(|p| p * 100.0);

        Some(TickerData {
            provider: "bybit".to_string(),
            symbol: Self::normalize_bybit_symbol(symbol),
            price: last_price,
            bid,
            ask,
            bid_size: bid_sz,
            ask_size: ask_sz,
            volume,
            high,
            low,
            open: None,
            close: Some(last_price),
            change: None,
            change_percent,
            quote_volume: data.get("turnover24h")?.as_str().and_then(|s| s.parse::<f64>().ok()),
            timestamp: Self::now(),
        })
    }

    /// Parse Bybit order book
    fn parse_orderbook(&self, data: &Value, symbol: &str) -> Option<OrderBookData> {
        let bids = data.get("b")?.as_array()?;
        let asks = data.get("a")?.as_array()?;

        let parse_levels = |levels: &Vec<Value>| -> Vec<OrderBookLevel> {
            levels
                .iter()
                .filter_map(|level| {
                    let arr = level.as_array()?;
                    Some(OrderBookLevel {
                        price: arr.get(0)?.as_str()?.parse::<f64>().ok()?,
                        quantity: arr.get(1)?.as_str()?.parse::<f64>().ok()?,
                        count: None,
                    })
                })
                .collect()
        };

        Some(OrderBookData {
            provider: "bybit".to_string(),
            symbol: Self::normalize_bybit_symbol(symbol),
            bids: parse_levels(bids),
            asks: parse_levels(asks),
            timestamp: data.get("ts")?.as_u64().unwrap_or_else(Self::now),
            is_snapshot: data.get("type")?.as_str() == Some("snapshot"),
        })
    }

    /// Parse Bybit trade
    fn parse_trade(&self, data: &Value) -> Option<TradeData> {
        let symbol = data.get("s")?.as_str()?;
        let price = data.get("p")?.as_str()?.parse::<f64>().ok()?;
        let quantity = data.get("v")?.as_str()?.parse::<f64>().ok()?;
        let side = data.get("S")?.as_str()?;
        let trade_id = data.get("i")?.as_str().map(|s| s.to_string());
        let ts = data.get("T")?.as_u64().unwrap_or_else(Self::now);

        Some(TradeData {
            provider: "bybit".to_string(),
            symbol: Self::normalize_bybit_symbol(symbol),
            trade_id,
            price,
            quantity,
            side: match side {
                "Buy" => TradeSide::Buy,
                "Sell" => TradeSide::Sell,
                _ => TradeSide::Unknown,
            },
            timestamp: ts,
        })
    }

    /// Parse Bybit kline/candle
    fn parse_candle(&self, data: &Value) -> Option<CandleData> {
        let symbol = data.get("symbol")?.as_str()?;
        let interval = data.get("interval")?.as_str()?;

        // Bybit sends candles as arrays
        let candle = data.get("list")?.as_array()?.first()?;
        let arr = candle.as_array()?;

        Some(CandleData {
            provider: "bybit".to_string(),
            symbol: Self::normalize_bybit_symbol(symbol),
            interval: interval.to_string(),
            open: arr.get(1)?.as_str()?.parse::<f64>().ok()?,
            high: arr.get(2)?.as_str()?.parse::<f64>().ok()?,
            low: arr.get(3)?.as_str()?.parse::<f64>().ok()?,
            close: arr.get(4)?.as_str()?.parse::<f64>().ok()?,
            volume: arr.get(5)?.as_str()?.parse::<f64>().ok()?,
            timestamp: arr.get(0)?.as_str()?.parse::<u64>().ok()?,
        })
    }
}

#[async_trait]
impl WebSocketAdapter for BybitAdapter {
    async fn connect(&mut self) -> anyhow::Result<()> {
        let url = if self.config.url.is_empty() {
            BYBIT_WS_URL
        } else {
            &self.config.url
        };

        let (ws_stream, _) = connect_async(url).await?;
        let ws = Arc::new(RwLock::new(ws_stream));
        self.ws = Some(ws.clone());
        *self.connected.write().await = true;

        let callback = self.message_callback.clone();
        let connected = self.connected.clone();

        tokio::spawn(async move {
            let mut ws_lock = ws.write().await;

            while let Some(msg) = ws_lock.next().await {
                match msg {
                    Ok(Message::Text(text)) => {
                        if let Ok(data) = serde_json::from_str::<Value>(&text) {
                            if let Some(ref cb) = callback {
                                // Check topic for message type
                                if let Some(topic) = data.get("topic").and_then(|t| t.as_str()) {
                                    if let Some(msg_data) = data.get("data") {
                                        let adapter = BybitAdapter::new(ProviderConfig::default());

                                        let message = if topic.starts_with("tickers.") {
                                            adapter.parse_ticker(msg_data).map(MarketMessage::Ticker)
                                        } else if topic.starts_with("orderbook.") {
                                            let symbol = topic.split('.').last().unwrap_or("");
                                            adapter.parse_orderbook(msg_data, symbol).map(MarketMessage::OrderBook)
                                        } else if topic.starts_with("publicTrade.") {
                                            // Trades come as an array
                                            if let Some(trades) = msg_data.as_array() {
                                                for trade in trades {
                                                    if let Some(t) = adapter.parse_trade(trade) {
                                                        cb(MarketMessage::Trade(t));
                                                    }
                                                }
                                            }
                                            None
                                        } else if topic.starts_with("kline.") {
                                            adapter.parse_candle(msg_data).map(MarketMessage::Candle)
                                        } else {
                                            None
                                        };

                                        if let Some(msg) = message {
                                            cb(msg);
                                        }
                                    }
                                }
                            }
                        }
                    }
                    Ok(Message::Ping(data)) => {
                        // Respond to ping
                        let mut ws_lock_inner = ws.write().await;
                        let _ = ws_lock_inner.send(Message::Pong(data)).await;
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
        Ok(())
    }

    async fn subscribe(
        &mut self,
        symbol: &str,
        channel: &str,
        params: Option<Value>,
    ) -> anyhow::Result<()> {
        let ws = self.ws.as_ref().ok_or_else(|| anyhow::anyhow!("Not connected"))?;
        let bybit_symbol = Self::to_bybit_symbol(symbol);

        let topic = match channel {
            "ticker" => format!("tickers.{}", bybit_symbol),
            "book" | "depth" => {
                let depth = params.as_ref()
                    .and_then(|p| p.get("depth"))
                    .and_then(|d| d.as_u64())
                    .unwrap_or(25);
                format!("orderbook.{}.{}", depth, bybit_symbol)
            }
            "trade" => format!("publicTrade.{}", bybit_symbol),
            "candle" | "ohlc" => {
                let interval = params.as_ref()
                    .and_then(|p| p.get("interval"))
                    .and_then(|i| i.as_str())
                    .unwrap_or("1");
                format!("kline.{}.{}", interval, bybit_symbol)
            }
            _ => return Err(anyhow::anyhow!("Unsupported channel: {}", channel)),
        };

        let subscribe_msg = json!({
            "op": "subscribe",
            "args": [topic]
        });

        let mut ws_lock = ws.write().await;
        ws_lock.send(Message::Text(subscribe_msg.to_string())).await?;

        Ok(())
    }

    async fn unsubscribe(&mut self, symbol: &str, channel: &str) -> anyhow::Result<()> {
        let ws = self.ws.as_ref().ok_or_else(|| anyhow::anyhow!("Not connected"))?;
        let bybit_symbol = Self::to_bybit_symbol(symbol);

        let topic = match channel {
            "ticker" => format!("tickers.{}", bybit_symbol),
            "book" | "depth" => format!("orderbook.25.{}", bybit_symbol),
            "trade" => format!("publicTrade.{}", bybit_symbol),
            "candle" | "ohlc" => format!("kline.1.{}", bybit_symbol),
            _ => return Err(anyhow::anyhow!("Unsupported channel: {}", channel)),
        };

        let unsubscribe_msg = json!({
            "op": "unsubscribe",
            "args": [topic]
        });

        let mut ws_lock = ws.write().await;
        ws_lock.send(Message::Text(unsubscribe_msg.to_string())).await?;

        Ok(())
    }

    fn set_message_callback(&mut self, callback: Box<dyn Fn(MarketMessage) + Send + Sync>) {
        self.message_callback = Some(Arc::new(callback));
    }

    fn provider_name(&self) -> &str {
        "bybit"
    }

    fn is_connected(&self) -> bool {
        *self.connected.blocking_read()
    }
}
