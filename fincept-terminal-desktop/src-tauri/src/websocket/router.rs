// Message Router - Event-driven message distribution
//
// Routes parsed WebSocket messages to multiple consumers:
// - Frontend UI (via Tauri events)
// - Paper trading engine
// - Arbitrage detector
// - Portfolio tracker
// - Analytics services

use super::types::*;
use dashmap::DashMap;
use serde::Serialize;
use std::sync::Arc;
use tauri::Emitter;
use tokio::sync::broadcast;

const CHANNEL_CAPACITY: usize = 1000;

/// Message router that broadcasts messages to multiple consumers
pub struct MessageRouter {
    // Broadcast channels for different message types
    ticker_tx: broadcast::Sender<TickerData>,
    orderbook_tx: broadcast::Sender<OrderBookData>,
    trade_tx: broadcast::Sender<TradeData>,
    candle_tx: broadcast::Sender<CandleData>,
    status_tx: broadcast::Sender<StatusData>,

    // Track which topics have frontend subscribers
    frontend_subscribers: Arc<DashMap<String, bool>>,

    // Tauri app handle for emitting events
    app_handle: Option<tauri::AppHandle>,
}

impl MessageRouter {
    pub fn new() -> Self {
        let (ticker_tx, _) = broadcast::channel(CHANNEL_CAPACITY);
        let (orderbook_tx, _) = broadcast::channel(CHANNEL_CAPACITY);
        let (trade_tx, _) = broadcast::channel(CHANNEL_CAPACITY);
        let (candle_tx, _) = broadcast::channel(CHANNEL_CAPACITY);
        let (status_tx, _) = broadcast::channel(CHANNEL_CAPACITY);

        Self {
            ticker_tx,
            orderbook_tx,
            trade_tx,
            candle_tx,
            status_tx,
            frontend_subscribers: Arc::new(DashMap::new()),
            app_handle: None,
        }
    }

    /// Set Tauri app handle for frontend event emission
    pub fn set_app_handle(&mut self, app_handle: tauri::AppHandle) {
        self.app_handle = Some(app_handle);
    }

    /// Register frontend subscriber for a topic
    pub fn subscribe_frontend(&self, topic: &str) {
        self.frontend_subscribers.insert(topic.to_string(), true);
    }

    /// Unregister frontend subscriber
    pub fn unsubscribe_frontend(&self, topic: &str) {
        self.frontend_subscribers.remove(topic);
    }

    /// Check if frontend is subscribed to topic
    fn has_frontend_subscriber(&self, provider: &str, symbol: &str, channel: &str) -> bool {
        // The incoming symbol is already normalized by adapters (e.g., "BTCUSD" without slash)
        // But frontend might register with slash format (e.g., "BTC/USD")

        // Normalize the symbol by removing any slashes
        let normalized_symbol = symbol.replace("/", "");

        // Build list of topic variants to check
        let mut topics_to_check = vec![
            format!("{}.{}.{}", provider, channel, symbol),           // Original (e.g., BTCUSD)
            format!("{}.{}.{}", provider, channel, normalized_symbol.clone()), // Normalized (e.g., BTCUSD)
        ];

        // Also check common slash patterns for crypto pairs
        // For symbols like BTCUSD, check BTC/USD
        if normalized_symbol.len() >= 6 {
            // Try standard crypto pair format (3 chars / remaining)
            let variant = format!("{}/{}", &normalized_symbol[..3], &normalized_symbol[3..]);
            topics_to_check.push(format!("{}.{}.{}", provider, channel, variant));
        }
        if normalized_symbol.len() >= 8 {
            // Try 4/4 split (e.g., DOGE/USDT -> DOGE/USDT)
            let variant = format!("{}/{}", &normalized_symbol[..4], &normalized_symbol[4..]);
            topics_to_check.push(format!("{}.{}.{}", provider, channel, variant));
        }

        // Return true if any topic matches
        topics_to_check.iter().any(|topic| self.frontend_subscribers.contains_key(topic))
    }

    /// Route message to all consumers
    pub async fn route(&self, message: MarketMessage) {
        match message {
            MarketMessage::Ticker(data) => {
                self.route_ticker(data).await;
            }
            MarketMessage::OrderBook(data) => {
                self.route_orderbook(data).await;
            }
            MarketMessage::Trade(data) => {
                self.route_trade(data).await;
            }
            MarketMessage::Candle(data) => {
                self.route_candle(data).await;
            }
            MarketMessage::Status(data) => {
                self.route_status(data).await;
            }
        }
    }

    // ========================================================================
    // ROUTING METHODS
    // ========================================================================

    async fn route_ticker(&self, data: TickerData) {
        // 1. Broadcast to backend services
        let _ = self.ticker_tx.send(data.clone());

        // 2. Emit to frontend if subscribed
        if self.has_frontend_subscriber(&data.provider, &data.symbol, "ticker") {
            self.emit_to_frontend("ws_ticker", &data);
        }
    }

    async fn route_orderbook(&self, data: OrderBookData) {
        // 1. Broadcast to backend services
        let _ = self.orderbook_tx.send(data.clone());

        // 2. Emit to frontend if subscribed
        if self.has_frontend_subscriber(&data.provider, &data.symbol, "book") {
            self.emit_to_frontend("ws_orderbook", &data);
        }
    }

    async fn route_trade(&self, data: TradeData) {
        // 1. Broadcast to backend services
        let _ = self.trade_tx.send(data.clone());

        // 2. Emit to frontend if subscribed
        if self.has_frontend_subscriber(&data.provider, &data.symbol, "trade") {
            self.emit_to_frontend("ws_trade", &data);
        }
    }

    async fn route_candle(&self, data: CandleData) {
        // 1. Broadcast to backend services
        let _ = self.candle_tx.send(data.clone());

        // 2. Emit to frontend if subscribed
        if self.has_frontend_subscriber(&data.provider, &data.symbol, "candle") {
            self.emit_to_frontend("ws_candle", &data);
        }
    }

    async fn route_status(&self, data: StatusData) {
        // 1. Broadcast to backend services
        let _ = self.status_tx.send(data.clone());

        // 2. Always emit status to frontend
        self.emit_to_frontend("ws_status", &data);
    }

    // ========================================================================
    // CHANNEL SUBSCRIPTIONS (for backend services)
    // ========================================================================

    /// Subscribe to ticker updates
    pub fn subscribe_ticker(&self) -> broadcast::Receiver<TickerData> {
        self.ticker_tx.subscribe()
    }

    /// Subscribe to orderbook updates
    pub fn subscribe_orderbook(&self) -> broadcast::Receiver<OrderBookData> {
        self.orderbook_tx.subscribe()
    }

    /// Subscribe to trade updates
    pub fn subscribe_trade(&self) -> broadcast::Receiver<TradeData> {
        self.trade_tx.subscribe()
    }

    /// Subscribe to candle updates
    pub fn subscribe_candle(&self) -> broadcast::Receiver<CandleData> {
        self.candle_tx.subscribe()
    }

    /// Subscribe to status updates
    pub fn subscribe_status(&self) -> broadcast::Receiver<StatusData> {
        self.status_tx.subscribe()
    }

    // ========================================================================
    // FRONTEND EMISSION
    // ========================================================================

    fn emit_to_frontend<T: Serialize + Clone>(&self, event: &str, payload: &T) {
        if let Some(app) = &self.app_handle {
            let _ = app.emit(event, payload);
        }
    }
}

impl Default for MessageRouter {
    fn default() -> Self {
        Self::new()
    }
}
