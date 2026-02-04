use serde::{Deserialize, Serialize};
use crate::market_sim::types::*;

/// All events in the simulation are captured for deterministic replay
#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum SimEvent {
    // Order lifecycle
    OrderSubmitted(OrderSubmittedEvent),
    OrderAccepted(OrderAcceptedEvent),
    OrderRejected(OrderRejectedEvent),
    OrderCancelled(OrderCancelledEvent),
    OrderExpired(OrderExpiredEvent),
    OrderModified(OrderModifiedEvent),

    // Trade events
    TradeExecuted(TradeExecutedEvent),

    // Market data
    QuoteUpdate(L1Quote),
    BookUpdate(BookUpdateEvent),

    // Market structure
    PhaseChange(PhaseChangeEvent),
    CircuitBreakerTriggered(CircuitBreakerEvent),
    HaltLifted(HaltLiftedEvent),

    // Participant events
    MarginCall(MarginCallEvent),
    KillSwitchTriggered(KillSwitchEvent),
    ForcedLiquidation(ForcedLiquidationEvent),

    // Auction events
    AuctionIndicative(AuctionIndicativeEvent),
    AuctionResult(AuctionResultEvent),

    // News / information events
    NewsEvent(NewsInjectionEvent),
}

impl SimEvent {
    pub fn timestamp(&self) -> Nanos {
        match self {
            SimEvent::OrderSubmitted(e) => e.timestamp,
            SimEvent::OrderAccepted(e) => e.timestamp,
            SimEvent::OrderRejected(e) => e.timestamp,
            SimEvent::OrderCancelled(e) => e.timestamp,
            SimEvent::OrderExpired(e) => e.timestamp,
            SimEvent::OrderModified(e) => e.timestamp,
            SimEvent::TradeExecuted(e) => e.trade.timestamp,
            SimEvent::QuoteUpdate(e) => e.timestamp,
            SimEvent::BookUpdate(e) => e.timestamp,
            SimEvent::PhaseChange(e) => e.timestamp,
            SimEvent::CircuitBreakerTriggered(e) => e.timestamp,
            SimEvent::HaltLifted(e) => e.timestamp,
            SimEvent::MarginCall(e) => e.timestamp,
            SimEvent::KillSwitchTriggered(e) => e.timestamp,
            SimEvent::ForcedLiquidation(e) => e.timestamp,
            SimEvent::AuctionIndicative(e) => e.timestamp,
            SimEvent::AuctionResult(e) => e.timestamp,
            SimEvent::NewsEvent(e) => e.timestamp,
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct OrderSubmittedEvent {
    pub order: Order,
    pub timestamp: Nanos,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct OrderAcceptedEvent {
    pub order_id: OrderId,
    pub participant_id: ParticipantId,
    pub timestamp: Nanos,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct OrderRejectedEvent {
    pub order_id: OrderId,
    pub participant_id: ParticipantId,
    pub reason: RejectReason,
    pub timestamp: Nanos,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct OrderCancelledEvent {
    pub order_id: OrderId,
    pub participant_id: ParticipantId,
    pub remaining_quantity: Qty,
    pub timestamp: Nanos,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct OrderExpiredEvent {
    pub order_id: OrderId,
    pub participant_id: ParticipantId,
    pub timestamp: Nanos,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct OrderModifiedEvent {
    pub order_id: OrderId,
    pub participant_id: ParticipantId,
    pub new_price: Price,
    pub new_quantity: Qty,
    pub timestamp: Nanos,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TradeExecutedEvent {
    pub trade: Trade,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BookUpdateEvent {
    pub instrument_id: InstrumentId,
    pub side: Side,
    pub price: Price,
    pub new_quantity: Qty,
    pub timestamp: Nanos,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PhaseChangeEvent {
    pub instrument_id: InstrumentId,
    pub old_phase: MarketPhase,
    pub new_phase: MarketPhase,
    pub timestamp: Nanos,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CircuitBreakerEvent {
    pub instrument_id: InstrumentId,
    pub level: CircuitBreakerLevel,
    pub trigger_price: Price,
    pub reference_price: Price,
    pub halt_duration_nanos: Nanos,
    pub timestamp: Nanos,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct HaltLiftedEvent {
    pub instrument_id: InstrumentId,
    pub timestamp: Nanos,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MarginCallEvent {
    pub participant_id: ParticipantId,
    pub margin_shortfall: f64,
    pub timestamp: Nanos,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct KillSwitchEvent {
    pub participant_id: ParticipantId,
    pub reason: String,
    pub pnl_at_trigger: f64,
    pub timestamp: Nanos,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ForcedLiquidationEvent {
    pub participant_id: ParticipantId,
    pub instrument_id: InstrumentId,
    pub quantity: Qty,
    pub side: Side,
    pub timestamp: Nanos,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AuctionIndicativeEvent {
    pub instrument_id: InstrumentId,
    pub indicative_price: Price,
    pub indicative_volume: Qty,
    pub buy_surplus: Qty,
    pub sell_surplus: Qty,
    pub timestamp: Nanos,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AuctionResultEvent {
    pub instrument_id: InstrumentId,
    pub auction_price: Price,
    pub auction_volume: Qty,
    pub trades: Vec<Trade>,
    pub timestamp: Nanos,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct NewsInjectionEvent {
    pub headline: String,
    pub sentiment: f64,           // -1.0 to 1.0
    pub impact_magnitude: f64,    // 0.0 to 1.0
    pub affected_instruments: Vec<InstrumentId>,
    pub timestamp: Nanos,
}

/// Event log for deterministic replay
pub struct EventLog {
    events: Vec<SimEvent>,
}

impl EventLog {
    pub fn new() -> Self {
        Self { events: Vec::with_capacity(1_000_000) }
    }

    pub fn record(&mut self, event: SimEvent) {
        self.events.push(event);
    }

    pub fn events(&self) -> &[SimEvent] {
        &self.events
    }

    pub fn len(&self) -> usize {
        self.events.len()
    }

    pub fn is_empty(&self) -> bool {
        self.events.is_empty()
    }

    pub fn clear(&mut self) {
        self.events.clear();
    }

    pub fn events_in_range(&self, from: Nanos, to: Nanos) -> Vec<&SimEvent> {
        self.events
            .iter()
            .filter(|e| {
                let ts = e.timestamp();
                ts >= from && ts <= to
            })
            .collect()
    }

    pub fn trades(&self) -> Vec<&Trade> {
        self.events
            .iter()
            .filter_map(|e| match e {
                SimEvent::TradeExecuted(te) => Some(&te.trade),
                _ => None,
            })
            .collect()
    }
}
