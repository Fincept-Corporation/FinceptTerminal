use super::{AgentAction, AgentView, TradingAgent};
use crate::market_sim::types::*;
use crate::market_sim::latency::Rng;

#[derive(Debug, Clone, Copy)]
pub enum ExecutionAlgo {
    VWAP,
    TWAP,
    PercentageOfVolume,
}

/// Large institutional trader that executes a block order over time
pub struct InstitutionalAgent {
    name: String,
    instrument_id: InstrumentId,
    /// Target total quantity to execute
    target_quantity: Qty,
    /// Side of the block
    side: Side,
    /// Executed so far
    executed: Qty,
    /// Execution style
    algo: ExecutionAlgo,
    /// Participation rate (% of volume)
    participation_rate: f64,
    /// Time slicing: total duration to execute over
    execution_window_nanos: Nanos,
    start_time: Nanos,
    /// Use hidden/iceberg orders
    use_iceberg: bool,
    display_ratio: f64,
}

impl InstitutionalAgent {
    pub fn new(
        name: String,
        instrument_id: InstrumentId,
        target_quantity: Qty,
        side: Side,
        algo: ExecutionAlgo,
        execution_window_nanos: Nanos,
    ) -> Self {
        Self {
            name,
            instrument_id,
            target_quantity,
            side,
            executed: 0,
            algo,
            participation_rate: 0.10, // 10% of volume
            execution_window_nanos,
            start_time: 0,
            use_iceberg: true,
            display_ratio: 0.2,
        }
    }
}

impl TradingAgent for InstitutionalAgent {
    fn participant_type(&self) -> ParticipantType {
        ParticipantType::Institutional
    }

    fn on_tick(&mut self, view: &AgentView, rng: &mut Rng) -> Vec<AgentAction> {
        if !matches!(view.market_phase, MarketPhase::ContinuousTrading) {
            return vec![];
        }

        if self.executed >= self.target_quantity {
            return vec![]; // done
        }

        if self.start_time == 0 {
            self.start_time = view.current_time;
        }

        let quote = match view.quotes.get(&self.instrument_id) {
            Some(q) if q.bid_price > 0 && q.ask_price > 0 => q,
            _ => return vec![],
        };

        let remaining = self.target_quantity - self.executed;

        // Calculate slice size based on algo
        let slice_size = match self.algo {
            ExecutionAlgo::TWAP => {
                let elapsed = view.current_time.saturating_sub(self.start_time);
                let total = self.execution_window_nanos;
                if total == 0 {
                    remaining
                } else {
                    let expected_progress = elapsed as f64 / total as f64;
                    let expected_executed = (self.target_quantity as f64 * expected_progress) as Qty;
                    let behind = expected_executed.saturating_sub(self.executed);
                    behind.max(10).min(remaining)
                }
            }
            ExecutionAlgo::VWAP => {
                // Participate at a fixed rate of recent volume
                let recent_vol = quote.volume;
                let target_vol = (recent_vol as f64 * self.participation_rate) as Qty;
                target_vol.max(10).min(remaining).min(500)
            }
            ExecutionAlgo::PercentageOfVolume => {
                // Simple POV
                (quote.last_size as f64 * self.participation_rate).max(10.0) as Qty
            }
        };

        if slice_size <= 0 || rng.next_f64() > 0.3 {
            return vec![];
        }

        let price = match self.side {
            Side::Buy => quote.ask_price, // take liquidity
            Side::Sell => quote.bid_price,
        };

        let display_qty = if self.use_iceberg {
            (slice_size as f64 * self.display_ratio).max(1.0) as Qty
        } else {
            slice_size
        };

        vec![AgentAction::SubmitOrder {
            instrument_id: self.instrument_id,
            side: self.side,
            order_type: if self.use_iceberg { OrderType::Iceberg } else { OrderType::Limit },
            price,
            quantity: slice_size,
            time_in_force: TimeInForce::IOC,
            display_quantity: display_qty,
            hidden: false,
        }]
    }

    fn on_fill(&mut self, trade: &Trade, my_participant_id: ParticipantId) {
        // Only count fills where we were the participant
        if trade.buyer_id == my_participant_id || trade.seller_id == my_participant_id {
            self.executed += trade.quantity;
        }
    }

    fn on_order_accepted(&mut self, _order_id: OrderId, _side: Side) {}

    fn on_cancel(&mut self, _order_id: OrderId) {}

    fn name(&self) -> &str {
        &self.name
    }
}
