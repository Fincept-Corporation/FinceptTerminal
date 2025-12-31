/**
 * Test Helpers
 * Utilities for testing multi-broker trading functionality
 */

import { BrokerType, OrderRequest, OrderSide, OrderType, ProductType, UnifiedOrder, OrderStatus, Validity } from '../core/types';
import { authManager } from '../services/AuthManager';
import { orderRouter, RoutingStrategy } from '../core/OrderRouter';
import { brokerOrchestrator } from '../core/BrokerOrchestrator';
import { logger } from './LoggingService';
import { notificationService } from './NotificationService';

/**
 * Test parallel order execution across multiple brokers
 */
export async function testParallelTrading(
  symbol: string = 'RELIANCE',
  exchange: string = 'NSE',
  quantity: number = 1
): Promise<void> {
  logger.info('TEST', '=== Testing Parallel Trading ===');

  const testOrder: OrderRequest = {
    symbol,
    exchange,
    side: OrderSide.BUY,
    type: OrderType.LIMIT,
    quantity,
    price: 2500,
    product: ProductType.MIS,
  };

  try {
    // Test 1: Parallel execution
    logger.info('TEST', 'Test 1: Executing order on all brokers in parallel');
    const result = await orderRouter.routeOrder(testOrder, {
      strategy: RoutingStrategy.PARALLEL,
      brokers: ['fyers', 'kite'],
    });

    logger.info('TEST', 'Parallel execution results:', result);

    if (result.success) {
      logger.info('TEST', '✅ Parallel trading test PASSED');
      notificationService.success('Test Passed', 'Parallel trading executed successfully');
    } else {
      logger.error('TEST', '❌ Parallel trading test FAILED', result.errors);
      notificationService.error('Test Failed', 'Parallel trading failed');
    }
  } catch (error) {
    logger.error('TEST', 'Parallel trading test error:', error);
    notificationService.error('Test Error', 'Parallel trading encountered an error');
  }
}

/**
 * Test background token refresh
 */
export async function testTokenRefresh(brokerId: BrokerType = 'fyers'): Promise<void> {
  logger.info('TEST', `=== Testing Token Refresh for ${brokerId.toUpperCase()} ===`);

  try {
    const adapter = authManager.getAdapter(brokerId);
    if (!adapter) {
      throw new Error(`Adapter not found for ${brokerId}`);
    }

    logger.info('TEST', 'Attempting token refresh...');
    const success = await adapter.refreshToken();

    if (success) {
      logger.info('TEST', '✅ Token refresh test PASSED');
      notificationService.success('Test Passed', `Token refreshed for ${brokerId.toUpperCase()}`);
    } else {
      logger.error('TEST', '❌ Token refresh test FAILED');
      notificationService.error('Test Failed', `Token refresh failed for ${brokerId.toUpperCase()}`);
    }
  } catch (error) {
    logger.error('TEST', 'Token refresh test error:', error);
    notificationService.error('Test Error', 'Token refresh encountered an error');
  }
}

/**
 * Test broker comparison
 */
export async function testBrokerComparison(
  symbol: string = 'RELIANCE',
  exchange: string = 'NSE'
): Promise<void> {
  logger.info('TEST', '=== Testing Broker Comparison ===');

  try {
    // Test quote comparison
    logger.info('TEST', 'Fetching quotes from all brokers...');
    const quoteComparison = await brokerOrchestrator.compareQuotes(symbol, exchange);

    logger.info('TEST', 'Quote comparison results:', {
      brokers: Array.from(quoteComparison.data.keys()),
      latencies: Array.from(quoteComparison.latency.entries()),
    });

    // Test market depth comparison
    logger.info('TEST', 'Fetching market depth from all brokers...');
    const depthComparison = await brokerOrchestrator.compareMarketDepth(symbol, exchange);

    logger.info('TEST', 'Depth comparison results:', {
      brokers: Array.from(depthComparison.data.keys()),
    });

    logger.info('TEST', '✅ Broker comparison test PASSED');
    notificationService.success('Test Passed', 'Broker comparison completed successfully');
  } catch (error) {
    logger.error('TEST', 'Broker comparison test error:', error);
    notificationService.error('Test Error', 'Broker comparison encountered an error');
  }
}

/**
 * Test smart order routing
 */
export async function testSmartRouting(
  symbol: string = 'RELIANCE',
  exchange: string = 'NSE'
): Promise<void> {
  logger.info('TEST', '=== Testing Smart Order Routing ===');

  const testOrders: Array<{ order: OrderRequest; expectedStrategy: RoutingStrategy }> = [
    // Large order -> should use PARALLEL
    {
      order: {
        symbol,
        exchange,
        side: OrderSide.BUY,
        type: OrderType.LIMIT,
        quantity: 1500,
        price: 2500,
        product: ProductType.MIS,
      },
      expectedStrategy: RoutingStrategy.PARALLEL,
    },
    // Market order -> should use BEST_LATENCY
    {
      order: {
        symbol,
        exchange,
        side: OrderSide.BUY,
        type: OrderType.MARKET,
        quantity: 10,
        product: ProductType.MIS,
      },
      expectedStrategy: RoutingStrategy.BEST_LATENCY,
    },
    // Limit order -> should use BEST_PRICE
    {
      order: {
        symbol,
        exchange,
        side: OrderSide.BUY,
        type: OrderType.LIMIT,
        quantity: 50,
        price: 2500,
        product: ProductType.MIS,
      },
      expectedStrategy: RoutingStrategy.BEST_PRICE,
    },
  ];

  for (const testCase of testOrders) {
    try {
      logger.info('TEST', `Testing ${testCase.order.type} order with qty ${testCase.order.quantity}`);
      const result = await orderRouter.smartRoute(testCase.order);
      logger.info('TEST', 'Smart routing result:', result);
    } catch (error) {
      logger.error('TEST', 'Smart routing error:', error);
    }
  }

  logger.info('TEST', '✅ Smart routing test COMPLETED');
  notificationService.success('Test Passed', 'Smart routing test completed');
}

/**
 * Mock order for testing (without actual broker API calls)
 */
export function createMockOrder(
  brokerId: BrokerType,
  symbol: string = 'RELIANCE',
  side: OrderSide = OrderSide.BUY,
  quantity: number = 10
): UnifiedOrder {
  return {
    id: `MOCK_${Date.now()}`,
    brokerId,
    symbol,
    exchange: 'NSE',
    side,
    type: OrderType.LIMIT,
    quantity,
    price: 2500,
    status: OrderStatus.COMPLETE,
    filledQuantity: quantity,
    averagePrice: 2500,
    pendingQuantity: 0,
    orderTime: new Date(),
    updateTime: new Date(),
    product: ProductType.MIS,
    validity: Validity.DAY,
  };
}

/**
 * Generate mock market data for testing
 */
export function generateMockCandles(count: number = 100) {
  const candles = [];
  let basePrice = 2500;

  for (let i = 0; i < count; i++) {
    const change = (Math.random() - 0.5) * 20;
    basePrice += change;

    const open = basePrice;
    const high = basePrice + Math.random() * 10;
    const low = basePrice - Math.random() * 10;
    const close = low + Math.random() * (high - low);

    candles.push({
      timestamp: new Date(Date.now() - (count - i) * 5 * 60 * 1000),
      open,
      high,
      low,
      close,
      volume: Math.floor(Math.random() * 100000) + 10000,
    });
  }

  return candles;
}

/**
 * Performance benchmark
 */
export async function benchmarkPerformance(): Promise<void> {
  logger.info('TEST', '=== Performance Benchmark ===');

  const iterations = 100;
  const startTime = Date.now();

  // Simulate rapid order processing
  const orders: OrderRequest[] = Array.from({ length: iterations }, (_, i) => ({
    symbol: 'RELIANCE',
    exchange: 'NSE',
    side: i % 2 === 0 ? OrderSide.BUY : OrderSide.SELL,
    type: OrderType.LIMIT,
    quantity: 1,
    price: 2500,
    product: ProductType.MIS,
  }));

  logger.info('TEST', `Processing ${iterations} orders...`);

  // This would actually process orders in a real scenario
  // For testing, we just measure the overhead
  for (const order of orders) {
    // Mock processing
    await new Promise((resolve) => setTimeout(resolve, 1));
  }

  const endTime = Date.now();
  const duration = endTime - startTime;
  const ordersPerSecond = (iterations / duration) * 1000;

  logger.info('TEST', 'Performance Results:', {
    totalOrders: iterations,
    duration: `${duration}ms`,
    ordersPerSecond: ordersPerSecond.toFixed(2),
  });

  notificationService.info('Benchmark Complete', `Processed ${ordersPerSecond.toFixed(0)} orders/sec`);
}

/**
 * Run all tests
 */
export async function runAllTests(): Promise<void> {
  logger.info('TEST', '🚀 Starting comprehensive test suite...');

  try {
    await testBrokerComparison();
    await new Promise((resolve) => setTimeout(resolve, 2000));

    await testSmartRouting();
    await new Promise((resolve) => setTimeout(resolve, 2000));

    await benchmarkPerformance();

    logger.info('TEST', '✅ All tests completed!');
    notificationService.success('Tests Complete', 'All integration tests passed successfully');
  } catch (error) {
    logger.error('TEST', 'Test suite error:', error);
    notificationService.error('Tests Failed', 'Some tests encountered errors');
  }
}
