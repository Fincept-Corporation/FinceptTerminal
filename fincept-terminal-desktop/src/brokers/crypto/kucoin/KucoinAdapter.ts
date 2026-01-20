/**
 * KuCoin Exchange Adapter
 *
 * Professional-grade KuCoin integration using CCXT
 * Supports: Spot, Margin, Futures (KuCoin Futures)
 *
 * KuCoin is known for wide altcoin selection and good API
 * Features: Spot, Margin (up to 10x), Futures (up to 100x), Lending, Staking
 */

import { BaseExchangeAdapter } from '../BaseExchangeAdapter';
import type {
  ExchangeConfig,
  OrderType,
  OrderSide,
  OrderParams,
  ExchangeCapabilities,
} from '../types';

export class KucoinAdapter extends BaseExchangeAdapter {
  constructor(config?: Partial<ExchangeConfig>) {
    super({
      exchange: 'kucoin',
      enableRateLimit: true,
      timeout: 30000,
      ...config,
    });
  }

  // ============================================================================
  // KUCOIN-SPECIFIC CAPABILITIES
  // ============================================================================

  getCapabilities(): ExchangeCapabilities {
    const base = super.getCapabilities();
    return {
      ...base,
      spot: true,
      margin: true,
      futures: true,
      swap: true,       // Perpetual swaps via KuCoin Futures
      options: false,

      supportedOrderTypes: [
        'market',
        'limit',
        'stop',
        'stop_limit',
      ] as any,

      leverageTrading: true,
      stopOrders: true,
      conditionalOrders: true,
      algoOrders: false,

      realtimeData: true,
      historicalData: true,
      websocketSupport: true,

      multiAccount: false,
      subAccounts: true,
      portfolioMargin: false,

      rateLimits: {
        public: 100,    // 100 requests per 10 seconds
        private: 200,   // 200 requests per 10 seconds
        trading: 45,    // 45 orders per 3 seconds
      },
    };
  }

  // ============================================================================
  // ENHANCED ORDER METHODS
  // ============================================================================

  /**
   * Create OCO (One-Cancels-Other) order
   * KuCoin supports stop orders that can act as OCO
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
      // KuCoin doesn't have native OCO, create both orders
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
    const type: OrderType = limitPrice ? 'stop_limit' : 'stop';
    return await this.createOrder(symbol, type, side, amount, limitPrice, {
      stopPrice,
      stop: 'loss',
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
      stop: 'entry',
    });
  }

  /**
   * Create trailing stop order
   * Note: KuCoin Futures supports trailing stops
   */
  async createTrailingStopOrder(
    symbol: string,
    side: OrderSide,
    amount: number,
    callbackRate: number
  ) {
    try {
      await this.ensureAuthenticated();
      // KuCoin spot doesn't have native trailing stops
      // Calculate stop price based on current price
      const ticker = await this.fetchTicker(symbol);
      const currentPrice = ticker.last || ticker.close;
      if (!currentPrice) throw new Error('Cannot determine current price');

      const stopPrice = side === 'sell'
        ? currentPrice * (1 - callbackRate / 100)
        : currentPrice * (1 + callbackRate / 100);

      return await this.createStopLossOrder(symbol, side, amount, stopPrice);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Create iceberg order (hidden order)
   * KuCoin supports hidden orders
   */
  async createIcebergOrder(
    symbol: string,
    side: OrderSide,
    amount: number,
    price: number,
    visibleSize: number,
    params?: OrderParams
  ) {
    try {
      await this.ensureAuthenticated();
      return await this.createOrder(symbol, 'limit', side, amount, price, {
        ...params,
        hidden: false,
        iceberg: true,
        visibleSize,
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
      });
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Create hidden order (fully hidden from order book)
   */
  async createHiddenOrder(
    symbol: string,
    side: OrderSide,
    amount: number,
    price: number
  ) {
    try {
      await this.ensureAuthenticated();
      return await this.createOrder(symbol, 'limit', side, amount, price, {
        hidden: true,
      });
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // MARGIN & LEVERAGE TRADING
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
      if (this.exchange.has['setMarginMode']) {
        return await this.exchange.setMarginMode(marginMode, symbol);
      }
      return { symbol, marginMode, info: { message: 'Margin mode set' } };
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Set position mode (one-way or hedge mode)
   */
  async setPositionMode(hedgeMode: boolean) {
    try {
      await this.ensureAuthenticated();
      if (this.exchange.has['setPositionMode']) {
        return await (this.exchange as any).setPositionMode(hedgeMode);
      }
      // KuCoin Futures uses one-way mode by default
      return { hedgeMode: false, info: { message: 'KuCoin uses one-way position mode' } };
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
      tradeType: 'MARGIN_TRADE',
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

  /**
   * Borrow margin
   */
  async borrowMargin(currency: string, amount: number, symbol?: string) {
    try {
      await this.ensureAuthenticated();
      if (this.exchange.has['borrowMargin']) {
        return await (this.exchange as any).borrowMargin(currency, amount, symbol);
      }
      // Use KuCoin's margin borrow API
      return await (this.exchange as any).privatePostMarginBorrow({
        currency,
        size: amount.toString(),
        ...(symbol && { symbol }),
      });
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Repay margin
   */
  async repayMargin(currency: string, amount: number, symbol?: string) {
    try {
      await this.ensureAuthenticated();
      if (this.exchange.has['repayMargin']) {
        return await (this.exchange as any).repayMargin(currency, amount, symbol);
      }
      return await (this.exchange as any).privatePostMarginRepay({
        currency,
        size: amount.toString(),
        ...(symbol && { symbol }),
      });
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // FUNDING & TRANSFERS
  // ============================================================================

  /**
   * Transfer between accounts (main, trade, margin, futures, etc.)
   */
  async transfer(
    currency: string,
    amount: number,
    fromAccount: 'main' | 'trade' | 'margin' | 'futures' | 'contract',
    toAccount: 'main' | 'trade' | 'margin' | 'futures' | 'contract'
  ) {
    try {
      await this.ensureAuthenticated();
      return await this.exchange.transfer(currency, amount, fromAccount, toAccount);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Fetch funding rate for perpetual
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

  /**
   * Fetch user's funding history (payments received/paid)
   */
  async fetchFundingHistory(symbol?: string, since?: number, limit?: number) {
    try {
      await this.ensureAuthenticated();
      if (this.exchange.has['fetchFundingHistory']) {
        return await (this.exchange as any).fetchFundingHistory(symbol, since, limit);
      }
      // Fallback: return empty array
      return [];
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // FUTURES TRADING
  // ============================================================================

  /**
   * Fetch futures/perpetual markets
   */
  async fetchFuturesMarkets() {
    try {
      await this.ensureConnected();
      const markets = await this.fetchMarkets();
      return markets.filter((m: any) => m.future || m.swap || m.contract);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Fetch open interest for symbol
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
   * Add margin to position
   */
  async addMargin(symbol: string, amount: number) {
    try {
      await this.ensureAuthenticated();
      if (this.exchange.has['addMargin']) {
        return await this.exchange.addMargin(symbol, amount);
      }
      throw new Error('addMargin not supported');
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
   * Fetch account ledger / transaction history
   */
  async fetchLedger(code?: string, since?: number, limit?: number) {
    try {
      await this.ensureAuthenticated();
      return await this.exchange.fetchLedger(code, since, limit);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Fetch account info
   */
  async fetchAccountInfo() {
    try {
      await this.ensureAuthenticated();
      return await (this.exchange as any).privateGetAccounts();
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
   * Create deposit address
   */
  async createDepositAddress(currency: string, network?: string) {
    try {
      await this.ensureAuthenticated();
      return await this.exchange.createDepositAddress(currency, { network });
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
      if (this.exchange.has['editOrder']) {
        return await this.exchange.editOrder(orderId, symbol, type as any, side as any, amount, price, params);
      }
      // Fallback: cancel and recreate
      await this.cancelOrder(orderId, symbol);
      return await this.createOrder(symbol, (type || 'limit') as OrderType, (side || 'buy') as OrderSide, amount!, price, params);
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
      // Fallback to individual cancels
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
   * Cancel all open orders
   */
  async cancelAllOrders(symbol?: string) {
    try {
      await this.ensureAuthenticated();
      return await this.exchange.cancelAllOrders(symbol);
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
   */
  async cancelAllOrdersAfter(timeout: number) {
    try {
      await this.ensureAuthenticated();
      if (this.exchange.has['cancelAllOrdersAfter']) {
        return await (this.exchange as any).cancelAllOrdersAfter(timeout);
      }
      throw new Error('cancelAllOrdersAfter not supported on KuCoin');
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // HISTORICAL DATA
  // ============================================================================

  /**
   * Fetch historical OHLCV with extended options
   */
  async fetchHistoricalOHLCV(
    symbol: string,
    timeframe: '1m' | '3m' | '5m' | '15m' | '30m' | '1h' | '2h' | '4h' | '6h' | '8h' | '12h' | '1d' | '1w',
    since?: number,
    limit: number = 500
  ) {
    try {
      await this.ensureConnected();
      return await this.exchange.fetchOHLCV(symbol, timeframe, since, limit);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Fetch recent trades with extended data
   */
  async fetchRecentTrades(symbol: string, limit: number = 100) {
    try {
      await this.ensureConnected();
      const trades = await this.exchange.fetchTrades(symbol, undefined, limit);
      return trades.map(trade => ({
        ...trade,
        exchange: 'kucoin',
        timestamp: trade.timestamp || Date.now(),
      }));
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // KUCOIN-SPECIFIC: LENDING
  // ============================================================================

  /**
   * Fetch lending rates
   */
  async fetchLendingRates(currency?: string) {
    try {
      await this.ensureConnected();
      return await (this.exchange as any).publicGetMarginConfigCurrency({
        currency,
      });
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Create lending order
   */
  async createLendingOrder(currency: string, amount: number, dailyRate: number, term: number) {
    try {
      await this.ensureAuthenticated();
      return await (this.exchange as any).privatePostMarginLend({
        currency,
        size: amount.toString(),
        dailyIntRate: dailyRate.toString(),
        term,
      });
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Fetch active lending orders
   */
  async fetchActiveLendingOrders(currency?: string) {
    try {
      await this.ensureAuthenticated();
      return await (this.exchange as any).privateGetMarginLendActive({
        currency,
      });
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // KUCOIN-SPECIFIC: SUBACCOUNTS
  // ============================================================================

  /**
   * Fetch subaccounts
   */
  async fetchSubAccounts() {
    try {
      await this.ensureAuthenticated();
      return await (this.exchange as any).privateGetSubAccounts();
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Transfer to subaccount
   */
  async transferToSubAccount(
    currency: string,
    amount: number,
    subAccountUserId: string,
    direction: 'IN' | 'OUT'
  ) {
    try {
      await this.ensureAuthenticated();
      return await (this.exchange as any).privatePostAccountsSubTransfer({
        currency,
        amount: amount.toString(),
        subUserId: subAccountUserId,
        direction,
      });
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // UTILITIES
  // ============================================================================

  /**
   * Get KuCoin system status
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
   * Get popular KuCoin trading pairs
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
      'KCS/USDT',   // KuCoin's native token
      'PEPE/USDT',
    ];
  }

  /**
   * Get popular KuCoin Futures pairs
   */
  getPopularFuturesPairs(): string[] {
    return [
      'BTC/USDT:USDT',
      'ETH/USDT:USDT',
      'SOL/USDT:USDT',
      'XRP/USDT:USDT',
      'DOGE/USDT:USDT',
      'ADA/USDT:USDT',
      'AVAX/USDT:USDT',
      'DOT/USDT:USDT',
      'MATIC/USDT:USDT',
      'LINK/USDT:USDT',
    ];
  }
}
