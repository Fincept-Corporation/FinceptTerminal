/**
 * Interactive Brokers (IBKR) Adapter
 *
 * Implementation of BaseStockBrokerAdapter for Interactive Brokers
 * Supports both OAuth (api.ibkr.com) and Gateway (localhost) modes
 */

import { invoke } from '@tauri-apps/api/core';
import { BaseStockBrokerAdapter } from '../../BaseStockBrokerAdapter';
import type {
  BrokerCredentials,
  AuthResponse,
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
  Funds,
  TimeFrame,
  StockExchange,
  Instrument,
  WebSocketConfig,
  SubscriptionMode,
  BulkOperationResult,
  SmartOrderParams,
  MarginRequired,
  StockBrokerMetadata,
  Region,
  InstrumentType,
  Currency,
} from '../../types';

import { IBKR_METADATA, IBKR_GATEWAY_BASE, IBKR_API_BASE, IBKR_DEFAULT_SNAPSHOT_FIELDS } from './constants';
import type { IBKROrder, IBKRPosition, IBKRMarketDataSnapshot, IBKRHistoricalData, IBKRSecDefSearch, IBKRAccountSummary, IBKRAuthStatus } from './types';

/**
 * Interactive Brokers Adapter
 *
 * Supports:
 * - OAuth mode (api.ibkr.com) - for web/cloud applications
 * - Gateway mode (localhost:5000) - for local applications
 * - Paper trading via account ID prefix (DU = paper, U = live)
 */
export class IBKRAdapter extends BaseStockBrokerAdapter {
  readonly brokerId = 'ibkr';
  readonly brokerName = 'Interactive Brokers';
  readonly region: Region = 'us';
  readonly metadata: StockBrokerMetadata = IBKR_METADATA;

  private accountId: string = '';
  private useGateway: boolean = false;
  private conidCache: Map<string, number> = new Map();
  private sessionValid: boolean = false;

  /**
   * Get the API base URL based on mode
   */
  private getApiBase(): string {
    return this.useGateway ? IBKR_GATEWAY_BASE : IBKR_API_BASE;
  }

  /**
   * Check if current account is paper trading
   */
  isPaperTrading(): boolean {
    return this.accountId.startsWith('DU');
  }

  /**
   * Set whether to use Gateway mode (localhost) or OAuth mode (api.ibkr.com)
   */
  setGatewayMode(useGateway: boolean): void {
    this.useGateway = useGateway;
  }

  /**
   * Set the active account ID
   */
  setAccountId(accountId: string): void {
    this.accountId = accountId;
  }

  // ============================================================================
  // AUTHENTICATION
  // ============================================================================

  async authenticate(credentials: BrokerCredentials): Promise<AuthResponse> {
    try {
      // Store credentials
      await this.storeCredentials(credentials);

      // For Gateway mode, check session status
      if (this.useGateway) {
        const result = await invoke<{ success: boolean; data?: IBKRAuthStatus }>('ibkr_get_auth_status', {
          useGateway: this.useGateway,
        });

        if (result.success && result.data?.authenticated) {
          this.sessionValid = true;
          this._isConnected = true;
          // Get accounts and set first one as active
          const accounts = await this.getAccounts();
          if (accounts.length > 0) {
            this.accountId = accounts[0].id;
          }
          return {
            success: true,
            message: 'Connected to IBKR Gateway',
            userId: this.accountId,
          };
        }
        return {
          success: false,
          message: 'IBKR Gateway not authenticated. Please log in via the gateway.',
        };
      }

      // For OAuth mode, validate token
      if (credentials.accessToken) {
        const result = await invoke<{ success: boolean; data?: { validated: boolean } }>('ibkr_validate_sso', {
          accessToken: credentials.accessToken,
        });

        if (result.success && result.data?.validated) {
          this.sessionValid = true;
          this._isConnected = true;
          this.accessToken = credentials.accessToken;
          // Initialize brokerage session
          await this.initBrokerageSession();
          return {
            success: true,
            message: 'Connected to Interactive Brokers',
            userId: this.accountId,
            accessToken: credentials.accessToken,
          };
        }
      }

      return {
        success: false,
        message: 'Authentication failed. Please check your credentials.',
      };
    } catch (error) {
      console.error('[IBKRAdapter] Authentication failed:', error);
      return {
        success: false,
        message: error instanceof Error ? error.message : 'Authentication failed',
      };
    }
  }

  /**
   * Initialize brokerage session (required for trading endpoints)
   */
  private async initBrokerageSession(): Promise<boolean> {
    try {
      const result = await invoke<{ success: boolean }>('ibkr_init_session', {
        useGateway: this.useGateway,
      });
      return result.success;
    } catch (error) {
      console.error('[IBKRAdapter] Failed to init brokerage session:', error);
      return false;
    }
  }

  /**
   * Get available accounts
   */
  async getAccounts(): Promise<Array<{ id: string; name: string; type: string }>> {
    try {
      const result = await invoke<{ success: boolean; data?: Array<{ id: string; accountId: string; accountTitle: string; type: string }> }>('ibkr_get_accounts', {
        useGateway: this.useGateway,
      });

      if (result.success && result.data) {
        return result.data.map(acc => ({
          id: acc.accountId || acc.id,
          name: acc.accountTitle || acc.accountId,
          type: (acc.accountId || acc.id).startsWith('DU') ? 'paper' : 'live',
        }));
      }
      return [];
    } catch (error) {
      console.error('[IBKRAdapter] Failed to get accounts:', error);
      return [];
    }
  }

  // ============================================================================
  // ORDERS - Internal Implementations
  // ============================================================================

  protected async placeOrderInternal(params: OrderParams): Promise<OrderResponse> {
    try {
      const conid = await this.getConidForSymbol(params.symbol, params.exchange);
      if (!conid) {
        return { success: false, message: `Could not find contract for symbol: ${params.symbol}` };
      }

      const orderRequest = {
        acctId: this.accountId,
        conid: conid,
        orderType: this.mapOrderType(params.orderType),
        side: params.side,
        quantity: params.quantity,
        price: params.price,
        tif: this.mapValidity(params.validity),
      };

      const result = await invoke<{ success: boolean; data?: { order_id: string; order_status: string }; error?: string }>('ibkr_place_order', {
        accountId: this.accountId,
        order: orderRequest,
        useGateway: this.useGateway,
      });

      if (result.success && result.data) {
        return {
          success: true,
          orderId: result.data.order_id,
          message: `Order placed: ${result.data.order_status}`,
        };
      }

      return { success: false, message: result.error || 'Failed to place order' };
    } catch (error) {
      console.error('[IBKRAdapter] Place order failed:', error);
      return { success: false, message: error instanceof Error ? error.message : 'Place order failed' };
    }
  }

  protected async modifyOrderInternal(params: ModifyOrderParams): Promise<OrderResponse> {
    try {
      const result = await invoke<{ success: boolean; data?: IBKROrder; error?: string }>('ibkr_modify_order', {
        accountId: this.accountId,
        orderId: params.orderId,
        params: {
          quantity: params.quantity,
          price: params.price,
          auxPrice: params.triggerPrice,
        },
        useGateway: this.useGateway,
      });

      if (result.success) {
        return { success: true, orderId: params.orderId, message: 'Order modified' };
      }

      return { success: false, message: result.error || 'Failed to modify order' };
    } catch (error) {
      console.error('[IBKRAdapter] Modify order failed:', error);
      return { success: false, message: error instanceof Error ? error.message : 'Modify order failed' };
    }
  }

  protected async cancelOrderInternal(orderId: string): Promise<OrderResponse> {
    try {
      const result = await invoke<{ success: boolean; error?: string }>('ibkr_cancel_order', {
        accountId: this.accountId,
        orderId: orderId,
        useGateway: this.useGateway,
      });

      return {
        success: result.success,
        orderId: orderId,
        message: result.success ? 'Order cancelled' : (result.error || 'Failed to cancel order'),
      };
    } catch (error) {
      console.error('[IBKRAdapter] Cancel order failed:', error);
      return { success: false, message: error instanceof Error ? error.message : 'Cancel order failed' };
    }
  }

  protected async getOrdersInternal(): Promise<Order[]> {
    try {
      const result = await invoke<{ success: boolean; data?: IBKROrder[] }>('ibkr_get_orders', {
        useGateway: this.useGateway,
      });

      if (result.success && result.data) {
        return result.data.map(order => this.mapIBKROrderToOrder(order));
      }

      return [];
    } catch (error) {
      console.error('[IBKRAdapter] Get orders failed:', error);
      return [];
    }
  }

  protected async getTradeBookInternal(): Promise<Trade[]> {
    try {
      const result = await invoke<{ success: boolean; data?: IBKROrder[] }>('ibkr_get_orders', {
        useGateway: this.useGateway,
      });

      if (result.success && result.data) {
        // Filter to filled orders and convert to trades
        return result.data
          .filter(o => o.status === 'Filled')
          .map(order => this.mapIBKROrderToTrade(order));
      }

      return [];
    } catch (error) {
      console.error('[IBKRAdapter] Get trade book failed:', error);
      return [];
    }
  }

  // ============================================================================
  // POSITIONS & HOLDINGS - Internal Implementations
  // ============================================================================

  protected async getPositionsInternal(): Promise<Position[]> {
    try {
      const result = await invoke<{ success: boolean; data?: IBKRPosition[] }>('ibkr_get_positions', {
        accountId: this.accountId,
        useGateway: this.useGateway,
      });

      if (result.success && result.data) {
        return result.data
          .filter(p => p.position !== 0)
          .map(pos => this.mapIBKRPositionToPosition(pos));
      }

      return [];
    } catch (error) {
      console.error('[IBKRAdapter] Get positions failed:', error);
      return [];
    }
  }

  protected async getHoldingsInternal(): Promise<Holding[]> {
    // IBKR doesn't distinguish between positions and holdings
    const positions = await this.getPositionsInternal();
    return positions.map(pos => ({
      symbol: pos.symbol,
      exchange: pos.exchange,
      quantity: Math.abs(pos.quantity),
      averagePrice: pos.averagePrice,
      lastPrice: pos.lastPrice,
      investedValue: Math.abs(pos.quantity) * pos.averagePrice,
      currentValue: Math.abs(pos.quantity) * pos.lastPrice,
      pnl: pos.pnl,
      pnlPercent: pos.pnlPercent,
    }));
  }

  protected async getFundsInternal(): Promise<Funds> {
    try {
      const result = await invoke<{ success: boolean; data?: IBKRAccountSummary }>('ibkr_get_account_summary', {
        accountId: this.accountId,
        useGateway: this.useGateway,
      });

      if (result.success && result.data) {
        const summary = result.data;
        return {
          availableCash: summary.availablefunds || 0,
          usedMargin: summary.initmarginreq || 0,
          availableMargin: summary.availablefunds || 0,
          totalBalance: summary.netliquidation || 0,
          currency: 'USD' as Currency,
        };
      }

      return {
        availableCash: 0,
        usedMargin: 0,
        availableMargin: 0,
        totalBalance: 0,
        currency: 'USD' as Currency,
      };
    } catch (error) {
      console.error('[IBKRAdapter] Get funds failed:', error);
      return {
        availableCash: 0,
        usedMargin: 0,
        availableMargin: 0,
        totalBalance: 0,
        currency: 'USD' as Currency,
      };
    }
  }

  // ============================================================================
  // MARKET DATA - Internal Implementations
  // ============================================================================

  protected async getQuoteInternal(symbol: string, exchange: StockExchange): Promise<Quote> {
    try {
      const conid = await this.getConidForSymbol(symbol, exchange);
      if (!conid) {
        throw new Error(`Could not find contract for symbol: ${symbol}`);
      }

      const result = await invoke<{ success: boolean; data?: Record<string, IBKRMarketDataSnapshot> }>('ibkr_get_snapshot', {
        conids: [conid],
        fields: IBKR_DEFAULT_SNAPSHOT_FIELDS,
        useGateway: this.useGateway,
      });

      if (result.success && result.data && result.data[conid.toString()]) {
        return this.mapIBKRSnapshotToQuote(symbol, exchange, result.data[conid.toString()]);
      }

      throw new Error(`No quote data for ${symbol}`);
    } catch (error) {
      console.error('[IBKRAdapter] Get quote failed:', error);
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
      const conid = await this.getConidForSymbol(symbol, exchange);
      if (!conid) {
        throw new Error(`Could not find contract for symbol: ${symbol}`);
      }

      const barSize = this.mapTimeframeToBarSize(timeframe);
      const duration = this.calculateDuration(from, to);

      const result = await invoke<{ success: boolean; data?: IBKRHistoricalData }>('ibkr_get_historical_data', {
        conid: conid,
        period: duration,
        bar: barSize,
        outsideRth: false,
        useGateway: this.useGateway,
      });

      if (result.success && result.data?.data) {
        return result.data.data.map((bar: { t: number; o: number; h: number; l: number; c: number; v: number }) => ({
          timestamp: bar.t * 1000,
          open: bar.o,
          high: bar.h,
          low: bar.l,
          close: bar.c,
          volume: bar.v,
        }));
      }

      return [];
    } catch (error) {
      console.error('[IBKRAdapter] Get OHLCV failed:', error);
      return [];
    }
  }

  protected async getMarketDepthInternal(symbol: string, exchange: StockExchange): Promise<MarketDepth> {
    // IBKR provides market depth via WebSocket
    // For REST, we return basic bid/ask from snapshot
    const quote = await this.getQuoteInternal(symbol, exchange);

    return {
      bids: [{ price: quote.bid, quantity: quote.bidQty, orders: 1 }],
      asks: [{ price: quote.ask, quantity: quote.askQty, orders: 1 }],
      timestamp: quote.timestamp,
    };
  }

  // ============================================================================
  // WEBSOCKET - Internal Implementations
  // ============================================================================

  protected async connectWebSocketInternal(config: WebSocketConfig): Promise<void> {
    try {
      await invoke('ibkr_ws_connect', { useGateway: this.useGateway });
    } catch (error) {
      console.error('[IBKRAdapter] WebSocket connect failed:', error);
      throw error;
    }
  }

  protected async subscribeInternal(symbol: string, exchange: StockExchange, mode: SubscriptionMode): Promise<void> {
    try {
      const conid = await this.getConidForSymbol(symbol, exchange);
      if (!conid) return;

      await invoke('ibkr_ws_subscribe_market_data', {
        conids: [conid],
        fields: IBKR_DEFAULT_SNAPSHOT_FIELDS,
      });
    } catch (error) {
      console.error('[IBKRAdapter] Subscribe failed:', error);
    }
  }

  protected async unsubscribeInternal(symbol: string, exchange: StockExchange): Promise<void> {
    try {
      const conid = await this.getConidForSymbol(symbol, exchange);
      if (!conid) return;

      await invoke('ibkr_ws_unsubscribe_market_data', { conids: [conid] });
    } catch (error) {
      console.error('[IBKRAdapter] Unsubscribe failed:', error);
    }
  }

  // ============================================================================
  // BULK OPERATIONS - Internal Implementations
  // ============================================================================

  protected async cancelAllOrdersInternal(): Promise<BulkOperationResult> {
    try {
      const orders = await this.getOrdersInternal();
      const openOrders = orders.filter(o => o.status === 'OPEN' || o.status === 'PENDING');

      let successCount = 0;
      let failedCount = 0;
      const results: Array<{ success: boolean; orderId?: string; error?: string }> = [];

      for (const order of openOrders) {
        const result = await this.cancelOrderInternal(order.orderId);
        if (result.success) {
          successCount++;
          results.push({ success: true, orderId: order.orderId });
        } else {
          failedCount++;
          results.push({ success: false, orderId: order.orderId, error: result.message });
        }
      }

      return {
        success: failedCount === 0,
        totalCount: openOrders.length,
        successCount,
        failedCount,
        results,
      };
    } catch (error) {
      console.error('[IBKRAdapter] Cancel all orders failed:', error);
      return { success: false, totalCount: 0, successCount: 0, failedCount: 0, results: [] };
    }
  }

  protected async closeAllPositionsInternal(): Promise<BulkOperationResult> {
    try {
      const positions = await this.getPositionsInternal();
      let successCount = 0;
      let failedCount = 0;
      const results: Array<{ success: boolean; symbol?: string; error?: string }> = [];

      for (const position of positions) {
        const closeOrder: OrderParams = {
          symbol: position.symbol,
          exchange: position.exchange,
          side: position.quantity > 0 ? 'SELL' : 'BUY',
          orderType: 'MARKET',
          quantity: Math.abs(position.quantity),
          productType: 'CASH',
        };

        const result = await this.placeOrderInternal(closeOrder);
        if (result.success) {
          successCount++;
          results.push({ success: true, symbol: position.symbol });
        } else {
          failedCount++;
          results.push({ success: false, symbol: position.symbol, error: result.message });
        }
      }

      return {
        success: failedCount === 0,
        totalCount: positions.length,
        successCount,
        failedCount,
        results,
      };
    } catch (error) {
      console.error('[IBKRAdapter] Close all positions failed:', error);
      return { success: false, totalCount: 0, successCount: 0, failedCount: 0, results: [] };
    }
  }

  // ============================================================================
  // SMART ORDER - Internal Implementation
  // ============================================================================

  protected async placeSmartOrderInternal(params: SmartOrderParams): Promise<OrderResponse> {
    // For IBKR, smart order is basically a regular order with SMART routing
    return this.placeOrderInternal({
      ...params,
      exchange: 'SMART' as StockExchange,
    });
  }

  // ============================================================================
  // MARGIN CALCULATOR - Internal Implementation
  // ============================================================================

  protected async calculateMarginInternal(orders: OrderParams[]): Promise<MarginRequired> {
    // IBKR requires using What-If endpoint for margin calculation
    let totalMargin = 0;
    let initialMargin = 0;

    for (const order of orders) {
      try {
        const conid = await this.getConidForSymbol(order.symbol, order.exchange);
        if (!conid) continue;

        const result = await invoke<{ success: boolean; data?: { initMargin: number; maintMargin: number } }>('ibkr_get_margin_impact', {
          accountId: this.accountId,
          conid: conid,
          quantity: order.quantity,
          side: order.side,
          useGateway: this.useGateway,
        });

        if (result.success && result.data) {
          initialMargin += result.data.initMargin || 0;
          totalMargin += result.data.maintMargin || 0;
        }
      } catch {
        // Skip if margin calc fails for one order
      }
    }

    return {
      totalMargin,
      initialMargin,
    };
  }

  // ============================================================================
  // SYMBOL SEARCH
  // ============================================================================

  async searchSymbols(query: string, exchange?: StockExchange): Promise<Instrument[]> {
    try {
      const result = await invoke<{ success: boolean; data?: IBKRSecDefSearch[] }>('ibkr_search_contracts', {
        query: query,
        useGateway: this.useGateway,
      });

      if (result.success && result.data) {
        return result.data.map(contract => ({
          symbol: contract.symbol,
          name: contract.companyName || contract.symbol,
          exchange: (contract.sections?.[0]?.exchange || 'SMART') as StockExchange,
          instrumentType: this.mapSecType(contract.sections?.[0]?.secType),
          currency: 'USD' as Currency,
          token: contract.conid.toString(),
          lotSize: 1,
          tickSize: 0.01,
        }));
      }

      return [];
    } catch (error) {
      console.error('[IBKRAdapter] Search symbols failed:', error);
      return [];
    }
  }

  async getInstrument(symbol: string, exchange: StockExchange): Promise<Instrument | null> {
    const results = await this.searchSymbols(symbol, exchange);
    return results.find(i => i.symbol.toUpperCase() === symbol.toUpperCase()) || null;
  }

  // ============================================================================
  // HELPER METHODS
  // ============================================================================

  /**
   * Get conid for a symbol (cached)
   */
  private async getConidForSymbol(symbol: string, exchange?: StockExchange): Promise<number | null> {
    const cacheKey = `${symbol}:${exchange || 'SMART'}`;

    if (this.conidCache.has(cacheKey)) {
      return this.conidCache.get(cacheKey)!;
    }

    try {
      const result = await invoke<{ success: boolean; data?: IBKRSecDefSearch[] }>('ibkr_search_contracts', {
        query: symbol,
        useGateway: this.useGateway,
      });

      if (result.success && result.data && result.data.length > 0) {
        const match = result.data.find(c => c.symbol.toUpperCase() === symbol.toUpperCase());
        const conid = match?.conid || result.data[0].conid;
        this.conidCache.set(cacheKey, conid);
        return conid;
      }

      return null;
    } catch (error) {
      console.error('[IBKRAdapter] Get conid failed:', error);
      return null;
    }
  }

  private mapOrderType(orderType: string): string {
    const mapping: Record<string, string> = {
      'MARKET': 'MKT',
      'LIMIT': 'LMT',
      'STOP': 'STP',
      'STOP_LIMIT': 'STP LMT',
      'TRAILING_STOP': 'TRAIL',
    };
    return mapping[orderType] || 'MKT';
  }

  private mapValidity(validity?: string): string {
    const mapping: Record<string, string> = {
      'DAY': 'DAY',
      'GTC': 'GTC',
      'IOC': 'IOC',
      'GTD': 'GTD',
    };
    return mapping[validity || 'DAY'] || 'DAY';
  }

  private mapTimeframeToBarSize(timeframe: TimeFrame): string {
    const mapping: Record<string, string> = {
      '1m': '1min',
      '5m': '5mins',
      '15m': '15mins',
      '30m': '30mins',
      '1h': '1hour',
      '4h': '4hours',
      '1d': '1day',
      '1w': '1week',
      '1M': '1month',
    };
    return mapping[timeframe] || '1day';
  }

  private calculateDuration(from: Date, to: Date): string {
    const diffDays = Math.ceil((to.getTime() - from.getTime()) / (1000 * 60 * 60 * 24));
    if (diffDays <= 7) return '1w';
    if (diffDays <= 30) return '1M';
    if (diffDays <= 90) return '3M';
    if (diffDays <= 180) return '6M';
    return '1Y';
  }

  private mapSecType(secType?: string): InstrumentType {
    const mapping: Record<string, InstrumentType> = {
      'STK': 'EQUITY',
      'ETF': 'ETF',
      'OPT': 'OPTION',
      'FUT': 'FUTURE',
      'BOND': 'BOND',
    };
    return mapping[secType || ''] || 'EQUITY';
  }

  private mapIBKROrderToOrder(ibkrOrder: IBKROrder): Order {
    return {
      orderId: ibkrOrder.orderId?.toString() || '',
      symbol: ibkrOrder.ticker || '',
      exchange: (ibkrOrder.listingExchange || 'SMART') as StockExchange,
      side: ibkrOrder.side === 'BUY' ? 'BUY' : 'SELL',
      quantity: ibkrOrder.totalSize || 0,
      filledQuantity: ibkrOrder.filledQuantity || 0,
      pendingQuantity: (ibkrOrder.totalSize || 0) - (ibkrOrder.filledQuantity || 0),
      price: ibkrOrder.price || 0,
      averagePrice: ibkrOrder.avgPrice || 0,
      orderType: this.reverseMapOrderType(ibkrOrder.orderType),
      productType: 'CASH',
      validity: this.reverseMapValidity(ibkrOrder.timeInForce),
      status: this.mapIBKRStatus(ibkrOrder.status),
      placedAt: new Date(ibkrOrder.lastExecutionTime_r || Date.now()),
    };
  }

  private mapIBKROrderToTrade(ibkrOrder: IBKROrder): Trade {
    return {
      tradeId: ibkrOrder.orderId?.toString() || '',
      orderId: ibkrOrder.orderId?.toString() || '',
      symbol: ibkrOrder.ticker || '',
      exchange: (ibkrOrder.listingExchange || 'SMART') as StockExchange,
      side: ibkrOrder.side === 'BUY' ? 'BUY' : 'SELL',
      quantity: ibkrOrder.filledQuantity || 0,
      price: ibkrOrder.avgPrice || 0,
      productType: 'CASH',
      tradedAt: new Date(ibkrOrder.lastExecutionTime_r || Date.now()),
    };
  }

  private mapIBKRPositionToPosition(ibkrPos: IBKRPosition): Position {
    const quantity = ibkrPos.position || 0;
    const avgPrice = ibkrPos.avgCost || 0;
    const lastPrice = ibkrPos.mktPrice || 0;
    const pnl = ibkrPos.unrealizedPnl || 0;
    const pnlPercent = avgPrice !== 0 ? (pnl / (Math.abs(quantity) * avgPrice)) * 100 : 0;

    return {
      symbol: ibkrPos.contractDesc || '',
      exchange: 'SMART' as StockExchange,
      productType: 'CASH',
      quantity: quantity,
      buyQuantity: quantity > 0 ? Math.abs(quantity) : 0,
      sellQuantity: quantity < 0 ? Math.abs(quantity) : 0,
      buyValue: quantity > 0 ? Math.abs(quantity) * avgPrice : 0,
      sellValue: quantity < 0 ? Math.abs(quantity) * avgPrice : 0,
      averagePrice: avgPrice,
      lastPrice: lastPrice,
      pnl: pnl,
      pnlPercent: pnlPercent,
      dayPnl: ibkrPos.realizedPnl || 0,
    };
  }

  private mapIBKRSnapshotToQuote(symbol: string, exchange: StockExchange, snapshot: IBKRMarketDataSnapshot[string]): Quote {
    return {
      symbol,
      exchange,
      lastPrice: snapshot['31'] ? parseFloat(snapshot['31']) : 0,
      open: snapshot['7294'] ? parseFloat(snapshot['7294']) : 0,
      high: snapshot['70'] ? parseFloat(snapshot['70']) : 0,
      low: snapshot['71'] ? parseFloat(snapshot['71']) : 0,
      close: snapshot['31'] ? parseFloat(snapshot['31']) : 0,
      previousClose: snapshot['7762'] ? parseFloat(snapshot['7762']) : 0,
      change: snapshot['82'] ? parseFloat(snapshot['82']) : 0,
      changePercent: snapshot['83'] ? parseFloat(snapshot['83']) : 0,
      volume: snapshot['87'] ? parseInt(snapshot['87']) : 0,
      bid: snapshot['84'] ? parseFloat(snapshot['84']) : 0,
      bidQty: snapshot['88'] ? parseInt(snapshot['88']) : 0,
      ask: snapshot['86'] ? parseFloat(snapshot['86']) : 0,
      askQty: snapshot['85'] ? parseInt(snapshot['85']) : 0,
      timestamp: Date.now(),
    };
  }

  private reverseMapOrderType(ibkrType?: string): 'MARKET' | 'LIMIT' | 'STOP' | 'STOP_LIMIT' | 'TRAILING_STOP' {
    const mapping: Record<string, 'MARKET' | 'LIMIT' | 'STOP' | 'STOP_LIMIT' | 'TRAILING_STOP'> = {
      'MKT': 'MARKET',
      'LMT': 'LIMIT',
      'STP': 'STOP',
      'STP LMT': 'STOP_LIMIT',
      'TRAIL': 'TRAILING_STOP',
    };
    return mapping[ibkrType || ''] || 'MARKET';
  }

  private reverseMapValidity(tif?: string): 'DAY' | 'GTC' | 'IOC' | 'GTD' {
    const mapping: Record<string, 'DAY' | 'GTC' | 'IOC' | 'GTD'> = {
      'DAY': 'DAY',
      'GTC': 'GTC',
      'IOC': 'IOC',
      'GTD': 'GTD',
    };
    return mapping[tif || ''] || 'DAY';
  }

  private mapIBKRStatus(status?: string): 'PENDING' | 'OPEN' | 'PARTIALLY_FILLED' | 'FILLED' | 'CANCELLED' | 'REJECTED' | 'EXPIRED' | 'TRIGGER_PENDING' {
    const mapping: Record<string, 'PENDING' | 'OPEN' | 'PARTIALLY_FILLED' | 'FILLED' | 'CANCELLED' | 'REJECTED' | 'EXPIRED' | 'TRIGGER_PENDING'> = {
      'PreSubmitted': 'PENDING',
      'Submitted': 'OPEN',
      'Filled': 'FILLED',
      'Cancelled': 'CANCELLED',
      'Inactive': 'CANCELLED',
      'PendingSubmit': 'PENDING',
      'PendingCancel': 'PENDING',
    };
    return mapping[status || ''] || 'PENDING';
  }
}

// Export singleton instance
export const ibkrAdapter = new IBKRAdapter();
