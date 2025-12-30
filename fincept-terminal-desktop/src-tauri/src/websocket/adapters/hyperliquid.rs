// HyperLiquid WebSocket Adapter (Stub - to be implemented)

use super::WebSocketAdapter;
use crate::websocket::types::*;
use async_trait::async_trait;
use serde_json::Value;

pub struct HyperLiquidAdapter {
    config: ProviderConfig,
    message_callback: Option<Box<dyn Fn(MarketMessage) + Send + Sync>>,
}

impl HyperLiquidAdapter {
    pub fn new(config: ProviderConfig) -> Self {
        Self {
            config,
            message_callback: None,
        }
    }
}

#[async_trait]
impl WebSocketAdapter for HyperLiquidAdapter {
    async fn connect(&mut self) -> anyhow::Result<()> {
        // TODO: Implement HyperLiquid connection
        Err(anyhow::anyhow!("HyperLiquid adapter not yet implemented"))
    }

    async fn disconnect(&mut self) -> anyhow::Result<()> {
        Ok(())
    }

    async fn subscribe(
        &mut self,
        _symbol: &str,
        _channel: &str,
        _params: Option<Value>,
    ) -> anyhow::Result<()> {
        Err(anyhow::anyhow!("HyperLiquid adapter not yet implemented"))
    }

    async fn unsubscribe(&mut self, _symbol: &str, _channel: &str) -> anyhow::Result<()> {
        Ok(())
    }

    fn set_message_callback(&mut self, callback: Box<dyn Fn(MarketMessage) + Send + Sync>) {
        self.message_callback = Some(callback);
    }

    fn provider_name(&self) -> &str {
        "hyperliquid"
    }

    fn is_connected(&self) -> bool {
        false
    }
}
