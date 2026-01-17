/**
 * Zerodha (Kite) Broker Adapter
 * Implements the IIndianBrokerAdapter interface for Zerodha's Kite Connect API
 */

import { invoke } from '@tauri-apps/api/core';
import { BaseIndianBrokerAdapter } from '../BaseIndianBrokerAdapter';
import type {
  IndianBrokerMetadata,
  AuthResponse,
  AuthCallbackParams,
  OrderParams,
  OrderResponse,
  ModifyOrderParams,
  Order,
  Position,
  Holding,
  FundsData,
  Quote,
  OHLCV,
  MarketDepth,
  TimeFrame,
  IndianExchange,
  WebSocketConfig,
  SubscriptionMode,
  Trade,
  SmartOrderParams,
  OpenPositionResult,
  BulkOperationResult,
  MarginOrderParams,
  MarginCalculation,
  ProductType,
} from '../types';
import {
  ZERODHA_METADATA,
  ZERODHA_API_BASE_URL,
  ZERODHA_LOGIN_URL,
  ZERODHA_ENDPOINTS,
  ZERODHA_EXCHANGE_MAP,
} from './constants';
import {
  toZerodhaOrderParams,
  toZerodhaModifyParams,
  toZerodhaInterval,
  fromZerodhaOrder,
  fromZerodhaPosition,
  fromZerodhaHolding,
  fromZerodhaFunds,
  fromZerodhaQuote,
  fromZerodhaOHLCV,
  fromZerodhaDepth,
  fromZerodhaInstrument,
  fromZerodhaTick,
  fromZerodhaTrade,
  toZerodhaWSMode,
  toZerodhaMarginParams,
} from './mapper';

/**
 * Zerodha broker adapter implementation
 */
export class ZerodhaAdapter extends BaseIndianBrokerAdapter {
  readonly brokerId = 'zerodha';
  readonly brokerName = 'Zerodha';
  readonly metadata: IndianBrokerMetadata = ZERODHA_METADATA;

  private apiKey: string | null = null;
  private apiSecret: string | null = null;

  // Token to symbol mapping for WebSocket
  private tokenSymbolMap: Map<number, { symbol: string; exchange: IndianExchange }> = new Map();

  // ============================================================================
  // Authentication Methods
  // ============================================================================

  getAuthUrl(): string {
    if (!this.apiKey) {
      throw new Error('API key not set. Please set API key before getting auth URL.');
    }
    return `${ZERODHA_LOGIN_URL}?api_key=${this.apiKey}&v=3`;
  }

  async handleAuthCallback(params: AuthCallbackParams): Promise<AuthResponse> {
    try {
      if (!params.requestToken) {
        return {
          success: false,
          message: 'Request token not provided',
          errorCode: 'AUTH001',
        };
      }

      if (!this.apiKey || !this.apiSecret) {
        return {
          success: false,
          message: 'API key and secret must be set before handling callback',
          errorCode: 'AUTH001',
        };
      }

      // Exchange request token for access token via Tauri backend
      const response = await invoke<{
        success: boolean;
        access_token?: string;
        user_id?: string;
        error?: string;
      }>('zerodha_exchange_token', {
        apiKey: this.apiKey,
        apiSecret: this.apiSecret,
        requestToken: params.requestToken,
      });

      if (!response.success || !response.access_token) {
        return {
          success: false,
          message: response.error || 'Failed to exchange token',
          errorCode: 'AUTH001',
        };
      }

      // Store the access token
      this.accessToken = response.access_token;
      this.userId = response.user_id || null;
      this._isConnected = true;

      // Store credentials in database
      await this.storeCredentials({
        brokerId: this.brokerId,
        apiKey: this.apiKey,
        apiSecret: this.apiSecret,
        accessToken: this.accessToken,
        userId: this.userId || undefined,
      });

      return {
        success: true,
        accessToken: this.accessToken,
        userId: this.userId || undefined,
        message: 'Authentication successful',
      };
    } catch (error) {
      return this.handleAuthError(error);
    }
  }

  /**
   * Set API credentials
   */
  setCredentials(apiKey: string, apiSecret: string): void {
    this.apiKey = apiKey;
    this.apiSecret = apiSecret;
  }

  /**
   * Initialize from stored credentials
   */
  async initFromStorage(): Promise<boolean> {
    try {
      const credentials = await this.loadCredentials();
      if (!credentials) return false;

      this.apiKey = credentials.apiKey;
      this.apiSecret = credentials.apiSecret || null;
      this.accessToken = credentials.accessToken || null;
      this.userId = credentials.userId || null;

      if (this.accessToken) {
        // Validate the token
        const isValid = await this.validateToken(this.accessToken);
        this._isConnected = isValid;
        return isValid;
      }

      return false;
    } catch {
      return false;
    }
  }

  protected override async validateToken(token: string): Promise<boolean> {
    try {
      const response = await invoke<{ success: boolean }>('zerodha_validate_token', {
        apiKey: this.apiKey,
        accessToken: token,
      });
      return response.success;
    } catch {
      return false;
    }
  }

  // ============================================================================
  // Order Methods
  // ============================================================================

  protected async placeOrderInternal(params: OrderParams): Promise<OrderResponse> {
    const zerodhaParams = toZerodhaOrderParams(params);

    const response = await invoke<{
      success: boolean;
      order_id?: string;
      error?: string;
    }>('zerodha_place_order', {
      apiKey: this.apiKey,
      accessToken: this.accessToken,
      params: zerodhaParams,
      variety: params.amo ? 'amo' : 'regular',
    });

    return {
      success: response.success,
      orderId: response.order_id,
      message: response.error,
    };
  }

  protected async modifyOrderInternal(params: ModifyOrderParams): Promise<OrderResponse> {
    const zerodhaParams = toZerodhaModifyParams(params);

    const response = await invoke<{
      success: boolean;
      order_id?: string;
      error?: string;
    }>('zerodha_modify_order', {
      apiKey: this.apiKey,
      accessToken: this.accessToken,
      orderId: params.orderId,
      params: zerodhaParams,
    });

    return {
      success: response.success,
      orderId: response.order_id,
      message: response.error,
    };
  }

  protected async cancelOrderInternal(orderId: string): Promise<OrderResponse> {
    const response = await invoke<{
      success: boolean;
      order_id?: string;
      error?: string;
    }>('zerodha_cancel_order', {
      apiKey: this.apiKey,
      accessToken: this.accessToken,
      orderId,
    });

    return {
      success: response.success,
      orderId: response.order_id,
      message: response.error,
    };
  }

  protected async getOrdersInternal(): Promise<Order[]> {
    const response = await invoke<{
      success: boolean;
      data?: Record<string, unknown>[];
      error?: string;
    }>('zerodha_get_orders', {
      apiKey: this.apiKey,
      accessToken: this.accessToken,
    });

    if (!response.success || !response.data) {
      return [];
    }

    return response.data.map(fromZerodhaOrder);
  }

  // ============================================================================
  // Position & Holdings Methods
  // ============================================================================

  protected async getPositionsInternal(): Promise<Position[]> {
    const response = await invoke<{
      success: boolean;
      data?: {
        net?: Record<string, unknown>[];
        day?: Record<string, unknown>[];
      };
      error?: string;
    }>('zerodha_get_positions', {
      apiKey: this.apiKey,
      accessToken: this.accessToken,
    });

    if (!response.success || !response.data) {
      return [];
    }

    // Combine net and day positions
    const positions = response.data.net || [];
    return positions.map(fromZerodhaPosition);
  }

  protected async getHoldingsInternal(): Promise<Holding[]> {
    const response = await invoke<{
      success: boolean;
      data?: Record<string, unknown>[];
      error?: string;
    }>('zerodha_get_holdings', {
      apiKey: this.apiKey,
      accessToken: this.accessToken,
    });

    if (!response.success || !response.data) {
      return [];
    }

    return response.data.map(fromZerodhaHolding);
  }

  protected async getFundsInternal(): Promise<FundsData> {
    const response = await invoke<{
      success: boolean;
      data?: Record<string, unknown>;
      error?: string;
    }>('zerodha_get_margins', {
      apiKey: this.apiKey,
      accessToken: this.accessToken,
    });

    if (!response.success || !response.data) {
      return {
        availableCash: 0,
        usedMargin: 0,
        availableMargin: 0,
        collateral: 0,
        unrealizedMtm: 0,
        realizedMtm: 0,
      };
    }

    return fromZerodhaFunds(response.data);
  }

  // ============================================================================
  // Market Data Methods
  // ============================================================================

  protected async getQuoteInternal(
    symbol: string,
    exchange: IndianExchange
  ): Promise<Quote> {
    const instrumentKey = `${ZERODHA_EXCHANGE_MAP[exchange] || exchange}:${symbol}`;

    const response = await invoke<{
      success: boolean;
      data?: Record<string, Record<string, unknown>>;
      error?: string;
    }>('zerodha_get_quote', {
      apiKey: this.apiKey,
      accessToken: this.accessToken,
      instruments: [instrumentKey],
    });

    if (!response.success || !response.data) {
      throw new Error('Failed to fetch quote');
    }

    const quoteData = response.data[instrumentKey];
    if (!quoteData) {
      throw new Error(`Quote not found for ${symbol}`);
    }

    return fromZerodhaQuote(quoteData, symbol, exchange);
  }

  protected async getOHLCVInternal(
    symbol: string,
    exchange: IndianExchange,
    timeframe: TimeFrame,
    from: Date,
    to: Date
  ): Promise<OHLCV[]> {
    // Get instrument token first
    const symbolToken = await this.getSymbolToken(symbol, exchange);
    if (!symbolToken) {
      throw new Error(`Symbol token not found for ${symbol}`);
    }

    // Extract instrument token from the combined token
    const instrumentToken = symbolToken.token.split('::::')[0];

    const response = await invoke<{
      success: boolean;
      data?: {
        candles?: unknown[][];
      };
      error?: string;
    }>('zerodha_get_historical', {
      apiKey: this.apiKey,
      accessToken: this.accessToken,
      instrumentToken,
      interval: toZerodhaInterval(timeframe),
      from: from.toISOString().split('T')[0],
      to: to.toISOString().split('T')[0],
    });

    if (!response.success || !response.data?.candles) {
      return [];
    }

    return response.data.candles.map(fromZerodhaOHLCV);
  }

  protected async getMarketDepthInternal(
    symbol: string,
    exchange: IndianExchange
  ): Promise<MarketDepth> {
    const quote = await this.getQuoteInternal(symbol, exchange);

    // Quote includes depth data - need to fetch full quote with depth
    const instrumentKey = `${ZERODHA_EXCHANGE_MAP[exchange] || exchange}:${symbol}`;

    const response = await invoke<{
      success: boolean;
      data?: Record<string, Record<string, unknown>>;
      error?: string;
    }>('zerodha_get_quote', {
      apiKey: this.apiKey,
      accessToken: this.accessToken,
      instruments: [instrumentKey],
    });

    if (!response.success || !response.data) {
      return { buy: [], sell: [] };
    }

    const quoteData = response.data[instrumentKey];
    if (!quoteData?.depth) {
      return { buy: [], sell: [] };
    }

    return fromZerodhaDepth(quoteData.depth as Record<string, unknown>);
  }

  // ============================================================================
  // WebSocket Methods
  // ============================================================================

  protected async connectWebSocketInternal(config: WebSocketConfig): Promise<void> {
    if (!this.apiKey || !this.accessToken) {
      throw new Error('API key and access token required for WebSocket');
    }

    // Connect via Tauri backend
    await invoke('zerodha_ws_connect', {
      apiKey: this.apiKey,
      accessToken: this.accessToken,
    });

    // Listen for tick events from Tauri
    // This will be handled by the Tauri event system
  }

  protected async subscribeInternal(
    symbol: string,
    exchange: IndianExchange,
    mode: SubscriptionMode
  ): Promise<void> {
    // Get instrument token
    const symbolToken = await this.getSymbolToken(symbol, exchange);
    if (!symbolToken) {
      throw new Error(`Symbol token not found for ${symbol}`);
    }

    const instrumentToken = parseInt(symbolToken.token.split('::::')[0], 10);

    // Store mapping for tick processing
    this.tokenSymbolMap.set(instrumentToken, { symbol, exchange });

    // Subscribe via Tauri backend
    await invoke('zerodha_ws_subscribe', {
      tokens: [instrumentToken],
      mode: toZerodhaWSMode(mode),
    });
  }

  protected async unsubscribeInternal(
    symbol: string,
    exchange: IndianExchange
  ): Promise<void> {
    // Get instrument token
    const symbolToken = await this.getSymbolToken(symbol, exchange);
    if (!symbolToken) return;

    const instrumentToken = parseInt(symbolToken.token.split('::::')[0], 10);

    // Remove from mapping
    this.tokenSymbolMap.delete(instrumentToken);

    // Unsubscribe via Tauri backend
    await invoke('zerodha_ws_unsubscribe', {
      tokens: [instrumentToken],
    });
  }

  /**
   * Handle incoming tick from WebSocket
   * This is called from Tauri event listener
   */
  handleTick(tick: Record<string, unknown>): void {
    const instrumentToken = Number(tick.instrument_token);
    const symbolInfo = this.tokenSymbolMap.get(instrumentToken);

    if (!symbolInfo) {
      console.warn(`[Zerodha] Unknown instrument token: ${instrumentToken}`);
      return;
    }

    // Determine mode from tick data
    let mode: SubscriptionMode = 'ltp';
    if (tick.depth) {
      mode = 'full';
    } else if (tick.ohlc || tick.volume) {
      mode = 'quote';
    }

    const tickData = fromZerodhaTick(tick, symbolInfo, mode);
    this.emitTick(tickData);
  }

  // ============================================================================
  // Master Contract Methods
  // ============================================================================

  protected async downloadMasterContractInternal(): Promise<void> {
    // Download and process instruments via Tauri backend
    await invoke('zerodha_download_master_contract', {
      apiKey: this.apiKey,
      accessToken: this.accessToken,
    });
  }

  // ============================================================================
  // Trade Book Methods
  // ============================================================================

  async getTradeBook(): Promise<Trade[]> {
    const response = await invoke<{
      success: boolean;
      data?: Record<string, unknown>[];
      error?: string;
    }>('zerodha_get_trade_book', {
      apiKey: this.apiKey,
      accessToken: this.accessToken,
    });

    if (!response.success || !response.data) {
      return [];
    }

    return response.data.map(fromZerodhaTrade);
  }

  // ============================================================================
  // Open Position Methods
  // ============================================================================

  async getOpenPosition(
    symbol: string,
    exchange: IndianExchange,
    product: ProductType
  ): Promise<OpenPositionResult> {
    const zerodhaExchange = ZERODHA_EXCHANGE_MAP[exchange] || exchange;

    const response = await invoke<{
      success: boolean;
      data?: {
        tradingsymbol: string;
        exchange: string;
        product: string;
        quantity: number;
        position?: Record<string, unknown>;
      };
      error?: string;
    }>('zerodha_get_open_position', {
      apiKey: this.apiKey,
      accessToken: this.accessToken,
      tradingsymbol: symbol,
      exchange: zerodhaExchange,
      product,
    });

    if (!response.success || !response.data) {
      return {
        symbol,
        exchange,
        product,
        quantity: 0,
      };
    }

    return {
      symbol: response.data.tradingsymbol,
      exchange,
      product,
      quantity: response.data.quantity,
      position: response.data.position ? fromZerodhaPosition(response.data.position) : undefined,
    };
  }

  // ============================================================================
  // Smart Order Methods
  // ============================================================================

  async placeSmartOrder(params: SmartOrderParams): Promise<OrderResponse> {
    const zerodhaExchange = ZERODHA_EXCHANGE_MAP[params.exchange] || params.exchange;

    // Get current position size if not provided
    let positionSize = params.positionSize ?? 0;
    if (params.positionSize === undefined) {
      const position = await this.getOpenPosition(params.symbol, params.exchange, params.product);
      positionSize = position.quantity;
    }

    const response = await invoke<{
      success: boolean;
      order_id?: string;
      error?: string;
    }>('zerodha_place_smart_order', {
      apiKey: this.apiKey,
      accessToken: this.accessToken,
      tradingsymbol: params.symbol,
      exchange: zerodhaExchange,
      product: params.product,
      action: params.transactionType,
      quantity: params.quantity,
      orderType: params.orderType,
      price: params.price,
      triggerPrice: params.triggerPrice,
      positionSize,
    });

    return {
      success: response.success,
      orderId: response.order_id,
      message: response.error,
    };
  }

  // ============================================================================
  // Bulk Operation Methods
  // ============================================================================

  async closeAllPositions(): Promise<BulkOperationResult> {
    const response = await invoke<{
      success: boolean;
      data?: Array<{
        success: boolean;
        order_id?: string;
        error?: string;
      }>;
      error?: string;
    }>('zerodha_close_all_positions', {
      apiKey: this.apiKey,
      accessToken: this.accessToken,
    });

    if (!response.success || !response.data) {
      return {
        success: false,
        totalCount: 0,
        successCount: 0,
        failedCount: 0,
        results: [],
      };
    }

    const results = response.data.map(r => ({
      success: r.success,
      orderId: r.order_id,
      error: r.error,
    }));

    return {
      success: true,
      totalCount: results.length,
      successCount: results.filter(r => r.success).length,
      failedCount: results.filter(r => !r.success).length,
      results,
    };
  }

  async cancelAllOrders(): Promise<BulkOperationResult> {
    const response = await invoke<{
      success: boolean;
      data?: Array<{
        success: boolean;
        order_id?: string;
        error?: string;
      }>;
      error?: string;
    }>('zerodha_cancel_all_orders', {
      apiKey: this.apiKey,
      accessToken: this.accessToken,
    });

    if (!response.success || !response.data) {
      return {
        success: false,
        totalCount: 0,
        successCount: 0,
        failedCount: 0,
        results: [],
      };
    }

    const results = response.data.map(r => ({
      success: r.success,
      orderId: r.order_id,
      error: r.error,
    }));

    return {
      success: true,
      totalCount: results.length,
      successCount: results.filter(r => r.success).length,
      failedCount: results.filter(r => !r.success).length,
      results,
    };
  }

  // ============================================================================
  // Margin Calculator Methods
  // ============================================================================

  async calculateMargin(orders: MarginOrderParams[]): Promise<MarginCalculation> {
    const zerodhaOrders = orders.map(toZerodhaMarginParams);

    const response = await invoke<{
      success: boolean;
      data?: Record<string, unknown>;
      error?: string;
    }>('zerodha_calculate_margin', {
      apiKey: this.apiKey,
      accessToken: this.accessToken,
      orders: zerodhaOrders,
    });

    if (!response.success || !response.data) {
      return {
        totalMargin: 0,
        initialMargin: 0,
        exposureMargin: 0,
        orders: [],
      };
    }

    const data = response.data;

    // Extract margin data from response
    const initial = Number(data.initial || data.total || 0);
    const exposure = Number(data.exposure || 0);
    const total = Number(data.total || initial + exposure);

    return {
      totalMargin: total,
      initialMargin: initial,
      exposureMargin: exposure,
      spanMargin: data.span ? Number(data.span) : undefined,
      optionPremium: data.option_premium ? Number(data.option_premium) : undefined,
      leverage: data.leverage ? Number(data.leverage) : undefined,
      orders: orders.map((o, i) => ({
        symbol: o.symbol,
        margin: Array.isArray(data.orders) ? Number((data.orders as Record<string, unknown>[])[i]?.margin || 0) : 0,
      })),
    };
  }
}

// Export singleton instance
export const zerodhaAdapter = new ZerodhaAdapter();
