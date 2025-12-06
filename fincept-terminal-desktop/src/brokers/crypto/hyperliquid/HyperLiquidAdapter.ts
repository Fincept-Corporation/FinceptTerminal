/**
 * HyperLiquid Exchange Adapter
 *
 * Professional-grade HyperLiquid DEX integration using CCXT
 * Features: Perpetual futures, spot, vault trading, subaccounts
 * Note: Simulates market orders with 5% max slippage limit orders
 */

import { BaseExchangeAdapter } from '../BaseExchangeAdapter';
import type {
  ExchangeConfig,
  OrderType,
  OrderSide,
  OrderParams,
  ExchangeCapabilities,
} from '../types';

export class HyperLiquidAdapter extends BaseExchangeAdapter {
  constructor(config?: Partial<ExchangeConfig>) {
    super({
      exchange: 'hyperliquid',
      enableRateLimit: true,
      timeout: 30000,
      ...config,
    });
  }

  // ============================================================================
  // HYPERLIQUID-SPECIFIC FEATURES
  // ============================================================================

  /**
   * Get HyperLiquid-specific capabilities
   */
  getCapabilities(): ExchangeCapabilities {
    const base = super.getCapabilities();
    return {
      ...base,
      spot: true,
      margin: false, // Uses perpetuals instead
      futures: true, // Perpetual futures
      swap: true,
      options: false,

      supportedOrderTypes: [
        'limit',
        'market', // Simulated with limit orders
        'stop',
        'stop_limit',
      ],

      leverageTrading: true, // Up to 50x on perpetuals
      stopOrders: true,
      conditionalOrders: true,
      algoOrders: false,

      realtimeData: true,
      historicalData: true,
      websocketSupport: true,

      rateLimits: {
        public: 100, // Very fast, decentralized
        private: 200,
        trading: 100,
      },
    };
  }

  // ============================================================================
  // VAULT & SUBACCOUNT SUPPORT
  // ============================================================================

  /**
   * Set vault address for trading from vault
   */
  setVaultAddress(vaultAddress: string) {
    this.exchange.options = {
      ...this.exchange.options,
      vaultAddress,
    };
  }

  /**
   * Set subaccount address
   */
  setSubAccountAddress(subAccountAddress: string) {
    this.exchange.options = {
      ...this.exchange.options,
      subAccountAddress,
    };
  }

  /**
   * Get vault balance
   */
  async fetchVaultBalance(vaultAddress: string) {
    try {
      await this.ensureAuthenticated();
      this.setVaultAddress(vaultAddress);
      return await this.fetchBalance();
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // ENHANCED ORDER METHODS (HyperLiquid-specific)
  // ============================================================================

  /**
   * Create stop-loss order
   * HyperLiquid uses stop-loss-limit orders
   */
  async createStopLossOrder(
    symbol: string,
    side: OrderSide,
    amount: number,
    stopPrice: number,
    limitPrice?: number
  ) {
    const price = limitPrice || stopPrice;
    return await this.createOrder(symbol, 'stop_limit', side, amount, price, {
      stopPrice,
      stopLossPrice: stopPrice,
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
    const price = limitPrice || stopPrice;
    return await this.createOrder(symbol, 'stop_limit', side, amount, price, {
      stopPrice,
      takeProfitPrice: stopPrice,
    });
  }

  /**
   * Create reduce-only order
   */
  async createReduceOnlyOrder(
    symbol: string,
    type: OrderType,
    side: OrderSide,
    amount: number,
    price?: number
  ) {
    return await this.createOrder(symbol, type, side, amount, price, {
      reduceOnly: true,
    });
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
    return await this.createOrder(symbol, 'limit', side, amount, price, {
      postOnly: true,
    });
  }

  // ============================================================================
  // LEVERAGE & MARGIN
  // ============================================================================

  /**
   * Set leverage for symbol (1x to 50x)
   */
  async setLeverage(symbol: string, leverage: number) {
    try {
      await this.ensureAuthenticated();
      if (leverage < 1 || leverage > 50) {
        throw new Error('HyperLiquid leverage must be between 1x and 50x');
      }
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
  // TRADING METHODS (Extended)
  // ============================================================================

  /**
   * Fetch trading fee for specific symbol
   */
  async fetchTradingFee(symbol: string) {
    try {
      await this.ensureAuthenticated();
      const fees = await this.exchange.fetchTradingFees();
      return fees[symbol] || null;
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Fetch funding rates for multiple symbols
   */
  async fetchFundingRates(symbols?: string[]) {
    try {
      await this.ensureConnected();
      if (!this.exchange.has['fetchFundingRates']) {
        if (!symbols) {
          const markets = await this.fetchMarkets();
          symbols = markets.filter((m: any) => m.swap).map((m: any) => m.symbol);
        }
        const results: any = {};
        for (const symbol of symbols) {
          try {
            results[symbol] = await this.fetchFundingRate(symbol);
          } catch (e) {
            // Skip symbols without funding rates
          }
        }
        return results;
      }
      return await this.exchange.fetchFundingRates(symbols);
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
   * Fetch specific order
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
   * Fetch orders by IDs
   */
  async fetchOrdersByIds(orderIds: string[], symbol?: string) {
    try {
      await this.ensureAuthenticated();
      const orders = [];
      for (const orderId of orderIds) {
        if (!symbol) throw new Error('Symbol required');
        const order = await this.fetchOrder(orderId, symbol);
        orders.push(order);
      }
      return orders;
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Fetch order trades
   */
  async fetchOrderTrades(orderId: string, symbol: string) {
    try {
      await this.ensureAuthenticated();
      return await this.exchange.fetchOrderTrades(orderId, symbol);
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
      const results = [];
      for (const orderId of orderIds) {
        if (!symbol) throw new Error('Symbol required');
        const result = await this.cancelOrder(orderId, symbol);
        results.push(result);
      }
      return results;
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Create market order with cost
   * Note: HyperLiquid simulates market orders
   */
  async createMarketBuyOrderWithCost(symbol: string, cost: number) {
    try {
      await this.ensureAuthenticated();
      // Fetch current price
      const ticker = await this.fetchTicker(symbol);
      const price = ticker.ask || ticker.last;
      if (!price) throw new Error('Cannot determine price');

      // Calculate amount with 5% slippage buffer
      const amount = cost / (price * 1.05);

      return await this.createOrder(symbol, 'market', 'buy', amount);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // BULK ORDER OPERATIONS (HyperLiquid-specific)
  // ============================================================================

  /**
   * Create multiple orders in bulk (REST)
   */
  async createOrdersRequest(orders: Array<{
    symbol: string;
    type: string;
    side: string;
    amount: number;
    price?: number;
    params?: any;
  }>) {
    try {
      await this.ensureAuthenticated();
      if (!this.exchange.has['createOrders']) {
        return await this.createOrders(orders as any);
      }
      return await (this.exchange as any).createOrdersRequest(orders);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Edit multiple orders in bulk
   */
  async editOrders(orders: Array<{
    id: string;
    symbol: string;
    type?: string;
    side?: string;
    amount?: number;
    price?: number;
    params?: any;
  }>) {
    try {
      await this.ensureAuthenticated();
      const results = [];
      for (const order of orders) {
        const result = await this.editOrder(
          order.id,
          order.symbol,
          order.type,
          order.side,
          order.amount,
          order.price,
          order.params
        );
        results.push(result);
      }
      return results;
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Cancel orders request
   */
  async cancelOrdersRequest(orderIds: string[], symbol?: string) {
    try {
      await this.ensureAuthenticated();
      return await this.cancelOrders(orderIds, symbol);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Cancel orders for specific symbols
   */
  async cancelOrdersForSymbols(symbols: string[]) {
    try {
      await this.ensureAuthenticated();
      const results = [];
      for (const symbol of symbols) {
        const canceled = await this.cancelAllOrders(symbol);
        results.push(...canceled);
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
      if (!this.exchange.has['cancelAllOrdersAfter']) {
        throw new Error('cancelAllOrdersAfter not supported');
      }
      return await (this.exchange as any).cancelAllOrdersAfter(timeout);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // ORDER FETCHING (Extended)
  // ============================================================================

  /**
   * Fetch single position for a symbol
   */
  async fetchPosition(symbol: string) {
    try {
      await this.ensureAuthenticated();
      const positions = await this.fetchPositions([symbol]);
      return positions.find((p: any) => p.symbol === symbol) || null;
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Fetch all orders (all states)
   */
  async fetchOrders(symbol?: string, since?: number, limit?: number) {
    try {
      await this.ensureAuthenticated();
      if (!this.exchange.has['fetchOrders']) {
        const open = await this.fetchOpenOrders(symbol);
        const closed = await this.fetchClosedOrders(symbol, limit);
        return [...open, ...closed];
      }
      return await this.exchange.fetchOrders(symbol, since, limit);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Fetch canceled orders
   */
  async fetchCanceledOrders(symbol?: string, since?: number, limit?: number) {
    try {
      await this.ensureAuthenticated();
      if (!this.exchange.has['fetchCanceledOrders']) {
        const orders = await this.fetchOrders(symbol, since, limit);
        return orders.filter((o: any) => o.status === 'canceled');
      }
      return await (this.exchange as any).fetchCanceledOrders(symbol, since, limit);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Fetch canceled and closed orders
   */
  async fetchCanceledAndClosedOrders(symbol?: string, since?: number, limit?: number) {
    try {
      await this.ensureAuthenticated();
      const orders = await this.fetchOrders(symbol, since, limit);
      return orders.filter((o: any) => o.status === 'canceled' || o.status === 'closed');
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // MARGIN OPERATIONS (Isolated margin)
  // ============================================================================

  /**
   * Add margin to position
   */
  async addMargin(symbol: string, amount: number) {
    try {
      await this.ensureAuthenticated();
      if (!this.exchange.has['addMargin']) {
        throw new Error('addMargin not supported');
      }
      return await this.exchange.addMargin(symbol, amount);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Reduce margin from position
   */
  async reduceMargin(symbol: string, amount: number) {
    try {
      await this.ensureAuthenticated();
      if (!this.exchange.has['reduceMargin']) {
        throw new Error('reduceMargin not supported');
      }
      return await this.exchange.reduceMargin(symbol, amount);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // VAULT OPERATIONS (HyperLiquid-specific)
  // ============================================================================

  /**
   * Create a new vault
   */
  async createVault(name: string, params?: any) {
    try {
      await this.ensureAuthenticated();
      if (!this.exchange.has['createVault']) {
        throw new Error('createVault not supported - use HyperLiquid web interface');
      }
      return await (this.exchange as any).createVault(name, params);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // MARKET DATA (Extended HyperLiquid methods)
  // ============================================================================

  /**
   * Fetch HIP-3 markets (spot markets)
   */
  async fetchHip3Markets() {
    try {
      await this.ensureConnected();
      if (!this.exchange.has['fetchHip3Markets']) {
        return await this.fetchSpotMarkets();
      }
      return await (this.exchange as any).fetchHip3Markets();
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Fetch swap/perpetual markets
   */
  async fetchSwapMarkets() {
    try {
      await this.ensureConnected();
      const markets = await this.fetchMarkets();
      return markets.filter((m: any) => m.swap || m.type === 'swap');
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Fetch spot markets
   */
  async fetchSpotMarkets() {
    try {
      await this.ensureConnected();
      const markets = await this.fetchMarkets();
      return markets.filter((m: any) => m.spot || m.type === 'spot');
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Calculate price precision for a symbol
   */
  calculatePricePrecision(symbol: string): number {
    const market = this.exchange.markets[symbol];
    if (!market) return 8;
    return market.precision?.price || 8;
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
   * Fetch open interests for multiple symbols
   */
  async fetchOpenInterests(symbols?: string[]) {
    try {
      await this.ensureConnected();
      if (!this.exchange.has['fetchOpenInterests']) {
        if (!symbols) throw new Error('Symbols required');
        const results: any[] = [];
        for (const symbol of symbols) {
          const oi = await this.fetchOpenInterest(symbol);
          results.push(oi);
        }
        return results;
      }
      return await (this.exchange as any).fetchOpenInterests(symbols);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Fetch funding history
   */
  async fetchFundingHistory(symbol?: string, since?: number, limit?: number) {
    try {
      await this.ensureConnected();
      if (!this.exchange.has['fetchFundingHistory']) {
        return await this.fetchFundingRateHistory(symbol!, since, limit);
      }
      return await (this.exchange as any).fetchFundingHistory(symbol, since, limit);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Reserve request weight for rate limiting
   */
  reserveRequestWeight(weight: number) {
    // HyperLiquid specific rate limit management
    if ((this.exchange as any).reserveRequestWeight) {
      return (this.exchange as any).reserveRequestWeight(weight);
    }
    return true;
  }

  // ============================================================================
  // WEBSOCKET METHODS (Watch functions)
  // ============================================================================

  /**
   * Create order via WebSocket
   */
  async createOrderWs(
    symbol: string,
    type: string,
    side: string,
    amount: number,
    price?: number,
    params?: any
  ) {
    try {
      await this.ensureAuthenticated();
      if (!this.exchange.has['createOrderWs']) {
        return await this.createOrder(symbol, type as any, side as any, amount, price, params);
      }
      return await (this.exchange as any).createOrderWs(symbol, type, side, amount, price, params);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Create multiple orders via WebSocket
   */
  async createOrdersWs(orders: any[]) {
    try {
      await this.ensureAuthenticated();
      if (!this.exchange.has['createOrdersWs']) {
        return await this.createOrdersRequest(orders);
      }
      return await (this.exchange as any).createOrdersWs(orders);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Edit order via WebSocket
   */
  async editOrderWs(
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
      if (!this.exchange.has['editOrderWs']) {
        return await this.editOrder(orderId, symbol, type, side, amount, price, params);
      }
      return await (this.exchange as any).editOrderWs(orderId, symbol, type, side, amount, price, params);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Cancel order via WebSocket
   */
  async cancelOrderWs(orderId: string, symbol: string, params?: any) {
    try {
      await this.ensureAuthenticated();
      if (!this.exchange.has['cancelOrderWs']) {
        return await this.cancelOrder(orderId, symbol);
      }
      return await (this.exchange as any).cancelOrderWs(orderId, symbol, params);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Cancel multiple orders via WebSocket
   */
  async cancelOrdersWs(orderIds: string[], symbol?: string, params?: any) {
    try {
      await this.ensureAuthenticated();
      if (!this.exchange.has['cancelOrdersWs']) {
        return await this.cancelOrders(orderIds, symbol);
      }
      return await (this.exchange as any).cancelOrdersWs(orderIds, symbol, params);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Watch order book (requires ccxt.pro)
   */
  async watchOrderBook(symbol: string, limit?: number) {
    try {
      if (!this.exchange.has['watchOrderBook']) {
        throw new Error('watchOrderBook requires ccxt.pro package');
      }
      return await (this.exchange as any).watchOrderBook(symbol, limit);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Unwatch order book
   */
  async unWatchOrderBook(symbol: string) {
    try {
      if (!this.exchange.has['unwatch']) {
        return;
      }
      return await (this.exchange as any).unwatch('orderbook', symbol);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Watch ticker (requires ccxt.pro)
   */
  async watchTicker(symbol: string) {
    try {
      if (!this.exchange.has['watchTicker']) {
        throw new Error('watchTicker requires ccxt.pro package');
      }
      return await (this.exchange as any).watchTicker(symbol);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Watch multiple tickers
   */
  async watchTickers(symbols?: string[]) {
    try {
      if (!this.exchange.has['watchTickers']) {
        throw new Error('watchTickers requires ccxt.pro package');
      }
      return await (this.exchange as any).watchTickers(symbols);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Unwatch tickers
   */
  async unWatchTickers(symbols?: string[]) {
    try {
      if (!this.exchange.has['unwatch']) {
        return;
      }
      if (symbols) {
        for (const symbol of symbols) {
          await (this.exchange as any).unwatch('ticker', symbol);
        }
      }
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Watch my trades
   */
  async watchMyTrades(symbol?: string) {
    try {
      if (!this.exchange.has['watchMyTrades']) {
        throw new Error('watchMyTrades requires ccxt.pro package');
      }
      return await (this.exchange as any).watchMyTrades(symbol);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Watch public trades
   */
  async watchTrades(symbol: string) {
    try {
      if (!this.exchange.has['watchTrades']) {
        throw new Error('watchTrades requires ccxt.pro package');
      }
      return await (this.exchange as any).watchTrades(symbol);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Unwatch trades
   */
  async unWatchTrades(symbol: string) {
    try {
      if (!this.exchange.has['unwatch']) {
        return;
      }
      return await (this.exchange as any).unwatch('trades', symbol);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Watch OHLCV candles
   */
  async watchOHLCV(symbol: string, timeframe: string = '1m', since?: number, limit?: number) {
    try {
      if (!this.exchange.has['watchOHLCV']) {
        throw new Error('watchOHLCV requires ccxt.pro package');
      }
      return await (this.exchange as any).watchOHLCV(symbol, timeframe, since, limit);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Unwatch OHLCV candles
   */
  async unWatchOHLCV(symbol: string, timeframe: string = '1m') {
    try {
      if (!this.exchange.has['unwatch']) {
        return;
      }
      return await (this.exchange as any).unwatch('ohlcv', symbol, timeframe);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Watch orders (requires ccxt.pro)
   */
  async watchOrders(symbol?: string) {
    try {
      if (!this.exchange.has['watchOrders']) {
        throw new Error('watchOrders requires ccxt.pro package');
      }
      return await (this.exchange as any).watchOrders(symbol);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // DEPOSITS & WITHDRAWALS (L1 Bridge operations)
  // ============================================================================

  /**
   * Fetch deposit address (L1 bridge address)
   */
  async fetchDepositAddress(currency: string, params?: any) {
    try {
      await this.ensureAuthenticated();
      return await this.exchange.fetchDepositAddress(currency, params);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Fetch deposits from L1
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
   * Fetch withdrawals to L1
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
   * Withdraw to L1 (Arbitrum)
   */
  async withdraw(currency: string, amount: number, address: string, tag?: string, params?: any) {
    try {
      await this.ensureAuthenticated();
      return await this.exchange.withdraw(currency, amount, address, tag, params);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // TRANSFERS (Vault/Subaccount)
  // ============================================================================

  /**
   * Transfer between accounts
   */
  async transfer(currency: string, amount: number, fromAccount: string, toAccount: string) {
    try {
      await this.ensureAuthenticated();
      return await this.exchange.transfer(currency, amount, fromAccount, toAccount);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // FUNDING RATES (Perpetuals)
  // ============================================================================

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
  // UTILITIES
  // ============================================================================

  /**
   * Get popular HyperLiquid trading pairs
   */
  getPopularPairs(): string[] {
    return [
      'BTC/USDC:USDC',
      'ETH/USDC:USDC',
      'SOL/USDC:USDC',
      'ARB/USDC:USDC',
      'AVAX/USDC:USDC',
      'MATIC/USDC:USDC',
      'OP/USDC:USDC',
      'DOGE/USDC:USDC',
      'LINK/USDC:USDC',
      'UNI/USDC:USDC',
    ];
  }

  /**
   * Get system status
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
   * Check if using Coincurve for faster signing
   * (45ms -> 0.05ms signing time improvement)
   */
  isUsingCoincurve(): boolean {
    return !!(this.exchange as any).coincurve;
  }
}
