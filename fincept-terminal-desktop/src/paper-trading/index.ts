/**
 * Universal Paper Trading Module
 *
 * All business logic is handled by the Rust backend for performance.
 * This module provides TypeScript types and a thin wrapper for Tauri commands.
 */

// Export types for TypeScript
export * from './types';

// Export database wrapper (thin Tauri command wrapper)
export { PaperTradingDatabase, paperTradingDatabase } from './PaperTradingDatabase';

// Export adapter (thin wrapper using Rust backend)
export { PaperTradingAdapter, createPaperTradingAdapter } from './PaperTradingAdapter';

// Re-export from sqliteService for convenience
export {
  // Types
  type PaperTradingPortfolio,
  type PaperTradingPosition,
  type PaperTradingOrder,
  type PaperTradingTrade,
  type MarginBlock,
  type PaperTradingHolding,
  type PortfolioStats,
  // Portfolio operations
  createPortfolio,
  getPortfolio,
  listPortfolios,
  updatePortfolioBalance,
  deletePortfolio,
  // Position operations
  createPosition,
  getPosition,
  getPositionBySymbol,
  getPositionBySymbolAndSide,
  getPortfolioPositions,
  updatePosition,
  deletePosition,
  // Order operations
  createOrder,
  getOrder,
  getPendingOrders,
  getPortfolioOrders,
  updateOrder,
  deleteOrder,
  // Trade operations
  createTrade,
  getTrade,
  getOrderTrades,
  getPortfolioTrades,
  deleteTrade,
  // Margin block operations
  createMarginBlock,
  getMarginBlocks,
  getMarginBlockByOrder,
  deleteMarginBlock,
  getTotalBlockedMargin,
  getAvailableMargin,
  // Holdings operations (T+1 settlement)
  createHolding,
  getHoldings,
  getHoldingBySymbol,
  updateHolding,
  deleteHolding,
  processT1Settlement,
  // Execution engine operations
  fillOrder,
  getPortfolioStats,
  resetPortfolio,
} from '../services/sqliteService';
