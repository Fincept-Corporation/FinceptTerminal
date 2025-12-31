/**
 * Order Router
 * Advanced order routing with smart execution strategies
 * Supports parallel, sequential, and intelligent routing
 */

import {
  BrokerType,
  OrderRequest,
  OrderResponse,
  OrderSide,
  MultiBrokerOrder,
  MultiBrokerOrderResult,
  BrokerComparison,
  OrderHook,
  OrderPluginContext,
  UnifiedOrder,
} from './types';
import { brokerOrchestrator } from './BrokerOrchestrator';
import { pluginManager, PluginType } from './PluginSystem';

export enum RoutingStrategy {
  PARALLEL = 'PARALLEL',           // Execute on all brokers simultaneously
  BEST_PRICE = 'BEST_PRICE',       // Route to broker with best price
  BEST_LATENCY = 'BEST_LATENCY',   // Route to fastest broker
  ROUND_ROBIN = 'ROUND_ROBIN',     // Distribute across brokers
  CUSTOM = 'CUSTOM',                // Custom routing logic
}

interface RoutingConfig {
  strategy: RoutingStrategy;
  brokers?: BrokerType[];
  fallbackBroker?: BrokerType;
  customLogic?: (comparison: BrokerComparison) => BrokerType | null;
}

export interface OrderResult {
  success: boolean;
  brokerId?: BrokerType;
  orderId?: string;
  message: string;
  results?: Record<BrokerType, OrderResponse>;
  errors?: Record<BrokerType, string>;
}

export class OrderRouter {
  private static instance: OrderRouter;
  private roundRobinIndex = 0;
  private hooks: Map<string, OrderHook[]> = new Map();

  private constructor() {
    // Singleton pattern
  }

  static getInstance(): OrderRouter {
    if (!OrderRouter.instance) {
      OrderRouter.instance = new OrderRouter();
    }
    return OrderRouter.instance;
  }

  // ==================== ORDER ROUTING ====================

  /**
   * Route order based on strategy
   */
  async routeOrder(
    order: OrderRequest,
    config: RoutingConfig
  ): Promise<OrderResult> {
    console.log(`[OrderRouter] Routing order with strategy: ${config.strategy}`);

    try {
      // Execute plugin PRE_ORDER hooks (for paper trading, etc.)
      let cancelled = false;
      const cancelFn = () => { cancelled = true; };

      const pluginResults = await pluginManager.executeHook(PluginType.PRE_ORDER, {
        type: PluginType.PRE_ORDER,
        data: order,
        cancel: cancelFn,
      });

      // Check if any plugin cancelled the order (e.g., paper trading intercepted it)
      if (cancelled) {
        console.log('[OrderRouter] Order cancelled by plugin (paper trading)');
        const paperResult = pluginResults.find(r => r.success && r.data);
        if (paperResult) {
          return {
            success: true,
            message: paperResult.data.message || 'Order handled by plugin',
            brokerId: 'paper' as BrokerType,
            orderId: paperResult.data.orderId,
          };
        }
        return {
          success: false,
          message: 'Order cancelled by plugin',
        };
      }

      // Execute legacy pre-order hooks
      await this.executeHooks('pre_order', order);

      let result: OrderResult;

      switch (config.strategy) {
        case RoutingStrategy.PARALLEL:
          result = await this.executeParallel(order, config.brokers);
          break;

        case RoutingStrategy.BEST_PRICE:
          result = await this.executeBestPrice(order);
          break;

        case RoutingStrategy.BEST_LATENCY:
          result = await this.executeBestLatency(order);
          break;

        case RoutingStrategy.ROUND_ROBIN:
          result = await this.executeRoundRobin(order, config.brokers);
          break;

        case RoutingStrategy.CUSTOM:
          if (!config.customLogic) {
            throw new Error('Custom logic required for CUSTOM strategy');
          }
          result = await this.executeCustom(order, config.customLogic);
          break;

        default:
          throw new Error(`Unknown routing strategy: ${config.strategy}`);
      }

      // Execute post-order hooks
      await this.executeHooks('post_order', result);

      return result;
    } catch (error: any) {
      console.error('[OrderRouter] Routing failed:', error);
      return {
        success: false,
        message: error.message || 'Order routing failed',
      };
    }
  }

  /**
   * Execute order on all specified brokers in parallel
   */
  private async executeParallel(
    order: OrderRequest,
    brokers?: BrokerType[]
  ): Promise<OrderResult> {
    const targetBrokers = brokers || brokerOrchestrator.getActiveBrokers();

    if (targetBrokers.length === 0) {
      return {
        success: false,
        message: 'No active brokers available',
      };
    }

    const multiBrokerOrder: MultiBrokerOrder = {
      ...order,
      brokers: targetBrokers,
    };

    const result = await brokerOrchestrator.placeMultiBrokerOrder(multiBrokerOrder);

    return {
      success: result.success,
      message: result.success ? 'Order placed on multiple brokers' : 'Some orders failed',
      results: result.results,
    };
  }

  /**
   * Route to broker with best price
   */
  private async executeBestPrice(order: OrderRequest): Promise<OrderResult> {
    const comparison = await brokerOrchestrator.compareQuotes(order.symbol, order.exchange);

    const bestBroker = brokerOrchestrator.getBestBrokerByPrice(comparison, order.side);

    if (!bestBroker) {
      return {
        success: false,
        message: 'No broker available with valid quotes',
      };
    }

    return await this.executeSingle(order, bestBroker);
  }

  /**
   * Route to broker with lowest latency
   */
  private async executeBestLatency(order: OrderRequest): Promise<OrderResult> {
    const comparison = await brokerOrchestrator.compareQuotes(order.symbol, order.exchange);

    const bestBroker = brokerOrchestrator.getBestBrokerByLatency(comparison);

    if (!bestBroker) {
      return {
        success: false,
        message: 'No broker available',
      };
    }

    return await this.executeSingle(order, bestBroker);
  }

  /**
   * Distribute orders across brokers in round-robin fashion
   */
  private async executeRoundRobin(
    order: OrderRequest,
    brokers?: BrokerType[]
  ): Promise<OrderResult> {
    const targetBrokers = brokers || brokerOrchestrator.getActiveBrokers();

    if (targetBrokers.length === 0) {
      return {
        success: false,
        message: 'No active brokers available',
      };
    }

    const selectedBroker = targetBrokers[this.roundRobinIndex % targetBrokers.length];
    this.roundRobinIndex++;

    return await this.executeSingle(order, selectedBroker);
  }

  /**
   * Execute with custom routing logic
   */
  private async executeCustom(
    order: OrderRequest,
    customLogic: (comparison: BrokerComparison) => BrokerType | null
  ): Promise<OrderResult> {
    const comparison = await brokerOrchestrator.compareQuotes(order.symbol, order.exchange);

    const selectedBroker = customLogic(comparison);

    if (!selectedBroker) {
      return {
        success: false,
        message: 'Custom logic returned no broker',
      };
    }

    return await this.executeSingle(order, selectedBroker);
  }

  /**
   * Execute order on a single broker
   */
  private async executeSingle(
    order: OrderRequest,
    brokerId: BrokerType
  ): Promise<OrderResult> {
    const adapter = brokerOrchestrator['getAdapter'](brokerId);

    if (!adapter) {
      return {
        success: false,
        message: `Broker ${brokerId} not available`,
      };
    }

    try {
      const response = await adapter.placeOrder(order);

      return {
        success: response.success,
        brokerId,
        orderId: response.orderId,
        message: response.message,
      };
    } catch (error: any) {
      console.error(`[OrderRouter] Order failed on ${brokerId}:`, error);
      return {
        success: false,
        brokerId,
        message: error.message || 'Order execution failed',
      };
    }
  }

  // ==================== BATCH ORDERS ====================

  /**
   * Execute multiple orders with specified routing
   */
  async routeBatch(
    orders: OrderRequest[],
    config: RoutingConfig
  ): Promise<OrderResult[]> {
    console.log(`[OrderRouter] Routing ${orders.length} orders`);

    const promises = orders.map(order => this.routeOrder(order, config));
    const results = await Promise.allSettled(promises);

    return results.map(result =>
      result.status === 'fulfilled'
        ? result.value
        : { success: false, message: 'Promise rejected' }
    );
  }

  // ==================== ORDER MODIFICATION ====================

  /**
   * Modify order on specific broker
   */
  async modifyOrder(
    brokerId: BrokerType,
    orderId: string,
    updates: Partial<OrderRequest>
  ): Promise<OrderResponse> {
    const adapter = brokerOrchestrator['getAdapter'](brokerId);

    if (!adapter) {
      return {
        success: false,
        message: `Broker ${brokerId} not available`,
      };
    }

    return await adapter.modifyOrder(orderId, updates);
  }

  /**
   * Cancel order on specific broker
   */
  async cancelOrder(brokerId: BrokerType, orderId: string): Promise<OrderResponse> {
    return await brokerOrchestrator.cancelOrder(brokerId, orderId);
  }

  // ==================== PLUGIN HOOKS ====================

  /**
   * Register a hook for order lifecycle events
   */
  registerHook(event: 'pre_order' | 'post_order' | 'order_update', hook: OrderHook): void {
    if (!this.hooks.has(event)) {
      this.hooks.set(event, []);
    }
    this.hooks.get(event)!.push(hook);
    console.log(`[OrderRouter] Registered hook: ${hook.name} for event: ${event}`);
  }

  /**
   * Unregister a hook
   */
  unregisterHook(event: 'pre_order' | 'post_order' | 'order_update', hookName: string): void {
    const hooks = this.hooks.get(event);
    if (hooks) {
      const index = hooks.findIndex(h => h.name === hookName);
      if (index !== -1) {
        hooks.splice(index, 1);
        console.log(`[OrderRouter] Unregistered hook: ${hookName}`);
      }
    }
  }

  /**
   * Execute hooks for an event
   */
  private async executeHooks(event: string, data: any): Promise<void> {
    const hooks = this.hooks.get(event);
    if (!hooks || hooks.length === 0) return;

    for (const hook of hooks) {
      try {
        await hook.execute(data);
      } catch (error) {
        console.error(`[OrderRouter] Hook ${hook.name} failed:`, error);
      }
    }
  }

  // ==================== SMART ROUTING ====================

  /**
   * Automatically select best routing strategy based on order characteristics
   */
  async smartRoute(order: OrderRequest): Promise<OrderResult> {
    // Large orders: use parallel execution for better fill rates
    if (order.quantity > 1000) {
      return await this.routeOrder(order, {
        strategy: RoutingStrategy.PARALLEL,
      });
    }

    // Market orders: prioritize latency
    if (order.type === 'MARKET') {
      return await this.routeOrder(order, {
        strategy: RoutingStrategy.BEST_LATENCY,
      });
    }

    // Limit orders: prioritize price
    return await this.routeOrder(order, {
      strategy: RoutingStrategy.BEST_PRICE,
    });
  }
}

// Export singleton instance
export const orderRouter = OrderRouter.getInstance();
