#![allow(dead_code)]
// KuCoin WebSocket Adapter
//
// Implements KuCoin WebSocket API
// Note: KuCoin requires a token from REST API before connecting
// Supports: ticker, level2 (orderbook), match (trades) channels

use super::WebSocketAdapter;
use crate::websocket::types::*;
use async_trait::async_trait;
use futures_util::{SinkExt, StreamExt};
use serde_json::{json, Value};
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::Arc;
use std::time::{SystemTime, UNIX_EPOCH};
use tokio::sync::RwLock;
use tokio_tungstenite::{connect_async, tungstenite::Message};

const KUCOIN_WS_URL: &str = "wss://ws-api-spot.kucoin.com";

static MSG_ID: AtomicU64 = AtomicU64::new(1);

pub struct KucoinAdapter {
    config: ProviderConfig,
    ws: Option<Arc<RwLock<tokio_tungstenite::WebSocketStream<tokio_tungstenite::MaybeTlsStream<tokio::net::TcpStream>>>>>,
    message_callback: Option<Arc<Box<dyn Fn(MarketMessage) + Send + Sync>>>,
    connected: Arc<RwLock<bool>>,
}

impl KucoinAdapter {
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

    fn next_id() -> u64 {
        MSG_ID.fetch_add(1, Ordering::SeqCst)
    }

    /// Normalize KuCoin symbol (BTC-USDT -> BTC/USDT)
    fn normalize_kucoin_symbol(symbol: &str) -> String {
        symbol.replace('-', "/")
    }

    /// Convert symbol to KuCoin format (BTC/USDT -> BTC-USDT)
    fn to_kucoin_symbol(symbol: &str) -> String {
        symbol.replace('/', "-")
    }

    /// Parse KuCoin ticker
    fn parse_ticker(&self, data: &Value) -> Option<TickerData> {
        let symbol = data.get("symbol")?.as_str()?;
        let price = data.get("price")?.as_str()?.parse::<f64>().ok()?;
        let bid = data.get("bestBid")?.as_str().and_then(|s| s.parse::<f64>().ok());
        let ask = data.get("bestAsk")?.as_str().and_then(|s| s.parse::<f64>().ok());
        let bid_sz = data.get("bestBidSize")?.as_str().and_then(|s| s.parse::<f64>().ok());
        let ask_sz = data.get("bestAskSize")?.as_str().and_then(|s| s.parse::<f64>().ok());

        Some(TickerData {
            provider: "kucoin".to_string(),
            symbol: Self::normalize_kucoin_symbol(symbol),
            price,
            bid,
            ask,
            bid_size: bid_sz,
            ask_size: ask_sz,
            volume: None,
            high: None,
            low: None,
            open: None,
            close: Some(price),
            change: None,
            change_percent: None,
            quote_volume: None,
            timestamp: Self::now(),
        })
    }

    /// Parse KuCoin order book
    fn parse_orderbook(&self, data: &Value, symbol: &str) -> Option<OrderBookData> {
        let changes = data.get("changes")?;
        let bids = changes.get("bids")?.as_array()?;
        let asks = changes.get("asks")?.as_array()?;

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
            provider: "kucoin".to_string(),
            symbol: Self::normalize_kucoin_symbol(symbol),
            bids: parse_levels(bids),
            asks: parse_levels(asks),
            timestamp: Self::now(),
            is_snapshot: false,
        })
    }

    /// Parse KuCoin trade
    fn parse_trade(&self, data: &Value) -> Option<TradeData> {
        let symbol = data.get("symbol")?.as_str()?;
        let price = data.get("price")?.as_str()?.parse::<f64>().ok()?;
        let size = data.get("size")?.as_str()?.parse::<f64>().ok()?;
        let side = data.get("side")?.as_str()?;
        let trade_id = data.get("tradeId")?.as_str().map(|s| s.to_string());
        let ts = data.get("time")?.as_str()?.parse::<u64>().ok().unwrap_or_else(Self::now);

        Some(TradeData {
            provider: "kucoin".to_string(),
            symbol: Self::normalize_kucoin_symbol(symbol),
            trade_id,
            price,
            quantity: size,
            side: match side {
                "buy" => TradeSide::Buy,
                "sell" => TradeSide::Sell,
                _ => TradeSide::Unknown,
            },
            timestamp: ts,
        })
    }

    /// Parse KuCoin candle
    fn parse_candle(&self, data: &Value, symbol: &str) -> Option<CandleData> {
        let candles = data.get("candles")?.as_array()?;

        Some(CandleData {
            provider: "kucoin".to_string(),
            symbol: Self::normalize_kucoin_symbol(symbol),
            interval: "1min".to_string(),
            open: candles.get(1)?.as_str()?.parse::<f64>().ok()?,
            high: candles.get(3)?.as_str()?.parse::<f64>().ok()?,
            low: candles.get(4)?.as_str()?.parse::<f64>().ok()?,
            close: candles.get(2)?.as_str()?.parse::<f64>().ok()?,
            volume: candles.get(5)?.as_str()?.parse::<f64>().ok()?,
            timestamp: candles.get(0)?.as_str()?.parse::<u64>().ok()?,
        })
    }
}

#[async_trait]
impl WebSocketAdapter for KucoinAdapter {
    async fn connect(&mut self) -> anyhow::Result<()> {
        let url = if self.config.url.is_empty() {
            KUCOIN_WS_URL
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
                            // Handle ping/pong
                            if data.get("type").and_then(|t| t.as_str()) == Some("ping") {
                                let pong = json!({
                                    "id": Self::next_id().to_string(),
                                    "type": "pong"
                                });
                                let _ = ws_lock.send(Message::Text(pong.to_string())).await;
                                continue;
                            }

                            if let Some(ref cb) = callback {
                                if let Some(topic) = data.get("topic").and_then(|t| t.as_str()) {
                                    if let Some(msg_data) = data.get("data") {
                                        let adapter = KucoinAdapter::new(ProviderConfig::default());
                                        let symbol = data.get("subject").and_then(|s| s.as_str()).unwrap_or("");

                                        let message = if topic.starts_with("/market/ticker:") {
                                            adapter.parse_ticker(msg_data).map(MarketMessage::Ticker)
                                        } else if topic.starts_with("/market/level2:") {
                                            adapter.parse_orderbook(msg_data, symbol).map(MarketMessage::OrderBook)
                                        } else if topic.starts_with("/market/match:") {
                                            adapter.parse_trade(msg_data).map(MarketMessage::Trade)
                                        } else if topic.starts_with("/market/candles:") {
                                            adapter.parse_candle(msg_data, symbol).map(MarketMessage::Candle)
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
        _params: Option<Value>,
    ) -> anyhow::Result<()> {
        let ws = self.ws.as_ref().ok_or_else(|| anyhow::anyhow!("Not connected"))?;
        let kucoin_symbol = Self::to_kucoin_symbol(symbol);

        let topic = match channel {
            "ticker" => format!("/market/ticker:{}", kucoin_symbol),
            "book" | "depth" => format!("/market/level2:{}", kucoin_symbol),
            "trade" => format!("/market/match:{}", kucoin_symbol),
            "candle" | "ohlc" => format!("/market/candles:{}_1min", kucoin_symbol),
            _ => return Err(anyhow::anyhow!("Unsupported channel: {}", channel)),
        };

        let subscribe_msg = json!({
            "id": Self::next_id().to_string(),
            "type": "subscribe",
            "topic": topic,
            "privateChannel": false,
            "response": true
        });

        let mut ws_lock = ws.write().await;
        ws_lock.send(Message::Text(subscribe_msg.to_string())).await?;

        Ok(())
    }

    async fn unsubscribe(&mut self, symbol: &str, channel: &str) -> anyhow::Result<()> {
        let ws = self.ws.as_ref().ok_or_else(|| anyhow::anyhow!("Not connected"))?;
        let kucoin_symbol = Self::to_kucoin_symbol(symbol);

        let topic = match channel {
            "ticker" => format!("/market/ticker:{}", kucoin_symbol),
            "book" | "depth" => format!("/market/level2:{}", kucoin_symbol),
            "trade" => format!("/market/match:{}", kucoin_symbol),
            "candle" | "ohlc" => format!("/market/candles:{}_1min", kucoin_symbol),
            _ => return Err(anyhow::anyhow!("Unsupported channel: {}", channel)),
        };

        let unsubscribe_msg = json!({
            "id": Self::next_id().to_string(),
            "type": "unsubscribe",
            "topic": topic,
            "privateChannel": false,
            "response": true
        });

        let mut ws_lock = ws.write().await;
        ws_lock.send(Message::Text(unsubscribe_msg.to_string())).await?;

        Ok(())
    }

    fn set_message_callback(&mut self, callback: Box<dyn Fn(MarketMessage) + Send + Sync>) {
        self.message_callback = Some(Arc::new(callback));
    }

    fn provider_name(&self) -> &str {
        "kucoin"
    }

    fn is_connected(&self) -> bool {
        *self.connected.blocking_read()
    }
}
