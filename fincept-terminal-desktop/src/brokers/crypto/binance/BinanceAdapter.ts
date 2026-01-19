/**
 * Binance Exchange Adapter
 *
 * Professional-grade Binance integration using CCXT
 * Supports: Spot, Margin, Futures, Options, Staking, Earn
 */

import { BaseExchangeAdapter } from '../BaseExchangeAdapter';
import type {
  ExchangeConfig,
  OrderType,
  OrderSide,
  OrderParams,
  ExchangeCapabilities,
} from '../types';

export class BinanceAdapter extends BaseExchangeAdapter {
  constructor(config?: Partial<ExchangeConfig>) {
    super({
      exchange: 'binance',
      enableRateLimit: true,
      timeout: 30000,
      ...config,
    });
  }

  // ============================================================================
  // BINANCE-SPECIFIC CAPABILITIES
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
      ],

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
        public: 1200,
        private: 600,
        trading: 100,
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
      return await (this.exchange as any).createOrder(symbol, 'limit', side, amount, price, {
        stopPrice,
        stopLimitPrice: stopLimitPrice || stopPrice,
        type: 'oco',
      });
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
    const type: OrderType = limitPrice ? 'stop_limit' : 'stop';
    return await this.createOrder(symbol, type, side, amount, limitPrice, {
      stopPrice,
    });
  }

  /**
   * Create take-profit order
   */
  async createTakeProfitOrder(
    symbol: string,
    side: OrderSide,
    amount: number,
    stopPrice: number,
    limitPrice?: number
  ) {
    const type: OrderType = limitPrice ? 'stop_limit' : 'stop';
    return await this.createOrder(symbol, type, side, amount, limitPrice, {
      stopPrice,
      type: 'TAKE_PROFIT',
    });
  }

  /**
   * Create trailing stop order
   */
  async createTrailingStopOrder(
    symbol: string,
    side: OrderSide,
    amount: number,
    callbackRate: number
  ) {
    try {
      await this.ensureAuthenticated();
      return await this.createOrder(symbol, 'trailing_stop', side, amount, undefined, {
        callbackRate,
      });
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Create iceberg order (split large order into smaller visible chunks)
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
      return await this.createOrder(symbol, 'limit', side, amount, price, {
        ...params,
        icebergQty,
      });
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Create reduce-only order (futures only)
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
   * Create post-only order (maker-only, will cancel if would take)
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
        timeInForce: 'GTX' as any, // Binance uses GTX for post-only
      });
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // MARGIN TRADING
  // ============================================================================

  /**
   * Set leverage for futures
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
    return await this.createOrder(symbol, type, side, amount, price, {
      ...params,
      marginMode: params?.marginMode || 'cross',
    });
  }

  /**
   * Fetch margin balance
   */
  async fetchMarginBalance() {
    try {
      await this.ensureAuthenticated();
      return await this.exchange.fetchBalance({ type: 'margin' });
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Fetch futures balance
   */
  async fetchFuturesBalance() {
    try {
      await this.ensureAuthenticated();
      return await this.exchange.fetchBalance({ type: 'future' });
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // FUNDING & TRANSFERS
  // ============================================================================

  /**
   * Transfer between accounts
   */
  async transfer(
    currency: string,
    amount: number,
    fromAccount: 'spot' | 'margin' | 'futures' | 'funding',
    toAccount: 'spot' | 'margin' | 'futures' | 'funding'
  ) {
    try {
      await this.ensureAuthenticated();
      return await this.exchange.transfer(currency, amount, fromAccount, toAccount);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Fetch funding rate
   */
  async fetchFundingRate(symbol: string) {
    try {
      await this.ensureConnected();
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
      await this.ensureConnected();
      return await this.exchange.fetchFundingRateHistory(symbol, since, limit);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // FUTURES TRADING
  // ============================================================================

  /**
   * Fetch futures markets
   */
  async fetchFuturesMarkets() {
    try {
      await this.ensureConnected();
      const markets = await this.fetchMarkets();
      return markets.filter((m: any) => m.future || m.swap);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Fetch open interest
   */
  async fetchOpenInterest(symbol: string) {
    try {
      await this.ensureConnected();
      if (!this.exchange.has['fetchOpenInterest']) {
        throw new Error('fetchOpenInterest not supported');
      }
      return await this.exchange.fetchOpenInterest(symbol);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Set position mode (hedge or one-way)
   */
  async setPositionMode(hedgeMode: boolean) {
    try {
      await this.ensureAuthenticated();
      return await (this.exchange as any).setPositionMode(hedgeMode);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // ACCOUNT & FEES
  // ============================================================================

  /**
   * Fetch trading fees
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
   * Fetch account ledger
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
  // DEPOSITS & WITHDRAWALS
  // ============================================================================

  /**
   * Fetch deposit address
   */
  async fetchDepositAddress(currency: string, network?: string) {
    try {
      await this.ensureAuthenticated();
      return await this.exchange.fetchDepositAddress(currency, { network });
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Fetch deposits
   */
  async fetchDeposits(currency?: string, since?: number, limit?: number) {
    try {
      await this.ensureAuthenticated();
      return await this.exchange.fetchDeposits(currency, since, limit);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Fetch withdrawals
   */
  async fetchWithdrawals(currency?: string, since?: number, limit?: number) {
    try {
      await this.ensureAuthenticated();
      return await this.exchange.fetchWithdrawals(currency, since, limit);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Withdraw funds
   */
  async withdraw(currency: string, amount: number, address: string, tag?: string, network?: string) {
    try {
      await this.ensureAuthenticated();
      return await this.exchange.withdraw(currency, amount, address, tag, { network });
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // ORDER MANAGEMENT
  // ============================================================================

  /**
   * Edit existing order
   */
  async editOrder(
    orderId: string,
    symbol: string,
    type?: string,
    side?: string,
    amount?: number,
    price?: number,
    params?: any
  ) {
    try {
      await this.ensureAuthenticated();
      return await this.exchange.editOrder(orderId, symbol, type as any, side as any, amount, price, params);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Fetch my trades
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
   * Cancel multiple orders
   */
  async cancelOrders(orderIds: string[], symbol?: string) {
    try {
      await this.ensureAuthenticated();
      if (this.exchange.has['cancelOrders']) {
        return await this.exchange.cancelOrders(orderIds, symbol);
      }
      // Fallback
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
   * Cancel all orders after timeout (Dead Man's Switch)
   * Note: Binance doesn't have native support, use countdown cancel
   */
  async cancelAllOrdersAfter(timeout: number) {
    try {
      await this.ensureAuthenticated();
      if (this.exchange.has['cancelAllOrdersAfter']) {
        return await (this.exchange as any).cancelAllOrdersAfter(timeout);
      }
      // Binance futures has countdownCancelAll
      if ((this.exchange as any).fapiPrivatePostCountdownCancelAll) {
        return await (this.exchange as any).fapiPrivatePostCountdownCancelAll({
          countdownTime: timeout,
        });
      }
      throw new Error('cancelAllOrdersAfter not supported on Binance spot');
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Fetch funding history (user's funding payments)
   */
  async fetchFundingHistory(symbol?: string, since?: number, limit?: number) {
    try {
      await this.ensureAuthenticated();
      if (this.exchange.has['fetchFundingHistory']) {
        return await (this.exchange as any).fetchFundingHistory(symbol, since, limit);
      }
      // Binance futures income endpoint
      if ((this.exchange as any).fapiPrivateGetIncome) {
        const params: any = { incomeType: 'FUNDING_FEE' };
        if (symbol) params.symbol = symbol.replace('/', '');
        if (since) params.startTime = since;
        if (limit) params.limit = limit;
        return await (this.exchange as any).fapiPrivateGetIncome(params);
      }
      throw new Error('fetchFundingHistory not supported');
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // UTILITIES
  // ============================================================================

  /**
   * Get Binance system status
   */
  async getSystemStatus() {
    try {
      return await this.fetchStatus();
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Get server time
   */
  async getServerTime() {
    try {
      return await this.fetchTime();
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Get popular Binance trading pairs
   */
  getPopularPairs(): string[] {
    return [
      'BTC/USDT',
      'ETH/USDT',
      'BNB/USDT',
      'SOL/USDT',
      'XRP/USDT',
      'ADA/USDT',
      'DOGE/USDT',
      'MATIC/USDT',
      'DOT/USDT',
      'AVAX/USDT',
      'LINK/USDT',
      'UNI/USDT',
      'ATOM/USDT',
      'LTC/USDT',
      'ETC/USDT',
    ];
  }
}
