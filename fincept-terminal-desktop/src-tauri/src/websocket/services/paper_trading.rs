// Paper Trading Service (Stub - to be fully implemented)

use crate::websocket::types::*;
use dashmap::DashMap;
use std::sync::Arc;

pub struct PaperTradingService {
    price_cache: Arc<DashMap<String, f64>>,
}

impl PaperTradingService {
    pub fn new() -> Self {
        Self {
            price_cache: Arc::new(DashMap::new()),
        }
    }

    pub fn update_price(&self, data: &TickerData) {
        self.price_cache.insert(
            format!("{}.{}", data.provider, data.symbol),
            data.price
        );
    }

    pub fn get_price(&self, provider: &str, symbol: &str) -> Option<f64> {
        let key = format!("{}.{}", provider, symbol);
        self.price_cache.get(&key).map(|v| *v)
    }
}
