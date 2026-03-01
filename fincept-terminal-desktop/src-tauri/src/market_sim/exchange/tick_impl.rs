use super::Exchange;
use crate::market_sim::types::*;
use crate::market_sim::events::*;
use crate::market_sim::agents::{AgentAction, AgentView};
use std::collections::HashMap;

impl Exchange {
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

    pub(super) fn update_market_phase(&mut self) {
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

    pub(super) fn run_auction(&mut self, _is_opening: bool) {
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

    pub(super) fn check_halts(&mut self) {
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

    pub(super) fn collect_agent_actions(&mut self) -> Vec<(ParticipantId, Vec<AgentAction>)> {
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

    pub(super) fn process_agent_action(&mut self, participant_id: ParticipantId, action: AgentAction) {
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

    pub(super) fn handle_engine_event(&mut self, event: SimEvent) {
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

    pub(super) fn run_post_trade_checks(&mut self) {
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
}
