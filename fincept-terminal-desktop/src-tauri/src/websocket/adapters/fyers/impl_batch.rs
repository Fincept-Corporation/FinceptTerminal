use super::{FyersAdapter, FyersDataType};
use futures_util::SinkExt;
use tokio_tungstenite::tungstenite::Message;

/// Subscribe to multiple symbols at once (batch subscription) - FAST VERSION
/// Pre-fetches all fytokens in ONE API call before subscribing
impl FyersAdapter {
    pub async fn subscribe_batch(
        &mut self,
        symbols: &[String],
        data_type: FyersDataType,
    ) -> anyhow::Result<()> {
        if symbols.is_empty() {
            return Ok(());
        }


        let _start_time = std::time::Instant::now();

        // Step 1: Batch fetch ALL fytokens in one API call (FAST!)
        let fytokens = self.fetch_fytokens(symbols).await;

        if fytokens.is_empty() {
            return Err(anyhow::anyhow!("Failed to fetch any fytokens from API"));
        }

        // Step 2: Convert fytokens to HSM tokens
        let mut hsm_tokens: Vec<String> = Vec::new();
        let mut symbol_to_hsm: Vec<(String, String)> = Vec::new();

        for symbol in symbols {
            let is_index = symbol.contains("-INDEX");

            if let Some(fytoken) = fytokens.get(symbol) {
                // Cache the fytoken
                {
                    let mut cache = self.fytoken_cache.write().await;
                    cache.insert(symbol.clone(), fytoken.clone());
                }

                // Always add sf| token for LTP
                if let Some(sf_token) = Self::fytoken_to_hsm_token(fytoken, &FyersDataType::SymbolUpdate, is_index) {
                    hsm_tokens.push(sf_token.clone());
                    symbol_to_hsm.push((sf_token, symbol.clone()));
                }

                // Also add dp| token if depth requested and not index
                if data_type == FyersDataType::DepthUpdate && !is_index {
                    if let Some(dp_token) = Self::fytoken_to_hsm_token(fytoken, &FyersDataType::DepthUpdate, false) {
                        hsm_tokens.push(dp_token.clone());
                        symbol_to_hsm.push((dp_token, symbol.clone()));
                    }
                }
            } else {
            }
        }


        // Step 3: Store symbol mappings
        {
            let mut symbol_map = self.symbol_mappings.write().await;
            for (hsm_token, symbol) in &symbol_to_hsm {
                symbol_map.insert(hsm_token.clone(), symbol.clone());
            }
        }

        // Step 4: Subscribe in batches (Fyers supports up to 5000, batch in 100)
        const BATCH_SIZE: usize = 100;

        for chunk in hsm_tokens.chunks(BATCH_SIZE) {
            let sub_msg = Self::create_subscription_message(&chunk.to_vec(), 11);

            let mut stream_lock = self.ws_stream.write().await;
            if let Some(stream) = stream_lock.as_mut() {
                stream.send(Message::Binary(sub_msg)).await
                    .map_err(|e| anyhow::anyhow!("Failed to send batch subscription: {}", e))?;

            } else {
                return Err(anyhow::anyhow!("Not connected to WebSocket"));
            }
            drop(stream_lock);

            // Minimal delay between batches
            if chunk.len() == BATCH_SIZE {
                tokio::time::sleep(tokio::time::Duration::from_millis(50)).await;
            }
        }

        // Store subscriptions
        {
            let mut subs = self.subscriptions.write().await;
            for symbol in symbols {
                subs.insert(symbol.clone(), data_type.clone());
            }
        }

        Ok(())
    }

    pub async fn unsubscribe_batch(&mut self, symbols: &[String]) -> anyhow::Result<()> {
        // Note: HSM doesn't support selective unsubscription
        // Just remove from tracking
        let mut subs = self.subscriptions.write().await;
        let mut symbol_map = self.symbol_mappings.write().await;

        for symbol in symbols {
            subs.remove(symbol);
            symbol_map.retain(|_, v| v != symbol);
        }

        Ok(())
    }

    /// Get current subscription count
    pub async fn subscription_count(&self) -> usize {
        let subs = self.subscriptions.read().await;
        subs.len()
    }

    /// Check if authenticated
    pub async fn is_authenticated(&self) -> bool {
        *self.is_authenticated.read().await
    }
}
