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
use std::sync::Arc;
use std::time::{Duration, SystemTime, UNIX_EPOCH};
use tokio::sync::RwLock;
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

    // Subscription tracking (provider -> symbol -> channels)
    subscriptions: Arc<DashMap<String, DashMap<String, Vec<String>>>>,

    // Prevent duplicate connections
    connecting: Arc<DashMap<String, bool>>,
}

impl WebSocketManager {
    pub fn new(router: Arc<RwLock<MessageRouter>>) -> Self {
        Self {
            connections: Arc::new(DashMap::new()),
            router,
            configs: Arc::new(DashMap::new()),
            metrics: Arc::new(DashMap::new()),
            subscriptions: Arc::new(DashMap::new()),
            connecting: Arc::new(DashMap::new()),
        }
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

    /// Connect to a provider
    pub async fn connect(&self, provider: &str) -> Result<()> {
        // Check if already connected
        if self.is_connected(provider) {
            return Err(WebSocketError::AlreadyConnected(provider.to_string()));
        }

        // Check if already connecting
        if self.connecting.contains_key(provider) {
            return Ok(()); // Wait for existing connection attempt
        }

        self.connecting.insert(provider.to_string(), true);

        let result = self.connect_internal(provider).await;

        self.connecting.remove(provider);

        result
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

    /// Reconnect to a provider
    pub async fn reconnect(&self, provider: &str) -> Result<()> {
        // Get existing subscriptions
        let subs = self.get_provider_subscriptions(provider);

        // Disconnect
        let _ = self.disconnect(provider).await;

        // Small delay
        time::sleep(Duration::from_millis(1000)).await;

        // Connect
        self.connect(provider).await?;

        // Restore subscriptions
        for (symbol, channels) in subs {
            for channel in channels {
                let _ = self.subscribe(provider, &symbol, &channel, None).await;
            }
        }

        Ok(())
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

        // Track subscription
        self.subscriptions
            .entry(provider.to_string())
            .or_insert_with(DashMap::new)
            .entry(symbol.to_string())
            .or_insert_with(Vec::new)
            .push(channel.to_string());

        // Update metrics
        if let Some(mut metrics) = self.metrics.get_mut(provider) {
            metrics.active_subscriptions = self.count_subscriptions(provider);
        }

        Ok(())
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

        // Remove from tracking
        if let Some(provider_subs) = self.subscriptions.get(provider) {
            if let Some(mut symbol_channels) = provider_subs.get_mut(symbol) {
                symbol_channels.retain(|c| c != channel);
                if symbol_channels.is_empty() {
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

    /// Get all subscriptions for a provider
    fn get_provider_subscriptions(&self, provider: &str) -> Vec<(String, Vec<String>)> {
        self.subscriptions.get(provider)
            .map(|subs| {
                subs.iter()
                    .map(|entry| (entry.key().clone(), entry.value().clone()))
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
            .unwrap()
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
