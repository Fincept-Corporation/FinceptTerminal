#![allow(dead_code)]
// Bitget WebSocket Adapter
//
// Implements Bitget WebSocket API v2
// Supports: ticker, books, trades channels

use super::WebSocketAdapter;
use crate::websocket::types::*;
use async_trait::async_trait;
use futures_util::{SinkExt, StreamExt};
use serde_json::{json, Value};
use std::sync::Arc;
use std::time::{SystemTime, UNIX_EPOCH};
use tokio::sync::RwLock;
use tokio_tungstenite::{connect_async, tungstenite::Message};

const BITGET_WS_URL: &str = "wss://ws.bitget.com/v2/ws/public";

pub struct BitgetAdapter {
    config: ProviderConfig,
    ws: Option<Arc<RwLock<tokio_tungstenite::WebSocketStream<tokio_tungstenite::MaybeTlsStream<tokio::net::TcpStream>>>>>,
    message_callback: Option<Arc<Box<dyn Fn(MarketMessage) + Send + Sync>>>,
    connected: Arc<RwLock<bool>>,
}

impl BitgetAdapter {
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

    /// Normalize Bitget symbol (BTCUSDT -> BTC/USDT)
    fn normalize_bitget_symbol(symbol: &str) -> String {
        for quote in ["USDT", "USDC", "USD", "BTC", "ETH"] {
            if let Some(base) = symbol.strip_suffix(quote) {
                return format!("{}/{}", base, quote);
            }
        }
        symbol.to_string()
    }

    /// Convert symbol to Bitget format (BTC/USDT -> BTCUSDT)
    fn to_bitget_symbol(symbol: &str) -> String {
        symbol.replace('/', "").to_uppercase()
    }

    /// Parse Bitget ticker
    fn parse_ticker(&self, data: &Value) -> Option<TickerData> {
        let inst_id = data.get("instId")?.as_str()?;
        let last = data.get("lastPr")?.as_str()?.parse::<f64>().ok()?;
        let bid = data.get("bidPr")?.as_str().and_then(|s| s.parse::<f64>().ok());
        let ask = data.get("askPr")?.as_str().and_then(|s| s.parse::<f64>().ok());
        let bid_sz = data.get("bidSz")?.as_str().and_then(|s| s.parse::<f64>().ok());
        let ask_sz = data.get("askSz")?.as_str().and_then(|s| s.parse::<f64>().ok());
        let volume = data.get("baseVolume")?.as_str().and_then(|s| s.parse::<f64>().ok());
        let high = data.get("high24h")?.as_str().and_then(|s| s.parse::<f64>().ok());
        let low = data.get("low24h")?.as_str().and_then(|s| s.parse::<f64>().ok());
        let open = data.get("open24h")?.as_str().and_then(|s| s.parse::<f64>().ok());
        let change_percent = data.get("change24h")?.as_str().and_then(|s| s.parse::<f64>().ok())
            .map(|c| c * 100.0);

        Some(TickerData {
            provider: "bitget".to_string(),
            symbol: Self::normalize_bitget_symbol(inst_id),
            price: last,
            bid,
            ask,
            bid_size: bid_sz,
            ask_size: ask_sz,
            volume,
            high,
            low,
            open,
            close: Some(last),
            change: None,
            change_percent,
            quote_volume: data.get("quoteVolume")?.as_str().and_then(|s| s.parse::<f64>().ok()),
            timestamp: data.get("ts")?.as_str()?.parse::<u64>().ok().unwrap_or_else(Self::now),
        })
    }

    /// Parse Bitget order book
    fn parse_orderbook(&self, data: &Value, inst_id: &str) -> Option<OrderBookData> {
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
            provider: "bitget".to_string(),
            symbol: Self::normalize_bitget_symbol(inst_id),
            bids: parse_levels(bids),
            asks: parse_levels(asks),
            timestamp: data.get("ts")?.as_str()?.parse::<u64>().ok().unwrap_or_else(Self::now),
            is_snapshot: true,
        })
    }

    /// Parse Bitget trade
    fn parse_trade(&self, data: &Value) -> Option<TradeData> {
        let inst_id = data.get("instId")?.as_str()?;
        let price = data.get("price")?.as_str()?.parse::<f64>().ok()?;
        let size = data.get("size")?.as_str()?.parse::<f64>().ok()?;
        let side = data.get("side")?.as_str()?;
        let trade_id = data.get("tradeId")?.as_str().map(|s| s.to_string());
        let ts = data.get("ts")?.as_str()?.parse::<u64>().ok().unwrap_or_else(Self::now);

        Some(TradeData {
            provider: "bitget".to_string(),
            symbol: Self::normalize_bitget_symbol(inst_id),
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
}

#[async_trait]
impl WebSocketAdapter for BitgetAdapter {
    async fn connect(&mut self) -> anyhow::Result<()> {
        let url = if self.config.url.is_empty() {
            BITGET_WS_URL
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
                            // Handle ping
                            if text == "ping" {
                                let _ = ws_lock.send(Message::Text("pong".to_string())).await;
                                continue;
                            }

                            if let Some(ref cb) = callback {
                                if let Some(arg) = data.get("arg") {
                                    let channel = arg.get("channel").and_then(|c| c.as_str()).unwrap_or("");
                                    let inst_id = arg.get("instId").and_then(|i| i.as_str()).unwrap_or("");

                                    if let Some(data_arr) = data.get("data").and_then(|d| d.as_array()) {
                                        for item in data_arr {
                                            let adapter = BitgetAdapter::new(ProviderConfig::default());

                                            let message = match channel {
                                                "ticker" => adapter.parse_ticker(item).map(MarketMessage::Ticker),
                                                "books5" | "books15" => {
                                                    adapter.parse_orderbook(item, inst_id).map(MarketMessage::OrderBook)
                                                }
                                                "trade" => adapter.parse_trade(item).map(MarketMessage::Trade),
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
                    Ok(Message::Ping(data)) => {
                        let _ = ws_lock.send(Message::Pong(data)).await;
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
        let bitget_symbol = Self::to_bitget_symbol(symbol);

        let bitget_channel = match channel {
            "ticker" => "ticker",
            "book" | "depth" => "books5",
            "trade" => "trade",
            _ => return Err(anyhow::anyhow!("Unsupported channel: {}", channel)),
        };

        let subscribe_msg = json!({
            "op": "subscribe",
            "args": [{
                "instType": "SPOT",
                "channel": bitget_channel,
                "instId": bitget_symbol
            }]
        });

        let mut ws_lock = ws.write().await;
        ws_lock.send(Message::Text(subscribe_msg.to_string())).await?;

        Ok(())
    }

    async fn unsubscribe(&mut self, symbol: &str, channel: &str) -> anyhow::Result<()> {
        let ws = self.ws.as_ref().ok_or_else(|| anyhow::anyhow!("Not connected"))?;
        let bitget_symbol = Self::to_bitget_symbol(symbol);

        let bitget_channel = match channel {
            "ticker" => "ticker",
            "book" | "depth" => "books5",
            "trade" => "trade",
            _ => return Err(anyhow::anyhow!("Unsupported channel: {}", channel)),
        };

        let unsubscribe_msg = json!({
            "op": "unsubscribe",
            "args": [{
                "instType": "SPOT",
                "channel": bitget_channel,
                "instId": bitget_symbol
            }]
        });

        let mut ws_lock = ws.write().await;
        ws_lock.send(Message::Text(unsubscribe_msg.to_string())).await?;

        Ok(())
    }

    fn set_message_callback(&mut self, callback: Box<dyn Fn(MarketMessage) + Send + Sync>) {
        self.message_callback = Some(Arc::new(callback));
    }

    fn provider_name(&self) -> &str {
        "bitget"
    }

    fn is_connected(&self) -> bool {
        *self.connected.blocking_read()
    }
}
