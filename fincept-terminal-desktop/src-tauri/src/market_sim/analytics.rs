use crate::market_sim::types::*;
use crate::market_sim::events::*;
use serde::{Deserialize, Serialize};
use std::collections::HashMap;

/// Comprehensive market quality and participant performance analytics
pub struct SimulationAnalytics {
    /// Per-instrument metrics
    instrument_metrics: HashMap<InstrumentId, InstrumentMetrics>,
    /// Per-participant metrics
    participant_metrics: HashMap<ParticipantId, ParticipantMetrics>,
    /// Global metrics
    pub total_trades: u64,
    pub total_volume: Qty,
    pub total_orders: u64,
    pub total_cancels: u64,
    pub total_events: u64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct InstrumentMetrics {
    pub instrument_id: InstrumentId,
    // Market quality
    pub avg_spread: f64,
    pub min_spread: Price,
    pub max_spread: Price,
    pub spread_samples: u64,
    pub avg_depth_at_bbo: f64,
    pub depth_samples: u64,
    // Volume
    pub total_volume: Qty,
    pub total_trades: u64,
    pub vwap: f64,
    pub volume_weighted_sum: f64,
    // Volatility
    pub realized_volatility: f64,
    pub returns: Vec<f64>,
    pub last_price: f64,
    // Resilience: how fast spread recovers after a trade
    pub avg_spread_recovery_nanos: f64,
    pub recovery_samples: u64,
    // Auction metrics
    pub opening_auction_volume: Qty,
    pub closing_auction_volume: Qty,
    // Circuit breaker
    pub circuit_breaker_count: u32,
    pub halt_duration_total: Nanos,
}

impl InstrumentMetrics {
    pub fn new(instrument_id: InstrumentId) -> Self {
        Self {
            instrument_id,
            avg_spread: 0.0,
            min_spread: Price::MAX,
            max_spread: 0,
            spread_samples: 0,
            avg_depth_at_bbo: 0.0,
            depth_samples: 0,
            total_volume: 0,
            total_trades: 0,
            vwap: 0.0,
            volume_weighted_sum: 0.0,
            realized_volatility: 0.0,
            returns: Vec::new(),
            last_price: 0.0,
            avg_spread_recovery_nanos: 0.0,
            recovery_samples: 0,
            opening_auction_volume: 0,
            closing_auction_volume: 0,
            circuit_breaker_count: 0,
            halt_duration_total: 0,
        }
    }

    pub fn record_spread(&mut self, spread: Price) {
        if spread < self.min_spread {
            self.min_spread = spread;
        }
        if spread > self.max_spread {
            self.max_spread = spread;
        }
        self.spread_samples += 1;
        self.avg_spread += (spread as f64 - self.avg_spread) / self.spread_samples as f64;
    }

    pub fn record_depth(&mut self, bid_size: Qty, ask_size: Qty) {
        let avg = (bid_size + ask_size) as f64 / 2.0;
        self.depth_samples += 1;
        self.avg_depth_at_bbo += (avg - self.avg_depth_at_bbo) / self.depth_samples as f64;
    }

    pub fn record_trade(&mut self, price: Price, qty: Qty) {
        self.total_trades += 1;
        self.total_volume += qty;
        self.volume_weighted_sum += price as f64 * qty as f64;
        if self.total_volume > 0 {
            self.vwap = self.volume_weighted_sum / self.total_volume as f64;
        }

        // Track returns for volatility
        let price_f = price as f64;
        if self.last_price > 0.0 {
            let ret = (price_f / self.last_price).ln();
            self.returns.push(ret);
        }
        self.last_price = price_f;

        // Realized volatility (annualized, assuming ~252 trading days * 23400 seconds)
        if self.returns.len() > 1 {
            let mean: f64 = self.returns.iter().sum::<f64>() / self.returns.len() as f64;
            let variance: f64 = self.returns.iter().map(|r| (r - mean).powi(2)).sum::<f64>()
                / (self.returns.len() - 1) as f64;
            self.realized_volatility = variance.sqrt();
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ParticipantMetrics {
    pub participant_id: ParticipantId,
    pub participant_type: ParticipantType,
    pub name: String,
    // P&L
    pub realized_pnl: f64,
    pub unrealized_pnl: f64,
    pub total_pnl: f64,
    pub pnl_history: Vec<(Nanos, f64)>,
    pub max_drawdown: f64,
    pub peak_pnl: f64,
    // Execution quality
    pub total_trades: u64,
    pub total_orders: u64,
    pub total_cancels: u64,
    pub order_to_trade_ratio: f64,
    pub fill_rate: f64,
    pub avg_fill_price: f64,
    pub total_filled_qty: Qty,
    pub total_notional: f64,
    // Fees
    pub total_maker_rebates: f64,
    pub total_taker_fees: f64,
    pub net_fees: f64,
    // Spread capture (for market makers)
    pub spread_captured: f64,
    // Market impact
    pub avg_market_impact: f64,
    pub impact_samples: u64,
    // Inventory
    pub max_position: Qty,
    pub avg_position: f64,
    pub position_samples: u64,
    // Latency stats
    pub avg_order_latency_nanos: f64,
    pub latency_samples: u64,
}

impl ParticipantMetrics {
    pub fn new(id: ParticipantId, name: String, ptype: ParticipantType) -> Self {
        Self {
            participant_id: id,
            participant_type: ptype,
            name,
            realized_pnl: 0.0,
            unrealized_pnl: 0.0,
            total_pnl: 0.0,
            pnl_history: Vec::new(),
            max_drawdown: 0.0,
            peak_pnl: 0.0,
            total_trades: 0,
            total_orders: 0,
            total_cancels: 0,
            order_to_trade_ratio: 0.0,
            fill_rate: 0.0,
            avg_fill_price: 0.0,
            total_filled_qty: 0,
            total_notional: 0.0,
            total_maker_rebates: 0.0,
            total_taker_fees: 0.0,
            net_fees: 0.0,
            spread_captured: 0.0,
            avg_market_impact: 0.0,
            impact_samples: 0,
            max_position: 0,
            avg_position: 0.0,
            position_samples: 0,
            avg_order_latency_nanos: 0.0,
            latency_samples: 0,
        }
    }

    pub fn record_trade(&mut self, price: Price, qty: Qty, is_maker: bool, fee_bps: f64) {
        self.total_trades += 1;
        let notional = price as f64 * qty as f64;
        self.total_notional += notional;
        self.total_filled_qty += qty;
        self.avg_fill_price = self.total_notional / self.total_filled_qty as f64;

        let fee = notional * fee_bps / 10_000.0;
        if is_maker {
            self.total_maker_rebates += fee.abs(); // rebate is negative fee
        } else {
            self.total_taker_fees += fee;
        }
        self.net_fees = self.total_taker_fees - self.total_maker_rebates;

        self.update_ratios();
    }

    pub fn record_order(&mut self) {
        self.total_orders += 1;
        self.update_ratios();
    }

    pub fn record_cancel(&mut self) {
        self.total_cancels += 1;
        self.update_ratios();
    }

    pub fn update_pnl(&mut self, realized: f64, unrealized: f64, timestamp: Nanos) {
        self.realized_pnl = realized;
        self.unrealized_pnl = unrealized;
        self.total_pnl = realized + unrealized - self.net_fees;
        self.pnl_history.push((timestamp, self.total_pnl));

        if self.total_pnl > self.peak_pnl {
            self.peak_pnl = self.total_pnl;
        }
        let drawdown = self.peak_pnl - self.total_pnl;
        if drawdown > self.max_drawdown {
            self.max_drawdown = drawdown;
        }
    }

    pub fn record_position(&mut self, position: Qty) {
        if position.abs() > self.max_position {
            self.max_position = position.abs();
        }
        self.position_samples += 1;
        self.avg_position += (position.abs() as f64 - self.avg_position) / self.position_samples as f64;
    }

    fn update_ratios(&mut self) {
        if self.total_trades > 0 {
            self.order_to_trade_ratio = self.total_orders as f64 / self.total_trades as f64;
            self.fill_rate = self.total_trades as f64 / self.total_orders as f64;
        }
    }

    /// Sharpe-like ratio (simplified, using PnL history)
    pub fn sharpe_ratio(&self) -> f64 {
        if self.pnl_history.len() < 2 {
            return 0.0;
        }

        let returns: Vec<f64> = self.pnl_history.windows(2).map(|w| w[1].1 - w[0].1).collect();
        let mean: f64 = returns.iter().sum::<f64>() / returns.len() as f64;
        let variance: f64 = returns.iter().map(|r| (r - mean).powi(2)).sum::<f64>() / returns.len() as f64;
        let std_dev = variance.sqrt();

        if std_dev > 0.0 {
            mean / std_dev
        } else {
            0.0
        }
    }
}

impl SimulationAnalytics {
    pub fn new() -> Self {
        Self {
            instrument_metrics: HashMap::new(),
            participant_metrics: HashMap::new(),
            total_trades: 0,
            total_volume: 0,
            total_orders: 0,
            total_cancels: 0,
            total_events: 0,
        }
    }

    pub fn get_or_create_instrument(&mut self, id: InstrumentId) -> &mut InstrumentMetrics {
        self.instrument_metrics
            .entry(id)
            .or_insert_with(|| InstrumentMetrics::new(id))
    }

    pub fn get_or_create_participant(
        &mut self,
        id: ParticipantId,
        name: String,
        ptype: ParticipantType,
    ) -> &mut ParticipantMetrics {
        self.participant_metrics
            .entry(id)
            .or_insert_with(|| ParticipantMetrics::new(id, name, ptype))
    }

    pub fn process_event(&mut self, event: &SimEvent) {
        self.total_events += 1;

        match event {
            SimEvent::TradeExecuted(te) => {
                self.total_trades += 1;
                self.total_volume += te.trade.quantity;

                let im = self.get_or_create_instrument(te.trade.instrument_id);
                im.record_trade(te.trade.price, te.trade.quantity);
            }
            SimEvent::OrderAccepted(_) => {
                self.total_orders += 1;
            }
            SimEvent::OrderCancelled(_) => {
                self.total_cancels += 1;
            }
            SimEvent::CircuitBreakerTriggered(cb) => {
                let im = self.get_or_create_instrument(cb.instrument_id);
                im.circuit_breaker_count += 1;
                im.halt_duration_total += cb.halt_duration_nanos;
            }
            SimEvent::AuctionResult(ar) => {
                // Tracked by exchange phase
                let _ = ar;
            }
            _ => {}
        }
    }

    /// Generate summary report
    pub fn summary(&self) -> AnalyticsSummary {
        AnalyticsSummary {
            total_trades: self.total_trades,
            total_volume: self.total_volume,
            total_orders: self.total_orders,
            total_cancels: self.total_cancels,
            global_otr: if self.total_trades > 0 {
                self.total_orders as f64 / self.total_trades as f64
            } else {
                0.0
            },
            instruments: self.instrument_metrics.values().cloned().collect(),
            participants: self.participant_metrics.values().cloned().collect(),
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AnalyticsSummary {
    pub total_trades: u64,
    pub total_volume: Qty,
    pub total_orders: u64,
    pub total_cancels: u64,
    pub global_otr: f64,
    pub instruments: Vec<InstrumentMetrics>,
    pub participants: Vec<ParticipantMetrics>,
}
