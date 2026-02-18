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
    auction_engines: HashMap<InstrumentId, AuctionEngine>,

    // Price generators per instrument
    price_generators: HashMap<InstrumentId, PriceGenerator>,

    // Participant accounts
    pub accounts: HashMap<ParticipantId, ParticipantAccount>,

    // Trading agents
    agents: Vec<(ParticipantId, Box<dyn TradingAgent>)>,

    // Event log for replay
    pub event_log: EventLog,

    // State
    pub current_time: Nanos,
    pub market_phase: MarketPhase,
    pub config: SimulationConfig,
    next_participant_id: ParticipantId,
    tick_count: u64,

    // Circuit breaker state
    halt_end_time: HashMap<InstrumentId, Nanos>,

    // Statistics
    pub messages_this_second: u64,
    pub orders_this_second: u64,
    last_stats_time: Nanos,
}

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

    /// Run the simulation for one tick
    pub fn tick(&mut self) {
        self.tick_count += 1;
        self.current_time += self.config.tick_interval_nanos;

        // Update per-second stats
        let one_second = 1_000_000_000;
        if self.current_time - self.last_stats_time >= one_second {
            self.messages_this_second = 0;
            self.orders_this_second = 0;
            self.last_stats_time = self.current_time;
        }

        // Phase transitions
        self.update_market_phase();

        // Check circuit breaker halts
        self.check_halts();

        // Update price generators (background price process)
        let dt = self.config.tick_interval_nanos as f64 / (252.0 * 23_400.0 * 1_000_000_000.0);
        let instrument_ids = self.matching_engine.instrument_ids();

        for &inst_id in &instrument_ids {
            if let Some(pg) = self.price_generators.get_mut(&inst_id) {
                pg.next_price(dt);
            }
        }

        // Update market data from books
        for &inst_id in &instrument_ids {
            if let Some(book) = self.matching_engine.get_book(inst_id) {
                self.market_data.update_from_book(book, self.current_time);
            }
        }

        // Collect agent actions
        let agent_actions = self.collect_agent_actions();

        // Process agent actions (with latency)
        for (participant_id, actions) in agent_actions {
            for action in actions {
                self.process_agent_action(participant_id, action);
            }
        }

        // Post-trade risk checks
        self.run_post_trade_checks();

        // Process settlements
        self.clearing_house.process_settlements(self.current_time, &mut self.accounts);

        // Update analytics spread/depth
        for &inst_id in &instrument_ids {
            if let Some(book) = self.matching_engine.get_book(inst_id) {
                if let Some(spread) = book.spread() {
                    self.analytics.get_or_create_instrument(inst_id).record_spread(spread);
                }
                if let (Some(_bid), Some(_ask)) = (book.best_bid, book.best_ask) {
                    let bid_depth = book.get_depth(Side::Buy, 1);
                    let ask_depth = book.get_depth(Side::Sell, 1);
                    let bid_size = bid_depth.first().map(|l| l.quantity).unwrap_or(0);
                    let ask_size = ask_depth.first().map(|l| l.quantity).unwrap_or(0);
                    self.analytics.get_or_create_instrument(inst_id).record_depth(bid_size, ask_size);
                }
            }
        }
    }

    fn update_market_phase(&mut self) {
        let old_phase = self.market_phase;

        let new_phase = if self.current_time < self.config.market_open_nanos.saturating_sub(self.config.opening_auction_duration_nanos) {
            MarketPhase::PreOpen
        } else if self.current_time < self.config.market_open_nanos {
            MarketPhase::OpeningAuction
        } else if self.current_time < self.config.market_close_nanos.saturating_sub(self.config.closing_auction_duration_nanos) {
            MarketPhase::ContinuousTrading
        } else if self.current_time < self.config.market_close_nanos {
            MarketPhase::ClosingAuction
        } else {
            MarketPhase::PostClose
        };

        if old_phase != new_phase {
            self.market_phase = new_phase;

            // Handle phase transitions
            match (old_phase, new_phase) {
                (MarketPhase::OpeningAuction, MarketPhase::ContinuousTrading) => {
                    self.run_auction(true);
                }
                (MarketPhase::ContinuousTrading, MarketPhase::ClosingAuction) => {
                    // Begin accumulating closing auction orders
                }
                (MarketPhase::ClosingAuction, MarketPhase::PostClose) => {
                    self.run_auction(false);
                }
                _ => {}
            }

            // Log phase change for all instruments
            for &inst_id in &self.matching_engine.instrument_ids() {
                let event = SimEvent::PhaseChange(PhaseChangeEvent {
                    instrument_id: inst_id,
                    old_phase,
                    new_phase,
                    timestamp: self.current_time,
                });
                self.event_log.record(event);
            }
        }
    }

    fn run_auction(&mut self, _is_opening: bool) {
        let instrument_ids = self.matching_engine.instrument_ids();
        let mut next_trade_id = self.analytics.total_trades + 1;

        for inst_id in instrument_ids {
            if let Some(auction) = self.auction_engines.get_mut(&inst_id) {
                let result = auction.execute(inst_id, self.current_time, &mut next_trade_id);

                // Process auction trades
                for trade in &result.trades {
                    self.clearing_house.register_trade(trade);

                    // Update participant positions
                    if let Some(buyer) = self.accounts.get_mut(&trade.buyer_id) {
                        let pos = buyer.get_or_create_position(trade.instrument_id);
                        pos.update_on_fill(Side::Buy, trade.quantity, trade.price);
                        buyer.total_trades += 1;
                    }
                    if let Some(seller) = self.accounts.get_mut(&trade.seller_id) {
                        let pos = seller.get_or_create_position(trade.instrument_id);
                        pos.update_on_fill(Side::Sell, trade.quantity, trade.price);
                        seller.total_trades += 1;
                    }

                    self.analytics.process_event(&SimEvent::TradeExecuted(TradeExecutedEvent {
                        trade: trade.clone(),
                    }));
                }

                // Update reference price
                if result.auction_price > 0 {
                    if let Some(book) = self.matching_engine.get_book_mut(inst_id) {
                        book.record_trade(result.auction_price, result.auction_volume);
                    }
                }

                self.event_log.record(SimEvent::AuctionResult(result));
                auction.clear();
            }
        }
    }

    fn check_halts(&mut self) {
        // Lift halts that have expired
        let mut lifted = Vec::new();
        for (&inst_id, &end_time) in &self.halt_end_time {
            if self.current_time >= end_time {
                lifted.push(inst_id);
                self.event_log.record(SimEvent::HaltLifted(HaltLiftedEvent {
                    instrument_id: inst_id,
                    timestamp: self.current_time,
                }));
            }
        }
        for id in lifted {
            self.halt_end_time.remove(&id);
        }

        // Check for new circuit breakers
        if !self.config.enable_circuit_breakers {
            return;
        }

        let instrument_ids = self.matching_engine.instrument_ids();
        for inst_id in instrument_ids {
            if self.halt_end_time.contains_key(&inst_id) {
                continue; // already halted
            }

            let (ref_price, last_price) = {
                let instrument = match self.matching_engine.get_instrument(inst_id) {
                    Some(i) => i,
                    None => continue,
                };
                let book = match self.matching_engine.get_book(inst_id) {
                    Some(b) => b,
                    None => continue,
                };
                (instrument.reference_price, book.last_trade_price)
            };

            if ref_price <= 0 || last_price <= 0 {
                continue;
            }

            let pct_change = (last_price - ref_price).abs() as f64 / ref_price as f64;

            for (idx, &(threshold, halt_duration)) in self.config.circuit_breaker_thresholds.iter().enumerate() {
                if pct_change >= threshold {
                    let level = match idx {
                        0 => CircuitBreakerLevel::Level1,
                        1 => CircuitBreakerLevel::Level2,
                        _ => CircuitBreakerLevel::Level3,
                    };

                    self.halt_end_time.insert(inst_id, self.current_time + halt_duration);
                    self.market_phase = MarketPhase::Halted;

                    let event = SimEvent::CircuitBreakerTriggered(CircuitBreakerEvent {
                        instrument_id: inst_id,
                        level,
                        trigger_price: last_price,
                        reference_price: ref_price,
                        halt_duration_nanos: halt_duration,
                        timestamp: self.current_time,
                    });
                    self.event_log.record(event.clone());
                    self.analytics.process_event(&event);
                    break;
                }
            }
        }
    }

    fn collect_agent_actions(&mut self) -> Vec<(ParticipantId, Vec<AgentAction>)> {
        let mut all_actions = Vec::new();

        // Build views for each agent
        let quotes: HashMap<InstrumentId, L1Quote> = self.matching_engine.instrument_ids()
            .iter()
            .filter_map(|&id| {
                self.matching_engine.get_book(id).map(|book| (id, book.l1_quote(self.current_time)))
            })
            .collect();

        let depth: HashMap<InstrumentId, L2Snapshot> = self.matching_engine.instrument_ids()
            .iter()
            .filter_map(|&id| {
                self.matching_engine.get_book(id).map(|book| (id, book.l2_snapshot(5, self.current_time)))
            })
            .collect();

        // Collect recent trades from event log
        let recent_trades: Vec<Trade> = self.event_log.events()
            .iter()
            .rev()
            .take(50)
            .filter_map(|e| match e {
                SimEvent::TradeExecuted(te) => Some(te.trade.clone()),
                _ => None,
            })
            .collect();

        for (participant_id, agent) in &mut self.agents {
            let account = match self.accounts.get(participant_id) {
                Some(a) => a,
                None => continue,
            };

            if !account.is_active || account.kill_switch_triggered {
                continue;
            }

            // Apply latency: agent sees slightly stale data
            let view = AgentView {
                current_time: self.current_time,
                quotes: quotes.clone(),
                depth: depth.clone(),
                position: account.positions.clone(),
                cash_balance: account.cash_balance,
                active_orders: Vec::new(), // simplified
                recent_trades: recent_trades.clone(),
                market_phase: self.market_phase,
                news_sentiment: 0.0,
            };

            let actions = agent.on_tick(&view, self.latency_sim.rng());
            if !actions.is_empty() {
                all_actions.push((*participant_id, actions));
            }
        }

        // Sort by latency tier (co-located first)
        all_actions.sort_by_key(|(pid, _)| {
            self.accounts.get(pid)
                .map(|a| a.latency_tier.base_latency_nanos())
                .unwrap_or(u64::MAX)
        });

        all_actions
    }

    fn process_agent_action(&mut self, participant_id: ParticipantId, action: AgentAction) {
        match action {
            AgentAction::SubmitOrder {
                instrument_id,
                side,
                order_type,
                price,
                quantity,
                time_in_force,
                display_quantity,
                hidden,
            } => {
                let order_id = self.matching_engine.allocate_order_id();

                let current_display = if matches!(order_type, OrderType::Iceberg) {
                    display_quantity.min(quantity)
                } else {
                    quantity
                };

                let order = Order {
                    id: order_id,
                    participant_id,
                    instrument_id,
                    side,
                    order_type,
                    time_in_force,
                    price,
                    quantity,
                    filled_quantity: 0,
                    remaining_quantity: quantity,
                    stop_price: 0,
                    display_quantity,
                    current_display,
                    hidden,
                    trailing_offset: 0,
                    status: OrderStatus::PendingNew,
                    timestamp: self.current_time,
                    last_update: self.current_time,
                    venue_id: 0,
                };

                // Pre-trade risk check
                let account = match self.accounts.get(&participant_id) {
                    Some(a) => a,
                    None => return,
                };

                let ref_price = self.matching_engine
                    .get_book(instrument_id)
                    .map(|b| b.last_trade_price)
                    .unwrap_or(0);

                if let Some(reject_reason) = self.risk_engine.check_order(&order, account, ref_price, self.current_time) {
                    let event = SimEvent::OrderRejected(OrderRejectedEvent {
                        order_id,
                        participant_id,
                        reason: reject_reason,
                        timestamp: self.current_time,
                    });
                    self.event_log.record(event.clone());
                    self.analytics.process_event(&event);
                    return;
                }

                // Submit to matching engine
                self.event_log.record(SimEvent::OrderSubmitted(OrderSubmittedEvent {
                    order: order.clone(),
                    timestamp: self.current_time,
                }));

                if let Some(account) = self.accounts.get_mut(&participant_id) {
                    account.total_orders += 1;
                }

                let events = self.matching_engine.process_order(order, self.current_time);
                self.orders_this_second += 1;

                // Process resulting events
                for event in events {
                    self.handle_engine_event(event);
                }
            }
            AgentAction::CancelOrder { instrument_id, order_id } => {
                let events = self.matching_engine.cancel_order(
                    instrument_id,
                    order_id,
                    participant_id,
                    self.current_time,
                );
                if let Some(account) = self.accounts.get_mut(&participant_id) {
                    account.total_cancels += 1;
                }
                for event in events {
                    self.handle_engine_event(event);
                }
            }
            AgentAction::ModifyOrder { instrument_id, order_id, new_price, new_quantity } => {
                let events = self.matching_engine.modify_order(
                    instrument_id,
                    order_id,
                    participant_id,
                    new_price,
                    new_quantity,
                    self.current_time,
                );
                for event in events {
                    self.handle_engine_event(event);
                }
            }
            AgentAction::DoNothing => {}
        }
    }

    fn handle_engine_event(&mut self, event: SimEvent) {
        self.messages_this_second += 1;
        self.analytics.process_event(&event);

        match &event {
            SimEvent::TradeExecuted(te) => {
                let trade = &te.trade;
                let instrument_id = trade.instrument_id;
                let timestamp = trade.timestamp;

                // Register with clearing
                self.clearing_house.register_trade(trade);

                // Update buyer
                if let Some(buyer) = self.accounts.get_mut(&trade.buyer_id) {
                    let pos = buyer.get_or_create_position(trade.instrument_id);
                    pos.update_on_fill(Side::Buy, trade.quantity, trade.price);
                    buyer.total_trades += 1;
                    buyer.update_pnl();
                    buyer.update_order_trade_ratio();
                }

                // Update seller
                if let Some(seller) = self.accounts.get_mut(&trade.seller_id) {
                    let pos = seller.get_or_create_position(trade.instrument_id);
                    pos.update_on_fill(Side::Sell, trade.quantity, trade.price);
                    seller.total_trades += 1;
                    seller.update_pnl();
                    seller.update_order_trade_ratio();
                }

                // Notify agents of fills
                for (pid, agent) in &mut self.agents {
                    if *pid == trade.buyer_id || *pid == trade.seller_id {
                        agent.on_fill(trade, *pid);
                    }
                }

                // Update trailing stops and check stop order triggers
                self.matching_engine.update_trailing_stops(instrument_id);
                let stop_events = self.matching_engine.check_stop_orders(instrument_id, timestamp);
                for stop_event in stop_events {
                    self.handle_engine_event(stop_event);
                }

                // Update pegged orders when BBO may have changed
                let peg_events = self.matching_engine.update_pegged_orders(instrument_id, timestamp);
                for peg_event in peg_events {
                    self.handle_engine_event(peg_event);
                }
            }
            SimEvent::OrderAccepted(oa) => {
                // Notify agent of accepted order so it can track order IDs
                for (pid, agent) in &mut self.agents {
                    if *pid == oa.participant_id {
                        agent.on_order_accepted(oa.order_id, oa.side);
                    }
                }
            }
            SimEvent::OrderCancelled(oc) => {
                for (pid, agent) in &mut self.agents {
                    if *pid == oc.participant_id {
                        agent.on_cancel(oc.order_id);
                    }
                }
            }
            SimEvent::KillSwitchTriggered(ks) => {
                self.risk_engine.activate_kill_switch(ks.participant_id);
                if let Some(account) = self.accounts.get_mut(&ks.participant_id) {
                    account.kill_switch_triggered = true;
                    account.is_active = false;
                }
            }
            _ => {}
        }

        self.event_log.record(event);
    }

    fn run_post_trade_checks(&mut self) {
        let participant_ids: Vec<ParticipantId> = self.accounts.keys().copied().collect();

        for pid in participant_ids {
            let account = match self.accounts.get(&pid) {
                Some(a) => a.clone(),
                None => continue,
            };

            if !account.is_active {
                continue;
            }

            let events = self.risk_engine.post_trade_check(&account, self.current_time);
            for event in events {
                self.handle_engine_event(event);
            }
        }
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

/// Create a default simulation with pre-configured instruments and agents
pub fn create_default_simulation() -> Exchange {
    let instrument = Instrument {
        id: 1,
        symbol: "SIM-AAPL".to_string(),
        tick_size: 1,        // $0.01
        lot_size: 1,
        min_quantity: 1,
        max_quantity: 1_000_000,
        price_band_pct: 0.20,
        reference_price: 15000,  // $150.00
        maker_fee_bps: -0.2,
        taker_fee_bps: 0.3,
        short_sell_allowed: true,
        tick_size_table: vec![],
    };

    let config = SimulationConfig {
        name: "Default Market Simulation".to_string(),
        instruments: vec![instrument],
        duration_nanos: 23_400_000_000_000,
        tick_interval_nanos: 1_000_000,     // 1ms
        random_seed: 42,
        enable_circuit_breakers: true,
        enable_auctions: true,
        ..Default::default()
    };

    let mut exchange = Exchange::new(config);

    // Register market makers (co-located)
    for i in 0..3 {
        let pid = exchange.register_participant(
            format!("MM-{}", i),
            ParticipantType::MarketMaker,
            10_000_000.0,
            LatencyTier::CoLocated,
        );
        let agent = Box::new(MarketMakerAgent::new(
            format!("MM-{}", i),
            1, // instrument_id
            2, // half_spread in ticks
            500, // quote size
            3,   // levels
            20_000, // max inventory
            1,   // tick size
        ));
        exchange.add_agent(pid, agent);
    }

    // Register HFT agents
    for i in 0..2 {
        let pid = exchange.register_participant(
            format!("HFT-{}", i),
            ParticipantType::HFT,
            5_000_000.0,
            LatencyTier::CoLocated,
        );
        let agent = Box::new(HFTAgent::new(
            format!("HFT-{}", i),
            1, 3, 5000,
        ));
        exchange.add_agent(pid, agent);
    }

    // Register stat arb agents
    for i in 0..2 {
        let pid = exchange.register_participant(
            format!("StatArb-{}", i),
            ParticipantType::StatArb,
            5_000_000.0,
            LatencyTier::ProximityHosted,
        );
        let agent = Box::new(StatArbAgent::new(
            format!("StatArb-{}", i),
            1, 50, 2.0, 10_000, 200,
        ));
        exchange.add_agent(pid, agent);
    }

    // Register momentum agents
    for i in 0..2 {
        let pid = exchange.register_participant(
            format!("Momentum-{}", i),
            ParticipantType::Momentum,
            3_000_000.0,
            LatencyTier::DirectConnect,
        );
        let agent = Box::new(MomentumAgent::new(
            format!("Momentum-{}", i),
            1, 10, 30, 10_000, 300,
        ));
        exchange.add_agent(pid, agent);
    }

    // Register noise traders (retail)
    for i in 0..10 {
        let pid = exchange.register_participant(
            format!("Noise-{}", i),
            ParticipantType::NoiseTrader,
            500_000.0,
            LatencyTier::Retail,
        );
        let agent = Box::new(NoiseTraderAgent::new(
            format!("Noise-{}", i),
            1, 0.005, (10, 200), 2000,
        ));
        exchange.add_agent(pid, agent);
    }

    // Register institutional trader
    {
        let pid = exchange.register_participant(
            "Institution-VWAP".to_string(),
            ParticipantType::Institutional,
            50_000_000.0,
            LatencyTier::DirectConnect,
        );
        let agent = Box::new(InstitutionalAgent::new(
            "Institution-VWAP".to_string(),
            1,
            50_000,
            Side::Buy,
            ExecutionAlgo::VWAP,
            10_000_000_000_000, // execute over full day
        ));
        exchange.add_agent(pid, agent);
    }

    // Register sniper bot
    {
        let pid = exchange.register_participant(
            "Sniper-1".to_string(),
            ParticipantType::SniperBot,
            2_000_000.0,
            LatencyTier::CoLocated,
        );
        let agent = Box::new(SniperAgent::new(
            "Sniper-1".to_string(),
            1, 5000, 100,
        ));
        exchange.add_agent(pid, agent);
    }

    exchange
}
