/**
 * Symbol Master Service - Type Definitions
 * Unified symbol management across all brokers and data sources
 */

// ==================== CORE SYMBOL TYPES ====================

export interface UnifiedSymbol {
  /** Normalized symbol (e.g., "AAPL", "NIFTY") */
  symbol: string;
  /** Company/instrument name */
  name: string;
  /** Exchange code (NSE, BSE, NYSE, NASDAQ, etc.) */
  exchange: string;
  /** Instrument type (EQ, FUT, CE, PE, INDEX, etc.) */
  instrumentType: string;
  /** Country/region code (IN, US, etc.) */
  region: string;
  /** Currency code (INR, USD, etc.) */
  currency: string;
  /** ISIN code (if available) */
  isin?: string;
  /** Lot size for trading */
  lotSize: number;
  /** Minimum tick size */
  tickSize: number;
  /** Trading status (ACTIVE, SUSPENDED, DELISTED) */
  status: 'ACTIVE' | 'SUSPENDED' | 'DELISTED';
  /** Metadata (optional) */
  metadata?: Record<string, any>;
}

export interface DerivativeSymbol extends UnifiedSymbol {
  /** Underlying asset symbol */
  underlying: string;
  /** Expiry date (ISO string) */
  expiry: string;
  /** Strike price (for options) */
  strike?: number;
  /** Option type (CE/PE/CALL/PUT) */
  optionType?: 'CE' | 'PE' | 'CALL' | 'PUT';
}

// ==================== BROKER-SPECIFIC MAPPINGS ====================

export interface BrokerSymbolMapping {
  /** Fincept unified symbol */
  unifiedSymbol: string;
  /** Broker-specific symbol format */
  brokerSymbol: string;
  /** Broker ID (fyers, kite, kraken, etc.) */
  brokerId: string;
  /** Exchange on broker platform */
  brokerExchange: string;
  /** Broker's internal token/ID */
  token?: string;
  /** Additional broker-specific data */
  brokerMetadata?: Record<string, any>;
}

// ==================== SEARCH TYPES ====================

export interface SymbolSearchRequest {
  /** Search query (partial symbol, name, ISIN) */
  query: string;
  /** Filter by exchange */
  exchange?: string;
  /** Filter by instrument type */
  instrumentType?: string;
  /** Filter by region */
  region?: string;
  /** Maximum results to return */
  limit?: number;
  /** Include inactive symbols */
  includeInactive?: boolean;
}

export interface SymbolSearchResult {
  /** Unified symbol */
  symbol: UnifiedSymbol;
  /** Match score (0-1, higher is better) */
  score: number;
  /** Matched fields (symbol, name, isin) */
  matchedFields: string[];
  /** Broker mappings (if available) */
  brokerMappings?: BrokerSymbolMapping[];
}

// ==================== CACHE TYPES ====================

export interface CacheEntry<T> {
  /** Cached data */
  data: T;
  /** Timestamp when cached */
  timestamp: number;
  /** Time-to-live in milliseconds */
  ttl: number;
}

export interface CacheStats {
  /** Total entries in cache */
  size: number;
  /** Cache hit count */
  hits: number;
  /** Cache miss count */
  misses: number;
  /** Hit rate (0-1) */
  hitRate: number;
  /** Memory usage estimate (bytes) */
  memoryUsage: number;
}

// ==================== SYMBOL TRANSFORMATION ====================

export interface SymbolTransformRequest {
  /** Input symbol */
  symbol: string;
  /** Input exchange */
  exchange: string;
  /** Source format (broker ID or 'unified') */
  from: string;
  /** Target format (broker ID or 'unified') */
  to: string;
}

export interface SymbolTransformResult {
  /** Success flag */
  success: boolean;
  /** Transformed symbol */
  symbol?: string;
  /** Transformed exchange */
  exchange?: string;
  /** Token (if applicable) */
  token?: string;
  /** Error message (if failed) */
  error?: string;
  /** Original request */
  request: SymbolTransformRequest;
}

// ==================== BATCH OPERATIONS ====================

export interface BatchSymbolRequest {
  /** List of symbols to lookup */
  symbols: Array<{
    symbol: string;
    exchange: string;
    brokerId?: string;
  }>;
  /** Include broker mappings */
  includeMappings?: boolean;
}

export interface BatchSymbolResult {
  /** Successfully resolved symbols */
  resolved: UnifiedSymbol[];
  /** Failed lookups */
  failed: Array<{
    symbol: string;
    exchange: string;
    error: string;
  }>;
  /** Execution time (ms) */
  executionTime: number;
}

// ==================== VALIDATION ====================

export interface SymbolValidationRequest {
  symbol: string;
  exchange: string;
  brokerId?: string;
}

export interface SymbolValidationResult {
  /** Is valid */
  valid: boolean;
  /** Validation errors (if any) */
  errors: string[];
  /** Warnings (symbol exists but inactive, etc.) */
  warnings: string[];
  /** Normalized symbol (if valid) */
  normalizedSymbol?: string;
  /** Suggestions (if invalid) */
  suggestions?: string[];
}

// ==================== EXCHANGE DETECTION ====================

export interface ExchangeDetectionRequest {
  /** Symbol to analyze */
  symbol: string;
  /** Hint: country/region */
  region?: string;
}

export interface ExchangeDetectionResult {
  /** Detected exchange */
  exchange: string;
  /** Confidence level (0-1) */
  confidence: number;
  /** Alternative exchanges (with confidence) */
  alternatives: Array<{
    exchange: string;
    confidence: number;
  }>;
}

// ==================== SERVICE CONFIGURATION ====================

export interface SymbolMasterConfig {
  /** Memory cache size (max entries) */
  memoryCacheSize: number;
  /** Memory cache TTL (ms) */
  memoryCacheTTL: number;
  /** Enable IndexedDB cache */
  enableIndexedDB: boolean;
  /** IndexedDB cache TTL (ms) */
  indexedDBCacheTTL: number;
  /** Auto-load broker mappings */
  autoLoadMappings: boolean;
  /** Default broker IDs to load */
  defaultBrokers: string[];
  /** Enable autocomplete */
  enableAutocomplete: boolean;
  /** Autocomplete debounce (ms) */
  autocompleteDebounce: number;
}

// ==================== ERROR TYPES ====================

export class SymbolMasterError extends Error {
  constructor(
    message: string,
    public code?: string,
    public details?: any
  ) {
    super(message);
    this.name = 'SymbolMasterError';
  }
}

export class SymbolNotFoundError extends SymbolMasterError {
  constructor(symbol: string, exchange: string) {
    super(`Symbol not found: ${symbol} on ${exchange}`, 'SYMBOL_NOT_FOUND', { symbol, exchange });
    this.name = 'SymbolNotFoundError';
  }
}

export class InvalidSymbolError extends SymbolMasterError {
  constructor(symbol: string, reason: string) {
    super(`Invalid symbol: ${symbol} - ${reason}`, 'INVALID_SYMBOL', { symbol, reason });
    this.name = 'InvalidSymbolError';
  }
}

export class MappingNotFoundError extends SymbolMasterError {
  constructor(symbol: string, brokerId: string) {
    super(`Mapping not found for ${symbol} on broker ${brokerId}`, 'MAPPING_NOT_FOUND', { symbol, brokerId });
    this.name = 'MappingNotFoundError';
  }
}

// ==================== UTILITY TYPES ====================

export type SymbolFormat = 'unified' | string; // 'unified' or broker ID

export interface SymbolMetadata {
  /** Last updated timestamp */
  lastUpdated: number;
  /** Source of data (API, manual, etc.) */
  source: string;
  /** Data version */
  version: string;
}
