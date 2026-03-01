/**
 * Market Simulation Tab - Type Definitions
 * Mirrors Rust types exactly: Price = i64 (cents), Qty = i64, Nanos = u64
 */

export interface SimResponse<T> {
  success: boolean;
  data?: T;
  error?: string;
}

export interface L1Quote {
  instrument_id: number;      // u32
  bid_price: number;          // i64 (cents)
  bid_size: number;           // i64
  ask_price: number;          // i64 (cents)
  ask_size: number;           // i64
  last_price: number;         // i64 (cents)
  last_size: number;          // i64
  volume: number;             // i64
  vwap: number;               // f64
  open: number;               // i64 (cents)
  high: number;               // i64 (cents)
  low: number;                // i64 (cents)
  timestamp: number;          // u64 (nanos)
}

export interface PriceLevel {
  price: number;              // i64 (cents)
  quantity: number;           // i64
  order_count: number;        // u32
}

export interface L2Snapshot {
  instrument_id: number;
  bids: PriceLevel[];
  asks: PriceLevel[];
  timestamp: number;
}

// Rust Side enum serializes as "Buy" or "Sell" string
export type Side = 'Buy' | 'Sell' | string | Record<string, unknown>;

export interface Trade {
  id: number;                 // u64
  instrument_id: number;      // u32
  price: number;              // i64 (cents)
  quantity: number;           // i64
  aggressor_side: Side;       // enum Side
  buyer_id: number;           // u32
  seller_id: number;          // u32
  buyer_order_id?: number;    // u64
  seller_order_id?: number;   // u64
  timestamp: number;          // u64 (nanos)
  venue_id?: number;          // u32
  is_auction_trade?: boolean;
}

// Rust ParticipantType and LatencyTier enums serialize as strings
export type ParticipantType = 'MarketMaker' | 'HFT' | 'StatArb' | 'Momentum' | 'MeanReversion' |
  'NoiseTrader' | 'InformedTrader' | 'Institutional' | 'RetailTrader' | 'ToxicFlow' |
  'Spoofer' | 'Arbitrageur' | 'SniperBot' | string | Record<string, unknown>;

export type LatencyTier = 'CoLocated' | 'ProximityHosted' | 'DirectConnect' | 'Retail' |
  string | Record<string, unknown>;

export type MarketPhase = 'PreOpen' | 'OpeningAuction' | 'ContinuousTrading' | 'VolatilityAuction' |
  'ClosingAuction' | 'PostClose' | 'Halted' | string | Record<string, unknown>;

export interface ParticipantSummary {
  id: number;                 // u32
  name: string;
  participant_type: ParticipantType;
  pnl: number;                // f64
  position: number;           // i64
  trade_count: number;        // u64
  order_count: number;        // u64
  cancel_count: number;       // u64
  is_active: boolean;
  latency_tier: LatencyTier;
}

export interface SimulationSnapshot {
  current_time: number;           // u64 (nanos)
  market_phase: MarketPhase;      // enum MarketPhase
  quotes: L1Quote[];
  recent_trades: Trade[];
  participant_summaries: ParticipantSummary[];
  total_volume: number;           // i64
  total_trades: number;           // u64
  circuit_breaker_active: boolean;
  messages_per_second: number;    // u64
  orders_per_second: number;      // u64
}

export interface InstrumentMetrics {
  instrument_id: number;
  avg_spread: number;
  total_volume: number;
  total_trades: number;
  vwap: number;
  realized_volatility: number;
  circuit_breaker_count: number;
}

export interface ParticipantMetrics {
  participant_id: number;         // u32
  participant_type: ParticipantType;
  name: string;
  realized_pnl: number;           // f64
  unrealized_pnl: number;         // f64
  total_pnl: number;              // f64
  max_drawdown: number;           // f64
  total_trades: number;           // u64
  total_orders: number;           // u64
  total_cancels: number;          // u64
  order_to_trade_ratio: number;   // f64
  fill_rate: number;              // f64
  net_fees: number;               // f64
}

export interface AnalyticsSummary {
  total_trades: number;
  total_volume: number;
  total_orders: number;
  total_cancels: number;
  global_otr: number;
  instruments: InstrumentMetrics[];
  participants: ParticipantMetrics[];
}
