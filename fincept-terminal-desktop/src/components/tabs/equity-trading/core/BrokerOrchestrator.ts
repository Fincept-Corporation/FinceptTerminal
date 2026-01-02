/**
 * Broker Orchestrator
 * Central coordinator for multi-broker operations
 * Manages parallel execution, data aggregation, and broker selection
 */

import {
  BrokerType,
  UnifiedOrder,
  OrderRequest,
  OrderResponse,
  UnifiedPosition,
  UnifiedHolding,
  UnifiedQuote,
  UnifiedMarketDepth,
  UnifiedFunds,
  MultiBrokerOrder,
  MultiBrokerOrderResult,
  BrokerComparison,
  AuthStatus,
} from './types';
import { BaseBrokerAdapter } from '../adapters/BaseBrokerAdapter';
import { authManager } from '../services/AuthManager';

export class BrokerOrchestrator {
  private static instance: BrokerOrchestrator;
  private activeBrokers: Set<BrokerType> = new Set();

  private constructor() {
    // Singleton pattern
  }

  static getInstance(): BrokerOrchestrator {
    if (!BrokerOrchestrator.instance) {
      BrokerOrchestrator.instance = new BrokerOrchestrator();
    }
    return BrokerOrchestrator.instance;
  }

  // ==================== BROKER MANAGEMENT ====================

  /**
   * Get all active (authenticated) brokers
   */
  getActiveBrokers(): BrokerType[] {
    return Array.from(this.activeBrokers);
  }

  /**
   * Check if broker is active
   */
  isBrokerActive(brokerId: BrokerType): boolean {
    return this.activeBrokers.has(brokerId) && authManager.isAuthenticated(brokerId);
  }

  /**
   * Enable a broker
   */
  enableBroker(brokerId: BrokerType): void {
    if (authManager.isAuthenticated(brokerId)) {
      this.activeBrokers.add(brokerId);
      console.log(`[Orchestrator] Enabled broker: ${brokerId}`);
    } else {
      console.warn(`[Orchestrator] Cannot enable ${brokerId} - not authenticated`);
    }
  }

  /**
   * Disable a broker
   */
  disableBroker(brokerId: BrokerType): void {
    this.activeBrokers.delete(brokerId);
    console.log(`[Orchestrator] Disabled broker: ${brokerId}`);
  }

  /**
   * Get adapter for a broker
   */
  private getAdapter(brokerId: BrokerType): BaseBrokerAdapter | null {
    const adapter = authManager.getAdapter(brokerId);
    if (!adapter) {
      console.error(`[Orchestrator] No adapter found for ${brokerId}`);
      return null;
    }
    if (!this.isBrokerActive(brokerId)) {
      console.warn(`[Orchestrator] Broker ${brokerId} is not active`);
      return null;
    }
    return adapter;
  }

  /**
   * Get brokers that support a specific exchange
   */
  getBrokersForExchange(exchange: string): BrokerType[] {
    const supportedBrokers: BrokerType[] = [];

    for (const brokerId of this.activeBrokers) {
      const adapter = authManager.getAdapter(brokerId);
      if (adapter && adapter.supportsExchange(exchange)) {
        supportedBrokers.push(brokerId);
      }
    }

    console.log(`[Orchestrator] Brokers supporting ${exchange}:`, supportedBrokers);
    return supportedBrokers;
  }

  // ==================== PARALLEL ORDER EXECUTION ====================

  /**
   * Place order on multiple brokers simultaneously
   */
  async placeMultiBrokerOrder(order: MultiBrokerOrder): Promise<MultiBrokerOrderResult> {
    const { brokers, ...orderRequest } = order;
    const results: Record<BrokerType, OrderResponse> = {} as any;

    // Execute orders in parallel
    const promises = brokers.map(async (brokerId) => {
      const adapter = this.getAdapter(brokerId);
      if (!adapter) {
        return {
          brokerId,
          result: {
            success: false,
            message: `Broker ${brokerId} not available`,
          },
        };
      }

      try {
        const result = await adapter.placeOrder(orderRequest);
        return { brokerId, result };
      } catch (error: any) {
        return {
          brokerId,
          result: {
            success: false,
            message: error.message || 'Order failed',
          },
        };
      }
    });

    const responses = await Promise.allSettled(promises);

    // Aggregate results
    responses.forEach((response) => {
      if (response.status === 'fulfilled') {
        results[response.value.brokerId] = response.value.result;
      } else {
        // Handle rejected promise (shouldn't happen with try-catch above)
        console.error('[Orchestrator] Promise rejected:', response.reason);
      }
    });

    const success = Object.values(results).some((r) => r.success);

    return { success, results };
  }

  /**
   * Cancel order on specific broker
   */
  async cancelOrder(brokerId: BrokerType, orderId: string): Promise<OrderResponse> {
    const adapter = this.getAdapter(brokerId);
    if (!adapter) {
      return {
        success: false,
        message: `Broker ${brokerId} not available`,
      };
    }

    return await adapter.cancelOrder(orderId);
  }

  // ==================== DATA AGGREGATION ====================

  /**
   * Get orders from all active brokers
   */
  async getAllOrders(): Promise<Map<BrokerType, UnifiedOrder[]>> {
    const ordersMap = new Map<BrokerType, UnifiedOrder[]>();

    const promises = this.getActiveBrokers().map(async (brokerId) => {
      const adapter = this.getAdapter(brokerId);
      if (!adapter) return { brokerId, orders: [] };

      try {
        const orders = await adapter.getOrders();
        return { brokerId, orders };
      } catch (error) {
        console.error(`[Orchestrator] Failed to get orders from ${brokerId}:`, error);
        return { brokerId, orders: [] };
      }
    });

    const results = await Promise.allSettled(promises);

    results.forEach((result) => {
      if (result.status === 'fulfilled') {
        ordersMap.set(result.value.brokerId, result.value.orders);
      }
    });

    return ordersMap;
  }

  /**
   * Get positions from all active brokers
   */
  async getAllPositions(): Promise<Map<BrokerType, UnifiedPosition[]>> {
    const positionsMap = new Map<BrokerType, UnifiedPosition[]>();

    const promises = this.getActiveBrokers().map(async (brokerId) => {
      const adapter = this.getAdapter(brokerId);
      if (!adapter) return { brokerId, positions: [] };

      try {
        const positions = await adapter.getPositions();
        return { brokerId, positions };
      } catch (error) {
        console.error(`[Orchestrator] Failed to get positions from ${brokerId}:`, error);
        return { brokerId, positions: [] };
      }
    });

    const results = await Promise.allSettled(promises);

    results.forEach((result) => {
      if (result.status === 'fulfilled') {
        positionsMap.set(result.value.brokerId, result.value.positions);
      }
    });

    return positionsMap;
  }

  /**
   * Get holdings from all active brokers
   */
  async getAllHoldings(): Promise<Map<BrokerType, UnifiedHolding[]>> {
    const holdingsMap = new Map<BrokerType, UnifiedHolding[]>();

    const promises = this.getActiveBrokers().map(async (brokerId) => {
      const adapter = this.getAdapter(brokerId);
      if (!adapter) return { brokerId, holdings: [] };

      try {
        const holdings = await adapter.getHoldings();
        return { brokerId, holdings };
      } catch (error) {
        console.error(`[Orchestrator] Failed to get holdings from ${brokerId}:`, error);
        return { brokerId, holdings: [] };
      }
    });

    const results = await Promise.allSettled(promises);

    results.forEach((result) => {
      if (result.status === 'fulfilled') {
        holdingsMap.set(result.value.brokerId, result.value.holdings);
      }
    });

    return holdingsMap;
  }

  /**
   * Get funds from all active brokers
   */
  async getAllFunds(): Promise<Map<BrokerType, UnifiedFunds>> {
    const fundsMap = new Map<BrokerType, UnifiedFunds>();

    const promises = this.getActiveBrokers().map(async (brokerId) => {
      const adapter = this.getAdapter(brokerId);
      if (!adapter) return { brokerId, funds: null };

      try {
        const funds = await adapter.getFunds();
        return { brokerId, funds };
      } catch (error) {
        console.error(`[Orchestrator] Failed to get funds from ${brokerId}:`, error);
        return { brokerId, funds: null };
      }
    });

    const results = await Promise.allSettled(promises);

    results.forEach((result) => {
      if (result.status === 'fulfilled' && result.value.funds) {
        fundsMap.set(result.value.brokerId, result.value.funds);
      }
    });

    return fundsMap;
  }

  // ==================== MARKET DATA COMPARISON ====================

  /**
   * Compare quotes from multiple brokers
   */
  async compareQuotes(symbol: string, exchange: string): Promise<BrokerComparison> {
    const dataMap = new Map();
    const latencyMap = new Map();
    const comparison: BrokerComparison = {
      symbol,
      data: dataMap,
      latency: latencyMap,
      brokers: {
        fyers: { latency: -1 },
        kite: { latency: -1 },
        alpaca: { latency: -1 },
      },
    };

    // CRITICAL: Only compare quotes from brokers that support this exchange
    const supportedBrokers = this.getBrokersForExchange(exchange);

    const promises = supportedBrokers.map(async (brokerId) => {
      const adapter = this.getAdapter(brokerId);
      if (!adapter) return { brokerId, quote: null, latency: -1 };

      const start = Date.now();
      try {
        const quote = await adapter.getQuote(symbol, exchange);
        const latency = Date.now() - start;
        return { brokerId, quote, latency };
      } catch (error) {
        console.error(`[Orchestrator] Failed to get quote from ${brokerId}:`, error);
        return { brokerId, quote: null, latency: -1 };
      }
    });

    const results = await Promise.allSettled(promises);

    results.forEach((result) => {
      if (result.status === 'fulfilled') {
        const { brokerId, quote, latency } = result.value;
        if (quote) {
          dataMap.set(brokerId, quote);
        }
        latencyMap.set(brokerId, latency);
        comparison.brokers[brokerId] = {
          quote: quote || undefined,
          latency,
        };
      }
    });

    return comparison;
  }

  /**
   * Compare market depth from multiple brokers
   */
  async compareMarketDepth(symbol: string, exchange: string): Promise<BrokerComparison> {
    const dataMap = new Map();
    const latencyMap = new Map();
    const comparison: BrokerComparison = {
      symbol,
      data: dataMap,
      latency: latencyMap,
      brokers: {
        fyers: { latency: -1 },
        kite: { latency: -1 },
        alpaca: { latency: -1 },
      },
    };

    // CRITICAL: Only compare depth from brokers that support this exchange
    const supportedBrokers = this.getBrokersForExchange(exchange);

    const promises = supportedBrokers.map(async (brokerId) => {
      const adapter = this.getAdapter(brokerId);
      if (!adapter) return { brokerId, depth: null, latency: -1 };

      const start = Date.now();
      try {
        const depth = await adapter.getMarketDepth(symbol, exchange);
        const latency = Date.now() - start;
        return { brokerId, depth, latency };
      } catch (error) {
        console.error(`[Orchestrator] Failed to get market depth from ${brokerId}:`, error);
        return { brokerId, depth: null, latency: -1 };
      }
    });

    const results = await Promise.allSettled(promises);

    results.forEach((result) => {
      if (result.status === 'fulfilled') {
        const { brokerId, depth, latency } = result.value;
        if (depth) {
          dataMap.set(brokerId, depth);
        }
        latencyMap.set(brokerId, latency);
        comparison.brokers[brokerId] = {
          depth: depth || undefined,
          latency,
        };
      }
    });

    return comparison;
  }

  // ==================== BROKER SELECTION ====================

  /**
   * Get best broker based on latency
   */
  getBestBrokerByLatency(comparison: BrokerComparison): BrokerType | null {
    let bestBroker: BrokerType | null = null;
    let lowestLatency = Infinity;

    for (const [brokerId, data] of Object.entries(comparison.brokers)) {
      if (data.latency > 0 && data.latency < lowestLatency) {
        lowestLatency = data.latency;
        bestBroker = brokerId as BrokerType;
      }
    }

    return bestBroker;
  }

  /**
   * Get broker with best price for an order
   */
  getBestBrokerByPrice(comparison: BrokerComparison, side: 'BUY' | 'SELL'): BrokerType | null {
    let bestBroker: BrokerType | null = null;
    let bestPrice = side === 'BUY' ? Infinity : 0;

    for (const [brokerId, data] of Object.entries(comparison.brokers)) {
      if (!data.quote) continue;

      const price = side === 'BUY' ? data.quote.ask : data.quote.bid;
      if (!price) continue;

      if (side === 'BUY' && price < bestPrice) {
        bestPrice = price;
        bestBroker = brokerId as BrokerType;
      } else if (side === 'SELL' && price > bestPrice) {
        bestPrice = price;
        bestBroker = brokerId as BrokerType;
      }
    }

    return bestBroker;
  }
}

// Export singleton instance
export const brokerOrchestrator = BrokerOrchestrator.getInstance();
