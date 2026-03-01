/**
 * Fyers Stock Broker Adapter
 *
 * Implements IStockBrokerAdapter for Fyers API v3
 */

import { invoke } from '@tauri-apps/api/core';
import { BaseStockBrokerAdapter } from '../../BaseStockBrokerAdapter';
import type {
  StockBrokerMetadata,
  Region,
  BrokerCredentials,
  AuthResponse,
  AuthCallbackParams,
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
  TickData,
  Instrument,
} from '../../types';
import {
  FYERS_METADATA,
  FYERS_LOGIN_URL,
} from './constants';
import { fromFyersQuote, toFyersSymbol } from './mapper';

// Domain helpers
import {
  fyersBuildAuthUrl,
  fyersExchangeToken,
  fyersStoreCredentials,
} from './auth';
import {
  fyersPlaceOrder,
  fyersModifyOrder,
  fyersCancelOrder,
  fyersGetOrders,
  fyersGetTradeBook,
  fyersCancelAllOrders,
  fyersPlaceSmartOrder,
  fyersCalculateMargin,
  fyersCloseAllPositions,
} from './orders';
import {
  fyersGetPositions,
  fyersGetOpenPositionQuantity,
  fyersGetHoldings,
  fyersGetFunds,
} from './portfolio';
import {
  fyersGetQuote,
  fyersGetMultipleQuotes,
  fyersGetOHLCV,
  fyersGetMarketDepth,
  fyersFetchBatchQuotesRaw,
} from './marketData';
import {
  fyersIsMarketOpen,
  fyersConnectWebSocket,
  fyersDisconnectWebSocket,
  fyersIsWebSocketConnected,
  fyersWsSubscribe,
  fyersWsSubscribeBatch,
  fyersWsUnsubscribe,
  fyersSetupWebSocketListeners,
} from './websocket';
import {
  fyersSearchSymbols,
  fyersGetInstrument,
  fyersDownloadMasterContract,
  fyersGetMasterContractLastUpdated,
} from './symbolSearch';

/**
 * Fyers broker adapter
 */
export class FyersAdapter extends BaseStockBrokerAdapter {
  readonly brokerId = 'fyers';
  readonly brokerName = 'Fyers';
  readonly region: Region = 'india';
  readonly metadata: StockBrokerMetadata = FYERS_METADATA;

  private apiKey: string | null = null;
  private apiSecret: string | null = null;

  // WebSocket state (protected to match base class)
  protected override wsConnected: boolean = false;
  protected wsSubscriptions: Set<string> = new Set();

  // Tauri event unlisteners for WebSocket
  protected wsUnlisteners: Array<() => void> = [];

  // Hybrid data strategy: WebSocket during market hours, polling off-hours
  private pollingInterval: NodeJS.Timeout | null = null;
  private pollingSymbols: Map<string, { symbol: string; exchange: StockExchange; mode: SubscriptionMode }> = new Map();
  private readonly POLLING_INTERVAL_MS = 30 * 60 * 1000; // 30 minutes for off-market polling
  private usePollingMode: boolean = false;
  private marketHoursCheckInterval: NodeJS.Timeout | null = null;

  // Debounce timer for batched initial fetch
  private pendingInitialFetch: NodeJS.Timeout | null = null;
  private pendingSymbols: Set<string> = new Set();

  // Track symbols with errors (to avoid repeated error logging)
  private errorSymbols: Set<string> = new Set();

  // ============================================================================
  // Authentication
  // ============================================================================

  getAuthUrl(): string {
    if (!this.apiKey) {
      throw new Error('API key not set. Call setCredentials first.');
    }
    return fyersBuildAuthUrl(this.apiKey, FYERS_LOGIN_URL);
  }

  setCredentials(apiKey: string, apiSecret: string): void {
    console.log('[FyersAdapter] setCredentials called with:', {
      apiKey: apiKey ? `${apiKey.substring(0, 10)}...` : 'empty',
      apiSecret: apiSecret ? '***' : 'empty',
      apiKeyLength: apiKey?.length || 0,
      apiSecretLength: apiSecret?.length || 0,
    });
    this.apiKey = apiKey;
    this.apiSecret = apiSecret;
    console.log('[FyersAdapter] Credentials stored in memory:', {
      hasApiKey: !!this.apiKey,
      hasApiSecret: !!this.apiSecret,
    });
  }

  async authenticate(credentials: BrokerCredentials): Promise<AuthResponse> {
    try {
      this.apiKey = credentials.apiKey;
      this.apiSecret = credentials.apiSecret || null;

      const authCode = (credentials as any).authCode;
      if (authCode) {
        console.log('[FyersAdapter] Auth code detected, exchanging for access token...');
        return this.handleAuthCallback({ authCode });
      }

      if (credentials.accessToken) {
        console.log('[FyersAdapter] Access token provided, storing...');
        if (credentials.accessToken.length < 40 && /^[A-Za-z0-9]+$/.test(credentials.accessToken)) {
          console.log('[FyersAdapter] Short token detected, treating as auth_code...');
          return this.handleAuthCallback({ authCode: credentials.accessToken });
        }

        this.accessToken = credentials.accessToken;
        this.userId = credentials.userId || null;
        this._isConnected = true;

        await this.storeCredentials(credentials);

        return {
          success: true,
          accessToken: this.accessToken,
          userId: this.userId || undefined,
          message: 'Authentication successful',
        };
      }

      console.warn('[FyersAdapter] No access token or auth code provided');
      return {
        success: false,
        message: 'Please complete OAuth login',
        errorCode: 'AUTH_REQUIRED',
      };
    } catch (error) {
      return {
        success: false,
        message: error instanceof Error ? error.message : 'Authentication failed',
        errorCode: 'AUTH_FAILED',
      };
    }
  }

  async handleAuthCallback(params: AuthCallbackParams): Promise<AuthResponse> {
    try {
      if (!params.authCode && !params.requestToken) {
        return {
          success: false,
          message: 'Authorization code not provided',
          errorCode: 'AUTH_FAILED',
        };
      }

      if (!this.apiKey || !this.apiSecret) {
        return {
          success: false,
          message: 'API credentials not set',
          errorCode: 'CREDENTIALS_MISSING',
        };
      }

      const authCode = params.authCode || params.requestToken || '';
      const result = await fyersExchangeToken(this.apiKey, this.apiSecret, authCode);

      if (result.success && result.accessToken) {
        this.accessToken = result.accessToken;
        this.userId = result.userId || null;
        this._isConnected = true;

        await this.storeCredentials({
          apiKey: this.apiKey,
          apiSecret: this.apiSecret,
          accessToken: this.accessToken,
          userId: this.userId || undefined,
        });
      }

      return result;
    } catch (error) {
      return {
        success: false,
        message: error instanceof Error ? error.message : 'Authentication callback failed',
        errorCode: 'AUTH_CALLBACK_FAILED',
      };
    }
  }

  async initFromStorage(): Promise<boolean> {
    try {
      console.log(`[${this.brokerId}] Attempting to load stored credentials...`);
      const credentials = await this.loadCredentials();

      if (!credentials || !credentials.apiKey) {
        console.log(`[${this.brokerId}] No stored credentials found`);
        return false;
      }

      console.log(`[${this.brokerId}] Found stored credentials, restoring session...`);
      this.apiKey = credentials.apiKey;
      this.apiSecret = credentials.apiSecret || null;
      this.accessToken = credentials.accessToken || null;

      if (this.accessToken) {
        console.log(`[${this.brokerId}] Access token found, validating...`);

        const isTokenValid = await this.validateToken(this.accessToken);
        if (!isTokenValid) {
          console.warn(`[${this.brokerId}] Token validation failed (expired or from previous day)`);

          await this.storeCredentials({
            apiKey: this.apiKey,
            apiSecret: this.apiSecret || '',
          });

          this.accessToken = null;
          this._isConnected = false;

          console.log(`[${this.brokerId}] Please re-authenticate to continue`);
          return false;
        }

        this._isConnected = true;
        console.log(`[${this.brokerId}] ✓ Session restored successfully`);

        try {
          console.log(`[${this.brokerId}] Downloading master contract for WebSocket...`);
          await invoke('download_fyers_master_contract');
          console.log(`[${this.brokerId}] ✓ Master contract ready`);
        } catch (mcErr) {
          console.warn(`[${this.brokerId}] Master contract download failed:`, mcErr);
        }

        return true;
      }

      console.log(`[${this.brokerId}] No access token found, manual authentication required`);
      return false;
    } catch (error) {
      console.error(`[${this.brokerId}] Failed to initialize from storage:`, error);
      return false;
    }
  }

  private async validateToken(_token: string): Promise<boolean> {
    return this.validateTokenWithDateCheck();
  }

  protected override async validateTokenWithApi(_token: string): Promise<boolean> {
    try {
      await this.getFundsInternal();
      return true;
    } catch {
      return false;
    }
  }

  // ============================================================================
  // Orders - Internal Implementations
  // ============================================================================

  protected async placeOrderInternal(params: OrderParams): Promise<OrderResponse> {
    this.ensureAuthenticated();
    return fyersPlaceOrder(this.apiKey!, this.accessToken!, params);
  }

  protected async modifyOrderInternal(params: ModifyOrderParams): Promise<OrderResponse> {
    this.ensureAuthenticated();
    return fyersModifyOrder(this.apiKey!, this.accessToken!, params);
  }

  protected async cancelOrderInternal(orderId: string): Promise<OrderResponse> {
    this.ensureAuthenticated();
    return fyersCancelOrder(this.apiKey!, this.accessToken!, orderId);
  }

  protected async getOrdersInternal(): Promise<Order[]> {
    this.ensureAuthenticated();
    return fyersGetOrders(this.apiKey!, this.accessToken!);
  }

  protected async getTradeBookInternal(): Promise<Trade[]> {
    this.ensureAuthenticated();
    return fyersGetTradeBook(this.apiKey!, this.accessToken!);
  }

  // ============================================================================
  // Portfolio - Internal Implementations
  // ============================================================================

  protected async getPositionsInternal(): Promise<Position[]> {
    this.ensureAuthenticated();
    return fyersGetPositions(this.apiKey!, this.accessToken!);
  }

  private async getOpenPositionQuantity(
    symbol: string,
    exchange: StockExchange,
    productType: string
  ): Promise<number> {
    return fyersGetOpenPositionQuantity(this.apiKey!, this.accessToken!, symbol, exchange, productType);
  }

  protected async getHoldingsInternal(): Promise<Holding[]> {
    this.ensureAuthenticated();
    return fyersGetHoldings(this.apiKey!, this.accessToken!);
  }

  protected async getFundsInternal(): Promise<Funds> {
    this.ensureAuthenticated();
    return fyersGetFunds(this.apiKey!, this.accessToken!);
  }

  // ============================================================================
  // Market Data - Internal Implementations
  // ============================================================================

  protected async getQuoteInternal(symbol: string, exchange: StockExchange): Promise<Quote> {
    this.ensureAuthenticated();
    return fyersGetQuote(this.apiKey!, this.accessToken!, symbol, exchange);
  }

  protected async getMultipleQuotesInternal(
    symbols: Array<{ symbol: string; exchange: StockExchange }>
  ): Promise<Quote[]> {
    this.ensureAuthenticated();
    return fyersGetMultipleQuotes(this.apiKey!, this.accessToken!, symbols);
  }

  protected async getOHLCVInternal(
    symbol: string,
    exchange: StockExchange,
    timeframe: TimeFrame,
    from: Date,
    to: Date
  ): Promise<OHLCV[]> {
    this.ensureAuthenticated();
    return fyersGetOHLCV(this.apiKey!, this.accessToken!, symbol, exchange, timeframe, from, to);
  }

  protected async getMarketDepthInternal(symbol: string, exchange: StockExchange): Promise<MarketDepth> {
    this.ensureAuthenticated();
    return fyersGetMarketDepth(this.apiKey!, this.accessToken!, symbol, exchange);
  }

  // ============================================================================
  // WebSocket - Internal Implementations (Hybrid: WebSocket + Polling)
  // ============================================================================

  protected async connectWebSocketInternal(config: WebSocketConfig): Promise<void> {
    this.ensureAuthenticated();

    try {
      if (!this.apiKey || !this.accessToken) {
        throw new Error('API credentials not set');
      }

      const marketOpen = fyersIsMarketOpen();

      if (marketOpen) {
        console.log('[Fyers] Market is OPEN - attempting WebSocket connection...');
        await this.connectWebSocketDirect();
      } else {
        console.log('[Fyers] Market is CLOSED - using polling mode for data');
        this.usePollingMode = true;
        this.startPolling();
      }

      this.startMarketHoursMonitor();

    } catch (error) {
      console.error('[Fyers] Connection failed, falling back to polling:', error);
      this.usePollingMode = true;
      this.startPolling();
    }
  }

  private async connectWebSocketDirect(): Promise<void> {
    await fyersConnectWebSocket(this.apiKey!, this.accessToken!);
    this.wsConnected = true;
    this.usePollingMode = false;

    this.wsUnlisteners = await fyersSetupWebSocketListeners(
      (tick) => this.emitTick(tick),
      (depth) => this.emitDepth(depth),
      () => { this.wsConnected = false; }
    );

    await this.resubscribeViaWebSocket();
  }

  private startMarketHoursMonitor(): void {
    if (this.marketHoursCheckInterval) {
      clearInterval(this.marketHoursCheckInterval);
    }

    this.marketHoursCheckInterval = setInterval(async () => {
      const marketOpen = fyersIsMarketOpen();

      if (marketOpen && this.usePollingMode) {
        console.log('[Fyers] Market opened - switching to WebSocket');
        this.stopPolling();
        try {
          await this.connectWebSocketDirect();
        } catch (err) {
          console.warn('[Fyers] WebSocket failed, continuing with polling:', err);
          this.startPolling();
        }
      } else if (!marketOpen && !this.usePollingMode && this.wsConnected) {
        console.log('[Fyers] Market closed - switching to polling');
        await this.disconnectWebSocket();
        this.usePollingMode = true;
        this.startPolling();
      }
    }, 5 * 60 * 1000); // Check every 5 minutes
  }

  private async resubscribeViaWebSocket(): Promise<void> {
    if (this.pollingSymbols.size === 0) {
      console.log('[Fyers] No symbols to resubscribe');
      return;
    }

    const depthSymbols: string[] = [];
    const quoteSymbols: string[] = [];

    for (const [fyersSymbol, data] of this.pollingSymbols) {
      if (data.mode === 'full') {
        depthSymbols.push(fyersSymbol);
      } else {
        quoteSymbols.push(fyersSymbol);
      }
    }

    if (depthSymbols.length > 0) {
      console.log(`[Fyers] Batch subscribing ${depthSymbols.length} depth symbols...`);
      try {
        const response = await fyersWsSubscribeBatch(depthSymbols, 'depth');
        if (response.success) {
          depthSymbols.forEach(s => this.wsSubscriptions.add(s));
          console.log(`[Fyers] ✓ Batch subscribed ${depthSymbols.length} depth symbols`);
        } else {
          console.warn(`[Fyers] Batch depth subscription failed: ${response.error}`);
        }
      } catch (err) {
        console.error('[Fyers] Batch depth subscription error:', err);
      }
    }

    if (quoteSymbols.length > 0) {
      console.log(`[Fyers] Batch subscribing ${quoteSymbols.length} quote symbols...`);
      try {
        const response = await fyersWsSubscribeBatch(quoteSymbols, 'quote');
        if (response.success) {
          quoteSymbols.forEach(s => this.wsSubscriptions.add(s));
          console.log(`[Fyers] ✓ Batch subscribed ${quoteSymbols.length} quote symbols`);
        } else {
          console.warn(`[Fyers] Batch quote subscription failed: ${response.error}`);
        }
      } catch (err) {
        console.error('[Fyers] Batch quote subscription error:', err);
      }
    }
  }

  private cleanupWebSocketListeners(): void {
    for (const unlisten of this.wsUnlisteners) {
      unlisten();
    }
    this.wsUnlisteners = [];
  }

  // ============================================================================
  // Polling Mode - For Off-Market Hours
  // ============================================================================

  private startPolling(): void {
    if (this.pollingInterval) {
      return;
    }

    console.log(`[Fyers] Polling mode started (${this.POLLING_INTERVAL_MS / 1000}s interval)`);

    this.pollingInterval = setInterval(async () => {
      await this.pollSubscribedSymbols();
    }, this.POLLING_INTERVAL_MS);

    this.pollSubscribedSymbols();
  }

  private stopPolling(): void {
    if (this.pollingInterval) {
      clearInterval(this.pollingInterval);
      this.pollingInterval = null;
      console.log('[Fyers] Polling stopped');
    }
  }

  private async pollSubscribedSymbols(): Promise<void> {
    if (this.pollingSymbols.size === 0) {
      return;
    }

    try {
      const fyersSymbols: string[] = [];
      for (const [fyersSymbol] of this.pollingSymbols) {
        fyersSymbols.push(fyersSymbol);
      }

      const response = await fyersFetchBatchQuotesRaw(this.apiKey!, this.accessToken!, fyersSymbols);

      if (response.success && response.data) {
        for (const quoteData of response.data) {
          const quote = fromFyersQuote(quoteData as Record<string, unknown>);
          if (quote.error) continue;

          const tick: TickData = {
            symbol: quote.symbol,
            exchange: quote.exchange,
            mode: 'quote',
            lastPrice: quote.lastPrice,
            open: quote.open,
            high: quote.high,
            low: quote.low,
            close: quote.close,
            volume: quote.volume,
            change: quote.change,
            changePercent: quote.changePercent,
            bid: quote.bid,
            ask: quote.ask,
            bidQty: quote.bidQty,
            askQty: quote.askQty,
            timestamp: quote.timestamp,
          };

          this.emitTick(tick);
        }
      } else if (!response.success) {
        console.error('[Fyers] Poll failed:', response.error);
      }
    } catch (error) {
      console.error('[Fyers] Polling error:', error);
    }
  }

  // ============================================================================
  // Subscribe/Unsubscribe - Hybrid Mode (Override base class)
  // ============================================================================

  async subscribe(symbol: string, exchange: StockExchange, mode: SubscriptionMode): Promise<void> {
    console.log(`[Fyers] >>> SUBSCRIBE: ${symbol}@${exchange} mode=${mode} | ws=${this.wsConnected} poll=${this.usePollingMode}`);
    await this.subscribeInternal(symbol, exchange, mode);
  }

  async subscribeBatch(symbols: Array<{ symbol: string; exchange: StockExchange }>, mode: SubscriptionMode = 'quote'): Promise<void> {
    if (symbols.length === 0) return;

    console.log(`[Fyers] >>> BATCH SUBSCRIBE: ${symbols.length} symbols, mode=${mode}`);
    const startTime = Date.now();

    const fyersSymbols = symbols.map(({ symbol, exchange }) => {
      const fyersSymbol = toFyersSymbol(symbol, exchange);
      this.pollingSymbols.set(fyersSymbol, { symbol, exchange, mode });
      return fyersSymbol;
    });

    if (!this.wsConnected && !this.usePollingMode && !this.pollingInterval) {
      const marketOpen = fyersIsMarketOpen();
      console.log(`[Fyers] Init strategy: market=${marketOpen ? 'OPEN' : 'CLOSED'}`);

      if (marketOpen) {
        try {
          await this.connectWebSocketDirect();
          this.startMarketHoursMonitor();
        } catch (wsError) {
          console.error('[Fyers] WS failed, fallback to polling:', wsError);
          this.usePollingMode = true;
          this.startPolling();
        }
      } else {
        this.usePollingMode = true;
        this.startPolling();
        this.startMarketHoursMonitor();
      }
    }

    if (this.wsConnected && !this.usePollingMode) {
      try {
        const wsMode = mode === 'full' ? 'depth' : 'quote';
        const response = await fyersWsSubscribeBatch(fyersSymbols, wsMode);

        if (response.success) {
          fyersSymbols.forEach(s => this.wsSubscriptions.add(s));
          console.log(`[Fyers] ✓ Batch WS subscribed to ${fyersSymbols.length} symbols in ${Date.now() - startTime}ms`);
          return;
        } else {
          console.warn(`[Fyers] Batch WS subscribe failed: ${response.error}, falling back to polling`);
        }
      } catch (err) {
        console.error('[Fyers] Batch WS subscribe error:', err);
      }
    }

    console.log(`[Fyers] Fetching ${fyersSymbols.length} quotes via REST (polling mode)`);
    await this.fetchBatchQuotes(fyersSymbols);

    if (!this.pollingInterval) {
      this.usePollingMode = true;
      this.startPolling();
    }

    console.log(`[Fyers] ✓ Batch subscribe complete in ${Date.now() - startTime}ms`);
  }

  protected async subscribeInternal(symbol: string, exchange: StockExchange, mode: SubscriptionMode): Promise<void> {
    const fyersSymbol = toFyersSymbol(symbol, exchange);
    this.pollingSymbols.set(fyersSymbol, { symbol, exchange, mode });

    try {
      if (!this.wsConnected && !this.usePollingMode && !this.pollingInterval) {
        const marketOpen = fyersIsMarketOpen();
        console.log(`[Fyers] Init strategy: market=${marketOpen ? 'OPEN' : 'CLOSED'}`);

        if (marketOpen) {
          try {
            await this.connectWebSocketDirect();
            this.startMarketHoursMonitor();
          } catch (wsError) {
            console.error('[Fyers] WS failed, fallback to polling:', wsError);
            this.usePollingMode = true;
            this.startPolling();
          }
        } else {
          this.usePollingMode = true;
          this.startPolling();
          this.startMarketHoursMonitor();
        }
      }

      if (this.usePollingMode) {
        if (!this.pollingInterval) this.startPolling();
        this.queueInitialFetch(fyersSymbol);
      } else if (this.wsConnected) {
        const channel = mode === 'full' ? 'depth' : 'SymbolUpdate';
        const response = await fyersWsSubscribe(fyersSymbol, channel);
        if (!response.success) {
          console.warn(`[Fyers] WS subscribe failed for ${fyersSymbol}: ${response.error}`);
          this.queueInitialFetch(fyersSymbol);
        }
        this.wsSubscriptions.add(fyersSymbol);
      } else {
        this.queueInitialFetch(fyersSymbol);
        if (!this.pollingInterval) {
          this.usePollingMode = true;
          this.startPolling();
        }
      }
    } catch (error) {
      console.error(`[Fyers] Subscribe error for ${fyersSymbol}:`, error);
      this.queueInitialFetch(fyersSymbol);
    }
  }

  private queueInitialFetch(fyersSymbol: string): void {
    this.pendingSymbols.add(fyersSymbol);

    if (this.pendingInitialFetch) {
      clearTimeout(this.pendingInitialFetch);
    }

    this.pendingInitialFetch = setTimeout(async () => {
      const symbols = Array.from(this.pendingSymbols);
      this.pendingSymbols.clear();
      this.pendingInitialFetch = null;

      if (symbols.length > 0) {
        console.log(`[Fyers] Batch fetching ${symbols.length} symbols for initial data`);
        await this.fetchBatchQuotes(symbols);
      }
    }, 200);
  }

  private async fetchBatchQuotes(fyersSymbols: string[]): Promise<void> {
    try {
      const response = await fyersFetchBatchQuotesRaw(this.apiKey!, this.accessToken!, fyersSymbols);

      if (response.success && response.data) {
        let successCount = 0;
        for (const quoteData of response.data) {
          const quote = fromFyersQuote(quoteData as Record<string, unknown>);
          if (quote.error) continue;

          const tick: TickData = {
            symbol: quote.symbol,
            exchange: quote.exchange,
            mode: 'quote',
            lastPrice: quote.lastPrice,
            open: quote.open,
            high: quote.high,
            low: quote.low,
            close: quote.close,
            volume: quote.volume,
            change: quote.change,
            changePercent: quote.changePercent,
            bid: quote.bid,
            ask: quote.ask,
            bidQty: quote.bidQty,
            askQty: quote.askQty,
            timestamp: quote.timestamp,
          };

          this.emitTick(tick);
          successCount++;
        }
        console.log(`[Fyers] Batch fetch: ${successCount}/${fyersSymbols.length} quotes`);
      } else {
        console.error('[Fyers] Batch fetch failed:', response.error);
      }
    } catch (error) {
      console.error('[Fyers] Batch fetch error:', error);
    }
  }

  private async fetchAndEmitQuote(fyersSymbol: string, symbol: string, exchange: StockExchange): Promise<void> {
    try {
      const response = await fyersFetchBatchQuotesRaw(this.apiKey!, this.accessToken!, [fyersSymbol]);

      if (response.success && response.data && response.data.length > 0) {
        const quote = fromFyersQuote(response.data[0] as Record<string, unknown>);

        const tick: TickData = {
          symbol: quote.symbol || symbol,
          exchange: quote.exchange || exchange,
          mode: 'quote',
          lastPrice: quote.lastPrice,
          open: quote.open,
          high: quote.high,
          low: quote.low,
          close: quote.close,
          volume: quote.volume,
          change: quote.change,
          changePercent: quote.changePercent,
          bid: quote.bid,
          ask: quote.ask,
          bidQty: quote.bidQty,
          askQty: quote.askQty,
          timestamp: quote.timestamp,
        };

        this.emitTick(tick);
        console.log(`[Fyers] Fetched quote for ${fyersSymbol}: ${tick.lastPrice}`);
      }
    } catch (error) {
      console.error(`[Fyers] Failed to fetch quote for ${fyersSymbol}:`, error);
    }
  }

  protected async unsubscribeInternal(symbol: string, exchange: StockExchange): Promise<void> {
    try {
      const fyersSymbol = toFyersSymbol(symbol, exchange);

      this.pollingSymbols.delete(fyersSymbol);

      if (this.wsConnected && this.wsSubscriptions.has(fyersSymbol)) {
        const response = await fyersWsUnsubscribe(fyersSymbol);

        if (!response.success) {
          console.warn(`[Fyers] WebSocket unsubscribe failed: ${response.error}`);
        }

        this.wsSubscriptions.delete(fyersSymbol);
      }

      console.log(`[Fyers] Unsubscribed from ${fyersSymbol} (${this.pollingSymbols.size} remaining)`);

      if (this.pollingSymbols.size === 0 && this.pollingInterval) {
        this.stopPolling();
      }

    } catch (error) {
      console.error('[Fyers] Unsubscribe error:', error);
    }
  }

  async disconnectWebSocket(): Promise<void> {
    await fyersDisconnectWebSocket();
    this.wsConnected = false;
    this.wsSubscriptions.clear();
    this.cleanupWebSocketListeners();
  }

  async isWebSocketConnected(): Promise<boolean> {
    const connected = await fyersIsWebSocketConnected();
    this.wsConnected = connected;
    return connected;
  }

  async logout(): Promise<void> {
    this.stopPolling();
    if (this.marketHoursCheckInterval) {
      clearInterval(this.marketHoursCheckInterval);
      this.marketHoursCheckInterval = null;
    }
    await this.disconnectWebSocket();
    this.pollingSymbols.clear();
    await super.logout();
  }

  // ============================================================================
  // Bulk Operations - Internal Implementations
  // ============================================================================

  protected async cancelAllOrdersInternal(): Promise<BulkOperationResult> {
    this.ensureAuthenticated();
    return fyersCancelAllOrders(this.apiKey!, this.accessToken!);
  }

  protected async closeAllPositionsInternal(): Promise<BulkOperationResult> {
    this.ensureAuthenticated();

    try {
      const positions = await this.getPositionsInternal();
      return fyersCloseAllPositions(this.apiKey!, this.accessToken!, positions);
    } catch (error) {
      console.error('Failed to close all positions:', error);
      return {
        success: false,
        totalCount: 0,
        successCount: 0,
        failedCount: 0,
        results: [],
      };
    }
  }

  protected async placeSmartOrderInternal(params: SmartOrderParams): Promise<OrderResponse> {
    this.ensureAuthenticated();
    return fyersPlaceSmartOrder(
      this.apiKey!,
      this.accessToken!,
      params,
      (symbol, exchange, productType) => this.getOpenPositionQuantity(symbol, exchange, productType)
    );
  }

  protected async calculateMarginInternal(orders: OrderParams[]): Promise<MarginRequired> {
    this.ensureAuthenticated();
    return fyersCalculateMargin(this.apiKey!, this.accessToken!, orders);
  }

  // ============================================================================
  // Helper Methods
  // ============================================================================

  protected async storeCredentials(credentials: BrokerCredentials): Promise<void> {
    await fyersStoreCredentials(this.brokerId, credentials);
  }

  // ============================================================================
  // Symbol Search & Master Contract
  // ============================================================================

  async searchSymbols(query: string, exchange?: StockExchange): Promise<Instrument[]> {
    return fyersSearchSymbols(query, exchange);
  }

  async getInstrument(symbol: string, exchange: StockExchange): Promise<Instrument | null> {
    return fyersGetInstrument(symbol, exchange);
  }

  async downloadMasterContract(): Promise<void> {
    return fyersDownloadMasterContract();
  }

  async getMasterContractLastUpdated(): Promise<Date | null> {
    return fyersGetMasterContractLastUpdated();
  }
}
