#![allow(dead_code)]
// Arbitrage Detection Service (Stub - to be fully implemented)

use crate::websocket::types::*;
use dashmap::DashMap;
use std::sync::Arc;

pub struct ArbitrageService {
    prices: Arc<DashMap<String, DashMap<String, f64>>>, // symbol -> provider -> price
}

impl ArbitrageService {
    pub fn new() -> Self {
        Self {
            prices: Arc::new(DashMap::new()),
        }
    }

    pub fn update_price(&self, data: &TickerData) {
        self.prices
            .entry(data.symbol.clone())
            .or_insert_with(DashMap::new)
            .insert(data.provider.clone(), data.price);
    }

    pub fn check_arbitrage(&self, symbol: &str) -> Option<f64> {
        if let Some(providers) = self.prices.get(symbol) {
            if providers.len() >= 2 {
                let prices: Vec<f64> = providers.iter().map(|p| *p.value()).collect();
                let min = prices.iter().copied().fold(f64::INFINITY, f64::min);
                let max = prices.iter().copied().fold(f64::NEG_INFINITY, f64::max);
                let spread = (max - min) / min * 100.0;
                return Some(spread);
            }
        }
        None
    }
}
