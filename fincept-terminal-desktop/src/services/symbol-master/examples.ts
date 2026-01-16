/**
 * Symbol Master Service - Usage Examples
 * Demonstrates common use cases and integration patterns
 */

import {
  symbolMasterService,
  createMockSymbol,
  type UnifiedSymbol,
  type BrokerSymbolMapping,
} from './index';

// ==================== EXAMPLE 1: BASIC SEARCH ====================

export async function exampleBasicSearch() {
  console.log('\n=== Example 1: Basic Search ===\n');

  // Search for symbols
  const results = await symbolMasterService.search({
    query: 'NIFTY',
    exchange: 'NSE',
    limit: 5,
  });

  console.log(`Found ${results.length} results:`);
  results.forEach((result, index) => {
    console.log(
      `${index + 1}. ${result.symbol.symbol} - ${result.symbol.name} (Score: ${result.score.toFixed(2)})`
    );
  });
}

// ==================== EXAMPLE 2: AUTOCOMPLETE ====================

export async function exampleAutocomplete() {
  console.log('\n=== Example 2: Autocomplete ===\n');

  const queries = ['SBI', 'RELI', 'AAPL'];

  for (const query of queries) {
    const suggestions = await symbolMasterService.autocomplete(query, 3);

    console.log(`\nAutocomplete for "${query}":`);
    suggestions.forEach((suggestion, index) => {
      console.log(`  ${index + 1}. ${suggestion.symbol.symbol} - ${suggestion.symbol.name}`);
    });
  }
}

// ==================== EXAMPLE 3: SYMBOL TRANSFORMATION ====================

export async function exampleSymbolTransformation() {
  console.log('\n=== Example 3: Symbol Transformation ===\n');

  // Convert unified to broker format
  const toBroker = await symbolMasterService.toBrokerFormat('SBIN', 'NSE', 'fyers');

  console.log('Unified to Fyers:');
  console.log(`  Unified: SBIN (NSE)`);
  console.log(`  Fyers: ${toBroker.symbol}`);
  console.log(`  Token: ${toBroker.token || 'N/A'}`);

  // Convert broker format to unified
  const toUnified = await symbolMasterService.toUnifiedFormat('NSE:SBIN-EQ', 'NSE', 'fyers');

  console.log('\nFyers to Unified:');
  console.log(`  Fyers: NSE:SBIN-EQ`);
  console.log(`  Unified: ${toUnified.symbol} (${toUnified.exchange})`);
}

// ==================== EXAMPLE 4: BATCH LOOKUP ====================

export async function exampleBatchLookup() {
  console.log('\n=== Example 4: Batch Lookup ===\n');

  const result = await symbolMasterService.batchLookup({
    symbols: [
      { symbol: 'SBIN', exchange: 'NSE' },
      { symbol: 'RELIANCE', exchange: 'NSE' },
      { symbol: 'TCS', exchange: 'NSE' },
      { symbol: 'INVALID_SYMBOL', exchange: 'NSE' },
    ],
    includeMappings: true,
  });

  console.log(`Resolved: ${result.resolved.length} symbols`);
  console.log(`Failed: ${result.failed.length} symbols`);
  console.log(`Execution time: ${result.executionTime}ms`);

  if (result.failed.length > 0) {
    console.log('\nFailed symbols:');
    result.failed.forEach((failed) => {
      console.log(`  - ${failed.symbol}: ${failed.error}`);
    });
  }
}

// ==================== EXAMPLE 5: SYMBOL VALIDATION ====================

export async function exampleSymbolValidation() {
  console.log('\n=== Example 5: Symbol Validation ===\n');

  const symbols = ['SBIN', 'INVALID', 'RELIANCEE'];

  for (const symbol of symbols) {
    const validation = await symbolMasterService.validateSymbol({
      symbol,
      exchange: 'NSE',
    });

    console.log(`\n${symbol}:`);
    console.log(`  Valid: ${validation.valid}`);

    if (validation.valid) {
      console.log(`  Normalized: ${validation.normalizedSymbol}`);
      if (validation.warnings.length > 0) {
        console.log(`  Warnings: ${validation.warnings.join(', ')}`);
      }
    } else {
      console.log(`  Errors: ${validation.errors.join(', ')}`);
      if (validation.suggestions && validation.suggestions.length > 0) {
        console.log(`  Suggestions: ${validation.suggestions.join(', ')}`);
      }
    }
  }
}

// ==================== EXAMPLE 6: EXCHANGE DETECTION ====================

export async function exampleExchangeDetection() {
  console.log('\n=== Example 6: Exchange Detection ===\n');

  const symbols = ['NIFTY', 'AAPL', 'SBIN'];

  for (const symbol of symbols) {
    const detection = await symbolMasterService.detectExchange({
      symbol,
      region: 'IN',
    });

    console.log(`\n${symbol}:`);
    console.log(`  Exchange: ${detection.exchange}`);
    console.log(`  Confidence: ${(detection.confidence * 100).toFixed(1)}%`);

    if (detection.alternatives.length > 0) {
      console.log('  Alternatives:');
      detection.alternatives.forEach((alt) => {
        console.log(`    - ${alt.exchange} (${(alt.confidence * 100).toFixed(1)}%)`);
      });
    }
  }
}

// ==================== EXAMPLE 7: DERIVATIVE HANDLING ====================

export async function exampleDerivativeHandling() {
  console.log('\n=== Example 7: Derivative Handling ===\n');

  const derivatives = [
    'NIFTY25JAN23800CE',
    'BANKNIFTY25JAN45000PE',
    'RELIANCE25FEB2800FUT',
  ];

  for (const symbol of derivatives) {
    const isDerivative = symbolMasterService.isDerivative(symbol);
    console.log(`\n${symbol}:`);
    console.log(`  Is Derivative: ${isDerivative}`);

    if (isDerivative) {
      const parsed = symbolMasterService.parseDerivative(symbol);
      console.log(`  Underlying: ${parsed.underlying}`);
      console.log(`  Expiry: ${parsed.expiry}`);
      console.log(`  Strike: ${parsed.strike || 'N/A'}`);
      console.log(`  Option Type: ${parsed.optionType || 'N/A'}`);
      console.log(`  Instrument Type: ${parsed.instrumentType}`);

      const underlying = symbolMasterService.getUnderlying(symbol);
      console.log(`  Underlying Symbol: ${underlying}`);
    }
  }
}

// ==================== EXAMPLE 8: LOADING SYMBOLS ====================

export async function exampleLoadingSymbols() {
  console.log('\n=== Example 8: Loading Symbols ===\n');

  // Create mock symbols
  const symbols: UnifiedSymbol[] = [
    createMockSymbol('SBIN', 'State Bank of India', 'NSE', 'EQ'),
    createMockSymbol('RELIANCE', 'Reliance Industries Limited', 'NSE', 'EQ'),
    createMockSymbol('TCS', 'Tata Consultancy Services', 'NSE', 'EQ'),
    createMockSymbol('AAPL', 'Apple Inc.', 'NASDAQ', 'EQ'),
    createMockSymbol('GOOGL', 'Alphabet Inc.', 'NASDAQ', 'EQ'),
  ];

  console.log(`Loading ${symbols.length} symbols...`);
  await symbolMasterService.loadSymbols(symbols);

  // Verify
  const stats = symbolMasterService.getStatistics();
  console.log(`Total symbols in database: ${stats.totalSymbols}`);
}

// ==================== EXAMPLE 9: BROKER MAPPINGS ====================

export async function exampleBrokerMappings() {
  console.log('\n=== Example 9: Broker Mappings ===\n');

  // Add broker mapping
  const mapping: BrokerSymbolMapping = {
    unifiedSymbol: 'SBIN',
    brokerSymbol: 'NSE:SBIN-EQ',
    brokerId: 'fyers',
    brokerExchange: 'NSE',
    token: '101000000004106',
    brokerMetadata: {
      lotSize: 1,
      tickSize: 0.05,
    },
  };

  console.log('Adding broker mapping:');
  console.log(`  Unified: ${mapping.unifiedSymbol}`);
  console.log(`  Broker: ${mapping.brokerSymbol} (${mapping.brokerId})`);
  console.log(`  Token: ${mapping.token}`);

  symbolMasterService.addBrokerMapping(mapping);

  // Test transformation
  const result = await symbolMasterService.toBrokerFormat('SBIN', 'NSE', 'fyers');

  console.log('\nTransformation result:');
  console.log(`  Success: ${result.success}`);
  console.log(`  Symbol: ${result.symbol}`);
  console.log(`  Token: ${result.token}`);
}

// ==================== EXAMPLE 10: CACHE STATISTICS ====================

export async function exampleCacheStatistics() {
  console.log('\n=== Example 10: Cache Statistics ===\n');

  // Perform some operations to populate cache
  await symbolMasterService.search({ query: 'NIFTY', limit: 5 });
  await symbolMasterService.autocomplete('SBI', 5);
  await symbolMasterService.toBrokerFormat('SBIN', 'NSE', 'fyers');

  // Get statistics
  const stats = symbolMasterService.getStatistics();

  console.log('Service Statistics:');
  console.log(`  Total Symbols: ${stats.totalSymbols}`);
  console.log('\nCache Stats:');
  console.log(`  Size: ${stats.cacheStats.size} entries`);
  console.log(`  Hits: ${stats.cacheStats.hits}`);
  console.log(`  Misses: ${stats.cacheStats.misses}`);
  console.log(`  Hit Rate: ${(stats.cacheStats.hitRate * 100).toFixed(1)}%`);
  console.log(`  Memory Usage: ${(stats.cacheStats.memoryUsage / 1024).toFixed(2)} KB`);
  console.log('\nRegistered Formatters:');
  stats.registeredFormatters.forEach((formatter) => {
    console.log(`  - ${formatter}`);
  });
}

// ==================== RUN ALL EXAMPLES ====================

export async function runAllExamples() {
  console.log('╔════════════════════════════════════════════╗');
  console.log('║  Symbol Master Service - Usage Examples   ║');
  console.log('╚════════════════════════════════════════════╝');

  try {
    // Load sample data first
    await exampleLoadingSymbols();

    // Run examples
    await exampleBasicSearch();
    await exampleAutocomplete();
    await exampleSymbolTransformation();
    await exampleBatchLookup();
    await exampleSymbolValidation();
    await exampleExchangeDetection();
    await exampleDerivativeHandling();
    await exampleBrokerMappings();
    await exampleCacheStatistics();

    console.log('\n✅ All examples completed successfully!\n');
  } catch (error) {
    console.error('\n❌ Error running examples:', error);
  }
}

// Auto-run if executed directly
if (typeof window === 'undefined') {
  // Node.js environment
  runAllExamples();
}
