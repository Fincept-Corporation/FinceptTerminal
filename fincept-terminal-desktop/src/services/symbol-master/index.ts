/**
 * Symbol Master Service - Exports
 * Unified symbol management across all brokers and data sources
 */

// Main service
export { SymbolMasterService, symbolMasterService } from './SymbolMasterService';

// Sub-services
export { SymbolSearchService, symbolSearchService, createMockSymbol } from './SymbolSearchService';
export { SymbolTransformer, symbolTransformer, normalizeSymbol, isDerivative, extractUnderlying, parseDerivative } from './SymbolTransformer';
export { SymbolCacheService, symbolCacheService } from './SymbolCacheService';

// Types
export type {
  UnifiedSymbol,
  DerivativeSymbol,
  BrokerSymbolMapping,
  SymbolSearchRequest,
  SymbolSearchResult,
  SymbolTransformRequest,
  SymbolTransformResult,
  BatchSymbolRequest,
  BatchSymbolResult,
  SymbolValidationRequest,
  SymbolValidationResult,
  ExchangeDetectionRequest,
  ExchangeDetectionResult,
  SymbolMasterConfig,
  CacheEntry,
  CacheStats,
  SymbolFormat,
  SymbolMetadata,
} from './types';

// Errors
export {
  SymbolMasterError,
  SymbolNotFoundError,
  InvalidSymbolError,
  MappingNotFoundError,
} from './types';

// Default export
export { symbolMasterService as default } from './SymbolMasterService';
