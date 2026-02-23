#![allow(dead_code)]
// MEXC WebSocket Adapter
//
// Implements MEXC WebSocket API
// Supports: ticker, depth, trades channels

use super::WebSocketAdapter;
use crate::websocket::types::*;
use async_trait::async_trait;
use futures_util::{SinkExt, StreamExt};
use serde_json::{json, Value};
use std::sync::Arc;
use std::time::{SystemTime, UNIX_EPOCH};
use tokio::sync::RwLock;
use tokio_tungstenite::{connect_async, tungstenite::Message};

const MEXC_WS_URL: &str = "wss://wbs.mexc.com/ws";

pub struct MexcAdapter {
    config: ProviderConfig,
    ws: Option<Arc<RwLock<tokio_tungstenite::WebSocketStream<tokio_tungstenite::MaybeTlsStream<tokio::net::TcpStream>>>>>,
    message_callback: Option<Arc<Box<dyn Fn(MarketMessage) + Send + Sync>>>,
    connected: Arc<RwLock<bool>>,
}

impl MexcAdapter {
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

    /// Normalize MEXC symbol (BTCUSDT -> BTC/USDT)
    fn normalize_mexc_symbol(symbol: &str) -> String {
        for quote in ["USDT", "USDC", "USD", "BTC", "ETH"] {
            if let Some(base) = symbol.strip_suffix(quote) {
                return format!("{}/{}", base, quote);
            }
        }
        symbol.to_string()
    }

    /// Convert symbol to MEXC format (BTC/USDT -> BTCUSDT)
    fn to_mexc_symbol(symbol: &str) -> String {
        symbol.replace('/', "").to_uppercase()
    }

    /// Parse MEXC ticker
    fn parse_ticker(&self, data: &Value) -> Option<TickerData> {
        let d = data.get("d")?;
        let symbol = data.get("s")?.as_str()?;
        let last = d.get("p")?.as_str()?.parse::<f64>().ok()?;
        let volume = d.get("v")?.as_str().and_then(|s| s.parse::<f64>().ok());
        let high = d.get("h")?.as_str().and_then(|s| s.parse::<f64>().ok());
        let low = d.get("l")?.as_str().and_then(|s| s.parse::<f64>().ok());
        let open = d.get("o")?.as_str().and_then(|s| s.parse::<f64>().ok());
        let change_percent = d.get("r")?.as_str().and_then(|s| s.parse::<f64>().ok())
            .map(|r| r * 100.0);

        Some(TickerData {
            provider: "mexc".to_string(),
            symbol: Self::normalize_mexc_symbol(symbol),
            price: last,
            bid: None,
            ask: None,
            bid_size: None,
            ask_size: None,
            volume,
            high,
            low,
            open,
            close: Some(last),
            change: None,
            change_percent,
            quote_volume: None,
            timestamp: Self::now(),
        })
    }

    /// Parse MEXC order book
    fn parse_orderbook(&self, data: &Value) -> Option<OrderBookData> {
        let d = data.get("d")?;
        let symbol = data.get("s")?.as_str()?;
        let bids = d.get("bids")?.as_array()?;
        let asks = d.get("asks")?.as_array()?;

        let parse_levels = |levels: &Vec<Value>| -> Vec<OrderBookLevel> {
            levels
                .iter()
                .filter_map(|level| {
                    let price = level.get("p")?.as_str()?.parse::<f64>().ok()?;
                    let quantity = level.get("v")?.as_str()?.parse::<f64>().ok()?;
                    Some(OrderBookLevel {
                        price,
                        quantity,
                        count: None,
                    })
                })
                .collect()
        };

        Some(OrderBookData {
            provider: "mexc".to_string(),
            symbol: Self::normalize_mexc_symbol(symbol),
            bids: parse_levels(bids),
            asks: parse_levels(asks),
            timestamp: Self::now(),
            is_snapshot: true,
        })
    }

    /// Parse MEXC trade
    fn parse_trade(&self, data: &Value) -> Option<TradeData> {
        let d = data.get("d")?;
        let deals = d.get("deals")?.as_array()?.first()?;
        let symbol = data.get("s")?.as_str()?;
        let price = deals.get("p")?.as_str()?.parse::<f64>().ok()?;
        let quantity = deals.get("v")?.as_str()?.parse::<f64>().ok()?;
        let side = deals.get("S")?.as_i64()?;
        let ts = deals.get("t")?.as_u64().unwrap_or_else(Self::now);

        Some(TradeData {
            provider: "mexc".to_string(),
            symbol: Self::normalize_mexc_symbol(symbol),
            trade_id: None,
            price,
            quantity,
            side: if side == 1 { TradeSide::Buy } else { TradeSide::Sell },
            timestamp: ts,
        })
    }
}

#[async_trait]
impl WebSocketAdapter for MexcAdapter {
    async fn connect(&mut self) -> anyhow::Result<()> {
        let url = if self.config.url.is_empty() {
            MEXC_WS_URL
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
                            if data.get("ping").is_some() {
                                let pong = json!({ "pong": Self::now() });
                                let _ = ws_lock.send(Message::Text(pong.to_string())).await;
                                continue;
                            }

                            if let Some(ref cb) = callback {
                                if let Some(channel) = data.get("c").and_then(|c| c.as_str()) {
                                    let adapter = MexcAdapter::new(ProviderConfig::default());

                                    let message = if channel.contains("ticker") {
                                        adapter.parse_ticker(&data).map(MarketMessage::Ticker)
                                    } else if channel.contains("depth") {
                                        adapter.parse_orderbook(&data).map(MarketMessage::OrderBook)
                                    } else if channel.contains("deals") {
                                        adapter.parse_trade(&data).map(MarketMessage::Trade)
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
        let mexc_symbol = Self::to_mexc_symbol(symbol);

        let param = match channel {
            "ticker" => format!("spot@public.miniTicker.v3.api@{}", mexc_symbol),
            "book" | "depth" => format!("spot@public.limit.depth.v3.api@{}@20", mexc_symbol),
            "trade" => format!("spot@public.deals.v3.api@{}", mexc_symbol),
            _ => return Err(anyhow::anyhow!("Unsupported channel: {}", channel)),
        };

        let subscribe_msg = json!({
            "method": "SUBSCRIPTION",
            "params": [param]
        });

        let mut ws_lock = ws.write().await;
        ws_lock.send(Message::Text(subscribe_msg.to_string())).await?;

        Ok(())
    }

    async fn unsubscribe(&mut self, symbol: &str, channel: &str) -> anyhow::Result<()> {
        let ws = self.ws.as_ref().ok_or_else(|| anyhow::anyhow!("Not connected"))?;
        let mexc_symbol = Self::to_mexc_symbol(symbol);

        let param = match channel {
            "ticker" => format!("spot@public.miniTicker.v3.api@{}", mexc_symbol),
            "book" | "depth" => format!("spot@public.limit.depth.v3.api@{}@20", mexc_symbol),
            "trade" => format!("spot@public.deals.v3.api@{}", mexc_symbol),
            _ => return Err(anyhow::anyhow!("Unsupported channel: {}", channel)),
        };

        let unsubscribe_msg = json!({
            "method": "UNSUBSCRIPTION",
            "params": [param]
        });

        let mut ws_lock = ws.write().await;
        ws_lock.send(Message::Text(unsubscribe_msg.to_string())).await?;

        Ok(())
    }

    fn set_message_callback(&mut self, callback: Box<dyn Fn(MarketMessage) + Send + Sync>) {
        self.message_callback = Some(Arc::new(callback));
    }

    fn provider_name(&self) -> &str {
        "mexc"
    }

    fn is_connected(&self) -> bool {
        *self.connected.blocking_read()
    }
}
