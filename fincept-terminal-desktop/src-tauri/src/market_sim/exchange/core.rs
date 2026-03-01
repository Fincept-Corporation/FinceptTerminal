use super::Exchange;
use crate::market_sim::types::*;
use crate::market_sim::matching_engine::MatchingEngine;
use crate::market_sim::risk_engine::{RiskEngine, RiskLimits};
use crate::market_sim::clearing::ClearingHouse;
use crate::market_sim::market_data::MarketDataFeed;
use crate::market_sim::latency::{LatencySimulator, PriceGenerator};
use crate::market_sim::auction::AuctionEngine;
use crate::market_sim::analytics::SimulationAnalytics;
use crate::market_sim::events::*;
use crate::market_sim::agents::TradingAgent;
use std::collections::HashMap;

impl Exchange {
    pub fn new(config: SimulationConfig) -> Self {
        let seed = config.random_seed;
        let mut exchange = Self {
            matching_engine: MatchingEngine::new(),
            risk_engine: RiskEngine::new(),
            clearing_house: ClearingHouse::new(),
            market_data: MarketDataFeed::new(),
            latency_sim: LatencySimulator::new(seed),
            analytics: SimulationAnalytics::new(),
            auction_engines: HashMap::new(),
            price_generators: HashMap::new(),
            accounts: HashMap::new(),
            agents: Vec::new(),
            event_log: EventLog::new(),
            current_time: config.market_open_nanos.saturating_sub(config.opening_auction_duration_nanos),
            market_phase: MarketPhase::PreOpen,
            config: config.clone(),
            next_participant_id: 1,
            tick_count: 0,
            halt_end_time: HashMap::new(),
            messages_this_second: 0,
            orders_this_second: 0,
            last_stats_time: 0,
        };

        // Register instruments
        for instrument in &config.instruments {
            exchange.matching_engine.add_instrument(instrument.clone());
            exchange.auction_engines.insert(instrument.id, AuctionEngine::new());

            // Create price generator (jump-diffusion by default)
            let pg = PriceGenerator::jump_diffusion(
                seed + instrument.id as u64,
                instrument.reference_price as f64,
                0.20,   // 20% annualized vol
                5.0,    // 5 jumps per year on average
                0.0,    // mean jump = 0
                0.02,   // jump std = 2%
            );
            exchange.price_generators.insert(instrument.id, pg);
        }

        exchange
    }

    /// Register a participant and get their ID
    pub fn register_participant(
        &mut self,
        name: String,
        participant_type: ParticipantType,
        initial_balance: f64,
        latency_tier: LatencyTier,
    ) -> ParticipantId {
        let id = self.next_participant_id;
        self.next_participant_id += 1;

        let account = ParticipantAccount::new(id, name.clone(), participant_type, initial_balance, latency_tier);

        // Set default risk limits based on participant type
        let limits = match participant_type {
            ParticipantType::MarketMaker => RiskLimits {
                max_position_per_instrument: 50_000,
                max_order_rate_per_sec: 5000,
                max_order_to_trade_ratio: 200.0,
                max_drawdown_pct: 0.05,
                ..Default::default()
            },
            ParticipantType::HFT | ParticipantType::SniperBot => RiskLimits {
                max_position_per_instrument: 10_000,
                max_order_rate_per_sec: 10_000,
                max_order_to_trade_ratio: 500.0,
                max_drawdown_pct: 0.03,
                ..Default::default()
            },
            ParticipantType::Institutional => RiskLimits {
                max_position_per_instrument: 200_000,
                max_order_rate_per_sec: 100,
                max_order_to_trade_ratio: 5.0,
                max_drawdown_pct: 0.10,
                ..Default::default()
            },
            ParticipantType::NoiseTrader | ParticipantType::RetailTrader => RiskLimits {
                max_position_per_instrument: 5_000,
                max_order_rate_per_sec: 10,
                max_order_to_trade_ratio: 3.0,
                max_drawdown_pct: 0.20,
                ..Default::default()
            },
            _ => RiskLimits::default(),
        };

        self.risk_engine.set_limits(id, limits);
        self.accounts.insert(id, account);
        self.analytics.get_or_create_participant(id, name, participant_type);

        id
    }

    /// Add a trading agent
    pub fn add_agent(&mut self, participant_id: ParticipantId, agent: Box<dyn TradingAgent>) {
        self.agents.push((participant_id, agent));
    }

    /// Inject a news event that affects prices
    pub fn inject_news(&mut self, headline: String, sentiment: f64, magnitude: f64, instruments: Vec<InstrumentId>) {
        for &inst_id in &instruments {
            if let Some(pg) = self.price_generators.get_mut(&inst_id) {
                pg.apply_shock(sentiment * magnitude);
            }
        }

        let event = SimEvent::NewsEvent(NewsInjectionEvent {
            headline,
            sentiment,
            impact_magnitude: magnitude,
            affected_instruments: instruments,
            timestamp: self.current_time,
        });
        self.event_log.record(event);
    }

    /// Run the full simulation
    pub fn run(&mut self) -> SimulationSnapshot {
        let end_time = self.config.market_close_nanos + 60_000_000_000; // 1min after close

        while self.current_time < end_time {
            self.tick();

            if matches!(self.market_phase, MarketPhase::PostClose) {
                break;
            }
        }

        self.snapshot()
    }

    /// Run N ticks (for incremental stepping from frontend)
    pub fn step(&mut self, n: u64) -> SimulationSnapshot {
        for _ in 0..n {
            self.tick();
        }
        self.snapshot()
    }

    /// Get current simulation state snapshot
    pub fn snapshot(&self) -> SimulationSnapshot {
        let mut quotes = Vec::new();
        let instrument_ids = self.matching_engine.instrument_ids();

        for &inst_id in &instrument_ids {
            if let Some(book) = self.matching_engine.get_book(inst_id) {
                quotes.push(book.l1_quote(self.current_time));
            }
        }

        let recent_trades: Vec<Trade> = self.event_log.events()
            .iter()
            .rev()
            .take(100)
            .filter_map(|e| match e {
                SimEvent::TradeExecuted(te) => Some(te.trade.clone()),
                _ => None,
            })
            .collect();

        let participant_summaries: Vec<ParticipantSummary> = self.accounts
            .values()
            .map(|a| {
                let position: Qty = a.positions.values().map(|p| p.net_quantity).sum();
                ParticipantSummary {
                    id: a.id,
                    name: a.name.clone(),
                    participant_type: a.participant_type,
                    pnl: a.total_pnl,
                    position,
                    trade_count: a.total_trades,
                    order_count: a.total_orders,
                    cancel_count: a.total_cancels,
                    is_active: a.is_active,
                    latency_tier: a.latency_tier,
                }
            })
            .collect();

        SimulationSnapshot {
            current_time: self.current_time,
            market_phase: self.market_phase,
            quotes,
            recent_trades,
            participant_summaries,
            total_volume: self.analytics.total_volume,
            total_trades: self.analytics.total_trades,
            circuit_breaker_active: !self.halt_end_time.is_empty(),
            messages_per_second: self.messages_this_second,
            orders_per_second: self.orders_this_second,
        }
    }
}
