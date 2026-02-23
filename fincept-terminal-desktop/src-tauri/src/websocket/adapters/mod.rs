#![allow(dead_code)]
// WebSocket Adapters - Provider-specific implementations

use super::types::*;
use async_trait::async_trait;

// Crypto exchange adapters
pub mod kraken;
pub mod hyperliquid;
pub mod binance;
pub mod okx;
pub mod bybit;
pub mod coinbase;
pub mod kucoin;
pub mod gateio;
pub mod huobi;
pub mod mexc;
pub mod bitget;
pub mod bitfinex;

// Indian stock broker adapters
pub mod fyers;
pub mod zerodha;
pub mod upstox;
pub mod dhan;
pub mod angelone;
pub mod kotak;
pub mod groww;
pub mod aliceblue;
pub mod fivepaisa;
pub mod iifl;
pub mod motilal;
pub mod shoonya;

// Crypto exports
pub use kraken::KrakenAdapter;
pub use hyperliquid::HyperLiquidAdapter;
pub use binance::BinanceAdapter;
pub use okx::OkxAdapter;
pub use bybit::BybitAdapter;
pub use coinbase::CoinbaseAdapter;
pub use kucoin::KucoinAdapter;
pub use gateio::GateioAdapter;
pub use huobi::HuobiAdapter;
pub use mexc::MexcAdapter;
pub use bitget::BitgetAdapter;
pub use bitfinex::BitfinexAdapter;

// Indian broker exports
pub use fyers::FyersAdapter;
pub use zerodha::ZerodhaAdapter;
pub use upstox::UpstoxAdapter;
pub use dhan::DhanAdapter;
pub use angelone::AngelOneAdapter;
pub use kotak::KotakAdapter;
pub use groww::GrowwAdapter;
pub use aliceblue::AliceBlueAdapter;
pub use fivepaisa::FivePaisaAdapter;
pub use iifl::IiflAdapter;
pub use motilal::MotilalAdapter;
pub use shoonya::ShoonyaAdapter;

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
        // Crypto exchanges
        "kraken" => Ok(Box::new(KrakenAdapter::new(config))),
        "hyperliquid" => Ok(Box::new(HyperLiquidAdapter::new(config))),
        "binance" | "binanceus" => Ok(Box::new(BinanceAdapter::new(config))),
        "okx" => Ok(Box::new(OkxAdapter::new(config))),
        "bybit" => Ok(Box::new(BybitAdapter::new(config))),
        "coinbase" | "coinbasepro" => Ok(Box::new(CoinbaseAdapter::new(config))),
        "kucoin" => Ok(Box::new(KucoinAdapter::new(config))),
        "gateio" | "gate" => Ok(Box::new(GateioAdapter::new(config))),
        "huobi" | "htx" => Ok(Box::new(HuobiAdapter::new(config))),
        "mexc" => Ok(Box::new(MexcAdapter::new(config))),
        "bitget" => Ok(Box::new(BitgetAdapter::new(config))),
        "bitfinex" => Ok(Box::new(BitfinexAdapter::new(config))),
        // Indian stock brokers
        "fyers" => Ok(Box::new(FyersAdapter::new(config))),
        "zerodha" | "kite" => Ok(Box::new(ZerodhaAdapter::new(config))),
        "upstox" => Ok(Box::new(UpstoxAdapter::new(config))),
        "dhan" => Ok(Box::new(DhanAdapter::new(config))),
        "angelone" | "angel" | "smartapi" => Ok(Box::new(AngelOneAdapter::new(config))),
        "kotak" | "kotak_neo" => Ok(Box::new(KotakAdapter::new(config))),
        "groww" => Ok(Box::new(GrowwAdapter::new(config))),
        "aliceblue" | "alice" | "ant" => Ok(Box::new(AliceBlueAdapter::new(config))),
        "fivepaisa" | "5paisa" => Ok(Box::new(FivePaisaAdapter::new(config))),
        "iifl" | "iifl_securities" => Ok(Box::new(IiflAdapter::new(config))),
        "motilal" | "motilaloswal" => Ok(Box::new(MotilalAdapter::new(config))),
        "shoonya" | "finvasia" => Ok(Box::new(ShoonyaAdapter::new(config))),
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
