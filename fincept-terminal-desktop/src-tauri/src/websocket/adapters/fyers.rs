// Fyers WebSocket Adapter (Stub - to be implemented)

use super::WebSocketAdapter;
use crate::websocket::types::*;
use async_trait::async_trait;
use serde_json::Value;

pub struct FyersAdapter {
    config: ProviderConfig,
    message_callback: Option<Box<dyn Fn(MarketMessage) + Send + Sync>>,
}

impl FyersAdapter {
    pub fn new(config: ProviderConfig) -> Self {
        Self {
            config,
            message_callback: None,
        }
    }
}

#[async_trait]
impl WebSocketAdapter for FyersAdapter {
    async fn connect(&mut self) -> anyhow::Result<()> {
        // TODO: Implement Fyers connection
        Err(anyhow::anyhow!("Fyers adapter not yet implemented"))
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
        Err(anyhow::anyhow!("Fyers adapter not yet implemented"))
    }

    async fn unsubscribe(&mut self, _symbol: &str, _channel: &str) -> anyhow::Result<()> {
        Ok(())
    }

    fn set_message_callback(&mut self, callback: Box<dyn Fn(MarketMessage) + Send + Sync>) {
        self.message_callback = Some(callback);
    }

    fn provider_name(&self) -> &str {
        "fyers"
    }

    fn is_connected(&self) -> bool {
        false
    }
}
