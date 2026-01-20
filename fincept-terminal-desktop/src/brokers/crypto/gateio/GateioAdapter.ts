/**
 * Gate.io Exchange Adapter
 *
 * Professional-grade Gate.io integration using CCXT
 * Supports: Spot, Margin, Futures, Perpetuals, Options, Copy Trading
 *
 * Gate.io is the #4 spot exchange globally with 1400+ trading pairs
 * Known for: Wide altcoin selection, copy trading, lending/borrowing
 */

import { BaseExchangeAdapter } from '../BaseExchangeAdapter';
import type {
  ExchangeConfig,
  OrderType,
  OrderSide,
  OrderParams,
  ExchangeCapabilities,
} from '../types';

export class GateioAdapter extends BaseExchangeAdapter {
  constructor(config?: Partial<ExchangeConfig>) {
    super({
      exchange: 'gateio',
      enableRateLimit: true,
      timeout: 30000,
      ...config,
    });
  }

  // ============================================================================
  // GATE.IO-SPECIFIC CAPABILITIES
  // ============================================================================

  getCapabilities(): ExchangeCapabilities {
    const base = super.getCapabilities();
    return {
      ...base,
      spot: true,
      margin: true,
      futures: true,
      swap: true,       // Perpetual swaps
      options: true,    // Options trading

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
        public: 900,    // 900 requests per minute
        private: 900,   // 900 requests per minute
        trading: 300,   // 300 orders per minute
      },
    };
  }

  // ============================================================================
  // ENHANCED ORDER METHODS
  // ============================================================================

  /**
   * Create OCO (One-Cancels-Other) order
   * Gate.io supports conditional orders that can act as OCO
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
      // Gate.io uses price triggers for OCO-like functionality
      return await (this.exchange as any).createOrder(symbol, 'limit', side, amount, price, {
        stopPrice,
        stopLossPrice: stopLimitPrice || stopPrice,
        takeProfitPrice: price,
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
      trigger: 'le', // Less than or equal
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
      trigger: 'ge', // Greater than or equal
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
        trailingPercent: callbackRate,
        callbackRate: callbackRate.toString(),
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
        iceberg: icebergQty,
        visible_size: icebergQty,
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
        reduce_only: true,
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
        timeInForce: 'PO' as any,  // Gate.io Post-Only
        tif: 'poc', // Post only cancel
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
    triggerBy: 'last' | 'index' | 'mark' = 'last'
  ) {
    try {
      await this.ensureAuthenticated();
      return await this.createOrder(symbol, type, side, amount, price, {
        triggerPrice,
        trigger_price: triggerPrice.toString(),
        trigger: side === 'buy' ? 'le' : 'ge',
        price_type: triggerBy === 'mark' ? 1 : (triggerBy === 'index' ? 2 : 0),
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
      if (this.exchange.has['setPositionMode']) {
        return await (this.exchange as any).setPositionMode(hedgeMode);
      }
      // Gate.io uses dual_mode parameter
      return await (this.exchange as any).privatePostFuturesSettleDualMode({
        settle: 'usdt',
        dual_mode: hedgeMode,
      });
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
      account: 'margin',
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
      return await this.exchange.fetchBalance({ type: 'swap' });
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Borrow margin funds
   */
  async borrowMargin(currency: string, amount: number, symbol?: string) {
    try {
      await this.ensureAuthenticated();
      if (this.exchange.has['borrowMargin']) {
        return await (this.exchange as any).borrowMargin(currency, amount, symbol);
      }
      return await (this.exchange as any).privatePostMarginLoans({
        currency,
        amount: amount.toString(),
        currency_pair: symbol?.replace('/', '_'),
      });
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Repay margin loan
   */
  async repayMargin(currency: string, amount: number, symbol?: string) {
    try {
      await this.ensureAuthenticated();
      if (this.exchange.has['repayMargin']) {
        return await (this.exchange as any).repayMargin(currency, amount, symbol);
      }
      return await (this.exchange as any).privatePostMarginLoansLoanIdRepayment({
        currency,
        amount: amount.toString(),
        currency_pair: symbol?.replace('/', '_'),
      });
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
      const change = type === 'add' ? amount : -amount;
      return await (this.exchange as any).addMargin(symbol, change);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // FUNDING & TRANSFERS
  // ============================================================================

  /**
   * Transfer between accounts (spot, margin, futures, etc.)
   */
  async transfer(
    currency: string,
    amount: number,
    fromAccount: 'spot' | 'margin' | 'futures' | 'delivery' | 'options',
    toAccount: 'spot' | 'margin' | 'futures' | 'delivery' | 'options'
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
      // Gate.io alternative
      return await (this.exchange as any).privateGetFuturesSettleFunding({
        settle: 'usdt',
        contract: symbol?.replace('/', '_'),
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
      // Gate.io liquidation orders
      return await (this.exchange as any).publicGetFuturesSettleLiqOrders({
        settle: 'usdt',
        contract: symbol?.replace('/', '_'),
        limit,
      });
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
      const orders = [];

      if (takeProfit) {
        orders.push(this.createTakeProfitOrder(symbol, 'sell', 0, takeProfit));
      }
      if (stopLoss) {
        orders.push(this.createStopLossOrder(symbol, 'sell', 0, stopLoss));
      }

      return await Promise.all(orders);
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
      return await (this.exchange as any).privateGetAccountDetail();
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
      return await this.exchange.withdraw(currency, amount, address, tag, { network, chain: network });
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
      throw new Error('cancelAllOrdersAfter not supported on Gate.io');
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
    timeframe: '10s' | '1m' | '5m' | '15m' | '30m' | '1h' | '4h' | '8h' | '1d' | '7d' | '30d',
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
        exchange: 'gateio',
        timestamp: trade.timestamp || Date.now(),
      }));
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // GATE.IO-SPECIFIC: COPY TRADING
  // ============================================================================

  /**
   * Fetch copy trading leaders
   */
  async fetchCopyTradingLeaders(params?: any) {
    try {
      await this.ensureConnected();
      return await (this.exchange as any).publicGetCopyTradingLeaders(params);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Follow a copy trading leader
   */
  async followCopyTradingLeader(leaderId: string, params?: any) {
    try {
      await this.ensureAuthenticated();
      return await (this.exchange as any).privatePostCopyTradingFollow({
        leader_id: leaderId,
        ...params,
      });
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Unfollow a copy trading leader
   */
  async unfollowCopyTradingLeader(leaderId: string) {
    try {
      await this.ensureAuthenticated();
      return await (this.exchange as any).privateDeleteCopyTradingFollow({
        leader_id: leaderId,
      });
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
      return await (this.exchange as any).privateGetCopyTradingStatus();
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // GATE.IO-SPECIFIC: LENDING & BORROWING
  // ============================================================================

  /**
   * Fetch lending rates
   */
  async fetchLendingRates(currency?: string) {
    try {
      await this.ensureConnected();
      if (currency) {
        return await (this.exchange as any).publicGetMarginUniCurrencyEstimatedRate({
          currency,
        });
      }
      return await (this.exchange as any).publicGetMarginUniInterestRecords();
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Create lending order
   */
  async createLendingOrder(currency: string, amount: number, rate: number, days: number = 10) {
    try {
      await this.ensureAuthenticated();
      return await (this.exchange as any).privatePostMarginUniLoans({
        currency,
        amount: amount.toString(),
        rate: rate.toString(),
        days,
      });
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Fetch my lending orders
   */
  async fetchMyLendingOrders(currency?: string, status?: 'active' | 'finished') {
    try {
      await this.ensureAuthenticated();
      return await (this.exchange as any).privateGetMarginUniLoans({
        currency,
        status: status === 'active' ? 1 : (status === 'finished' ? 2 : undefined),
      });
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Cancel lending order
   */
  async cancelLendingOrder(orderId: string, currency: string) {
    try {
      await this.ensureAuthenticated();
      return await (this.exchange as any).privateDeleteMarginUniLoansLoanId({
        loan_id: orderId,
        currency,
      });
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // GATE.IO-SPECIFIC: EARN PRODUCTS
  // ============================================================================

  /**
   * Fetch earn/staking products
   */
  async fetchEarnProducts() {
    try {
      await this.ensureAuthenticated();
      return await (this.exchange as any).privateGetEarnProducts();
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
      return await (this.exchange as any).privatePostEarnOrders({
        product_id: productId,
        amount: amount.toString(),
      });
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Redeem from earn product
   */
  async redeemFromEarn(orderId: string, amount?: number) {
    try {
      await this.ensureAuthenticated();
      return await (this.exchange as any).privateDeleteEarnOrdersOrderId({
        order_id: orderId,
        amount: amount?.toString(),
      });
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Fetch my earn orders
   */
  async fetchMyEarnOrders(productId?: string) {
    try {
      await this.ensureAuthenticated();
      return await (this.exchange as any).privateGetEarnOrders({
        product_id: productId,
      });
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // GATE.IO-SPECIFIC: SUB-ACCOUNTS
  // ============================================================================

  /**
   * Fetch sub-accounts list
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
   * Create sub-account
   */
  async createSubAccount(email: string, password: string, remark?: string) {
    try {
      await this.ensureAuthenticated();
      return await (this.exchange as any).privatePostSubAccounts({
        email,
        password,
        remark,
      });
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Transfer to sub-account
   */
  async transferToSubAccount(
    subAccountId: string,
    currency: string,
    amount: number,
    direction: 'to' | 'from' = 'to'
  ) {
    try {
      await this.ensureAuthenticated();
      return await (this.exchange as any).privatePostSubAccountsTransfers({
        sub_account_id: subAccountId,
        currency,
        amount: amount.toString(),
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
   * Get Gate.io system status
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
   * Get risk limit info for futures
   */
  async getRiskLimitInfo(symbol: string) {
    try {
      await this.ensureConnected();
      return await (this.exchange as any).publicGetFuturesSettleContracts({
        settle: 'usdt',
        contract: symbol.replace('/', '_'),
      });
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Set risk limit for position
   */
  async setRiskLimit(symbol: string, riskLimit: number) {
    try {
      await this.ensureAuthenticated();
      return await (this.exchange as any).privatePostFuturesSettlePositionsContractRiskLimit({
        settle: 'usdt',
        contract: symbol.replace('/', '_'),
        risk_limit: riskLimit,
      });
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Fetch currency chains (networks)
   */
  async fetchCurrencyChains(currency: string) {
    try {
      await this.ensureConnected();
      return await (this.exchange as any).publicGetWalletCurrencyChains({
        currency,
      });
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Get popular Gate.io trading pairs
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
      'FIL/USDT',
      'NEAR/USDT',
      'APE/USDT',
      'SAND/USDT',
      'MANA/USDT',
      'GT/USDT',     // Gate Token
      'SHIB/USDT',
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

  /**
   * Get new listings on Gate.io (known for early listings)
   */
  async fetchNewListings(limit: number = 20) {
    try {
      await this.ensureConnected();
      const markets = await this.fetchMarkets();
      // Sort by listing date (if available) or return recent ones
      return markets
        .filter((m: any) => m.active && m.spot)
        .slice(0, limit);
    } catch (error) {
      throw this.handleError(error);
    }
  }
}
