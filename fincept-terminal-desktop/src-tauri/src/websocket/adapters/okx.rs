#![allow(dead_code)]
// OKX WebSocket Adapter
//
// Implements OKX WebSocket API v5
// Supports: ticker, book (depth), trade, candle channels

use super::WebSocketAdapter;
use crate::websocket::types::*;
use async_trait::async_trait;
use futures_util::{SinkExt, StreamExt};
use serde_json::{json, Value};
use std::sync::Arc;
use std::time::{SystemTime, UNIX_EPOCH};
use tokio::sync::RwLock;
use tokio_tungstenite::{connect_async, tungstenite::Message};

const OKX_WS_URL: &str = "wss://ws.okx.com:8443/ws/v5/public";

pub struct OkxAdapter {
    config: ProviderConfig,
    ws: Option<Arc<RwLock<tokio_tungstenite::WebSocketStream<tokio_tungstenite::MaybeTlsStream<tokio::net::TcpStream>>>>>,
    message_callback: Option<Arc<Box<dyn Fn(MarketMessage) + Send + Sync>>>,
    connected: Arc<RwLock<bool>>,
}

impl OkxAdapter {
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

    /// Normalize OKX symbol (BTC-USDT -> BTC/USDT)
    fn normalize_okx_symbol(symbol: &str) -> String {
        symbol.replace('-', "/")
    }

    /// Convert symbol to OKX format (BTC/USDT -> BTC-USDT)
    fn to_okx_symbol(symbol: &str) -> String {
        symbol.replace('/', "-")
    }

    /// Parse OKX ticker
    fn parse_ticker(&self, data: &Value) -> Option<TickerData> {
        let inst_id = data.get("instId")?.as_str()?;
        let last = data.get("last")?.as_str()?.parse::<f64>().ok()?;
        let bid = data.get("bidPx")?.as_str().and_then(|s| s.parse::<f64>().ok());
        let ask = data.get("askPx")?.as_str().and_then(|s| s.parse::<f64>().ok());
        let bid_sz = data.get("bidSz")?.as_str().and_then(|s| s.parse::<f64>().ok());
        let ask_sz = data.get("askSz")?.as_str().and_then(|s| s.parse::<f64>().ok());
        let vol24h = data.get("vol24h")?.as_str().and_then(|s| s.parse::<f64>().ok());
        let high24h = data.get("high24h")?.as_str().and_then(|s| s.parse::<f64>().ok());
        let low24h = data.get("low24h")?.as_str().and_then(|s| s.parse::<f64>().ok());
        let open24h = data.get("open24h")?.as_str().and_then(|s| s.parse::<f64>().ok());

        let change_percent = if let Some(open) = open24h {
            if open > 0.0 {
                Some((last - open) / open * 100.0)
            } else {
                None
            }
        } else {
            None
        };

        Some(TickerData {
            provider: "okx".to_string(),
            symbol: Self::normalize_okx_symbol(inst_id),
            price: last,
            bid,
            ask,
            bid_size: bid_sz,
            ask_size: ask_sz,
            volume: vol24h,
            high: high24h,
            low: low24h,
            open: open24h,
            close: Some(last),
            change: None,
            change_percent,
            quote_volume: None,
            timestamp: Self::now(),
        })
    }

    /// Parse OKX order book
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
                        count: arr.get(3)?.as_str().and_then(|s| s.parse::<u32>().ok()),
                    })
                })
                .collect()
        };

        Some(OrderBookData {
            provider: "okx".to_string(),
            symbol: Self::normalize_okx_symbol(inst_id),
            bids: parse_levels(bids),
            asks: parse_levels(asks),
            timestamp: Self::now(),
            is_snapshot: true,
        })
    }

    /// Parse OKX trade
    fn parse_trade(&self, data: &Value) -> Option<TradeData> {
        let inst_id = data.get("instId")?.as_str()?;
        let price = data.get("px")?.as_str()?.parse::<f64>().ok()?;
        let quantity = data.get("sz")?.as_str()?.parse::<f64>().ok()?;
        let side = data.get("side")?.as_str()?;
        let trade_id = data.get("tradeId")?.as_str().map(|s| s.to_string());
        let ts = data.get("ts")?.as_str().and_then(|s| s.parse::<u64>().ok()).unwrap_or_else(Self::now);

        Some(TradeData {
            provider: "okx".to_string(),
            symbol: Self::normalize_okx_symbol(inst_id),
            trade_id,
            price,
            quantity,
            side: match side {
                "buy" => TradeSide::Buy,
                "sell" => TradeSide::Sell,
                _ => TradeSide::Unknown,
            },
            timestamp: ts,
        })
    }

    /// Parse OKX candle
    fn parse_candle(&self, data: &Value, inst_id: &str, interval: &str) -> Option<CandleData> {
        let arr = data.as_array()?;

        Some(CandleData {
            provider: "okx".to_string(),
            symbol: Self::normalize_okx_symbol(inst_id),
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
impl WebSocketAdapter for OkxAdapter {
    async fn connect(&mut self) -> anyhow::Result<()> {
        let url = if self.config.url.is_empty() {
            OKX_WS_URL
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
                                // Check if it's a data message
                                if let Some(arg) = data.get("arg") {
                                    let channel = arg.get("channel").and_then(|c| c.as_str()).unwrap_or("");
                                    let inst_id = arg.get("instId").and_then(|i| i.as_str()).unwrap_or("");

                                    if let Some(data_arr) = data.get("data").and_then(|d| d.as_array()) {
                                        for item in data_arr {
                                            let adapter = OkxAdapter::new(ProviderConfig::default());
                                            let message = match channel {
                                                "tickers" => adapter.parse_ticker(item).map(MarketMessage::Ticker),
                                                "books5" | "books" | "books50-l2-tbt" => {
                                                    adapter.parse_orderbook(item, inst_id).map(MarketMessage::OrderBook)
                                                }
                                                "trades" => adapter.parse_trade(item).map(MarketMessage::Trade),
                                                c if c.starts_with("candle") => {
                                                    let interval = c.strip_prefix("candle").unwrap_or("1m");
                                                    adapter.parse_candle(item, inst_id, interval).map(MarketMessage::Candle)
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
        Ok(())
    }

    async fn subscribe(
        &mut self,
        symbol: &str,
        channel: &str,
        params: Option<Value>,
    ) -> anyhow::Result<()> {
        let ws = self.ws.as_ref().ok_or_else(|| anyhow::anyhow!("Not connected"))?;
        let okx_symbol = Self::to_okx_symbol(symbol);

        let okx_channel = match channel {
            "ticker" => "tickers",
            "book" | "depth" => {
                let depth = params.as_ref()
                    .and_then(|p| p.get("depth"))
                    .and_then(|d| d.as_u64())
                    .unwrap_or(5);
                if depth <= 5 { "books5" } else { "books" }
            }
            "trade" => "trades",
            "candle" | "ohlc" => {
                let interval = params.as_ref()
                    .and_then(|p| p.get("interval"))
                    .and_then(|i| i.as_str())
                    .unwrap_or("1m");
                &format!("candle{}", interval)
            }
            _ => return Err(anyhow::anyhow!("Unsupported channel: {}", channel)),
        };

        let subscribe_msg = json!({
            "op": "subscribe",
            "args": [{
                "channel": okx_channel,
                "instId": okx_symbol
            }]
        });

        let mut ws_lock = ws.write().await;
        ws_lock.send(Message::Text(subscribe_msg.to_string())).await?;

        Ok(())
    }

    async fn unsubscribe(&mut self, symbol: &str, channel: &str) -> anyhow::Result<()> {
        let ws = self.ws.as_ref().ok_or_else(|| anyhow::anyhow!("Not connected"))?;
        let okx_symbol = Self::to_okx_symbol(symbol);

        let okx_channel = match channel {
            "ticker" => "tickers",
            "book" | "depth" => "books5",
            "trade" => "trades",
            "candle" | "ohlc" => "candle1m",
            _ => return Err(anyhow::anyhow!("Unsupported channel: {}", channel)),
        };

        let unsubscribe_msg = json!({
            "op": "unsubscribe",
            "args": [{
                "channel": okx_channel,
                "instId": okx_symbol
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
        "okx"
    }

    fn is_connected(&self) -> bool {
        *self.connected.blocking_read()
    }
}
