// Polymarket API service — single source of truth for all Polymarket endpoints

const GAMMA_API_BASE = 'https://gamma-api.polymarket.com';
const CLOB_API_BASE = 'https://clob.polymarket.com';
const DATA_API_BASE = 'https://data-api.polymarket.com';
const WS_MARKET_BASE = 'wss://ws-subscriptions-clob.polymarket.com/ws/market';
const WS_USER_BASE = 'wss://ws-subscriptions-clob.polymarket.com/ws/user';

export type {
  PolymarketMarket,
  PolymarketEvent,
  PolymarketTag,
  PolymarketSport,
  PolymarketTrade,
  PolymarketOrderBook,
  PolymarketOrderBookEnriched,
  PolymarketHolder,
  PolymarketTopHolders,
  PolymarketOpenInterest,
  PolymarketProfile,
  PolymarketComment,
  UserPosition,
  UserPortfolioValue,
  UserActivity,
  UserProfile,
  ClosedPosition,
  UserTradeHistory,
  PricePoint,
  HistoricalPrice,
  MidpointPrice,
  BulkPriceRequest,
  BulkPriceResponse,
  OrderRequest,
  Order,
  CreateOrderResponse,
  WSMarketUpdate,
  WSUserOrderEvent,
  WSUserTradeEvent,
  WSUserUpdate,
  MarketQueryParams,
  EventQueryParams,
  PriceHistoryParams,
  APICredentials,
} from './polymarketApiTypes';

import type {
  PolymarketMarket,
  PolymarketEvent,
  PolymarketTag,
  PolymarketSport,
  PolymarketTrade,
  PolymarketOrderBook,
  PolymarketOrderBookEnriched,
  PolymarketHolder,
  PolymarketTopHolders,
  PolymarketOpenInterest,
  UserPosition,
  UserPortfolioValue,
  UserActivity,
  UserProfile,
  ClosedPosition,
  UserTradeHistory,
  HistoricalPrice,
  MidpointPrice,
  BulkPriceResponse,
  OrderRequest,
  Order,
  CreateOrderResponse,
  WSMarketUpdate,
  WSUserUpdate,
  MarketQueryParams,
  EventQueryParams,
  PriceHistoryParams,
  APICredentials,
} from './polymarketApiTypes';

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

  get hasCredentials(): boolean {
    return !!this.credentials;
  }

  getWalletAddress(): string | undefined {
    return this.credentials?.walletAddress;
  }

  private async generateAuthHeaders(timestamp: number, method: string, path: string, body?: string): Promise<HeadersInit> {
    if (!this.credentials) {
      throw new Error('API credentials not set');
    }

    // Build the message to sign: timestamp + method + path + body
    // timestamp is in MILLISECONDS per Polymarket HMAC spec
    const message = timestamp.toString() + method.toUpperCase() + path + (body ?? '');

    // HMAC-SHA256 signature using Web Crypto API
    // The API secret is base64-encoded — decode it to raw bytes before importing as key
    const encoder = new TextEncoder();
    const secretBytes = Uint8Array.from(atob(this.credentials.apiSecret), c => c.charCodeAt(0));
    const msgData = encoder.encode(message);

    const cryptoKey = await crypto.subtle.importKey(
      'raw',
      secretBytes,
      { name: 'HMAC', hash: 'SHA-256' },
      false,
      ['sign']
    );

    const signatureBuffer = await crypto.subtle.sign('HMAC', cryptoKey, msgData);
    const signatureBytes = new Uint8Array(signatureBuffer);
    // URL-safe base64: replace + with - and / with _ (Polymarket spec)
    const signature = btoa(String.fromCharCode(...signatureBytes))
      .replace(/\+/g, '-')
      .replace(/\//g, '_');

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

  async search(query: string, type: 'markets' | 'events' | 'all' = 'all'): Promise<{ markets: PolymarketMarket[]; events: PolymarketEvent[] }> {
    try {
      // Official public search endpoint — no auth required
      // events_status=open filters to only active/open events
      // keep_closed_markets=0 excludes resolved markets from nested market lists
      const params = new URLSearchParams({
        q: query,
        limit_per_type: '50',
        events_status: 'open',
        keep_closed_markets: '0',
        search_tags: 'false',
        search_profiles: 'false',
        sort: 'volume24hr',
        ascending: 'false',
      });
      const url = `${this.gammaApiBase}/public-search?${params.toString()}`;
      const res = await fetch(url);
      if (!res.ok) throw new Error(`Gamma search error: ${res.status}`);
      const data = await res.json();

      // /public-search returns events containing nested markets
      const allEvents: PolymarketEvent[] = Array.isArray(data.events) ? data.events : [];

      // Keep only active, non-closed, non-archived events — sorted by volume
      const events = allEvents
        .filter(e => e.active && !e.closed && !e.archived)
        .sort((a, b) => parseFloat(String(b.volume ?? '0')) - parseFloat(String(a.volume ?? '0')));

      // Flatten markets, keep only active non-closed ones, sort by volumeNum desc
      const markets: PolymarketMarket[] = events
        .flatMap(e => Array.isArray(e.markets) ? e.markets : [])
        .filter(m => m.active && !m.closed && !m.archived)
        .sort((a, b) => (b.volumeNum ?? 0) - (a.volumeNum ?? 0));

      return { markets, events };
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
      const response = await fetch(url);
      if (!response.ok) {
        throw new Error(`CLOB API error: ${response.status}`);
      }
      const data = await response.json();

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

  async getUserPositions(
    userAddress: string,
    params?: {
      limit?: number;
      offset?: number;
      sortBy?: 'CURRENT' | 'INITIAL' | 'TOKENS' | 'CASHPNL' | 'PERCENTPNL' | 'TITLE' | 'RESOLVING' | 'PRICE' | 'AVGPRICE';
      sortDirection?: 'ASC' | 'DESC';
      redeemable?: boolean;
      mergeable?: boolean;
      sizeThreshold?: number;
    }
  ): Promise<UserPosition[]> {
    try {
      const q = new URLSearchParams({ user: userAddress });
      if (params?.limit)         q.set('limit', String(params.limit));
      if (params?.offset)        q.set('offset', String(params.offset));
      if (params?.sortBy)        q.set('sortBy', params.sortBy);
      if (params?.sortDirection) q.set('sortDirection', params.sortDirection);
      if (params?.redeemable != null) q.set('redeemable', String(params.redeemable));
      if (params?.mergeable  != null) q.set('mergeable',  String(params.mergeable));
      if (params?.sizeThreshold != null) q.set('sizeThreshold', String(params.sizeThreshold));
      const response = await fetch(`${this.dataApiBase}/positions?${q}`);
      if (!response.ok) throw new Error(`Data API error: ${response.status}`);
      return await response.json();
    } catch (error) {
      console.error('Error fetching user positions:', error);
      throw error;
    }
  }

  async getPortfolioValue(userAddress: string): Promise<UserPortfolioValue[]> {
    try {
      const response = await fetch(`${this.dataApiBase}/value?user=${userAddress}`);
      if (!response.ok) throw new Error(`Data API error: ${response.status}`);
      return await response.json();
    } catch (error) {
      console.error('Error fetching portfolio value:', error);
      throw error;
    }
  }

  async getUserActivity(
    userAddress: string,
    params?: { limit?: number; offset?: number; type?: string }
  ): Promise<UserActivity[]> {
    try {
      const q = new URLSearchParams({ user: userAddress });
      if (params?.limit)  q.set('limit',  String(params.limit));
      if (params?.offset) q.set('offset', String(params.offset));
      if (params?.type)   q.set('type',   params.type);
      const response = await fetch(`${this.dataApiBase}/activity?${q}`);
      if (!response.ok) throw new Error(`Data API error: ${response.status}`);
      return await response.json();
    } catch (error) {
      console.error('Error fetching user activity:', error);
      throw error;
    }
  }

  // Derives a UserProfile from activity/position records (profile fields are embedded in API responses)
  async getUserProfile(userAddress: string): Promise<UserProfile> {
    // Fetch a small slice of activity — profile fields (name, pseudonym, bio, profileImage) are on each record
    const activity = await this.getUserActivity(userAddress, { limit: 5 }).catch(() => [] as UserActivity[]);
    const first = activity[0];
    return {
      proxyWallet: userAddress,
      name:                  first?.name                  ?? '',
      pseudonym:             first?.pseudonym             ?? '',
      bio:                   first?.bio                   ?? '',
      profileImage:          first?.profileImage          ?? '',
      profileImageOptimized: first?.profileImageOptimized ?? '',
    };
  }

  async getUserTrades(
    userAddress: string,
    params?: { limit?: number; offset?: number; side?: 'BUY' | 'SELL' }
  ): Promise<UserTradeHistory[]> {
    try {
      const q = new URLSearchParams({ user: userAddress });
      if (params?.limit)  q.set('limit',  String(params.limit));
      if (params?.offset) q.set('offset', String(params.offset));
      if (params?.side)   q.set('side',   params.side);
      const response = await fetch(`${this.dataApiBase}/trades?${q}`);
      if (!response.ok) throw new Error(`Data API error: ${response.status}`);
      return await response.json();
    } catch (error) {
      console.error('Error fetching user trades:', error);
      throw error;
    }
  }

  // GET /balanceAllowance — USDC collateral balance (requires L2 auth)
  // Use asset_type=COLLATERAL to get the USDC.e cash balance available for trading.
  async getBalanceAllowance(assetType: 'COLLATERAL' | 'CONDITIONAL' = 'COLLATERAL', tokenId?: string): Promise<{ balance: string; allowance: string }> {
    if (!this.credentials) throw new Error('Authentication required');
    const timestamp = Date.now();
    const q = new URLSearchParams({ asset_type: assetType });
    if (tokenId) q.set('token_id', tokenId);
    const path = `/balance-allowance?${q}`;
    const headers = await this.generateAuthHeaders(timestamp, 'GET', path);
    const response = await fetch(`${this.clobApiBase}${path}`, { headers });
    if (!response.ok) throw new Error(`CLOB API error: ${response.status}`);
    return await response.json();
  }

  async getClosedPositions(
    userAddress: string,
    params?: {
      limit?: number;
      offset?: number;
      sortBy?: 'REALIZEDPNL' | 'TITLE' | 'PRICE' | 'AVGPRICE' | 'TIMESTAMP';
      sortDirection?: 'ASC' | 'DESC';
    }
  ): Promise<ClosedPosition[]> {
    try {
      const q = new URLSearchParams({ user: userAddress });
      if (params?.limit)         q.set('limit',         String(params.limit));
      if (params?.offset)        q.set('offset',        String(params.offset));
      if (params?.sortBy)        q.set('sortBy',        params.sortBy);
      if (params?.sortDirection) q.set('sortDirection', params.sortDirection);
      const response = await fetch(`${this.dataApiBase}/closed-positions?${q}`);
      if (!response.ok) throw new Error(`Data API error: ${response.status}`);
      return await response.json();
    } catch (error) {
      console.error('Error fetching closed positions:', error);
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
      const timestamp = Date.now();
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
      const timestamp = Date.now();
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
      const timestamp = Date.now();
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
      const timestamp = Date.now();
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
    // cashPnl already provided by the API; compute fallback from price delta if zero
    if (position.cashPnl !== 0) return position.cashPnl;
    return (position.curPrice - position.avgPrice) * position.size;
  }
}

// Export singleton instance
export const polymarketApiService = new PolymarketServiceEnhanced();
export default polymarketApiService;
