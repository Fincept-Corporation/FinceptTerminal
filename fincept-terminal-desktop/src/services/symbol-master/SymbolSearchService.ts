/**
 * Symbol Search Service
 * Fast symbol search with autocomplete and fuzzy matching
 */

import type {
  UnifiedSymbol,
  SymbolSearchRequest,
  SymbolSearchResult,
} from './types';
import { SymbolCacheService } from './SymbolCacheService';

// ==================== SEARCH ALGORITHMS ====================

/**
 * Calculate Levenshtein distance between two strings
 */
function levenshteinDistance(a: string, b: string): number {
  const matrix: number[][] = [];

  for (let i = 0; i <= b.length; i++) {
    matrix[i] = [i];
  }

  for (let j = 0; j <= a.length; j++) {
    matrix[0][j] = j;
  }

  for (let i = 1; i <= b.length; i++) {
    for (let j = 1; j <= a.length; j++) {
      if (b.charAt(i - 1) === a.charAt(j - 1)) {
        matrix[i][j] = matrix[i - 1][j - 1];
      } else {
        matrix[i][j] = Math.min(
          matrix[i - 1][j - 1] + 1, // substitution
          matrix[i][j - 1] + 1,     // insertion
          matrix[i - 1][j] + 1      // deletion
        );
      }
    }
  }

  return matrix[b.length][a.length];
}

/**
 * Calculate similarity score (0-1, higher is better)
 */
function calculateSimilarity(query: string, target: string): number {
  if (query === target) return 1.0;
  if (query.length === 0 || target.length === 0) return 0;

  const distance = levenshteinDistance(query.toLowerCase(), target.toLowerCase());
  const maxLength = Math.max(query.length, target.length);

  return 1 - (distance / maxLength);
}

/**
 * Calculate match score with weighted factors
 */
function calculateMatchScore(query: string, symbol: UnifiedSymbol): number {
  const normalizedQuery = query.toLowerCase();
  const normalizedSymbol = symbol.symbol.toLowerCase();
  const normalizedName = symbol.name.toLowerCase();

  let score = 0;
  let matchedFields: string[] = [];

  // Exact match (highest priority)
  if (normalizedSymbol === normalizedQuery) {
    score = 1.0;
    matchedFields.push('symbol');
  }
  // Starts with query (high priority)
  else if (normalizedSymbol.startsWith(normalizedQuery)) {
    score = 0.9;
    matchedFields.push('symbol');
  }
  // Contains query
  else if (normalizedSymbol.includes(normalizedQuery)) {
    score = 0.7;
    matchedFields.push('symbol');
  }
  // Name exact match
  else if (normalizedName === normalizedQuery) {
    score = 0.85;
    matchedFields.push('name');
  }
  // Name starts with
  else if (normalizedName.startsWith(normalizedQuery)) {
    score = 0.8;
    matchedFields.push('name');
  }
  // Name contains
  else if (normalizedName.includes(normalizedQuery)) {
    score = 0.6;
    matchedFields.push('name');
  }
  // ISIN match
  else if (symbol.isin && symbol.isin.toLowerCase().includes(normalizedQuery)) {
    score = 0.75;
    matchedFields.push('isin');
  }
  // Fuzzy match (fallback)
  else {
    const symbolSimilarity = calculateSimilarity(normalizedQuery, normalizedSymbol);
    const nameSimilarity = calculateSimilarity(normalizedQuery, normalizedName);
    score = Math.max(symbolSimilarity, nameSimilarity) * 0.5;

    if (symbolSimilarity > nameSimilarity) {
      matchedFields.push('symbol');
    } else {
      matchedFields.push('name');
    }
  }

  return score;
}

// ==================== TRIE FOR AUTOCOMPLETE ====================

class TrieNode {
  children: Map<string, TrieNode> = new Map();
  symbols: UnifiedSymbol[] = [];
  isEnd: boolean = false;
}

class Trie {
  private root: TrieNode = new TrieNode();

  /**
   * Insert symbol into trie
   */
  insert(symbol: UnifiedSymbol): void {
    // Insert by symbol
    this.insertString(symbol.symbol, symbol);

    // Insert by name words
    const nameWords = symbol.name.toLowerCase().split(/\s+/);
    nameWords.forEach(word => {
      if (word.length > 2) { // Skip very short words
        this.insertString(word, symbol);
      }
    });
  }

  /**
   * Insert string into trie
   */
  private insertString(str: string, symbol: UnifiedSymbol): void {
    let node = this.root;
    const normalized = str.toLowerCase();

    for (const char of normalized) {
      if (!node.children.has(char)) {
        node.children.set(char, new TrieNode());
      }
      node = node.children.get(char)!;
    }

    node.isEnd = true;
    if (!node.symbols.find(s => s.symbol === symbol.symbol)) {
      node.symbols.push(symbol);
    }
  }

  /**
   * Search for symbols by prefix
   */
  search(prefix: string): UnifiedSymbol[] {
    let node = this.root;
    const normalized = prefix.toLowerCase();

    // Navigate to prefix node
    for (const char of normalized) {
      if (!node.children.has(char)) {
        return [];
      }
      node = node.children.get(char)!;
    }

    // Collect all symbols under this prefix
    const results: UnifiedSymbol[] = [];
    const seen = new Set<string>();

    const collectSymbols = (n: TrieNode) => {
      n.symbols.forEach(symbol => {
        if (!seen.has(symbol.symbol)) {
          results.push(symbol);
          seen.add(symbol.symbol);
        }
      });

      n.children.forEach(child => collectSymbols(child));
    };

    collectSymbols(node);
    return results;
  }

  /**
   * Clear trie
   */
  clear(): void {
    this.root = new TrieNode();
  }
}

// ==================== SYMBOL SEARCH SERVICE ====================

export class SymbolSearchService {
  private symbolDatabase: Map<string, UnifiedSymbol>;
  private trie: Trie;
  private cache: SymbolCacheService;

  constructor() {
    this.symbolDatabase = new Map();
    this.trie = new Trie();
    this.cache = new SymbolCacheService(200, 2 * 60 * 1000); // 2 min cache for searches
  }

  /**
   * Load symbols into search index
   */
  loadSymbols(symbols: UnifiedSymbol[]): void {
    this.symbolDatabase.clear();
    this.trie.clear();

    symbols.forEach(symbol => {
      const key = `${symbol.symbol}_${symbol.exchange}`;
      this.symbolDatabase.set(key, symbol);
      this.trie.insert(symbol);
    });

    console.log(`[SymbolSearch] Loaded ${symbols.length} symbols into search index`);
  }

  /**
   * Add single symbol to index
   */
  addSymbol(symbol: UnifiedSymbol): void {
    const key = `${symbol.symbol}_${symbol.exchange}`;
    this.symbolDatabase.set(key, symbol);
    this.trie.insert(symbol);
  }

  /**
   * Search symbols
   */
  async search(request: SymbolSearchRequest): Promise<SymbolSearchResult[]> {
    const {
      query,
      exchange,
      instrumentType,
      region,
      limit = 20,
      includeInactive = false,
    } = request;

    if (!query || query.length === 0) {
      return [];
    }

    // Check cache
    const cacheKey = SymbolCacheService.generateKey(
      'search',
      query,
      exchange || 'all',
      instrumentType || 'all',
      region || 'all',
      String(limit)
    );

    const cached = await this.cache.get<SymbolSearchResult[]>(cacheKey);
    if (cached) return cached;

    // Get candidates from trie (prefix search)
    const candidates = this.trie.search(query);

    // Apply filters
    let filtered = candidates.filter(symbol => {
      if (!includeInactive && symbol.status !== 'ACTIVE') return false;
      if (exchange && symbol.exchange !== exchange) return false;
      if (instrumentType && symbol.instrumentType !== instrumentType) return false;
      if (region && symbol.region !== region) return false;
      return true;
    });

    // Calculate scores and sort
    const results: SymbolSearchResult[] = filtered.map(symbol => {
      const score = calculateMatchScore(query, symbol);
      return {
        symbol,
        score,
        matchedFields: [], // Will be populated in calculateMatchScore
      };
    });

    // Sort by score (descending)
    results.sort((a, b) => b.score - a.score);

    // Apply limit
    const limited = results.slice(0, limit);

    // Cache results
    await this.cache.set(cacheKey, limited);

    return limited;
  }

  /**
   * Autocomplete search (optimized for speed)
   */
  async autocomplete(query: string, limit: number = 10): Promise<SymbolSearchResult[]> {
    if (!query || query.length === 0) return [];

    // Use trie for fast prefix search
    const candidates = this.trie.search(query);

    // Quick scoring and sorting
    const results = candidates
      .filter(symbol => symbol.status === 'ACTIVE')
      .map(symbol => ({
        symbol,
        score: calculateMatchScore(query, symbol),
        matchedFields: ['symbol'],
      }))
      .sort((a, b) => b.score - a.score)
      .slice(0, limit);

    return results;
  }

  /**
   * Get symbol by exact symbol + exchange
   */
  getSymbol(symbol: string, exchange: string): UnifiedSymbol | null {
    const key = `${symbol}_${exchange}`;
    return this.symbolDatabase.get(key) || null;
  }

  /**
   * Get all symbols
   */
  getAllSymbols(): UnifiedSymbol[] {
    return Array.from(this.symbolDatabase.values());
  }

  /**
   * Get symbols by exchange
   */
  getSymbolsByExchange(exchange: string): UnifiedSymbol[] {
    return this.getAllSymbols().filter(s => s.exchange === exchange);
  }

  /**
   * Get symbols by instrument type
   */
  getSymbolsByInstrumentType(instrumentType: string): UnifiedSymbol[] {
    return this.getAllSymbols().filter(s => s.instrumentType === instrumentType);
  }

  /**
   * Get database size
   */
  getDatabaseSize(): number {
    return this.symbolDatabase.size;
  }

  /**
   * Clear database
   */
  clear(): void {
    this.symbolDatabase.clear();
    this.trie.clear();
    this.cache.clear();
  }

  /**
   * Get cache statistics
   */
  getCacheStats() {
    return this.cache.getStats();
  }
}

// ==================== UTILITY FUNCTIONS ====================

/**
 * Create mock symbol data for testing
 */
export function createMockSymbol(
  symbol: string,
  name: string,
  exchange: string,
  instrumentType: string = 'EQ'
): UnifiedSymbol {
  return {
    symbol,
    name,
    exchange,
    instrumentType,
    region: exchange.startsWith('N') || exchange.startsWith('B') ? 'IN' : 'US',
    currency: exchange.startsWith('N') || exchange.startsWith('B') ? 'INR' : 'USD',
    lotSize: 1,
    tickSize: 0.05,
    status: 'ACTIVE',
  };
}

// Export singleton instance
export const symbolSearchService = new SymbolSearchService();
export default symbolSearchService;
