/**
 * Paper Trading Integration
 * Connects equity trading to paper trading system
 */

import { Plugin, PluginType, PluginContext, PluginResult, createPlugin } from '../core/PluginSystem';
import { OrderRequest, UnifiedOrder, UnifiedPosition, BrokerType, OrderSide, OrderType, ProductType } from '../core/types';
import { createPaperTradingAdapter } from '@/paper-trading';
import type { IExchangeAdapter } from '@/brokers/crypto/types';
import type { PaperTradingConfig } from '@/paper-trading/types';

/**
 * Adapter bridge to make equity brokers compatible with crypto paper trading system
 */
class EquityBrokerBridge implements Partial<IExchangeAdapter> {
  private lastPrices: Map<string, number> = new Map();
  private symbolMetadata: Map<string, { exchange: string; originalSymbol: string }> = new Map();
  private priceUpdateInterval: NodeJS.Timeout | null = null;

  /**
   * Fetch ticker with real-time Fyers price
   */
  async fetchTicker(symbol: string): Promise<any> {

    // Try to fetch fresh price from Fyers if we have metadata
    const metadata = this.symbolMetadata.get(symbol);
    if (metadata) {
      try {
        // Use AuthManager to get authenticated Fyers adapter
        const { authManager } = await import('../services/AuthManager');
        const adapter = authManager.getAdapter('fyers');

        if (!adapter) {
        } else {
          const fyersSymbol = `${metadata.originalSymbol}-EQ`;
          const quote = await adapter.getQuote(fyersSymbol, metadata.exchange);

          if (quote && quote.lastPrice && quote.lastPrice > 0) {
            const ltp = quote.lastPrice;
            this.lastPrices.set(symbol, ltp);
            return {
              symbol,
              last: ltp,
              bid: ltp * 0.999,
              ask: ltp * 1.001,
              high: quote.high || ltp * 1.02,
              low: quote.low || ltp * 0.98,
              volume: quote.volume || 1000000,
            };
          }
        }
      } catch (error) {
      }
    } else {
    }

    // Fallback to cached price
    const last = this.lastPrices.get(symbol) || 100;
    return {
      symbol,
      last,
      bid: last * 0.999,
      ask: last * 1.001,
      high: last * 1.02,
      low: last * 0.98,
      volume: 1000000,
    };
  }

  /**
   * Update price and store metadata for future fetches
   */
  updatePrice(symbol: string, price: number, exchange: string = 'NSE'): void {
    // Update the price in the cache
    this.lastPrices.set(symbol, price);

    // Store metadata for future price fetches
    const cleanSymbol = symbol.replace('/INR', '');
    this.symbolMetadata.set(symbol, {
      exchange,
      originalSymbol: cleanSymbol,
    });

  }

  /**
   * Start periodic price updates for all tracked symbols
   */
  startPriceUpdates(intervalMs: number = 5000): void {
    if (this.priceUpdateInterval) {
      clearInterval(this.priceUpdateInterval);
    }

    this.priceUpdateInterval = setInterval(async () => {
      const symbols = Array.from(this.symbolMetadata.keys());
      if (symbols.length === 0) return;


      for (const symbol of symbols) {
        try {
          await this.fetchTicker(symbol);
        } catch (error) {
          console.error('[EquityBrokerBridge] Failed to update price for', symbol, error);
        }
      }
    }, intervalMs);

  }

  /**
   * Stop periodic price updates
   */
  stopPriceUpdates(): void {
    if (this.priceUpdateInterval) {
      clearInterval(this.priceUpdateInterval);
      this.priceUpdateInterval = null;
    }
  }

  async connect(): Promise<void> {
    this.startPriceUpdates(5000); // Update prices every 5 seconds
  }

  async disconnect(): Promise<void> {
    this.stopPriceUpdates();
  }

  isConnected(): boolean {
    return true;
  }

  async fetchMarkets(): Promise<any[]> {
    return [];
  }

  async fetchTickers(): Promise<any> {
    return {};
  }

  async fetchOrderBook(): Promise<any> {
    return { bids: [], asks: [] };
  }

  async fetchTrades(): Promise<any[]> {
    return [];
  }

  async fetchOHLCV(): Promise<any[]> {
    return [];
  }
}

export class PaperTradingIntegration {
  private paperAdapter: any; // PaperTradingAdapter instance
  private bridgeAdapter: EquityBrokerBridge;
  private enabled: boolean = false;
  private virtualBroker: BrokerType = 'fyers';
  private config: PaperTradingConfig;

  constructor() {
    this.bridgeAdapter = new EquityBrokerBridge();

    // Default configuration for equity paper trading
    this.config = {
      portfolioId: 'equity-paper-trading',
      portfolioName: 'Equity Paper Trading Portfolio',
      provider: 'kraken', // Use valid CCXT exchange (required by BaseExchangeAdapter, not actually used)
      assetClass: 'stocks',
      initialBalance: 1000000, // 10 lakhs
      currency: 'INR',
      fees: {
        maker: 0.0003, // 0.03% (typical Indian broker fees)
        taker: 0.0003,
      },
      slippage: {
        market: 0.0005, // 0.05% base slippage
        limit: 0,
        modelType: 'size-dependent',
        sizeImpactFactor: 0.00005,
      },
      defaultLeverage: 1, // No leverage for equity delivery
      marginMode: 'cross',
      enableRealtimeUpdates: true,
      priceUpdateInterval: 500, // 500ms for stocks
    };
  }

  /**
   * Create paper trading plugin
   */
  createPlugin(): Plugin {
    return createPlugin({
      id: 'paper-trading',
      name: 'Paper Trading',
      type: PluginType.PRE_ORDER,
      version: '3.1.0', // Updated to match paper trading system version
      execute: async (context: PluginContext): Promise<PluginResult> => {

        if (!this.enabled) {
          return { success: true };
        }

        // Intercept order and route to paper trading
        if (context.type === PluginType.PRE_ORDER) {
          const order = context.data as OrderRequest;

          try {
            // Route to paper trading adapter
            const result = await this.executePaperOrder(order);

            // Cancel real order
            if (context.cancel) {
              context.cancel();
            }


            return {
              success: true,
              data: {
                mode: 'paper',
                message: 'Order sent to paper trading',
                orderId: result.id,
                status: result.status,
              },
            };
          } catch (error: any) {
            console.error('[PaperTrading Plugin] ❌ Error:', error);
            return {
              success: false,
              error: error.message,
            };
          }
        }

        return { success: true };
      },
      onEnable: async () => {
        this.enabled = true;
        await this.initializePaperAdapter();
      },
      onDisable: async () => {
        this.enabled = false;
        if (this.paperAdapter) {
          await this.paperAdapter.disconnect();
        }
      },
    });
  }

  /**
   * Initialize paper trading adapter
   */
  private async initializePaperAdapter(): Promise<void> {
    try {

      // Create paper trading adapter
      this.paperAdapter = createPaperTradingAdapter(this.config, this.bridgeAdapter as any);

      // Connect adapter
      await this.paperAdapter.connect();

    } catch (error) {
      console.error('[PaperTrading] ❌ Failed to initialize adapter:', error);
      throw error;
    }
  }

  /**
   * Fetch current market price from Fyers using AuthManager
   */
  private async fetchFyersPrice(symbol: string, exchange: string): Promise<number> {
    try {

      // Use AuthManager to get authenticated Fyers adapter
      const { authManager } = await import('../services/AuthManager');
      const adapter = authManager.getAdapter('fyers');

      if (!adapter) {
        return 0;
      }

      // Format symbol for Fyers API (e.g., "SBIN-EQ")
      const fyersSymbol = `${symbol}-EQ`;


      // Get quote using the adapter's getQuote method
      const quote = await adapter.getQuote(fyersSymbol, exchange);


      if (quote && quote.lastPrice && quote.lastPrice > 0) {
        return quote.lastPrice;
      }

      return 0;
    } catch (error) {
      console.error('[PaperTrading] ❌ Failed to fetch price from Fyers:', error);
      return 0;
    }
  }

  /**
   * Execute order in paper trading
   */
  private async executePaperOrder(order: OrderRequest): Promise<any> {
    if (!this.paperAdapter) {
      throw new Error('Paper trading adapter not initialized');
    }


    let currentPrice = 0;

    // For market orders, ALWAYS fetch the current market price from Fyers
    // For limit orders, use the specified limit price
    if (order.type === OrderType.MARKET) {
      const fetchedPrice = await this.fetchFyersPrice(order.symbol, order.exchange);

      if (fetchedPrice > 0) {
        currentPrice = fetchedPrice;
      } else {
        console.error('[PaperTrading] ❌ Failed to fetch market price for', order.symbol);
        throw new Error(`Failed to fetch market price for ${order.symbol}. Cannot execute market order.`);
      }
    } else if (order.type === OrderType.LIMIT) {
      // For limit orders, use the specified price
      currentPrice = order.price || 0;
      if (currentPrice <= 0) {
        throw new Error(`Invalid limit price for ${order.symbol}. Must specify a price for limit orders.`);
      }
    } else {
      // For other order types (stop loss, etc.), try to fetch current price
      const fetchedPrice = await this.fetchFyersPrice(order.symbol, order.exchange);
      currentPrice = fetchedPrice > 0 ? fetchedPrice : (order.price || 0);
      if (currentPrice <= 0) {
        throw new Error(`Failed to determine execution price for ${order.symbol}.`);
      }
    }

    // Convert equity order to paper trading format
    const symbol = `${order.symbol}/INR`; // Convert NSE:SBIN to SBIN/INR format

    // Update current price in bridge with BOTH symbol formats
    this.bridgeAdapter.updatePrice(order.symbol, currentPrice, order.exchange); // Original symbol
    this.bridgeAdapter.updatePrice(symbol, currentPrice, order.exchange); // Paper trading format (SYMBOL/INR)

    const side = order.side === OrderSide.BUY ? 'buy' : 'sell';

    // Map order types
    let orderType: 'market' | 'limit' | 'stop' | 'stop_limit' = 'market';
    if (order.type === OrderType.LIMIT) {
      orderType = 'limit';
    } else if (order.type === OrderType.STOP_LOSS || order.type === OrderType.STOP_LOSS_MARKET) {
      orderType = 'stop';
    }

    // Determine leverage based on product type
    let leverage = 1;
    if (order.product === ProductType.MIS || order.product === ProductType.MARGIN) {
      leverage = 5; // 5x leverage for intraday/margin
    }

    // Place order via paper trading adapter
    const result = await this.paperAdapter.createOrder(
      symbol,
      orderType,
      side,
      order.quantity,
      currentPrice, // Use the determined current price (market price for market orders, limit price for limit orders)
      {
        stopPrice: order.triggerPrice,
        leverage,
        marginMode: order.product === ProductType.MIS ? 'isolated' : 'cross',
        clientOrderId: order.tag,
      }
    );

    return result;
  }

  /**
   * Get paper trading balance
   */
  async getBalance(): Promise<number> {
    if (!this.paperAdapter) {
      return this.config.initialBalance;
    }

    try {
      const balance = await this.paperAdapter.fetchBalance();
      const currency = this.config.currency || 'INR';


      const balanceValue = balance.total?.[currency] || balance.total?.['USD'] || this.config.initialBalance;

      return balanceValue;
    } catch (error) {
      console.error('[PaperTrading] Failed to fetch balance:', error);
      return this.config.initialBalance;
    }
  }

  /**
   * Get paper positions
   */
  async getPositions(): Promise<UnifiedPosition[]> {
    if (!this.paperAdapter) {
      return [];
    }

    try {
      const positions = await this.paperAdapter.fetchPositions();

      // Convert paper trading positions to unified format
      return positions.map((pos: any) => ({
        id: pos.id,
        brokerId: 'paper' as BrokerType,
        symbol: pos.symbol.replace('/INR', ''), // Convert SBIN/INR back to SBIN
        exchange: 'NSE',
        product: pos.leverage > 1 ? ProductType.MIS : ProductType.CNC,
        quantity: pos.contracts,
        averagePrice: pos.entryPrice,
        lastPrice: pos.markPrice || pos.entryPrice,
        pnl: (pos.unrealizedPnl || 0) + (pos.realizedPnl || 0),
        realizedPnl: pos.realizedPnl || 0,
        unrealizedPnl: pos.unrealizedPnl || 0,
        side: pos.side === 'long' ? OrderSide.BUY : OrderSide.SELL,
        value: pos.notional || (pos.contracts * pos.entryPrice),
      }));
    } catch (error: any) {
      // Silently return empty array if adapter not initialized (happens during startup)
      if (error.message?.includes('not initialized')) {
        return [];
      }
      console.error('[PaperTrading] Failed to fetch positions:', error);
      return [];
    }
  }

  /**
   * Get paper trading orders (both open and recently closed)
   */
  async getOrders(): Promise<UnifiedOrder[]> {
    if (!this.paperAdapter) {
      return [];
    }

    try {
      // Get both open and closed orders to show order history
      const openOrders = await this.paperAdapter.fetchOpenOrders();
      const closedOrders = await this.paperAdapter.fetchClosedOrders();
      const orders = [...openOrders, ...closedOrders];

      // Convert to unified format
      return orders.map((order: any) => ({
        id: order.id,
        brokerId: 'paper' as BrokerType,
        symbol: order.symbol.replace('/INR', ''),
        exchange: 'NSE',
        side: order.side === 'buy' ? OrderSide.BUY : OrderSide.SELL,
        type: this.mapOrderType(order.type),
        quantity: order.amount,
        price: order.price,
        triggerPrice: order.stopPrice,
        product: order.leverage > 1 ? ProductType.MIS : ProductType.CNC,
        validity: 'DAY' as any,
        status: this.mapOrderStatus(order.status),
        filledQuantity: order.filled || 0,
        pendingQuantity: order.remaining || order.amount,
        averagePrice: order.average || 0,
        orderTime: new Date(order.timestamp),
        tag: order.clientOrderId,
      }));
    } catch (error) {
      console.error('[PaperTrading] Failed to fetch orders:', error);
      return [];
    }
  }

  /**
   * Get paper trading statistics
   */
  async getStatistics(): Promise<any> {
    if (!this.paperAdapter) {
      return null;
    }

    try {
      return await this.paperAdapter.getStatistics();
    } catch (error) {
      console.error('[PaperTrading] Failed to fetch statistics:', error);
      return null;
    }
  }

  /**
   * Reset paper account
   */
  async reset(initialBalance: number = 1000000): Promise<void> {

    if (this.paperAdapter) {
      await this.paperAdapter.resetAccount();
    }

    // Update config with INR currency
    this.config.initialBalance = initialBalance;
    this.config.currency = 'INR'; // Ensure INR is set

    // Reinitialize if enabled
    if (this.enabled) {
      await this.initializePaperAdapter();
    }
  }

  /**
   * Check if adapter is initialized
   */
  isInitialized(): boolean {
    return this.paperAdapter !== null && this.enabled;
  }

  // Helper methods
  private mapOrderType(type: string): OrderType {
    switch (type) {
      case 'limit': return OrderType.LIMIT;
      case 'stop':
      case 'stop_limit': return OrderType.STOP_LOSS;
      default: return OrderType.MARKET;
    }
  }

  private mapOrderStatus(status: string): any {
    switch (status) {
      case 'open':
      case 'pending': return 'OPEN';
      case 'closed':
      case 'filled': return 'COMPLETE';
      case 'canceled': return 'CANCELLED';
      case 'rejected': return 'REJECTED';
      default: return 'PENDING';
    }
  }
}

export const paperTradingIntegration = new PaperTradingIntegration();
