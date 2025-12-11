/**
 * Trading Hooks Exports
 */

export { useExchangeCapabilities, useSupportsFeature, useAvailableOrderTypes, useHasAdvancedOrders, useHasMarginTrading } from './useExchangeCapabilities';
export { useOrderExecution } from './useOrderExecution';
export type { UseOrderExecutionResult } from './useOrderExecution';
export { usePositions, useClosePosition } from './usePositions';
export { useOrders, useCancelOrder, useClosedOrders } from './useOrders';
export { useOHLCV, useOrderBook, useTicker, useVolumeProfile } from './useMarketData';
export type { VolumeProfileLevel } from './useMarketData';
export { useBalance, useFees, useMarginInfo, useTradingStats } from './useAccountInfo';
export type { FeeStructure, MarginInfo } from './useAccountInfo';
export { useCrossExchangePortfolio, useArbitrageDetection, useCrossExchangePositions } from './useCrossExchange';
export type { CrossExchangeBalance, AggregatedBalance, ArbitrageOpportunity, CrossExchangePosition } from './useCrossExchange';
