use super::{AgentAction, AgentView, TradingAgent};
use crate::market_sim::types::*;
use crate::market_sim::latency::Rng;

/// Mean-reversion stat arb agent
pub struct StatArbAgent {
    name: String,
    instrument_id: InstrumentId,
    /// Moving average window
    ma_window: usize,
    /// Entry threshold in standard deviations
    entry_z_score: f64,
    /// Exit threshold
    exit_z_score: f64,
    /// Position limit
    max_position: Qty,
    /// Order size
    order_size: Qty,
    position: Qty,
    price_history: Vec<f64>,
}

impl StatArbAgent {
    pub fn new(
        name: String,
        instrument_id: InstrumentId,
        ma_window: usize,
        entry_z_score: f64,
        max_position: Qty,
        order_size: Qty,
    ) -> Self {
        Self {
            name,
            instrument_id,
            ma_window,
            entry_z_score,
            exit_z_score: 0.5,
            max_position,
            order_size,
            position: 0,
            price_history: Vec::new(),
        }
    }
}

impl TradingAgent for StatArbAgent {
    fn participant_type(&self) -> ParticipantType {
        ParticipantType::StatArb
    }

    fn on_tick(&mut self, view: &AgentView, _rng: &mut Rng) -> Vec<AgentAction> {
        if !matches!(view.market_phase, MarketPhase::ContinuousTrading) {
            return vec![];
        }

        let quote = match view.quotes.get(&self.instrument_id) {
            Some(q) if q.last_price > 0 => q,
            _ => return vec![],
        };

        self.price_history.push(quote.last_price as f64);
        if self.price_history.len() > self.ma_window * 2 {
            self.price_history.remove(0);
        }

        if self.price_history.len() < self.ma_window {
            return vec![];
        }

        // Calculate z-score
        let window = &self.price_history[self.price_history.len() - self.ma_window..];
        let mean: f64 = window.iter().sum::<f64>() / window.len() as f64;
        let variance: f64 = window.iter().map(|p| (p - mean).powi(2)).sum::<f64>() / window.len() as f64;
        let std_dev = variance.sqrt();

        if std_dev < 0.01 {
            return vec![];
        }

        let current_price = quote.last_price as f64;
        let z_score = (current_price - mean) / std_dev;

        let mut actions = Vec::new();

        if z_score > self.entry_z_score && self.position > -self.max_position {
            // Price too high, sell (expect reversion)
            actions.push(AgentAction::SubmitOrder {
                instrument_id: self.instrument_id,
                side: Side::Sell,
                order_type: OrderType::Limit,
                price: quote.bid_price,
                quantity: self.order_size,
                time_in_force: TimeInForce::IOC,
                display_quantity: self.order_size,
                hidden: false,
            });
        } else if z_score < -self.entry_z_score && self.position < self.max_position {
            // Price too low, buy
            actions.push(AgentAction::SubmitOrder {
                instrument_id: self.instrument_id,
                side: Side::Buy,
                order_type: OrderType::Limit,
                price: quote.ask_price,
                quantity: self.order_size,
                time_in_force: TimeInForce::IOC,
                display_quantity: self.order_size,
                hidden: false,
            });
        } else if z_score.abs() < self.exit_z_score && self.position != 0 {
            // Close position
            let (side, price) = if self.position > 0 {
                (Side::Sell, quote.bid_price)
            } else {
                (Side::Buy, quote.ask_price)
            };
            actions.push(AgentAction::SubmitOrder {
                instrument_id: self.instrument_id,
                side,
                order_type: OrderType::Limit,
                price,
                quantity: self.position.abs().min(self.order_size),
                time_in_force: TimeInForce::IOC,
                display_quantity: self.position.abs().min(self.order_size),
                hidden: false,
            });
        }

        actions
    }

    fn on_fill(&mut self, trade: &Trade, my_participant_id: ParticipantId) {
        if trade.buyer_id == my_participant_id {
            self.position += trade.quantity;
        } else if trade.seller_id == my_participant_id {
            self.position -= trade.quantity;
        }
    }

    fn on_order_accepted(&mut self, _order_id: OrderId, _side: Side) {}

    fn on_cancel(&mut self, _order_id: OrderId) {}

    fn name(&self) -> &str {
        &self.name
    }
}
