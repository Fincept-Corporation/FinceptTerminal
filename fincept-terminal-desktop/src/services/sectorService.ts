// Sector Classification Service
// Maps stock symbols to their sectors and industries

export interface SectorInfo {
  sector: string;
  industry?: string;
  description?: string;
}

// Common sector classifications (GICS sectors)
export const SECTORS = {
  TECHNOLOGY: 'Technology',
  HEALTHCARE: 'Healthcare',
  FINANCIAL: 'Financial Services',
  CONSUMER_CYCLICAL: 'Consumer Cyclical',
  CONSUMER_DEFENSIVE: 'Consumer Defensive',
  INDUSTRIALS: 'Industrials',
  ENERGY: 'Energy',
  UTILITIES: 'Utilities',
  REAL_ESTATE: 'Real Estate',
  MATERIALS: 'Basic Materials',
  COMMUNICATION: 'Communication Services',
  CRYPTO: 'Cryptocurrency',
  OTHER: 'Other'
};

// Known symbol to sector mappings
const SYMBOL_SECTOR_MAP: Record<string, SectorInfo> = {
  // Technology
  'AAPL': { sector: SECTORS.TECHNOLOGY, industry: 'Consumer Electronics' },
  'MSFT': { sector: SECTORS.TECHNOLOGY, industry: 'Software' },
  'GOOGL': { sector: SECTORS.COMMUNICATION, industry: 'Internet Content' },
  'GOOG': { sector: SECTORS.COMMUNICATION, industry: 'Internet Content' },
  'AMZN': { sector: SECTORS.CONSUMER_CYCLICAL, industry: 'Internet Retail' },
  'META': { sector: SECTORS.COMMUNICATION, industry: 'Social Media' },
  'NVDA': { sector: SECTORS.TECHNOLOGY, industry: 'Semiconductors' },
  'TSLA': { sector: SECTORS.CONSUMER_CYCLICAL, industry: 'Auto Manufacturers' },
  'AMD': { sector: SECTORS.TECHNOLOGY, industry: 'Semiconductors' },
  'INTC': { sector: SECTORS.TECHNOLOGY, industry: 'Semiconductors' },
  'CRM': { sector: SECTORS.TECHNOLOGY, industry: 'Software' },
  'ORCL': { sector: SECTORS.TECHNOLOGY, industry: 'Software' },
  'ADBE': { sector: SECTORS.TECHNOLOGY, industry: 'Software' },
  'CSCO': { sector: SECTORS.TECHNOLOGY, industry: 'Communication Equipment' },
  'IBM': { sector: SECTORS.TECHNOLOGY, industry: 'Information Technology Services' },

  // Healthcare
  'JNJ': { sector: SECTORS.HEALTHCARE, industry: 'Drug Manufacturers' },
  'UNH': { sector: SECTORS.HEALTHCARE, industry: 'Healthcare Plans' },
  'PFE': { sector: SECTORS.HEALTHCARE, industry: 'Drug Manufacturers' },
  'ABBV': { sector: SECTORS.HEALTHCARE, industry: 'Drug Manufacturers' },
  'TMO': { sector: SECTORS.HEALTHCARE, industry: 'Diagnostics & Research' },
  'ABT': { sector: SECTORS.HEALTHCARE, industry: 'Medical Devices' },
  'MRK': { sector: SECTORS.HEALTHCARE, industry: 'Drug Manufacturers' },

  // Financial
  'JPM': { sector: SECTORS.FINANCIAL, industry: 'Banks' },
  'BAC': { sector: SECTORS.FINANCIAL, industry: 'Banks' },
  'WFC': { sector: SECTORS.FINANCIAL, industry: 'Banks' },
  'GS': { sector: SECTORS.FINANCIAL, industry: 'Capital Markets' },
  'MS': { sector: SECTORS.FINANCIAL, industry: 'Capital Markets' },
  'V': { sector: SECTORS.FINANCIAL, industry: 'Credit Services' },
  'MA': { sector: SECTORS.FINANCIAL, industry: 'Credit Services' },
  'BRK.B': { sector: SECTORS.FINANCIAL, industry: 'Insurance' },

  // Consumer
  'WMT': { sector: SECTORS.CONSUMER_DEFENSIVE, industry: 'Discount Stores' },
  'PG': { sector: SECTORS.CONSUMER_DEFENSIVE, industry: 'Household Products' },
  'KO': { sector: SECTORS.CONSUMER_DEFENSIVE, industry: 'Beverages' },
  'PEP': { sector: SECTORS.CONSUMER_DEFENSIVE, industry: 'Beverages' },
  'COST': { sector: SECTORS.CONSUMER_DEFENSIVE, industry: 'Discount Stores' },
  'NKE': { sector: SECTORS.CONSUMER_CYCLICAL, industry: 'Footwear & Accessories' },
  'MCD': { sector: SECTORS.CONSUMER_CYCLICAL, industry: 'Restaurants' },
  'SBUX': { sector: SECTORS.CONSUMER_CYCLICAL, industry: 'Restaurants' },
  'DIS': { sector: SECTORS.COMMUNICATION, industry: 'Entertainment' },
  'NFLX': { sector: SECTORS.COMMUNICATION, industry: 'Entertainment' },

  // Energy
  'XOM': { sector: SECTORS.ENERGY, industry: 'Oil & Gas' },
  'CVX': { sector: SECTORS.ENERGY, industry: 'Oil & Gas' },
  'COP': { sector: SECTORS.ENERGY, industry: 'Oil & Gas' },

  // Industrials
  'BA': { sector: SECTORS.INDUSTRIALS, industry: 'Aerospace & Defense' },
  'HON': { sector: SECTORS.INDUSTRIALS, industry: 'Conglomerates' },
  'UPS': { sector: SECTORS.INDUSTRIALS, industry: 'Integrated Freight & Logistics' },
  'CAT': { sector: SECTORS.INDUSTRIALS, industry: 'Farm & Heavy Machinery' },

  // Indian Stocks
  'RELIANCE.NS': { sector: SECTORS.ENERGY, industry: 'Oil & Gas Refining' },
  'RELIANCE.BO': { sector: SECTORS.ENERGY, industry: 'Oil & Gas Refining' },
  'TCS.NS': { sector: SECTORS.TECHNOLOGY, industry: 'Information Technology Services' },
  'TCS.BO': { sector: SECTORS.TECHNOLOGY, industry: 'Information Technology Services' },
  'INFY.NS': { sector: SECTORS.TECHNOLOGY, industry: 'Information Technology Services' },
  'INFY.BO': { sector: SECTORS.TECHNOLOGY, industry: 'Information Technology Services' },
  'HDFCBANK.NS': { sector: SECTORS.FINANCIAL, industry: 'Banks' },
  'HDFCBANK.BO': { sector: SECTORS.FINANCIAL, industry: 'Banks' },
  'ITC.NS': { sector: SECTORS.CONSUMER_DEFENSIVE, industry: 'Tobacco' },
  'ITC.BO': { sector: SECTORS.CONSUMER_DEFENSIVE, industry: 'Tobacco' },

  // Cryptocurrency
  'BTC-USD': { sector: SECTORS.CRYPTO, industry: 'Cryptocurrency' },
  'ETH-USD': { sector: SECTORS.CRYPTO, industry: 'Cryptocurrency' },
  'USDT-USD': { sector: SECTORS.CRYPTO, industry: 'Stablecoin' },
  'BNB-USD': { sector: SECTORS.CRYPTO, industry: 'Cryptocurrency' },
  'XRP-USD': { sector: SECTORS.CRYPTO, industry: 'Cryptocurrency' },
  'SOL-USD': { sector: SECTORS.CRYPTO, industry: 'Cryptocurrency' },
  'ADA-USD': { sector: SECTORS.CRYPTO, industry: 'Cryptocurrency' },
  'DOGE-USD': { sector: SECTORS.CRYPTO, industry: 'Cryptocurrency' },

  // ETFs
  'SPY': { sector: SECTORS.OTHER, industry: 'Index ETF' },
  'QQQ': { sector: SECTORS.OTHER, industry: 'Index ETF' },
  'VOO': { sector: SECTORS.OTHER, industry: 'Index ETF' },
  'VTI': { sector: SECTORS.OTHER, industry: 'Index ETF' },
  'IWM': { sector: SECTORS.OTHER, industry: 'Index ETF' },
};

class SectorService {
  /**
   * Get sector information for a symbol
   */
  getSectorInfo(symbol: string): SectorInfo {
    // Normalize symbol (uppercase, remove exchange suffix variations)
    const normalizedSymbol = symbol.toUpperCase().trim();

    // Check direct mapping
    if (SYMBOL_SECTOR_MAP[normalizedSymbol]) {
      return SYMBOL_SECTOR_MAP[normalizedSymbol];
    }

    // Check without exchange suffix (.NS, .BO, etc.)
    const baseSymbol = normalizedSymbol.split('.')[0];
    if (SYMBOL_SECTOR_MAP[baseSymbol]) {
      return SYMBOL_SECTOR_MAP[baseSymbol];
    }

    // Heuristic classification based on symbol patterns
    return this.classifyByPattern(normalizedSymbol);
  }

  /**
   * Classify symbol by patterns (fallback)
   */
  private classifyByPattern(symbol: string): SectorInfo {
    // Crypto patterns
    if (symbol.endsWith('-USD') || symbol.endsWith('USD') || symbol.endsWith('USDT')) {
      return { sector: SECTORS.CRYPTO, industry: 'Cryptocurrency' };
    }

    // ETF patterns
    if (symbol.length === 3 && (symbol.startsWith('V') || symbol.startsWith('I') || symbol.startsWith('S'))) {
      return { sector: SECTORS.OTHER, industry: 'Index ETF' };
    }

    // Indian stocks
    if (symbol.endsWith('.NS') || symbol.endsWith('.BO')) {
      return { sector: SECTORS.OTHER, industry: 'Indian Stock' };
    }

    // Default fallback
    return { sector: SECTORS.OTHER, industry: 'Unknown' };
  }

  /**
   * Get all unique sectors from a list of symbols
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
   * Group symbols by sector
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
   * Add custom sector mapping
   */
  addSectorMapping(symbol: string, sectorInfo: SectorInfo): void {
    SYMBOL_SECTOR_MAP[symbol.toUpperCase()] = sectorInfo;
  }

  /**
   * Get all supported sectors
   */
  getAllSectors(): string[] {
    return Object.values(SECTORS);
  }
}

// Export singleton
export const sectorService = new SectorService();
export default sectorService;
