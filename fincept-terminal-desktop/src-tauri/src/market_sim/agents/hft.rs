use super::{AgentAction, AgentView, TradingAgent};
use crate::market_sim::types::*;
use crate::market_sim::latency::Rng;

/// HFT agent that exploits stale quotes and speed advantages
pub struct HFTAgent {
    name: String,
    instrument_id: InstrumentId,
    /// Minimum edge required to trade (in price units)
    min_edge: Price,
    /// Position limit
    max_position: Qty,
    /// Current position
    position: Qty,
    /// Cooldown between trades
    trade_cooldown: Nanos,
    last_trade_time: Nanos,
    /// Track recent price for momentum detection
    price_history: Vec<(Nanos, Price)>,
    history_window: usize,
}

impl HFTAgent {
    pub fn new(name: String, instrument_id: InstrumentId, min_edge: Price, max_position: Qty) -> Self {
        Self {
            name,
            instrument_id,
            min_edge,
            max_position,
            position: 0,
            trade_cooldown: 100_000, // 100 microseconds
            last_trade_time: 0,
            price_history: Vec::new(),
            history_window: 100,
        }
    }
}

impl TradingAgent for HFTAgent {
    fn participant_type(&self) -> ParticipantType {
        ParticipantType::HFT
    }

    fn on_tick(&mut self, view: &AgentView, rng: &mut Rng) -> Vec<AgentAction> {
        if !matches!(view.market_phase, MarketPhase::ContinuousTrading) {
            return vec![];
        }

        if view.current_time - self.last_trade_time < self.trade_cooldown {
            return vec![];
        }

        let quote = match view.quotes.get(&self.instrument_id) {
            Some(q) if q.bid_price > 0 && q.ask_price > 0 => q,
            _ => return vec![],
        };

        // Track price
        self.price_history.push((view.current_time, quote.last_price));
        if self.price_history.len() > self.history_window {
            self.price_history.remove(0);
        }

        // Short-term momentum detection
        if self.price_history.len() < 10 {
            return vec![];
        }

        let recent_avg: f64 = self.price_history.iter().rev().take(5).map(|&(_, p)| p as f64).sum::<f64>() / 5.0;
        let older_avg: f64 = self.price_history.iter().rev().skip(5).take(5).map(|&(_, p)| p as f64).sum::<f64>() / 5.0;

        let momentum = recent_avg - older_avg;

        let mut actions = Vec::new();

        // Trade in direction of short-term momentum if edge exists
        if momentum > self.min_edge as f64 && self.position < self.max_position {
            // Price moving up, buy
            actions.push(AgentAction::SubmitOrder {
                instrument_id: self.instrument_id,
                side: Side::Buy,
                order_type: OrderType::Limit,
                price: quote.ask_price, // take the ask
                quantity: 100,
                time_in_force: TimeInForce::IOC,
                display_quantity: 100,
                hidden: false,
            });
            self.last_trade_time = view.current_time;
        } else if momentum < -(self.min_edge as f64) && self.position > -self.max_position {
            // Price moving down, sell
            actions.push(AgentAction::SubmitOrder {
                instrument_id: self.instrument_id,
                side: Side::Sell,
                order_type: OrderType::Limit,
                price: quote.bid_price, // hit the bid
                quantity: 100,
                time_in_force: TimeInForce::IOC,
                display_quantity: 100,
                hidden: false,
            });
            self.last_trade_time = view.current_time;
        }

        // Flatten position if holding too long
        if self.position.abs() > 0 && rng.next_f64() < 0.01 {
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
                quantity: self.position.abs().min(100),
                time_in_force: TimeInForce::IOC,
                display_quantity: self.position.abs().min(100),
                hidden: false,
            });
        }

        actions
    }

    fn on_fill(&mut self, trade: &Trade, my_participant_id: ParticipantId) {
        // Update position based on our role in the trade
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
