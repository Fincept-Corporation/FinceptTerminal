/**
 * Symbol Master Service
 * Fetches and caches NSE symbol data from Fyers
 */

import { BaseDirectory, mkdir, readTextFile, writeTextFile } from '@tauri-apps/plugin-fs';

interface SymbolData {
  fyToken: string;
  exSymbol: string;
  exSymName: string;
  exchange: string;
  isin: string;
  symbolDesc: string;
  symTicker: string;
  short_name: string;
  display_format_mob: string;
}

interface SymbolMasterData {
  [key: string]: SymbolData;
}

class SymbolMasterService {
  private symbols: SymbolMasterData = {};
  private symbolsList: Array<{ ticker: string; name: string; shortName: string }> = [];
  private isLoaded = false;
  private isLoading = false;
  private cacheFileName = 'nse_symbols_master.json';
  private cacheDir = 'fincept-terminal';
  private cacheMaxAge = 24 * 60 * 60 * 1000; // 24 hours

  /**
   * Initialize and load symbols (from cache or API)
   */
  async initialize(): Promise<void> {
    if (this.isLoaded || this.isLoading) return;

    this.isLoading = true;
    try {
      // Try to load from cache first
      const cacheLoaded = await this.loadFromCache();

      if (!cacheLoaded) {
        // If cache is not available or stale, fetch from API
        await this.fetchAndCache();
      }

      this.isLoaded = true;
    } catch (error) {
      console.error('[SymbolMasterService] Initialization error:', error);
    } finally {
      this.isLoading = false;
    }
  }

  /**
   * Load symbols from local cache
   */
  private async loadFromCache(): Promise<boolean> {
    try {
      // Ensure cache directory exists
      await this.ensureCacheDir();

      // Try to read cache file directly (will throw if doesn't exist)
      const cacheContent = await readTextFile(this.cacheFileName, { baseDir: BaseDirectory.AppData });
      const cacheData = JSON.parse(cacheContent);

      // Check cache age
      const cacheAge = Date.now() - cacheData.timestamp;
      if (cacheAge > this.cacheMaxAge) {
        console.log('[SymbolMasterService] Cache is stale, will refresh');
        return false;
      }

      // Load symbols from cache
      this.symbols = cacheData.symbols;
      this.buildSymbolsList();

      console.log(`[SymbolMasterService] ✅ Loaded ${this.symbolsList.length} symbols from cache`);
      return true;
    } catch (error) {
      // File doesn't exist or read error - will fetch fresh data
      console.log('[SymbolMasterService] Cache not available, will fetch fresh data');
      return false;
    }
  }

  /**
   * Fetch symbols from Fyers API and cache locally
   */
  private async fetchAndCache(): Promise<void> {
    try {
      console.log('[SymbolMasterService] Fetching symbols from Fyers API...');

      const response = await fetch('https://public.fyers.in/sym_details/NSE_CM_sym_master.json');

      if (!response.ok) {
        throw new Error(`Failed to fetch symbols: ${response.status}`);
      }

      const data: SymbolMasterData = await response.json();
      this.symbols = data;
      this.buildSymbolsList();

      // Cache the data
      await this.saveToCache();

      console.log(`[SymbolMasterService] ✅ Fetched and cached ${this.symbolsList.length} symbols`);
    } catch (error) {
      console.error('[SymbolMasterService] Error fetching symbols:', error);
      throw error;
    }
  }

  /**
   * Save symbols to cache
   */
  private async saveToCache(): Promise<void> {
    try {
      await this.ensureCacheDir();

      const cacheData = {
        timestamp: Date.now(),
        symbols: this.symbols,
      };

      await writeTextFile(
        this.cacheFileName,
        JSON.stringify(cacheData),
        { baseDir: BaseDirectory.AppData }
      );

      console.log('[SymbolMasterService] ✅ Saved symbols to cache');
    } catch (error) {
      console.error('[SymbolMasterService] Error saving to cache:', error);
    }
  }

  /**
   * Ensure cache directory exists
   */
  private async ensureCacheDir(): Promise<void> {
    try {
      // Just try to create the directory - recursive: true will not fail if it exists
      await mkdir(this.cacheDir, { baseDir: BaseDirectory.AppData, recursive: true });
    } catch (error) {
      // Ignore error if directory already exists
      console.log('[SymbolMasterService] Cache directory ready');
    }
  }

  /**
   * Build searchable symbols list
   */
  private buildSymbolsList(): void {
    this.symbolsList = [];

    // Only include equity stocks (EQ series)
    for (const [ticker, data] of Object.entries(this.symbols)) {
      if (ticker.includes('-EQ')) {
        this.symbolsList.push({
          ticker: data.exSymbol,
          name: data.symbolDesc || data.exSymName,
          shortName: data.short_name || data.exSymbol,
        });
      }
    }

    // Sort alphabetically
    this.symbolsList.sort((a, b) => a.ticker.localeCompare(b.ticker));
  }

  /**
   * Search symbols by query
   */
  searchSymbols(query: string, limit: number = 10): Array<{ ticker: string; name: string; shortName: string }> {
    if (!query || query.length < 1) return [];

    const searchTerm = query.toUpperCase();
    const results: Array<{ ticker: string; name: string; shortName: string; score: number }> = [];

    for (const symbol of this.symbolsList) {
      let score = 0;

      // Exact match at start of ticker (highest priority)
      if (symbol.ticker.startsWith(searchTerm)) {
        score = 1000;
      }
      // Contains in ticker
      else if (symbol.ticker.includes(searchTerm)) {
        score = 500;
      }
      // Contains in name
      else if (symbol.name.toUpperCase().includes(searchTerm)) {
        score = 100;
      }
      // Contains in short name
      else if (symbol.shortName.toUpperCase().includes(searchTerm)) {
        score = 50;
      }

      if (score > 0) {
        results.push({ ...symbol, score });
      }
    }

    // Sort by score (descending) and return top results
    return results
      .sort((a, b) => b.score - a.score)
      .slice(0, limit)
      .map(({ ticker, name, shortName }) => ({ ticker, name, shortName }));
  }

  /**
   * Get symbol details by ticker
   */
  getSymbolDetails(ticker: string): SymbolData | null {
    const key = `NSE:${ticker}-EQ`;
    return this.symbols[key] || null;
  }

  /**
   * Force refresh symbols from API
   */
  async forceRefresh(): Promise<void> {
    this.isLoaded = false;
    await this.fetchAndCache();
    this.isLoaded = true;
  }

  /**
   * Check if service is ready
   */
  isReady(): boolean {
    return this.isLoaded;
  }

  /**
   * Get total symbols count
   */
  getSymbolsCount(): number {
    return this.symbolsList.length;
  }
}

// Export singleton instance
export const symbolMasterService = new SymbolMasterService();
