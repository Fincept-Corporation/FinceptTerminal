use super::{AgentAction, AgentView, TradingAgent};
use crate::market_sim::types::*;
use crate::market_sim::latency::Rng;

/// Detects large resting orders and front-runs them
pub struct SniperAgent {
    name: String,
    instrument_id: InstrumentId,
    /// Minimum size to trigger sniping
    min_target_size: Qty,
    snipe_size: Qty,
    position: Qty,
    max_position: Qty,
    cooldown: Nanos,
    last_snipe: Nanos,
}

impl SniperAgent {
    pub fn new(name: String, instrument_id: InstrumentId, min_target_size: Qty, snipe_size: Qty) -> Self {
        Self {
            name,
            instrument_id,
            min_target_size,
            snipe_size,
            position: 0,
            max_position: 5000,
            cooldown: 1_000_000, // 1ms
            last_snipe: 0,
        }
    }
}

impl TradingAgent for SniperAgent {
    fn participant_type(&self) -> ParticipantType {
        ParticipantType::SniperBot
    }

    fn on_tick(&mut self, view: &AgentView, _rng: &mut Rng) -> Vec<AgentAction> {
        if !matches!(view.market_phase, MarketPhase::ContinuousTrading) {
            return vec![];
        }

        if view.current_time - self.last_snipe < self.cooldown {
            return vec![];
        }

        let depth = match view.depth.get(&self.instrument_id) {
            Some(d) => d,
            None => return vec![],
        };

        let _quote = match view.quotes.get(&self.instrument_id) {
            Some(q) if q.bid_price > 0 && q.ask_price > 0 => q,
            _ => return vec![],
        };

        // Look for large resting orders to front-run
        // If big bid visible → someone wants to buy → buy before them
        for level in &depth.bids {
            if level.quantity >= self.min_target_size && self.position < self.max_position {
                self.last_snipe = view.current_time;
                return vec![AgentAction::SubmitOrder {
                    instrument_id: self.instrument_id,
                    side: Side::Buy,
                    order_type: OrderType::Limit,
                    price: level.price + 1, // penny jump
                    quantity: self.snipe_size,
                    time_in_force: TimeInForce::IOC,
                    display_quantity: self.snipe_size,
                    hidden: false,
                }];
            }
        }

        // Large ask → someone wants to sell → sell before them
        for level in &depth.asks {
            if level.quantity >= self.min_target_size && self.position > -self.max_position {
                self.last_snipe = view.current_time;
                return vec![AgentAction::SubmitOrder {
                    instrument_id: self.instrument_id,
                    side: Side::Sell,
                    order_type: OrderType::Limit,
                    price: level.price - 1,
                    quantity: self.snipe_size,
                    time_in_force: TimeInForce::IOC,
                    display_quantity: self.snipe_size,
                    hidden: false,
                }];
            }
        }

        vec![]
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
