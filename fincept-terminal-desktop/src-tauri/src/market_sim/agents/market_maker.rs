use super::{AgentAction, AgentView, TradingAgent};
use crate::market_sim::types::*;
use crate::market_sim::latency::Rng;

/// Professional market maker: quotes two-sided, manages inventory risk,
/// captures bid-ask spread
pub struct MarketMakerAgent {
    name: String,
    instrument_id: InstrumentId,
    /// Base half-spread in ticks
    half_spread_ticks: i64,
    /// Quote size per level
    quote_size: Qty,
    /// Number of price levels to quote
    num_levels: usize,
    /// Inventory limit before skewing
    max_inventory: Qty,
    /// Current inventory
    inventory: Qty,
    /// Skew factor: how much to shift quotes per unit of inventory
    skew_per_unit: f64,
    /// Active order tracking
    active_bids: Vec<OrderId>,
    active_asks: Vec<OrderId>,
    /// Requote interval (ticks)
    requote_interval: u64,
    last_requote: Nanos,
    tick_size: Price,
}

impl MarketMakerAgent {
    pub fn new(
        name: String,
        instrument_id: InstrumentId,
        half_spread_ticks: i64,
        quote_size: Qty,
        num_levels: usize,
        max_inventory: Qty,
        tick_size: Price,
    ) -> Self {
        Self {
            name,
            instrument_id,
            half_spread_ticks,
            quote_size,
            num_levels,
            max_inventory,
            inventory: 0,
            skew_per_unit: 0.1,
            active_bids: Vec::new(),
            active_asks: Vec::new(),
            requote_interval: 10_000_000, // 10ms
            last_requote: 0,
            tick_size,
        }
    }
}

impl TradingAgent for MarketMakerAgent {
    fn participant_type(&self) -> ParticipantType {
        ParticipantType::MarketMaker
    }

    fn on_tick(&mut self, view: &AgentView, _rng: &mut Rng) -> Vec<AgentAction> {
        if !matches!(view.market_phase, MarketPhase::ContinuousTrading) {
            return vec![];
        }

        // Only requote periodically
        if view.current_time - self.last_requote < self.requote_interval {
            return vec![];
        }
        self.last_requote = view.current_time;

        let mut actions = Vec::new();

        // Cancel all existing orders
        for &bid_id in &self.active_bids {
            actions.push(AgentAction::CancelOrder {
                instrument_id: self.instrument_id,
                order_id: bid_id,
            });
        }
        for &ask_id in &self.active_asks {
            actions.push(AgentAction::CancelOrder {
                instrument_id: self.instrument_id,
                order_id: ask_id,
            });
        }
        self.active_bids.clear();
        self.active_asks.clear();

        // Get fair value from midpoint
        let quote = match view.quotes.get(&self.instrument_id) {
            Some(q) if q.bid_price > 0 && q.ask_price > 0 => q,
            _ => return actions,
        };

        let mid = (quote.bid_price + quote.ask_price) as f64 / 2.0;

        // Inventory skew: shift midpoint against inventory
        let skew = self.inventory as f64 * self.skew_per_unit * self.tick_size as f64;
        let fair_value = mid - skew;

        // Place orders at multiple levels
        for level in 0..self.num_levels {
            let offset = (self.half_spread_ticks + level as i64) * self.tick_size;
            let bid_price = (fair_value as Price) - offset;
            let ask_price = (fair_value as Price) + offset;

            // Reduce size when inventory is high
            let inventory_ratio = (self.inventory.abs() as f64 / self.max_inventory as f64).min(1.0);
            let bid_size = if self.inventory > 0 {
                ((self.quote_size as f64) * (1.0 - inventory_ratio * 0.8)) as Qty
            } else {
                self.quote_size
            };
            let ask_size = if self.inventory < 0 {
                ((self.quote_size as f64) * (1.0 - inventory_ratio * 0.8)) as Qty
            } else {
                self.quote_size
            };

            if bid_price > 0 && bid_size > 0 && self.inventory < self.max_inventory {
                actions.push(AgentAction::SubmitOrder {
                    instrument_id: self.instrument_id,
                    side: Side::Buy,
                    order_type: OrderType::Limit,
                    price: bid_price,
                    quantity: bid_size,
                    time_in_force: TimeInForce::Day,
                    display_quantity: bid_size,
                    hidden: false,
                });
            }

            if ask_price > 0 && ask_size > 0 && self.inventory > -self.max_inventory {
                actions.push(AgentAction::SubmitOrder {
                    instrument_id: self.instrument_id,
                    side: Side::Sell,
                    order_type: OrderType::Limit,
                    price: ask_price,
                    quantity: ask_size,
                    time_in_force: TimeInForce::Day,
                    display_quantity: ask_size,
                    hidden: false,
                });
            }
        }

        actions
    }

    fn on_fill(&mut self, trade: &Trade, my_participant_id: ParticipantId) {
        // Update inventory based on our role in the trade
        if trade.buyer_id == my_participant_id {
            self.inventory += trade.quantity;
        } else if trade.seller_id == my_participant_id {
            self.inventory -= trade.quantity;
        }
    }

    fn on_order_accepted(&mut self, order_id: OrderId, side: Side) {
        match side {
            Side::Buy => self.active_bids.push(order_id),
            Side::Sell => self.active_asks.push(order_id),
        }
    }

    fn on_cancel(&mut self, order_id: OrderId) {
        self.active_bids.retain(|&id| id != order_id);
        self.active_asks.retain(|&id| id != order_id);
    }

    fn name(&self) -> &str {
        &self.name
    }
}
