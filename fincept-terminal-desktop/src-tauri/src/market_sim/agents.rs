use crate::market_sim::types::*;
use crate::market_sim::latency::Rng;
use std::collections::HashMap;

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
    fn on_fill(&mut self, trade: &Trade);
    fn on_cancel(&mut self, order_id: OrderId);
    fn name(&self) -> &str;
}

// ============================================================================
// Market Maker Agent
// ============================================================================

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

    fn on_fill(&mut self, trade: &Trade) {
        // Update inventory based on our role in the trade
        // This is called by the exchange with the fill info
        self.inventory += trade.quantity; // simplified; exchange should indicate side
    }

    fn on_cancel(&mut self, order_id: OrderId) {
        self.active_bids.retain(|&id| id != order_id);
        self.active_asks.retain(|&id| id != order_id);
    }

    fn name(&self) -> &str {
        &self.name
    }
}

// ============================================================================
// HFT / Latency Arbitrage Agent
// ============================================================================

/// HFT agent that exploits stale quotes and speed advantages
pub struct HFTAgent {
    name: String,
    instrument_id: InstrumentId,
    /// Minimum edge required to trade (in price units)
    min_edge: Price,
    /// Position limit
    max_position: Qty,
    /// Current position
    position: Qty,
    /// Cooldown between trades
    trade_cooldown: Nanos,
    last_trade_time: Nanos,
    /// Track recent price for momentum detection
    price_history: Vec<(Nanos, Price)>,
    history_window: usize,
}

impl HFTAgent {
    pub fn new(name: String, instrument_id: InstrumentId, min_edge: Price, max_position: Qty) -> Self {
        Self {
            name,
            instrument_id,
            min_edge,
            max_position,
            position: 0,
            trade_cooldown: 100_000, // 100 microseconds
            last_trade_time: 0,
            price_history: Vec::new(),
            history_window: 100,
        }
    }
}

impl TradingAgent for HFTAgent {
    fn participant_type(&self) -> ParticipantType {
        ParticipantType::HFT
    }

    fn on_tick(&mut self, view: &AgentView, rng: &mut Rng) -> Vec<AgentAction> {
        if !matches!(view.market_phase, MarketPhase::ContinuousTrading) {
            return vec![];
        }

        if view.current_time - self.last_trade_time < self.trade_cooldown {
            return vec![];
        }

        let quote = match view.quotes.get(&self.instrument_id) {
            Some(q) if q.bid_price > 0 && q.ask_price > 0 => q,
            _ => return vec![],
        };

        // Track price
        self.price_history.push((view.current_time, quote.last_price));
        if self.price_history.len() > self.history_window {
            self.price_history.remove(0);
        }

        // Short-term momentum detection
        if self.price_history.len() < 10 {
            return vec![];
        }

        let recent_avg: f64 = self.price_history.iter().rev().take(5).map(|&(_, p)| p as f64).sum::<f64>() / 5.0;
        let older_avg: f64 = self.price_history.iter().rev().skip(5).take(5).map(|&(_, p)| p as f64).sum::<f64>() / 5.0;

        let momentum = recent_avg - older_avg;

        let mut actions = Vec::new();

        // Trade in direction of short-term momentum if edge exists
        if momentum > self.min_edge as f64 && self.position < self.max_position {
            // Price moving up, buy
            actions.push(AgentAction::SubmitOrder {
                instrument_id: self.instrument_id,
                side: Side::Buy,
                order_type: OrderType::Limit,
                price: quote.ask_price, // take the ask
                quantity: 100,
                time_in_force: TimeInForce::IOC,
                display_quantity: 100,
                hidden: false,
            });
            self.last_trade_time = view.current_time;
        } else if momentum < -(self.min_edge as f64) && self.position > -self.max_position {
            // Price moving down, sell
            actions.push(AgentAction::SubmitOrder {
                instrument_id: self.instrument_id,
                side: Side::Sell,
                order_type: OrderType::Limit,
                price: quote.bid_price, // hit the bid
                quantity: 100,
                time_in_force: TimeInForce::IOC,
                display_quantity: 100,
                hidden: false,
            });
            self.last_trade_time = view.current_time;
        }

        // Flatten position if holding too long
        if self.position.abs() > 0 && rng.next_f64() < 0.01 {
            let (side, price) = if self.position > 0 {
                (Side::Sell, quote.bid_price)
            } else {
                (Side::Buy, quote.ask_price)
            };
            actions.push(AgentAction::SubmitOrder {
                instrument_id: self.instrument_id,
                side,
                order_type: OrderType::Limit,
                price,
                quantity: self.position.abs().min(100),
                time_in_force: TimeInForce::IOC,
                display_quantity: self.position.abs().min(100),
                hidden: false,
            });
        }

        actions
    }

    fn on_fill(&mut self, trade: &Trade) {
        self.position += trade.quantity;
    }

    fn on_cancel(&mut self, _order_id: OrderId) {}

    fn name(&self) -> &str {
        &self.name
    }
}

// ============================================================================
// Statistical Arbitrage Agent
// ============================================================================

/// Mean-reversion stat arb agent
pub struct StatArbAgent {
    name: String,
    instrument_id: InstrumentId,
    /// Moving average window
    ma_window: usize,
    /// Entry threshold in standard deviations
    entry_z_score: f64,
    /// Exit threshold
    exit_z_score: f64,
    /// Position limit
    max_position: Qty,
    /// Order size
    order_size: Qty,
    position: Qty,
    price_history: Vec<f64>,
}

impl StatArbAgent {
    pub fn new(
        name: String,
        instrument_id: InstrumentId,
        ma_window: usize,
        entry_z_score: f64,
        max_position: Qty,
        order_size: Qty,
    ) -> Self {
        Self {
            name,
            instrument_id,
            ma_window,
            entry_z_score,
            exit_z_score: 0.5,
            max_position,
            order_size,
            position: 0,
            price_history: Vec::new(),
        }
    }
}

impl TradingAgent for StatArbAgent {
    fn participant_type(&self) -> ParticipantType {
        ParticipantType::StatArb
    }

    fn on_tick(&mut self, view: &AgentView, _rng: &mut Rng) -> Vec<AgentAction> {
        if !matches!(view.market_phase, MarketPhase::ContinuousTrading) {
            return vec![];
        }

        let quote = match view.quotes.get(&self.instrument_id) {
            Some(q) if q.last_price > 0 => q,
            _ => return vec![],
        };

        self.price_history.push(quote.last_price as f64);
        if self.price_history.len() > self.ma_window * 2 {
            self.price_history.remove(0);
        }

        if self.price_history.len() < self.ma_window {
            return vec![];
        }

        // Calculate z-score
        let window = &self.price_history[self.price_history.len() - self.ma_window..];
        let mean: f64 = window.iter().sum::<f64>() / window.len() as f64;
        let variance: f64 = window.iter().map(|p| (p - mean).powi(2)).sum::<f64>() / window.len() as f64;
        let std_dev = variance.sqrt();

        if std_dev < 0.01 {
            return vec![];
        }

        let current_price = quote.last_price as f64;
        let z_score = (current_price - mean) / std_dev;

        let mut actions = Vec::new();

        if z_score > self.entry_z_score && self.position > -self.max_position {
            // Price too high, sell (expect reversion)
            actions.push(AgentAction::SubmitOrder {
                instrument_id: self.instrument_id,
                side: Side::Sell,
                order_type: OrderType::Limit,
                price: quote.bid_price,
                quantity: self.order_size,
                time_in_force: TimeInForce::IOC,
                display_quantity: self.order_size,
                hidden: false,
            });
        } else if z_score < -self.entry_z_score && self.position < self.max_position {
            // Price too low, buy
            actions.push(AgentAction::SubmitOrder {
                instrument_id: self.instrument_id,
                side: Side::Buy,
                order_type: OrderType::Limit,
                price: quote.ask_price,
                quantity: self.order_size,
                time_in_force: TimeInForce::IOC,
                display_quantity: self.order_size,
                hidden: false,
            });
        } else if z_score.abs() < self.exit_z_score && self.position != 0 {
            // Close position
            let (side, price) = if self.position > 0 {
                (Side::Sell, quote.bid_price)
            } else {
                (Side::Buy, quote.ask_price)
            };
            actions.push(AgentAction::SubmitOrder {
                instrument_id: self.instrument_id,
                side,
                order_type: OrderType::Limit,
                price,
                quantity: self.position.abs().min(self.order_size),
                time_in_force: TimeInForce::IOC,
                display_quantity: self.position.abs().min(self.order_size),
                hidden: false,
            });
        }

        actions
    }

    fn on_fill(&mut self, trade: &Trade) {
        self.position += trade.quantity;
    }

    fn on_cancel(&mut self, _order_id: OrderId) {}

    fn name(&self) -> &str {
        &self.name
    }
}

// ============================================================================
// Momentum / Trend Following Agent
// ============================================================================

pub struct MomentumAgent {
    name: String,
    instrument_id: InstrumentId,
    /// Lookback periods for fast/slow moving averages
    fast_period: usize,
    slow_period: usize,
    position: Qty,
    max_position: Qty,
    order_size: Qty,
    price_history: Vec<f64>,
}

impl MomentumAgent {
    pub fn new(
        name: String,
        instrument_id: InstrumentId,
        fast_period: usize,
        slow_period: usize,
        max_position: Qty,
        order_size: Qty,
    ) -> Self {
        Self {
            name,
            instrument_id,
            fast_period,
            slow_period,
            position: 0,
            max_position,
            order_size,
            price_history: Vec::new(),
        }
    }
}

impl TradingAgent for MomentumAgent {
    fn participant_type(&self) -> ParticipantType {
        ParticipantType::Momentum
    }

    fn on_tick(&mut self, view: &AgentView, _rng: &mut Rng) -> Vec<AgentAction> {
        if !matches!(view.market_phase, MarketPhase::ContinuousTrading) {
            return vec![];
        }

        let quote = match view.quotes.get(&self.instrument_id) {
            Some(q) if q.last_price > 0 => q,
            _ => return vec![],
        };

        self.price_history.push(quote.last_price as f64);
        if self.price_history.len() > self.slow_period * 2 {
            self.price_history.remove(0);
        }

        if self.price_history.len() < self.slow_period {
            return vec![];
        }

        let len = self.price_history.len();
        let fast_ma: f64 = self.price_history[len - self.fast_period..].iter().sum::<f64>() / self.fast_period as f64;
        let slow_ma: f64 = self.price_history[len - self.slow_period..].iter().sum::<f64>() / self.slow_period as f64;

        let mut actions = Vec::new();

        if fast_ma > slow_ma && self.position < self.max_position {
            // Bullish crossover
            actions.push(AgentAction::SubmitOrder {
                instrument_id: self.instrument_id,
                side: Side::Buy,
                order_type: OrderType::Limit,
                price: quote.ask_price,
                quantity: self.order_size,
                time_in_force: TimeInForce::IOC,
                display_quantity: self.order_size,
                hidden: false,
            });
        } else if fast_ma < slow_ma && self.position > -self.max_position {
            // Bearish crossover
            actions.push(AgentAction::SubmitOrder {
                instrument_id: self.instrument_id,
                side: Side::Sell,
                order_type: OrderType::Limit,
                price: quote.bid_price,
                quantity: self.order_size,
                time_in_force: TimeInForce::IOC,
                display_quantity: self.order_size,
                hidden: false,
            });
        }

        actions
    }

    fn on_fill(&mut self, trade: &Trade) {
        self.position += trade.quantity;
    }

    fn on_cancel(&mut self, _order_id: OrderId) {}

    fn name(&self) -> &str {
        &self.name
    }
}

// ============================================================================
// Noise Trader Agent
// ============================================================================

/// Random noise trader that provides background liquidity and volume
pub struct NoiseTraderAgent {
    name: String,
    instrument_id: InstrumentId,
    /// Probability of trading per tick
    trade_probability: f64,
    order_size_range: (Qty, Qty),
    position: Qty,
    max_position: Qty,
}

impl NoiseTraderAgent {
    pub fn new(
        name: String,
        instrument_id: InstrumentId,
        trade_probability: f64,
        order_size_range: (Qty, Qty),
        max_position: Qty,
    ) -> Self {
        Self {
            name,
            instrument_id,
            trade_probability,
            order_size_range,
            position: 0,
            max_position,
        }
    }
}

impl TradingAgent for NoiseTraderAgent {
    fn participant_type(&self) -> ParticipantType {
        ParticipantType::NoiseTrader
    }

    fn on_tick(&mut self, view: &AgentView, rng: &mut Rng) -> Vec<AgentAction> {
        if !matches!(view.market_phase, MarketPhase::ContinuousTrading) {
            return vec![];
        }

        if rng.next_f64() > self.trade_probability {
            return vec![];
        }

        let quote = match view.quotes.get(&self.instrument_id) {
            Some(q) if q.bid_price > 0 && q.ask_price > 0 => q,
            _ => return vec![],
        };

        let size = self.order_size_range.0
            + rng.next_bounded((self.order_size_range.1 - self.order_size_range.0) as u64) as Qty;

        // Random side with slight mean-reversion tendency
        let buy_prob = if self.position > 0 { 0.4 } else if self.position < 0 { 0.6 } else { 0.5 };
        let side = if rng.next_f64() < buy_prob { Side::Buy } else { Side::Sell };

        if (side == Side::Buy && self.position >= self.max_position)
            || (side == Side::Sell && self.position <= -self.max_position)
        {
            return vec![];
        }

        // Mix of market and limit orders
        let use_market = rng.next_f64() < 0.3;

        if use_market {
            vec![AgentAction::SubmitOrder {
                instrument_id: self.instrument_id,
                side,
                order_type: OrderType::Market,
                price: 0,
                quantity: size,
                time_in_force: TimeInForce::IOC,
                display_quantity: size,
                hidden: false,
            }]
        } else {
            // Limit order near the spread
            let spread = quote.ask_price - quote.bid_price;
            let offset = rng.next_bounded(spread.max(1) as u64) as Price;
            let price = match side {
                Side::Buy => quote.bid_price + offset,
                Side::Sell => quote.ask_price - offset,
            };

            vec![AgentAction::SubmitOrder {
                instrument_id: self.instrument_id,
                side,
                order_type: OrderType::Limit,
                price,
                quantity: size,
                time_in_force: TimeInForce::Day,
                display_quantity: size,
                hidden: false,
            }]
        }
    }

    fn on_fill(&mut self, trade: &Trade) {
        self.position += trade.quantity;
    }

    fn on_cancel(&mut self, _order_id: OrderId) {}

    fn name(&self) -> &str {
        &self.name
    }
}

// ============================================================================
// Informed Trader Agent
// ============================================================================

/// Trades on private information (news, earnings)
pub struct InformedTraderAgent {
    name: String,
    instrument_id: InstrumentId,
    /// Target price based on information
    target_price: f64,
    /// Confidence in information (0.0 to 1.0)
    confidence: f64,
    position: Qty,
    max_position: Qty,
    order_size: Qty,
    /// How aggressively to trade (0.0 = passive, 1.0 = aggressive)
    aggression: f64,
}

impl InformedTraderAgent {
    pub fn new(
        name: String,
        instrument_id: InstrumentId,
        target_price: f64,
        confidence: f64,
        max_position: Qty,
        order_size: Qty,
    ) -> Self {
        Self {
            name,
            instrument_id,
            target_price,
            confidence,
            position: 0,
            max_position,
            order_size,
            aggression: 0.7,
        }
    }

    pub fn update_target(&mut self, new_target: f64, confidence: f64) {
        self.target_price = new_target;
        self.confidence = confidence;
    }
}

impl TradingAgent for InformedTraderAgent {
    fn participant_type(&self) -> ParticipantType {
        ParticipantType::InformedTrader
    }

    fn on_tick(&mut self, view: &AgentView, rng: &mut Rng) -> Vec<AgentAction> {
        if !matches!(view.market_phase, MarketPhase::ContinuousTrading) {
            return vec![];
        }

        let quote = match view.quotes.get(&self.instrument_id) {
            Some(q) if q.bid_price > 0 && q.ask_price > 0 => q,
            _ => return vec![],
        };

        let mid = (quote.bid_price + quote.ask_price) as f64 / 2.0;
        let edge = (self.target_price - mid) / mid;

        // Only trade if edge is significant
        if edge.abs() < 0.001 {
            return vec![];
        }

        // Trade probability scales with confidence and edge
        let trade_prob = self.confidence * edge.abs().min(0.05) * 20.0;
        if rng.next_f64() > trade_prob {
            return vec![];
        }

        let mut actions = Vec::new();

        if edge > 0.0 && self.position < self.max_position {
            // Undervalued, buy
            let price = if rng.next_f64() < self.aggression {
                quote.ask_price // aggressive: take ask
            } else {
                quote.bid_price + 1 // passive: improve bid
            };
            actions.push(AgentAction::SubmitOrder {
                instrument_id: self.instrument_id,
                side: Side::Buy,
                order_type: OrderType::Limit,
                price,
                quantity: self.order_size,
                time_in_force: TimeInForce::IOC,
                display_quantity: self.order_size,
                hidden: false,
            });
        } else if edge < 0.0 && self.position > -self.max_position {
            // Overvalued, sell
            let price = if rng.next_f64() < self.aggression {
                quote.bid_price // aggressive: hit bid
            } else {
                quote.ask_price - 1 // passive: improve ask
            };
            actions.push(AgentAction::SubmitOrder {
                instrument_id: self.instrument_id,
                side: Side::Sell,
                order_type: OrderType::Limit,
                price,
                quantity: self.order_size,
                time_in_force: TimeInForce::IOC,
                display_quantity: self.order_size,
                hidden: false,
            });
        }

        actions
    }

    fn on_fill(&mut self, trade: &Trade) {
        self.position += trade.quantity;
    }

    fn on_cancel(&mut self, _order_id: OrderId) {}

    fn name(&self) -> &str {
        &self.name
    }
}

// ============================================================================
// Institutional (VWAP/TWAP) Agent
// ============================================================================

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

#[derive(Debug, Clone, Copy)]
pub enum ExecutionAlgo {
    VWAP,
    TWAP,
    PercentageOfVolume,
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

    fn on_fill(&mut self, trade: &Trade) {
        self.executed += trade.quantity;
    }

    fn on_cancel(&mut self, _order_id: OrderId) {}

    fn name(&self) -> &str {
        &self.name
    }
}

// ============================================================================
// Spoofing Agent (for detection training)
// ============================================================================

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

#[derive(Debug, Clone, Copy, PartialEq)]
enum SpoofState {
    Idle,
    SpoofPlaced,
    RealOrderSent,
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

    fn on_fill(&mut self, trade: &Trade) {
        self.position += trade.quantity;
    }

    fn on_cancel(&mut self, order_id: OrderId) {
        self.active_spoof_orders.retain(|&id| id != order_id);
    }

    fn name(&self) -> &str {
        &self.name
    }
}

// ============================================================================
// Sniper Bot (Queue Jumper)
// ============================================================================

/// Detects large resting orders and front-runs them
pub struct SniperAgent {
    name: String,
    instrument_id: InstrumentId,
    /// Minimum size to trigger sniping
    min_target_size: Qty,
    snipe_size: Qty,
    position: Qty,
    max_position: Qty,
    cooldown: Nanos,
    last_snipe: Nanos,
}

impl SniperAgent {
    pub fn new(name: String, instrument_id: InstrumentId, min_target_size: Qty, snipe_size: Qty) -> Self {
        Self {
            name,
            instrument_id,
            min_target_size,
            snipe_size,
            position: 0,
            max_position: 5000,
            cooldown: 1_000_000, // 1ms
            last_snipe: 0,
        }
    }
}

impl TradingAgent for SniperAgent {
    fn participant_type(&self) -> ParticipantType {
        ParticipantType::SniperBot
    }

    fn on_tick(&mut self, view: &AgentView, _rng: &mut Rng) -> Vec<AgentAction> {
        if !matches!(view.market_phase, MarketPhase::ContinuousTrading) {
            return vec![];
        }

        if view.current_time - self.last_snipe < self.cooldown {
            return vec![];
        }

        let depth = match view.depth.get(&self.instrument_id) {
            Some(d) => d,
            None => return vec![],
        };

        let _quote = match view.quotes.get(&self.instrument_id) {
            Some(q) if q.bid_price > 0 && q.ask_price > 0 => q,
            _ => return vec![],
        };

        // Look for large resting orders to front-run
        // If big bid visible → someone wants to buy → buy before them
        for level in &depth.bids {
            if level.quantity >= self.min_target_size && self.position < self.max_position {
                self.last_snipe = view.current_time;
                return vec![AgentAction::SubmitOrder {
                    instrument_id: self.instrument_id,
                    side: Side::Buy,
                    order_type: OrderType::Limit,
                    price: level.price + 1, // penny jump
                    quantity: self.snipe_size,
                    time_in_force: TimeInForce::IOC,
                    display_quantity: self.snipe_size,
                    hidden: false,
                }];
            }
        }

        // Large ask → someone wants to sell → sell before them
        for level in &depth.asks {
            if level.quantity >= self.min_target_size && self.position > -self.max_position {
                self.last_snipe = view.current_time;
                return vec![AgentAction::SubmitOrder {
                    instrument_id: self.instrument_id,
                    side: Side::Sell,
                    order_type: OrderType::Limit,
                    price: level.price - 1,
                    quantity: self.snipe_size,
                    time_in_force: TimeInForce::IOC,
                    display_quantity: self.snipe_size,
                    hidden: false,
                }];
            }
        }

        vec![]
    }

    fn on_fill(&mut self, trade: &Trade) {
        self.position += trade.quantity;
    }

    fn on_cancel(&mut self, _order_id: OrderId) {}

    fn name(&self) -> &str {
        &self.name
    }
}
