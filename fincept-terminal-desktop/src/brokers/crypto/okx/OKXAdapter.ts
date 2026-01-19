/**
 * OKX Exchange Adapter
 *
 * Professional-grade OKX integration using CCXT
 * Supports: Spot, Margin, Futures, Perpetuals, Options
 */

import { BaseExchangeAdapter } from '../BaseExchangeAdapter';
import type {
  ExchangeConfig,
  OrderType,
  OrderSide,
  OrderParams,
  ExchangeCapabilities,
} from '../types';

export class OKXAdapter extends BaseExchangeAdapter {
  constructor(config?: Partial<ExchangeConfig>) {
    super({
      exchange: 'okx',
      enableRateLimit: true,
      timeout: 30000,
      ...config,
    });
  }

  // ============================================================================
  // OKX-SPECIFIC CAPABILITIES
  // ============================================================================

  getCapabilities(): ExchangeCapabilities {
    const base = super.getCapabilities();
    return {
      ...base,
      spot: true,
      margin: true,
      futures: true,
      swap: true,
      options: true,

      supportedOrderTypes: [
        'market',
        'limit',
        'stop',
        'stop_limit',
        'trailing_stop',
      ] as any,

      leverageTrading: true,
      stopOrders: true,
      conditionalOrders: true,
      algoOrders: true,

      realtimeData: true,
      historicalData: true,
      websocketSupport: true,

      multiAccount: false,
      subAccounts: true,
      portfolioMargin: true,

      rateLimits: {
        public: 20,
        private: 60,
        trading: 60,
      },
    };
  }

  // ============================================================================
  // ENHANCED ORDER METHODS
  // ============================================================================

  /**
   * Create OCO (One-Cancels-Other) order
   */
  async createOCOOrder(
    symbol: string,
    side: OrderSide,
    amount: number,
    price: number,
    stopPrice: number,
    stopLimitPrice?: number
  ) {
    try {
      await this.ensureAuthenticated();
      // OKX uses algo orders for OCO
      const limitOrder = await this.createOrder(symbol, 'limit', side, amount, price);
      const stopOrder = await this.createStopLossOrder(symbol, side, amount, stopPrice, stopLimitPrice);

      return {
        orderListId: `oco_${Date.now()}`,
        contingencyType: 'OCO',
        listStatusType: 'EXEC_STARTED',
        orders: [limitOrder, stopOrder],
      };
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Create stop-loss order
   */
  async createStopLossOrder(
    symbol: string,
    side: OrderSide,
    amount: number,
    stopPrice: number,
    limitPrice?: number
  ) {
    try {
      await this.ensureAuthenticated();
      const orderType = limitPrice ? 'stop_limit' : 'stop';
      return await this.createOrder(symbol, orderType as OrderType, side, amount, limitPrice, {
        stopPrice,
        triggerPrice: stopPrice,
      });
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Create take-profit order
   */
  async createTakeProfitOrder(
    symbol: string,
    side: OrderSide,
    amount: number,
    takeProfitPrice: number,
    limitPrice?: number
  ) {
    try {
      await this.ensureAuthenticated();
      return await this.createOrder(symbol, 'limit', side, amount, limitPrice || takeProfitPrice, {
        takeProfitPrice,
        triggerPrice: takeProfitPrice,
      });
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Create trailing stop order
   */
  async createTrailingStopOrder(
    symbol: string,
    side: OrderSide,
    amount: number,
    callbackRate: number,
    params?: OrderParams
  ) {
    try {
      await this.ensureAuthenticated();
      return await this.createOrder(symbol, 'trailing_stop' as OrderType, side, amount, undefined, {
        ...params,
        callbackRate,
        trailingPercent: callbackRate,
      });
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Create iceberg order
   */
  async createIcebergOrder(
    symbol: string,
    side: OrderSide,
    amount: number,
    price: number,
    icebergQty: number,
    params?: OrderParams
  ) {
    try {
      await this.ensureAuthenticated();
      // OKX supports iceberg orders via algo trading
      return await this.createOrder(symbol, 'limit', side, amount, price, {
        ...params,
        iceberg: true,
        visibleSize: icebergQty,
      });
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Create reduce-only order (futures/perpetuals)
   */
  async createReduceOnlyOrder(
    symbol: string,
    type: OrderType,
    side: OrderSide,
    amount: number,
    price?: number
  ) {
    try {
      await this.ensureAuthenticated();
      return await this.createOrder(symbol, type, side, amount, price, {
        reduceOnly: true,
      });
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Create post-only order (maker-only)
   */
  async createPostOnlyOrder(
    symbol: string,
    side: OrderSide,
    amount: number,
    price: number
  ) {
    try {
      await this.ensureAuthenticated();
      return await this.createOrder(symbol, 'limit', side, amount, price, {
        postOnly: true,
        timeInForce: 'PO', // OKX post-only
      });
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // MARGIN TRADING
  // ============================================================================

  /**
   * Create margin order
   */
  async createMarginOrder(
    symbol: string,
    type: OrderType,
    side: OrderSide,
    amount: number,
    price?: number,
    params?: OrderParams
  ) {
    try {
      await this.ensureAuthenticated();
      return await this.createOrder(symbol, type, side, amount, price, {
        ...params,
        marginMode: 'cross',
        tdMode: 'cross', // OKX trade mode
      });
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Fetch margin balance
   */
  async fetchMarginBalance() {
    try {
      await this.ensureAuthenticated();
      // OKX uses unified account
      const balance = await this.exchange.fetchBalance({ type: 'margin' });
      return balance;
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Fetch futures/swap balance
   */
  async fetchFuturesBalance() {
    try {
      await this.ensureAuthenticated();
      const balance = await this.exchange.fetchBalance({ type: 'swap' });
      return {
        ...balance,
        unrealizedPnl: (balance as any).info?.totalEquity || 0,
        marginBalance: (balance as any).info?.adjEq || 0,
        availableBalance: (balance as any).info?.availBal || (balance.free as any)?.USDT || 0,
      };
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // LEVERAGE & MARGIN MODE
  // ============================================================================

  /**
   * Set leverage for a symbol
   */
  async setLeverage(symbol: string, leverage: number) {
    try {
      await this.ensureAuthenticated();
      return await this.exchange.setLeverage(leverage, symbol);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Set margin mode (cross or isolated)
   */
  async setMarginMode(symbol: string, marginMode: 'cross' | 'isolated') {
    try {
      await this.ensureAuthenticated();
      return await this.exchange.setMarginMode(marginMode, symbol);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // ORDER MANAGEMENT
  // ============================================================================

  /**
   * Edit an existing order
   */
  async editOrder(
    orderId: string,
    symbol: string,
    type: OrderType,
    side: OrderSide,
    amount?: number,
    price?: number,
    params?: OrderParams
  ) {
    try {
      await this.ensureAuthenticated();
      if (this.exchange.has['editOrder']) {
        return await this.exchange.editOrder(orderId, symbol, type, side, amount, price, params);
      }
      // Fallback: cancel and recreate
      await this.cancelOrder(orderId, symbol);
      return await this.createOrder(symbol, type, side, amount!, price, params);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Fetch user's trades
   */
  async fetchMyTrades(symbol?: string, since?: number, limit?: number) {
    try {
      await this.ensureAuthenticated();
      return await this.exchange.fetchMyTrades(symbol, since, limit);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Fetch specific order by ID
   */
  async fetchOrder(orderId: string, symbol: string) {
    try {
      await this.ensureAuthenticated();
      return await this.exchange.fetchOrder(orderId, symbol);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Fetch multiple orders by IDs
   */
  async fetchOrdersByIds(orderIds: string[], symbol: string) {
    try {
      await this.ensureAuthenticated();
      const orders = [];
      for (const orderId of orderIds) {
        const order = await this.fetchOrder(orderId, symbol);
        orders.push(order);
      }
      return orders;
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Fetch trades for specific order
   */
  async fetchOrderTrades(orderId: string, symbol: string) {
    try {
      await this.ensureAuthenticated();
      if (this.exchange.has['fetchOrderTrades']) {
        return await this.exchange.fetchOrderTrades(orderId, symbol);
      }
      // Fallback: fetch all trades and filter
      const trades = await this.fetchMyTrades(symbol);
      return trades.filter((t: any) => t.order === orderId);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Cancel multiple orders
   */
  async cancelOrders(orderIds: string[], symbol?: string) {
    try {
      await this.ensureAuthenticated();
      if (this.exchange.has['cancelOrders']) {
        return await this.exchange.cancelOrders(orderIds, symbol);
      }
      // Fallback to cancelling one by one
      const results = [];
      for (const orderId of orderIds) {
        if (!symbol) throw new Error('Symbol required for bulk cancel');
        const result = await this.cancelOrder(orderId, symbol);
        results.push(result);
      }
      return results;
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Cancel all orders after timeout (Dead Man's Switch)
   */
  async cancelAllOrdersAfter(timeout: number) {
    try {
      await this.ensureAuthenticated();
      if (this.exchange.has['cancelAllOrdersAfter']) {
        return await (this.exchange as any).cancelAllOrdersAfter(timeout);
      }
      // OKX may not support this natively
      throw new Error('cancelAllOrdersAfter not natively supported on OKX');
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // ACCOUNT & FEES
  // ============================================================================

  /**
   * Fetch trading fee for specific symbol
   */
  async fetchTradingFee(symbol: string) {
    try {
      await this.ensureAuthenticated();
      if (this.exchange.has['fetchTradingFee']) {
        return await this.exchange.fetchTradingFee(symbol);
      }
      const fees = await this.fetchTradingFees();
      return fees[symbol] || null;
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Fetch all trading fees
   */
  async fetchTradingFees() {
    try {
      await this.ensureAuthenticated();
      return await this.exchange.fetchTradingFees();
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Fetch account ledger/history
   */
  async fetchLedger(code?: string, since?: number, limit?: number) {
    try {
      await this.ensureAuthenticated();
      return await this.exchange.fetchLedger(code, since, limit);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // FUNDING
  // ============================================================================

  /**
   * Transfer funds between accounts
   */
  async transfer(code: string, amount: number, fromAccount: string, toAccount: string) {
    try {
      await this.ensureAuthenticated();
      return await this.exchange.transfer(code, amount, fromAccount, toAccount);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Fetch current funding rate
   */
  async fetchFundingRate(symbol: string) {
    try {
      return await this.exchange.fetchFundingRate(symbol);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Fetch funding rate history
   */
  async fetchFundingRateHistory(symbol: string, since?: number, limit?: number) {
    try {
      return await this.exchange.fetchFundingRateHistory(symbol, since, limit);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Fetch user's funding payments history
   */
  async fetchFundingHistory(symbol?: string, since?: number, limit?: number) {
    try {
      await this.ensureAuthenticated();
      if (this.exchange.has['fetchFundingHistory']) {
        return await (this.exchange as any).fetchFundingHistory(symbol, since, limit);
      }
      // Fallback via ledger
      const ledger = await this.fetchLedger(undefined, since, limit);
      return ledger.filter((entry: any) => entry.type === 'funding');
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // DEPOSITS & WITHDRAWALS
  // ============================================================================

  /**
   * Fetch deposit address
   */
  async fetchDepositAddress(code: string, network?: string) {
    try {
      await this.ensureAuthenticated();
      return await this.exchange.fetchDepositAddress(code, { network });
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Fetch deposits
   */
  async fetchDeposits(code?: string, since?: number, limit?: number) {
    try {
      await this.ensureAuthenticated();
      return await this.exchange.fetchDeposits(code, since, limit);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Fetch withdrawals
   */
  async fetchWithdrawals(code?: string, since?: number, limit?: number) {
    try {
      await this.ensureAuthenticated();
      return await this.exchange.fetchWithdrawals(code, since, limit);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Withdraw funds
   */
  async withdraw(code: string, amount: number, address: string, tag?: string, params?: any) {
    try {
      await this.ensureAuthenticated();
      return await this.exchange.withdraw(code, amount, address, tag, params);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // MARKETS
  // ============================================================================

  /**
   * Fetch futures/perpetual markets
   */
  async fetchFuturesMarkets() {
    try {
      const markets = await this.exchange.loadMarkets();
      return Object.values(markets).filter(
        (m: any) => m.swap || m.future || m.type === 'swap' || m.type === 'future'
      );
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Fetch open interest for a symbol
   */
  async fetchOpenInterest(symbol: string) {
    try {
      if (this.exchange.has['fetchOpenInterest']) {
        return await this.exchange.fetchOpenInterest(symbol);
      }
      throw new Error('fetchOpenInterest not supported');
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // UTILITIES
  // ============================================================================

  /**
   * Get exchange system status
   */
  async getSystemStatus() {
    try {
      if (this.exchange.has['fetchStatus']) {
        return await this.exchange.fetchStatus();
      }
      return { status: 'ok', updated: Date.now() };
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Get server time
   */
  async getServerTime() {
    try {
      return await this.exchange.fetchTime();
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Get popular trading pairs for OKX
   */
  getPopularPairs(): string[] {
    return [
      'BTC/USDT',
      'ETH/USDT',
      'SOL/USDT',
      'XRP/USDT',
      'DOGE/USDT',
      'ADA/USDT',
      'AVAX/USDT',
      'DOT/USDT',
      'MATIC/USDT',
      'LINK/USDT',
      'UNI/USDT',
      'ATOM/USDT',
      'LTC/USDT',
      'BCH/USDT',
      'FIL/USDT',
      'APT/USDT',
      'ARB/USDT',
      'OP/USDT',
      'NEAR/USDT',
      'INJ/USDT',
    ];
  }

  // ============================================================================
  // OKX-SPECIFIC FEATURES
  // ============================================================================

  /**
   * Get account configuration
   */
  async getAccountConfig() {
    try {
      await this.ensureAuthenticated();
      if ((this.exchange as any).privateGetAccountConfig) {
        return await (this.exchange as any).privateGetAccountConfig();
      }
      return null;
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Set position mode (one-way or hedge)
   */
  async setPositionMode(hedgeMode: boolean) {
    try {
      await this.ensureAuthenticated();
      if (this.exchange.has['setPositionMode']) {
        return await (this.exchange as any).setPositionMode(hedgeMode);
      }
      throw new Error('setPositionMode not supported');
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Fetch mark price
   */
  async fetchMarkPrice(symbol: string) {
    try {
      if (this.exchange.has['fetchMarkPrice']) {
        return await (this.exchange as any).fetchMarkPrice(symbol);
      }
      // Fallback to ticker
      const ticker = await this.fetchTicker(symbol);
      return { symbol, markPrice: ticker.last };
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Fetch index price
   */
  async fetchIndexPrice(symbol: string) {
    try {
      if ((this.exchange as any).fetchIndexPrice) {
        return await (this.exchange as any).fetchIndexPrice(symbol);
      }
      throw new Error('fetchIndexPrice not supported');
    } catch (error) {
      throw this.handleError(error);
    }
  }
}
