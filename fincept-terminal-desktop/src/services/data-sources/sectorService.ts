// Sector Classification Service
// Maps stock symbols to their sectors and industries
// Uses yfinance .info for real-time sector data - fully dynamic

import { invoke } from '@tauri-apps/api/core';

export interface SectorInfo {
  sector: string;
  industry?: string;
  description?: string;
}

// Default sector for unknown/missing values
export const DEFAULT_SECTOR = 'Other';

// Crypto sector constant (for pattern matching)
export const CRYPTO_SECTOR = 'Cryptocurrency';

// Runtime cache for fetched sector data (fully dynamic)
const sectorCache: Record<string, SectorInfo> = {};

// Pending fetch promises to avoid duplicate requests
const pendingFetches: Record<string, Promise<SectorInfo>> = {};

class SectorService {
  /**
   * Get sector information for a symbol (synchronous - uses cache)
   * Returns cached data or pattern-based fallback
   */
  getSectorInfo(symbol: string): SectorInfo {
    const normalizedSymbol = symbol.toUpperCase().trim();

    // Check runtime cache first
    if (sectorCache[normalizedSymbol]) {
      return sectorCache[normalizedSymbol];
    }

    // Check without exchange suffix
    const baseSymbol = normalizedSymbol.split('.')[0];
    if (sectorCache[baseSymbol]) {
      return sectorCache[baseSymbol];
    }

    // Return pattern-based fallback (async fetch will update cache)
    return this.classifyByPattern(normalizedSymbol);
  }

  /**
   * Fetch sector info from yfinance API (async)
   */
  async fetchSectorInfo(symbol: string): Promise<SectorInfo> {
    const normalizedSymbol = symbol.toUpperCase().trim();

    // Check cache
    if (sectorCache[normalizedSymbol]) {
      return sectorCache[normalizedSymbol];
    }

    // Check if already fetching
    if (normalizedSymbol in pendingFetches) {
      return pendingFetches[normalizedSymbol];
    }

    // Pattern-based symbols (crypto, etc.) - no API call needed
    const patternInfo = this.classifyByPattern(normalizedSymbol);
    if (patternInfo.sector !== DEFAULT_SECTOR || patternInfo.industry !== 'Unknown') {
      sectorCache[normalizedSymbol] = patternInfo;
      return patternInfo;
    }

    // Fetch from yfinance API
    const fetchPromise = this.fetchFromYfinance(normalizedSymbol);
    pendingFetches[normalizedSymbol] = fetchPromise;

    try {
      const result = await fetchPromise;
      sectorCache[normalizedSymbol] = result;
      return result;
    } finally {
      delete pendingFetches[normalizedSymbol];
    }
  }

  /**
   * Fetch sector data from yfinance API
   * Returns whatever sector/industry yfinance provides - fully dynamic
   */
  private async fetchFromYfinance(symbol: string): Promise<SectorInfo> {
    try {
      const response = await invoke<string>('execute_yfinance_command', {
        command: 'info',
        args: [symbol]
      });
      const data = JSON.parse(response);

      // Check for error response
      if (data.error) {
        console.warn(`[SectorService] yfinance error for ${symbol}:`, data.error);
        return { sector: DEFAULT_SECTOR, industry: 'Unknown' };
      }

      // Use sector/industry directly from yfinance - no normalization needed
      const sector = data.sector && data.sector !== 'N/A' ? data.sector : DEFAULT_SECTOR;
      const industry = data.industry && data.industry !== 'N/A' ? data.industry : undefined;
      const description = data.description && data.description !== 'N/A' ? data.description : undefined;

      return { sector, industry, description };
    } catch (error) {
      console.warn(`[SectorService] Failed to fetch sector for ${symbol}:`, error);
      return { sector: DEFAULT_SECTOR, industry: 'Unknown' };
    }
  }

  /**
   * Prefetch sector data for multiple symbols
   */
  async prefetchSectors(symbols: string[]): Promise<void> {
    const unknownSymbols = symbols.filter(s => {
      const normalized = s.toUpperCase().trim();
      return !sectorCache[normalized];
    });

    // Fetch in parallel (limit concurrency to avoid overwhelming API)
    const batchSize = 5;
    for (let i = 0; i < unknownSymbols.length; i += batchSize) {
      const batch = unknownSymbols.slice(i, i + batchSize);
      await Promise.all(batch.map(s => this.fetchSectorInfo(s)));
    }
  }

  /**
   * Classify symbol by patterns (fallback for crypto, ETFs, etc.)
   */
  private classifyByPattern(symbol: string): SectorInfo {
    // Crypto patterns
    if (symbol.endsWith('-USD') || symbol.endsWith('USDT') ||
        symbol.includes('BTC') || symbol.includes('ETH') || symbol.includes('USDC')) {
      return { sector: CRYPTO_SECTOR, industry: 'Cryptocurrency' };
    }

    // Default fallback - will be updated by async fetch
    return { sector: DEFAULT_SECTOR, industry: 'Unknown' };
  }

  /**
   * Get all unique sectors from a list of symbols (from cache)
   */
  getUniqueSectors(symbols: string[]): string[] {
    const sectors = new Set<string>();
    symbols.forEach(symbol => {
      const info = this.getSectorInfo(symbol);
      sectors.add(info.sector);
    });
    return Array.from(sectors).sort();
  }

  /**
   * Group symbols by sector (from cache)
   */
  groupBySector(symbols: string[]): Record<string, string[]> {
    const grouped: Record<string, string[]> = {};

    symbols.forEach(symbol => {
      const info = this.getSectorInfo(symbol);
      if (!grouped[info.sector]) {
        grouped[info.sector] = [];
      }
      grouped[info.sector].push(symbol);
    });

    return grouped;
  }

  /**
   * Manually add sector mapping to cache
   */
  addSectorMapping(symbol: string, sectorInfo: SectorInfo): void {
    sectorCache[symbol.toUpperCase()] = sectorInfo;
  }

  /**
   * Get all sectors currently in cache
   */
  getCachedSectors(): string[] {
    const sectors = new Set<string>();
    Object.values(sectorCache).forEach(info => sectors.add(info.sector));
    return Array.from(sectors).sort();
  }

  /**
   * Clear cache (useful for refresh)
   */
  clearCache(): void {
    Object.keys(sectorCache).forEach(key => delete sectorCache[key]);
  }
}

// Export singleton
export const sectorService = new SectorService();
export default sectorService;
