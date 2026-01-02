// File: src/services/polymarketService.ts
// Polymarket API service for prediction markets integration

const GAMMA_API_BASE = 'https://gamma-api.polymarket.com';
const CLOB_API_BASE = 'https://clob.polymarket.com';

// Types
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
  id: number;
  sport: string;
  image?: string;
  resolution?: string;
  ordering?: string;
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

// Query parameters
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

class PolymarketService {
  private gammaApiBase: string;
  private clobApiBase: string;

  constructor() {
    this.gammaApiBase = GAMMA_API_BASE;
    this.clobApiBase = CLOB_API_BASE;
  }

  // ============== GAMMA API - Market Data ==============

  /**
   * Fetch markets from Gamma API
   */
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

  /**
   * Fetch a specific market by slug
   */
  async getMarketBySlug(slug: string): Promise<PolymarketMarket> {
    try {
      const url = `${this.gammaApiBase}/markets/slug/${slug}`;
      const response = await fetch(url);

      if (!response.ok) {
        throw new Error(`Gamma API error: ${response.status} ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      console.error('Error fetching market by slug:', error);
      throw error;
    }
  }

  /**
   * Fetch events from Gamma API
   */
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

      if (!response.ok) {
        throw new Error(`Gamma API error: ${response.status} ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      console.error('Error fetching events:', error);
      throw error;
    }
  }

  /**
   * Fetch a specific event by slug
   */
  async getEventBySlug(slug: string): Promise<PolymarketEvent> {
    try {
      const url = `${this.gammaApiBase}/events/slug/${slug}`;
      const response = await fetch(url);

      if (!response.ok) {
        throw new Error(`Gamma API error: ${response.status} ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      console.error('Error fetching event by slug:', error);
      throw error;
    }
  }

  /**
   * Fetch all available tags
   */
  async getTags(): Promise<PolymarketTag[]> {
    try {
      const url = `${this.gammaApiBase}/tags`;
      const response = await fetch(url);

      if (!response.ok) {
        throw new Error(`Gamma API error: ${response.status} ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      console.error('Error fetching tags:', error);
      throw error;
    }
  }

  /**
   * Fetch sports metadata
   */
  async getSports(): Promise<PolymarketSport[]> {
    try {
      const url = `${this.gammaApiBase}/sports`;
      const response = await fetch(url);

      if (!response.ok) {
        throw new Error(`Gamma API error: ${response.status} ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      console.error('Error fetching sports:', error);
      throw error;
    }
  }

  /**
   * Search across markets and events
   */
  async search(query: string, type: 'markets' | 'events' | 'all' = 'all'): Promise<any> {
    try {
      const url = `${this.gammaApiBase}/search?q=${encodeURIComponent(query)}&type=${type}`;
      const response = await fetch(url);

      if (!response.ok) {
        throw new Error(`Gamma API error: ${response.status} ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      console.error('Error searching:', error);
      throw error;
    }
  }

  /**
   * Get comments for a market
   */
  async getMarketComments(marketId: string): Promise<PolymarketComment[]> {
    try {
      const url = `${this.gammaApiBase}/markets/${marketId}/comments`;
      const response = await fetch(url);

      if (!response.ok) {
        throw new Error(`Gamma API error: ${response.status} ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      console.error('Error fetching market comments:', error);
      throw error;
    }
  }

  /**
   * Get profile information by wallet address
   */
  async getProfile(walletAddress: string): Promise<PolymarketProfile> {
    try {
      const url = `${this.gammaApiBase}/profiles/${walletAddress}`;
      const response = await fetch(url);

      if (!response.ok) {
        throw new Error(`Gamma API error: ${response.status} ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      console.error('Error fetching profile:', error);
      throw error;
    }
  }

  // ============== CLOB API - Trading Data ==============

  /**
   * Get CLOB markets (tradeable markets)
   */
  async getCLOBMarkets(nextCursor?: string): Promise<any> {
    try {
      const url = `${this.clobApiBase}/markets${nextCursor ? `?next_cursor=${nextCursor}` : ''}`;
      const response = await fetch(url);

      if (!response.ok) {
        throw new Error(`CLOB API error: ${response.status} ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      console.error('Error fetching CLOB markets:', error);
      throw error;
    }
  }

  /**
   * Get order book for a specific token
   */
  async getOrderBook(tokenId: string): Promise<PolymarketOrderBook> {
    try {
      const url = `${this.clobApiBase}/book?token_id=${tokenId}`;
      const response = await fetch(url);

      if (!response.ok) {
        throw new Error(`CLOB API error: ${response.status} ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      console.error('Error fetching order book:', error);
      throw error;
    }
  }

  /**
   * Get price data for a token
   */
  async getPrice(tokenId: string): Promise<any> {
    try {
      const url = `${this.clobApiBase}/price?token_id=${tokenId}`;
      const response = await fetch(url);

      if (!response.ok) {
        throw new Error(`CLOB API error: ${response.status} ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      console.error('Error fetching price:', error);
      throw error;
    }
  }

  /**
   * Get spread data for a token
   */
  async getSpread(tokenId: string): Promise<any> {
    try {
      const url = `${this.clobApiBase}/spread?token_id=${tokenId}`;
      const response = await fetch(url);

      if (!response.ok) {
        throw new Error(`CLOB API error: ${response.status} ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      console.error('Error fetching spread:', error);
      throw error;
    }
  }

  /**
   * Get recent trades
   */
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

      if (!response.ok) {
        throw new Error(`CLOB API error: ${response.status} ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      console.error('Error fetching trades:', error);
      throw error;
    }
  }

  // ============== Utility Methods ==============

  /**
   * Calculate implied probability from price
   */
  calculateProbability(price: string | number): number {
    const priceNum = typeof price === 'string' ? parseFloat(price) : price;
    return priceNum * 100; // Price is 0-1, convert to percentage
  }

  /**
   * Calculate potential profit for a bet
   */
  calculateProfit(betAmount: number, price: number, outcome: 'yes' | 'no'): number {
    if (outcome === 'yes') {
      return betAmount * (1 / price - 1);
    } else {
      return betAmount * (1 / (1 - price) - 1);
    }
  }

  /**
   * Format market volume
   */
  formatVolume(volume: string | number): string {
    const volNum = typeof volume === 'string' ? parseFloat(volume) : volume;

    if (volNum >= 1000000) {
      return `$${(volNum / 1000000).toFixed(2)}M`;
    } else if (volNum >= 1000) {
      return `$${(volNum / 1000).toFixed(2)}K`;
    } else {
      return `$${volNum.toFixed(2)}`;
    }
  }

  /**
   * Get market status
   */
  getMarketStatus(market: PolymarketMarket): 'active' | 'closed' | 'archived' {
    if (market.archived) return 'archived';
    if (market.closed) return 'closed';
    if (market.active) return 'active';
    return 'closed';
  }

  /**
   * Check if market is tradeable
   */
  isMarketTradeable(market: PolymarketMarket): boolean {
    return market.active && !market.closed && !market.archived && market.enableOrderBook === true;
  }
}

// Export singleton instance
export const polymarketService = new PolymarketService();
export default polymarketService;
