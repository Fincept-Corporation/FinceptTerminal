use crate::market_sim::types::*;
use crate::market_sim::latency::Rng;
use std::collections::HashMap;

mod market_maker;
mod hft;
mod stat_arb;
mod momentum;
mod noise;
mod informed;
mod institutional;
mod spoofing;
mod sniper;

pub use market_maker::MarketMakerAgent;
pub use hft::HFTAgent;
pub use stat_arb::StatArbAgent;
pub use momentum::MomentumAgent;
pub use noise::NoiseTraderAgent;
pub use informed::InformedTraderAgent;
pub use institutional::{InstitutionalAgent, ExecutionAlgo};
pub use spoofing::SpoofingAgent;
pub use sniper::SniperAgent;

/// Action an agent wants to take
#[derive(Debug, Clone)]
pub enum AgentAction {
    SubmitOrder {
        instrument_id: InstrumentId,
        side: Side,
        order_type: OrderType,
        price: Price,
        quantity: Qty,
        time_in_force: TimeInForce,
        display_quantity: Qty,
        hidden: bool,
    },
    CancelOrder {
        instrument_id: InstrumentId,
        order_id: OrderId,
    },
    ModifyOrder {
        instrument_id: InstrumentId,
        order_id: OrderId,
        new_price: Price,
        new_quantity: Qty,
    },
    DoNothing,
}

/// Market state visible to an agent
#[derive(Debug, Clone)]
pub struct AgentView {
    pub current_time: Nanos,
    pub quotes: HashMap<InstrumentId, L1Quote>,
    pub depth: HashMap<InstrumentId, L2Snapshot>,
    pub position: HashMap<InstrumentId, Position>,
    pub cash_balance: f64,
    pub active_orders: Vec<(OrderId, InstrumentId, Side, Price, Qty)>,
    pub recent_trades: Vec<Trade>,
    pub market_phase: MarketPhase,
    pub news_sentiment: f64, // -1.0 to 1.0, from recent news
}

/// Trait that all trading agents implement
pub trait TradingAgent: Send {
    fn participant_type(&self) -> ParticipantType;
    fn on_tick(&mut self, view: &AgentView, rng: &mut Rng) -> Vec<AgentAction>;
    fn on_fill(&mut self, trade: &Trade, my_participant_id: ParticipantId);
    fn on_order_accepted(&mut self, order_id: OrderId, side: Side);
    fn on_cancel(&mut self, order_id: OrderId);
    fn name(&self) -> &str;
}
