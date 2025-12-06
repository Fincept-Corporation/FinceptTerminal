/**
 * Kraken Exchange Adapter
 *
 * Professional-grade Kraken integration using CCXT
 * Supports: Spot, Margin, Futures, Staking, Earn
 */

import ccxt from 'ccxt';
import { BaseExchangeAdapter } from '../BaseExchangeAdapter';
import type {
  ExchangeConfig,
  OrderType,
  OrderSide,
  OrderParams,
  ExchangeCapabilities,
} from '../types';

export class KrakenAdapter extends BaseExchangeAdapter {
  constructor(config?: Partial<ExchangeConfig>) {
    super({
      exchange: 'kraken',
      enableRateLimit: true,
      timeout: 30000,
      ...config,
    });
  }

  // ============================================================================
  // KRAKEN-SPECIFIC FEATURES
  // ============================================================================

  /**
   * Get Kraken-specific capabilities
   */
  getCapabilities(): ExchangeCapabilities {
    const base = super.getCapabilities();
    return {
      ...base,
      spot: true,
      margin: true,
      futures: true,
      swap: false,
      options: false,

      supportedOrderTypes: [
        'market',
        'limit',
        'stop',
        'stop_limit',
        'trailing_stop',
        'iceberg',
      ],

      leverageTrading: true,
      stopOrders: true,
      conditionalOrders: true,
      algoOrders: true,

      realtimeData: true,
      historicalData: true,
      websocketSupport: true,

      rateLimits: {
        public: 1000, // 1 req/sec
        private: 3000, // 1 req/3sec
        trading: 1000, // 1 req/sec
      },
    };
  }

  // ============================================================================
  // ENHANCED ORDER METHODS
  // ============================================================================

  /**
   * Create stop-loss order (Kraken-specific)
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
   * Create take-profit order (Kraken-specific)
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
      trigger: 'last',
    });
  }

  /**
   * Create trailing stop order
   */
  async createTrailingStopOrder(
    symbol: string,
    side: OrderSide,
    amount: number,
    trailingPercent: number
  ) {
    return await this.createOrder(symbol, 'trailing_stop', side, amount, undefined, {
      trailingPercent,
    });
  }

  /**
   * Create iceberg order (hidden liquidity)
   */
  async createIcebergOrder(
    symbol: string,
    side: OrderSide,
    amount: number,
    price: number,
    visibleAmount: number
  ) {
    return await this.createOrder(symbol, 'iceberg', side, amount, price, {
      icebergQty: visibleAmount,
    });
  }

  // ============================================================================
  // MARGIN TRADING
  // ============================================================================

  /**
   * Set leverage for symbol
   */
  async setLeverage(symbol: string, leverage: number) {
    try {
      await this.ensureAuthenticated();
      if (!this.exchange.has['setLeverage']) {
        throw new Error('Leverage not supported');
      }
      return await this.exchange.setLeverage(leverage, symbol);
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
    leverage?: number
  ) {
    const params: OrderParams = {
      leverage,
      marginMode: 'cross',
    };
    return await this.createOrder(symbol, type, side, amount, price, params);
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

  // ============================================================================
  // FUNDING & TRANSFERS
  // ============================================================================

  /**
   * Transfer between spot and margin accounts
   */
  async transfer(
    currency: string,
    amount: number,
    fromAccount: 'spot' | 'margin' | 'futures',
    toAccount: 'spot' | 'margin' | 'futures'
  ) {
    try {
      await this.ensureAuthenticated();
      if (!this.exchange.has['transfer']) {
        throw new Error('Transfers not supported');
      }
      return await this.exchange.transfer(currency, amount, fromAccount, toAccount);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Fetch funding history
   */
  async fetchFundingHistory(symbol?: string, since?: number, limit?: number) {
    try {
      await this.ensureAuthenticated();
      if (!this.exchange.has['fetchFundingHistory']) {
        throw new Error('Funding history not available');
      }
      return await this.exchange.fetchFundingHistory(symbol, since, limit);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // STAKING & EARN (Kraken-specific)
  // ============================================================================

  /**
   * Fetch staking assets
   */
  async fetchStakingAssets() {
    try {
      await this.ensureAuthenticated();
      // Use private API endpoint
      return await (this.exchange as any).privatePostStakingAssets();
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Stake asset
   */
  async stakeAsset(asset: string, amount: number, method: string) {
    try {
      await this.ensureAuthenticated();
      return await (this.exchange as any).privatePostStakingStake({
        asset,
        amount: amount.toString(),
        method,
      });
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Unstake asset
   */
  async unstakeAsset(asset: string, amount: number) {
    try {
      await this.ensureAuthenticated();
      return await (this.exchange as any).privatePostStakingUnstake({
        asset,
        amount: amount.toString(),
      });
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // KRAKEN FUTURES
  // ============================================================================

  /**
   * Fetch futures markets
   */
  async fetchFuturesMarkets() {
    try {
      // Switch to futures endpoint
      const futuresExchange = new ccxt.krakenfutures({
        apiKey: this.exchange.apiKey,
        secret: this.exchange.secret,
        enableRateLimit: true,
      });
      return await futuresExchange.loadMarkets();
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // HISTORICAL DATA (Enhanced)
  // ============================================================================

  /**
   * Fetch historical OHLCV with extended options
   */
  async fetchHistoricalOHLCV(
    symbol: string,
    timeframe: '1m' | '5m' | '15m' | '30m' | '1h' | '4h' | '1d' | '1w',
    since?: number,
    limit: number = 720
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
        exchange: 'kraken',
        timestamp: trade.timestamp || Date.now(),
      }));
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // ACCOUNT INFORMATION & FEES
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
      const fees = await this.exchange.fetchTradingFees();
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
  // DEPOSITS & WITHDRAWALS (Kraken-specific)
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
   * Fetch deposit methods
   */
  async fetchDepositMethods(currency?: string) {
    try {
      await this.ensureAuthenticated();
      return await (this.exchange as any).fetchDepositMethods(currency);
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
  // ADVANCED ORDER METHODS (Kraken-specific)
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
        if (!symbol) throw new Error('Symbol required for Kraken fetchOrdersByIds');
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
        if (!symbol) throw new Error('Symbol required for Kraken bulk cancel');
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
      return await (this.exchange as any).cancelAllOrdersAfter(timeout);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  /**
   * Create market order with cost (buy specific USD amount)
   */
  async createMarketBuyOrderWithCost(symbol: string, cost: number) {
    try {
      await this.ensureAuthenticated();
      return await (this.exchange as any).createMarketBuyOrderWithCost(symbol, cost);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // UTILITIES
  // ============================================================================

  /**
   * Get Kraken system status
   */
  async getSystemStatus() {
    try {
      return await (this.exchange as any).publicGetSystemStatus();
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
   * Convert Kraken pair format to standard format
   * Example: XXBTZUSD -> BTC/USD
   */
  normalizeSymbol(krakenSymbol: string): string {
    const markets = this.exchange.markets;
    for (const symbol in markets) {
      if (markets[symbol].id === krakenSymbol) {
        return symbol;
      }
    }
    return krakenSymbol;
  }

  /**
   * Get popular Kraken trading pairs
   */
  getPopularPairs(): string[] {
    return [
      'BTC/USD',
      'ETH/USD',
      'XRP/USD',
      'SOL/USD',
      'ADA/USD',
      'MATIC/USD',
      'AVAX/USD',
      'DOT/USD',
      'LINK/USD',
      'UNI/USD',
      'BTC/EUR',
      'ETH/EUR',
      'BTC/USDT',
      'ETH/USDT',
    ];
  }
}
