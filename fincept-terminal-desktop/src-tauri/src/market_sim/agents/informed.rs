use super::{AgentAction, AgentView, TradingAgent};
use crate::market_sim::types::*;
use crate::market_sim::latency::Rng;

/// Trades on private information (news, earnings)
pub struct InformedTraderAgent {
    name: String,
    instrument_id: InstrumentId,
    /// Target price based on information
    target_price: f64,
    /// Confidence in information (0.0 to 1.0)
    confidence: f64,
    position: Qty,
    max_position: Qty,
    order_size: Qty,
    /// How aggressively to trade (0.0 = passive, 1.0 = aggressive)
    aggression: f64,
}

impl InformedTraderAgent {
    pub fn new(
        name: String,
        instrument_id: InstrumentId,
        target_price: f64,
        confidence: f64,
        max_position: Qty,
        order_size: Qty,
    ) -> Self {
        Self {
            name,
            instrument_id,
            target_price,
            confidence,
            position: 0,
            max_position,
            order_size,
            aggression: 0.7,
        }
    }

    pub fn update_target(&mut self, new_target: f64, confidence: f64) {
        self.target_price = new_target;
        self.confidence = confidence;
    }
}

impl TradingAgent for InformedTraderAgent {
    fn participant_type(&self) -> ParticipantType {
        ParticipantType::InformedTrader
    }

    fn on_tick(&mut self, view: &AgentView, rng: &mut Rng) -> Vec<AgentAction> {
        if !matches!(view.market_phase, MarketPhase::ContinuousTrading) {
            return vec![];
        }

        let quote = match view.quotes.get(&self.instrument_id) {
            Some(q) if q.bid_price > 0 && q.ask_price > 0 => q,
            _ => return vec![],
        };

        let mid = (quote.bid_price + quote.ask_price) as f64 / 2.0;
        let edge = (self.target_price - mid) / mid;

        // Only trade if edge is significant
        if edge.abs() < 0.001 {
            return vec![];
        }

        // Trade probability scales with confidence and edge
        let trade_prob = self.confidence * edge.abs().min(0.05) * 20.0;
        if rng.next_f64() > trade_prob {
            return vec![];
        }

        let mut actions = Vec::new();

        if edge > 0.0 && self.position < self.max_position {
            // Undervalued, buy
            let price = if rng.next_f64() < self.aggression {
                quote.ask_price // aggressive: take ask
            } else {
                quote.bid_price + 1 // passive: improve bid
            };
            actions.push(AgentAction::SubmitOrder {
                instrument_id: self.instrument_id,
                side: Side::Buy,
                order_type: OrderType::Limit,
                price,
                quantity: self.order_size,
                time_in_force: TimeInForce::IOC,
                display_quantity: self.order_size,
                hidden: false,
            });
        } else if edge < 0.0 && self.position > -self.max_position {
            // Overvalued, sell
            let price = if rng.next_f64() < self.aggression {
                quote.bid_price // aggressive: hit bid
            } else {
                quote.ask_price - 1 // passive: improve ask
            };
            actions.push(AgentAction::SubmitOrder {
                instrument_id: self.instrument_id,
                side: Side::Sell,
                order_type: OrderType::Limit,
                price,
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
