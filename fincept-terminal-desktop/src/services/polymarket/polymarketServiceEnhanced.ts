// File: src/services/polymarketServiceEnhanced.ts
// Complete Polymarket API service with all endpoints

const GAMMA_API_BASE = 'https://gamma-api.polymarket.com';
const CLOB_API_BASE = 'https://clob.polymarket.com';
const DATA_API_BASE = 'https://data-api.polymarket.com';
const WS_MARKET_BASE = 'wss://ws-subscriptions-clob.polymarket.com/ws/market';
const WS_USER_BASE = 'wss://ws-subscriptions-clob.polymarket.com/ws/user';

// ==================== TYPE DEFINITIONS ====================

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

// Data API Types
export interface UserPosition {
  id: string;
  market_id: string;
  asset_id: string;
  user_address: string;
  outcome: string;
  size: string;
  entry_price: string;
  current_price?: string;
  unrealized_pnl?: string;
  timestamp: number;
}

export interface UserActivity {
  id: string;
  user_address: string;
  type: 'trade' | 'deposit' | 'withdrawal' | 'claim';
  market_id?: string;
  amount: string;
  timestamp: number;
  transaction_hash: string;
  status: 'pending' | 'confirmed' | 'failed';
}

export interface UserTradeHistory {
  id: string;
  market_id: string;
  user_address: string;
  side: 'BUY' | 'SELL';
  outcome: string;
  size: string;
  price: string;
  fee: string;
  timestamp: number;
  realized_pnl?: string;
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

export interface WSUserUpdate {
  type: 'order' | 'trade' | 'position';
  data: Order | PolymarketTrade | UserPosition;
  timestamp: number;
}

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

// ==================== SERVICE CLASS ====================

class PolymarketServiceEnhanced {
  private gammaApiBase: string;
  private clobApiBase: string;
  private dataApiBase: string;
  private wsMarketBase: string;
  private wsUserBase: string;
  private credentials?: APICredentials;
  private wsConnections: Map<string, WebSocket>;

  constructor() {
    this.gammaApiBase = GAMMA_API_BASE;
    this.clobApiBase = CLOB_API_BASE;
    this.dataApiBase = DATA_API_BASE;
    this.wsMarketBase = WS_MARKET_BASE;
    this.wsUserBase = WS_USER_BASE;
    this.wsConnections = new Map();
  }

  // ==================== AUTHENTICATION ====================

  setCredentials(credentials: APICredentials) {
    this.credentials = credentials;
  }

  clearCredentials() {
    this.credentials = undefined;
  }

  private async generateAuthHeaders(timestamp: number, method: string, path: string, body?: string): Promise<HeadersInit> {
    if (!this.credentials) {
      throw new Error('API credentials not set');
    }

    // Build the message to sign: timestamp + method + path + body
    const message = timestamp.toString() + method.toUpperCase() + path + (body ?? '');

    // HMAC-SHA256 signature using Web Crypto API
    const encoder = new TextEncoder();
    const keyData = encoder.encode(this.credentials.apiSecret);
    const msgData = encoder.encode(message);

    const cryptoKey = await crypto.subtle.importKey(
      'raw',
      keyData,
      { name: 'HMAC', hash: 'SHA-256' },
      false,
      ['sign']
    );

    const signatureBuffer = await crypto.subtle.sign('HMAC', cryptoKey, msgData);
    const signatureBytes = new Uint8Array(signatureBuffer);
    // Base64 encode the signature
    const signature = btoa(String.fromCharCode(...signatureBytes));

    return {
      'Content-Type': 'application/json',
      'POLY_ADDRESS': this.credentials.walletAddress ?? '',
      'POLY_API_KEY': this.credentials.apiKey,
      'POLY_PASSPHRASE': this.credentials.apiPassphrase,
      'POLY_SIGNATURE': signature,
      'POLY_TIMESTAMP': timestamp.toString(),
    };
  }

  // ==================== GAMMA API - Market Data ====================

  async getMarkets(params?: MarketQueryParams): Promise<PolymarketMarket[]> {
    try {
      const queryParams = new URLSearchParams();
      if (params?.limit) queryParams.append('limit', params.limit.toString());
      if (params?.offset) queryParams.append('offset', params.offset.toString());
      if (params?.tag_id) queryParams.append('tag_id', params.tag_id);
      if (params?.closed !== undefined) queryParams.append('closed', params.closed.toString());
      if (params?.archived !== undefined) queryParams.append('archived', params.archived.toString());
      if (params?.active !== undefined) queryParams.append('active', params.active.toString());
      if (params?.order) queryParams.append('order', params.order);
      if (params?.ascending !== undefined) queryParams.append('ascending', params.ascending.toString());

      const url = `${this.gammaApiBase}/markets${queryParams.toString() ? '?' + queryParams.toString() : ''}`;
      const response = await fetch(url);

      if (!response.ok) {
        throw new Error(`Gamma API error: ${response.status} ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      console.error('Error fetching markets:', error);
      throw error;
    }
  }

  async getMarketBySlug(slug: string): Promise<PolymarketMarket> {
    try {
      const url = `${this.gammaApiBase}/markets/slug/${slug}`;
      const response = await fetch(url);
      if (!response.ok) throw new Error(`Gamma API error: ${response.status}`);
      return await response.json();
    } catch (error) {
      console.error('Error fetching market by slug:', error);
      throw error;
    }
  }

  async getEvents(params?: EventQueryParams): Promise<PolymarketEvent[]> {
    try {
      const queryParams = new URLSearchParams();
      if (params?.limit) queryParams.append('limit', params.limit.toString());
      if (params?.offset) queryParams.append('offset', params.offset.toString());
      if (params?.tag_id) queryParams.append('tag_id', params.tag_id);
      if (params?.closed !== undefined) queryParams.append('closed', params.closed.toString());
      if (params?.archived !== undefined) queryParams.append('archived', params.archived.toString());
      if (params?.active !== undefined) queryParams.append('active', params.active.toString());
      if (params?.order) queryParams.append('order', params.order);
      if (params?.ascending !== undefined) queryParams.append('ascending', params.ascending.toString());
      if (params?.related_tags !== undefined) queryParams.append('related_tags', params.related_tags.toString());
      if (params?.exclude_tag_id) queryParams.append('exclude_tag_id', params.exclude_tag_id);

      const url = `${this.gammaApiBase}/events${queryParams.toString() ? '?' + queryParams.toString() : ''}`;
      const response = await fetch(url);
      if (!response.ok) throw new Error(`Gamma API error: ${response.status}`);
      return await response.json();
    } catch (error) {
      console.error('Error fetching events:', error);
      throw error;
    }
  }

  async getEventBySlug(slug: string): Promise<PolymarketEvent> {
    try {
      const url = `${this.gammaApiBase}/events/slug/${slug}`;
      const response = await fetch(url);
      if (!response.ok) throw new Error(`Gamma API error: ${response.status}`);
      return await response.json();
    } catch (error) {
      console.error('Error fetching event by slug:', error);
      throw error;
    }
  }

  async getTags(): Promise<PolymarketTag[]> {
    try {
      const url = `${this.gammaApiBase}/tags`;
      const response = await fetch(url);
      if (!response.ok) throw new Error(`Gamma API error: ${response.status}`);
      return await response.json();
    } catch (error) {
      console.error('Error fetching tags:', error);
      throw error;
    }
  }

  async getSports(): Promise<PolymarketSport[]> {
    try {
      const url = `${this.gammaApiBase}/sports`;
      const response = await fetch(url);
      if (!response.ok) throw new Error(`Gamma API error: ${response.status}`);
      return await response.json();
    } catch (error) {
      console.error('Error fetching sports:', error);
      throw error;
    }
  }

  async search(query: string, type: 'markets' | 'events' | 'all' = 'all'): Promise<any> {
    try {
      const url = `${this.gammaApiBase}/search?q=${encodeURIComponent(query)}&type=${type}`;
      const response = await fetch(url);
      if (!response.ok) throw new Error(`Gamma API error: ${response.status}`);
      return await response.json();
    } catch (error) {
      console.error('Error searching:', error);
      throw error;
    }
  }

  // ==================== CLOB API - Trading Data ====================

  async getCLOBMarkets(nextCursor?: string): Promise<any> {
    try {
      const url = `${this.clobApiBase}/markets${nextCursor ? `?next_cursor=${nextCursor}` : ''}`;
      const response = await fetch(url);
      if (!response.ok) throw new Error(`CLOB API error: ${response.status}`);
      return await response.json();
    } catch (error) {
      console.error('Error fetching CLOB markets:', error);
      throw error;
    }
  }

  async getOrderBook(tokenId: string): Promise<PolymarketOrderBook> {
    try {
      // Official endpoint: GET /orderbook/:tokenId (path parameter, not query string)
      const url = `${this.clobApiBase}/orderbook/${tokenId}`;
      const response = await fetch(url);
      if (!response.ok) throw new Error(`CLOB API error: ${response.status}`);
      return await response.json();
    } catch (error) {
      console.error('Error fetching order book:', error);
      throw error;
    }
  }

  async getOrderBooks(tokenIds: string[]): Promise<PolymarketOrderBook[]> {
    try {
      const url = `${this.clobApiBase}/books`;
      const response = await fetch(url, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ token_ids: tokenIds })
      });
      if (!response.ok) throw new Error(`CLOB API error: ${response.status}`);
      return await response.json();
    } catch (error) {
      console.error('Error fetching multiple order books:', error);
      throw error;
    }
  }

  async getPrice(tokenId: string, side?: 'BUY' | 'SELL'): Promise<any> {
    try {
      const url = `${this.clobApiBase}/price?token_id=${tokenId}${side ? `&side=${side}` : ''}`;
      const response = await fetch(url);
      if (!response.ok) throw new Error(`CLOB API error: ${response.status}`);
      return await response.json();
    } catch (error) {
      console.error('Error fetching price:', error);
      throw error;
    }
  }

  async getPrices(tokenIds: string[], side?: 'BUY' | 'SELL'): Promise<BulkPriceResponse> {
    try {
      const url = `${this.clobApiBase}/prices`;
      const response = await fetch(url, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ token_ids: tokenIds, side })
      });
      if (!response.ok) throw new Error(`CLOB API error: ${response.status}`);
      return await response.json();
    } catch (error) {
      console.error('Error fetching multiple prices:', error);
      throw error;
    }
  }

  async getMidpoint(tokenId: string): Promise<MidpointPrice> {
    try {
      const url = `${this.clobApiBase}/midpoint?token_id=${tokenId}`;
      const response = await fetch(url);
      if (!response.ok) throw new Error(`CLOB API error: ${response.status}`);
      return await response.json();
    } catch (error) {
      console.error('Error fetching midpoint:', error);
      throw error;
    }
  }

  async getPriceHistory(params: PriceHistoryParams): Promise<HistoricalPrice> {
    try {
      const queryParams = new URLSearchParams();
      // API requires: market = numeric tokenId (NOT conditionId — conditionId returns empty)
      queryParams.append('market', params.token_id);
      // 'interval' is MANDATORY — omitting it returns an API error
      // Valid values: 'max' | 'all' | '1m' | '1w' | '1d' | '6h' | '1h'
      queryParams.append('interval', params.interval ?? '1d');
      // fidelity = resolution accuracy in MINUTES (default 1 = 1-minute resolution)
      queryParams.append('fidelity', (params.fidelity ?? 1).toString());

      const url = `${this.clobApiBase}/prices-history?${queryParams.toString()}`;
      console.log('Price history URL:', url);

      const response = await fetch(url);
      if (!response.ok) {
        const errorText = await response.text();
        console.error('Price history error:', errorText);
        throw new Error(`CLOB API error: ${response.status}`);
      }

      const data = await response.json();
      console.log('Price history response:', data);
      console.log('First history item:', data.history?.[0]);

      // API returns { history: [{ p: number, t: number }, ...] }
      // p = price (0–1 float), t = unix timestamp in seconds
      const raw: any[] = data.history || data.prices || [];
      const prices = raw
        .map((item: any) => {
          const p = parseFloat(item.p ?? item.price ?? '0');
          const t = Number(item.t ?? item.timestamp ?? 0);
          return { price: p, timestamp: t };
        })
        .filter(pt => !isNaN(pt.price) && pt.timestamp > 0)
        .sort((a, b) => a.timestamp - b.timestamp); // ensure chronological order

      return {
        token_id: params.token_id,
        prices,
        interval: params.interval ?? '1d',
      };
    } catch (error) {
      console.error('Error fetching price history:', error);
      throw error;
    }
  }

  async getSpread(tokenId: string): Promise<any> {
    try {
      const url = `${this.clobApiBase}/spread?token_id=${tokenId}`;
      const response = await fetch(url);
      if (!response.ok) throw new Error(`CLOB API error: ${response.status}`);
      return await response.json();
    } catch (error) {
      console.error('Error fetching spread:', error);
      throw error;
    }
  }

  async getSpreads(tokenIds: string[]): Promise<any> {
    try {
      const url = `${this.clobApiBase}/spreads`;
      const response = await fetch(url, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ token_ids: tokenIds })
      });
      if (!response.ok) throw new Error(`CLOB API error: ${response.status}`);
      return await response.json();
    } catch (error) {
      console.error('Error fetching multiple spreads:', error);
      throw error;
    }
  }

  async getTrades(params?: {
    token_id?: string;
    market?: string;
    maker_address?: string;
    limit?: number;
    offset?: number;
  }): Promise<PolymarketTrade[]> {
    try {
      const queryParams = new URLSearchParams();
      if (params?.token_id) queryParams.append('token_id', params.token_id);
      if (params?.market) queryParams.append('market', params.market);
      if (params?.maker_address) queryParams.append('maker_address', params.maker_address);
      if (params?.limit) queryParams.append('limit', params.limit.toString());
      if (params?.offset) queryParams.append('offset', params.offset.toString());

      const url = `${this.clobApiBase}/data/trades${queryParams.toString() ? '?' + queryParams.toString() : ''}`;
      const response = await fetch(url);
      if (!response.ok) throw new Error(`CLOB API error: ${response.status}`);
      const json = await response.json();
      // CLOB API returns paginated: { data: PolymarketTrade[], count, limit, offset }
      return Array.isArray(json) ? json : (json.data ?? []);
    } catch (error) {
      console.error('Error fetching trades:', error);
      throw error;
    }
  }

  // ==================== DATA API - User Data ====================

  async getUserPositions(userAddress: string): Promise<UserPosition[]> {
    try {
      const url = `${this.dataApiBase}/positions?user=${userAddress}`;
      const response = await fetch(url);
      if (!response.ok) throw new Error(`Data API error: ${response.status}`);
      return await response.json();
    } catch (error) {
      console.error('Error fetching user positions:', error);
      throw error;
    }
  }

  async getUserActivity(userAddress: string, limit?: number): Promise<UserActivity[]> {
    try {
      const url = `${this.dataApiBase}/activity?user=${userAddress}${limit ? `&limit=${limit}` : ''}`;
      const response = await fetch(url);
      if (!response.ok) throw new Error(`Data API error: ${response.status}`);
      return await response.json();
    } catch (error) {
      console.error('Error fetching user activity:', error);
      throw error;
    }
  }

  async getUserTrades(userAddress: string, limit?: number): Promise<UserTradeHistory[]> {
    try {
      const url = `${this.dataApiBase}/trades?user=${userAddress}${limit ? `&limit=${limit}` : ''}`;
      const response = await fetch(url);
      if (!response.ok) throw new Error(`Data API error: ${response.status}`);
      return await response.json();
    } catch (error) {
      console.error('Error fetching user trades:', error);
      throw error;
    }
  }

  // ==================== DATA API - Market Data ====================

  async getTopHolders(conditionId: string, limit: number = 20): Promise<PolymarketTopHolders> {
    try {
      const url = `${this.dataApiBase}/holders?market=${encodeURIComponent(conditionId)}&limit=${limit}`;
      const response = await fetch(url);
      if (!response.ok) throw new Error(`Data API error: ${response.status}`);
      const data = await response.json();
      // API returns array of MetaHolder objects: [{token, holders:[...]}, ...]
      if (Array.isArray(data)) {
        // Flatten all holders across token entries
        const allHolders: PolymarketHolder[] = data.flatMap((entry: any) =>
          (entry.holders ?? []).map((h: any) => ({
            ...h,
            outcomeIndex: h.outcomeIndex ?? (entry.token === data[0]?.token ? 0 : 1),
          }))
        );
        return { token: data[0]?.token ?? '', holders: allHolders };
      }
      return data as PolymarketTopHolders;
    } catch (error) {
      console.error('Error fetching top holders:', error);
      throw error;
    }
  }

  async getOpenInterest(conditionIds: string[]): Promise<PolymarketOpenInterest[]> {
    try {
      const marketParam = conditionIds.map(encodeURIComponent).join(',');
      const url = `${this.dataApiBase}/openInterest?market=${marketParam}`;
      const response = await fetch(url);
      if (!response.ok) throw new Error(`Data API error: ${response.status}`);
      const data = await response.json();
      return Array.isArray(data) ? data : [data];
    } catch (error) {
      console.error('Error fetching open interest:', error);
      throw error;
    }
  }

  async getOrderBookEnriched(tokenId: string): Promise<PolymarketOrderBookEnriched> {
    try {
      // Use /book?token_id= which returns extra fields: min_order_size, tick_size, last_trade_price, neg_risk
      const url = `${this.clobApiBase}/book?token_id=${tokenId}`;
      const response = await fetch(url);
      if (!response.ok) throw new Error(`CLOB API error: ${response.status}`);
      const data = await response.json();
      // Ensure bids sorted desc by price, asks sorted asc
      if (data.bids) {
        data.bids = data.bids.sort((a: any, b: any) => parseFloat(b.price) - parseFloat(a.price));
      }
      if (data.asks) {
        data.asks = data.asks.sort((a: any, b: any) => parseFloat(a.price) - parseFloat(b.price));
      }
      return data as PolymarketOrderBookEnriched;
    } catch (error) {
      console.error('Error fetching enriched order book:', error);
      throw error;
    }
  }

  // ==================== ORDER MANAGEMENT (L2) ====================
  // Note: These require authentication and proper signature generation

  async createOrder(orderRequest: OrderRequest): Promise<CreateOrderResponse> {
    if (!this.credentials) {
      throw new Error('Authentication required. Please set API credentials.');
    }

    try {
      const timestamp = Math.floor(Date.now() / 1000);
      const url = `${this.clobApiBase}/order`;
      const body = JSON.stringify(orderRequest);
      const headers = await this.generateAuthHeaders(timestamp, 'POST', '/order', body);

      const response = await fetch(url, {
        method: 'POST',
        headers,
        body: JSON.stringify(orderRequest)
      });

      if (!response.ok) throw new Error(`CLOB API error: ${response.status}`);
      return await response.json();
    } catch (error) {
      console.error('Error creating order:', error);
      throw error;
    }
  }

  async getOpenOrders(userAddress?: string): Promise<Order[]> {
    if (!this.credentials) {
      throw new Error('Authentication required. Please set API credentials.');
    }

    try {
      const timestamp = Math.floor(Date.now() / 1000);
      const path = userAddress ? `/orders?user=${userAddress}` : '/orders';
      const url = `${this.clobApiBase}${path}`;
      const headers = await this.generateAuthHeaders(timestamp, 'GET', path);

      const response = await fetch(url, { headers });
      if (!response.ok) throw new Error(`CLOB API error: ${response.status}`);
      return await response.json();
    } catch (error) {
      console.error('Error fetching open orders:', error);
      throw error;
    }
  }

  async cancelOrder(orderId: string): Promise<{ success: boolean }> {
    if (!this.credentials) {
      throw new Error('Authentication required. Please set API credentials.');
    }

    try {
      const timestamp = Math.floor(Date.now() / 1000);
      const path = `/order/${orderId}`;
      const url = `${this.clobApiBase}${path}`;
      const headers = await this.generateAuthHeaders(timestamp, 'DELETE', path);

      const response = await fetch(url, {
        method: 'DELETE',
        headers
      });

      if (!response.ok) throw new Error(`CLOB API error: ${response.status}`);
      return await response.json();
    } catch (error) {
      console.error('Error canceling order:', error);
      throw error;
    }
  }

  async cancelAllOrders(marketId?: string): Promise<{ canceled: number }> {
    if (!this.credentials) {
      throw new Error('Authentication required. Please set API credentials.');
    }

    try {
      const timestamp = Math.floor(Date.now() / 1000);
      // Official endpoint: DELETE /cancel-all (optionally with body { market })
      const path = '/cancel-all';
      const body = marketId ? JSON.stringify({ market: marketId }) : undefined;
      const url = `${this.clobApiBase}${path}`;
      const headers = await this.generateAuthHeaders(timestamp, 'DELETE', path, body);

      const response = await fetch(url, {
        method: 'DELETE',
        headers,
        ...(body ? { body } : {}),
      });

      if (!response.ok) throw new Error(`CLOB API error: ${response.status}`);
      return await response.json();
    } catch (error) {
      console.error('Error canceling all orders:', error);
      throw error;
    }
  }

  // ==================== WEBSOCKET CONNECTIONS ====================

  connectMarketWebSocket(
    assetIds: string[],
    onMessage: (update: WSMarketUpdate) => void,
    onError?: (error: Event) => void
  ): WebSocket {
    const ws = new WebSocket(this.wsMarketBase);

    ws.onopen = () => {
      console.log('Market WebSocket connected');
      // Official subscribe format: type="market", assets_ids=[...tokenIds], initial_dump=true
      // custom_feature_enabled=true enables extra event types: best_bid_ask, new_market, market_resolved
      ws.send(JSON.stringify({
        type: 'market',
        assets_ids: assetIds,
        initial_dump: true,
        custom_feature_enabled: true,
      }));
      // Keepalive: docs require PING every 10 seconds (server closes after ~10s silence)
      const pingInterval = setInterval(() => {
        if (ws.readyState === WebSocket.OPEN) {
          ws.send('PING');
        } else {
          clearInterval(pingInterval);
        }
      }, 10_000);
      (ws as any)._pingInterval = pingInterval;
    };

    ws.onmessage = (event) => {
      try {
        if (event.data === 'PONG') return;
        const data = JSON.parse(event.data);
        // Polymarket sends messages as arrays of event objects
        const messages: WSMarketUpdate[] = Array.isArray(data) ? data : [data];
        for (const msg of messages) {
          onMessage(msg);
        }
      } catch (error) {
        console.error('Error parsing WebSocket message:', error);
      }
    };

    ws.onerror = (error) => {
      console.error('Market WebSocket error:', error);
      if (onError) onError(error);
    };

    ws.onclose = () => {
      console.log('Market WebSocket disconnected');
      if ((ws as any)._pingInterval) clearInterval((ws as any)._pingInterval);
    };

    this.wsConnections.set('market', ws);
    return ws;
  }

  connectUserWebSocket(
    conditionIds: string[],
    onMessage: (update: WSUserUpdate) => void,
    onError?: (error: Event) => void
  ): WebSocket {
    if (!this.credentials) {
      throw new Error('Authentication required for user WebSocket');
    }

    const ws = new WebSocket(this.wsUserBase);
    const creds = this.credentials;

    ws.onopen = () => {
      console.log('User WebSocket connected');
      // Official auth format: auth object + type="user" + markets=[conditionIds]
      ws.send(JSON.stringify({
        auth: {
          apiKey: creds.apiKey,
          secret: creds.apiSecret,
          passphrase: creds.apiPassphrase,
        },
        type: 'user',
        markets: conditionIds,
        initial_dump: true,
      }));
      // Keepalive PING every 10 seconds (docs requirement)
      const pingInterval = setInterval(() => {
        if (ws.readyState === WebSocket.OPEN) {
          ws.send('PING');
        } else {
          clearInterval(pingInterval);
        }
      }, 10_000);
      (ws as any)._pingInterval = pingInterval;
    };

    ws.onmessage = (event) => {
      try {
        if (event.data === 'PONG') return;
        const data = JSON.parse(event.data);
        // Polymarket sends messages as arrays of event objects
        const messages: WSUserUpdate[] = Array.isArray(data) ? data : [data];
        for (const msg of messages) {
          onMessage(msg);
        }
      } catch (error) {
        console.error('Error parsing WebSocket message:', error);
      }
    };

    ws.onerror = (error) => {
      console.error('User WebSocket error:', error);
      if (onError) onError(error);
    };

    ws.onclose = () => {
      console.log('User WebSocket disconnected');
      if ((ws as any)._pingInterval) clearInterval((ws as any)._pingInterval);
    };

    this.wsConnections.set('user', ws);
    return ws;
  }

  subscribeToMarket(assetIds: string[]) {
    const ws = this.wsConnections.get('market');
    if (ws && ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify({
        type: 'market',
        assets_ids: assetIds,
        initial_dump: false,
      }));
    }
  }

  unsubscribeFromMarket(_assetIds: string[]) {
    // Polymarket WS does not have an explicit unsubscribe message;
    // close and reconnect with a different assets_ids list if needed.
    console.warn('[Polymarket WS] To unsubscribe, reconnect with updated assets_ids list.');
  }

  disconnectWebSocket(type: 'market' | 'user' | 'all' = 'all') {
    if (type === 'all') {
      this.wsConnections.forEach((ws) => ws.close());
      this.wsConnections.clear();
    } else {
      const ws = this.wsConnections.get(type);
      if (ws) {
        ws.close();
        this.wsConnections.delete(type);
      }
    }
  }

  // ==================== UTILITY METHODS ====================

  calculateProbability(price: string | number): number {
    const priceNum = typeof price === 'string' ? parseFloat(price) : price;
    return priceNum * 100;
  }

  calculateProfit(betAmount: number, price: number, outcome: 'yes' | 'no'): number {
    if (outcome === 'yes') {
      return betAmount * (1 / price - 1);
    } else {
      return betAmount * (1 / (1 - price) - 1);
    }
  }

  formatVolume(volume: string | number): string {
    const volNum = typeof volume === 'string' ? parseFloat(volume) : volume;

    // Handle NaN, null, undefined
    if (isNaN(volNum) || volNum === null || volNum === undefined) {
      return '$0.00';
    }

    if (volNum >= 1000000) {
      return `$${(volNum / 1000000).toFixed(2)}M`;
    } else if (volNum >= 1000) {
      return `$${(volNum / 1000).toFixed(2)}K`;
    } else {
      return `$${volNum.toFixed(2)}`;
    }
  }

  getMarketStatus(market: PolymarketMarket): 'active' | 'closed' | 'archived' {
    if (market.archived) return 'archived';
    if (market.closed) return 'closed';
    if (market.active) return 'active';
    return 'closed';
  }

  isMarketTradeable(market: PolymarketMarket): boolean {
    return market.active && !market.closed && !market.archived && market.enableOrderBook === true;
  }

  calculateUnrealizedPnL(position: UserPosition): number {
    if (!position.current_price) return 0;
    const entryPrice = parseFloat(position.entry_price);
    const currentPrice = parseFloat(position.current_price);
    const size = parseFloat(position.size);
    return (currentPrice - entryPrice) * size;
  }
}

// Export singleton instance
export const polymarketServiceEnhanced = new PolymarketServiceEnhanced();
export default polymarketServiceEnhanced;
