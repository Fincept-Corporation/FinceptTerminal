/**
 * Stock Brokers Module
 *
 * Unified exports for all stock broker functionality
 */

// Types
export * from './types';

// Constants
export * from './constants';

// Base Adapter
export { BaseStockBrokerAdapter } from './BaseStockBrokerAdapter';

// Registry
export {
  STOCK_BROKER_REGISTRY,
  registerBrokerAdapter,
  createStockBrokerAdapter,
  getAllStockBrokers,
  getStockBrokersByRegion,
  getStockBrokerMetadata,
  isStockBrokerRegistered,
  getStockBrokerDisplayName,
  getStockBrokerDefaultSymbols,
  stockBrokerSupportsFeature,
  stockBrokerSupportsTradingFeature,
  getAvailableRegions,
  getStockBrokersWithFeature,
} from './registry';

// Initialization
export { initializeStockBrokers } from './init';

// Indian Brokers
export * from './india';

// YFinance Paper Trading
export * from './yfinance';
