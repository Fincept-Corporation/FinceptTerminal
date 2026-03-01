use super::{AgentAction, AgentView, TradingAgent};
use crate::market_sim::types::*;
use crate::market_sim::latency::Rng;

pub struct MomentumAgent {
    name: String,
    instrument_id: InstrumentId,
    /// Lookback periods for fast/slow moving averages
    fast_period: usize,
    slow_period: usize,
    position: Qty,
    max_position: Qty,
    order_size: Qty,
    price_history: Vec<f64>,
}

impl MomentumAgent {
    pub fn new(
        name: String,
        instrument_id: InstrumentId,
        fast_period: usize,
        slow_period: usize,
        max_position: Qty,
        order_size: Qty,
    ) -> Self {
        Self {
            name,
            instrument_id,
            fast_period,
            slow_period,
            position: 0,
            max_position,
            order_size,
            price_history: Vec::new(),
        }
    }
}

impl TradingAgent for MomentumAgent {
    fn participant_type(&self) -> ParticipantType {
        ParticipantType::Momentum
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
        if self.price_history.len() > self.slow_period * 2 {
            self.price_history.remove(0);
        }

        if self.price_history.len() < self.slow_period {
            return vec![];
        }

        let len = self.price_history.len();
        let fast_ma: f64 = self.price_history[len - self.fast_period..].iter().sum::<f64>() / self.fast_period as f64;
        let slow_ma: f64 = self.price_history[len - self.slow_period..].iter().sum::<f64>() / self.slow_period as f64;

        let mut actions = Vec::new();

        if fast_ma > slow_ma && self.position < self.max_position {
            // Bullish crossover
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
        } else if fast_ma < slow_ma && self.position > -self.max_position {
            // Bearish crossover
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
