// Watchlist Service - Manages watchlists and stock tracking for Fincept Terminal
// Handles watchlist CRUD, stock management, and live market data integration

import Database from '@tauri-apps/plugin-sql';
import { marketDataService, QuoteData } from './marketDataService';

// ==================== TYPES ====================

export interface Watchlist {
  id: string;
  name: string;
  description?: string;
  color: string;
  created_at: string;
  updated_at: string;
}

export interface WatchlistStock {
  id: string;
  watchlist_id: string;
  symbol: string;
  added_at: string;
  notes?: string;
}

export interface WatchlistStockWithQuote extends WatchlistStock {
  quote: QuoteData | null;
}

export interface WatchlistWithStocks {
  watchlist: Watchlist;
  stocks: WatchlistStockWithQuote[];
  stock_count: number;
}

// ==================== WATCHLIST SERVICE CLASS ====================

class WatchlistService {
  private db: Database | null = null;
  private dbPath = 'sqlite:fincept_terminal.db';
  private isInitialized = false;

  /**
   * Initialize database connection
   */
  async initialize(): Promise<void> {
    if (this.isInitialized && this.db) {
      return;
    }

    try {
      this.db = await Database.load(this.dbPath);
      this.isInitialized = true;
      console.log('[WatchlistService] Initialized successfully');
    } catch (error) {
      console.error('[WatchlistService] Initialization failed:', error);
      throw error;
    }
  }

  /**
   * Ensure database is initialized
   */
  private ensureInitialized(): void {
    if (!this.db || !this.isInitialized) {
      throw new Error('WatchlistService not initialized. Call initialize() first.');
    }
  }

  // ==================== WATCHLIST CRUD ====================

  /**
   * Create a new watchlist
   */
  async createWatchlist(name: string, description?: string, color: string = '#FFA500'): Promise<Watchlist> {
    this.ensureInitialized();

    const id = crypto.randomUUID();
    await this.db!.execute(
      `INSERT INTO watchlists (id, name, description, color)
       VALUES ($1, $2, $3, $4)`,
      [id, name, description || null, color]
    );

    const result = await this.db!.select<Watchlist[]>(
      'SELECT * FROM watchlists WHERE id = $1',
      [id]
    );

    console.log(`[WatchlistService] Created watchlist: ${name} (ID: ${id})`);
    return result[0];
  }

  /**
   * Get all watchlists
   */
  async getWatchlists(): Promise<Watchlist[]> {
    this.ensureInitialized();

    const watchlists = await this.db!.select<Watchlist[]>(
      'SELECT * FROM watchlists ORDER BY created_at DESC'
    );

    return watchlists;
  }

  /**
   * Get watchlist by ID
   */
  async getWatchlist(watchlistId: string): Promise<Watchlist | null> {
    this.ensureInitialized();

    const result = await this.db!.select<Watchlist[]>(
      'SELECT * FROM watchlists WHERE id = $1',
      [watchlistId]
    );

    return result.length > 0 ? result[0] : null;
  }

  /**
   * Update watchlist
   */
  async updateWatchlist(watchlistId: string, updates: Partial<Omit<Watchlist, 'id' | 'created_at' | 'updated_at'>>): Promise<void> {
    this.ensureInitialized();

    const sets: string[] = [];
    const values: any[] = [];

    if (updates.name !== undefined) {
      sets.push('name = $' + (values.length + 1));
      values.push(updates.name);
    }
    if (updates.description !== undefined) {
      sets.push('description = $' + (values.length + 1));
      values.push(updates.description);
    }
    if (updates.color !== undefined) {
      sets.push('color = $' + (values.length + 1));
      values.push(updates.color);
    }

    if (sets.length > 0) {
      sets.push("updated_at = datetime('now')");
      values.push(watchlistId);

      await this.db!.execute(
        `UPDATE watchlists SET ${sets.join(', ')} WHERE id = $${values.length}`,
        values
      );
    }
  }

  /**
   * Delete watchlist (cascades to stocks)
   */
  async deleteWatchlist(watchlistId: string): Promise<void> {
    this.ensureInitialized();

    await this.db!.execute('DELETE FROM watchlists WHERE id = $1', [watchlistId]);
    console.log(`[WatchlistService] Deleted watchlist: ${watchlistId}`);
  }

  // ==================== STOCK MANAGEMENT ====================

  /**
   * Add stock to watchlist
   */
  async addStock(watchlistId: string, symbol: string, notes?: string): Promise<WatchlistStock> {
    this.ensureInitialized();

    // Check if stock already exists in watchlist
    const existing = await this.db!.select<WatchlistStock[]>(
      'SELECT * FROM watchlist_stocks WHERE watchlist_id = $1 AND symbol = $2',
      [watchlistId, symbol]
    );

    if (existing.length > 0) {
      throw new Error(`Stock ${symbol} already exists in watchlist`);
    }

    const id = crypto.randomUUID();
    await this.db!.execute(
      `INSERT INTO watchlist_stocks (id, watchlist_id, symbol, notes)
       VALUES ($1, $2, $3, $4)`,
      [id, watchlistId, symbol.toUpperCase(), notes || null]
    );

    const result = await this.db!.select<WatchlistStock[]>(
      'SELECT * FROM watchlist_stocks WHERE id = $1',
      [id]
    );

    console.log(`[WatchlistService] Added stock ${symbol} to watchlist ${watchlistId}`);
    return result[0];
  }

  /**
   * Remove stock from watchlist
   */
  async removeStock(watchlistId: string, symbol: string): Promise<void> {
    this.ensureInitialized();

    await this.db!.execute(
      'DELETE FROM watchlist_stocks WHERE watchlist_id = $1 AND symbol = $2',
      [watchlistId, symbol.toUpperCase()]
    );

    console.log(`[WatchlistService] Removed stock ${symbol} from watchlist ${watchlistId}`);
  }

  /**
   * Update stock notes
   */
  async updateStockNotes(watchlistId: string, symbol: string, notes: string): Promise<void> {
    this.ensureInitialized();

    await this.db!.execute(
      'UPDATE watchlist_stocks SET notes = $1 WHERE watchlist_id = $2 AND symbol = $3',
      [notes, watchlistId, symbol.toUpperCase()]
    );
  }

  /**
   * Get all stocks in a watchlist
   */
  async getWatchlistStocks(watchlistId: string): Promise<WatchlistStock[]> {
    this.ensureInitialized();

    return await this.db!.select<WatchlistStock[]>(
      'SELECT * FROM watchlist_stocks WHERE watchlist_id = $1 ORDER BY added_at DESC',
      [watchlistId]
    );
  }

  /**
   * Get watchlist stocks with live market quotes (with 10-min cache)
   */
  async getWatchlistStocksWithQuotes(watchlistId: string): Promise<WatchlistStockWithQuote[]> {
    this.ensureInitialized();

    const stocks = await this.getWatchlistStocks(watchlistId);

    if (stocks.length === 0) {
      return [];
    }

    // Fetch quotes for all symbols using cached method (10-minute cache)
    const symbols = stocks.map(s => s.symbol);
    const quotes = await marketDataService.getEnhancedQuotesWithCache(
      symbols,
      `watchlist_${watchlistId}`,
      10 // 10-minute cache
    );

    // Map quotes to stocks
    const quoteMap = new Map<string, QuoteData>();
    quotes.forEach(quote => {
      quoteMap.set(quote.symbol, quote);
    });

    return stocks.map(stock => ({
      ...stock,
      quote: quoteMap.get(stock.symbol) || null
    }));
  }

  /**
   * Get complete watchlist with stocks and quotes
   */
  async getWatchlistWithStocks(watchlistId: string): Promise<WatchlistWithStocks | null> {
    this.ensureInitialized();

    const watchlist = await this.getWatchlist(watchlistId);
    if (!watchlist) {
      return null;
    }

    const stocks = await this.getWatchlistStocksWithQuotes(watchlistId);

    return {
      watchlist,
      stocks,
      stock_count: stocks.length
    };
  }

  /**
   * Get all watchlists with stock counts
   */
  async getWatchlistsWithCounts(): Promise<Array<Watchlist & { stock_count: number }>> {
    this.ensureInitialized();

    const watchlists = await this.getWatchlists();

    const withCounts = await Promise.all(
      watchlists.map(async (watchlist) => {
        const stocks = await this.getWatchlistStocks(watchlist.id);
        return {
          ...watchlist,
          stock_count: stocks.length
        };
      })
    );

    return withCounts;
  }

  // ==================== MARKET DATA HELPERS ====================

  /**
   * Get market movers (top gainers and losers) from all watchlists (with 10-min cache)
   */
  async getMarketMovers(limit: number = 5): Promise<{
    gainers: WatchlistStockWithQuote[];
    losers: WatchlistStockWithQuote[];
  }> {
    this.ensureInitialized();

    // Get all unique symbols from all watchlists
    const allStocks = await this.db!.select<{ symbol: string }[]>(
      'SELECT DISTINCT symbol FROM watchlist_stocks'
    );

    if (allStocks.length === 0) {
      return { gainers: [], losers: [] };
    }

    const symbols = allStocks.map(s => s.symbol);
    const quotes = await marketDataService.getEnhancedQuotesWithCache(
      symbols,
      'market_movers',
      10 // 10-minute cache
    );

    // Sort by change percent
    const sortedQuotes = quotes.sort((a, b) => b.change_percent - a.change_percent);

    const gainers = sortedQuotes.slice(0, limit).map(quote => ({
      id: '',
      watchlist_id: '',
      symbol: quote.symbol,
      added_at: '',
      quote
    }));

    const losers = sortedQuotes.slice(-limit).reverse().map(quote => ({
      id: '',
      watchlist_id: '',
      symbol: quote.symbol,
      added_at: '',
      quote
    }));

    return { gainers, losers };
  }

  /**
   * Get volume leaders from all watchlists (with 10-min cache)
   */
  async getVolumeLeaders(limit: number = 5): Promise<WatchlistStockWithQuote[]> {
    this.ensureInitialized();

    const allStocks = await this.db!.select<{ symbol: string }[]>(
      'SELECT DISTINCT symbol FROM watchlist_stocks'
    );

    if (allStocks.length === 0) {
      return [];
    }

    const symbols = allStocks.map(s => s.symbol);
    const quotes = await marketDataService.getEnhancedQuotesWithCache(
      symbols,
      'volume_leaders',
      10 // 10-minute cache
    );

    // Sort by volume
    const sortedQuotes = quotes
      .filter(q => q.volume !== undefined && q.volume !== null)
      .sort((a, b) => (b.volume || 0) - (a.volume || 0));

    return sortedQuotes.slice(0, limit).map(quote => ({
      id: '',
      watchlist_id: '',
      symbol: quote.symbol,
      added_at: '',
      quote
    }));
  }

  // ==================== EXPORT ====================

  /**
   * Export watchlist as CSV
   */
  async exportWatchlistCSV(watchlistId: string): Promise<string> {
    const data = await this.getWatchlistWithStocks(watchlistId);

    if (!data) {
      throw new Error('Watchlist not found');
    }

    let csv = 'Symbol,Price,Change,Change %,Volume,High,Low,Open,Prev Close,Notes\n';

    data.stocks.forEach(stock => {
      const q = stock.quote;
      if (q) {
        csv += `${stock.symbol},${q.price.toFixed(2)},${q.change.toFixed(2)},${q.change_percent.toFixed(2)},${q.volume || 'N/A'},${q.high?.toFixed(2) || 'N/A'},${q.low?.toFixed(2) || 'N/A'},${q.open?.toFixed(2) || 'N/A'},${q.previous_close?.toFixed(2) || 'N/A'},"${stock.notes || ''}"\n`;
      } else {
        csv += `${stock.symbol},N/A,N/A,N/A,N/A,N/A,N/A,N/A,N/A,"${stock.notes || ''}"\n`;
      }
    });

    return csv;
  }

  // ==================== UTILITY ====================

  /**
   * Check if service is ready
   */
  isReady(): boolean {
    return this.isInitialized && this.db !== null;
  }

  /**
   * Get service status
   */
  getStatus(): { initialized: boolean; ready: boolean } {
    return {
      initialized: this.isInitialized,
      ready: this.isReady()
    };
  }
}

// Export singleton instance
export const watchlistService = new WatchlistService();
export default watchlistService;
