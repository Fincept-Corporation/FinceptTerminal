use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use std::fmt;

// ============================================================================
// Time & Identity
// ============================================================================

/// Nanosecond-precision timestamp for event ordering
pub type Nanos = u64;

/// Unique order identifier
pub type OrderId = u64;

/// Unique trade identifier
pub type TradeId = u64;

/// Participant identifier
pub type ParticipantId = u32;

/// Instrument identifier
pub type InstrumentId = u32;

/// Price in fixed-point (cents / basis points) to avoid floating point
/// e.g. 15050 = $150.50
pub type Price = i64;

/// Quantity in shares/contracts
pub type Qty = i64;

// ============================================================================
// Enums
// ============================================================================

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub enum Side {
    Buy,
    Sell,
}

impl Side {
    pub fn opposite(&self) -> Self {
        match self {
            Side::Buy => Side::Sell,
            Side::Sell => Side::Buy,
        }
    }
}

impl fmt::Display for Side {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Side::Buy => write!(f, "BUY"),
            Side::Sell => write!(f, "SELL"),
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub enum OrderType {
    Market,
    Limit,
    Stop,
    StopLimit,
    Iceberg,
    Pegged(PegType),
    TrailingStop,
    MarketToLimit,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub enum PegType {
    Midpoint,
    Primary,   // best bid for buy, best ask for sell
    Market,    // best ask for buy, best bid for sell
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub enum TimeInForce {
    Day,
    GTC,          // Good Till Cancel
    GTD(Nanos),   // Good Till Date (expiry timestamp)
    IOC,          // Immediate or Cancel
    FOK,          // Fill or Kill
    ATO,          // At The Open
    ATC,          // At The Close
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub enum OrderStatus {
    New,
    PartiallyFilled,
    Filled,
    Cancelled,
    Rejected,
    Expired,
    PendingNew,
    PendingCancel,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub enum MarketPhase {
    PreOpen,
    OpeningAuction,
    ContinuousTrading,
    VolatilityAuction,
    ClosingAuction,
    PostClose,
    Halted,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub enum ParticipantType {
    MarketMaker,
    HFT,
    StatArb,
    Momentum,
    MeanReversion,
    NoiseTrader,
    InformedTrader,
    Institutional,     // VWAP/TWAP block trader
    RetailTrader,
    ToxicFlow,         // Adversarial agent
    Spoofer,           // For detection training
    Arbitrageur,       // Cross-venue arb
    SniperBot,         // Latency arb / queue jumping
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub enum VenueType {
    PrimaryExchange,
    ECN,
    DarkPool,
    ATS,
    Internalizer,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub enum CircuitBreakerLevel {
    Level1,  // 7% drop - 15min halt
    Level2,  // 13% drop - 15min halt
    Level3,  // 20% drop - market closed for day
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub enum RejectReason {
    InsufficientMargin,
    PositionLimitExceeded,
    RateLimitExceeded,
    FatFingerCheck,
    MarketHalted,
    InvalidPrice,
    InvalidQuantity,
    KillSwitchActive,
    SelfTradePrevention,
    PriceOutOfBands,
    InsufficientBalance,
}

// ============================================================================
// Core Structures
// ============================================================================

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Order {
    pub id: OrderId,
    pub participant_id: ParticipantId,
    pub instrument_id: InstrumentId,
    pub side: Side,
    pub order_type: OrderType,
    pub time_in_force: TimeInForce,
    pub price: Price,            // 0 for market orders
    pub quantity: Qty,
    pub filled_quantity: Qty,
    pub remaining_quantity: Qty,
    pub stop_price: Price,       // for stop/stop-limit
    pub display_quantity: Qty,   // for iceberg (max visible portion per reload)
    pub current_display: Qty,    // for iceberg (currently visible portion)
    pub hidden: bool,            // fully hidden order
    pub trailing_offset: Price,  // for trailing stop
    pub status: OrderStatus,
    pub timestamp: Nanos,
    pub last_update: Nanos,
    pub venue_id: u32,
}

impl Order {
    pub fn new_limit(
        id: OrderId,
        participant_id: ParticipantId,
        instrument_id: InstrumentId,
        side: Side,
        price: Price,
        quantity: Qty,
        tif: TimeInForce,
        timestamp: Nanos,
    ) -> Self {
        Self {
            id,
            participant_id,
            instrument_id,
            side,
            order_type: OrderType::Limit,
            time_in_force: tif,
            price,
            quantity,
            filled_quantity: 0,
            remaining_quantity: quantity,
            stop_price: 0,
            display_quantity: quantity,
            current_display: quantity,
            hidden: false,
            trailing_offset: 0,
            status: OrderStatus::New,
            timestamp,
            last_update: timestamp,
            venue_id: 0,
        }
    }

    pub fn new_market(
        id: OrderId,
        participant_id: ParticipantId,
        instrument_id: InstrumentId,
        side: Side,
        quantity: Qty,
        timestamp: Nanos,
    ) -> Self {
        Self {
            id,
            participant_id,
            instrument_id,
            side,
            order_type: OrderType::Market,
            time_in_force: TimeInForce::IOC,
            price: 0,
            quantity,
            filled_quantity: 0,
            remaining_quantity: quantity,
            stop_price: 0,
            display_quantity: quantity,
            current_display: quantity,
            hidden: false,
            trailing_offset: 0,
            status: OrderStatus::New,
            timestamp,
            last_update: timestamp,
            venue_id: 0,
        }
    }

    pub fn new_iceberg(
        id: OrderId,
        participant_id: ParticipantId,
        instrument_id: InstrumentId,
        side: Side,
        price: Price,
        total_quantity: Qty,
        display_quantity: Qty,
        tif: TimeInForce,
        timestamp: Nanos,
    ) -> Self {
        let initial_display = display_quantity.min(total_quantity);
        Self {
            id,
            participant_id,
            instrument_id,
            side,
            order_type: OrderType::Iceberg,
            time_in_force: tif,
            price,
            quantity: total_quantity,
            filled_quantity: 0,
            remaining_quantity: total_quantity,
            stop_price: 0,
            display_quantity,
            current_display: initial_display,
            hidden: false,
            trailing_offset: 0,
            status: OrderStatus::New,
            timestamp,
            last_update: timestamp,
            venue_id: 0,
        }
    }

    pub fn is_active(&self) -> bool {
        matches!(self.status, OrderStatus::New | OrderStatus::PartiallyFilled)
    }

    pub fn fill(&mut self, qty: Qty, timestamp: Nanos) {
        self.filled_quantity += qty;
        self.remaining_quantity -= qty;
        self.last_update = timestamp;
        if self.remaining_quantity <= 0 {
            self.status = OrderStatus::Filled;
        } else {
            self.status = OrderStatus::PartiallyFilled;
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Trade {
    pub id: TradeId,
    pub instrument_id: InstrumentId,
    pub price: Price,
    pub quantity: Qty,
    pub aggressor_side: Side,
    pub buyer_id: ParticipantId,
    pub seller_id: ParticipantId,
    pub buyer_order_id: OrderId,
    pub seller_order_id: OrderId,
    pub timestamp: Nanos,
    pub venue_id: u32,
    pub is_auction_trade: bool,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Instrument {
    pub id: InstrumentId,
    pub symbol: String,
    pub tick_size: Price,        // minimum price increment
    pub lot_size: Qty,           // minimum quantity increment
    pub min_quantity: Qty,
    pub max_quantity: Qty,
    pub price_band_pct: f64,     // max % move from reference price
    pub reference_price: Price,  // for circuit breakers
    pub maker_fee_bps: f64,      // maker fee in basis points
    pub taker_fee_bps: f64,      // taker fee in basis points
    pub short_sell_allowed: bool,
    pub tick_size_table: Vec<TickSizeRule>, // variable tick sizes
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TickSizeRule {
    pub price_from: Price,
    pub price_to: Price,
    pub tick_size: Price,
}

impl Instrument {
    pub fn get_tick_size(&self, price: Price) -> Price {
        for rule in &self.tick_size_table {
            if price >= rule.price_from && price < rule.price_to {
                return rule.tick_size;
            }
        }
        self.tick_size
    }

    pub fn round_to_tick(&self, price: Price) -> Price {
        let tick = self.get_tick_size(price);
        if tick == 0 {
            return price;
        }
        (price / tick) * tick
    }

    pub fn validate_price(&self, price: Price) -> bool {
        if price <= 0 {
            return false;
        }
        let tick = self.get_tick_size(price);
        if tick > 0 && price % tick != 0 {
            return false;
        }
        // Price band check
        if self.reference_price > 0 && self.price_band_pct > 0.0 {
            let band = (self.reference_price as f64 * self.price_band_pct) as Price;
            if price > self.reference_price + band || price < self.reference_price - band {
                return false;
            }
        }
        true
    }

    pub fn validate_quantity(&self, qty: Qty) -> bool {
        qty >= self.min_quantity && qty <= self.max_quantity && (self.lot_size == 0 || qty % self.lot_size == 0)
    }
}

// ============================================================================
// Market Data
// ============================================================================

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct L1Quote {
    pub instrument_id: InstrumentId,
    pub bid_price: Price,
    pub bid_size: Qty,
    pub ask_price: Price,
    pub ask_size: Qty,
    pub last_price: Price,
    pub last_size: Qty,
    pub volume: Qty,
    pub vwap: f64,
    pub open: Price,
    pub high: Price,
    pub low: Price,
    pub timestamp: Nanos,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PriceLevel {
    pub price: Price,
    pub quantity: Qty,
    pub order_count: u32,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct L2Snapshot {
    pub instrument_id: InstrumentId,
    pub bids: Vec<PriceLevel>,
    pub asks: Vec<PriceLevel>,
    pub timestamp: Nanos,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct L3Order {
    pub order_id: OrderId,
    pub price: Price,
    pub quantity: Qty,
    pub side: Side,
    pub participant_id: ParticipantId,
    pub timestamp: Nanos,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct L3Snapshot {
    pub instrument_id: InstrumentId,
    pub bids: Vec<L3Order>,
    pub asks: Vec<L3Order>,
    pub timestamp: Nanos,
}

// ============================================================================
// Participant & Position
// ============================================================================

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Position {
    pub instrument_id: InstrumentId,
    pub net_quantity: Qty,     // + long, - short
    pub avg_price: f64,
    pub realized_pnl: f64,
    pub unrealized_pnl: f64,
    pub total_bought: Qty,
    pub total_sold: Qty,
    pub total_volume: Qty,
}

impl Position {
    pub fn new(instrument_id: InstrumentId) -> Self {
        Self {
            instrument_id,
            net_quantity: 0,
            avg_price: 0.0,
            realized_pnl: 0.0,
            unrealized_pnl: 0.0,
            total_bought: 0,
            total_sold: 0,
            total_volume: 0,
        }
    }

    pub fn update_on_fill(&mut self, side: Side, qty: Qty, price: Price) {
        let price_f = price as f64;
        match side {
            Side::Buy => {
                if self.net_quantity >= 0 {
                    // Adding to long
                    let total_cost = self.avg_price * self.net_quantity as f64 + price_f * qty as f64;
                    self.net_quantity += qty;
                    if self.net_quantity > 0 {
                        self.avg_price = total_cost / self.net_quantity as f64;
                    }
                } else {
                    // Covering short
                    let cover_qty = qty.min(-self.net_quantity);
                    self.realized_pnl += (self.avg_price - price_f) * cover_qty as f64;
                    self.net_quantity += qty;
                    if self.net_quantity > 0 {
                        self.avg_price = price_f;
                    }
                }
                self.total_bought += qty;
            }
            Side::Sell => {
                if self.net_quantity <= 0 {
                    // Adding to short
                    let total_cost = self.avg_price * (-self.net_quantity) as f64 + price_f * qty as f64;
                    self.net_quantity -= qty;
                    if self.net_quantity < 0 {
                        self.avg_price = total_cost / (-self.net_quantity) as f64;
                    }
                } else {
                    // Selling long
                    let sell_qty = qty.min(self.net_quantity);
                    self.realized_pnl += (price_f - self.avg_price) * sell_qty as f64;
                    self.net_quantity -= qty;
                    if self.net_quantity < 0 {
                        self.avg_price = price_f;
                    }
                }
                self.total_sold += qty;
            }
        }
        self.total_volume += qty;
    }

    pub fn mark_to_market(&mut self, current_price: Price) {
        let price_f = current_price as f64;
        if self.net_quantity > 0 {
            self.unrealized_pnl = (price_f - self.avg_price) * self.net_quantity as f64;
        } else if self.net_quantity < 0 {
            self.unrealized_pnl = (self.avg_price - price_f) * (-self.net_quantity) as f64;
        } else {
            self.unrealized_pnl = 0.0;
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ParticipantAccount {
    pub id: ParticipantId,
    pub name: String,
    pub participant_type: ParticipantType,
    pub cash_balance: f64,
    pub initial_balance: f64,
    pub positions: HashMap<InstrumentId, Position>,
    pub margin_used: f64,
    pub margin_available: f64,
    pub total_pnl: f64,
    pub total_trades: u64,
    pub total_orders: u64,
    pub total_cancels: u64,
    pub order_to_trade_ratio: f64,
    pub is_active: bool,
    pub kill_switch_triggered: bool,
    pub max_position_limit: Qty,
    pub max_order_rate: u32,        // orders per second
    pub max_drawdown_pct: f64,      // kill switch trigger
    pub latency_tier: LatencyTier,
    pub venue_access: Vec<VenueType>,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub enum LatencyTier {
    CoLocated,     // ~1-10 microseconds
    ProximityHosted, // ~50-100 microseconds
    DirectConnect, // ~1-5 milliseconds
    Retail,        // ~10-100 milliseconds
}

impl LatencyTier {
    pub fn base_latency_nanos(&self) -> u64 {
        match self {
            LatencyTier::CoLocated => 1_000,          // 1 microsecond
            LatencyTier::ProximityHosted => 50_000,   // 50 microseconds
            LatencyTier::DirectConnect => 1_000_000,  // 1 millisecond
            LatencyTier::Retail => 50_000_000,        // 50 milliseconds
        }
    }

    pub fn jitter_range_nanos(&self) -> u64 {
        match self {
            LatencyTier::CoLocated => 500,
            LatencyTier::ProximityHosted => 20_000,
            LatencyTier::DirectConnect => 500_000,
            LatencyTier::Retail => 30_000_000,
        }
    }
}

impl ParticipantAccount {
    pub fn new(
        id: ParticipantId,
        name: String,
        participant_type: ParticipantType,
        initial_balance: f64,
        latency_tier: LatencyTier,
    ) -> Self {
        Self {
            id,
            name,
            participant_type,
            cash_balance: initial_balance,
            initial_balance,
            positions: HashMap::new(),
            margin_used: 0.0,
            margin_available: initial_balance,
            total_pnl: 0.0,
            total_trades: 0,
            total_orders: 0,
            total_cancels: 0,
            order_to_trade_ratio: 0.0,
            is_active: true,
            kill_switch_triggered: false,
            max_position_limit: 100_000,
            max_order_rate: 1000,
            max_drawdown_pct: 0.10,
            latency_tier,
            venue_access: vec![VenueType::PrimaryExchange],
        }
    }

    pub fn get_position(&self, instrument_id: InstrumentId) -> Option<&Position> {
        self.positions.get(&instrument_id)
    }

    pub fn get_or_create_position(&mut self, instrument_id: InstrumentId) -> &mut Position {
        self.positions
            .entry(instrument_id)
            .or_insert_with(|| Position::new(instrument_id))
    }

    pub fn update_pnl(&mut self) {
        self.total_pnl = self
            .positions
            .values()
            .map(|p| p.realized_pnl + p.unrealized_pnl)
            .sum();
    }

    pub fn check_drawdown(&self) -> bool {
        if self.initial_balance <= 0.0 {
            return false;
        }
        let drawdown = (self.initial_balance - (self.cash_balance + self.total_pnl)) / self.initial_balance;
        drawdown >= self.max_drawdown_pct
    }

    pub fn update_order_trade_ratio(&mut self) {
        if self.total_trades > 0 {
            self.order_to_trade_ratio = self.total_orders as f64 / self.total_trades as f64;
        }
    }
}

// ============================================================================
// Simulation Configuration
// ============================================================================

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SimulationConfig {
    pub name: String,
    pub instruments: Vec<Instrument>,
    pub participants: Vec<ParticipantConfig>,
    pub duration_nanos: Nanos,
    pub tick_interval_nanos: Nanos,    // simulation step size
    pub random_seed: u64,
    pub enable_circuit_breakers: bool,
    pub enable_auctions: bool,
    pub enable_dark_pools: bool,
    pub enable_latency_simulation: bool,
    pub market_open_nanos: Nanos,
    pub market_close_nanos: Nanos,
    pub opening_auction_duration_nanos: Nanos,
    pub closing_auction_duration_nanos: Nanos,
    pub circuit_breaker_thresholds: Vec<(f64, Nanos)>, // (pct_drop, halt_duration)
    pub maker_rebate_bps: f64,
    pub taker_fee_bps: f64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ParticipantConfig {
    pub name: String,
    pub participant_type: ParticipantType,
    pub initial_balance: f64,
    pub latency_tier: LatencyTier,
    pub count: u32,  // spawn N agents of this type
    pub params: HashMap<String, f64>,  // type-specific parameters
}

impl Default for SimulationConfig {
    fn default() -> Self {
        Self {
            name: "Default Simulation".to_string(),
            instruments: vec![],
            participants: vec![],
            duration_nanos: 23_400_000_000_000, // 6.5 hours in nanos (trading day)
            tick_interval_nanos: 1_000_000,     // 1ms ticks
            random_seed: 42,
            enable_circuit_breakers: true,
            enable_auctions: true,
            enable_dark_pools: false,
            enable_latency_simulation: true,
            market_open_nanos: 34_200_000_000_000,  // 9:30 AM
            market_close_nanos: 57_600_000_000_000, // 4:00 PM
            opening_auction_duration_nanos: 300_000_000_000, // 5 min
            closing_auction_duration_nanos: 300_000_000_000, // 5 min
            circuit_breaker_thresholds: vec![
                (0.07, 900_000_000_000),   // 7% → 15min halt
                (0.13, 900_000_000_000),   // 13% → 15min halt
                (0.20, u64::MAX),          // 20% → close for day
            ],
            maker_rebate_bps: -0.2,  // maker gets paid
            taker_fee_bps: 0.3,
        }
    }
}

// ============================================================================
// Margin & Collateral
// ============================================================================

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MarginRequirement {
    pub initial_margin_pct: f64,     // e.g., 0.50 = 50%
    pub maintenance_margin_pct: f64, // e.g., 0.25 = 25%
    pub short_margin_pct: f64,       // extra margin for shorts
}

impl Default for MarginRequirement {
    fn default() -> Self {
        Self {
            initial_margin_pct: 0.50,
            maintenance_margin_pct: 0.25,
            short_margin_pct: 0.50,
        }
    }
}

// ============================================================================
// Venue Configuration
// ============================================================================

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct VenueConfig {
    pub id: u32,
    pub name: String,
    pub venue_type: VenueType,
    pub maker_fee_bps: f64,
    pub taker_fee_bps: f64,
    pub min_order_size: Qty,
    pub supports_hidden_orders: bool,
    pub supports_iceberg: bool,
    pub supports_pegged: bool,
    pub dark_pool_min_size: Qty,  // minimum size for dark pool
    pub feed_latency_nanos: Nanos, // additional latency for market data
}

// ============================================================================
// Simulation State Snapshot (for frontend)
// ============================================================================

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SimulationSnapshot {
    pub current_time: Nanos,
    pub market_phase: MarketPhase,
    pub quotes: Vec<L1Quote>,
    pub recent_trades: Vec<Trade>,
    pub participant_summaries: Vec<ParticipantSummary>,
    pub total_volume: Qty,
    pub total_trades: u64,
    pub circuit_breaker_active: bool,
    pub messages_per_second: u64,
    pub orders_per_second: u64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ParticipantSummary {
    pub id: ParticipantId,
    pub name: String,
    pub participant_type: ParticipantType,
    pub pnl: f64,
    pub position: Qty,
    pub trade_count: u64,
    pub order_count: u64,
    pub cancel_count: u64,
    pub is_active: bool,
    pub latency_tier: LatencyTier,
}
