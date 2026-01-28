// WebSocket Manager - Connection pool and lifecycle management
//
// Responsibilities:
// - Maintain single connection per provider (connection pooling)
// - Multiplex thousands of subscriptions over each connection
// - Auto-reconnection with subscription restoration
// - Lifecycle management (connect, disconnect, cleanup)
// - Metrics tracking

use super::adapters::{create_adapter, WebSocketAdapter};
use super::router::MessageRouter;
use super::types::*;
use dashmap::DashMap;
use std::collections::HashSet;
use std::sync::Arc;
use std::time::{Duration, SystemTime, UNIX_EPOCH};
use tokio::sync::{Mutex, RwLock};
use tokio::time;

/// WebSocket Manager - orchestrates all WebSocket connections
pub struct WebSocketManager {
    // Connection pool (1 connection per provider)
    connections: Arc<DashMap<String, Arc<RwLock<Box<dyn WebSocketAdapter>>>>>,

    // Message router
    router: Arc<RwLock<MessageRouter>>,

    // Provider configurations
    configs: Arc<DashMap<String, ProviderConfig>>,

    // Connection metrics
    metrics: Arc<DashMap<String, ConnectionMetrics>>,

    // Subscription tracking (provider -> symbol -> channels as HashSet to prevent duplicates)
    subscriptions: Arc<DashMap<String, DashMap<String, HashSet<String>>>>,

    // Connection locks to prevent race conditions (one mutex per provider)
    connection_locks: Arc<DashMap<String, Arc<Mutex<()>>>>,
}

impl WebSocketManager {
    pub fn new(router: Arc<RwLock<MessageRouter>>) -> Self {
        Self {
            connections: Arc::new(DashMap::new()),
            router,
            configs: Arc::new(DashMap::new()),
            metrics: Arc::new(DashMap::new()),
            subscriptions: Arc::new(DashMap::new()),
            connection_locks: Arc::new(DashMap::new()),
        }
    }

    /// Get or create a connection lock for a provider (prevents race conditions)
    fn get_connection_lock(&self, provider: &str) -> Arc<Mutex<()>> {
        self.connection_locks
            .entry(provider.to_string())
            .or_insert_with(|| Arc::new(Mutex::new(())))
            .clone()
    }

    // ========================================================================
    // CONFIGURATION
    // ========================================================================

    /// Set provider configuration
    pub fn set_config(&self, config: ProviderConfig) {
        self.configs.insert(config.name.clone(), config);
    }

    /// Get provider configuration
    pub fn get_config(&self, provider: &str) -> Option<ProviderConfig> {
        self.configs.get(provider).map(|c| c.clone())
    }

    // ========================================================================
    // CONNECTION MANAGEMENT
    // ========================================================================

    /// Connect to a provider (thread-safe with proper locking)
    pub async fn connect(&self, provider: &str) -> Result<()> {
        // Acquire lock for this provider to prevent race conditions
        let lock = self.get_connection_lock(provider);
        let _guard = lock.lock().await;

        // Now safely check if already connected (inside the lock)
        if self.is_connected(provider) {
            return Ok(()); // Return Ok instead of error - connection exists
        }

        self.connect_internal(provider).await
    }

    async fn connect_internal(&self, provider: &str) -> Result<()> {
        // Get configuration
        let config = self.get_config(provider)
            .ok_or_else(|| WebSocketError::ProviderNotFound(provider.to_string()))?;

        if !config.enabled {
            return Err(WebSocketError::ConnectionError(
                format!("Provider {} is disabled", provider)
            ));
        }

        // Create adapter
        let mut adapter = create_adapter(provider, config.clone())
            .map_err(|e| WebSocketError::ConnectionError(e.to_string()))?;

        // Set message callback
        let router = self.router.clone();
        adapter.set_message_callback(Box::new(move |msg| {
            let router = router.clone();
            tokio::spawn(async move {
                router.read().await.route(msg).await;
            });
        }));

        // Connect
        adapter.connect().await
            .map_err(|e| WebSocketError::ConnectionError(e.to_string()))?;

        // Store connection
        self.connections.insert(
            provider.to_string(),
            Arc::new(RwLock::new(adapter))
        );

        // Initialize metrics
        let metrics = ConnectionMetrics {
            provider: provider.to_string(),
            status: ConnectionStatus::Connected,
            connected_at: Some(Self::now()),
            ..Default::default()
        };
        self.metrics.insert(provider.to_string(), metrics);

        // Update status
        self.emit_status(provider, ConnectionStatus::Connected, None).await;

        Ok(())
    }

    /// Disconnect from a provider
    pub async fn disconnect(&self, provider: &str) -> Result<()> {
        if let Some((_, adapter)) = self.connections.remove(provider) {
            adapter.write().await.disconnect().await
                .map_err(|e| WebSocketError::ConnectionError(e.to_string()))?;

            // Update metrics
            if let Some(mut metrics) = self.metrics.get_mut(provider) {
                metrics.status = ConnectionStatus::Disconnected;
                metrics.connected_at = None;
            }

            // Update status
            self.emit_status(provider, ConnectionStatus::Disconnected, None).await;

            Ok(())
        } else {
            Err(WebSocketError::NotConnected(provider.to_string()))
        }
    }

    /// Reconnect to a provider with exponential backoff
    pub async fn reconnect(&self, provider: &str) -> Result<()> {
        // Get existing subscriptions before disconnecting
        let subs = self.get_provider_subscriptions(provider);

        // Get config for reconnect settings
        let config = self.get_config(provider);
        let base_delay = config.as_ref().map(|c| c.reconnect_delay_ms).unwrap_or(5000);
        let max_attempts = config.as_ref().map(|c| c.max_reconnect_attempts).unwrap_or(10);

        // Disconnect
        let _ = self.disconnect(provider).await;

        // Exponential backoff reconnection
        let mut attempt = 0;
        let mut last_error = None;

        while attempt < max_attempts {
            // Calculate delay with exponential backoff (capped at 60 seconds)
            let delay = std::cmp::min(
                base_delay * (2_u64.pow(attempt)),
                60_000
            );

            time::sleep(Duration::from_millis(delay)).await;

            // Update status
            self.emit_status(provider, ConnectionStatus::Reconnecting,
                Some(format!("Attempt {} of {}", attempt + 1, max_attempts))).await;

            // Try to connect
            match self.connect(provider).await {
                Ok(_) => {
                    // Update reconnect count in metrics
                    if let Some(mut metrics) = self.metrics.get_mut(provider) {
                        metrics.reconnect_count += 1;
                    }

                    // Restore subscriptions
                    for (symbol, channels) in &subs {
                        for channel in channels {
                            let _ = self.subscribe(provider, symbol, channel, None).await;
                        }
                    }

                    return Ok(());
                }
                Err(e) => {
                    last_error = Some(e);
                    attempt += 1;
                }
            }
        }

        // All attempts failed
        self.emit_status(provider, ConnectionStatus::Error,
            Some(format!("Failed to reconnect after {} attempts", max_attempts))).await;

        Err(last_error.unwrap_or_else(|| WebSocketError::ConnectionError(
            format!("Failed to reconnect to {} after {} attempts", provider, max_attempts)
        )))
    }

    /// Check if provider is connected
    pub fn is_connected(&self, provider: &str) -> bool {
        self.connections.contains_key(provider)
    }

    // ========================================================================
    // SUBSCRIPTION MANAGEMENT
    // ========================================================================

    /// Subscribe to a channel
    pub async fn subscribe(
        &self,
        provider: &str,
        symbol: &str,
        channel: &str,
        params: Option<serde_json::Value>,
    ) -> Result<()> {
        // Check if already subscribed (prevent duplicates)
        if self.is_subscribed(provider, symbol, channel) {
            return Ok(()); // Already subscribed, no-op
        }

        // Ensure connected
        if !self.is_connected(provider) {
            self.connect(provider).await?;
        }

        // Get adapter
        let adapter = self.connections.get(provider)
            .ok_or_else(|| WebSocketError::NotConnected(provider.to_string()))?;

        // Subscribe via adapter
        adapter.write().await.subscribe(symbol, channel, params).await
            .map_err(|e| WebSocketError::SubscriptionError(e.to_string()))?;

        // Track subscription using HashSet (prevents duplicates)
        self.subscriptions
            .entry(provider.to_string())
            .or_insert_with(DashMap::new)
            .entry(symbol.to_string())
            .or_insert_with(HashSet::new)
            .insert(channel.to_string());

        // Update metrics
        if let Some(mut metrics) = self.metrics.get_mut(provider) {
            metrics.active_subscriptions = self.count_subscriptions(provider);
        }

        Ok(())
    }

    /// Check if already subscribed to a channel
    fn is_subscribed(&self, provider: &str, symbol: &str, channel: &str) -> bool {
        self.subscriptions.get(provider)
            .and_then(|subs| subs.get(symbol).map(|channels| channels.contains(channel)))
            .unwrap_or(false)
    }

    /// Unsubscribe from a channel
    pub async fn unsubscribe(
        &self,
        provider: &str,
        symbol: &str,
        channel: &str,
    ) -> Result<()> {
        // Get adapter
        let adapter = self.connections.get(provider)
            .ok_or_else(|| WebSocketError::NotConnected(provider.to_string()))?;

        // Unsubscribe via adapter
        adapter.write().await.unsubscribe(symbol, channel).await
            .map_err(|e| WebSocketError::SubscriptionError(e.to_string()))?;

        // Remove from tracking (HashSet automatically handles non-existent keys)
        if let Some(provider_subs) = self.subscriptions.get(provider) {
            if let Some(mut symbol_channels) = provider_subs.get_mut(symbol) {
                symbol_channels.remove(channel);
                if symbol_channels.is_empty() {
                    drop(symbol_channels); // Release the lock before removing
                    provider_subs.remove(symbol);
                }
            }
        }

        // Update metrics
        if let Some(mut metrics) = self.metrics.get_mut(provider) {
            metrics.active_subscriptions = self.count_subscriptions(provider);
        }

        Ok(())
    }

    /// Get all subscriptions for a provider (converts HashSet to Vec for iteration)
    fn get_provider_subscriptions(&self, provider: &str) -> Vec<(String, Vec<String>)> {
        self.subscriptions.get(provider)
            .map(|subs| {
                subs.iter()
                    .map(|entry| (entry.key().clone(), entry.value().iter().cloned().collect()))
                    .collect()
            })
            .unwrap_or_default()
    }

    /// Count total subscriptions for a provider
    fn count_subscriptions(&self, provider: &str) -> usize {
        self.subscriptions.get(provider)
            .map(|subs| {
                subs.iter()
                    .map(|entry| entry.value().len())
                    .sum()
            })
            .unwrap_or(0)
    }

    // ========================================================================
    // METRICS
    // ========================================================================

    /// Get metrics for a provider
    pub fn get_metrics(&self, provider: &str) -> Option<ConnectionMetrics> {
        self.metrics.get(provider).map(|m| m.clone())
    }

    /// Get all metrics
    pub fn get_all_metrics(&self) -> Vec<ConnectionMetrics> {
        self.metrics.iter()
            .map(|entry| entry.value().clone())
            .collect()
    }

    /// Update message count
    pub fn increment_message_count(&self, provider: &str) {
        if let Some(mut metrics) = self.metrics.get_mut(provider) {
            metrics.messages_received += 1;
            metrics.last_message_at = Some(Self::now());
        }
    }

    // ========================================================================
    // UTILITIES
    // ========================================================================

    async fn emit_status(&self, provider: &str, status: ConnectionStatus, message: Option<String>) {
        let status_data = StatusData {
            provider: provider.to_string(),
            status,
            message,
            timestamp: Self::now(),
        };
        self.router.read().await.route(MarketMessage::Status(status_data)).await;
    }

    fn now() -> u64 {
        SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .unwrap_or_else(|_| Duration::from_secs(0))
            .as_millis() as u64
    }

    /// Cleanup idle connections (called periodically)
    pub async fn cleanup_idle(&self, idle_timeout_ms: u64) {
        let now = Self::now();
        let mut to_remove = Vec::new();

        for entry in self.metrics.iter() {
            let provider = entry.key();
            let metrics = entry.value();

            if let Some(last_msg) = metrics.last_message_at {
                if now - last_msg > idle_timeout_ms {
                    // No subscriptions and idle
                    if metrics.active_subscriptions == 0 {
                        to_remove.push(provider.clone());
                    }
                }
            }
        }

        for provider in to_remove {
            let _ = self.disconnect(&provider).await;
        }
    }
}
