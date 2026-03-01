use super::{AgentAction, AgentView, TradingAgent};
use crate::market_sim::types::*;
use crate::market_sim::latency::Rng;

/// Random noise trader that provides background liquidity and volume
pub struct NoiseTraderAgent {
    name: String,
    instrument_id: InstrumentId,
    /// Probability of trading per tick
    trade_probability: f64,
    order_size_range: (Qty, Qty),
    position: Qty,
    max_position: Qty,
}

impl NoiseTraderAgent {
    pub fn new(
        name: String,
        instrument_id: InstrumentId,
        trade_probability: f64,
        order_size_range: (Qty, Qty),
        max_position: Qty,
    ) -> Self {
        Self {
            name,
            instrument_id,
            trade_probability,
            order_size_range,
            position: 0,
            max_position,
        }
    }
}

impl TradingAgent for NoiseTraderAgent {
    fn participant_type(&self) -> ParticipantType {
        ParticipantType::NoiseTrader
    }

    fn on_tick(&mut self, view: &AgentView, rng: &mut Rng) -> Vec<AgentAction> {
        if !matches!(view.market_phase, MarketPhase::ContinuousTrading) {
            return vec![];
        }

        if rng.next_f64() > self.trade_probability {
            return vec![];
        }

        let quote = match view.quotes.get(&self.instrument_id) {
            Some(q) if q.bid_price > 0 && q.ask_price > 0 => q,
            _ => return vec![],
        };

        let size = self.order_size_range.0
            + rng.next_bounded((self.order_size_range.1 - self.order_size_range.0) as u64) as Qty;

        // Random side with slight mean-reversion tendency
        let buy_prob = if self.position > 0 { 0.4 } else if self.position < 0 { 0.6 } else { 0.5 };
        let side = if rng.next_f64() < buy_prob { Side::Buy } else { Side::Sell };

        if (side == Side::Buy && self.position >= self.max_position)
            || (side == Side::Sell && self.position <= -self.max_position)
        {
            return vec![];
        }

        // Mix of market and limit orders
        let use_market = rng.next_f64() < 0.3;

        if use_market {
            vec![AgentAction::SubmitOrder {
                instrument_id: self.instrument_id,
                side,
                order_type: OrderType::Market,
                price: 0,
                quantity: size,
                time_in_force: TimeInForce::IOC,
                display_quantity: size,
                hidden: false,
            }]
        } else {
            // Limit order near the spread
            let spread = quote.ask_price - quote.bid_price;
            let offset = rng.next_bounded(spread.max(1) as u64) as Price;
            let price = match side {
                Side::Buy => quote.bid_price + offset,
                Side::Sell => quote.ask_price - offset,
            };

            vec![AgentAction::SubmitOrder {
                instrument_id: self.instrument_id,
                side,
                order_type: OrderType::Limit,
                price,
                quantity: size,
                time_in_force: TimeInForce::Day,
                display_quantity: size,
                hidden: false,
            }]
        }
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
