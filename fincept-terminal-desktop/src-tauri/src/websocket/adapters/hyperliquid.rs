// HyperLiquid WebSocket Adapter
// WebSocket URL: wss://api.hyperliquid.xyz/ws
// Documentation: https://hyperliquid.gitbook.io/hyperliquid-docs/for-developers/api/websocket

use super::WebSocketAdapter;
use crate::websocket::types::*;
use async_trait::async_trait;
use futures_util::{SinkExt, StreamExt};
use serde::{Deserialize, Serialize};
use serde_json::{json, Value};
use std::collections::HashMap;
use std::sync::Arc;
use tokio::sync::RwLock;
use tokio_tungstenite::{connect_async, tungstenite::Message, WebSocketStream, MaybeTlsStream};
use tokio::net::TcpStream;

const HYPERLIQUID_WS_URL: &str = "wss://api.hyperliquid.xyz/ws";

// HyperLiquid WebSocket message types
#[derive(Debug, Serialize)]
struct SubscribeMessage {
    method: String,
    subscription: Subscription,
}

#[derive(Debug, Serialize)]
#[serde(tag = "type")]
enum Subscription {
    #[serde(rename = "trades")]
    Trades { coin: String },
    #[serde(rename = "l2Book")]
    L2Book { coin: String },
    #[serde(rename = "allMids")]
    AllMids,
}

// Response types
#[derive(Debug, Deserialize)]
struct WsTrade {
    coin: String,
    side: String,
    px: String,
    sz: String,
    time: u64,
    #[serde(default)]
    tid: Option<u64>,
}

#[derive(Debug, Deserialize)]
struct WsBook {
    coin: String,
    levels: [Vec<WsLevel>; 2], // [bids, asks]
    time: u64,
}

#[derive(Debug, Deserialize)]
struct WsLevel {
    px: String,
    sz: String,
    n: u32,
}

#[derive(Debug, Deserialize)]
struct AllMids {
    mids: HashMap<String, String>,
}

#[derive(Debug, Deserialize)]
struct WsBbo {
    coin: String,
    time: u64,
    bbo: [WsLevel; 2], // [bid level, ask level]
}

#[derive(Debug, Deserialize)]
struct WsCandle {
    #[serde(rename = "s")]
    coin: String,
    #[serde(rename = "t")]
    start_time: u64,
    #[serde(rename = "T")]
    end_time: u64,
    #[serde(rename = "i")]
    interval: String,
    #[serde(rename = "o")]
    open: String,
    #[serde(rename = "h")]
    high: String,
    #[serde(rename = "l")]
    low: String,
    #[serde(rename = "c")]
    close: String,
    #[serde(rename = "v")]
    volume: String,
    #[serde(rename = "n")]
    trades: u64,
}

#[derive(Debug, Deserialize)]
struct WsMessage {
    channel: String,
    data: Value,
}

// Ticker state tracking for combining multiple channels
#[derive(Debug, Clone, Default)]
struct TickerState {
    mid: Option<f64>,
    bid: Option<f64>,
    ask: Option<f64>,
    high_24h: Option<f64>,
    low_24h: Option<f64>,
    volume_24h: Option<f64>,
    open_24h: Option<f64>,
    timestamp: u64,
}

pub struct HyperLiquidAdapter {
    config: ProviderConfig,
    message_callback: Option<Arc<Box<dyn Fn(MarketMessage) + Send + Sync>>>,
    ws_stream: Arc<RwLock<Option<WebSocketStream<MaybeTlsStream<TcpStream>>>>>,
    subscriptions: Arc<RwLock<HashMap<String, String>>>, // key: "symbol:channel", value: coin
    ticker_states: Arc<RwLock<HashMap<String, TickerState>>>, // coin -> ticker state
}

impl HyperLiquidAdapter {
    pub fn new(config: ProviderConfig) -> Self {
        Self {
            config,
            message_callback: None,
            ws_stream: Arc::new(RwLock::new(None)),
            subscriptions: Arc::new(RwLock::new(HashMap::new())),
            ticker_states: Arc::new(RwLock::new(HashMap::new())),
        }
    }

    fn normalize_symbol(&self, symbol: &str) -> String {
        // HyperLiquid uses symbols like "BTC", "ETH" without slash
        symbol.replace("/", "").replace("USD", "")
    }

    async fn send_message(&self, msg: Value) -> anyhow::Result<()> {
        let mut stream = self.ws_stream.write().await;
        if let Some(ws) = stream.as_mut() {
            let text = serde_json::to_string(&msg)?;
            ws.send(Message::Text(text)).await?;
            Ok(())
        } else {
            Err(anyhow::anyhow!("WebSocket not connected"))
        }
    }

    fn parse_trade(&self, coin: &str, data: &Value) -> Option<MarketMessage> {
        let trades: Vec<WsTrade> = serde_json::from_value(data.clone()).ok()?;

        for trade in trades {
            if let (Ok(price), Ok(quantity)) = (trade.px.parse::<f64>(), trade.sz.parse::<f64>()) {
                return Some(MarketMessage::Trade(TradeData {
                    provider: "hyperliquid".to_string(),
                    symbol: format!("{}/USD", coin.to_uppercase()),
                    trade_id: trade.tid.map(|t| t.to_string()),
                    price,
                    quantity,
                    side: match trade.side.as_str() {
                        "B" => TradeSide::Buy,
                        "A" => TradeSide::Sell,
                        _ => TradeSide::Unknown,
                    },
                    timestamp: trade.time,
                }));
            }
        }
        None
    }

    fn parse_orderbook(&self, coin: &str, data: &Value) -> Option<MarketMessage> {
        let book: WsBook = serde_json::from_value(data.clone()).ok()?;

        let bids: Vec<OrderBookLevel> = book.levels[0]
            .iter()
            .filter_map(|level| {
                let price = level.px.parse::<f64>().ok()?;
                let quantity = level.sz.parse::<f64>().ok()?;
                Some(OrderBookLevel {
                    price,
                    quantity,
                    count: Some(level.n),
                })
            })
            .collect();

        let asks: Vec<OrderBookLevel> = book.levels[1]
            .iter()
            .filter_map(|level| {
                let price = level.px.parse::<f64>().ok()?;
                let quantity = level.sz.parse::<f64>().ok()?;
                Some(OrderBookLevel {
                    price,
                    quantity,
                    count: Some(level.n),
                })
            })
            .collect();

        Some(MarketMessage::OrderBook(OrderBookData {
            provider: "hyperliquid".to_string(),
            symbol: format!("{}/USD", coin.to_uppercase()),
            bids,
            asks,
            timestamp: book.time,
            is_snapshot: true,
        }))
    }

    fn parse_all_mids(&self, data: &Value) -> Option<Vec<MarketMessage>> {
        let all_mids: AllMids = serde_json::from_value(data.clone()).ok()?;

        let mut messages = Vec::new();
        for (coin, mid_price) in all_mids.mids {
            if let Ok(price) = mid_price.parse::<f64>() {
                messages.push(MarketMessage::Ticker(TickerData {
                    provider: "hyperliquid".to_string(),
                    symbol: format!("{}/USD", coin.to_uppercase()),
                    price,
                    bid: None,
                    ask: None,
                    bid_size: None,
                    ask_size: None,
                    volume: None,
                    high: None,
                    low: None,
                    open: None,
                    close: None,
                    change: None,
                    change_percent: None,
                    timestamp: chrono::Utc::now().timestamp_millis() as u64,
                }));
            }
        }
        Some(messages)
    }

    async fn handle_message(&self, msg: Message) {
        if let Message::Text(text) = msg {
            if let Ok(ws_msg) = serde_json::from_str::<WsMessage>(&text) {
                let parsed_messages = match ws_msg.channel.as_str() {
                    c if c.starts_with("trades") => {
                        if let Some(coin) = c.strip_prefix("trades@") {
                            self.parse_trade(coin, &ws_msg.data).map(|m| vec![m])
                        } else {
                            None
                        }
                    }
                    c if c.starts_with("l2Book") => {
                        if let Some(coin) = c.strip_prefix("l2Book@") {
                            self.parse_orderbook(coin, &ws_msg.data).map(|m| vec![m])
                        } else {
                            None
                        }
                    }
                    "allMids" => self.parse_all_mids(&ws_msg.data),
                    _ => None,
                };

                if let Some(messages) = parsed_messages {
                    if let Some(callback) = &self.message_callback {
                        for message in messages {
                            callback(message);
                        }
                    }
                }
            }
        }
    }
}

#[async_trait]
impl WebSocketAdapter for HyperLiquidAdapter {
    async fn connect(&mut self) -> anyhow::Result<()> {
        let (ws_stream, _) = connect_async(HYPERLIQUID_WS_URL).await?;
        *self.ws_stream.write().await = Some(ws_stream);

        // Start message handling loop
        let ws_clone = self.ws_stream.clone();
        let callback_clone = self.message_callback.clone();
        let ticker_states_clone = self.ticker_states.clone();

        tokio::spawn(async move {
            loop {
                let message = {
                    let mut stream = ws_clone.write().await;
                    if let Some(ws) = stream.as_mut() {
                        ws.next().await
                    } else {
                        break;
                    }
                };

                match message {
                    Some(Ok(msg)) => {
                        if let Message::Text(text) = msg {
                            // Parse message
                            if let Ok(ws_msg) = serde_json::from_str::<WsMessage>(&text) {

                                // Parse and send messages
                                let messages = match ws_msg.channel.as_str() {
                                    c if c == "allMids" => {
                                        if let Ok(all_mids) = serde_json::from_value::<AllMids>(ws_msg.data.clone()) {
                                            let mut msgs = Vec::new();
                                            for (coin, mid_price) in all_mids.mids {
                                                if let Ok(price) = mid_price.parse::<f64>() {
                                                    msgs.push(MarketMessage::Ticker(TickerData {
                                                        provider: "hyperliquid".to_string(),
                                                        symbol: format!("{}/USD", coin.to_uppercase()),
                                                        price,
                                                        bid: None,
                                                        ask: None,
                                                        bid_size: None,
                                                        ask_size: None,
                                                        volume: None,
                                                        high: None,
                                                        low: None,
                                                        open: None,
                                                        close: None,
                                                        change: None,
                                                        change_percent: None,
                                                        timestamp: chrono::Utc::now().timestamp_millis() as u64,
                                                    }));
                                                }
                                            }
                                            Some(msgs)
                                        } else {
                                            None
                                        }
                                    }
                                    c if c.starts_with("l2Book") => {
                                        if let Ok(book) = serde_json::from_value::<WsBook>(ws_msg.data.clone()) {
                                            let bids: Vec<OrderBookLevel> = book.levels[0]
                                                .iter()
                                                .filter_map(|level| {
                                                    let price = level.px.parse::<f64>().ok()?;
                                                    let quantity = level.sz.parse::<f64>().ok()?;
                                                    Some(OrderBookLevel {
                                                        price,
                                                        quantity,
                                                        count: Some(level.n),
                                                    })
                                                })
                                                .collect();

                                            let asks: Vec<OrderBookLevel> = book.levels[1]
                                                .iter()
                                                .filter_map(|level| {
                                                    let price = level.px.parse::<f64>().ok()?;
                                                    let quantity = level.sz.parse::<f64>().ok()?;
                                                    Some(OrderBookLevel {
                                                        price,
                                                        quantity,
                                                        count: Some(level.n),
                                                    })
                                                })
                                                .collect();

                                            Some(vec![MarketMessage::OrderBook(OrderBookData {
                                                provider: "hyperliquid".to_string(),
                                                symbol: format!("{}/USD", book.coin.to_uppercase()),
                                                bids,
                                                asks,
                                                timestamp: book.time,
                                                is_snapshot: true,
                                            })])
                                        } else {
                                            None
                                        }
                                    }
                                    c if c.starts_with("bbo") => {
                                        // BBO (Best Bid/Offer) updates
                                        if let Ok(bbo) = serde_json::from_value::<WsBbo>(ws_msg.data.clone()) {
                                            let coin = bbo.coin.clone();

                                            if let (Ok(bid), Ok(ask)) = (bbo.bbo[0].px.parse::<f64>(), bbo.bbo[1].px.parse::<f64>()) {
                                                // Update ticker state
                                                let mut states = ticker_states_clone.write().await;
                                                let state = states.entry(coin.clone()).or_insert_with(TickerState::default);
                                                state.bid = Some(bid);
                                                state.ask = Some(ask);
                                                state.timestamp = bbo.time;

                                                // Emit ticker with updated bid/ask
                                                let ticker_msg = MarketMessage::Ticker(TickerData {
                                                    provider: "hyperliquid".to_string(),
                                                    symbol: format!("{}/USD", coin.to_uppercase()),
                                                    price: state.mid.unwrap_or((bid + ask) / 2.0),
                                                    bid: Some(bid),
                                                    ask: Some(ask),
                                                    bid_size: None,
                                                    ask_size: None,
                                                    volume: state.volume_24h,
                                                    high: state.high_24h,
                                                    low: state.low_24h,
                                                    open: state.open_24h,
                                                    close: None,
                                                    change: None,
                                                    change_percent: None,
                                                    timestamp: bbo.time,
                                                });
                                                drop(states); // Release lock

                                                Some(vec![ticker_msg])
                                            } else {
                                                None
                                            }
                                        } else {
                                            None
                                        }
                                    }
                                    c if c.starts_with("candle") => {
                                        // Candle updates for 24h stats
                                        if let Ok(candle) = serde_json::from_value::<WsCandle>(ws_msg.data.clone()) {
                                            let coin = candle.coin.clone();

                                            if let (Ok(open), Ok(high), Ok(low), Ok(close), Ok(volume)) = (
                                                candle.open.parse::<f64>(),
                                                candle.high.parse::<f64>(),
                                                candle.low.parse::<f64>(),
                                                candle.close.parse::<f64>(),
                                                candle.volume.parse::<f64>(),
                                            ) {
                                                // Update ticker state with 24h stats
                                                let mut states = ticker_states_clone.write().await;
                                                let state = states.entry(coin.clone()).or_insert_with(TickerState::default);
                                                state.open_24h = Some(open);
                                                state.high_24h = Some(high);
                                                state.low_24h = Some(low);
                                                state.volume_24h = Some(volume);

                                                // Emit ticker with 24h stats
                                                let price = state.mid.unwrap_or(close);
                                                let change = price - open;
                                                let change_percent = (change / open) * 100.0;

                                                let ticker_msg = MarketMessage::Ticker(TickerData {
                                                    provider: "hyperliquid".to_string(),
                                                    symbol: format!("{}/USD", coin.to_uppercase()),
                                                    price,
                                                    bid: state.bid,
                                                    ask: state.ask,
                                                    bid_size: None,
                                                    ask_size: None,
                                                    volume: Some(volume),
                                                    high: Some(high),
                                                    low: Some(low),
                                                    open: Some(open),
                                                    close: Some(close),
                                                    change: Some(change),
                                                    change_percent: Some(change_percent),
                                                    timestamp: candle.end_time,
                                                });
                                                drop(states); // Release lock

                                                Some(vec![ticker_msg])
                                            } else {
                                                None
                                            }
                                        } else {
                                            None
                                        }
                                    }
                                    c if c.starts_with("trades") => {
                                        if let Ok(trades) = serde_json::from_value::<Vec<WsTrade>>(ws_msg.data.clone()) {
                                            let mut msgs = Vec::new();
                                            for trade in trades {
                                                if let (Ok(price), Ok(quantity)) = (trade.px.parse::<f64>(), trade.sz.parse::<f64>()) {
                                                    msgs.push(MarketMessage::Trade(TradeData {
                                                        provider: "hyperliquid".to_string(),
                                                        symbol: format!("{}/USD", trade.coin.to_uppercase()),
                                                        trade_id: trade.tid.map(|t| t.to_string()),
                                                        price,
                                                        quantity,
                                                        side: match trade.side.as_str() {
                                                            "B" => TradeSide::Buy,
                                                            "A" => TradeSide::Sell,
                                                            _ => TradeSide::Unknown,
                                                        },
                                                        timestamp: trade.time,
                                                    }));
                                                }
                                            }
                                            Some(msgs)
                                        } else {
                                            None
                                        }
                                    }
                                    _ => None,
                                };

                                if let Some(messages) = messages {
                                    if let Some(callback) = &callback_clone {
                                        for message in messages {
                                            callback(message);
                                        }
                                    }
                                }
                            }
                        }
                    }
                    Some(Err(_)) => {
                        break;
                    }
                    None => {
                        break;
                    }
                }
            }
        });

        Ok(())
    }

    async fn disconnect(&mut self) -> anyhow::Result<()> {
        let mut stream = self.ws_stream.write().await;
        if let Some(mut ws) = stream.take() {
            ws.close(None).await?;
        }
        Ok(())
    }

    async fn subscribe(
        &mut self,
        symbol: &str,
        channel: &str,
        _params: Option<Value>,
    ) -> anyhow::Result<()> {
        let coin = self.normalize_symbol(symbol);

        match channel {
            "ticker" => {
                // For ticker, we need multiple subscriptions to get complete data:
                // 1. allMids - for mid price
                // 2. bbo - for bid/ask
                // 3. candle (1d) - for 24h high/low/volume

                // Subscribe to allMids
                let msg1 = SubscribeMessage {
                    method: "subscribe".to_string(),
                    subscription: Subscription::AllMids,
                };
                self.send_message(serde_json::to_value(&msg1)?).await?;

                // Subscribe to BBO for bid/ask
                let msg2 = json!({
                    "method": "subscribe",
                    "subscription": {
                        "type": "bbo",
                        "coin": coin
                    }
                });
                self.send_message(msg2).await?;

                // Subscribe to 1d candles for 24h stats
                let msg3 = json!({
                    "method": "subscribe",
                    "subscription": {
                        "type": "candle",
                        "coin": coin,
                        "interval": "1d"
                    }
                });
                self.send_message(msg3).await?;
            }
            "book" => {
                let msg = SubscribeMessage {
                    method: "subscribe".to_string(),
                    subscription: Subscription::L2Book { coin: coin.clone() },
                };
                self.send_message(serde_json::to_value(&msg)?).await?;
            }
            "trades" => {
                let msg = SubscribeMessage {
                    method: "subscribe".to_string(),
                    subscription: Subscription::Trades { coin: coin.clone() },
                };
                self.send_message(serde_json::to_value(&msg)?).await?;
            }
            _ => return Err(anyhow::anyhow!("Unsupported channel: {}", channel)),
        };

        // Track subscription
        let key = format!("{}:{}", symbol, channel);
        self.subscriptions.write().await.insert(key, coin);

        Ok(())
    }

    async fn unsubscribe(&mut self, symbol: &str, channel: &str) -> anyhow::Result<()> {
        let coin = self.normalize_symbol(symbol);

        let subscription = match channel {
            "ticker" => Subscription::AllMids,
            "book" => Subscription::L2Book { coin: coin.clone() },
            "trades" => Subscription::Trades { coin: coin.clone() },
            _ => return Err(anyhow::anyhow!("Unsupported channel: {}", channel)),
        };

        let msg = json!({
            "method": "unsubscribe",
            "subscription": subscription
        });

        self.send_message(msg).await?;

        // Remove from tracking
        let key = format!("{}:{}", symbol, channel);
        self.subscriptions.write().await.remove(&key);

        Ok(())
    }

    fn set_message_callback(&mut self, callback: Box<dyn Fn(MarketMessage) + Send + Sync>) {
        self.message_callback = Some(Arc::new(callback));
    }

    fn provider_name(&self) -> &str {
        "hyperliquid"
    }

    fn is_connected(&self) -> bool {
        // Use tokio block_in_place to check connection in sync context
        tokio::task::block_in_place(|| {
            tokio::runtime::Handle::current().block_on(async {
                self.ws_stream.read().await.is_some()
            })
        })
    }
}
