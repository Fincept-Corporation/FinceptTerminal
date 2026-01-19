// Ticker Storage Service - user preferences storage
// Stores user's custom ticker lists, panel arrangements, and watchlists

import { saveSetting, getSetting } from './sqliteService';

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

// Default tickers - verified working Yahoo Finance symbols
// Note: Some symbols like international bonds (DE10Y=X, etc) may not work on Yahoo Finance
const DEFAULT_PREFERENCES: UserMarketPreferences = {
  globalMarkets: [
    {
      category: 'Stock Indices',
      // Major global indices - expanded list
      tickers: [
        '^GSPC',    // S&P 500
        '^IXIC',    // NASDAQ Composite
        '^DJI',     // Dow Jones Industrial
        '^RUT',     // Russell 2000
        '^VIX',     // CBOE Volatility Index
        '^FTSE',    // FTSE 100 (UK)
        '^GDAXI',   // DAX (Germany)
        '^N225',    // Nikkei 225 (Japan)
        '^FCHI',    // CAC 40 (France)
        '^HSI',     // Hang Seng (Hong Kong)
        '^AXJO',    // ASX 200 (Australia)
        '^BSESN',   // BSE Sensex (India)
        '^NSEI',    // Nifty 50 (India)
        '^STOXX50E', // Euro Stoxx 50
        '^NYA',     // NYSE Composite
        '^IXID',    // NASDAQ Industrial
        '^SOX',     // PHLX Semiconductor
        '^BUK100P', // Cboe UK 100
        '^IBEX',    // IBEX 35 (Spain)
        '^AEX'      // AEX (Netherlands)
      ]
    },
    {
      category: 'Forex',
      // Using proper Yahoo Finance forex format: XXXYYY=X
      tickers: ['EURUSD=X', 'GBPUSD=X', 'USDJPY=X', 'USDCHF=X', 'USDCAD=X', 'AUDUSD=X', 'NZDUSD=X', 'EURGBP=X', 'EURJPY=X', 'GBPJPY=X', 'USDCNY=X', 'USDINR=X']
    },
    {
      category: 'Commodities',
      // Futures symbols - expanded list with metals, energy, agriculture
      tickers: [
        'GC=F',     // Gold
        'SI=F',     // Silver
        'PL=F',     // Platinum
        'PA=F',     // Palladium
        'HG=F',     // Copper
        'CL=F',     // Crude Oil WTI
        'BZ=F',     // Brent Crude Oil
        'NG=F',     // Natural Gas
        'RB=F',     // RBOB Gasoline
        'HO=F',     // Heating Oil
        'ZC=F',     // Corn
        'ZW=F',     // Wheat
        'ZS=F',     // Soybeans
        'ZM=F',     // Soybean Meal
        'ZL=F',     // Soybean Oil
        'KC=F',     // Coffee
        'CT=F',     // Cotton
        'SB=F',     // Sugar
        'CC=F',     // Cocoa
        'LBS=F'     // Lumber
      ]
    },
    {
      category: 'Bonds',
      // US Treasury yields work reliably, international bonds removed (not supported)
      tickers: ['^TNX', '^TYX', '^IRX', '^FVX', 'TLT', 'IEF', 'SHY', 'BND', 'AGG', 'LQD', 'HYG', 'JNK']
    },
    {
      category: 'ETFs',
      // Major US ETFs - all verified working
      tickers: ['SPY', 'QQQ', 'DIA', 'EEM', 'GLD', 'XLK', 'XLE', 'XLF', 'XLV', 'VNQ', 'IWM', 'VTI']
    },
    {
      category: 'Cryptocurrencies',
      // Crypto pairs with USD - verified working
      tickers: ['BTC-USD', 'ETH-USD', 'BNB-USD', 'SOL-USD', 'XRP-USD', 'ADA-USD', 'DOGE-USD', 'LINK-USD', 'DOT-USD', 'AVAX-USD', 'UNI-USD', 'ATOM-USD']
    }
  ],
  regionalMarkets: [
    {
      region: 'India',
      // NSE listed stocks - verified working
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
      // US-listed Chinese ADRs - verified working
      tickers: [
        { symbol: 'BABA', name: 'Alibaba Group' },
        { symbol: 'PDD', name: 'PDD Holdings' },
        { symbol: 'JD', name: 'JD.com' },
        { symbol: 'BIDU', name: 'Baidu' },
        { symbol: 'NIO', name: 'NIO Inc' },
        { symbol: 'LI', name: 'Li Auto' },
        { symbol: 'XPEV', name: 'XPeng' },
        { symbol: 'BILI', name: 'Bilibili' },
        { symbol: 'NTES', name: 'NetEase' },
        { symbol: 'ZTO', name: 'ZTO Express' },
        { symbol: 'VNET', name: 'VNET Group' },
        { symbol: 'TAL', name: 'TAL Education' }
      ]
    },
    {
      region: 'United States',
      // Major US stocks - all verified working
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
   * Load user preferences from storage
   * Automatically merges new default tickers with existing saved preferences
   */
  async loadPreferences(): Promise<UserMarketPreferences> {
    try {
      const stored = await getSetting(STORAGE_KEY);
      if (stored) {
        const parsed = JSON.parse(stored) as UserMarketPreferences;

        // Merge with defaults to ensure new tickers are included
        const merged = this.mergeWithDefaults(parsed);

        // Save merged preferences if there were changes
        if (JSON.stringify(merged) !== JSON.stringify(parsed)) {
          console.log('[TickerStorage] Merged new default tickers with saved preferences');
          await this.savePreferences(merged);
        }

        return merged;
      }
    } catch (error) {
      console.error('[TickerStorage] Failed to load preferences:', error);
    }

    // Return defaults if nothing stored or error
    return DEFAULT_PREFERENCES;
  }

  /**
   * Merge saved preferences with defaults to include any new tickers
   */
  private mergeWithDefaults(saved: UserMarketPreferences): UserMarketPreferences {
    const merged: UserMarketPreferences = {
      globalMarkets: [],
      regionalMarkets: [],
      lastUpdated: Date.now()
    };

    // Merge global markets - use defaults as base, preserve user's existing tickers
    for (const defaultMarket of DEFAULT_PREFERENCES.globalMarkets) {
      const savedMarket = saved.globalMarkets.find(m => m.category === defaultMarket.category);
      if (savedMarket) {
        // Merge: keep user's tickers + add any new default tickers not already present
        const mergedTickers = [...savedMarket.tickers];
        for (const ticker of defaultMarket.tickers) {
          if (!mergedTickers.includes(ticker)) {
            mergedTickers.push(ticker);
          }
        }
        merged.globalMarkets.push({
          category: defaultMarket.category,
          tickers: mergedTickers
        });
      } else {
        // New category from defaults
        merged.globalMarkets.push({ ...defaultMarket });
      }
    }

    // Merge regional markets
    for (const defaultRegion of DEFAULT_PREFERENCES.regionalMarkets) {
      const savedRegion = saved.regionalMarkets.find(m => m.region === defaultRegion.region);
      if (savedRegion) {
        // Merge: keep user's tickers + add any new default tickers not already present
        const mergedTickers = [...savedRegion.tickers];
        for (const ticker of defaultRegion.tickers) {
          if (!mergedTickers.some(t => t.symbol === ticker.symbol)) {
            mergedTickers.push(ticker);
          }
        }
        merged.regionalMarkets.push({
          region: defaultRegion.region,
          tickers: mergedTickers
        });
      } else {
        // New region from defaults
        merged.regionalMarkets.push({ ...defaultRegion });
      }
    }

    return merged;
  }

  /**
   * Save user preferences to storage
   */
  async savePreferences(preferences: UserMarketPreferences): Promise<boolean> {
    try {
      preferences.lastUpdated = Date.now();
      await saveSetting(STORAGE_KEY, JSON.stringify(preferences), 'market_preferences');
      return true;
    } catch (error) {
      console.error('[TickerStorage] Failed to save preferences:', error);
      return false;
    }
  }

  /**
   * Get tickers for a specific category
   */
  async getCategoryTickers(category: string): Promise<string[]> {
    const prefs = await this.loadPreferences();
    const found = prefs.globalMarkets.find(m => m.category === category);
    return found?.tickers || [];
  }

  /**
   * Update tickers for a specific category
   */
  async updateCategoryTickers(category: string, tickers: string[]): Promise<boolean> {
    const prefs = await this.loadPreferences();
    const index = prefs.globalMarkets.findIndex(m => m.category === category);

    if (index !== -1) {
      prefs.globalMarkets[index].tickers = tickers;
      return await this.savePreferences(prefs);
    }

    return false;
  }

  /**
   * Add ticker to category
   */
  async addTickerToCategory(category: string, ticker: string): Promise<boolean> {
    const tickers = await this.getCategoryTickers(category);
    if (!tickers.includes(ticker)) {
      tickers.push(ticker);
      return await this.updateCategoryTickers(category, tickers);
    }
    return false;
  }

  /**
   * Remove ticker from category
   */
  async removeTickerFromCategory(category: string, ticker: string): Promise<boolean> {
    const tickers = await this.getCategoryTickers(category);
    const filtered = tickers.filter(t => t !== ticker);
    if (filtered.length !== tickers.length) {
      return await this.updateCategoryTickers(category, filtered);
    }
    return false;
  }

  /**
   * Reorder tickers in category
   */
  async reorderCategoryTickers(category: string, fromIndex: number, toIndex: number): Promise<boolean> {
    const tickers = await this.getCategoryTickers(category);
    const [removed] = tickers.splice(fromIndex, 1);
    tickers.splice(toIndex, 0, removed);
    return await this.updateCategoryTickers(category, tickers);
  }

  /**
   * Get regional market tickers
   */
  async getRegionalTickers(region: string): Promise<Array<{ symbol: string; name: string }>> {
    const prefs = await this.loadPreferences();
    const found = prefs.regionalMarkets.find(m => m.region === region);
    return found?.tickers || [];
  }

  /**
   * Update regional market tickers
   */
  async updateRegionalTickers(region: string, tickers: Array<{ symbol: string; name: string }>): Promise<boolean> {
    const prefs = await this.loadPreferences();
    const index = prefs.regionalMarkets.findIndex(m => m.region === region);

    if (index !== -1) {
      prefs.regionalMarkets[index].tickers = tickers;
      return await this.savePreferences(prefs);
    }

    return false;
  }

}

export const tickerStorage = new TickerStorageService();
export default tickerStorage;
