#![allow(dead_code)]
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
use std::collections::HashSet;
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::Arc;
use tauri::Emitter;
use tokio::sync::broadcast;

const CHANNEL_CAPACITY: usize = 1000;

/// Metrics for tracking message routing performance
#[derive(Debug, Default)]
pub struct RouterMetrics {
    pub messages_routed: AtomicU64,
    pub messages_dropped: AtomicU64,
    pub frontend_emissions: AtomicU64,
}

/// Message router that broadcasts messages to multiple consumers
pub struct MessageRouter {
    // Broadcast channels for different message types
    ticker_tx: broadcast::Sender<TickerData>,
    orderbook_tx: broadcast::Sender<OrderBookData>,
    trade_tx: broadcast::Sender<TradeData>,
    candle_tx: broadcast::Sender<CandleData>,
    status_tx: broadcast::Sender<StatusData>,

    // Track which topics have frontend subscribers (using HashSet for faster lookup)
    // Format: "provider.channel.normalizedSymbol" (e.g., "kraken.ticker.BTCUSD")
    frontend_subscribers: Arc<DashMap<String, bool>>,

    // Pre-computed normalized symbols for faster matching
    // Maps "provider.channel" -> Set of normalized symbols
    normalized_subscriptions: Arc<DashMap<String, HashSet<String>>>,

    // Tauri app handle for emitting events
    app_handle: Option<tauri::AppHandle>,

    // Routing metrics
    metrics: Arc<RouterMetrics>,
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
            normalized_subscriptions: Arc::new(DashMap::new()),
            app_handle: None,
            metrics: Arc::new(RouterMetrics::default()),
        }
    }

    /// Set Tauri app handle for frontend event emission
    pub fn set_app_handle(&mut self, app_handle: tauri::AppHandle) {
        self.app_handle = Some(app_handle);
    }

    /// Normalize a symbol by removing slashes, dashes, and underscores, then uppercase
    #[inline]
    fn normalize_symbol(symbol: &str) -> String {
        symbol
            .replace("XBT", "BTC")  // Kraken uses XBT; normalize to BTC for matching
            .replace("XDG", "DOGE") // Kraken uses XDG for DOGE
            .replace('/', "")
            .replace('-', "")
            .replace('_', "")
            .to_uppercase()
    }

    /// Register frontend subscriber for a topic (with normalized symbol caching)
    pub fn subscribe_frontend(&self, topic: &str) {
        self.frontend_subscribers.insert(topic.to_string(), true);

        // Parse topic and cache normalized symbol for fast lookup
        // Topic format: "provider.channel.symbol"
        let parts: Vec<&str> = topic.splitn(3, '.').collect();
        if parts.len() == 3 {
            // Normalize provider to lowercase for consistent matching with Rust adapters
            let provider = parts[0].to_lowercase();
            let channel = parts[1];
            let symbol = parts[2];
            let normalized = Self::normalize_symbol(symbol);

            let key = format!("{}.{}", provider, channel);
            self.normalized_subscriptions
                .entry(key)
                .or_insert_with(HashSet::new)
                .insert(normalized);
        }
    }

    /// Unregister frontend subscriber
    pub fn unsubscribe_frontend(&self, topic: &str) {
        self.frontend_subscribers.remove(topic);

        // Remove from normalized cache
        let parts: Vec<&str> = topic.splitn(3, '.').collect();
        if parts.len() == 3 {
            // Normalize provider to lowercase for consistent matching
            let provider = parts[0].to_lowercase();
            let channel = parts[1];
            let symbol = parts[2];
            let normalized = Self::normalize_symbol(symbol);

            let key = format!("{}.{}", provider, channel);
            if let Some(mut symbols) = self.normalized_subscriptions.get_mut(&key) {
                symbols.remove(&normalized);
            }
        }
    }

    /// Remove all frontend subscribers for a provider (called on ws_disconnect).
    /// This prevents stale subscriptions in the router cache when a provider
    /// is fully disconnected and later reconnected.
    pub fn clear_provider_subscribers(&self, provider: &str) {
        let provider_prefix = format!("{}.", provider.to_lowercase());

        // Remove matching frontend_subscriber topics
        self.frontend_subscribers.retain(|key, _| {
            !key.to_lowercase().starts_with(&provider_prefix)
        });

        // Remove matching normalized_subscription keys ("provider.channel")
        self.normalized_subscriptions.retain(|key, _| {
            !key.starts_with(&provider_prefix)
        });
    }

    /// Check if frontend is subscribed to topic (optimized - O(1) lookup)
    #[inline]
    fn has_frontend_subscriber(&self, provider: &str, symbol: &str, channel: &str) -> bool {
        // Normalize the incoming symbol once
        let normalized_symbol = Self::normalize_symbol(symbol);

        // Fast lookup using pre-computed normalized subscriptions
        let key = format!("{}.{}", provider, channel);
        if let Some(symbols) = self.normalized_subscriptions.get(&key) {
            return symbols.contains(&normalized_symbol);
        }

        false
    }

    /// Get routing metrics
    pub fn get_metrics(&self) -> &RouterMetrics {
        &self.metrics
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
        self.metrics.messages_routed.fetch_add(1, Ordering::Relaxed);

        // 1. Broadcast to backend services (handle lag gracefully)
        if let Err(broadcast::error::SendError(_)) = self.ticker_tx.send(data.clone()) {
            // No receivers or all receivers lagged - this is normal when no services are listening
            self.metrics.messages_dropped.fetch_add(1, Ordering::Relaxed);
        }

        // 2. Emit to frontend if subscribed
        if self.has_frontend_subscriber(&data.provider, &data.symbol, "ticker") {
            self.emit_to_frontend("ws_ticker", &data);
        }

        // 3. Update strategy price cache in SQLite (for live strategy runner)
        Self::update_price_cache(&data);
    }

    /// Write latest tick price to strategy_price_cache table for Python live runner
    fn update_price_cache(data: &TickerData) {
        use crate::database::pool::get_pool;
        use std::sync::atomic::{AtomicU64, Ordering as AtOrd};
        static PRICE_CACHE_WRITES: AtomicU64 = AtomicU64::new(0);

        if let Ok(pool) = get_pool() {
            if let Ok(conn) = pool.get() {
                let _ = conn.execute(
                    "INSERT OR REPLACE INTO strategy_price_cache
                     (symbol, provider, price, bid, ask, volume, high, low, open, change_percent, updated_at)
                     VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11)",
                    rusqlite::params![
                        data.symbol,
                        data.provider,
                        data.price,
                        data.bid,
                        data.ask,
                        data.volume,
                        data.high,
                        data.low,
                        data.open,
                        data.change_percent,
                        data.timestamp,
                    ],
                );

                // Log every 100th write to avoid spam
                let count = PRICE_CACHE_WRITES.fetch_add(1, AtOrd::Relaxed);
                if count % 100 == 0 {
                    println!("[PriceCache] #{} {} price={:.2} provider={}", count, data.symbol, data.price, data.provider);
                }
            }
        }
    }

    async fn route_orderbook(&self, data: OrderBookData) {
        self.metrics.messages_routed.fetch_add(1, Ordering::Relaxed);

        // 1. Broadcast to backend services (handle lag gracefully)
        if let Err(broadcast::error::SendError(_)) = self.orderbook_tx.send(data.clone()) {
            self.metrics.messages_dropped.fetch_add(1, Ordering::Relaxed);
        }

        // 2. Emit to frontend if subscribed
        if self.has_frontend_subscriber(&data.provider, &data.symbol, "book") {
            self.emit_to_frontend("ws_orderbook", &data);
        }
    }

    async fn route_trade(&self, data: TradeData) {
        self.metrics.messages_routed.fetch_add(1, Ordering::Relaxed);

        // 1. Broadcast to backend services (handle lag gracefully)
        if let Err(broadcast::error::SendError(_)) = self.trade_tx.send(data.clone()) {
            self.metrics.messages_dropped.fetch_add(1, Ordering::Relaxed);
        }

        // 2. Emit to frontend if subscribed
        if self.has_frontend_subscriber(&data.provider, &data.symbol, "trade") {
            self.emit_to_frontend("ws_trade", &data);
        }
    }

    async fn route_candle(&self, data: CandleData) {
        self.metrics.messages_routed.fetch_add(1, Ordering::Relaxed);

        // 1. Broadcast to backend services (handle lag gracefully)
        if let Err(broadcast::error::SendError(_)) = self.candle_tx.send(data.clone()) {
            self.metrics.messages_dropped.fetch_add(1, Ordering::Relaxed);
        }

        // 2. Emit to frontend if subscribed
        if self.has_frontend_subscriber(&data.provider, &data.symbol, "candle") {
            self.emit_to_frontend("ws_candle", &data);
        }
    }

    async fn route_status(&self, data: StatusData) {
        self.metrics.messages_routed.fetch_add(1, Ordering::Relaxed);

        // 1. Broadcast to backend services (handle lag gracefully)
        if let Err(broadcast::error::SendError(_)) = self.status_tx.send(data.clone()) {
            self.metrics.messages_dropped.fetch_add(1, Ordering::Relaxed);
        }

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
            if app.emit(event, payload).is_ok() {
                self.metrics.frontend_emissions.fetch_add(1, Ordering::Relaxed);
            }
        }
    }
}

impl Default for MessageRouter {
    fn default() -> Self {
        Self::new()
    }
}
