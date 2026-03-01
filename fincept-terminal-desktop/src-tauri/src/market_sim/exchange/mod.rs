use crate::market_sim::types::*;
use crate::market_sim::matching_engine::MatchingEngine;
use crate::market_sim::risk_engine::{RiskEngine, RiskLimits};
use crate::market_sim::clearing::ClearingHouse;
use crate::market_sim::market_data::MarketDataFeed;
use crate::market_sim::latency::{LatencySimulator, PriceGenerator};
use crate::market_sim::auction::AuctionEngine;
use crate::market_sim::analytics::SimulationAnalytics;
use crate::market_sim::events::*;
use crate::market_sim::agents::*;
use std::collections::HashMap;

mod core;
mod tick_impl;
mod factory;

pub use factory::create_default_simulation;

/// The Exchange: ties everything together into a complete market simulation
pub struct Exchange {
    // Core components
    pub matching_engine: MatchingEngine,
    pub risk_engine: RiskEngine,
    pub clearing_house: ClearingHouse,
    pub market_data: MarketDataFeed,
    pub latency_sim: LatencySimulator,
    pub analytics: SimulationAnalytics,

    // Auction engines per instrument
    pub(super) auction_engines: HashMap<InstrumentId, AuctionEngine>,

    // Price generators per instrument
    pub(super) price_generators: HashMap<InstrumentId, PriceGenerator>,

    // Participant accounts
    pub accounts: HashMap<ParticipantId, ParticipantAccount>,

    // Trading agents
    pub(super) agents: Vec<(ParticipantId, Box<dyn TradingAgent>)>,

    // Event log for replay
    pub event_log: EventLog,

    // State
    pub current_time: Nanos,
    pub market_phase: MarketPhase,
    pub config: SimulationConfig,
    pub(super) next_participant_id: ParticipantId,
    pub(super) tick_count: u64,

    // Circuit breaker state
    pub(super) halt_end_time: HashMap<InstrumentId, Nanos>,

    // Statistics
    pub messages_this_second: u64,
    pub orders_this_second: u64,
    pub(super) last_stats_time: Nanos,
}
