/**
 * Symbol Master Service
 * Central service for managing symbols, transformations, and lookups
 */

import type {
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
} from './types';

import { SymbolSearchService } from './SymbolSearchService';
import { SymbolTransformer, normalizeSymbol, parseDerivative } from './SymbolTransformer';
import { SymbolCacheService } from './SymbolCacheService';
import {
  SymbolNotFoundError,
  InvalidSymbolError,
  MappingNotFoundError,
} from './types';

// ==================== SYMBOL MASTER SERVICE ====================

export class SymbolMasterService {
  private searchService: SymbolSearchService;
  private transformer: SymbolTransformer;
  private cache: SymbolCacheService;
  private config: SymbolMasterConfig;
  private initialized: boolean = false;

  constructor(config?: Partial<SymbolMasterConfig>) {
    this.config = {
      memoryCacheSize: 1000,
      memoryCacheTTL: 5 * 60 * 1000, // 5 minutes
      enableIndexedDB: true,
      indexedDBCacheTTL: 24 * 60 * 60 * 1000, // 24 hours
      autoLoadMappings: true,
      defaultBrokers: ['fyers', 'kite', 'kraken'],
      enableAutocomplete: true,
      autocompleteDebounce: 300,
      ...config,
    };

    this.searchService = new SymbolSearchService();
    this.transformer = new SymbolTransformer();
    this.cache = new SymbolCacheService(
      this.config.memoryCacheSize,
      this.config.memoryCacheTTL,
      this.config.indexedDBCacheTTL,
      this.config.enableIndexedDB
    );
  }

  // ==================== INITIALIZATION ====================

  /**
   * Initialize the service
   */
  async initialize(): Promise<void> {
    if (this.initialized) return;

    console.log('[SymbolMaster] Initializing...');

    // Load cached data if available
    await this.loadFromCache();

    this.initialized = true;
    console.log('[SymbolMaster] Initialized successfully');
  }

  /**
   * Load symbols from cache
   */
  private async loadFromCache(): Promise<void> {
    try {
      const cached = await this.cache.get<UnifiedSymbol[]>('all_symbols');
      if (cached && cached.length > 0) {
        this.searchService.loadSymbols(cached);
        console.log(`[SymbolMaster] Loaded ${cached.length} symbols from cache`);
      }
    } catch (error) {
      console.error('[SymbolMaster] Failed to load from cache:', error);
    }
  }

  /**
   * Save symbols to cache
   */
  private async saveToCache(): Promise<void> {
    try {
      const symbols = this.searchService.getAllSymbols();
      await this.cache.set('all_symbols', symbols);
      console.log(`[SymbolMaster] Saved ${symbols.length} symbols to cache`);
    } catch (error) {
      console.error('[SymbolMaster] Failed to save to cache:', error);
    }
  }

  // ==================== SYMBOL LOADING ====================

  /**
   * Load symbols into the database
   */
  async loadSymbols(symbols: UnifiedSymbol[]): Promise<void> {
    this.searchService.loadSymbols(symbols);
    await this.saveToCache();
  }

  /**
   * Add single symbol
   */
  async addSymbol(symbol: UnifiedSymbol): Promise<void> {
    this.searchService.addSymbol(symbol);
    await this.saveToCache();
  }

  /**
   * Add broker symbol mapping
   */
  addBrokerMapping(mapping: BrokerSymbolMapping): void {
    this.transformer.addMapping(mapping);
  }

  /**
   * Add multiple broker mappings
   */
  addBrokerMappings(mappings: BrokerSymbolMapping[]): void {
    this.transformer.addMappings(mappings);
  }

  // ==================== SEARCH ====================

  /**
   * Search for symbols
   */
  async search(request: SymbolSearchRequest): Promise<SymbolSearchResult[]> {
    return this.searchService.search(request);
  }

  /**
   * Autocomplete search (fast, for UI)
   */
  async autocomplete(query: string, limit?: number): Promise<SymbolSearchResult[]> {
    if (!this.config.enableAutocomplete) {
      return this.search({ query, limit });
    }
    return this.searchService.autocomplete(query, limit);
  }

  /**
   * Get symbol by exact match
   */
  getSymbol(symbol: string, exchange: string): UnifiedSymbol | null {
    return this.searchService.getSymbol(symbol, exchange);
  }

  /**
   * Get all symbols
   */
  getAllSymbols(): UnifiedSymbol[] {
    return this.searchService.getAllSymbols();
  }

  /**
   * Get symbols by exchange
   */
  getSymbolsByExchange(exchange: string): UnifiedSymbol[] {
    return this.searchService.getSymbolsByExchange(exchange);
  }

  // ==================== SYMBOL TRANSFORMATION ====================

  /**
   * Transform symbol between formats
   */
  async transformSymbol(request: SymbolTransformRequest): Promise<SymbolTransformResult> {
    return this.transformer.transform(request);
  }

  /**
   * Convert symbol to broker format
   */
  async toBrokerFormat(
    symbol: string,
    exchange: string,
    brokerId: string
  ): Promise<SymbolTransformResult> {
    return this.transformer.transform({
      symbol,
      exchange,
      from: 'unified',
      to: brokerId,
    });
  }

  /**
   * Convert broker symbol to unified format
   */
  async toUnifiedFormat(
    brokerSymbol: string,
    exchange: string,
    brokerId: string
  ): Promise<SymbolTransformResult> {
    return this.transformer.transform({
      symbol: brokerSymbol,
      exchange,
      from: brokerId,
      to: 'unified',
    });
  }

  /**
   * Get token for symbol (broker-specific)
   */
  async getToken(symbol: string, exchange: string, brokerId: string): Promise<string | null> {
    const mapping = this.transformer.findMapping(symbol, exchange, brokerId);
    return mapping?.token || null;
  }

  // ==================== BATCH OPERATIONS ====================

  /**
   * Batch symbol lookup
   */
  async batchLookup(request: BatchSymbolRequest): Promise<BatchSymbolResult> {
    const startTime = Date.now();
    const resolved: UnifiedSymbol[] = [];
    const failed: Array<{ symbol: string; exchange: string; error: string }> = [];

    for (const { symbol, exchange, brokerId } of request.symbols) {
      try {
        let unifiedSymbol: UnifiedSymbol | null = null;

        if (brokerId) {
          // Convert from broker format first
          const transformed = await this.toUnifiedFormat(symbol, exchange, brokerId);
          if (transformed.success && transformed.symbol) {
            unifiedSymbol = this.getSymbol(transformed.symbol, transformed.exchange || exchange);
          }
        } else {
          unifiedSymbol = this.getSymbol(symbol, exchange);
        }

        if (unifiedSymbol) {
          // Add broker mappings if requested
          if (request.includeMappings) {
            const mappings = this.transformer.getMappings(unifiedSymbol.symbol);
            (unifiedSymbol as any).brokerMappings = mappings;
          }
          resolved.push(unifiedSymbol);
        } else {
          failed.push({
            symbol,
            exchange,
            error: 'Symbol not found',
          });
        }
      } catch (error: any) {
        failed.push({
          symbol,
          exchange,
          error: error.message,
        });
      }
    }

    const executionTime = Date.now() - startTime;

    return {
      resolved,
      failed,
      executionTime,
    };
  }

  // ==================== VALIDATION ====================

  /**
   * Validate symbol
   */
  async validateSymbol(request: SymbolValidationRequest): Promise<SymbolValidationResult> {
    const { symbol, exchange, brokerId } = request;
    const errors: string[] = [];
    const warnings: string[] = [];

    // Normalize symbol
    const normalized = normalizeSymbol(symbol);

    if (!normalized || normalized.length === 0) {
      errors.push('Symbol cannot be empty');
      return { valid: false, errors, warnings };
    }

    // Check if symbol exists
    let unifiedSymbol: UnifiedSymbol | null = null;

    if (brokerId) {
      const transformed = await this.toUnifiedFormat(symbol, exchange, brokerId);
      if (transformed.success && transformed.symbol) {
        unifiedSymbol = this.getSymbol(transformed.symbol, transformed.exchange || exchange);
      }
    } else {
      unifiedSymbol = this.getSymbol(normalized, exchange);
    }

    if (!unifiedSymbol) {
      errors.push('Symbol not found in database');

      // Provide suggestions
      const suggestions = await this.autocomplete(normalized, 5);
      const suggestionSymbols = suggestions.map(s => s.symbol.symbol);

      return {
        valid: false,
        errors,
        warnings,
        suggestions: suggestionSymbols,
      };
    }

    // Check if symbol is active
    if (unifiedSymbol.status !== 'ACTIVE') {
      warnings.push(`Symbol is ${unifiedSymbol.status.toLowerCase()}`);
    }

    return {
      valid: true,
      errors,
      warnings,
      normalizedSymbol: unifiedSymbol.symbol,
    };
  }

  // ==================== EXCHANGE DETECTION ====================

  /**
   * Detect exchange for a symbol
   */
  async detectExchange(request: ExchangeDetectionRequest): Promise<ExchangeDetectionResult> {
    const { symbol, region } = request;
    const normalized = normalizeSymbol(symbol);

    // Search for symbol across all exchanges
    const searchResults = await this.search({
      query: normalized,
      region,
      limit: 10,
    });

    if (searchResults.length === 0) {
      return {
        exchange: 'UNKNOWN',
        confidence: 0,
        alternatives: [],
      };
    }

    // Sort by score
    searchResults.sort((a, b) => b.score - a.score);

    const mainExchange = searchResults[0].symbol.exchange;
    const mainConfidence = searchResults[0].score;

    // Get alternative exchanges
    const exchangeCounts = new Map<string, number>();
    searchResults.forEach(result => {
      const exchange = result.symbol.exchange;
      exchangeCounts.set(exchange, (exchangeCounts.get(exchange) || 0) + result.score);
    });

    const alternatives = Array.from(exchangeCounts.entries())
      .filter(([exchange]) => exchange !== mainExchange)
      .map(([exchange, score]) => ({
        exchange,
        confidence: score / searchResults.length,
      }))
      .sort((a, b) => b.confidence - a.confidence)
      .slice(0, 3);

    return {
      exchange: mainExchange,
      confidence: mainConfidence,
      alternatives,
    };
  }

  // ==================== DERIVATIVE HANDLING ====================

  /**
   * Parse derivative symbol
   */
  parseDerivative(symbol: string): ReturnType<typeof parseDerivative> {
    return parseDerivative(symbol);
  }

  /**
   * Check if symbol is a derivative
   */
  isDerivative(symbol: string): boolean {
    const parsed = parseDerivative(symbol);
    return parsed.instrumentType !== 'EQ';
  }

  /**
   * Get underlying symbol for derivative
   */
  getUnderlying(derivativeSymbol: string): string | null {
    const parsed = parseDerivative(derivativeSymbol);
    return parsed.underlying;
  }

  // ==================== STATISTICS ====================

  /**
   * Get service statistics
   */
  getStatistics() {
    return {
      totalSymbols: this.searchService.getDatabaseSize(),
      cacheStats: this.cache.getStats(),
      searchCacheStats: this.searchService.getCacheStats(),
      registeredFormatters: this.transformer.listFormatters(),
      config: this.config,
    };
  }

  /**
   * Clear all caches
   */
  async clearCaches(): Promise<void> {
    await this.cache.clear();
    this.searchService.clear();
    this.transformer.clearMappings();
    console.log('[SymbolMaster] All caches cleared');
  }

  /**
   * Cleanup expired cache entries
   */
  async cleanup(): Promise<void> {
    const deleted = await this.cache.cleanupIndexedDB();
    console.log(`[SymbolMaster] Cleaned up ${deleted} expired cache entries`);
  }
}

// ==================== SINGLETON INSTANCE ====================

export const symbolMasterService = new SymbolMasterService();
export default symbolMasterService;

// Auto-initialize
symbolMasterService.initialize().catch(err => {
  console.error('[SymbolMaster] Auto-initialization failed:', err);
});
