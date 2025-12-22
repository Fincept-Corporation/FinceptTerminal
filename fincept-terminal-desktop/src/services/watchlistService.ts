// Watchlist Service - Manages watchlists and stock tracking for Fincept Terminal
// Now using high-performance Rust SQLite backend via Tauri commands

import { invoke } from '@tauri-apps/api/core';
import { marketDataService, QuoteData } from './marketDataService';
import { watchlistLogger } from './loggerService';

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
  // No database initialization needed - Rust backend handles it

  // ==================== WATCHLIST CRUD ====================

  /**
   * Create a new watchlist
   */
  async createWatchlist(name: string, description?: string, color: string = '#FFA500'): Promise<Watchlist> {
    try {
      const watchlist = await invoke<Watchlist>('db_create_watchlist', {
        name,
        description: description || null,
        color
      });
      watchlistLogger.info(`Created watchlist: ${name}`, { id: watchlist.id });
      return watchlist;
    } catch (error) {
      watchlistLogger.error('Failed to create watchlist:', error);
      throw error;
    }
  }

  /**
   * Get all watchlists
   */
  async getWatchlists(): Promise<Watchlist[]> {
    try {
      return await invoke<Watchlist[]>('db_get_watchlists');
    } catch (error) {
      watchlistLogger.error('Failed to get watchlists:', error);
      throw error;
    }
  }

  /**
   * Get watchlist by ID
   */
  async getWatchlist(watchlistId: string): Promise<Watchlist | null> {
    try {
      const watchlists = await this.getWatchlists();
      return watchlists.find(w => w.id === watchlistId) || null;
    } catch (error) {
      watchlistLogger.error('Failed to get watchlist:', error);
      return null;
    }
  }

  /**
   * Delete watchlist (cascades to stocks)
   */
  async deleteWatchlist(watchlistId: string): Promise<void> {
    try {
      await invoke('db_delete_watchlist', { watchlistId });
      watchlistLogger.info(`Deleted watchlist: ${watchlistId}`);
    } catch (error) {
      watchlistLogger.error('Failed to delete watchlist:', error);
      throw error;
    }
  }

  // ==================== STOCK MANAGEMENT ====================

  /**
   * Add stock to watchlist
   */
  async addStock(watchlistId: string, symbol: string, notes?: string): Promise<WatchlistStock> {
    try {
      const stock = await invoke<WatchlistStock>('db_add_watchlist_stock', {
        watchlistId,
        symbol: symbol.toUpperCase(),
        notes: notes || null
      });
      watchlistLogger.info(`Added stock ${symbol} to watchlist`, { watchlistId });
      return stock;
    } catch (error) {
      watchlistLogger.error('Failed to add stock:', error);
      throw error;
    }
  }

  /**
   * Remove stock from watchlist
   */
  async removeStock(watchlistId: string, symbol: string): Promise<void> {
    try {
      await invoke('db_remove_watchlist_stock', {
        watchlistId,
        symbol: symbol.toUpperCase()
      });
      watchlistLogger.info(`Removed stock ${symbol} from watchlist`, { watchlistId });
    } catch (error) {
      watchlistLogger.error('Failed to remove stock:', error);
      throw error;
    }
  }

  /**
   * Get all stocks in a watchlist
   */
  async getWatchlistStocks(watchlistId: string): Promise<WatchlistStock[]> {
    try {
      return await invoke<WatchlistStock[]>('db_get_watchlist_stocks', { watchlistId });
    } catch (error) {
      watchlistLogger.error('Failed to get watchlist stocks:', error);
      throw error;
    }
  }

  /**
   * Get watchlist stocks with live market quotes (with 10-min cache)
   */
  async getWatchlistStocksWithQuotes(watchlistId: string): Promise<WatchlistStockWithQuote[]> {
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
    // Get all watchlists and their stocks
    const watchlists = await this.getWatchlists();
    const allStocksArrays = await Promise.all(
      watchlists.map(w => this.getWatchlistStocks(w.id))
    );

    const allStocks = allStocksArrays.flat();
    const uniqueSymbols = [...new Set(allStocks.map(s => s.symbol))];

    if (uniqueSymbols.length === 0) {
      return { gainers: [], losers: [] };
    }

    const quotes = await marketDataService.getEnhancedQuotesWithCache(
      uniqueSymbols,
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
    // Get all watchlists and their stocks
    const watchlists = await this.getWatchlists();
    const allStocksArrays = await Promise.all(
      watchlists.map(w => this.getWatchlistStocks(w.id))
    );

    const allStocks = allStocksArrays.flat();
    const uniqueSymbols = [...new Set(allStocks.map(s => s.symbol))];

    if (uniqueSymbols.length === 0) {
      return [];
    }

    const quotes = await marketDataService.getEnhancedQuotesWithCache(
      uniqueSymbols,
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
   * Check if service is ready (always ready with Rust backend)
   */
  isReady(): boolean {
    return true;
  }

  /**
   * Get service status
   */
  getStatus(): { initialized: boolean; ready: boolean } {
    return {
      initialized: true,
      ready: true
    };
  }

  /**
   * Initialize - kept for compatibility but not needed with Rust backend
   */
  async initialize(): Promise<void> {
    // Rust backend auto-initializes on app startup
    watchlistLogger.info('Using Rust SQLite backend');
  }
}

// Export singleton instance
export const watchlistService = new WatchlistService();
export default watchlistService;
