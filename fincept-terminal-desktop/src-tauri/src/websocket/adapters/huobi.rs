#![allow(dead_code)]
// Huobi WebSocket Adapter
//
// Implements Huobi WebSocket API
// Supports: ticker, depth, trade channels
// Note: Huobi uses gzip compression for messages

use super::WebSocketAdapter;
use crate::websocket::types::*;
use async_trait::async_trait;
use futures_util::{SinkExt, StreamExt};
use serde_json::{json, Value};
use std::io::Read;
use std::sync::Arc;
use std::time::{SystemTime, UNIX_EPOCH};
use tokio::sync::RwLock;
use tokio_tungstenite::{connect_async, tungstenite::Message};

const HUOBI_WS_URL: &str = "wss://api.huobi.pro/ws";

pub struct HuobiAdapter {
    config: ProviderConfig,
    ws: Option<Arc<RwLock<tokio_tungstenite::WebSocketStream<tokio_tungstenite::MaybeTlsStream<tokio::net::TcpStream>>>>>,
    message_callback: Option<Arc<Box<dyn Fn(MarketMessage) + Send + Sync>>>,
    connected: Arc<RwLock<bool>>,
}

impl HuobiAdapter {
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

    /// Decompress gzip data
    fn decompress_gzip(data: &[u8]) -> Option<String> {
        let mut decoder = flate2::read::GzDecoder::new(data);
        let mut decompressed = String::new();
        decoder.read_to_string(&mut decompressed).ok()?;
        Some(decompressed)
    }

    /// Normalize Huobi symbol (btcusdt -> BTC/USDT)
    fn normalize_huobi_symbol(symbol: &str) -> String {
        let symbol_upper = symbol.to_uppercase();
        for quote in ["USDT", "USDC", "USD", "BTC", "ETH", "HUSD"] {
            if let Some(base) = symbol_upper.strip_suffix(quote) {
                return format!("{}/{}", base, quote);
            }
        }
        symbol_upper
    }

    /// Convert symbol to Huobi format (BTC/USDT -> btcusdt)
    fn to_huobi_symbol(symbol: &str) -> String {
        symbol.replace('/', "").to_lowercase()
    }

    /// Parse Huobi ticker
    fn parse_ticker(&self, data: &Value, symbol: &str) -> Option<TickerData> {
        let tick = data.get("tick")?;
        let close = tick.get("close")?.as_f64()?;
        let open = tick.get("open")?.as_f64();
        let high = tick.get("high")?.as_f64();
        let low = tick.get("low")?.as_f64();
        let volume = tick.get("vol")?.as_f64();
        let bid = tick.get("bid")?.as_array().and_then(|a| a.first()?.as_f64());
        let ask = tick.get("ask")?.as_array().and_then(|a| a.first()?.as_f64());
        let bid_sz = tick.get("bid")?.as_array().and_then(|a| a.get(1)?.as_f64());
        let ask_sz = tick.get("ask")?.as_array().and_then(|a| a.get(1)?.as_f64());

        let change_percent = if let Some(o) = open {
            if o > 0.0 {
                Some((close - o) / o * 100.0)
            } else {
                None
            }
        } else {
            None
        };

        Some(TickerData {
            provider: "huobi".to_string(),
            symbol: Self::normalize_huobi_symbol(symbol),
            price: close,
            bid,
            ask,
            bid_size: bid_sz,
            ask_size: ask_sz,
            volume,
            high,
            low,
            open,
            close: Some(close),
            change: None,
            change_percent,
            quote_volume: None,
            timestamp: data.get("ts")?.as_u64().unwrap_or_else(Self::now),
        })
    }

    /// Parse Huobi order book
    fn parse_orderbook(&self, data: &Value, symbol: &str) -> Option<OrderBookData> {
        let tick = data.get("tick")?;
        let bids = tick.get("bids")?.as_array()?;
        let asks = tick.get("asks")?.as_array()?;

        let parse_levels = |levels: &Vec<Value>| -> Vec<OrderBookLevel> {
            levels
                .iter()
                .filter_map(|level| {
                    let arr = level.as_array()?;
                    Some(OrderBookLevel {
                        price: arr.get(0)?.as_f64()?,
                        quantity: arr.get(1)?.as_f64()?,
                        count: None,
                    })
                })
                .collect()
        };

        Some(OrderBookData {
            provider: "huobi".to_string(),
            symbol: Self::normalize_huobi_symbol(symbol),
            bids: parse_levels(bids),
            asks: parse_levels(asks),
            timestamp: data.get("ts")?.as_u64().unwrap_or_else(Self::now),
            is_snapshot: true,
        })
    }

    /// Parse Huobi trade
    fn parse_trade(&self, data: &Value, symbol: &str) -> Option<TradeData> {
        let tick = data.get("tick")?;
        let trade_data = tick.get("data")?.as_array()?.first()?;

        let price = trade_data.get("price")?.as_f64()?;
        let amount = trade_data.get("amount")?.as_f64()?;
        let direction = trade_data.get("direction")?.as_str()?;
        let id = trade_data.get("tradeId")?.as_u64().map(|i| i.to_string());
        let ts = trade_data.get("ts")?.as_u64().unwrap_or_else(Self::now);

        Some(TradeData {
            provider: "huobi".to_string(),
            symbol: Self::normalize_huobi_symbol(symbol),
            trade_id: id,
            price,
            quantity: amount,
            side: match direction {
                "buy" => TradeSide::Buy,
                "sell" => TradeSide::Sell,
                _ => TradeSide::Unknown,
            },
            timestamp: ts,
        })
    }
}

#[async_trait]
impl WebSocketAdapter for HuobiAdapter {
    async fn connect(&mut self) -> anyhow::Result<()> {
        let url = if self.config.url.is_empty() {
            HUOBI_WS_URL
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
                    Ok(Message::Binary(data)) => {
                        // Huobi sends gzip compressed messages
                        if let Some(text) = Self::decompress_gzip(&data) {
                            if let Ok(json_data) = serde_json::from_str::<Value>(&text) {
                                // Handle ping
                                if let Some(ping) = json_data.get("ping") {
                                    let pong = json!({ "pong": ping });
                                    let _ = ws_lock.send(Message::Text(pong.to_string())).await;
                                    continue;
                                }

                                if let Some(ref cb) = callback {
                                    if let Some(ch) = json_data.get("ch").and_then(|c| c.as_str()) {
                                        let parts: Vec<&str> = ch.split('.').collect();
                                        if parts.len() >= 3 {
                                            let symbol = parts[1];
                                            let channel = parts[2];
                                            let adapter = HuobiAdapter::new(ProviderConfig::default());

                                            let message = match channel {
                                                "ticker" => adapter.parse_ticker(&json_data, symbol).map(MarketMessage::Ticker),
                                                "depth" => adapter.parse_orderbook(&json_data, symbol).map(MarketMessage::OrderBook),
                                                "trade" => adapter.parse_trade(&json_data, symbol).map(MarketMessage::Trade),
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
        _params: Option<Value>,
    ) -> anyhow::Result<()> {
        let ws = self.ws.as_ref().ok_or_else(|| anyhow::anyhow!("Not connected"))?;
        let huobi_symbol = Self::to_huobi_symbol(symbol);

        let sub_channel = match channel {
            "ticker" => format!("market.{}.ticker", huobi_symbol),
            "book" | "depth" => format!("market.{}.depth.step0", huobi_symbol),
            "trade" => format!("market.{}.trade.detail", huobi_symbol),
            _ => return Err(anyhow::anyhow!("Unsupported channel: {}", channel)),
        };

        let subscribe_msg = json!({
            "sub": sub_channel,
            "id": format!("id{}", Self::now())
        });

        let mut ws_lock = ws.write().await;
        ws_lock.send(Message::Text(subscribe_msg.to_string())).await?;

        Ok(())
    }

    async fn unsubscribe(&mut self, symbol: &str, channel: &str) -> anyhow::Result<()> {
        let ws = self.ws.as_ref().ok_or_else(|| anyhow::anyhow!("Not connected"))?;
        let huobi_symbol = Self::to_huobi_symbol(symbol);

        let unsub_channel = match channel {
            "ticker" => format!("market.{}.ticker", huobi_symbol),
            "book" | "depth" => format!("market.{}.depth.step0", huobi_symbol),
            "trade" => format!("market.{}.trade.detail", huobi_symbol),
            _ => return Err(anyhow::anyhow!("Unsupported channel: {}", channel)),
        };

        let unsubscribe_msg = json!({
            "unsub": unsub_channel,
            "id": format!("id{}", Self::now())
        });

        let mut ws_lock = ws.write().await;
        ws_lock.send(Message::Text(unsubscribe_msg.to_string())).await?;

        Ok(())
    }

    fn set_message_callback(&mut self, callback: Box<dyn Fn(MarketMessage) + Send + Sync>) {
        self.message_callback = Some(Arc::new(callback));
    }

    fn provider_name(&self) -> &str {
        "huobi"
    }

    fn is_connected(&self) -> bool {
        *self.connected.blocking_read()
    }
}
