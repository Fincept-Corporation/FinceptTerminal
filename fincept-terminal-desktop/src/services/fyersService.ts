// Fyers Broker Integration Service
// Handles all Fyers API interactions: auth, trading, market data, portfolio

import { fetch } from '@tauri-apps/plugin-http';
import { fyersModel } from 'fyers-web-sdk-v3';
import { sqliteService } from './sqliteService';

// ==================== TYPE DEFINITIONS ====================

export interface FyersCredentials {
  appId: string;
  appType?: string;
  redirectUrl: string;
  accessToken: string;
}

export interface FyersProfile {
  name: string;
  image: string;
  display_name: string;
  email_id: string;
  PAN: string;
}

export interface FyersFund {
  id: number;
  title: string;
  equityAmount: number;
  commodityAmount: number;
}

export interface FyersHolding {
  costPrice: number;
  id: number;
  fyToken: string;
  symbol: string;
  isin: string;
  quantity: number;
  exchange: number;
  segment: number;
  qty_t1: number;
  remainingQuantity: number;
  collateralQuantity: number;
  remainingPledgeQuantity: number;
  pl: number;
  ltp: number;
  marketVal: number;
  holdingType: string;
}

export interface FyersHoldingsResponse {
  overall: {
    count_total: number;
    pnl_perc: number;
    total_current_value: number;
    total_investment: number;
    total_pl: number;
  };
  holdings: FyersHolding[];
}

export interface FyersPosition {
  id: string;
  symbol: string;
  fyToken: string;
  side: number;
  segment: number;
  qty: number;
  buyQty: number;
  sellQty: number;
  buyAvg: number;
  sellAvg: number;
  netQty: number;
  netAvg: number;
  ltp: number;
  pl: number;
  realized_profit: number;
  unrealized_profit: number;
  productType: string;
}

export interface PlaceOrderRequest {
  symbol: string;
  qty: number;
  type: number; // 1=Limit, 2=Market, 3=Stop, 4=Stoplimit
  side: number; // 1=Buy, -1=Sell
  productType: string; // "INTRADAY", "MARGIN", "CNC", "CO", "BO"
  limitPrice: number;
  stopPrice: number;
  disclosedQty: number;
  validity: string; // "DAY", "IOC"
  offlineOrder: boolean;
  stopLoss: number;
  takeProfit: number;
  orderTag?: string;
}

export interface FyersOrder {
  clientId: string;
  id: string;
  exchOrdId: string;
  qty: number;
  filledQty: number;
  limitPrice: number;
  type: number;
  fyToken: string;
  exchange: number;
  segment: number;
  symbol: string;
  orderDateTime: string;
  orderValidity: string;
  productType: string;
  side: number;
  status: number;
  ex_sym: string;
  description: string;
}

export interface FyersMarketStatus {
  exchange: number;
  market_type: string;
  segment: number;
  status: string;
}

export interface FyersQuote {
  n: string; // symbol
  s?: string; // status
  lp?: number; // last price
  ltp?: number; // last traded price
  ch?: number; // change
  chp?: number; // change percent
  high?: number; // high
  low?: number; // low
  open?: number; // open
  volume?: number; // volume
  v?: number | {
    ch: number;
    chp: number;
    lp: number;
    spread: number;
    ask: number;
    bid: number;
    open_price: number;
    high_price: number;
    low_price: number;
    prev_close_price: number;
    volume: number;
  };
  o?: number;
  h?: number;
  l?: number;
}

export interface FyersMarketDepth {
  totalbuyqty: number;
  totalsellqty: number;
  bids: Array<{ price: number; volume: number; ord: number }>;
  ask: Array<{ price: number; volume: number; ord: number }>;
  o: number; // open
  h: number; // high
  l: number; // low
  c: number; // close
  chp: number; // change percent
  ch: number; // change
  ltp: number; // last traded price
  v: number; // volume
  atp: number; // average traded price
  lower_ckt: number;
  upper_ckt: number;
}

export interface FyersHistoryCandle {
  timestamp: number;
  open: number;
  high: number;
  low: number;
  close: number;
  volume: number;
}

export interface SymbolMasterEntry {
  fytoken?: string;
  fyToken?: string;
  symbol: string;
  description: string;
  exchange: string;
  segment: string;
  isin?: string;
  lotsize?: number;
  lotSize?: number;
  tick_size?: number;
  tickSize?: number;
  shortName?: string;
  displayName?: string;
}

// ==================== FYERS SERVICE CLASS ====================

class FyersService {
  private fyers: any = null;
  private isInitialized = false;
  private symbolMasterCache: SymbolMasterEntry[] = [];

  /**
   * Initialize Fyers client with credentials from database
   */
  async initialize(): Promise<boolean> {
    try {
      const creds = await sqliteService.getCredentialByService('fyers');

      if (!creds || !creds.api_key || !creds.api_secret || !creds.password) {
        return false;
      }

      const additionalData = creds.additional_data ? JSON.parse(creds.additional_data) : {};
      const appType = additionalData.appType || '100';

      const fullAccessToken = `${creds.api_key}-${appType}:${creds.password}`;

      this.fyers = new fyersModel({
        AccessToken: fullAccessToken,
        AppID: creds.api_key,
        RedirectURL: creds.api_secret,
        enableLogging: false
      });

      this.isInitialized = true;
      return true;
    } catch (error) {
      console.error('[Fyers] ✗ Initialization error:', error);
      this.isInitialized = false;
      return false;
    }
  }

  /**
   * Save Fyers credentials to database
   */
  async saveCredentials(credentials: FyersCredentials): Promise<void> {

    await sqliteService.saveCredential({
      service_name: 'fyers',
      username: 'fyers_user',
      api_key: credentials.appId,
      api_secret: credentials.redirectUrl,
      password: credentials.accessToken,
      additional_data: JSON.stringify({
        appType: credentials.appType || '100',
        connected_at: new Date().toISOString()
      })
    });

    await this.initialize();
  }

  /**
   * Check if service is ready
   */
  isReady(): boolean {
    return this.isInitialized && this.fyers !== null;
  }

  // ==================== PROFILE & FUNDS ====================

  /**
   * Get user profile
   */
  async getProfile(): Promise<FyersProfile> {
    if (!this.isReady()) throw new Error('Fyers service not initialized');

    const response = await this.fyers.get_profile();

    if (response.s !== 'ok') {
      throw new Error(response.message || 'Failed to fetch profile');
    }

    return response.data;
  }

  /**
   * Get funds/limits
   */
  async getFunds(): Promise<FyersFund[]> {
    if (!this.isReady()) throw new Error('Fyers service not initialized');

    const response = await this.fyers.get_funds();

    if (response.s !== 'ok') {
      throw new Error(response.message || 'Failed to fetch funds');
    }

    return response.fund_limit;
  }

  // ==================== HOLDINGS & POSITIONS ====================

  /**
   * Get holdings
   */
  async getHoldings(): Promise<FyersHoldingsResponse> {
    if (!this.isReady()) throw new Error('Fyers service not initialized');

    const response = await this.fyers.get_holdings();

    if (response.s !== 'ok') {
      throw new Error(response.message || 'Failed to fetch holdings');
    }

    return {
      overall: response.overall,
      holdings: response.holdings
    };
  }

  /**
   * Get positions
   */
  async getPositions(): Promise<FyersPosition[]> {
    if (!this.isReady()) throw new Error('Fyers service not initialized');

    const response = await this.fyers.get_positions();

    if (response.s !== 'ok') {
      throw new Error(response.message || 'Failed to fetch positions');
    }

    return response.netPositions || [];
  }

  // ==================== ORDERS ====================

  /**
   * Place an order
   */
  async placeOrder(orderRequest: PlaceOrderRequest): Promise<{ orderId: string; message: string }> {
    if (!this.isReady()) throw new Error('Fyers service not initialized');

    const response = await this.fyers.place_order(orderRequest);

    if (response.s !== 'ok') {
      throw new Error(response.message || 'Failed to place order');
    }

    return {
      orderId: response.id || '',
      message: response.message || 'Order placed successfully'
    };
  }

  /**
   * Get order book
   */
  async getOrders(): Promise<FyersOrder[]> {
    if (!this.isReady()) throw new Error('Fyers service not initialized');

    const response = await this.fyers.get_orders();

    if (response.s !== 'ok') {
      throw new Error(response.message || 'Failed to fetch orders');
    }

    return response.orderBook || [];
  }

  /**
   * Cancel an order
   */
  async cancelOrder(orderId: string): Promise<void> {
    if (!this.isReady()) throw new Error('Fyers service not initialized');

    const response = await this.fyers.cancel_order({ id: orderId });

    if (response.s !== 'ok') {
      throw new Error(response.message || 'Failed to cancel order');
    }
  }

  // ==================== MARKET DATA ====================

  /**
   * Get market status
   */
  async getMarketStatus(): Promise<FyersMarketStatus[]> {
    if (!this.isReady()) throw new Error('Fyers service not initialized');

    const response = await this.fyers.market_status();

    if (response.s !== 'ok') {
      throw new Error(response.message || 'Failed to fetch market status');
    }

    return response.marketStatus;
  }

  /**
   * Place GTT order
   */
  async placeGTTOrder(gttPayload: any): Promise<any> {
    if (!this.isReady()) throw new Error('Fyers service not initialized');

    const response = await this.fyers.place_gtt_order(gttPayload);

    if (response.s !== 'ok') {
      throw new Error(response.message || 'GTT Order placement failed');
    }

    return response;
  }

  /**
   * Get quotes for symbols
   */
  async getQuotes(symbols: string[]): Promise<Record<string, FyersQuote>> {
    if (!this.isReady()) throw new Error('Fyers service not initialized');

    const response = await this.fyers.getQuotes(symbols);

    if (response.s !== 'ok') {
      console.error('[Fyers] Quote API error:', response);
      throw new Error(response.message || 'Failed to fetch quotes');
    }

    const dataArray = response.d || response.data || [];

    if (!Array.isArray(dataArray) || dataArray.length === 0) {
      console.error('[Fyers] Invalid or empty quote data');
      return {};
    }

    const quotesMap: Record<string, FyersQuote> = {};
    dataArray.forEach((quote: any) => {
      const symbol = quote.n || quote.symbol;
      if (!symbol) return;

      // Data is nested inside 'v' object
      const data = quote.v || quote;

      quotesMap[symbol] = {
        n: symbol,
        lp: data.lp || data.last_price || 0,
        ltp: data.lp || data.ltp || data.last_price || 0,
        ch: data.ch || data.change || 0,
        chp: data.chp || data.change_percent || 0,
        high: data.high_price || data.h || data.high || 0,
        low: data.low_price || data.l || data.low || 0,
        open: data.open_price || data.o || data.open || 0,
        volume: data.volume || data.v || 0,
        v: data.volume || data.v || 0,
        o: data.open_price || data.o || data.open || 0,
        h: data.high_price || data.h || data.high || 0,
        l: data.low_price || data.l || data.low || 0,
        ...data
      };
    });

    return quotesMap;
  }

  /**
   * Get market depth
   */
  async getMarketDepth(symbols: string[], includeOHLCV: boolean = true): Promise<Record<string, FyersMarketDepth>> {
    if (!this.isReady()) throw new Error('Fyers service not initialized');

    const response = await this.fyers.getMarketDepth({
      symbol: symbols,
      ohlcv_flag: includeOHLCV ? 1 : 0
    });

    if (response.s !== 'ok') {
      throw new Error(response.message || 'Failed to fetch market depth');
    }

    return response.d;
  }

  /**
   * Get historical data
   */
  async getHistory(params: {
    symbol: string;
    resolution: string;
    date_format: string;
    range_from: string;
    range_to: string;
    cont_flag: string;
  }): Promise<any> {
    if (!this.isReady()) throw new Error('Fyers service not initialized');

    const response = await this.fyers.getHistory(params);

    if (response.s !== 'ok') {
      throw new Error(response.message || 'Failed to fetch history');
    }

    return response;
  }

  /**
   * Get option chain
   */
  async getOptionChain(symbol: string, strikeCount: number = 5, timestamp: string = ""): Promise<any> {
    if (!this.isReady()) throw new Error('Fyers service not initialized');

    const response = await this.fyers.getOptionChain({
      symbol,
      strikecount: strikeCount,
      timestamp
    });

    if (response.code !== 200) {
      throw new Error(response.message || 'Failed to fetch option chain');
    }

    return response.data;
  }

  // ==================== MARKET HOURS ====================

  /**
   * Check if market is currently open (NSE/BSE hours)
   * NSE: 9:15 AM - 3:30 PM IST, Monday-Friday
   */
  isMarketOpen(): boolean {
    const now = new Date();

    // Convert to IST (UTC+5:30)
    const istOffset = 5.5 * 60 * 60 * 1000;
    const istTime = new Date(now.getTime() + istOffset);

    const day = istTime.getUTCDay(); // 0 = Sunday, 6 = Saturday
    const hour = istTime.getUTCHours();
    const minute = istTime.getUTCMinutes();
    const timeInMinutes = hour * 60 + minute;

    // Weekend check
    if (day === 0 || day === 6) {
      return false;
    }

    // Market hours: 9:15 AM (555 min) to 3:30 PM (930 min)
    const marketOpen = 9 * 60 + 15;
    const marketClose = 15 * 60 + 30;

    return timeInMinutes >= marketOpen && timeInMinutes <= marketClose;
  }

  // ==================== SYMBOL SEARCH ====================

  /**
   * Load symbol master from Fyers API
   */
  async loadSymbolMaster(): Promise<void> {
    try {
      const response = await fetch('https://public.fyers.in/sym_details/NSE_CM_sym_master.json');
      const data = await response.json();

      // The format is an object with symbol keys, not an array
      if (data && typeof data === 'object' && !Array.isArray(data)) {
        // Convert object to array of entries with symbol as key
        this.symbolMasterCache = Object.entries(data).map(([symbol, details]: [string, any]) => ({
          symbol: symbol,
          fytoken: details.fyToken || details.fytoken,
          fyToken: details.fyToken || details.fytoken,
          description: details.symbolDesc || details.symDetails || '',
          exchange: details.exchangeName || 'NSE',
          segment: details.exSeries || '',
          lotsize: details.minLotSize || 1,
          lotSize: details.minLotSize || 1,
          tick_size: details.tickSize || 0.05,
          tickSize: details.tickSize || 0.05,
          shortName: details.short_name || details.exSymbol || '',
          displayName: details.display_format_mob || details.exSymName || ''
        }));
      } else if (Array.isArray(data)) {
        this.symbolMasterCache = data;
      } else {
        this.symbolMasterCache = [];
      }
    } catch (error) {
      console.error('[Fyers] ✗ Failed to load symbol master:', error);
      this.symbolMasterCache = []; // Ensure it's always an array
    }
  }

  /**
   * Search symbols
   */
  searchSymbols(query: string, limit: number = 20): SymbolMasterEntry[] {
    if (!Array.isArray(this.symbolMasterCache) || this.symbolMasterCache.length === 0) {
      console.warn('[Fyers] Symbol master not loaded or invalid');
      return [];
    }

    if (!query || query.trim().length === 0) {
      return [];
    }

    const queryLower = query.toLowerCase().trim();
    try {
      return this.symbolMasterCache
        .filter(entry => {
          if (!entry) return false;
          const symbol = entry.symbol?.toLowerCase() || '';
          const desc = entry.description?.toLowerCase() || '';
          const shortName = entry.shortName?.toLowerCase() || '';
          const displayName = entry.displayName?.toLowerCase() || '';

          return symbol.includes(queryLower) ||
                 desc.includes(queryLower) ||
                 shortName.includes(queryLower) ||
                 displayName.includes(queryLower);
        })
        .slice(0, limit);
    } catch (error) {
      console.error('[Fyers] Error searching symbols:', error);
      return [];
    }
  }

  /**
   * Get symbol by exact match
   */
  getSymbol(symbol: string): SymbolMasterEntry | undefined {
    return this.symbolMasterCache.find(entry => entry.symbol === symbol);
  }

  // ==================== MARGIN CALCULATOR ====================

  /**
   * Calculate margin for a single order (span_margin endpoint)
   */
  async calculateSpanMargin(order: {
    symbol: string;
    qty: number;
    side: 'BUY' | 'SELL';
    type: 'MARKET' | 'LIMIT' | 'STOP';
    productType: 'CNC' | 'INTRADAY' | 'MARGIN';
    limitPrice?: number;
    stopLoss?: number;
    stopPrice?: number;
    takeProfit?: number;
  }): Promise<any> {
    if (!this.isReady()) throw new Error('Fyers service not initialized');

    const payload = {
      symbol: order.symbol,
      qty: order.qty,
      side: order.side === 'BUY' ? 1 : -1,
      type: order.type === 'MARKET' ? 2 : order.type === 'LIMIT' ? 1 : 3,
      productType: order.productType,
      limitPrice: order.limitPrice || 0,
      stopLoss: order.stopLoss || 0,
      stopPrice: order.stopPrice || 0,
      takeProfit: order.takeProfit || 0
    };

    const response = await this.fyers.span_margin(payload);

    if (response.s !== 'ok') {
      throw new Error(response.message || 'Failed to calculate margin');
    }

    return response;
  }

  /**
   * Calculate margin for multiple orders (multiorder/margin endpoint)
   */
  async calculateMultiOrderMargin(orders: Array<{
    symbol: string;
    qty: number;
    side: 'BUY' | 'SELL';
    type: 'MARKET' | 'LIMIT' | 'STOP';
    productType: 'CNC' | 'INTRADAY' | 'MARGIN';
    limitPrice?: number;
    stopLoss?: number;
    stopPrice?: number;
    takeProfit?: number;
  }>): Promise<any> {
    if (!this.isReady()) throw new Error('Fyers service not initialized');

    const payload = orders.map(order => ({
      symbol: order.symbol,
      qty: order.qty,
      side: order.side === 'BUY' ? 1 : -1,
      type: order.type === 'MARKET' ? 2 : order.type === 'LIMIT' ? 1 : 3,
      productType: order.productType,
      limitPrice: order.limitPrice || 0,
      stopLoss: order.stopLoss || 0,
      stopPrice: order.stopPrice || 0,
      takeProfit: order.takeProfit || 0
    }));

    const response = await this.fyers.multiorder_margin(payload);

    if (response.s !== 'ok') {
      throw new Error(response.message || 'Failed to calculate multi-order margin');
    }

    return response;
  }
}

// Singleton instance
export const fyersService = new FyersService();
