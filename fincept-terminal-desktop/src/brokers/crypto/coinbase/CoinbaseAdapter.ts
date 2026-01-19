/**
 * Coinbase Exchange Adapter
 *
 * Professional-grade Coinbase integration using CCXT
 * Supports: Spot trading only (no margin/futures)
 * Note: Coinbase is US-focused, highly regulated exchange
 */

import { BaseExchangeAdapter } from '../BaseExchangeAdapter';
import type {
  ExchangeConfig,
  OrderType,
  OrderSide,
  OrderParams,
  ExchangeCapabilities,
} from '../types';

export class CoinbaseAdapter extends BaseExchangeAdapter {
  constructor(config?: Partial<ExchangeConfig>) {
    super({
      exchange: 'coinbase',
      enableRateLimit: true,
      timeout: 30000,
      ...config,
    });
  }

  // ============================================================================
  // COINBASE-SPECIFIC CAPABILITIES
  // ============================================================================

  getCapabilities(): ExchangeCapabilities {
    const base = super.getCapabilities();
    return {
      ...base,
      spot: true,
      margin: false,
      futures: false,
      swap: false,
      options: false,

      supportedOrderTypes: [
        'market',
        'limit',
        'stop',
        'stop_limit',
      ],

      leverageTrading: false,
      stopOrders: true,
      conditionalOrders: true,
      algoOrders: false,

      realtimeData: true,
      historicalData: true,
      websocketSupport: true,

      multiAccount: false,
      subAccounts: false,
      portfolioMargin: false,

      rateLimits: {
        public: 10,
        private: 15,
        trading: 15,
      },
    };
  }

  // ============================================================================
  // CORE ORDER METHODS
  // ============================================================================

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
        stop_price: stopPrice,
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
      });
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Create trailing stop order
   * Note: Coinbase has limited trailing stop support, simulated via stop orders
   */
  async createTrailingStopOrder(
    symbol: string,
    side: OrderSide,
    amount: number,
    trailingDelta: number,
    params?: OrderParams
  ) {
    try {
      await this.ensureAuthenticated();
      // Coinbase doesn't support native trailing stops
      // Calculate stop price based on current price and trailing delta
      const ticker = await this.fetchTicker(symbol);
      const currentPrice = ticker.last || ticker.close;
      if (!currentPrice) throw new Error('Cannot determine current price');

      const stopPrice = side === 'sell'
        ? currentPrice * (1 - trailingDelta / 100)
        : currentPrice * (1 + trailingDelta / 100);

      return await this.createStopLossOrder(symbol, side, amount, stopPrice);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Create iceberg order
   * Note: Coinbase doesn't support native iceberg, simulated with multiple orders
   */
  async createIcebergOrder(
    symbol: string,
    side: OrderSide,
    amount: number,
    price: number,
    displayAmount: number,
    params?: OrderParams
  ) {
    try {
      await this.ensureAuthenticated();
      // Simulate iceberg by creating multiple smaller orders
      const chunks = Math.ceil(amount / displayAmount);
      const orders = [];

      for (let i = 0; i < chunks; i++) {
        const chunkAmount = Math.min(displayAmount, amount - i * displayAmount);
        const order = await this.createOrder(symbol, 'limit', side, chunkAmount, price, params);
        orders.push(order);
      }

      return orders;
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Create reduce-only order
   * Note: Not applicable for spot-only exchange
   */
  async createReduceOnlyOrder(
    symbol: string,
    type: OrderType,
    side: OrderSide,
    amount: number,
    price?: number
  ) {
    // Coinbase is spot-only, no reduce-only concept
    // Just create a regular order
    try {
      await this.ensureAuthenticated();
      return await this.createOrder(symbol, type, side, amount, price);
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
        post_only: true,
      });
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Create OCO order
   * Note: Coinbase doesn't support native OCO, simulated with two orders
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

  // ============================================================================
  // MARGIN TRADING (Not supported - Spot only)
  // ============================================================================

  /**
   * Create margin order
   * Note: Coinbase doesn't support margin trading
   */
  async createMarginOrder(
    symbol: string,
    type: OrderType,
    side: OrderSide,
    amount: number,
    price?: number,
    params?: OrderParams
  ) {
    throw new Error('Margin trading not supported on Coinbase');
  }

  /**
   * Fetch margin balance
   * Note: Not applicable for spot-only exchange
   */
  async fetchMarginBalance() {
    // Return regular balance as there's no margin
    try {
      await this.ensureAuthenticated();
      return await this.fetchBalance();
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Fetch futures balance
   * Note: Not applicable for spot-only exchange
   */
  async fetchFuturesBalance() {
    // Return regular balance as there's no futures
    try {
      await this.ensureAuthenticated();
      const balance = await this.fetchBalance();
      return {
        ...balance,
        unrealizedPnl: 0,
        marginBalance: 0,
        availableBalance: (balance.free as any)?.USD || (balance.free as any)?.USDC || 0,
      };
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // LEVERAGE & MARGIN MODE (Not supported)
  // ============================================================================

  /**
   * Set leverage
   * Note: Not applicable for spot-only exchange
   */
  async setLeverage(symbol: string, leverage: number) {
    throw new Error('Leverage not supported on Coinbase (spot-only exchange)');
  }

  /**
   * Set margin mode
   * Note: Not applicable for spot-only exchange
   */
  async setMarginMode(symbol: string, marginMode: 'cross' | 'isolated') {
    throw new Error('Margin mode not supported on Coinbase (spot-only exchange)');
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
   * Cancel all orders after timeout
   * Note: Not natively supported
   */
  async cancelAllOrdersAfter(timeout: number) {
    throw new Error('cancelAllOrdersAfter not supported on Coinbase');
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
      if (this.exchange.has['fetchLedger']) {
        return await this.exchange.fetchLedger(code, since, limit);
      }
      return [];
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
      if (this.exchange.has['transfer']) {
        return await this.exchange.transfer(code, amount, fromAccount, toAccount);
      }
      throw new Error('Transfer not supported');
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Fetch current funding rate
   * Note: Not applicable for spot-only exchange
   */
  async fetchFundingRate(symbol: string) {
    // No funding rates on spot-only exchange
    return {
      symbol,
      fundingRate: 0,
      fundingTimestamp: Date.now(),
      info: { message: 'Funding rates not applicable for spot trading' },
    };
  }

  /**
   * Fetch funding rate history
   * Note: Not applicable for spot-only exchange
   */
  async fetchFundingRateHistory(symbol: string, since?: number, limit?: number) {
    // No funding rates on spot-only exchange
    return [];
  }

  /**
   * Fetch user's funding payments history
   * Note: Not applicable for spot-only exchange
   */
  async fetchFundingHistory(symbol?: string, since?: number, limit?: number) {
    // No funding on spot-only exchange
    return [];
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
   * Fetch futures markets
   * Note: Not applicable for spot-only exchange
   */
  async fetchFuturesMarkets() {
    // No futures on Coinbase
    return [];
  }

  /**
   * Fetch open interest
   * Note: Not applicable for spot-only exchange
   */
  async fetchOpenInterest(symbol: string) {
    // No open interest on spot-only exchange
    return {
      symbol,
      openInterest: 0,
      info: { message: 'Open interest not applicable for spot trading' },
    };
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
   * Get popular trading pairs for Coinbase
   */
  getPopularPairs(): string[] {
    return [
      'BTC/USD',
      'ETH/USD',
      'SOL/USD',
      'XRP/USD',
      'DOGE/USD',
      'ADA/USD',
      'AVAX/USD',
      'DOT/USD',
      'MATIC/USD',
      'LINK/USD',
      'UNI/USD',
      'ATOM/USD',
      'LTC/USD',
      'BCH/USD',
      'SHIB/USD',
      'APE/USD',
      'AAVE/USD',
      'CRV/USD',
      'MKR/USD',
      'COMP/USD',
    ];
  }

  // ============================================================================
  // COINBASE-SPECIFIC FEATURES
  // ============================================================================

  /**
   * Get Coinbase Pro accounts
   */
  async getAccounts() {
    try {
      await this.ensureAuthenticated();
      if ((this.exchange as any).privateGetAccounts) {
        return await (this.exchange as any).privateGetAccounts();
      }
      return await this.fetchBalance();
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Get payment methods (for fiat on/off ramp)
   */
  async getPaymentMethods() {
    try {
      await this.ensureAuthenticated();
      if ((this.exchange as any).privateGetPaymentMethods) {
        return await (this.exchange as any).privateGetPaymentMethods();
      }
      return [];
    } catch (error) {
      throw this.handleError(error);
    }
  }
}
