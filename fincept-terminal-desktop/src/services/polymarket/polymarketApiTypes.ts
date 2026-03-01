// Polymarket API type definitions

export interface PolymarketMarket {
  id: string;
  question: string;
  description?: string;
  slug?: string;
  active: boolean;
  closed: boolean;
  archived: boolean;
  outcomes: string[];
  outcomePrices: string[];
  volume: string;
  liquidity?: string;
  endDate?: string;
  startDate?: string;
  image?: string;
  icon?: string;
  category?: string;
  tags?: PolymarketTag[];
  conditionId?: string;
  questionID?: string;
  enableOrderBook?: boolean;
  clobTokenIds?: string[];
  spread?: number;
  volumeNum?: number;
  liquidityNum?: number;
  // Gamma API nests the parent event(s) — used for constructing the correct URL
  events?: Array<{ id?: string; slug: string; title?: string }>;
  groupItemTitle?: string;
  // Volume variants
  volume24hr?: number;
  volume1wk?: number;
  volume1mo?: number;
  // Price change (raw decimal, multiply ×100 for %)
  oneDayPriceChange?: number;
  oneWeekPriceChange?: number;
  oneMonthPriceChange?: number;
  oneHourPriceChange?: number;
  // Live pricing from Gamma/CLOB
  lastTradePrice?: number;
  bestBid?: number;
  bestAsk?: number;
  // Status flags
  featured?: boolean;
  new?: boolean;
  acceptingOrders?: boolean;
  negRiskOther?: boolean;
  // Resolution metadata
  resolutionSource?: string;
  umaResolutionStatus?: string;
  resolvedBy?: string;
  // Fees & order config
  makerBaseFee?: number;
  takerBaseFee?: number;
  orderMinSize?: number;
  orderPriceMinTickSize?: number;
  // Rankings/discovery
  competitive?: number;
  score?: number;
  curationOrder?: number;
  // Sports
  gameStartTime?: string;
}

export interface PolymarketEvent {
  id: string;
  slug: string;
  title: string;
  description?: string;
  startDate?: string;
  endDate?: string;
  creationDate?: string;
  image?: string;
  icon?: string;
  active: boolean;
  closed: boolean;
  archived: boolean;
  markets: PolymarketMarket[];
  tags?: PolymarketTag[];
  liquidity?: string;
  volume?: string;
  commentCount?: number;
}

export interface PolymarketTag {
  id: string;
  label: string;
  slug?: string;
}

export interface PolymarketSport {
  id: number;
  sport: string;
  image?: string;
  resolution?: string;
  ordering?: string;
  /** Comma-separated tag IDs associated with this sport */
  tags?: string;
  series?: string;
  createdAt?: string;
}

export interface PolymarketTrade {
  id: string;
  market: string;
  asset_id: string;
  maker_address: string;
  taker_address?: string;
  price: string;
  size: string;
  side: 'BUY' | 'SELL';
  fee_rate_bps: string;
  timestamp: number;
  transaction_hash?: string;
}

export interface PolymarketOrderBook {
  market: string;
  asset_id: string;
  bids: Array<{ price: string; size: string }>;
  asks: Array<{ price: string; size: string }>;
  timestamp: number;
  hash?: string;
}

// Enriched order book from GET /book?token_id= (includes extra fields vs /orderbook/:id)
export interface PolymarketOrderBookEnriched extends PolymarketOrderBook {
  min_order_size?: string;
  tick_size?: string;
  last_trade_price?: string;
  neg_risk?: boolean;
}

// Top holders from Data API GET /holders?market={conditionId}
export interface PolymarketHolder {
  proxyWallet: string;
  pseudonym?: string;
  name?: string;
  amount: number;
  profileImage?: string;
  profileImageOptimized?: string;
  outcomeIndex: number;
  asset?: string;
  displayUsernamePublic?: boolean;
}

export interface PolymarketTopHolders {
  token: string;
  holders: PolymarketHolder[];
}

// Open interest from Data API GET /openInterest?market={conditionId}
export interface PolymarketOpenInterest {
  market: string;
  value: number;
}

export interface PolymarketProfile {
  wallet_address: string;
  username?: string;
  bio?: string;
  avatar_url?: string;
  twitter?: string;
  website?: string;
  reputation_score?: number;
}

export interface PolymarketComment {
  id: string;
  market_id: string;
  user_address: string;
  content: string;
  timestamp: number;
  likes?: number;
}

// Data API Types — schemas match https://data-api.polymarket.com responses exactly

// GET /positions?user=
export interface UserPosition {
  proxyWallet: string;
  asset: string;
  conditionId: string;
  size: number;
  avgPrice: number;
  initialValue: number;
  currentValue: number;
  cashPnl: number;
  percentPnl: number;
  totalBought: number;
  realizedPnl: number;
  percentRealizedPnl: number;
  curPrice: number;
  redeemable: boolean;
  mergeable: boolean;
  negativeRisk: boolean;
  title: string;
  slug: string;
  icon?: string;
  eventSlug?: string;
  outcome: string;
  oppositeOutcome?: string;
  oppositeAsset?: string;
  outcomeIndex: number;
  endDate?: string;
}

// GET /value?user=
export interface UserPortfolioValue {
  user: string;
  value: number;
}

// GET /activity?user= — real Data API wire format
export interface UserActivity {
  proxyWallet: string;
  timestamp: number;        // unix seconds
  conditionId: string;
  type: 'BUY' | 'SELL' | 'REDEEM' | 'REWARD' | 'DEPOSIT' | 'WITHDRAWAL' | string;
  size: number;             // token size
  usdcSize: number;         // USDC value
  transactionHash: string;
  price: number;
  asset: string;
  side: string;
  outcomeIndex: number;
  title: string;
  slug: string;
  icon: string;
  eventSlug: string;
  outcome: string;
  name: string;
  pseudonym: string;
  bio: string;
  profileImage: string;
  profileImageOptimized: string;
}

// Derived user profile — assembled from first activity/position record
export interface UserProfile {
  proxyWallet: string;
  name: string;
  pseudonym: string;
  bio: string;
  profileImage: string;
  profileImageOptimized: string;
}

// GET /closed-positions?user= — resolved positions with realized P&L
export interface ClosedPosition {
  proxyWallet: string;
  asset: string;
  conditionId: string;
  avgPrice: number;
  totalBought: number;
  realizedPnl: number;
  curPrice: number;
  timestamp: number;       // unix seconds
  title: string;
  slug?: string;
  icon?: string;
  eventSlug?: string;
  outcome: string;
  outcomeIndex: number;
  oppositeOutcome?: string;
  oppositeAsset?: string;
  endDate?: string;
}

// GET /trades?user= — Data API user trade history
export interface UserTradeHistory {
  proxyWallet: string;
  side: 'BUY' | 'SELL';
  asset: string;
  conditionId: string;
  size: number;
  price: number;
  timestamp: number;       // unix seconds (number from API)
  title: string;
  slug?: string;
  icon?: string;
  eventSlug?: string;
  outcome: string;
  outcomeIndex: number;
  name?: string;
  pseudonym?: string;
  profileImage?: string;
  transactionHash?: string;
}

// Pricing Types
export interface PricePoint {
  price: number;   // normalized 0–1 float
  timestamp: number; // unix seconds
}

export interface HistoricalPrice {
  token_id: string;
  prices: PricePoint[];
  interval: string;
}

export interface MidpointPrice {
  token_id: string;
  mid: string;
  timestamp: number;
}

export interface BulkPriceRequest {
  token_ids: string[];
  side?: 'BUY' | 'SELL';
}

export interface BulkPriceResponse {
  prices: Array<{
    token_id: string;
    price: string;
    timestamp: number;
  }>;
}

// Order Types
export interface OrderRequest {
  token_id: string;
  price: string;
  size: string;
  side: 'BUY' | 'SELL';
  fee_rate_bps?: string;
  nonce?: number;
  expiration?: number;
}

export interface Order {
  id: string;
  market: string;
  asset_id: string;
  owner: string;
  price: string;
  size: string;
  size_matched: string;
  side: 'BUY' | 'SELL';
  status: 'LIVE' | 'MATCHED' | 'CANCELED';
  created_at: number;
  updated_at: number;
  expiration?: number;
  signature?: string;
}

export interface CreateOrderResponse {
  order_id: string;
  status: string;
  created_at: number;
}

// WebSocket Types
// Event types available with custom_feature_enabled=true:
//   best_bid_ask, new_market, market_resolved
// Standard event types (always available):
//   book, price_change, tick_size_change, last_trade_price
// Actual wire format from Polymarket WebSocket (verified against live data):
// Fields are FLAT on the message object, not nested under "data".
// The discriminant field is "event_type", not "type".
export interface WSMarketUpdate {
  event_type: 'book' | 'price_change' | 'tick_size_change' | 'last_trade_price' | 'best_bid_ask' | 'new_market' | 'market_resolved';
  market: string;
  asset_id?: string;
  timestamp: number | string;
  hash?: string;
  // book event
  bids?: Array<{ price: string; size: string }>;
  asks?: Array<{ price: string; size: string }>;
  tick_size?: string;
  last_trade_price?: string;
  // price_change event
  price?: string;
  side?: 'BUY' | 'SELL';
  size?: string;
  // best_bid_ask event
  best_bid?: string;
  best_ask?: string;
  spread?: string;
  // new_market / market_resolved
  question?: string;
  outcomes?: string[];
  winner?: string;
  // keep data for any undocumented fields
  data?: any;
}

// User WebSocket — flat message format per asyncapi-user.json
// Discriminant is event_type: "order" | "trade" (NOT a { type, data } wrapper)
export interface WSUserOrderEvent {
  event_type: 'order';
  id: string;
  owner: string;
  market: string;         // conditionId
  asset_id: string;       // tokenId
  side: 'BUY' | 'SELL';
  original_size: string;
  size_matched: string;
  price: string;
  outcome: string;
  type: 'PLACEMENT' | 'UPDATE' | 'CANCELLATION';
  status: string;         // "LIVE", "CANCELED", "MATCHED", etc.
  order_type: 'GTC' | 'GTD' | 'FOK';
  created_at: string;
  expiration?: string;
  maker_address?: string;
  associate_trades?: string[];
  timestamp: string;      // milliseconds as string
}

export interface WSUserTradeEvent {
  event_type: 'trade';
  id: string;
  taker_order_id: string;
  market: string;         // conditionId
  asset_id: string;       // tokenId
  side: 'BUY' | 'SELL';
  size: string;
  price: string;
  fee_rate_bps: string;
  status: 'MATCHED' | 'MINED' | 'CONFIRMED' | 'RETRYING' | 'FAILED';
  matchtime: string;
  last_update: string;
  outcome: string;
  owner: string;
  trader_side: 'TAKER' | 'MAKER';
  transaction_hash?: string;
  maker_orders?: Array<{
    order_id: string;
    owner: string;
    maker_address: string;
    matched_amount: string;
    price: string;
    fee_rate_bps: string;
    asset_id: string;
    outcome: string;
    side: 'BUY' | 'SELL';
  }>;
  timestamp: string;      // milliseconds as string
}

export type WSUserUpdate = WSUserOrderEvent | WSUserTradeEvent;

// Query Parameters
export interface MarketQueryParams {
  limit?: number;
  offset?: number;
  tag_id?: string;
  closed?: boolean;
  archived?: boolean;
  active?: boolean;
  order?: string;
  ascending?: boolean;
  slug?: string;
  market_ids?: string[];
  token_ids?: string[];
  condition_ids?: string[];
}

export interface EventQueryParams {
  limit?: number;
  offset?: number;
  tag_id?: string;
  closed?: boolean;
  archived?: boolean;
  active?: boolean;
  order?: string;
  ascending?: boolean;
  slug?: string;
  related_tags?: boolean;
  exclude_tag_id?: string;
}

export interface PriceHistoryParams {
  token_id: string;
  /** Time window. Valid values: 'max' | 'all' | '1m' | '1w' | '1d' | '6h' | '1h'. Default: '1d' */
  interval?: string;
  /** Start timestamp in milliseconds (omit to use interval) */
  startTs?: number;
  /** End timestamp in milliseconds (omit to use interval) */
  endTs?: number;
  /** Resolution accuracy in MINUTES (e.g. 1 = 1-minute resolution). Default: 1 */
  fidelity?: number;
}

// API Credentials (for authenticated requests)
export interface APICredentials {
  apiKey: string;
  apiSecret: string;
  apiPassphrase: string;
  walletAddress?: string;
}
