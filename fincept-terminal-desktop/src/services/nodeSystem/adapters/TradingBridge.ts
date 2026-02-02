/**
 * Trading Bridge
 * Connects workflow nodes to broker adapters for order execution
 * Supports stocks, crypto, paper trading with safety controls
 */

import { IDataObject } from '../types';

// ================================
// TYPES
// ================================

export type OrderSide = 'BUY' | 'SELL';
export type OrderType = 'MARKET' | 'LIMIT' | 'STOP_LOSS' | 'STOP_LOSS_MARKET';
export type ProductType = 'CNC' | 'MIS' | 'NRML' | 'MARGIN' | 'INTRADAY';
export type OrderValidity = 'DAY' | 'IOC' | 'GTC' | 'GTD';
export type OrderStatus = 'PENDING' | 'OPEN' | 'COMPLETE' | 'CANCELLED' | 'REJECTED' | 'TRIGGER_PENDING';

export type BrokerType =
  | 'paper'
  | 'kite'
  | 'fyers'
  | 'alpaca'
  | 'binance'
  | 'kraken'
  | 'coinbase'
  | 'hyperliquid';

export interface OrderRequest {
  symbol: string;
  exchange?: string;
  side: OrderSide;
  quantity: number;
  type: OrderType;
  price?: number;
  triggerPrice?: number;
  product?: ProductType;
  validity?: OrderValidity;
  disclosedQuantity?: number;
  tag?: string;
}

export interface OrderResponse {
  success: boolean;
  orderId?: string;
  status?: OrderStatus;
  message?: string;
  filledQuantity?: number;
  filledPrice?: number;
  timestamp?: string;
  fees?: number;
}

export interface Position {
  symbol: string;
  exchange?: string;
  quantity: number;
  averagePrice: number;
  currentPrice?: number;
  pnl?: number;
  pnlPercent?: number;
  side: OrderSide;
  product?: ProductType;
  timestamp?: string;
}

export interface Holding {
  symbol: string;
  exchange?: string;
  quantity: number;
  averagePrice: number;
  currentPrice?: number;
  pnl?: number;
  pnlPercent?: number;
  investedValue?: number;
  currentValue?: number;
  timestamp?: string;
}

export interface AccountBalance {
  currency: string;
  available: number;
  used: number;
  total: number;
  margin?: number;
  marginUsed?: number;
}

export interface Order {
  orderId: string;
  symbol: string;
  exchange?: string;
  side: OrderSide;
  type: OrderType;
  quantity: number;
  price?: number;
  triggerPrice?: number;
  filledQuantity: number;
  filledPrice?: number;
  status: OrderStatus;
  product?: ProductType;
  validity?: OrderValidity;
  timestamp: string;
  tag?: string;
}

// ================================
// TRADING BRIDGE
// ================================

class TradingBridgeClass {
  private adapterCache: Map<string, any> = new Map();
  private paperTradingEnabled: boolean = true;
  private activePositions: Map<string, Position> = new Map();
  private paperBalance: number = 1000000; // $1M paper trading balance

  /**
   * Set paper trading mode
   */
  setPaperTradingMode(enabled: boolean): void {
    this.paperTradingEnabled = enabled;
    console.log(`[TradingBridge] Paper trading mode: ${enabled ? 'ENABLED' : 'DISABLED'}`);
  }

  /**
   * Check if paper trading is enabled
   */
  isPaperTradingEnabled(): boolean {
    return this.paperTradingEnabled;
  }

  /**
   * Place an order
   */
  async placeOrder(
    order: OrderRequest,
    broker: BrokerType = 'paper'
  ): Promise<OrderResponse> {
    // Force paper trading if enabled
    if (this.paperTradingEnabled && broker !== 'paper') {
      console.log('[TradingBridge] Routing to paper trading (paper trading mode enabled)');
      broker = 'paper';
    }

    try {
      if (broker === 'paper') {
        return this.executePaperOrder(order);
      }

      const adapter = await this.getAdapter(broker);
      if (!adapter) {
        throw new Error(`Broker adapter '${broker}' not available`);
      }

      // Execute real order through adapter
      const result = await adapter.placeOrder(this.formatOrderForBroker(order, broker));

      return this.normalizeOrderResponse(result);
    } catch (error: any) {
      console.error('[TradingBridge] Order placement failed:', error);
      return {
        success: false,
        message: error.message || 'Order placement failed',
      };
    }
  }

  /**
   * Modify an existing order
   */
  async modifyOrder(
    orderId: string,
    updates: Partial<OrderRequest>,
    broker: BrokerType = 'paper'
  ): Promise<OrderResponse> {
    if (this.paperTradingEnabled) broker = 'paper';

    try {
      if (broker === 'paper') {
        // Paper trading modification (simplified)
        return {
          success: true,
          orderId,
          status: 'PENDING',
          message: 'Paper order modified',
        };
      }

      const adapter = await this.getAdapter(broker);
      if (!adapter) {
        throw new Error(`Broker adapter '${broker}' not available`);
      }

      const result = await adapter.modifyOrder(orderId, updates);
      return this.normalizeOrderResponse(result);
    } catch (error: any) {
      return {
        success: false,
        message: error.message || 'Order modification failed',
      };
    }
  }

  /**
   * Cancel an order
   */
  async cancelOrder(
    orderId: string,
    broker: BrokerType = 'paper'
  ): Promise<OrderResponse> {
    if (this.paperTradingEnabled) broker = 'paper';

    try {
      if (broker === 'paper') {
        return {
          success: true,
          orderId,
          status: 'CANCELLED',
          message: 'Paper order cancelled',
        };
      }

      const adapter = await this.getAdapter(broker);
      if (!adapter) {
        throw new Error(`Broker adapter '${broker}' not available`);
      }

      const result = await adapter.cancelOrder(orderId);
      return this.normalizeOrderResponse(result);
    } catch (error: any) {
      return {
        success: false,
        message: error.message || 'Order cancellation failed',
      };
    }
  }

  /**
   * Get open positions
   */
  async getPositions(broker: BrokerType = 'paper'): Promise<Position[]> {
    if (this.paperTradingEnabled) broker = 'paper';

    try {
      if (broker === 'paper') {
        return Array.from(this.activePositions.values());
      }

      const adapter = await this.getAdapter(broker);
      if (!adapter) {
        return [];
      }

      const result = await adapter.getPositions();
      return this.normalizePositions(result);
    } catch (error) {
      console.error('[TradingBridge] Error fetching positions:', error);
      return [];
    }
  }

  /**
   * Get holdings (long-term positions)
   */
  async getHoldings(broker: BrokerType = 'paper'): Promise<Holding[]> {
    if (this.paperTradingEnabled) broker = 'paper';

    try {
      if (broker === 'paper') {
        // Convert positions to holdings for paper trading
        return Array.from(this.activePositions.values()).map((pos) => ({
          symbol: pos.symbol,
          exchange: pos.exchange,
          quantity: pos.quantity,
          averagePrice: pos.averagePrice,
          currentPrice: pos.currentPrice,
          pnl: pos.pnl,
          pnlPercent: pos.pnlPercent,
          investedValue: pos.quantity * pos.averagePrice,
          currentValue: pos.quantity * (pos.currentPrice || pos.averagePrice),
        }));
      }

      const adapter = await this.getAdapter(broker);
      if (!adapter) {
        return [];
      }

      const result = await adapter.getHoldings();
      return this.normalizeHoldings(result);
    } catch (error) {
      console.error('[TradingBridge] Error fetching holdings:', error);
      return [];
    }
  }

  /**
   * Get account balance
   */
  async getBalance(
    broker: BrokerType = 'paper',
    currency: string = 'USD'
  ): Promise<AccountBalance> {
    if (this.paperTradingEnabled) broker = 'paper';

    try {
      if (broker === 'paper') {
        const usedMargin = this.calculateUsedMargin();
        return {
          currency,
          available: this.paperBalance - usedMargin,
          used: usedMargin,
          total: this.paperBalance,
          margin: this.paperBalance * 4, // 4x margin for paper trading
          marginUsed: usedMargin,
        };
      }

      const adapter = await this.getAdapter(broker);
      if (!adapter) {
        throw new Error(`Broker adapter '${broker}' not available`);
      }

      const result = await adapter.getFunds ? await adapter.getFunds() : await adapter.fetchBalance();
      return this.normalizeBalance(result, currency);
    } catch (error: any) {
      console.error('[TradingBridge] Error fetching balance:', error);
      return {
        currency,
        available: 0,
        used: 0,
        total: 0,
      };
    }
  }

  /**
   * Get order history
   */
  async getOrders(
    broker: BrokerType = 'paper',
    status?: OrderStatus
  ): Promise<Order[]> {
    if (this.paperTradingEnabled) broker = 'paper';

    try {
      if (broker === 'paper') {
        // Return empty for paper trading (no persistent order history yet)
        return [];
      }

      const adapter = await this.getAdapter(broker);
      if (!adapter) {
        return [];
      }

      const result = await adapter.getOrders();
      const orders = this.normalizeOrders(result);

      if (status) {
        return orders.filter((order) => order.status === status);
      }

      return orders;
    } catch (error) {
      console.error('[TradingBridge] Error fetching orders:', error);
      return [];
    }
  }

  /**
   * Close a position
   */
  async closePosition(
    symbol: string,
    broker: BrokerType = 'paper',
    quantity?: number
  ): Promise<OrderResponse> {
    if (this.paperTradingEnabled) broker = 'paper';

    try {
      // Get current position
      const positions = await this.getPositions(broker);
      const position = positions.find((p) => p.symbol === symbol);

      if (!position) {
        return {
          success: false,
          message: `No open position found for ${symbol}`,
        };
      }

      // Place opposite order to close
      const closeQuantity = quantity || position.quantity;
      const closeSide: OrderSide = position.side === 'BUY' ? 'SELL' : 'BUY';

      return this.placeOrder(
        {
          symbol,
          exchange: position.exchange,
          side: closeSide,
          quantity: closeQuantity,
          type: 'MARKET',
          product: position.product,
        },
        broker
      );
    } catch (error: any) {
      return {
        success: false,
        message: error.message || 'Failed to close position',
      };
    }
  }

  /**
   * Get broker capabilities
   */
  async getBrokerCapabilities(broker: BrokerType): Promise<IDataObject> {
    const commonCapabilities = {
      placeOrder: true,
      modifyOrder: true,
      cancelOrder: true,
      getPositions: true,
      getHoldings: true,
      getBalance: true,
      getOrders: true,
    };

    const brokerSpecific: Record<BrokerType, IDataObject> = {
      paper: {
        ...commonCapabilities,
        realtime: false,
        margin: true,
        shortSelling: true,
        derivatives: true,
      },
      kite: {
        ...commonCapabilities,
        realtime: true,
        margin: true,
        shortSelling: true,
        derivatives: true,
        exchanges: ['NSE', 'BSE', 'MCX', 'NFO', 'CDS'],
      },
      fyers: {
        ...commonCapabilities,
        realtime: true,
        margin: true,
        shortSelling: true,
        derivatives: true,
        exchanges: ['NSE', 'BSE', 'MCX', 'NFO'],
      },
      alpaca: {
        ...commonCapabilities,
        realtime: true,
        margin: true,
        shortSelling: true,
        derivatives: false,
        fractionalShares: true,
      },
      binance: {
        ...commonCapabilities,
        realtime: true,
        margin: true,
        futures: true,
        spot: true,
      },
      kraken: {
        ...commonCapabilities,
        realtime: true,
        margin: true,
        futures: true,
        spot: true,
      },
      coinbase: {
        ...commonCapabilities,
        realtime: true,
        margin: false,
        spot: true,
      },
      hyperliquid: {
        ...commonCapabilities,
        realtime: true,
        margin: true,
        perpetuals: true,
      },
    };

    return brokerSpecific[broker] || commonCapabilities;
  }

  // ================================
  // PRIVATE METHODS
  // ================================

  /**
   * Execute paper trading order
   */
  private executePaperOrder(order: OrderRequest): OrderResponse {
    const orderId = `PAPER-${Date.now()}-${Math.random().toString(36).substr(2, 9)}`;
    const executionPrice = order.price || 100 + Math.random() * 100; // Mock price

    // Update positions
    const positionKey = `${order.symbol}-${order.exchange || 'DEFAULT'}`;
    const existingPosition = this.activePositions.get(positionKey);

    if (existingPosition) {
      // Update existing position
      if (order.side === existingPosition.side) {
        // Adding to position
        const totalQuantity = existingPosition.quantity + order.quantity;
        const newAvgPrice =
          (existingPosition.quantity * existingPosition.averagePrice +
            order.quantity * executionPrice) /
          totalQuantity;

        this.activePositions.set(positionKey, {
          ...existingPosition,
          quantity: totalQuantity,
          averagePrice: newAvgPrice,
        });
      } else {
        // Reducing or closing position
        const remainingQuantity = existingPosition.quantity - order.quantity;
        if (remainingQuantity <= 0) {
          this.activePositions.delete(positionKey);
        } else {
          this.activePositions.set(positionKey, {
            ...existingPosition,
            quantity: remainingQuantity,
          });
        }
      }
    } else {
      // New position
      this.activePositions.set(positionKey, {
        symbol: order.symbol,
        exchange: order.exchange,
        quantity: order.quantity,
        averagePrice: executionPrice,
        currentPrice: executionPrice,
        pnl: 0,
        pnlPercent: 0,
        side: order.side,
        product: order.product,
        timestamp: new Date().toISOString(),
      });
    }

    return {
      success: true,
      orderId,
      status: 'COMPLETE',
      message: 'Paper order executed successfully',
      filledQuantity: order.quantity,
      filledPrice: executionPrice,
      timestamp: new Date().toISOString(),
      fees: order.quantity * executionPrice * 0.001, // 0.1% mock fees
    };
  }

  /**
   * Get adapter instance
   */
  private async getAdapter(broker: BrokerType): Promise<any> {
    if (broker === 'paper') return null;

    if (this.adapterCache.has(broker)) {
      return this.adapterCache.get(broker);
    }

    try {
      const adapterPath = this.getAdapterPath(broker);
      const module = await import(/* @vite-ignore */ adapterPath);
      const AdapterClass = module.default || Object.values(module)[0];

      if (AdapterClass) {
        const adapter = new AdapterClass();
        this.adapterCache.set(broker, adapter);
        return adapter;
      }
    } catch (error) {
      console.warn(`[TradingBridge] Adapter for ${broker} not available:`, error);
    }

    return null;
  }

  /**
   * Get adapter import path
   */
  private getAdapterPath(broker: BrokerType): string {
    const paths: Record<BrokerType, string> = {
      paper: '',
      kite: '@/components/tabs/equity-trading/adapters/KiteStandardAdapter',
      fyers: '@/components/tabs/equity-trading/adapters/FyersStandardAdapter',
      alpaca: '@/components/tabs/equity-trading/adapters/AlpacaAdapter',
      binance: '@/brokers/crypto/exchanges/BinanceAdapter',
      kraken: '@/brokers/crypto/exchanges/KrakenAdapter',
      coinbase: '@/brokers/crypto/exchanges/CoinbaseAdapter',
      hyperliquid: '@/brokers/crypto/exchanges/HyperliquidAdapter',
    };
    return paths[broker];
  }

  /**
   * Format order for specific broker
   */
  private formatOrderForBroker(order: OrderRequest, broker: BrokerType): any {
    // Each broker has its own format - normalize here
    return {
      tradingsymbol: order.symbol,
      exchange: order.exchange || 'NSE',
      transaction_type: order.side,
      quantity: order.quantity,
      order_type: order.type,
      price: order.price,
      trigger_price: order.triggerPrice,
      product: order.product || 'CNC',
      validity: order.validity || 'DAY',
      disclosed_quantity: order.disclosedQuantity,
      tag: order.tag,
    };
  }

  /**
   * Calculate used margin for paper trading
   */
  private calculateUsedMargin(): number {
    let margin = 0;
    this.activePositions.forEach((position) => {
      margin += position.quantity * position.averagePrice;
    });
    return margin;
  }

  // ================================
  // NORMALIZATION METHODS
  // ================================

  private normalizeOrderResponse(result: any): OrderResponse {
    return {
      success: result.success || result.status === 'ok',
      orderId: result.orderId || result.order_id,
      status: result.status || 'PENDING',
      message: result.message,
      filledQuantity: result.filledQuantity || result.filled_quantity,
      filledPrice: result.filledPrice || result.average_price,
      timestamp: result.timestamp || new Date().toISOString(),
      fees: result.fees || result.charges,
    };
  }

  private normalizePositions(data: any[]): Position[] {
    if (!Array.isArray(data)) return [];

    return data.map((item) => ({
      symbol: item.symbol || item.tradingsymbol,
      exchange: item.exchange,
      quantity: item.quantity || item.netQuantity,
      averagePrice: item.averagePrice || item.average_price || item.avgPrice,
      currentPrice: item.currentPrice || item.last_price || item.ltp,
      pnl: item.pnl || item.unrealised || item.unrealized,
      pnlPercent: item.pnlPercent,
      side: item.quantity > 0 ? 'BUY' : 'SELL',
      product: item.product,
      timestamp: item.timestamp || new Date().toISOString(),
    }));
  }

  private normalizeHoldings(data: any[]): Holding[] {
    if (!Array.isArray(data)) return [];

    return data.map((item) => ({
      symbol: item.symbol || item.tradingsymbol,
      exchange: item.exchange,
      quantity: item.quantity,
      averagePrice: item.averagePrice || item.average_price,
      currentPrice: item.currentPrice || item.last_price,
      pnl: item.pnl,
      pnlPercent: item.pnlPercent,
      investedValue: item.investedValue || item.quantity * item.averagePrice,
      currentValue: item.currentValue || item.quantity * item.currentPrice,
      timestamp: item.timestamp || new Date().toISOString(),
    }));
  }

  private normalizeBalance(data: any, currency: string): AccountBalance {
    if (data.available !== undefined) {
      // Already normalized format
      return {
        currency,
        available: data.available,
        used: data.used || 0,
        total: data.total || data.available,
        margin: data.margin,
        marginUsed: data.marginUsed,
      };
    }

    // Handle various broker formats
    return {
      currency,
      available: data.cash || data.available_cash || data.free || 0,
      used: data.used || data.margin_used || data.used || 0,
      total: data.equity || data.total || data.balance || 0,
      margin: data.margin || data.available_margin,
      marginUsed: data.margin_used,
    };
  }

  private normalizeOrders(data: any[]): Order[] {
    if (!Array.isArray(data)) return [];

    return data.map((item) => ({
      orderId: item.orderId || item.order_id,
      symbol: item.symbol || item.tradingsymbol,
      exchange: item.exchange,
      side: item.side || item.transaction_type,
      type: item.type || item.order_type,
      quantity: item.quantity,
      price: item.price,
      triggerPrice: item.triggerPrice || item.trigger_price,
      filledQuantity: item.filledQuantity || item.filled_quantity,
      filledPrice: item.filledPrice || item.average_price,
      status: item.status,
      product: item.product,
      validity: item.validity,
      timestamp: item.timestamp || item.order_timestamp,
      tag: item.tag,
    }));
  }
}

// Export singleton
export const TradingBridge = new TradingBridgeClass();
export { TradingBridgeClass };
