/**
 * Grid Trading Module
 *
 * Unified grid trading system that works with both crypto and stock brokers.
 *
 * Usage:
 * ```typescript
 * import { getGridTradingService, CryptoGridAdapter, StockGridAdapter } from '@/services/gridTrading';
 *
 * // Get service instance
 * const service = getGridTradingService();
 *
 * // Register crypto adapter
 * const cryptoAdapter = new CryptoGridAdapter(krakenExchangeAdapter);
 * service.registerAdapter(cryptoAdapter);
 *
 * // Register stock adapter
 * const stockAdapter = new StockGridAdapter(zerodhaAdapter);
 * service.registerAdapter(stockAdapter);
 *
 * // Create and start a grid
 * const grid = await service.createGrid({
 *   id: 'my-grid',
 *   symbol: 'BTC/USD',
 *   upperPrice: 50000,
 *   lowerPrice: 40000,
 *   gridLevels: 10,
 *   gridType: 'arithmetic',
 *   totalInvestment: 1000,
 *   quantityPerGrid: 0.01,
 *   direction: 'neutral',
 *   brokerType: 'crypto',
 *   brokerId: 'kraken',
 * });
 *
 * await service.startGrid(grid.config.id);
 * ```
 */

// Types
export type {
  GridType,
  GridDirection,
  GridStatus,
  GridConfig,
  GridLevel,
  GridLevelStatus,
  GridState,
  IGridTradingService,
  IGridBrokerAdapter,
  GridOrderParams,
  GridOrderResult,
  GridOrderStatus,
  GridCalculation,
  GridEvent,
  GridPersistence,
  BrokerType,
} from './types';

// Engine functions
export {
  calculateGridLevels,
  calculateGridDetails,
  initializeGridLevels,
  createInitialGridState,
  updateGridOnPriceChange,
  updateGridLevelStatus,
  updateGridOnFill,
  getPendingLevels,
  getOpenLevels,
  checkGridStopConditions,
  validateGridConfig,
  generateGridId,
  calculateProfitPerGrid,
  estimateAnnualReturn,
} from './GridEngine';

// Service
export {
  GridTradingService,
  getGridTradingService,
  resetGridTradingService,
} from './GridTradingService';

// Adapters
export { CryptoGridAdapter } from './adapters/CryptoGridAdapter';
export { StockGridAdapter } from './adapters/StockGridAdapter';
