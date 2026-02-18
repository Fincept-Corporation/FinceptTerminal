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

// LocalStorage key for user-defined manual mappings
const MANUAL_MAPPINGS_KEY = 'fincept_sector_manual_mappings';

// Common ETF/fund sector fallbacks (yfinance returns no sector for most ETFs)
const ETF_FALLBACKS: Record<string, SectorInfo> = {
  // US Equity
  'SPY': { sector: 'US Equity', industry: 'S&P 500 ETF' },
  'QQQ': { sector: 'Technology', industry: 'Nasdaq 100 ETF' },
  'IWM': { sector: 'US Equity', industry: 'Small Cap ETF' },
  'VTI': { sector: 'US Equity', industry: 'Total Market ETF' },
  'VOO': { sector: 'US Equity', industry: 'S&P 500 ETF' },
  'DIA': { sector: 'US Equity', industry: 'Dow Jones ETF' },
  // International Equity
  'EFA': { sector: 'International Equity', industry: 'Developed Markets ETF' },
  'EEM': { sector: 'International Equity', industry: 'Emerging Markets ETF' },
  'VGK': { sector: 'International Equity', industry: 'Europe ETF' },
  'EWJ': { sector: 'International Equity', industry: 'Japan ETF' },
  'EWG': { sector: 'International Equity', industry: 'Germany ETF' },
  'MCHI': { sector: 'International Equity', industry: 'China ETF' },
  'KWEB': { sector: 'Technology', industry: 'China Internet ETF' },
  'EWZ': { sector: 'International Equity', industry: 'Brazil ETF' },
  'EWY': { sector: 'International Equity', industry: 'South Korea ETF' },
  // Fixed Income
  'AGG': { sector: 'Fixed Income', industry: 'US Aggregate Bond ETF' },
  'BND': { sector: 'Fixed Income', industry: 'Total Bond ETF' },
  'TLT': { sector: 'Fixed Income', industry: 'Long-Term Treasury ETF' },
  'IEF': { sector: 'Fixed Income', industry: 'Intermediate Treasury ETF' },
  'SHY': { sector: 'Fixed Income', industry: 'Short-Term Treasury ETF' },
  'LQD': { sector: 'Fixed Income', industry: 'Investment Grade Corporate ETF' },
  'HYG': { sector: 'Fixed Income', industry: 'High Yield Bond ETF' },
  'IAGG': { sector: 'Fixed Income', industry: 'International Bond ETF' },
  'CBON': { sector: 'Fixed Income', industry: 'China Bond ETF' },
  'DBJP': { sector: 'Fixed Income', industry: 'Japan Bond ETF' },
  // Commodities
  'GLD': { sector: 'Commodities', industry: 'Gold ETF' },
  'SLV': { sector: 'Commodities', industry: 'Silver ETF' },
  'USO': { sector: 'Commodities', industry: 'Oil ETF' },
  'DJP': { sector: 'Commodities', industry: 'Commodity Index ETF' },
  'PDBC': { sector: 'Commodities', industry: 'Commodity ETF' },
  // Sector ETFs
  'XLK': { sector: 'Technology', industry: 'Technology Sector ETF' },
  'XLF': { sector: 'Financial Services', industry: 'Financial Sector ETF' },
  'XLV': { sector: 'Healthcare', industry: 'Healthcare Sector ETF' },
  'XLE': { sector: 'Energy', industry: 'Energy Sector ETF' },
  'XLI': { sector: 'Industrials', industry: 'Industrial Sector ETF' },
  'XLY': { sector: 'Consumer Cyclical', industry: 'Consumer Discretionary ETF' },
  'XLP': { sector: 'Consumer Defensive', industry: 'Consumer Staples ETF' },
  'XLU': { sector: 'Utilities', industry: 'Utilities Sector ETF' },
  'XLRE': { sector: 'Real Estate', industry: 'Real Estate Sector ETF' },
  'XLB': { sector: 'Basic Materials', industry: 'Materials Sector ETF' },
  'XLC': { sector: 'Communication Services', industry: 'Communication Sector ETF' },
  // REITs
  'VNQ': { sector: 'Real Estate', industry: 'REIT ETF' },
};

// Runtime cache for fetched sector data (fully dynamic)
const sectorCache: Record<string, SectorInfo> = {};

// Pending fetch promises to avoid duplicate requests
const pendingFetches: Record<string, Promise<SectorInfo>> = {};

// Load persisted manual mappings into cache on startup
function loadManualMappings(): void {
  try {
    const raw = localStorage.getItem(MANUAL_MAPPINGS_KEY);
    if (raw) {
      const mappings: Record<string, SectorInfo> = JSON.parse(raw);
      Object.entries(mappings).forEach(([sym, info]) => {
        sectorCache[sym.toUpperCase()] = { ...info, _manual: true } as SectorInfo & { _manual?: boolean };
      });
    }
  } catch {}
}

function saveManualMappings(): void {
  try {
    const manual: Record<string, SectorInfo> = {};
    Object.entries(sectorCache).forEach(([sym, info]) => {
      if ((info as any)._manual) manual[sym] = info;
    });
    localStorage.setItem(MANUAL_MAPPINGS_KEY, JSON.stringify(manual));
  } catch {}
}

// Init: load ETF fallbacks then manual overrides
Object.entries(ETF_FALLBACKS).forEach(([sym, info]) => {
  sectorCache[sym] = info;
});
loadManualMappings(); // manual overrides ETF fallbacks

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
   * Manually add sector mapping to cache and persist it
   */
  addSectorMapping(symbol: string, sectorInfo: SectorInfo): void {
    const key = symbol.toUpperCase();
    sectorCache[key] = { ...sectorInfo, _manual: true } as any;
    saveManualMappings();
  }

  /**
   * Remove a manual mapping
   */
  removeManualMapping(symbol: string): void {
    const key = symbol.toUpperCase();
    // Restore ETF fallback if exists, otherwise delete
    if (ETF_FALLBACKS[key]) {
      sectorCache[key] = ETF_FALLBACKS[key];
    } else {
      delete sectorCache[key];
    }
    saveManualMappings();
  }

  /**
   * Get all manual mappings (user-defined)
   */
  getManualMappings(): Record<string, SectorInfo> {
    const manual: Record<string, SectorInfo> = {};
    Object.entries(sectorCache).forEach(([sym, info]) => {
      if ((info as any)._manual) manual[sym] = info;
    });
    return manual;
  }

  /**
   * Get full cache snapshot (for display in editor)
   */
  getAllMappings(): Record<string, SectorInfo> {
    return { ...sectorCache };
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
    // Re-init ETF fallbacks and manual mappings
    Object.entries(ETF_FALLBACKS).forEach(([sym, info]) => { sectorCache[sym] = info; });
    loadManualMappings();
  }
}

// Export singleton
export const sectorService = new SectorService();
export default sectorService;
