/**
 * Saxo Bank Adapter
 *
 * Full implementation of Saxo Bank OpenAPI integration
 * Supports OAuth 2.0 authentication, trading, positions, and real-time streaming
 *
 * Documentation: https://www.developer.saxo/openapi/learn
 */

import { invoke } from '@tauri-apps/api/core';
import { BaseStockBrokerAdapter } from '../../BaseStockBrokerAdapter';
import type {
  StockBrokerMetadata,
  Region,
  BrokerCredentials,
  AuthResponse,
  Funds,
  MarginRequired,
  OrderParams,
  OrderResponse,
  ModifyOrderParams,
  Order,
  Trade,
  Position,
  Holding,
  Quote,
  OHLCV,
  MarketDepth,
  TimeFrame,
  StockExchange,
  WebSocketConfig,
  SubscriptionMode,
  BulkOperationResult,
  SmartOrderParams,
  Currency,
} from '../../types';

import type {
  SaxoAccountBalance,
  SaxoOrder,
  SaxoPosition,
  SaxoNetPosition,
  SaxoTrade,
  SaxoInfoPrice,
  SaxoChartData,
  SaxoOrderRequest,
  SaxoModifyOrderRequest,
  SaxoInstrument,
  SaxoApiResponse,
  SaxoTokenResponse,
  SaxoAssetType,
  SaxoBuySell,
  SaxoOrderDuration,
  SaxoOrderType as SaxoOrderTypeEnum,
} from './types';

import {
  SAXO_SIM_API_BASE,
  SAXO_LIVE_API_BASE,
  SAXO_SIM_STREAM_URL,
  SAXO_LIVE_STREAM_URL,
  SAXO_ENDPOINTS,
  SAXO_METADATA,
  SAXO_ORDER_TYPE_MAP,
  SAXO_SIDE_MAP,
  SAXO_VALIDITY_MAP,
  SAXO_ASSET_TYPE_MAP,
  SAXO_TIMEFRAME_MAP,
} from './constants';

import {
  mapSaxoOrderToOrder,
  mapSaxoPositionToPosition,
  mapSaxoNetPositionToPosition,
  mapSaxoPositionToHolding,
  mapSaxoTradeToTrade,
  mapSaxoInfoPriceToQuote,
  mapSaxoChartDataToOHLCV,
  mapSaxoBalanceToFunds,
  mapSaxoToMarketDepth,
  mapSaxoWSQuoteToTickData,
  parseSymbolExchange,
} from './mapper';

/**
 * Extended credentials for Saxo Bank
 */
interface SaxoCredentials extends BrokerCredentials {
  refreshToken?: string;
}

/**
 * Saxo Bank Adapter Implementation
 */
export class SaxoBankAdapter extends BaseStockBrokerAdapter {
  readonly brokerId = 'saxobank';
  readonly brokerName = 'Saxo Bank';
  readonly region: Region = 'europe';
  readonly metadata: StockBrokerMetadata = SAXO_METADATA;

  // Saxo-specific state
  private clientKey: string | null = null;
  private accountKey: string | null = null;
  private refreshToken: string | null = null;
  private tokenExpiresAt: number = 0;
  private isPaperTrading: boolean = true; // Default to simulation

  // WebSocket state
  private ws: WebSocket | null = null;
  private contextId: string = '';
  private referenceId: number = 0;
  private uicToSymbolMap: Map<number, { symbol: string; exchange: string }> = new Map();

  // ============================================================================
  // AUTHENTICATION
  // ============================================================================

  async authenticate(credentials: BrokerCredentials): Promise<AuthResponse> {
    try {
      console.log(`[${this.brokerId}] Starting authentication...`);
      const saxoCredentials = credentials as SaxoCredentials;

      // If we have an access token directly (from OAuth callback)
      if (credentials.accessToken) {
        this.accessToken = credentials.accessToken;
        this.refreshToken = saxoCredentials.refreshToken || null;
        this.tokenExpiresAt = Date.now() + 3600000; // Default 1 hour

        // Fetch client and account info
        const clientInfo = await this.fetchClientInfo();
        if (!clientInfo.success) {
          return clientInfo;
        }

        this._isConnected = true;
        this.credentials = credentials;

        // Store credentials
        await this.storeCredentials({
          ...credentials,
          accessToken: this.accessToken,
        });

        console.log(`[${this.brokerId}] ✓ Authentication successful`);
        return {
          success: true,
          message: 'Successfully connected to Saxo Bank',
          userId: this.clientKey || undefined,
          accessToken: this.accessToken,
        };
      }

      // If we have stored credentials, try to use refresh token
      const stored = await this.loadCredentials();
      if (stored?.accessToken) {
        this.accessToken = stored.accessToken;
        this.refreshToken = (stored as SaxoCredentials).refreshToken || null;

        // Verify token is still valid
        try {
          const clientInfo = await this.fetchClientInfo();
          if (clientInfo.success) {
            this._isConnected = true;
            this.credentials = stored;
            return {
              success: true,
              message: 'Restored Saxo Bank session',
              userId: this.clientKey || undefined,
              accessToken: this.accessToken,
            };
          }
        } catch {
          // Token expired, try refresh
          if (this.refreshToken) {
            const refreshResult = await this.refreshAccessToken();
            if (refreshResult.success) {
              return refreshResult;
            }
          }
        }
      }

      // No valid credentials - need OAuth flow
      // Return information about OAuth requirement
      return {
        success: false,
        message: 'OAuth authentication required. Use getOAuthUrl() to get the authorization URL.',
      };
    } catch (error) {
      console.error(`[${this.brokerId}] Authentication error:`, error);
      return {
        success: false,
        message: error instanceof Error ? error.message : 'Authentication failed',
      };
    }
  }

  getOAuthUrl(clientId: string): string {
    const baseUrl = this.isPaperTrading
      ? 'https://sim.logonvalidation.net/authorize'
      : 'https://live.logonvalidation.net/authorize';

    const params = new URLSearchParams({
      response_type: 'code',
      client_id: clientId,
      redirect_uri: 'fincept://oauth/saxobank',
      state: Math.random().toString(36).substring(7),
    });

    return `${baseUrl}?${params.toString()}`;
  }

  async exchangeCodeForToken(
    code: string,
    clientId: string,
    clientSecret: string,
    redirectUri: string
  ): Promise<AuthResponse> {
    try {
      const tokenEndpoint = this.isPaperTrading
        ? 'https://sim.logonvalidation.net/token'
        : 'https://live.logonvalidation.net/token';

      const response = await invoke<SaxoTokenResponse>('saxo_exchange_token', {
        tokenEndpoint,
        code,
        clientId,
        clientSecret,
        redirectUri,
      });

      this.accessToken = response.access_token;
      this.refreshToken = response.refresh_token;
      this.tokenExpiresAt = Date.now() + (response.expires_in * 1000);

      // Fetch client info
      const clientInfo = await this.fetchClientInfo();
      if (!clientInfo.success) {
        return clientInfo;
      }

      this._isConnected = true;
      this.credentials = {
        apiKey: clientId,
        apiSecret: clientSecret,
        accessToken: this.accessToken,
      };

      await this.storeCredentials(this.credentials);

      return {
        success: true,
        message: 'Successfully connected to Saxo Bank',
        userId: this.clientKey || undefined,
        accessToken: this.accessToken,
      };
    } catch (error) {
      console.error(`[${this.brokerId}] Token exchange error:`, error);
      return {
        success: false,
        message: error instanceof Error ? error.message : 'Token exchange failed',
      };
    }
  }

  private async refreshAccessToken(): Promise<AuthResponse> {
    if (!this.refreshToken || !this.credentials?.apiKey || !this.credentials?.apiSecret) {
      return { success: false, message: 'No refresh token available' };
    }

    try {
      const tokenEndpoint = this.isPaperTrading
        ? 'https://sim.logonvalidation.net/token'
        : 'https://live.logonvalidation.net/token';

      const response = await invoke<SaxoTokenResponse>('saxo_refresh_token', {
        tokenEndpoint,
        refreshToken: this.refreshToken,
        clientId: this.credentials.apiKey,
        clientSecret: this.credentials.apiSecret,
      });

      this.accessToken = response.access_token;
      this.refreshToken = response.refresh_token;
      this.tokenExpiresAt = Date.now() + (response.expires_in * 1000);

      const clientInfo = await this.fetchClientInfo();
      if (clientInfo.success) {
        this._isConnected = true;
        return {
          success: true,
          message: 'Token refreshed successfully',
          accessToken: this.accessToken,
        };
      }

      return clientInfo;
    } catch (error) {
      console.error(`[${this.brokerId}] Token refresh error:`, error);
      return {
        success: false,
        message: error instanceof Error ? error.message : 'Token refresh failed',
      };
    }
  }

  private async fetchClientInfo(): Promise<AuthResponse> {
    try {
      const client = await this.apiRequest<{ ClientKey: string; DefaultAccountKey: string }>(
        SAXO_ENDPOINTS.PORT_CLIENT
      );

      this.clientKey = client.ClientKey;
      this.accountKey = client.DefaultAccountKey;

      return { success: true, message: 'Client info fetched' };
    } catch (error) {
      console.error(`[${this.brokerId}] Fetch client info error:`, error);
      return {
        success: false,
        message: error instanceof Error ? error.message : 'Failed to fetch client info',
      };
    }
  }

  // ============================================================================
  // API REQUEST HELPER
  // ============================================================================

  private getBaseUrl(): string {
    return this.isPaperTrading ? SAXO_SIM_API_BASE : SAXO_LIVE_API_BASE;
  }

  private async apiRequest<T>(
    endpoint: string,
    options: {
      method?: string;
      body?: unknown;
      params?: Record<string, string | number | boolean>;
    } = {}
  ): Promise<T> {
    const { method = 'GET', body, params } = options;

    let url = `${this.getBaseUrl()}${endpoint}`;

    if (params) {
      const queryParams = new URLSearchParams();
      for (const [key, value] of Object.entries(params)) {
        queryParams.append(key, String(value));
      }
      url += `?${queryParams.toString()}`;
    }

    const response = await invoke<T>('saxo_api_request', {
      url,
      method,
      accessToken: this.accessToken,
      body: body ? JSON.stringify(body) : null,
    });

    return response;
  }

  // ============================================================================
  // ORDERS
  // ============================================================================

  protected async placeOrderInternal(params: OrderParams): Promise<OrderResponse> {
    try {
      const { symbol, exchange } = parseSymbolExchange(params.symbol);

      // Get instrument UIC
      const uic = await this.getUicForSymbol(symbol, exchange || params.exchange);
      if (!uic) {
        return this.createErrorResponse(`Instrument not found: ${params.symbol}`);
      }

      const orderRequest: SaxoOrderRequest = {
        AccountKey: this.accountKey!,
        Amount: params.quantity,
        AssetType: this.mapProductTypeToAssetType(params.productType),
        BuySell: SAXO_SIDE_MAP[params.side] as SaxoBuySell,
        ManualOrder: true,
        OrderDuration: {
          DurationType: (SAXO_VALIDITY_MAP[params.validity || 'DAY'] || 'DayOrder') as SaxoOrderDuration,
        },
        OrderType: (SAXO_ORDER_TYPE_MAP[params.orderType] || 'Market') as SaxoOrderTypeEnum,
        Uic: uic,
      };

      // Add price for limit/stop orders
      if (params.price && params.orderType !== 'MARKET') {
        orderRequest.OrderPrice = params.price;
      }

      // Add stop-limit price
      if (params.triggerPrice && params.orderType === 'STOP_LIMIT') {
        orderRequest.StopLimitPrice = params.triggerPrice;
      }

      const response = await this.apiRequest<{ OrderId: string }>(
        SAXO_ENDPOINTS.TRADE_ORDERS,
        { method: 'POST', body: orderRequest }
      );

      return {
        success: true,
        message: 'Order placed successfully',
        orderId: response.OrderId,
      };
    } catch (error) {
      return this.createErrorResponse(
        error instanceof Error ? error.message : 'Order placement failed'
      );
    }
  }

  protected async modifyOrderInternal(params: ModifyOrderParams): Promise<OrderResponse> {
    try {
      const endpoint = SAXO_ENDPOINTS.TRADE_ORDER.replace(':orderId', params.orderId);

      // First, get the existing order
      const existingOrder = await this.apiRequest<SaxoOrder>(endpoint);

      const modifyRequest: SaxoModifyOrderRequest = {
        AccountKey: this.accountKey!,
        AssetType: existingOrder.AssetType,
      };

      if (params.quantity !== undefined) {
        modifyRequest.Amount = params.quantity;
      }

      if (params.price !== undefined) {
        modifyRequest.OrderPrice = params.price;
      }

      if (params.validity !== undefined) {
        modifyRequest.OrderDuration = {
          DurationType: (SAXO_VALIDITY_MAP[params.validity] || 'DayOrder') as SaxoOrderDuration,
        };
      }

      await this.apiRequest(endpoint, { method: 'PATCH', body: modifyRequest });

      return {
        success: true,
        message: 'Order modified successfully',
        orderId: params.orderId,
      };
    } catch (error) {
      return this.createErrorResponse(
        error instanceof Error ? error.message : 'Order modification failed'
      );
    }
  }

  protected async cancelOrderInternal(orderId: string): Promise<OrderResponse> {
    try {
      const endpoint = SAXO_ENDPOINTS.TRADE_ORDER.replace(':orderId', orderId);

      await this.apiRequest(endpoint, {
        method: 'DELETE',
        params: { AccountKey: this.accountKey! },
      });

      return {
        success: true,
        message: 'Order cancelled successfully',
        orderId,
      };
    } catch (error) {
      return this.createErrorResponse(
        error instanceof Error ? error.message : 'Order cancellation failed'
      );
    }
  }

  protected async getOrdersInternal(): Promise<Order[]> {
    try {
      const response = await this.apiRequest<SaxoApiResponse<SaxoOrder>>(
        SAXO_ENDPOINTS.PORT_ORDERS,
        { params: { ClientKey: this.clientKey! } }
      );

      return (response.Data || []).map(mapSaxoOrderToOrder);
    } catch (error) {
      console.error(`[${this.brokerId}] Get orders error:`, error);
      return [];
    }
  }

  protected async getTradeBookInternal(): Promise<Trade[]> {
    try {
      // Saxo uses closed positions for trade history
      const response = await this.apiRequest<SaxoApiResponse<SaxoTrade>>(
        SAXO_ENDPOINTS.PORT_CLOSED_POSITIONS,
        { params: { ClientKey: this.clientKey! } }
      );

      return (response.Data || []).map(mapSaxoTradeToTrade);
    } catch (error) {
      console.error(`[${this.brokerId}] Get trades error:`, error);
      return [];
    }
  }

  // ============================================================================
  // POSITIONS & HOLDINGS
  // ============================================================================

  protected async getPositionsInternal(): Promise<Position[]> {
    try {
      const response = await this.apiRequest<SaxoApiResponse<SaxoNetPosition>>(
        SAXO_ENDPOINTS.PORT_NET_POSITIONS,
        {
          params: {
            ClientKey: this.clientKey!,
            FieldGroups: 'NetPositionBase,NetPositionView,DisplayAndFormat,ExchangeInfo',
          },
        }
      );

      return (response.Data || []).map(mapSaxoNetPositionToPosition);
    } catch (error) {
      console.error(`[${this.brokerId}] Get positions error:`, error);
      return [];
    }
  }

  protected async getHoldingsInternal(): Promise<Holding[]> {
    try {
      const response = await this.apiRequest<SaxoApiResponse<SaxoPosition>>(
        SAXO_ENDPOINTS.PORT_POSITIONS,
        {
          params: {
            ClientKey: this.clientKey!,
            FieldGroups: 'PositionBase,PositionView,DisplayAndFormat,ExchangeInfo',
          },
        }
      );

      return (response.Data || []).map(mapSaxoPositionToHolding);
    } catch (error) {
      console.error(`[${this.brokerId}] Get holdings error:`, error);
      return [];
    }
  }

  protected async getFundsInternal(): Promise<Funds> {
    try {
      const response = await this.apiRequest<SaxoAccountBalance>(
        SAXO_ENDPOINTS.PORT_BALANCE.replace(':accountKey', this.accountKey!)
      );

      return mapSaxoBalanceToFunds(response, 'EUR' as Currency);
    } catch (error) {
      console.error(`[${this.brokerId}] Get funds error:`, error);
      return {
        availableCash: 0,
        usedMargin: 0,
        availableMargin: 0,
        totalBalance: 0,
        currency: 'EUR' as Currency,
      };
    }
  }

  // ============================================================================
  // MARKET DATA
  // ============================================================================

  protected async getQuoteInternal(symbol: string, exchange: StockExchange): Promise<Quote> {
    try {
      const uic = await this.getUicForSymbol(symbol, exchange);
      if (!uic) {
        throw new Error(`Instrument not found: ${symbol}`);
      }

      const assetType = 'Stock'; // Default, could be determined from search

      const response = await this.apiRequest<SaxoInfoPrice>(
        SAXO_ENDPOINTS.TRADE_INFO_PRICE
          .replace(':uic', uic.toString())
          .replace(':assetType', assetType),
        {
          params: {
            FieldGroups: 'DisplayAndFormat,InstrumentPriceDetails,Quote',
          },
        }
      );

      return mapSaxoInfoPriceToQuote(response, symbol, exchange);
    } catch (error) {
      console.error(`[${this.brokerId}] Get quote error:`, error);
      throw error;
    }
  }

  protected async getOHLCVInternal(
    symbol: string,
    exchange: StockExchange,
    timeframe: TimeFrame,
    from: Date,
    to: Date
  ): Promise<OHLCV[]> {
    try {
      const uic = await this.getUicForSymbol(symbol, exchange);
      if (!uic) {
        throw new Error(`Instrument not found: ${symbol}`);
      }

      const horizon = SAXO_TIMEFRAME_MAP[timeframe] || 1440;
      const assetType = 'Stock';

      const response = await this.apiRequest<SaxoChartData>(
        SAXO_ENDPOINTS.CHART_CHARTS,
        {
          params: {
            AssetType: assetType,
            Uic: uic,
            Horizon: horizon,
            Count: 500, // Max bars to fetch
            Mode: 'From',
            Time: from.toISOString(),
          },
        }
      );

      const ohlcv = mapSaxoChartDataToOHLCV(response.Data || []);

      // Filter by date range
      const fromTs = from.getTime();
      const toTs = to.getTime();
      return ohlcv.filter(bar => bar.timestamp >= fromTs && bar.timestamp <= toTs);
    } catch (error) {
      console.error(`[${this.brokerId}] Get OHLCV error:`, error);
      return [];
    }
  }

  protected async getMarketDepthInternal(
    symbol: string,
    exchange: StockExchange
  ): Promise<MarketDepth> {
    try {
      const uic = await this.getUicForSymbol(symbol, exchange);
      if (!uic) {
        throw new Error(`Instrument not found: ${symbol}`);
      }

      const assetType = 'Stock';

      const response = await this.apiRequest<SaxoInfoPrice>(
        SAXO_ENDPOINTS.TRADE_INFO_PRICE
          .replace(':uic', uic.toString())
          .replace(':assetType', assetType),
        {
          params: {
            FieldGroups: 'MarketDepth',
          },
        }
      );

      return mapSaxoToMarketDepth(response);
    } catch (error) {
      console.error(`[${this.brokerId}] Get market depth error:`, error);
      return { bids: [], asks: [], timestamp: Date.now() };
    }
  }

  // ============================================================================
  // WEBSOCKET
  // ============================================================================

  protected async connectWebSocketInternal(config: WebSocketConfig): Promise<void> {
    return new Promise((resolve, reject) => {
      const wsUrl = this.isPaperTrading ? SAXO_SIM_STREAM_URL : SAXO_LIVE_STREAM_URL;
      this.contextId = `ctx_${Date.now()}`;

      const url = `${wsUrl}?contextId=${this.contextId}`;

      this.ws = new WebSocket(url);

      this.ws.onopen = () => {
        console.log(`[${this.brokerId}] WebSocket connected`);

        // Send authorization
        this.ws?.send(JSON.stringify({
          ReferenceId: this.getNextReferenceId(),
          MessageType: 'Authorization',
          Data: {
            AccessToken: this.accessToken,
          },
        }));

        resolve();
      };

      this.ws.onmessage = (event) => {
        try {
          const messages = JSON.parse(event.data);
          for (const msg of Array.isArray(messages) ? messages : [messages]) {
            this.handleWSMessage(msg);
          }
        } catch (error) {
          console.error(`[${this.brokerId}] WebSocket message parse error:`, error);
        }
      };

      this.ws.onerror = (error) => {
        console.error(`[${this.brokerId}] WebSocket error:`, error);
        reject(error);
      };

      this.ws.onclose = () => {
        console.log(`[${this.brokerId}] WebSocket disconnected`);
        this.wsConnected = false;
      };
    });
  }

  private handleWSMessage(message: { ReferenceId: string; Data?: unknown }): void {
    const refId = message.ReferenceId;

    // Check if this is a price update
    if (refId.startsWith('price_')) {
      const uic = parseInt(refId.replace('price_', ''));
      const symbolInfo = this.uicToSymbolMap.get(uic);

      if (symbolInfo && message.Data) {
        const tick = mapSaxoWSQuoteToTickData(
          message.Data as Parameters<typeof mapSaxoWSQuoteToTickData>[0],
          symbolInfo.symbol,
          symbolInfo.exchange
        );
        this.emitTick(tick);
      }
    }
  }

  protected async subscribeInternal(
    symbol: string,
    exchange: StockExchange,
    mode: SubscriptionMode
  ): Promise<void> {
    const uic = await this.getUicForSymbol(symbol, exchange);
    if (!uic) {
      throw new Error(`Instrument not found: ${symbol}`);
    }

    // Store mapping for tick data
    this.uicToSymbolMap.set(uic, { symbol, exchange });

    const referenceId = `price_${uic}`;
    const assetType = 'Stock';

    // Create subscription via REST API
    await this.apiRequest(SAXO_ENDPOINTS.STREAM_PRICES, {
      method: 'POST',
      body: {
        ContextId: this.contextId,
        ReferenceId: referenceId,
        Arguments: {
          AssetType: assetType,
          Uic: uic,
          FieldGroups: mode === 'full'
            ? ['DisplayAndFormat', 'InstrumentPriceDetails', 'Quote']
            : ['Quote'],
        },
      },
    });

    console.log(`[${this.brokerId}] Subscribed to ${symbol}:${exchange}`);
  }

  protected async unsubscribeInternal(symbol: string, exchange: StockExchange): Promise<void> {
    const uic = await this.getUicForSymbol(symbol, exchange);
    if (!uic) return;

    const referenceId = `price_${uic}`;

    try {
      await this.apiRequest(
        `${SAXO_ENDPOINTS.STREAM_PRICES}/${this.contextId}/${referenceId}`,
        { method: 'DELETE' }
      );

      this.uicToSymbolMap.delete(uic);
      console.log(`[${this.brokerId}] Unsubscribed from ${symbol}:${exchange}`);
    } catch (error) {
      console.error(`[${this.brokerId}] Unsubscribe error:`, error);
    }
  }

  private getNextReferenceId(): string {
    return `ref_${++this.referenceId}`;
  }

  // ============================================================================
  // BULK OPERATIONS
  // ============================================================================

  protected async cancelAllOrdersInternal(): Promise<BulkOperationResult> {
    try {
      const orders = await this.getOrdersInternal();
      const results = await Promise.all(
        orders.map(order => this.cancelOrderInternal(order.orderId))
      );

      const successful = results.filter(r => r.success).length;
      const failed = results.length - successful;

      return {
        success: failed === 0,
        totalCount: results.length,
        successCount: successful,
        failedCount: failed,
        results: results.map((r, i) => ({
          success: r.success,
          orderId: orders[i].orderId,
          error: r.success ? undefined : r.message,
        })),
      };
    } catch (error) {
      return {
        success: false,
        totalCount: 0,
        successCount: 0,
        failedCount: 0,
        results: [],
      };
    }
  }

  protected async closeAllPositionsInternal(): Promise<BulkOperationResult> {
    try {
      const positions = await this.getPositionsInternal();
      const results: OrderResponse[] = [];

      for (const position of positions) {
        const closeOrder = await this.placeOrderInternal({
          symbol: position.symbol,
          exchange: position.exchange,
          side: position.quantity > 0 ? 'SELL' : 'BUY',
          orderType: 'MARKET',
          quantity: Math.abs(position.quantity),
          productType: position.productType,
        });
        results.push(closeOrder);
      }

      const successful = results.filter(r => r.success).length;
      const failed = results.length - successful;

      return {
        success: failed === 0,
        totalCount: results.length,
        successCount: successful,
        failedCount: failed,
        results: results.map((r, i) => ({
          success: r.success,
          symbol: positions[i].symbol,
          error: r.success ? undefined : r.message,
        })),
      };
    } catch (error) {
      return {
        success: false,
        totalCount: 0,
        successCount: 0,
        failedCount: 0,
        results: [],
      };
    }
  }

  // ============================================================================
  // SMART ORDER
  // ============================================================================

  protected async placeSmartOrderInternal(params: SmartOrderParams): Promise<OrderResponse> {
    // Saxo supports related orders (bracket orders)
    try {
      const { symbol, exchange } = parseSymbolExchange(params.symbol);
      const uic = await this.getUicForSymbol(symbol, exchange || params.exchange);

      if (!uic) {
        return this.createErrorResponse(`Instrument not found: ${params.symbol}`);
      }

      const mainOrder: SaxoOrderRequest = {
        AccountKey: this.accountKey!,
        Amount: params.quantity,
        AssetType: this.mapProductTypeToAssetType(params.productType),
        BuySell: SAXO_SIDE_MAP[params.side] as SaxoBuySell,
        ManualOrder: true,
        OrderDuration: {
          DurationType: 'DayOrder',
        },
        OrderType: (SAXO_ORDER_TYPE_MAP[params.orderType] || 'Market') as SaxoOrderTypeEnum,
        Uic: uic,
        Orders: [],
      };

      if (params.price) {
        mainOrder.OrderPrice = params.price;
      }

      // Add stop loss
      if (params.stopLoss) {
        mainOrder.Orders!.push({
          Amount: params.quantity,
          AssetType: mainOrder.AssetType,
          BuySell: params.side === 'BUY' ? 'Sell' : 'Buy',
          OrderDuration: { DurationType: 'GoodTillCancel' },
          OrderPrice: params.stopLoss,
          OrderType: 'Stop',
          Uic: uic,
        });
      }

      const response = await this.apiRequest<{ OrderId: string; Orders?: { OrderId: string }[] }>(
        SAXO_ENDPOINTS.TRADE_ORDERS,
        { method: 'POST', body: mainOrder }
      );

      return {
        success: true,
        message: 'Smart order placed successfully',
        orderId: response.OrderId,
      };
    } catch (error) {
      return this.createErrorResponse(
        error instanceof Error ? error.message : 'Smart order failed'
      );
    }
  }

  // ============================================================================
  // MARGIN CALCULATOR
  // ============================================================================

  protected async calculateMarginInternal(orders: OrderParams[]): Promise<MarginRequired> {
    // Saxo provides margin impact via InfoPrice with MarginImpact field group
    try {
      let totalMargin = 0;

      for (const order of orders) {
        const { symbol, exchange } = parseSymbolExchange(order.symbol);
        const uic = await this.getUicForSymbol(symbol, exchange || order.exchange);

        if (uic) {
          const assetType = this.mapProductTypeToAssetType(order.productType);

          const response = await this.apiRequest<{
            MarginImpact?: { InitialMargin: number };
          }>(
            SAXO_ENDPOINTS.TRADE_INFO_PRICE
              .replace(':uic', uic.toString())
              .replace(':assetType', assetType),
            {
              params: {
                FieldGroups: 'MarginImpact',
                Amount: order.quantity,
              },
            }
          );

          if (response.MarginImpact) {
            totalMargin += response.MarginImpact.InitialMargin;
          }
        }
      }

      return {
        totalMargin,
        initialMargin: totalMargin,
      };
    } catch (error) {
      console.error(`[${this.brokerId}] Margin calculation error:`, error);
      return { totalMargin: 0, initialMargin: 0 };
    }
  }

  // ============================================================================
  // HELPER METHODS
  // ============================================================================

  private async getUicForSymbol(symbol: string, exchange: string): Promise<number | null> {
    try {
      const response = await this.apiRequest<SaxoApiResponse<SaxoInstrument>>(
        SAXO_ENDPOINTS.REF_INSTRUMENTS,
        {
          params: {
            Keywords: symbol,
            ExchangeId: exchange,
            AssetTypes: 'Stock,Etf,Etn',
            IncludeNonTradable: false,
          },
        }
      );

      if (response.Data && response.Data.length > 0) {
        // Find exact match
        const exactMatch = response.Data.find(
          i => i.Symbol.toUpperCase() === symbol.toUpperCase()
        );
        return exactMatch?.Uic || response.Data[0].Uic;
      }

      return null;
    } catch (error) {
      console.error(`[${this.brokerId}] Get UIC error:`, error);
      return null;
    }
  }

  private mapProductTypeToAssetType(productType: string): SaxoAssetType {
    return (SAXO_ASSET_TYPE_MAP[productType] as SaxoAssetType) || 'Stock';
  }

  // ============================================================================
  // ACCOUNT MANAGEMENT
  // ============================================================================

  async getAccounts(): Promise<{ accountId: string; accountKey: string; displayName: string }[]> {
    try {
      const response = await this.apiRequest<SaxoApiResponse<{
        AccountId: string;
        AccountKey: string;
        DisplayName: string;
      }>>(SAXO_ENDPOINTS.PORT_ACCOUNTS, {
        params: { ClientKey: this.clientKey! },
      });

      return (response.Data || []).map(acc => ({
        accountId: acc.AccountId,
        accountKey: acc.AccountKey,
        displayName: acc.DisplayName,
      }));
    } catch (error) {
      console.error(`[${this.brokerId}] Get accounts error:`, error);
      return [];
    }
  }

  async switchAccount(accountKey: string): Promise<boolean> {
    try {
      // Verify account exists
      const accounts = await this.getAccounts();
      const account = accounts.find(a => a.accountKey === accountKey);

      if (!account) {
        console.error(`[${this.brokerId}] Account not found: ${accountKey}`);
        return false;
      }

      this.accountKey = accountKey;
      console.log(`[${this.brokerId}] Switched to account: ${account.displayName}`);
      return true;
    } catch (error) {
      console.error(`[${this.brokerId}] Switch account error:`, error);
      return false;
    }
  }

  // Paper trading mode
  setPaperTradingMode(enabled: boolean): void {
    this.isPaperTrading = enabled;
    console.log(`[${this.brokerId}] Paper trading mode: ${enabled ? 'enabled' : 'disabled'}`);
  }

  isPaperTradingEnabled(): boolean {
    return this.isPaperTrading;
  }
}

// Export singleton instance
export const saxoBankAdapter = new SaxoBankAdapter();
