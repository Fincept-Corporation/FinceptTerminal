// WebSocket Adapters - Provider-specific implementations

use super::types::*;
use async_trait::async_trait;

pub mod kraken;
pub mod hyperliquid;
pub mod binance;
pub mod fyers;

pub use kraken::KrakenAdapter;
pub use hyperliquid::HyperLiquidAdapter;
pub use binance::BinanceAdapter;
pub use fyers::FyersAdapter;

// ============================================================================
// ADAPTER TRAIT
// ============================================================================

/// WebSocket adapter trait - all providers must implement this
#[async_trait]
pub trait WebSocketAdapter: Send + Sync {
    /// Connect to WebSocket
    async fn connect(&mut self) -> anyhow::Result<()>;

    /// Disconnect from WebSocket
    async fn disconnect(&mut self) -> anyhow::Result<()>;

    /// Subscribe to a channel
    async fn subscribe(
        &mut self,
        symbol: &str,
        channel: &str,
        params: Option<serde_json::Value>,
    ) -> anyhow::Result<()>;

    /// Unsubscribe from a channel
    async fn unsubscribe(&mut self, symbol: &str, channel: &str) -> anyhow::Result<()>;

    /// Set message callback
    fn set_message_callback(&mut self, callback: Box<dyn Fn(MarketMessage) + Send + Sync>);

    /// Get provider name
    fn provider_name(&self) -> &str;

    /// Check if connected
    fn is_connected(&self) -> bool;
}

// ============================================================================
// ADAPTER FACTORY
// ============================================================================

/// Create adapter for a provider
pub fn create_adapter(
    provider: &str,
    config: ProviderConfig,
) -> anyhow::Result<Box<dyn WebSocketAdapter>> {
    match provider.to_lowercase().as_str() {
        "kraken" => Ok(Box::new(KrakenAdapter::new(config))),
        "hyperliquid" => Ok(Box::new(HyperLiquidAdapter::new(config))),
        "binance" => Ok(Box::new(BinanceAdapter::new(config))),
        "fyers" => Ok(Box::new(FyersAdapter::new(config))),
        _ => Err(anyhow::anyhow!("Unknown provider: {}", provider)),
    }
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

/// Normalize symbol (remove special characters, uppercase)
pub fn normalize_symbol(symbol: &str) -> String {
    symbol
        .replace('/', "")
        .replace('-', "")
        .replace('_', "")
        .to_uppercase()
}
