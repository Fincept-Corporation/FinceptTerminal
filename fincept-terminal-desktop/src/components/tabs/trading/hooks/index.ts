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
export { useBalance, useFees, useMarginInfo, useLeverage, useTradingStats } from './useAccountInfo';
export type { FeeStructure, MarginInfo, LeverageInfo } from './useAccountInfo';
export { useCrossExchangePortfolio, useArbitrageDetection, useCrossExchangePositions } from './useCrossExchange';
export type { CrossExchangeBalance, AggregatedBalance, ArbitrageOpportunity, CrossExchangePosition } from './useCrossExchange';
export { useTrades, useOrderTrades } from './useTrades';
export type { UnifiedTrade } from './useTrades';
export { useExchangeStatus, useExchangeTime } from './useExchangeStatus';
export type { ExchangeStatus } from './useExchangeStatus';
export { useDeposits, useWithdrawals, useDepositAddress, useWithdraw, useTransactions } from './useFunding';
export type { UnifiedTransaction, DepositAddress, WithdrawParams } from './useFunding';

// Derivatives (Funding Rates, Mark Prices, Open Interest, Liquidations, Borrow Rates)
export {
  useFundingRate,
  useFundingRates,
  useFundingRateHistory,
  useMarkPrice,
  useMarkPrices,
  useOpenInterest,
  useOpenInterestHistory,
  useLiquidations,
  useMyLiquidations,
  useBorrowRate,
  useBorrowRates,
  useBorrowInterest,
} from './useDerivatives';
export type {
  FundingRateInfo,
  MarkPriceInfo,
  OpenInterestInfo,
  LiquidationInfo,
  BorrowRateInfo,
} from './useDerivatives';

// Options Trading
export {
  useGreeks,
  useOption,
  useOptionChain,
  useVolatilityHistory,
} from './useOptions';
export type {
  GreeksInfo,
  OptionContract,
  OptionChain,
  VolatilityData,
} from './useOptions';

// Conversions & Transfers
export {
  useConvertCurrencies,
  useConvertQuote,
  useCreateConvertTrade,
  useConvertTradeHistory,
  useTransfer,
  useAvailableAccounts,
} from './useConversions';
export type {
  ConvertQuote,
  ConvertTrade,
  TransferResult,
} from './useConversions';

// Leverage & Margin Management
export {
  useFetchLeverage,
  useSetLeverage,
  useLeverageTiers,
  useSetMarginMode,
  usePositionMode,
  useSetPositionMode,
  useCloseAllPositions,
} from './useLeverageManagement';
export type {
  LeverageSettings,
  LeverageTier,
  MarginModeInfo,
} from './useLeverageManagement';

// Advanced Market Data (OHLCV Variants, Sentiment, Settlements, Position History)
export {
  useMarkOHLCV,
  useIndexOHLCV,
  usePremiumIndexOHLCV,
  useLongShortRatioHistory,
  useSettlementHistory,
  useMySettlementHistory,
  usePositionHistory,
} from './useAdvancedMarketData';
export type {
  OHLCVCandle,
  LongShortRatio,
  SettlementInfo,
  MySettlementInfo,
  PositionHistoryItem,
} from './useAdvancedMarketData';

// Advanced Orders (Batch Orders, Market Orders with Cost, Cancel All)
export {
  useCreateOrders,
  useCreateMarketOrderWithCost,
  useCancelOrders,
  useCancelAllOrders,
  useCancelAllOrdersAfter,
  useEditOrder,
} from './useAdvancedOrders';
export type {
  OrderParams,
  BatchOrderResult,
  CancelOrderResult,
} from './useAdvancedOrders';

// Ledger & Transfer History
export {
  useLedger,
  useTransferHistory,
  useDepositWithdrawFees,
  useBorrowRateHistory,
  useFundingHistory,
  useIncomeHistory,
} from './useLedger';
export type {
  LedgerEntry,
  TransferEntry,
  DepositWithdrawFee,
  BorrowRateHistoryEntry,
  FundingHistoryEntry,
  IncomeEntry,
} from './useLedger';
