use super::{AgentAction, AgentView, TradingAgent};
use crate::market_sim::types::*;
use crate::market_sim::latency::Rng;

#[derive(Debug, Clone, Copy, PartialEq)]
enum SpoofState {
    Idle,
    SpoofPlaced,
    RealOrderSent,
}

/// Places large orders it intends to cancel before execution
/// Used to train spoofing detection algorithms
pub struct SpoofingAgent {
    name: String,
    instrument_id: InstrumentId,
    spoof_size: Qty,
    real_size: Qty,
    position: Qty,
    max_position: Qty,
    active_spoof_orders: Vec<OrderId>,
    /// Ticks to keep spoof order alive
    spoof_lifetime_ticks: u64,
    spoof_placed_time: Nanos,
    state: SpoofState,
}

impl SpoofingAgent {
    pub fn new(name: String, instrument_id: InstrumentId, spoof_size: Qty, real_size: Qty) -> Self {
        Self {
            name,
            instrument_id,
            spoof_size,
            real_size,
            position: 0,
            max_position: 5000,
            active_spoof_orders: Vec::new(),
            spoof_lifetime_ticks: 5_000_000, // 5ms
            spoof_placed_time: 0,
            state: SpoofState::Idle,
        }
    }
}

impl TradingAgent for SpoofingAgent {
    fn participant_type(&self) -> ParticipantType {
        ParticipantType::Spoofer
    }

    fn on_tick(&mut self, view: &AgentView, rng: &mut Rng) -> Vec<AgentAction> {
        if !matches!(view.market_phase, MarketPhase::ContinuousTrading) {
            return vec![];
        }

        let quote = match view.quotes.get(&self.instrument_id) {
            Some(q) if q.bid_price > 0 && q.ask_price > 0 => q,
            _ => return vec![],
        };

        let mut actions = Vec::new();

        match self.state {
            SpoofState::Idle => {
                if rng.next_f64() < 0.01 && self.position.abs() < self.max_position {
                    // Place a large spoof bid to push price up
                    let spoof_price = quote.bid_price - 1;
                    actions.push(AgentAction::SubmitOrder {
                        instrument_id: self.instrument_id,
                        side: Side::Buy,
                        order_type: OrderType::Limit,
                        price: spoof_price,
                        quantity: self.spoof_size,
                        time_in_force: TimeInForce::Day,
                        display_quantity: self.spoof_size,
                        hidden: false,
                    });
                    self.state = SpoofState::SpoofPlaced;
                    self.spoof_placed_time = view.current_time;
                }
            }
            SpoofState::SpoofPlaced => {
                // After spoof has had time to influence, sell into the improved price
                if view.current_time - self.spoof_placed_time > self.spoof_lifetime_ticks / 2 {
                    // Place real sell order
                    actions.push(AgentAction::SubmitOrder {
                        instrument_id: self.instrument_id,
                        side: Side::Sell,
                        order_type: OrderType::Limit,
                        price: quote.bid_price,
                        quantity: self.real_size,
                        time_in_force: TimeInForce::IOC,
                        display_quantity: self.real_size,
                        hidden: false,
                    });
                    self.state = SpoofState::RealOrderSent;
                }
            }
            SpoofState::RealOrderSent => {
                // Cancel spoof orders
                for &spoof_id in &self.active_spoof_orders {
                    actions.push(AgentAction::CancelOrder {
                        instrument_id: self.instrument_id,
                        order_id: spoof_id,
                    });
                }
                self.active_spoof_orders.clear();
                self.state = SpoofState::Idle;
            }
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

    fn on_order_accepted(&mut self, order_id: OrderId, side: Side) {
        // Track spoof orders (buys placed in SpoofPlaced state)
        if self.state == SpoofState::SpoofPlaced && side == Side::Buy {
            self.active_spoof_orders.push(order_id);
        }
    }

    fn on_cancel(&mut self, order_id: OrderId) {
        self.active_spoof_orders.retain(|&id| id != order_id);
    }

    fn name(&self) -> &str {
        &self.name
    }
}
