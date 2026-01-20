/**
 * Bybit Exchange Adapter
 *
 * Professional-grade Bybit integration using CCXT
 * Supports: Spot, USDT Perpetuals, USDC Perpetuals, Inverse Perpetuals, Options
 *
 * Bybit is the #2 derivatives exchange globally with massive volume
 * Features: Unified Trading Account, Copy Trading, Earn Products
 */

import { BaseExchangeAdapter } from '../BaseExchangeAdapter';
import type {
  ExchangeConfig,
  OrderType,
  OrderSide,
  OrderParams,
  ExchangeCapabilities,
} from '../types';

export class BybitAdapter extends BaseExchangeAdapter {
  constructor(config?: Partial<ExchangeConfig>) {
    super({
      exchange: 'bybit',
      enableRateLimit: true,
      timeout: 30000,
      ...config,
    });
  }

  // ============================================================================
  // BYBIT-SPECIFIC CAPABILITIES
  // ============================================================================

  getCapabilities(): ExchangeCapabilities {
    const base = super.getCapabilities();
    return {
      ...base,
      spot: true,
      margin: true,
      futures: true,
      swap: true,       // Perpetual swaps
      options: true,    // USDC options

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
      portfolioMargin: true,  // Unified Trading Account

      rateLimits: {
        public: 120,    // 120 requests per second
        private: 50,    // 50 requests per second
        trading: 50,    // 50 orders per second
      },
    };
  }

  // ============================================================================
  // ENHANCED ORDER METHODS
  // ============================================================================

  /**
   * Create OCO (One-Cancels-Other) order
   * Bybit supports conditional orders that can act as OCO
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
      // Bybit uses conditional orders for OCO-like functionality
      return await (this.exchange as any).createOrder(symbol, 'limit', side, amount, price, {
        stopPrice,
        stopLimitPrice: stopLimitPrice || stopPrice,
        triggerDirection: side === 'buy' ? 2 : 1, // 1=triggered when price rises, 2=falls
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
      triggerPrice: stopPrice,
      triggerBy: 'LastPrice',
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
      triggerPrice: stopPrice,
      triggerBy: 'LastPrice',
      tpTriggerBy: 'LastPrice',
    });
  }

  /**
   * Create trailing stop order
   */
  async createTrailingStopOrder(
    symbol: string,
    side: OrderSide,
    amount: number,
    callbackRate: number  // Trailing distance in percentage
  ) {
    try {
      await this.ensureAuthenticated();
      return await this.createOrder(symbol, 'trailing_stop' as OrderType, side, amount, undefined, {
        trailingDelta: callbackRate * 100, // Bybit uses basis points
        triggerBy: 'LastPrice',
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
   * Create reduce-only order (derivatives only - close position only)
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
        timeInForce: 'PostOnly' as any,  // Bybit uses PostOnly for maker-only orders
      });
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Create conditional order (trigger order)
   */
  async createConditionalOrder(
    symbol: string,
    type: OrderType,
    side: OrderSide,
    amount: number,
    triggerPrice: number,
    price?: number,
    triggerBy: 'LastPrice' | 'IndexPrice' | 'MarkPrice' = 'LastPrice'
  ) {
    try {
      await this.ensureAuthenticated();
      return await this.createOrder(symbol, type, side, amount, price, {
        triggerPrice,
        triggerBy,
        stopOrderType: 'Stop',
      });
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // MARGIN & LEVERAGE TRADING
  // ============================================================================

  /**
   * Set leverage for derivatives
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
      // Bybit uses 0 for cross, 1 for isolated in some endpoints
      return await this.exchange.setMarginMode(marginMode, symbol);
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
      // Bybit: MergedSingle = one-way, BothSide = hedge
      return await (this.exchange as any).setPositionMode(hedgeMode);
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
   * Fetch derivatives/futures balance
   */
  async fetchFuturesBalance() {
    try {
      await this.ensureAuthenticated();
      // Bybit unified account
      return await this.exchange.fetchBalance({ type: 'swap' });
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Fetch unified account balance (Bybit's Unified Trading Account)
   */
  async fetchUnifiedBalance() {
    try {
      await this.ensureAuthenticated();
      return await this.exchange.fetchBalance({ type: 'unified' });
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Add/reduce margin for position
   */
  async modifyPositionMargin(symbol: string, amount: number, type: 'add' | 'reduce') {
    try {
      await this.ensureAuthenticated();
      const addMargin = type === 'add';
      return await (this.exchange as any).addMargin(symbol, amount, {
        type: addMargin ? 'ADD_MARGIN' : 'REDUCE_MARGIN'
      });
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // FUNDING & TRANSFERS
  // ============================================================================

  /**
   * Transfer between accounts (spot, contract, unified, etc.)
   */
  async transfer(
    currency: string,
    amount: number,
    fromAccount: 'spot' | 'contract' | 'unified' | 'funding' | 'option',
    toAccount: 'spot' | 'contract' | 'unified' | 'funding' | 'option'
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
      // Bybit alternative: fetch from transaction log
      return await (this.exchange as any).privateGetV5AccountTransactionLog({
        type: 'FUNDING_FEE',
        symbol: symbol?.replace('/', ''),
        limit,
      });
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // DERIVATIVES/FUTURES TRADING
  // ============================================================================

  /**
   * Fetch futures/perpetual markets
   */
  async fetchFuturesMarkets() {
    try {
      await this.ensureConnected();
      const markets = await this.fetchMarkets();
      return markets.filter((m: any) => m.future || m.swap || m.linear || m.inverse);
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
   * Fetch open interest history
   */
  async fetchOpenInterestHistory(symbol: string, timeframe: string = '1h', since?: number, limit?: number) {
    try {
      await this.ensureConnected();
      if (this.exchange.has['fetchOpenInterestHistory']) {
        return await (this.exchange as any).fetchOpenInterestHistory(symbol, timeframe, since, limit);
      }
      throw new Error('fetchOpenInterestHistory not available');
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Fetch liquidation history
   */
  async fetchLiquidations(symbol: string, since?: number, limit?: number) {
    try {
      await this.ensureConnected();
      if (this.exchange.has['fetchLiquidations']) {
        return await (this.exchange as any).fetchLiquidations(symbol, since, limit);
      }
      throw new Error('fetchLiquidations not available');
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Set take profit and stop loss for position
   */
  async setTakeProfitStopLoss(
    symbol: string,
    takeProfit?: number,
    stopLoss?: number,
    params?: any
  ) {
    try {
      await this.ensureAuthenticated();
      return await (this.exchange as any).setTpSlMode(symbol, {
        takeProfit,
        stopLoss,
        ...params,
      });
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
      return await (this.exchange as any).privateGetV5AccountInfo();
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
      throw new Error('cancelAllOrdersAfter not supported on Bybit');
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
    timeframe: '1m' | '3m' | '5m' | '15m' | '30m' | '1h' | '2h' | '4h' | '6h' | '12h' | '1d' | '1w' | '1M',
    since?: number,
    limit: number = 200
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
        exchange: 'bybit',
        timestamp: trade.timestamp || Date.now(),
      }));
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // BYBIT-SPECIFIC: COPY TRADING
  // ============================================================================

  /**
   * Fetch copy trading masters (top traders)
   */
  async fetchCopyTradingMasters(params?: any) {
    try {
      await this.ensureConnected();
      return await (this.exchange as any).publicGetV5CopyTradingMasterInfo(params);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Get copy trading status
   */
  async getCopyTradingStatus() {
    try {
      await this.ensureAuthenticated();
      return await (this.exchange as any).privateGetV5CopyTradingContractOrder();
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // BYBIT-SPECIFIC: EARN PRODUCTS
  // ============================================================================

  /**
   * Fetch earn products
   */
  async fetchEarnProducts() {
    try {
      await this.ensureAuthenticated();
      return await (this.exchange as any).privateGetV5AssetEarnProduct();
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Subscribe to earn product
   */
  async subscribeToEarn(productId: string, amount: number) {
    try {
      await this.ensureAuthenticated();
      return await (this.exchange as any).privatePostV5AssetEarnSubscribe({
        productId,
        amount: amount.toString(),
      });
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Redeem from earn product
   */
  async redeemFromEarn(productId: string, amount: number) {
    try {
      await this.ensureAuthenticated();
      return await (this.exchange as any).privatePostV5AssetEarnRedeem({
        productId,
        amount: amount.toString(),
      });
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // UTILITIES
  // ============================================================================

  /**
   * Get Bybit system status
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
   * Get risk limit info for symbol
   */
  async getRiskLimitInfo(symbol: string) {
    try {
      await this.ensureConnected();
      return await (this.exchange as any).publicGetV5MarketRiskLimit({
        symbol: symbol.replace('/', ''),
      });
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Set risk limit for position
   */
  async setRiskLimit(symbol: string, riskId: number) {
    try {
      await this.ensureAuthenticated();
      return await (this.exchange as any).privatePostV5PositionSetRiskLimit({
        symbol: symbol.replace('/', ''),
        riskId,
      });
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Get popular Bybit trading pairs
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
      'MATIC/USDT',
      'LINK/USDT',
      'DOT/USDT',
      'UNI/USDT',
      'ATOM/USDT',
      'LTC/USDT',
      'OP/USDT',
      'ARB/USDT',
    ];
  }

  /**
   * Get popular USDT perpetual pairs
   */
  getPopularPerpetuals(): string[] {
    return [
      'BTC/USDT:USDT',
      'ETH/USDT:USDT',
      'SOL/USDT:USDT',
      'XRP/USDT:USDT',
      'DOGE/USDT:USDT',
      'ADA/USDT:USDT',
      'AVAX/USDT:USDT',
      'MATIC/USDT:USDT',
      'LINK/USDT:USDT',
      'DOT/USDT:USDT',
    ];
  }
}
