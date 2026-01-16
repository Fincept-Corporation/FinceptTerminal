// File: src/services/polymarketServiceEnhanced.ts
// Complete Polymarket API service with all endpoints

const GAMMA_API_BASE = 'https://gamma-api.polymarket.com';
const CLOB_API_BASE = 'https://clob.polymarket.com';
const DATA_API_BASE = 'https://data-api.polymarket.com';
const WS_MARKET_BASE = 'wss://ws-subscriptions-clob.polymarket.com/ws/market';
const WS_USER_BASE = 'wss://ws-subscriptions-clob.polymarket.com/ws/user';
const WS_REALTIME_BASE = 'wss://ws-live-data.polymarket.com';

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
  id: string;
  label: string;
  slug: string;
  tagId: string;
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
  price: string;
  timestamp: number;
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
export interface WSMarketUpdate {
  type: 'book' | 'price_change' | 'tick_size_change' | 'last_trade_price';
  market: string;
  asset_id?: string;
  data: any;
  timestamp: number;
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
  interval?: '1m' | '1w' | '1d' | '6h' | '1h' | 'max';
  startTs?: number;
  endTs?: number;
  limit?: number;
  fidelity?: number;
}

// API Credentials (for authenticated requests)
export interface APICredentials {
  apiKey: string;
  apiSecret: string;
  apiPassphrase: string;
}

// ==================== SERVICE CLASS ====================

class PolymarketServiceEnhanced {
  private gammaApiBase: string;
  private clobApiBase: string;
  private dataApiBase: string;
  private wsMarketBase: string;
  private wsUserBase: string;
  private wsRealtimeBase: string;
  private credentials?: APICredentials;
  private wsConnections: Map<string, WebSocket>;

  constructor() {
    this.gammaApiBase = GAMMA_API_BASE;
    this.clobApiBase = CLOB_API_BASE;
    this.dataApiBase = DATA_API_BASE;
    this.wsMarketBase = WS_MARKET_BASE;
    this.wsUserBase = WS_USER_BASE;
    this.wsRealtimeBase = WS_REALTIME_BASE;
    this.wsConnections = new Map();
  }

  // ==================== AUTHENTICATION ====================

  setCredentials(credentials: APICredentials) {
    this.credentials = credentials;
  }

  clearCredentials() {
    this.credentials = undefined;
  }

  private generateAuthHeaders(timestamp: number, method: string, path: string, body?: string): HeadersInit {
    if (!this.credentials) {
      throw new Error('API credentials not set');
    }

    // HMAC-SHA256 signature generation would go here
    // For now, return basic headers
    return {
      'Content-Type': 'application/json',
      'POLY-API-KEY': this.credentials.apiKey,
      'POLY-TIMESTAMP': timestamp.toString(),
      'POLY-PASSPHRASE': this.credentials.apiPassphrase,
      // 'POLY-SIGNATURE': signature // Would compute actual signature
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
      const url = `${this.clobApiBase}/book?token_id=${tokenId}`;
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
      // API uses 'market' parameter, not 'token_id'
      queryParams.append('market', params.token_id);
      queryParams.append('interval', params.interval || 'max');

      // Add fidelity parameter (resolution in minutes)
      if (params.fidelity) {
        queryParams.append('fidelity', params.fidelity.toString());
      }

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

      // API returns {history: [...]} - map to PricePoint format
      const prices = (data.history || []).map((item: any) => ({
        price: item.p || item.price || '0', // 'p' is the price field
        timestamp: item.t || item.timestamp || 0 // 't' is the timestamp field
      }));

      console.log('Mapped prices:', prices.slice(0, 3));

      return {
        token_id: params.token_id,
        prices,
        interval: params.interval || 'max'
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
      return await response.json();
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

  // ==================== ORDER MANAGEMENT (L2) ====================
  // Note: These require authentication and proper signature generation

  async createOrder(orderRequest: OrderRequest): Promise<CreateOrderResponse> {
    if (!this.credentials) {
      throw new Error('Authentication required. Please set API credentials.');
    }

    try {
      const timestamp = Date.now();
      const url = `${this.clobApiBase}/order`;
      const headers = this.generateAuthHeaders(timestamp, 'POST', '/order', JSON.stringify(orderRequest));

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
      const headers = this.generateAuthHeaders(timestamp, 'GET', path);

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
      const headers = this.generateAuthHeaders(timestamp, 'DELETE', path);

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
      const path = marketId ? `/orders?market=${marketId}` : '/orders';
      const url = `${this.clobApiBase}${path}`;
      const headers = this.generateAuthHeaders(timestamp, 'DELETE', path);

      const response = await fetch(url, {
        method: 'DELETE',
        headers
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
    onMessage: (update: WSMarketUpdate) => void,
    onError?: (error: Event) => void
  ): WebSocket {
    const ws = new WebSocket(this.wsMarketBase);

    ws.onopen = () => {
      console.log('Market WebSocket connected');
    };

    ws.onmessage = (event) => {
      try {
        const data = JSON.parse(event.data);
        onMessage(data);
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
    };

    this.wsConnections.set('market', ws);
    return ws;
  }

  connectUserWebSocket(
    onMessage: (update: WSUserUpdate) => void,
    onError?: (error: Event) => void
  ): WebSocket {
    if (!this.credentials) {
      throw new Error('Authentication required for user WebSocket');
    }

    const ws = new WebSocket(this.wsUserBase);

    ws.onopen = () => {
      console.log('User WebSocket connected');
      // Send authentication message
      ws.send(JSON.stringify({
        type: 'auth',
        apiKey: this.credentials!.apiKey
      }));
    };

    ws.onmessage = (event) => {
      try {
        const data = JSON.parse(event.data);
        onMessage(data);
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
    };

    this.wsConnections.set('user', ws);
    return ws;
  }

  subscribeToMarket(marketId: string) {
    const ws = this.wsConnections.get('market');
    if (ws && ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify({
        type: 'subscribe',
        market: marketId
      }));
    }
  }

  unsubscribeFromMarket(marketId: string) {
    const ws = this.wsConnections.get('market');
    if (ws && ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify({
        type: 'unsubscribe',
        market: marketId
      }));
    }
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
