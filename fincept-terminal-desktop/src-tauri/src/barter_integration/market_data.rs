use crate::barter_integration::types::*;
use tokio::sync::mpsc;
use std::collections::HashMap;
use parking_lot::RwLock;
use std::sync::Arc;

 // Re-export for compatibility

/// Market data manager
pub struct MarketDataManager {
    streams: Arc<RwLock<HashMap<String, MarketStream>>>,
}

/// Individual market stream
struct MarketStream {
    exchange: Exchange,
    symbols: Vec<String>,
    _tx: mpsc::UnboundedSender<MarketEvent>,
}

/// Market events
#[derive(Debug, Clone)]
pub enum MarketEvent {
    Trade(Trade),
    OrderBook(OrderBook),
    Candle(Candle),
}

impl MarketDataManager {
    pub fn new() -> Self {
        Self {
            streams: Arc::new(RwLock::new(HashMap::new())),
        }
    }

    /// Start market data stream
    pub async fn start_stream(
        &self,
        config: MarketStreamConfig,
    ) -> BarterResult<String> {
        let stream_id = format!("{}_{}",
            format!("{:?}", config.exchange).to_lowercase(),
            config.symbols.join("_")
        );

        let (tx, mut rx) = mpsc::unbounded_channel();

        let stream = MarketStream {
            exchange: config.exchange.clone(),
            symbols: config.symbols.clone(),
            _tx: tx.clone(),
        };

        self.streams.write().insert(stream_id.clone(), stream);

        // Spawn background task to handle market data
        tokio::spawn(async move {
            while let Some(event) = rx.recv().await {
                // Process market events
                match event {
                    MarketEvent::Trade(trade) => {
                        tracing::debug!("Received trade: {:?}", trade);
                    }
                    MarketEvent::OrderBook(_book) => {
                        tracing::debug!("Received orderbook update");
                    }
                    MarketEvent::Candle(candle) => {
                        tracing::debug!("Received candle: {:?}", candle);
                    }
                }
            }
        });

        Ok(stream_id)
    }

    /// Stop market data stream
    pub async fn stop_stream(&self, stream_id: &str) -> BarterResult<()> {
        self.streams.write().remove(stream_id);
        Ok(())
    }

    /// Get active streams
    pub fn get_active_streams(&self) -> Vec<String> {
        self.streams.read().keys().cloned().collect()
    }

    /// Subscribe to specific symbols
    pub async fn subscribe(
        &self,
        stream_id: &str,
        symbols: Vec<String>,
    ) -> BarterResult<()> {
        let mut streams = self.streams.write();
        if let Some(stream) = streams.get_mut(stream_id) {
            for symbol in symbols {
                if !stream.symbols.contains(&symbol) {
                    stream.symbols.push(symbol);
                }
            }
            Ok(())
        } else {
            Err(BarterError::MarketData(format!(
                "Stream not found: {}",
                stream_id
            )))
        }
    }

    /// Unsubscribe from symbols
    pub async fn unsubscribe(
        &self,
        stream_id: &str,
        symbols: Vec<String>,
    ) -> BarterResult<()> {
        let mut streams = self.streams.write();
        if let Some(stream) = streams.get_mut(stream_id) {
            stream.symbols.retain(|s| !symbols.contains(s));
            Ok(())
        } else {
            Err(BarterError::MarketData(format!(
                "Stream not found: {}",
                stream_id
            )))
        }
    }
}

impl Default for MarketDataManager {
    fn default() -> Self {
        Self::new()
    }
}
