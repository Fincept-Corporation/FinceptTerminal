use crate::market_sim::types::*;
use crate::market_sim::events::*;
use std::collections::HashMap;

/// Pre-trade and real-time risk management engine
pub struct RiskEngine {
    /// Per-participant risk limits
    limits: HashMap<ParticipantId, RiskLimits>,
    /// Per-participant order rate counters (orders in current second)
    order_rates: HashMap<ParticipantId, OrderRateTracker>,
    /// Per-participant PnL tracking for kill switch
    pnl_high_water: HashMap<ParticipantId, f64>,
    /// Global margin requirements per instrument
    margin_requirements: HashMap<InstrumentId, MarginRequirement>,
    /// Fat finger thresholds
    fat_finger_max_qty: Qty,
    fat_finger_max_notional: f64,
    /// Price band percentage from reference
    price_band_pct: f64,
}

#[derive(Debug, Clone)]
pub struct RiskLimits {
    pub max_position_per_instrument: Qty,
    pub max_total_position: Qty,
    pub max_order_rate_per_sec: u32,
    pub max_order_to_trade_ratio: f64,
    pub max_drawdown_pct: f64,
    pub max_notional_per_order: f64,
    pub max_daily_loss: f64,
    pub kill_switch_active: bool,
}

impl Default for RiskLimits {
    fn default() -> Self {
        Self {
            max_position_per_instrument: 100_000,
            max_total_position: 500_000,
            max_order_rate_per_sec: 1000,
            max_order_to_trade_ratio: 100.0,
            max_drawdown_pct: 0.10,
            max_notional_per_order: 10_000_000.0,
            max_daily_loss: 1_000_000.0,
            kill_switch_active: false,
        }
    }
}

struct OrderRateTracker {
    count: u32,
    window_start: Nanos,
}

impl RiskEngine {
    pub fn new() -> Self {
        Self {
            limits: HashMap::new(),
            order_rates: HashMap::new(),
            pnl_high_water: HashMap::new(),
            margin_requirements: HashMap::new(),
            fat_finger_max_qty: 1_000_000,
            fat_finger_max_notional: 50_000_000.0,
            price_band_pct: 0.10,
        }
    }

    pub fn set_limits(&mut self, participant_id: ParticipantId, limits: RiskLimits) {
        self.limits.insert(participant_id, limits);
    }

    pub fn set_margin_requirement(&mut self, instrument_id: InstrumentId, req: MarginRequirement) {
        self.margin_requirements.insert(instrument_id, req);
    }

    /// Pre-trade risk check. Returns None if passed, or RejectReason if failed.
    pub fn check_order(
        &mut self,
        order: &Order,
        account: &ParticipantAccount,
        reference_price: Price,
        timestamp: Nanos,
    ) -> Option<RejectReason> {
        let limits = self.limits.get(&order.participant_id).cloned().unwrap_or_default();

        // Kill switch check
        if limits.kill_switch_active {
            return Some(RejectReason::KillSwitchActive);
        }

        // Fat finger: quantity check
        if order.quantity > self.fat_finger_max_qty {
            return Some(RejectReason::FatFingerCheck);
        }

        // Fat finger: notional check
        let effective_price = if order.price > 0 { order.price } else { reference_price };
        let notional = effective_price as f64 * order.quantity as f64;
        if notional > self.fat_finger_max_notional {
            return Some(RejectReason::FatFingerCheck);
        }

        // Max notional per order
        if notional > limits.max_notional_per_order {
            return Some(RejectReason::FatFingerCheck);
        }

        // Position limit check
        if let Some(pos) = account.get_position(order.instrument_id) {
            let projected = match order.side {
                Side::Buy => pos.net_quantity + order.quantity,
                Side::Sell => pos.net_quantity - order.quantity,
            };
            if projected.abs() > limits.max_position_per_instrument {
                return Some(RejectReason::PositionLimitExceeded);
            }
        }

        // Total position limit
        let total_pos: Qty = account
            .positions
            .values()
            .map(|p| p.net_quantity.abs())
            .sum();
        if total_pos + order.quantity > limits.max_total_position {
            return Some(RejectReason::PositionLimitExceeded);
        }

        // Order rate limit
        let rate = self.order_rates.entry(order.participant_id).or_insert(OrderRateTracker {
            count: 0,
            window_start: timestamp,
        });

        let one_second = 1_000_000_000; // 1 second in nanos
        if timestamp - rate.window_start >= one_second {
            rate.count = 0;
            rate.window_start = timestamp;
        }
        rate.count += 1;
        if rate.count > limits.max_order_rate_per_sec {
            return Some(RejectReason::RateLimitExceeded);
        }

        // Price band check for limit orders
        if order.price > 0 && reference_price > 0 {
            let band = (reference_price as f64 * self.price_band_pct) as Price;
            if order.price > reference_price + band || order.price < reference_price - band {
                return Some(RejectReason::PriceOutOfBands);
            }
        }

        // Margin check
        if let Some(margin_req) = self.margin_requirements.get(&order.instrument_id) {
            let required_margin = notional * margin_req.initial_margin_pct;
            if required_margin > account.margin_available {
                return Some(RejectReason::InsufficientMargin);
            }
        }

        // Cash balance check for buys
        if matches!(order.side, Side::Buy) {
            if notional > account.cash_balance {
                return Some(RejectReason::InsufficientBalance);
            }
        }

        None
    }

    /// Post-trade risk checks — returns events for margin calls, kill switches, etc.
    pub fn post_trade_check(
        &mut self,
        account: &ParticipantAccount,
        timestamp: Nanos,
    ) -> Vec<SimEvent> {
        let mut events = Vec::new();
        let limits = self.limits.get(&account.id).cloned().unwrap_or_default();

        // Update PnL high water mark
        let current_pnl = account.total_pnl;
        let high_water = self.pnl_high_water.entry(account.id).or_insert(0.0);
        if current_pnl > *high_water {
            *high_water = current_pnl;
        }

        // Drawdown kill switch
        if account.initial_balance > 0.0 {
            let total_value = account.cash_balance + current_pnl;
            let drawdown = (account.initial_balance - total_value) / account.initial_balance;
            if drawdown >= limits.max_drawdown_pct {
                events.push(SimEvent::KillSwitchTriggered(KillSwitchEvent {
                    participant_id: account.id,
                    reason: format!("Drawdown {:.2}% exceeds limit {:.2}%", drawdown * 100.0, limits.max_drawdown_pct * 100.0),
                    pnl_at_trigger: current_pnl,
                    timestamp,
                }));
            }
        }

        // Daily loss limit
        if current_pnl < -limits.max_daily_loss {
            events.push(SimEvent::KillSwitchTriggered(KillSwitchEvent {
                participant_id: account.id,
                reason: format!("Daily loss ${:.0} exceeds limit ${:.0}", -current_pnl, limits.max_daily_loss),
                pnl_at_trigger: current_pnl,
                timestamp,
            }));
        }

        // Margin maintenance check
        for (instrument_id, pos) in &account.positions {
            if pos.net_quantity == 0 {
                continue;
            }
            if let Some(margin_req) = self.margin_requirements.get(instrument_id) {
                let position_value = pos.avg_price * pos.net_quantity.abs() as f64;
                let required = position_value * margin_req.maintenance_margin_pct;
                if required > account.margin_available {
                    events.push(SimEvent::MarginCall(MarginCallEvent {
                        participant_id: account.id,
                        margin_shortfall: required - account.margin_available,
                        timestamp,
                    }));
                }
            }
        }

        // Order-to-trade ratio check
        if account.total_trades > 0 {
            let otr = account.total_orders as f64 / account.total_trades as f64;
            if otr > limits.max_order_to_trade_ratio {
                events.push(SimEvent::KillSwitchTriggered(KillSwitchEvent {
                    participant_id: account.id,
                    reason: format!("OTR {:.1} exceeds limit {:.1}", otr, limits.max_order_to_trade_ratio),
                    pnl_at_trigger: current_pnl,
                    timestamp,
                }));
            }
        }

        events
    }

    /// Activate kill switch for a participant
    pub fn activate_kill_switch(&mut self, participant_id: ParticipantId) {
        if let Some(limits) = self.limits.get_mut(&participant_id) {
            limits.kill_switch_active = true;
        }
    }

    /// Generate forced liquidation orders for a participant under margin call
    pub fn forced_liquidation_orders(
        &self,
        account: &ParticipantAccount,
        timestamp: Nanos,
    ) -> Vec<(InstrumentId, Side, Qty)> {
        let mut liquidations = Vec::new();

        for (instrument_id, pos) in &account.positions {
            if pos.net_quantity > 0 {
                liquidations.push((*instrument_id, Side::Sell, pos.net_quantity));
            } else if pos.net_quantity < 0 {
                liquidations.push((*instrument_id, Side::Buy, -pos.net_quantity));
            }
        }

        let _ = timestamp; // available for logging
        liquidations
    }
}
