#![allow(dead_code)]
// Coinbase WebSocket Adapter
//
// Implements Coinbase Exchange WebSocket API
// Supports: ticker, level2 (orderbook), matches (trades) channels

use super::WebSocketAdapter;
use crate::websocket::types::*;
use async_trait::async_trait;
use futures_util::{SinkExt, StreamExt};
use serde_json::{json, Value};
use std::sync::Arc;
use std::time::{SystemTime, UNIX_EPOCH};
use tokio::sync::RwLock;
use tokio_tungstenite::{connect_async, tungstenite::Message};

const COINBASE_WS_URL: &str = "wss://ws-feed.exchange.coinbase.com";

pub struct CoinbaseAdapter {
    config: ProviderConfig,
    ws: Option<Arc<RwLock<tokio_tungstenite::WebSocketStream<tokio_tungstenite::MaybeTlsStream<tokio::net::TcpStream>>>>>,
    message_callback: Option<Arc<Box<dyn Fn(MarketMessage) + Send + Sync>>>,
    connected: Arc<RwLock<bool>>,
}

impl CoinbaseAdapter {
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

    /// Normalize Coinbase symbol (BTC-USD -> BTC/USD)
    fn normalize_coinbase_symbol(symbol: &str) -> String {
        symbol.replace('-', "/")
    }

    /// Convert symbol to Coinbase format (BTC/USD -> BTC-USD)
    fn to_coinbase_symbol(symbol: &str) -> String {
        symbol.replace('/', "-")
    }

    /// Parse Coinbase ticker
    fn parse_ticker(&self, data: &Value) -> Option<TickerData> {
        let product_id = data.get("product_id")?.as_str()?;
        let price = data.get("price")?.as_str()?.parse::<f64>().ok()?;
        let bid = data.get("best_bid")?.as_str().and_then(|s| s.parse::<f64>().ok());
        let ask = data.get("best_ask")?.as_str().and_then(|s| s.parse::<f64>().ok());
        let bid_sz = data.get("best_bid_size")?.as_str().and_then(|s| s.parse::<f64>().ok());
        let ask_sz = data.get("best_ask_size")?.as_str().and_then(|s| s.parse::<f64>().ok());
        let volume = data.get("volume_24h")?.as_str().and_then(|s| s.parse::<f64>().ok());
        let high = data.get("high_24h")?.as_str().and_then(|s| s.parse::<f64>().ok());
        let low = data.get("low_24h")?.as_str().and_then(|s| s.parse::<f64>().ok());
        let open = data.get("open_24h")?.as_str().and_then(|s| s.parse::<f64>().ok());

        let change_percent = if let (Some(o), Some(p)) = (open, Some(price)) {
            if o > 0.0 {
                Some((p - o) / o * 100.0)
            } else {
                None
            }
        } else {
            None
        };

        Some(TickerData {
            provider: "coinbase".to_string(),
            symbol: Self::normalize_coinbase_symbol(product_id),
            price,
            bid,
            ask,
            bid_size: bid_sz,
            ask_size: ask_sz,
            volume,
            high,
            low,
            open,
            close: Some(price),
            change: None,
            change_percent,
            quote_volume: None,
            timestamp: Self::now(),
        })
    }

    /// Parse Coinbase level2 snapshot (orderbook)
    fn parse_orderbook_snapshot(&self, data: &Value) -> Option<OrderBookData> {
        let product_id = data.get("product_id")?.as_str()?;
        let bids = data.get("bids")?.as_array()?;
        let asks = data.get("asks")?.as_array()?;

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
            provider: "coinbase".to_string(),
            symbol: Self::normalize_coinbase_symbol(product_id),
            bids: parse_levels(bids),
            asks: parse_levels(asks),
            timestamp: Self::now(),
            is_snapshot: true,
        })
    }

    /// Parse Coinbase match (trade)
    fn parse_trade(&self, data: &Value) -> Option<TradeData> {
        let product_id = data.get("product_id")?.as_str()?;
        let price = data.get("price")?.as_str()?.parse::<f64>().ok()?;
        let size = data.get("size")?.as_str()?.parse::<f64>().ok()?;
        let side = data.get("side")?.as_str()?;
        let trade_id = data.get("trade_id")?.as_u64().map(|t| t.to_string());

        Some(TradeData {
            provider: "coinbase".to_string(),
            symbol: Self::normalize_coinbase_symbol(product_id),
            trade_id,
            price,
            quantity: size,
            side: match side {
                "buy" => TradeSide::Buy,
                "sell" => TradeSide::Sell,
                _ => TradeSide::Unknown,
            },
            timestamp: Self::now(),
        })
    }
}

#[async_trait]
impl WebSocketAdapter for CoinbaseAdapter {
    async fn connect(&mut self) -> anyhow::Result<()> {
        let url = if self.config.url.is_empty() {
            COINBASE_WS_URL
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
                                let msg_type = data.get("type").and_then(|t| t.as_str()).unwrap_or("");
                                let adapter = CoinbaseAdapter::new(ProviderConfig::default());

                                let message = match msg_type {
                                    "ticker" => adapter.parse_ticker(&data).map(MarketMessage::Ticker),
                                    "snapshot" => adapter.parse_orderbook_snapshot(&data).map(MarketMessage::OrderBook),
                                    "match" | "last_match" => adapter.parse_trade(&data).map(MarketMessage::Trade),
                                    _ => None,
                                };

                                if let Some(msg) = message {
                                    cb(msg);
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
        let coinbase_symbol = Self::to_coinbase_symbol(symbol);

        let coinbase_channel = match channel {
            "ticker" => "ticker",
            "book" | "depth" => "level2",
            "trade" => "matches",
            _ => return Err(anyhow::anyhow!("Unsupported channel: {}", channel)),
        };

        let subscribe_msg = json!({
            "type": "subscribe",
            "product_ids": [coinbase_symbol],
            "channels": [coinbase_channel]
        });

        let mut ws_lock = ws.write().await;
        ws_lock.send(Message::Text(subscribe_msg.to_string())).await?;

        Ok(())
    }

    async fn unsubscribe(&mut self, symbol: &str, channel: &str) -> anyhow::Result<()> {
        let ws = self.ws.as_ref().ok_or_else(|| anyhow::anyhow!("Not connected"))?;
        let coinbase_symbol = Self::to_coinbase_symbol(symbol);

        let coinbase_channel = match channel {
            "ticker" => "ticker",
            "book" | "depth" => "level2",
            "trade" => "matches",
            _ => return Err(anyhow::anyhow!("Unsupported channel: {}", channel)),
        };

        let unsubscribe_msg = json!({
            "type": "unsubscribe",
            "product_ids": [coinbase_symbol],
            "channels": [coinbase_channel]
        });

        let mut ws_lock = ws.write().await;
        ws_lock.send(Message::Text(unsubscribe_msg.to_string())).await?;

        Ok(())
    }

    fn set_message_callback(&mut self, callback: Box<dyn Fn(MarketMessage) + Send + Sync>) {
        self.message_callback = Some(Arc::new(callback));
    }

    fn provider_name(&self) -> &str {
        "coinbase"
    }

    fn is_connected(&self) -> bool {
        *self.connected.blocking_read()
    }
}
