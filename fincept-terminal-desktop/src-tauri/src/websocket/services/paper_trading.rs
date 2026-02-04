#![allow(dead_code)]
// Paper Trading WebSocket Service - uses unified paper_trading module

use crate::paper_trading;

pub struct PaperTradingService;

impl PaperTradingService {
    pub fn new() -> Self {
        Self
    }

    pub fn update_price(&self, portfolio_id: &str, symbol: &str, price: f64) {
        let _ = paper_trading::update_position_price(portfolio_id, symbol, price);
    }
}

impl Default for PaperTradingService {
    fn default() -> Self {
        Self::new()
    }
}
