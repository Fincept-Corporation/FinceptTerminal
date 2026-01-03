// Binance WebSocket Adapter
//
// Implements Binance WebSocket API
// Supports: ticker, book (depth), trade, kline channels

use super::WebSocketAdapter;
use crate::websocket::types::*;
use async_trait::async_trait;
use futures_util::{SinkExt, StreamExt};
use serde_json::{json, Value};
use std::sync::Arc;
use std::time::{SystemTime, UNIX_EPOCH};
use tokio::sync::RwLock;
use tokio_tungstenite::{connect_async, tungstenite::Message};

const BINANCE_WS_URL: &str = "wss://stream.binance.com:9443/ws";

pub struct BinanceAdapter {
    config: ProviderConfig,
    ws: Option<Arc<RwLock<tokio_tungstenite::WebSocketStream<tokio_tungstenite::MaybeTlsStream<tokio::net::TcpStream>>>>>,
    message_callback: Option<Arc<Box<dyn Fn(MarketMessage) + Send + Sync>>>,
    connected: Arc<RwLock<bool>>,
}

impl BinanceAdapter {
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

    /// Normalize Binance symbol (BTCUSDT -> BTC/USDT)
    fn normalize_binance_symbol(symbol: &str) -> String {
        // Common quote currencies
        for quote in ["USDT", "BUSD", "USD", "BTC", "ETH", "BNB"] {
            if let Some(base) = symbol.strip_suffix(quote) {
                return format!("{}/{}", base, quote);
            }
        }
        symbol.to_string()
    }

    /// Convert symbol to Binance format (BTC/USDT -> btcusdt)
    fn to_binance_symbol(symbol: &str) -> String {
        symbol.replace('/', "").to_lowercase()
    }

    /// Parse Binance ticker (24hr mini ticker)
    fn parse_ticker(&self, data: &Value) -> Option<TickerData> {
        let symbol = data.get("s")?.as_str()?;
        let close = data.get("c")?.as_str()?.parse::<f64>().ok()?;
        let open = data.get("o")?.as_str()?.parse::<f64>().ok();
        let high = data.get("h")?.as_str()?.parse::<f64>().ok();
        let low = data.get("l")?.as_str()?.parse::<f64>().ok();
        let volume = data.get("v")?.as_str()?.parse::<f64>().ok();
        let _quote_volume = data.get("q")?.as_str()?.parse::<f64>().ok();

        // Calculate change percent
        let change_percent = if let Some(open_price) = open {
            if open_price > 0.0 {
                Some((close - open_price) / open_price * 100.0)
            } else {
                None
            }
        } else {
            None
        };

        Some(TickerData {
            provider: "binance".to_string(),
            symbol: Self::normalize_binance_symbol(symbol),
            price: close,
            bid: None, // Mini ticker doesn't have bid/ask
            ask: None,
            bid_size: None,
            ask_size: None,
            volume,
            high,
            low,
            open,
            close: Some(close),
            change: None,
            change_percent,
            timestamp: Self::now(),
        })
    }

    /// Parse Binance book ticker (bid/ask)
    fn parse_book_ticker(&self, data: &Value) -> Option<TickerData> {
        let symbol = data.get("s")?.as_str()?;
        let bid = data.get("b")?.as_str()?.parse::<f64>().ok()?;
        let ask = data.get("a")?.as_str()?.parse::<f64>().ok()?;
        let bid_qty = data.get("B")?.as_str()?.parse::<f64>().ok();
        let ask_qty = data.get("A")?.as_str()?.parse::<f64>().ok();

        let price = (bid + ask) / 2.0;

        Some(TickerData {
            provider: "binance".to_string(),
            symbol: Self::normalize_binance_symbol(symbol),
            price,
            bid: Some(bid),
            ask: Some(ask),
            bid_size: bid_qty,
            ask_size: ask_qty,
            volume: None,
            high: None,
            low: None,
            open: None,
            close: Some(price),
            change: None,
            change_percent: None,
            timestamp: Self::now(),
        })
    }

    /// Parse Binance depth (order book)
    fn parse_depth(&self, data: &Value, symbol: &str) -> Option<OrderBookData> {
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
            provider: "binance".to_string(),
            symbol: Self::normalize_binance_symbol(symbol),
            bids: parse_levels(bids),
            asks: parse_levels(asks),
            timestamp: Self::now(),
            is_snapshot: data.get("e")?.as_str()? == "depthUpdate",
        })
    }

    /// Parse Binance trade
    fn parse_trade(&self, data: &Value) -> Option<TradeData> {
        let symbol = data.get("s")?.as_str()?;
        let price = data.get("p")?.as_str()?.parse::<f64>().ok()?;
        let quantity = data.get("q")?.as_str()?.parse::<f64>().ok()?;
        let trade_id = data.get("t")?.as_i64().map(|t| t.to_string());
        let is_buyer_maker = data.get("m")?.as_bool()?;

        Some(TradeData {
            provider: "binance".to_string(),
            symbol: Self::normalize_binance_symbol(symbol),
            trade_id,
            price,
            quantity,
            side: if is_buyer_maker {
                TradeSide::Sell
            } else {
                TradeSide::Buy
            },
            timestamp: data.get("T")?.as_u64().unwrap_or_else(Self::now),
        })
    }

    /// Parse Binance kline (candlestick)
    fn parse_kline(&self, data: &Value) -> Option<CandleData> {
        let kline = data.get("k")?;
        let symbol = kline.get("s")?.as_str()?;
        let interval = kline.get("i")?.as_str()?;

        Some(CandleData {
            provider: "binance".to_string(),
            symbol: Self::normalize_binance_symbol(symbol),
            interval: interval.to_string(),
            open: kline.get("o")?.as_str()?.parse::<f64>().ok()?,
            high: kline.get("h")?.as_str()?.parse::<f64>().ok()?,
            low: kline.get("l")?.as_str()?.parse::<f64>().ok()?,
            close: kline.get("c")?.as_str()?.parse::<f64>().ok()?,
            volume: kline.get("v")?.as_str()?.parse::<f64>().ok()?,
            timestamp: kline.get("t")?.as_u64()?,
        })
    }
}

#[async_trait]
impl WebSocketAdapter for BinanceAdapter {
    async fn connect(&mut self) -> anyhow::Result<()> {
        let (ws_stream, _) = connect_async(BINANCE_WS_URL).await?;
        let ws = Arc::new(RwLock::new(ws_stream));
        self.ws = Some(ws.clone());
        *self.connected.write().await = true;

        // Start message handler
        let callback = self.message_callback.clone();
        let connected = self.connected.clone();

        tokio::spawn(async move {
            let mut ws_lock = ws.write().await;

            while let Some(msg) = ws_lock.next().await {
                match msg {
                    Ok(Message::Text(text)) => {
                        if let Ok(data) = serde_json::from_str::<Value>(&text) {
                            if let Some(ref cb) = callback {
                                // Determine message type from event field
                                if let Some(event) = data.get("e").and_then(|e| e.as_str()) {
                                    let message = match event {
                                        "24hrMiniTicker" => {
                                            if let Some(adapter) = BinanceAdapter::new(ProviderConfig::default()).parse_ticker(&data) {
                                                Some(MarketMessage::Ticker(adapter))
                                            } else {
                                                None
                                            }
                                        }
                                        "bookTicker" => {
                                            if let Some(adapter) = BinanceAdapter::new(ProviderConfig::default()).parse_book_ticker(&data) {
                                                Some(MarketMessage::Ticker(adapter))
                                            } else {
                                                None
                                            }
                                        }
                                        "depthUpdate" => {
                                            if let Some(symbol) = data.get("s").and_then(|s| s.as_str()) {
                                                if let Some(book) = BinanceAdapter::new(ProviderConfig::default()).parse_depth(&data, symbol) {
                                                    Some(MarketMessage::OrderBook(book))
                                                } else {
                                                    None
                                                }
                                            } else {
                                                None
                                            }
                                        }
                                        "trade" => {
                                            if let Some(trade) = BinanceAdapter::new(ProviderConfig::default()).parse_trade(&data) {
                                                Some(MarketMessage::Trade(trade))
                                            } else {
                                                None
                                            }
                                        }
                                        "kline" => {
                                            if let Some(kline) = BinanceAdapter::new(ProviderConfig::default()).parse_kline(&data) {
                                                Some(MarketMessage::Candle(kline))
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
                    Ok(Message::Close(_)) => {
                        *connected.write().await = false;
                        break;
                    }
                    Err(e) => {
                        eprintln!("[Binance] WebSocket error: {}", e);
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
        let ws = self
            .ws
            .as_ref()
            .ok_or_else(|| anyhow::anyhow!("Not connected"))?;

        let binance_symbol = Self::to_binance_symbol(symbol);

        // Build stream name based on channel
        let stream = match channel {
            "ticker" => format!("{}@miniTicker", binance_symbol),
            "book_ticker" => format!("{}@bookTicker", binance_symbol),
            "book" | "depth" => {
                let levels = params
                    .as_ref()
                    .and_then(|p| p.get("levels"))
                    .and_then(|l| l.as_u64())
                    .unwrap_or(20);
                let speed = params
                    .as_ref()
                    .and_then(|p| p.get("speed"))
                    .and_then(|s| s.as_str())
                    .unwrap_or("100ms");
                format!("{}@depth{}@{}", binance_symbol, levels, speed)
            }
            "trade" => format!("{}@trade", binance_symbol),
            "candle" | "kline" => {
                let interval = params
                    .as_ref()
                    .and_then(|p| p.get("interval"))
                    .and_then(|i| i.as_str())
                    .unwrap_or("1m");
                format!("{}@kline_{}", binance_symbol, interval)
            }
            _ => return Err(anyhow::anyhow!("Unsupported channel: {}", channel)),
        };

        let subscribe_msg = json!({
            "method": "SUBSCRIBE",
            "params": [stream],
            "id": Self::now()
        });

        let mut ws_lock = ws.write().await;
        ws_lock
            .send(Message::Text(subscribe_msg.to_string()))
            .await?;

        Ok(())
    }

    async fn unsubscribe(&mut self, symbol: &str, channel: &str) -> anyhow::Result<()> {
        let ws = self
            .ws
            .as_ref()
            .ok_or_else(|| anyhow::anyhow!("Not connected"))?;

        let binance_symbol = Self::to_binance_symbol(symbol);
        let stream = match channel {
            "ticker" => format!("{}@miniTicker", binance_symbol),
            "book_ticker" => format!("{}@bookTicker", binance_symbol),
            "book" | "depth" => format!("{}@depth20@100ms", binance_symbol),
            "trade" => format!("{}@trade", binance_symbol),
            "candle" | "kline" => format!("{}@kline_1m", binance_symbol),
            _ => return Err(anyhow::anyhow!("Unsupported channel: {}", channel)),
        };

        let unsubscribe_msg = json!({
            "method": "UNSUBSCRIBE",
            "params": [stream],
            "id": Self::now()
        });

        let mut ws_lock = ws.write().await;
        ws_lock
            .send(Message::Text(unsubscribe_msg.to_string()))
            .await?;

        Ok(())
    }

    fn set_message_callback(&mut self, callback: Box<dyn Fn(MarketMessage) + Send + Sync>) {
        self.message_callback = Some(Arc::new(callback));
    }

    fn provider_name(&self) -> &str {
        "binance"
    }

    fn is_connected(&self) -> bool {
        *self.connected.blocking_read()
    }
}
