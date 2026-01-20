/**
 * Base Adapter Modules
 *
 * Modular adapter hierarchy for crypto exchange integration
 *
 * Inheritance Chain:
 * CoreExchangeAdapter (connection, auth, market data)
 *   └── TradingAdapter (order management)
 *       └── DerivativesAdapter (futures, options, leverage, positions)
 *           └── FundingAdapter (deposits, withdrawals, transfers)
 *               └── AdvancedAdapter (convert, sentiment) - Final in chain
 */

export { CoreExchangeAdapter } from './CoreAdapter';
export { TradingAdapter } from './TradingAdapter';
export { DerivativesAdapter } from './DerivativesAdapter';
export { FundingAdapter } from './FundingAdapter';
export { AdvancedAdapter } from './AdvancedAdapter';
