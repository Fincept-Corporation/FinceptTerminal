use super::{FyersAdapter, FyersDataType};
use super::{FYERS_HSM_WS_URL, SOURCE_IDENTIFIER};
use crate::websocket::types::*;
use crate::websocket::adapters::WebSocketAdapter;
use async_trait::async_trait;
use futures_util::SinkExt;
use serde_json::Value;
use std::sync::Arc;
use tokio_tungstenite::connect_async;
use tokio_tungstenite::tungstenite::Message;

#[async_trait]
impl WebSocketAdapter for FyersAdapter {
    async fn connect(&mut self) -> anyhow::Result<()> {
        if self.access_token.is_empty() {
            return Err(anyhow::anyhow!("Access token not configured. Format should be 'appid:accesstoken'"));
        }

        // Extract HSM key from JWT
        self.hsm_key = Self::extract_hsm_key(&self.access_token);
        if self.hsm_key.is_none() {
            return Err(anyhow::anyhow!("Failed to extract HSM key from access token"));
        }


        // Connect to WebSocket with Authorization header
        let url = url::Url::parse(FYERS_HSM_WS_URL)?;

        let request = tokio_tungstenite::tungstenite::http::Request::builder()
            .uri(FYERS_HSM_WS_URL)
            .header("Authorization", &self.access_token)
            .header("User-Agent", format!("{}/1.0", SOURCE_IDENTIFIER))
            .header("Host", url.host_str().unwrap_or("socket.fyers.in"))
            .header("Connection", "Upgrade")
            .header("Upgrade", "websocket")
            .header("Sec-WebSocket-Version", "13")
            .header("Sec-WebSocket-Key", tokio_tungstenite::tungstenite::handshake::client::generate_key())
            .body(())?;

        let (ws_stream, _response) = connect_async(request).await
            .map_err(|e| anyhow::anyhow!("WebSocket connection failed: {}", e))?;


        // Store stream
        {
            let mut stream_lock = self.ws_stream.write().await;
            *stream_lock = Some(ws_stream);
        }

        // Mark as connected
        {
            let mut connected = self.is_connected.write().await;
            *connected = true;
        }

        // Send authentication message
        let hsm_key = self.hsm_key.as_ref()
            .ok_or_else(|| anyhow::anyhow!("Fyers HSM key not set — call set_config with hsm_key before connecting"))?;
        let auth_msg = Self::create_auth_message(hsm_key);

        {
            let mut stream_lock = self.ws_stream.write().await;
            if let Some(stream) = stream_lock.as_mut() {
                stream.send(Message::Binary(auth_msg)).await
                    .map_err(|e| anyhow::anyhow!("Failed to send auth message: {}", e))?;
            }
        }

        // Start message processing loop
        let ws_stream_clone = Arc::clone(&self.ws_stream);
        let is_connected_clone = Arc::clone(&self.is_connected);
        let is_authenticated_clone = Arc::clone(&self.is_authenticated);
        let topic_mappings_clone = Arc::clone(&self.topic_mappings);
        let scrips_cache_clone = Arc::clone(&self.scrips_cache);
        let symbol_mappings_clone = Arc::clone(&self.symbol_mappings);
        let callback_clone = Arc::clone(&self.message_callback);

        tokio::spawn(async move {
            Self::start_message_loop(
                ws_stream_clone,
                is_connected_clone,
                is_authenticated_clone,
                topic_mappings_clone,
                scrips_cache_clone,
                symbol_mappings_clone,
                callback_clone,
            ).await;
        });

        // Wait briefly for authentication response (reduced from 500ms)
        tokio::time::sleep(tokio::time::Duration::from_millis(100)).await;

        Ok(())
    }

    async fn disconnect(&mut self) -> anyhow::Result<()> {
        let mut stream_lock = self.ws_stream.write().await;
        if let Some(mut stream) = stream_lock.take() {
            let _ = stream.close(None).await;
        }

        let mut connected = self.is_connected.write().await;
        *connected = false;

        let mut auth = self.is_authenticated.write().await;
        *auth = false;

        // Clear caches
        let mut subs = self.subscriptions.write().await;
        subs.clear();

        let mut topics = self.topic_mappings.write().await;
        topics.clear();

        let mut cache = self.scrips_cache.write().await;
        cache.clear();

        let mut symbols = self.symbol_mappings.write().await;
        symbols.clear();

        Ok(())
    }

    async fn subscribe(
        &mut self,
        symbol: &str,
        channel: &str,
        _params: Option<Value>,
    ) -> anyhow::Result<()> {

        // Determine data type from channel
        let data_type = match channel {
            "depth" | "market_depth" | "DepthUpdate" => FyersDataType::DepthUpdate,
            "index" | "IndexUpdate" => FyersDataType::IndexUpdate,
            _ => FyersDataType::SymbolUpdate,
        };

        // Check if we already have the fytoken cached
        let fytoken = {
            let cache = self.fytoken_cache.read().await;
            cache.get(symbol).cloned()
        };

        let fytoken_str = if let Some(fytoken) = fytoken {
            fytoken
        } else {
            // Fetch fytoken from API
            let fytokens = self.fetch_fytokens(&[symbol.to_string()]).await;

            if let Some(fytoken) = fytokens.get(symbol) {
                // Cache the fytoken
                {
                    let mut cache = self.fytoken_cache.write().await;
                    cache.insert(symbol.to_string(), fytoken.clone());
                }
                fytoken.clone()
            } else {
                // No fytoken available
                return Err(anyhow::anyhow!("Failed to get fytoken for symbol"));
            }
        };

        let is_index = symbol.contains("-INDEX");

        // Build list of HSM tokens to subscribe
        let mut hsm_tokens = Vec::new();

        // Always subscribe to symbol feed (sf|) for LTP/quote data
        if let Some(sf_token) = Self::fytoken_to_hsm_token(&fytoken_str, &FyersDataType::SymbolUpdate, is_index) {
            hsm_tokens.push(sf_token);
        }

        // If depth requested, also subscribe to depth feed (dp|)
        if data_type == FyersDataType::DepthUpdate && !is_index {
            if let Some(dp_token) = Self::fytoken_to_hsm_token(&fytoken_str, &FyersDataType::DepthUpdate, false) {
                hsm_tokens.push(dp_token);
            }
        }

        if hsm_tokens.is_empty() {
            return Err(anyhow::anyhow!("Failed to convert fytoken"));
        }


        // Store symbol mappings (HSM token -> original symbol)
        {
            let mut symbols = self.symbol_mappings.write().await;
            for token in &hsm_tokens {
                symbols.insert(token.clone(), symbol.to_string());
            }
        }

        // Create and send subscription message for all tokens
        let sub_msg = Self::create_subscription_message(&hsm_tokens, 11);

        let mut stream_lock = self.ws_stream.write().await;
        if let Some(stream) = stream_lock.as_mut() {
            stream.send(Message::Binary(sub_msg)).await
                .map_err(|e| anyhow::anyhow!("Failed to send subscription: {}", e))?;

            drop(stream_lock);

            // Store subscription
            let mut subs = self.subscriptions.write().await;
            subs.insert(symbol.to_string(), data_type);

            Ok(())
        } else {
            Err(anyhow::anyhow!("Not connected to WebSocket"))
        }
    }

    async fn unsubscribe(&mut self, symbol: &str, _channel: &str) -> anyhow::Result<()> {
        // Note: Fyers HSM doesn't support selective unsubscription
        // We just remove from our tracking
        let mut subs = self.subscriptions.write().await;
        subs.remove(symbol);

        let mut symbols = self.symbol_mappings.write().await;
        // Remove all mappings for this symbol
        symbols.retain(|_, v| v != symbol);

        Ok(())
    }

    fn set_message_callback(&mut self, callback: Box<dyn Fn(MarketMessage) + Send + Sync>) {
        if let Ok(mut cb_lock) = self.message_callback.write() {
            *cb_lock = Some(callback);
        }
    }

    fn provider_name(&self) -> &str {
        "fyers"
    }

    fn is_connected(&self) -> bool {
        futures::executor::block_on(async { *self.is_connected.read().await })
    }
}
