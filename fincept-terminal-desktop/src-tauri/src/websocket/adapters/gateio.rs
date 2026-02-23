#![allow(dead_code)]
// Gate.io WebSocket Adapter
//
// Implements Gate.io WebSocket API v4
// Supports: ticker, orderbook, trades channels

use super::WebSocketAdapter;
use crate::websocket::types::*;
use async_trait::async_trait;
use futures_util::{SinkExt, StreamExt};
use serde_json::{json, Value};
use std::sync::Arc;
use std::time::{SystemTime, UNIX_EPOCH};
use tokio::sync::RwLock;
use tokio_tungstenite::{connect_async, tungstenite::Message};

const GATEIO_WS_URL: &str = "wss://api.gateio.ws/ws/v4/";

pub struct GateioAdapter {
    config: ProviderConfig,
    ws: Option<Arc<RwLock<tokio_tungstenite::WebSocketStream<tokio_tungstenite::MaybeTlsStream<tokio::net::TcpStream>>>>>,
    message_callback: Option<Arc<Box<dyn Fn(MarketMessage) + Send + Sync>>>,
    connected: Arc<RwLock<bool>>,
}

impl GateioAdapter {
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

    /// Normalize Gate.io symbol (BTC_USDT -> BTC/USDT)
    fn normalize_gateio_symbol(symbol: &str) -> String {
        symbol.replace('_', "/")
    }

    /// Convert symbol to Gate.io format (BTC/USDT -> BTC_USDT)
    fn to_gateio_symbol(symbol: &str) -> String {
        symbol.replace('/', "_")
    }

    /// Parse Gate.io ticker
    fn parse_ticker(&self, data: &Value) -> Option<TickerData> {
        let result = data.get("result")?;
        let currency_pair = result.get("currency_pair")?.as_str()?;
        let last = result.get("last")?.as_str()?.parse::<f64>().ok()?;
        let high = result.get("high_24h")?.as_str().and_then(|s| s.parse::<f64>().ok());
        let low = result.get("low_24h")?.as_str().and_then(|s| s.parse::<f64>().ok());
        let volume = result.get("base_volume")?.as_str().and_then(|s| s.parse::<f64>().ok());
        let quote_volume = result.get("quote_volume")?.as_str().and_then(|s| s.parse::<f64>().ok());
        let change_percent = result.get("change_percentage")?.as_str().and_then(|s| s.parse::<f64>().ok());

        Some(TickerData {
            provider: "gateio".to_string(),
            symbol: Self::normalize_gateio_symbol(currency_pair),
            price: last,
            bid: None,
            ask: None,
            bid_size: None,
            ask_size: None,
            volume,
            high,
            low,
            open: None,
            close: Some(last),
            change: None,
            change_percent,
            quote_volume,
            timestamp: Self::now(),
        })
    }

    /// Parse Gate.io order book
    fn parse_orderbook(&self, data: &Value) -> Option<OrderBookData> {
        let result = data.get("result")?;
        let symbol = result.get("s")?.as_str()?;
        let bids = result.get("bids")?.as_array()?;
        let asks = result.get("asks")?.as_array()?;

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
            provider: "gateio".to_string(),
            symbol: Self::normalize_gateio_symbol(symbol),
            bids: parse_levels(bids),
            asks: parse_levels(asks),
            timestamp: result.get("t")?.as_u64().unwrap_or_else(Self::now),
            is_snapshot: true,
        })
    }

    /// Parse Gate.io trade
    fn parse_trade(&self, data: &Value) -> Option<TradeData> {
        let result = data.get("result")?;
        let currency_pair = result.get("currency_pair")?.as_str()?;
        let price = result.get("price")?.as_str()?.parse::<f64>().ok()?;
        let amount = result.get("amount")?.as_str()?.parse::<f64>().ok()?;
        let side = result.get("side")?.as_str()?;
        let id = result.get("id")?.as_u64().map(|i| i.to_string());
        let ts = result.get("create_time_ms")?.as_str()?.parse::<u64>().ok().unwrap_or_else(Self::now);

        Some(TradeData {
            provider: "gateio".to_string(),
            symbol: Self::normalize_gateio_symbol(currency_pair),
            trade_id: id,
            price,
            quantity: amount,
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
impl WebSocketAdapter for GateioAdapter {
    async fn connect(&mut self) -> anyhow::Result<()> {
        let url = if self.config.url.is_empty() {
            GATEIO_WS_URL
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
                                let channel = data.get("channel").and_then(|c| c.as_str()).unwrap_or("");
                                let event = data.get("event").and_then(|e| e.as_str()).unwrap_or("");

                                if event == "update" {
                                    let adapter = GateioAdapter::new(ProviderConfig::default());

                                    let message = match channel {
                                        "spot.tickers" => adapter.parse_ticker(&data).map(MarketMessage::Ticker),
                                        "spot.order_book" => adapter.parse_orderbook(&data).map(MarketMessage::OrderBook),
                                        "spot.trades" => adapter.parse_trade(&data).map(MarketMessage::Trade),
                                        _ => None,
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
        let gateio_symbol = Self::to_gateio_symbol(symbol);

        let (gateio_channel, payload) = match channel {
            "ticker" => ("spot.tickers", json!([gateio_symbol])),
            "book" | "depth" => ("spot.order_book", json!([gateio_symbol, "20", "100ms"])),
            "trade" => ("spot.trades", json!([gateio_symbol])),
            _ => return Err(anyhow::anyhow!("Unsupported channel: {}", channel)),
        };

        let subscribe_msg = json!({
            "time": Self::now() / 1000,
            "channel": gateio_channel,
            "event": "subscribe",
            "payload": payload
        });

        let mut ws_lock = ws.write().await;
        ws_lock.send(Message::Text(subscribe_msg.to_string())).await?;

        Ok(())
    }

    async fn unsubscribe(&mut self, symbol: &str, channel: &str) -> anyhow::Result<()> {
        let ws = self.ws.as_ref().ok_or_else(|| anyhow::anyhow!("Not connected"))?;
        let gateio_symbol = Self::to_gateio_symbol(symbol);

        let (gateio_channel, payload) = match channel {
            "ticker" => ("spot.tickers", json!([gateio_symbol])),
            "book" | "depth" => ("spot.order_book", json!([gateio_symbol, "20", "100ms"])),
            "trade" => ("spot.trades", json!([gateio_symbol])),
            _ => return Err(anyhow::anyhow!("Unsupported channel: {}", channel)),
        };

        let unsubscribe_msg = json!({
            "time": Self::now() / 1000,
            "channel": gateio_channel,
            "event": "unsubscribe",
            "payload": payload
        });

        let mut ws_lock = ws.write().await;
        ws_lock.send(Message::Text(unsubscribe_msg.to_string())).await?;

        Ok(())
    }

    fn set_message_callback(&mut self, callback: Box<dyn Fn(MarketMessage) + Send + Sync>) {
        self.message_callback = Some(Arc::new(callback));
    }

    fn provider_name(&self) -> &str {
        "gateio"
    }

    fn is_connected(&self) -> bool {
        *self.connected.blocking_read()
    }
}
