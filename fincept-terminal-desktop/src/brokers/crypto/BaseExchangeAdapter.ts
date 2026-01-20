/**
 * Base Exchange Adapter
 *
 * Re-exports from modular adapter chain for backwards compatibility
 *
 * The adapter functionality has been split into modular files for better organization:
 * - CoreAdapter.ts: Connection, authentication, market data, account/balance
 * - TradingAdapter.ts: Order creation, modification, cancellation, queries
 * - DerivativesAdapter.ts: Options, margin, leverage, positions, mark prices, liquidations
 * - FundingAdapter.ts: Deposits, withdrawals, transfers
 * - AdvancedAdapter.ts: Crypto convert, sentiment analysis
 *
 * Inheritance Chain:
 * CoreExchangeAdapter → TradingAdapter → DerivativesAdapter → FundingAdapter → AdvancedAdapter
 */

import { AdvancedAdapter } from './base/AdvancedAdapter';

/**
 * BaseExchangeAdapter - Complete adapter with all functionality
 * Extends from AdvancedAdapter which contains the full inheritance chain
 */
export abstract class BaseExchangeAdapter extends AdvancedAdapter {}

// Re-export individual adapters for selective use
export {
  CoreExchangeAdapter,
  TradingAdapter,
  DerivativesAdapter,
  FundingAdapter,
  AdvancedAdapter,
} from './base';
