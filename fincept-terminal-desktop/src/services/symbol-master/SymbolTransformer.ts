/**
 * Symbol Transformer
 * Converts symbols between different broker formats
 */

import type {
  SymbolTransformRequest,
  SymbolTransformResult,
  BrokerSymbolMapping,
} from './types';
import { SymbolCacheService } from './SymbolCacheService';

// ==================== BROKER FORMAT TRANSFORMERS ====================

interface BrokerFormatter {
  /** Broker ID */
  id: string;
  /** Format symbol for broker */
  format: (symbol: string, exchange: string, metadata?: any) => string;
  /** Parse broker symbol to unified format */
  parse: (brokerSymbol: string) => { symbol: string; exchange: string } | null;
}

// Fyers formatter
const fyersFormatter: BrokerFormatter = {
  id: 'fyers',
  format: (symbol, exchange, metadata) => {
    // Fyers format: EXCHANGE:SYMBOL
    // Example: NSE:SBIN-EQ, NFO:NIFTY25JAN23800CE
    const suffix = metadata?.instrumentType === 'EQ' ? '-EQ' : '';
    return `${exchange}:${symbol}${suffix}`;
  },
  parse: (brokerSymbol) => {
    const match = brokerSymbol.match(/^([A-Z_]+):([A-Z0-9-]+)$/);
    if (!match) return null;

    let exchange = match[1];
    let symbol = match[2].replace('-EQ', ''); // Remove -EQ suffix

    return { symbol, exchange };
  },
};

// Kite (Zerodha) formatter
const kiteFormatter: BrokerFormatter = {
  id: 'kite',
  format: (symbol, exchange, metadata) => {
    // Kite format varies by exchange
    // NSE: SYMBOL, NFO: NIFTY2510023800CE
    return symbol;
  },
  parse: (brokerSymbol) => {
    // Kite symbols are generally clean
    return { symbol: brokerSymbol, exchange: 'NSE' };
  },
};

// Kraken (crypto) formatter
const krakenFormatter: BrokerFormatter = {
  id: 'kraken',
  format: (symbol, exchange, metadata) => {
    // Kraken format: BASEQUOTE (e.g., BTCUSD, ETHUSD)
    // Some pairs have X/Z prefixes (XXBTZUSD)
    return symbol.replace('/', '');
  },
  parse: (brokerSymbol) => {
    // Parse crypto pairs
    // Common patterns: BTCUSD, ETHUSD, XBTUSD, etc.
    const commonQuotes = ['USD', 'EUR', 'GBP', 'JPY', 'USDT', 'USDC'];

    for (const quote of commonQuotes) {
      if (brokerSymbol.endsWith(quote)) {
        const base = brokerSymbol.slice(0, -quote.length);
        return { symbol: `${base}/${quote}`, exchange: 'KRAKEN' };
      }
    }

    return { symbol: brokerSymbol, exchange: 'KRAKEN' };
  },
};

// HyperLiquid formatter
const hyperliquidFormatter: BrokerFormatter = {
  id: 'hyperliquid',
  format: (symbol, exchange, metadata) => {
    // HyperLiquid uses simple pairs: BTC, ETH, etc.
    return symbol.replace('/', '');
  },
  parse: (brokerSymbol) => {
    return { symbol: brokerSymbol, exchange: 'HYPERLIQUID' };
  },
};

// Generic unified formatter (passthrough)
const unifiedFormatter: BrokerFormatter = {
  id: 'unified',
  format: (symbol, exchange) => symbol,
  parse: (brokerSymbol) => ({ symbol: brokerSymbol, exchange: 'UNKNOWN' }),
};

// ==================== SYMBOL TRANSFORMER ====================

export class SymbolTransformer {
  private formatters: Map<string, BrokerFormatter>;
  private cache: SymbolCacheService;
  private mappingStore: Map<string, BrokerSymbolMapping[]>;

  constructor() {
    this.formatters = new Map();
    this.cache = new SymbolCacheService(500, 10 * 60 * 1000); // 10 min cache
    this.mappingStore = new Map();

    // Register formatters
    this.registerFormatter(fyersFormatter);
    this.registerFormatter(kiteFormatter);
    this.registerFormatter(krakenFormatter);
    this.registerFormatter(hyperliquidFormatter);
    this.registerFormatter(unifiedFormatter);
  }

  /**
   * Register a broker formatter
   */
  registerFormatter(formatter: BrokerFormatter): void {
    this.formatters.set(formatter.id, formatter);
  }

  /**
   * Transform symbol between formats
   */
  async transform(request: SymbolTransformRequest): Promise<SymbolTransformResult> {
    const { symbol, exchange, from, to } = request;

    // Check cache
    const cacheKey = SymbolCacheService.generateKey('transform', symbol, exchange, from, to);
    const cached = await this.cache.get<SymbolTransformResult>(cacheKey);
    if (cached) return cached;

    try {
      let transformedSymbol = symbol;
      let transformedExchange = exchange;
      let token: string | undefined;

      // Step 1: Parse from source format to unified
      if (from !== 'unified') {
        const sourceFormatter = this.formatters.get(from);
        if (!sourceFormatter) {
          throw new Error(`Unknown source formatter: ${from}`);
        }

        const parsed = sourceFormatter.parse(symbol);
        if (!parsed) {
          throw new Error(`Failed to parse symbol: ${symbol}`);
        }

        transformedSymbol = parsed.symbol;
        transformedExchange = parsed.exchange || exchange;
      }

      // Step 2: Check for explicit mapping
      const mapping = this.findMapping(transformedSymbol, transformedExchange, to);
      if (mapping) {
        transformedSymbol = mapping.brokerSymbol;
        transformedExchange = mapping.brokerExchange;
        token = mapping.token;
      }

      // Step 3: Format to target format
      if (to !== 'unified' && !mapping) {
        const targetFormatter = this.formatters.get(to);
        if (!targetFormatter) {
          throw new Error(`Unknown target formatter: ${to}`);
        }

        transformedSymbol = targetFormatter.format(transformedSymbol, transformedExchange);
      }

      const result: SymbolTransformResult = {
        success: true,
        symbol: transformedSymbol,
        exchange: transformedExchange,
        token,
        request,
      };

      // Cache result
      await this.cache.set(cacheKey, result);

      return result;

    } catch (error: any) {
      const result: SymbolTransformResult = {
        success: false,
        error: error.message,
        request,
      };

      return result;
    }
  }

  /**
   * Batch transform symbols
   */
  async transformBatch(
    symbols: Array<{ symbol: string; exchange: string }>,
    from: string,
    to: string
  ): Promise<SymbolTransformResult[]> {
    const promises = symbols.map(({ symbol, exchange }) =>
      this.transform({ symbol, exchange, from, to })
    );

    return Promise.all(promises);
  }

  /**
   * Add broker symbol mapping
   */
  addMapping(mapping: BrokerSymbolMapping): void {
    const key = `${mapping.unifiedSymbol}_${mapping.brokerId}`;
    const existing = this.mappingStore.get(key) || [];
    existing.push(mapping);
    this.mappingStore.set(key, existing);

    // Invalidate cache for this symbol
    this.cache.delete(SymbolCacheService.generateKey('mapping', mapping.unifiedSymbol, mapping.brokerId));
  }

  /**
   * Add multiple mappings
   */
  addMappings(mappings: BrokerSymbolMapping[]): void {
    mappings.forEach(mapping => this.addMapping(mapping));
  }

  /**
   * Find mapping for symbol and broker
   */
  findMapping(unifiedSymbol: string, exchange: string, brokerId: string): BrokerSymbolMapping | null {
    const key = `${unifiedSymbol}_${brokerId}`;
    const mappings = this.mappingStore.get(key);

    if (!mappings || mappings.length === 0) return null;

    // Find exact match by exchange
    const exactMatch = mappings.find(m => m.brokerExchange === exchange);
    if (exactMatch) return exactMatch;

    // Return first mapping as fallback
    return mappings[0];
  }

  /**
   * Get all mappings for a symbol
   */
  getMappings(unifiedSymbol: string): BrokerSymbolMapping[] {
    const allMappings: BrokerSymbolMapping[] = [];

    for (const [key, mappings] of this.mappingStore.entries()) {
      if (key.startsWith(unifiedSymbol + '_')) {
        allMappings.push(...mappings);
      }
    }

    return allMappings;
  }

  /**
   * Clear all mappings
   */
  clearMappings(): void {
    this.mappingStore.clear();
    this.cache.clear();
  }

  /**
   * Get formatter for broker
   */
  getFormatter(brokerId: string): BrokerFormatter | undefined {
    return this.formatters.get(brokerId);
  }

  /**
   * List all registered formatters
   */
  listFormatters(): string[] {
    return Array.from(this.formatters.keys());
  }
}

// ==================== UTILITY FUNCTIONS ====================

/**
 * Normalize symbol (uppercase, trim, remove special chars)
 */
export function normalizeSymbol(symbol: string): string {
  return symbol
    .trim()
    .toUpperCase()
    .replace(/[^A-Z0-9-_/:]/g, '');
}

/**
 * Check if symbol is a derivative
 */
export function isDerivative(symbol: string): boolean {
  // Check for common derivative patterns
  const patterns = [
    /\d{2}[A-Z]{3}\d{2}/,      // Date pattern: 25JAN23
    /FUT$/,                     // Futures: ends with FUT
    /CE$/,                      // Call option: ends with CE
    /PE$/,                      // Put option: ends with PE
    /CALL$/,                    // Call option: ends with CALL
    /PUT$/,                     // Put option: ends with PUT
  ];

  return patterns.some(pattern => pattern.test(symbol));
}

/**
 * Extract underlying from derivative symbol
 */
export function extractUnderlying(derivativeSymbol: string): string | null {
  // Remove date patterns
  let underlying = derivativeSymbol.replace(/\d{2}[A-Z]{3}\d{2}/g, '');

  // Remove option type suffixes
  underlying = underlying.replace(/(CE|PE|CALL|PUT|FUT)$/g, '');

  // Remove strike price (digits at end)
  underlying = underlying.replace(/\d+$/g, '');

  return underlying || null;
}

/**
 * Parse derivative info from symbol
 */
export function parseDerivative(symbol: string): {
  underlying: string | null;
  expiry: string | null;
  strike: number | null;
  optionType: 'CE' | 'PE' | null;
  instrumentType: string;
} {
  let underlying = null;
  let expiry = null;
  let strike: number | null = null;
  let optionType: 'CE' | 'PE' | null = null;
  let instrumentType = 'EQ';

  // Extract expiry (DDMMMYY format)
  const expiryMatch = symbol.match(/(\d{2}[A-Z]{3}\d{2})/);
  if (expiryMatch) {
    expiry = expiryMatch[1];
    instrumentType = 'FUT';
  }

  // Extract option type
  if (symbol.endsWith('CE')) {
    optionType = 'CE';
    instrumentType = 'CE';
  } else if (symbol.endsWith('PE')) {
    optionType = 'PE';
    instrumentType = 'PE';
  }

  // Extract strike price (digits before option type)
  if (optionType) {
    const strikeMatch = symbol.match(/(\d+)(CE|PE)$/);
    if (strikeMatch) {
      strike = parseInt(strikeMatch[1]);
    }
  }

  // Extract underlying
  underlying = extractUnderlying(symbol);

  return { underlying, expiry, strike, optionType, instrumentType };
}

// Export singleton instance
export const symbolTransformer = new SymbolTransformer();
export default symbolTransformer;
