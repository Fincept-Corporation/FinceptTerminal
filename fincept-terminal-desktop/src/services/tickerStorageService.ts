// Ticker Storage Service - localStorage-based user preferences
// Stores user's custom ticker lists, panel arrangements, and watchlists

export interface TickerList {
  category: string;
  tickers: string[];
}

export interface RegionalTickerList {
  region: string;
  tickers: Array<{ symbol: string; name: string }>;
}

export interface UserMarketPreferences {
  globalMarkets: TickerList[];
  regionalMarkets: RegionalTickerList[];
  lastUpdated: number;
}

const STORAGE_KEY = 'fincept_market_preferences';

// Default tickers matching original static data
const DEFAULT_PREFERENCES: UserMarketPreferences = {
  globalMarkets: [
    {
      category: 'Stock Indices',
      tickers: ['^GSPC', '^IXIC', '^DJI', '^RUT', '^VIX', '^FTSE', '^GDAXI', '^N225', '^FCHI', '^HSI', '^AXJO', '^BSESN']
    },
    {
      category: 'Forex',
      tickers: ['EURUSD=X', 'GBPUSD=X', 'JPY=X', 'CHF=X', 'CAD=X', 'AUDUSD=X', 'NZDUSD=X', 'EURGBP=X', 'EURJPY=X', 'GBPJPY=X', 'USDCNY=X', 'USDINR=X']
    },
    {
      category: 'Commodities',
      tickers: ['GC=F', 'SI=F', 'CL=F', 'BZ=F', 'NG=F', 'HG=F', 'PL=F', 'PA=F', 'ZW=F', 'ZC=F', 'ZS=F', 'KC=F']
    },
    {
      category: 'Bonds',
      tickers: ['^TNX', '^TYX', '^IRX', 'DE10Y=X', 'GB10Y=X', 'JP10Y=X', 'FR10Y=X', 'IT10Y=X', 'ES10Y=X', 'CA10Y=X', 'AU10Y=X', 'IN10Y=X']
    },
    {
      category: 'ETFs',
      tickers: ['SPY', 'QQQ', 'DIA', 'EEM', 'GLD', 'XLK', 'XLE', 'XLF', 'XLV', 'VNQ', 'IWM', 'VTI']
    },
    {
      category: 'Cryptocurrencies',
      tickers: ['BTC-USD', 'ETH-USD', 'BNB-USD', 'SOL-USD', 'XRP-USD', 'ADA-USD', 'DOGE-USD', 'MATIC-USD', 'LINK-USD', 'DOT-USD', 'AVAX-USD', 'UNI-USD']
    }
  ],
  regionalMarkets: [
    {
      region: 'India',
      tickers: [
        { symbol: 'RELIANCE.NS', name: 'Reliance Industries' },
        { symbol: 'TCS.NS', name: 'Tata Consultancy' },
        { symbol: 'HDFCBANK.NS', name: 'HDFC Bank' },
        { symbol: 'INFY.NS', name: 'Infosys' },
        { symbol: 'HINDUNILVR.NS', name: 'Hindustan Unilever' },
        { symbol: 'ICICIBANK.NS', name: 'ICICI Bank' },
        { symbol: 'SBIN.NS', name: 'State Bank of India' },
        { symbol: 'BHARTIARTL.NS', name: 'Bharti Airtel' },
        { symbol: 'ITC.NS', name: 'ITC Limited' },
        { symbol: 'KOTAKBANK.NS', name: 'Kotak Mahindra Bank' },
        { symbol: 'LT.NS', name: 'Larsen & Toubro' },
        { symbol: 'WIPRO.NS', name: 'Wipro Limited' }
      ]
    },
    {
      region: 'China',
      tickers: [
        { symbol: 'BABA', name: 'Alibaba Group' },
        { symbol: 'PDD', name: 'PDD Holdings' },
        { symbol: 'JD', name: 'JD.com' },
        { symbol: 'BIDU', name: 'Baidu' },
        { symbol: 'TCEHY', name: 'Tencent Holdings' },
        { symbol: 'NIO', name: 'NIO Inc' },
        { symbol: 'LI', name: 'Li Auto' },
        { symbol: 'XPEV', name: 'XPeng' },
        { symbol: 'BILI', name: 'Bilibili' },
        { symbol: 'NTES', name: 'NetEase' },
        { symbol: 'TME', name: 'Tencent Music' },
        { symbol: 'BEKE', name: 'KE Holdings' }
      ]
    },
    {
      region: 'United States',
      tickers: [
        { symbol: 'AAPL', name: 'Apple Inc' },
        { symbol: 'MSFT', name: 'Microsoft Corp' },
        { symbol: 'GOOGL', name: 'Alphabet Inc' },
        { symbol: 'AMZN', name: 'Amazon.com' },
        { symbol: 'NVDA', name: 'NVIDIA Corp' },
        { symbol: 'META', name: 'Meta Platforms' },
        { symbol: 'TSLA', name: 'Tesla Inc' },
        { symbol: 'JPM', name: 'JPMorgan Chase' },
        { symbol: 'V', name: 'Visa Inc' },
        { symbol: 'WMT', name: 'Walmart Inc' },
        { symbol: 'UNH', name: 'UnitedHealth Group' },
        { symbol: 'MA', name: 'Mastercard Inc' }
      ]
    }
  ],
  lastUpdated: Date.now()
};

class TickerStorageService {
  /**
   * Load user preferences from localStorage
   */
  loadPreferences(): UserMarketPreferences {
    try {
      const stored = localStorage.getItem(STORAGE_KEY);
      if (stored) {
        const parsed = JSON.parse(stored);
        return parsed;
      }
    } catch (error) {
      console.error('[TickerStorage] Failed to load preferences:', error);
    }

    // Return defaults if nothing stored or error
    return DEFAULT_PREFERENCES;
  }

  /**
   * Save user preferences to localStorage
   */
  savePreferences(preferences: UserMarketPreferences): boolean {
    try {
      preferences.lastUpdated = Date.now();
      localStorage.setItem(STORAGE_KEY, JSON.stringify(preferences));
      return true;
    } catch (error) {
      console.error('[TickerStorage] Failed to save preferences:', error);
      return false;
    }
  }

  /**
   * Get tickers for a specific category
   */
  getCategoryTickers(category: string): string[] {
    const prefs = this.loadPreferences();
    const found = prefs.globalMarkets.find(m => m.category === category);
    return found?.tickers || [];
  }

  /**
   * Update tickers for a specific category
   */
  updateCategoryTickers(category: string, tickers: string[]): boolean {
    const prefs = this.loadPreferences();
    const index = prefs.globalMarkets.findIndex(m => m.category === category);

    if (index !== -1) {
      prefs.globalMarkets[index].tickers = tickers;
      return this.savePreferences(prefs);
    }

    return false;
  }

  /**
   * Add ticker to category
   */
  addTickerToCategory(category: string, ticker: string): boolean {
    const tickers = this.getCategoryTickers(category);
    if (!tickers.includes(ticker)) {
      tickers.push(ticker);
      return this.updateCategoryTickers(category, tickers);
    }
    return false;
  }

  /**
   * Remove ticker from category
   */
  removeTickerFromCategory(category: string, ticker: string): boolean {
    const tickers = this.getCategoryTickers(category);
    const filtered = tickers.filter(t => t !== ticker);
    if (filtered.length !== tickers.length) {
      return this.updateCategoryTickers(category, filtered);
    }
    return false;
  }

  /**
   * Reorder tickers in category
   */
  reorderCategoryTickers(category: string, fromIndex: number, toIndex: number): boolean {
    const tickers = this.getCategoryTickers(category);
    const [removed] = tickers.splice(fromIndex, 1);
    tickers.splice(toIndex, 0, removed);
    return this.updateCategoryTickers(category, tickers);
  }

  /**
   * Get regional market tickers
   */
  getRegionalTickers(region: string): Array<{ symbol: string; name: string }> {
    const prefs = this.loadPreferences();
    const found = prefs.regionalMarkets.find(m => m.region === region);
    return found?.tickers || [];
  }

  /**
   * Update regional market tickers
   */
  updateRegionalTickers(region: string, tickers: Array<{ symbol: string; name: string }>): boolean {
    const prefs = this.loadPreferences();
    const index = prefs.regionalMarkets.findIndex(m => m.region === region);

    if (index !== -1) {
      prefs.regionalMarkets[index].tickers = tickers;
      return this.savePreferences(prefs);
    }

    return false;
  }

  /**
   * Reset to defaults
   */
  resetToDefaults(): boolean {
    return this.savePreferences(DEFAULT_PREFERENCES);
  }

  /**
   * Export preferences as JSON
   */
  exportPreferences(): string {
    const prefs = this.loadPreferences();
    return JSON.stringify(prefs, null, 2);
  }

  /**
   * Import preferences from JSON
   */
  importPreferences(json: string): boolean {
    try {
      const parsed = JSON.parse(json);
      return this.savePreferences(parsed);
    } catch (error) {
      console.error('[TickerStorage] Failed to import preferences:', error);
      return false;
    }
  }
}

export const tickerStorage = new TickerStorageService();
export default tickerStorage;
