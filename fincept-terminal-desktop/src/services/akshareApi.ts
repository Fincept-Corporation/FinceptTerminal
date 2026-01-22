import { invoke } from '@tauri-apps/api/core';

export interface AKShareResponse<T = any> {
  success: boolean;
  data?: T[] | T;  // Can be array or single object
  count?: number;
  timestamp?: number;
  error?: any;
  data_quality?: string;
  source?: string;
}

export interface EndpointsResponse {
  available_endpoints: string[];
  total_count: number;
  categories?: Record<string, string[]>;
  timestamp?: number;
}

// Data source configuration
export interface AKShareDataSource {
  id: string;
  name: string;
  script: string;
  description: string;
  icon: string;
  color: string;
  categories: string[];
}

export const AKSHARE_DATA_SOURCES: AKShareDataSource[] = [
  {
    id: 'data',
    name: 'Market Data',
    script: 'akshare_data.py',
    description: 'Stock quotes, indices, forex, futures, ETFs - real-time and historical data',
    icon: 'BarChart3',
    color: '#06B6D4',
    categories: ['Stock Market', 'Forex', 'Futures', 'ETF', 'Indices', 'Macro']
  },
  {
    id: 'index',
    name: 'Index Data',
    script: 'akshare_index.py',
    description: 'Index constituents, weights, historical data, global indices, Shenwan, CNI',
    icon: 'TrendingUp',
    color: '#6366F1',
    categories: ['Chinese Index', 'Shenwan Index', 'CNI Index', 'Global Index', 'Option QVIX', 'Economic Index', 'Commodity Index']
  },
  {
    id: 'futures',
    name: 'Futures',
    script: 'akshare_futures.py',
    description: 'Futures contracts, warehouse receipts, positions, spot prices, delivery',
    icon: 'Activity',
    color: '#F97316',
    categories: ['Contract Info', 'Realtime', 'Historical', 'Global', 'Warehouse', 'Positions', 'Delivery', 'Spot', 'Inventory', 'Hog']
  },
  {
    id: 'bonds',
    name: 'Bonds',
    script: 'akshare_bonds.py',
    description: 'Chinese & International bond markets, yield curves, government & corporate bonds',
    icon: 'Landmark',
    color: '#3B82F6',
    categories: ['Chinese Bond Market', 'Yield Curves', 'Government Bonds', 'Corporate Bonds', 'Convertible Bonds', 'International Bonds', 'Bond Indices', 'Ratings']
  },
  {
    id: 'derivatives',
    name: 'Derivatives',
    script: 'akshare_derivatives.py',
    description: 'Options, futures, Greeks, volatility, spreads from Chinese exchanges',
    icon: 'LineChart',
    color: '#8B5CF6',
    categories: ['Options', 'Futures', 'Greeks', 'Volatility', 'Spreads', 'Inventory']
  },
  {
    id: 'currency',
    name: 'Currency/Forex',
    script: 'akshare_currency.py',
    description: 'Exchange rates, currency pairs, historical forex data, currency converter',
    icon: 'DollarSign',
    color: '#22C55E',
    categories: ['Exchange Rates', 'Currency Data']
  },
  {
    id: 'china_economics',
    name: 'China Economics',
    script: 'akshare_economics_china.py',
    description: 'Chinese macro indicators: GDP, CPI, PPI, PMI, money supply, trade data',
    icon: 'TrendingUp',
    color: '#EF4444',
    categories: ['GDP', 'Inflation', 'PMI', 'Money Supply', 'Trade', 'Industrial', 'Consumer', 'Fiscal', 'Banking', 'Real Estate']
  },
  {
    id: 'global_economics',
    name: 'Global Economics',
    script: 'akshare_economics_global.py',
    description: 'US, Eurozone, UK, Japan, Australia, Canada macro data & global indicators',
    icon: 'Globe',
    color: '#10B981',
    categories: ['US', 'Eurozone', 'UK', 'Japan', 'Australia', 'Canada', 'Switzerland', 'Global Indices', 'Commodities']
  },
  {
    id: 'funds',
    name: 'Funds',
    script: 'akshare_funds_expanded.py',
    description: 'ETF analytics, mutual funds, fund ratings, risk metrics, flows',
    icon: 'PieChart',
    color: '#F59E0B',
    categories: ['ETF', 'Mutual Funds', 'Fund Rankings', 'Risk Metrics', 'Fund Flows', 'Private Equity']
  },
  {
    id: 'energy',
    name: 'Energy',
    script: 'akshare_energy.py',
    description: 'Carbon trading markets, oil prices, energy data from China and EU',
    icon: 'Zap',
    color: '#FBBF24',
    categories: ['Carbon Trading', 'Oil']
  },
  {
    id: 'crypto',
    name: 'Crypto',
    script: 'akshare_crypto.py',
    description: 'Bitcoin CME futures, holding reports, crypto spot prices',
    icon: 'Bitcoin',
    color: '#F7931A',
    categories: ['Bitcoin', 'Spot']
  },
  {
    id: 'news',
    name: 'News',
    script: 'akshare_news.py',
    description: 'CCTV news, Baidu economic news, trade notifications, dividends',
    icon: 'Newspaper',
    color: '#64748B',
    categories: ['News Feeds', 'Trade Notifications']
  },
  {
    id: 'reits',
    name: 'REITs',
    script: 'akshare_reits.py',
    description: 'Real Estate Investment Trusts - realtime, historical, minute data',
    icon: 'Building',
    color: '#0EA5E9',
    categories: ['REITs']
  },
  {
    id: 'alternative',
    name: 'Alternative Data',
    script: 'akshare_alternative.py',
    description: 'Air quality, carbon trading, energy, real estate, auto sales, sentiment',
    icon: 'Layers',
    color: '#EC4899',
    categories: ['Air Quality', 'Carbon Trading', 'Energy', 'Real Estate', 'Auto Sales', 'Movies', 'Demographics', 'Sentiment', 'Rankings']
  },
  {
    id: 'analysis',
    name: 'Market Analysis',
    script: 'akshare_analysis.py',
    description: 'Technical analysis, market breadth, sector rotation, smart money',
    icon: 'BarChart3',
    color: '#14B8A6',
    categories: ['Technical', 'Breadth', 'Sectors', 'Smart Money']
  },
  {
    id: 'company',
    name: 'Company Info',
    script: 'akshare_company_info.py',
    description: 'Company profiles, basic info, financial data for CN, HK, US stocks',
    icon: 'Building2',
    color: '#8B5CF6',
    categories: ['CN Stocks', 'HK Stocks', 'US Stocks', 'Company Profiles']
  },
  // ==================== STOCK DATA (8 Batches - 398 endpoints) ====================
  {
    id: 'stocks_realtime',
    name: 'Stocks: Realtime',
    script: 'akshare_stocks_realtime.py',
    description: 'Realtime stock data: A-shares, B-shares, HK, US, indices, info',
    icon: 'Activity',
    color: '#10B981',
    categories: ['A-Shares Realtime', 'B-Shares', 'HK Stocks', 'US Stocks', 'A+H Shares', 'Index Realtime', 'STAR Market', 'Stock Info', 'Global Info', 'Individual Info']
  },
  {
    id: 'stocks_historical',
    name: 'Stocks: Historical',
    script: 'akshare_stocks_historical.py',
    description: 'Historical price data: daily, minute, intraday, tick, sector',
    icon: 'Clock',
    color: '#3B82F6',
    categories: ['A-Shares Historical', 'B-Shares Historical', 'HK Historical', 'US Historical', 'A+H Historical', 'Index Historical', 'STAR Market Historical', 'Intraday', 'Tick Data', 'Price Data', 'Bid-Ask', 'Sector Data', 'Classification', 'Exchange Summary', 'Valuation', 'Comparison']
  },
  {
    id: 'stocks_financial',
    name: 'Stocks: Financial',
    script: 'akshare_stocks_financial.py',
    description: 'Financial statements, analysis, earnings, dividends, profiles',
    icon: 'FileText',
    color: '#8B5CF6',
    categories: ['Balance Sheet', 'Income Statement', 'Cash Flow', 'Financial Analysis', 'HK Financial', 'US Financial', 'THS Financial', 'Earnings', 'Profit Forecast', 'Dividend', 'Company Profile']
  },
  {
    id: 'stocks_holders',
    name: 'Stocks: Shareholders',
    script: 'akshare_stocks_holders.py',
    description: 'Shareholder data, holdings, institutional ownership, pledge',
    icon: 'Users',
    color: '#F59E0B',
    categories: ['Major Shareholders', 'Shareholder Analysis', 'Shareholder Changes', 'Fund Holdings', 'Institutional Holdings', 'Management Holdings', 'Share Changes', 'Executive Holdings', 'Restricted Shares', 'Equity Pledge', 'CNINFO Holdings']
  },
  {
    id: 'stocks_funds',
    name: 'Stocks: Fund Flows',
    script: 'akshare_stocks_funds.py',
    description: 'Fund flows, capital movement, block trades, LHB, IPO',
    icon: 'ArrowRightLeft',
    color: '#EF4444',
    categories: ['Individual Fund Flow', 'Market Fund Flow', 'Fund Flow Types', 'Block Trade', 'LHB', 'LHB Broker', 'Repurchase', 'Allotment', 'IPO', 'New Issues']
  },
  {
    id: 'stocks_board',
    name: 'Stocks: Boards',
    script: 'akshare_stocks_board.py',
    description: 'Industry boards, concept boards, ZT pool, analyst, rankings',
    icon: 'LayoutGrid',
    color: '#6366F1',
    categories: ['Concept Board', 'Industry Board', 'Board Changes', 'Industry Category', 'ZT Pool', 'Analyst', 'Research', 'Rankings']
  },
  {
    id: 'stocks_margin',
    name: 'Stocks: Margin & HSGT',
    script: 'akshare_stocks_margin.py',
    description: 'Margin trading, HSGT (Stock Connect), registration, PE/PB',
    icon: 'Percent',
    color: '#EC4899',
    categories: ['Margin Trading', 'HSGT', 'Exchange Rate', 'Registration', 'HK Indicator', 'PE/PB Data', 'Market Indicators', 'A-Share Utils', 'Market Activity', 'CYQ', 'STAQ']
  },
  {
    id: 'stocks_hot',
    name: 'Stocks: Hot & News',
    script: 'akshare_stocks_hot.py',
    description: 'Hot stocks, news, comments, ESG, sentiment, special data',
    icon: 'Flame',
    color: '#F97316',
    categories: ['Hot Stocks', 'HK Hot Stocks', 'XueQiu Hot', 'News', 'Comments', 'Weibo Sentiment', 'ESG', 'Investor Relations', 'SNS Info', 'Disclosure', 'Special Data', 'Vote', 'Management']
  }
];

export class AKShareAPI {
  // ==================== GENERIC QUERY API ====================

  /**
   * Query any AKShare endpoint dynamically
   * @param script - Python script name (e.g., 'akshare_bonds.py')
   * @param endpoint - Endpoint name (e.g., 'bond_yield')
   * @param args - Optional arguments for the endpoint
   */
  static async query(script: string, endpoint: string, args?: string[]): Promise<AKShareResponse> {
    return await invoke('akshare_query', { script, endpoint, args });
  }

  /**
   * Get all available endpoints for a script
   * @param script - Python script name
   */
  static async getEndpoints(script: string): Promise<AKShareResponse<EndpointsResponse>> {
    return await invoke('akshare_get_endpoints', { script });
  }

  // ==================== STOCK MARKET DATA ====================

  static async getStockCNSpot(): Promise<AKShareResponse> {
    return await invoke('get_stock_cn_spot');
  }

  static async getStockUSSpot(): Promise<AKShareResponse> {
    return await invoke('get_stock_us_spot');
  }

  static async getStockHKSpot(): Promise<AKShareResponse> {
    return await invoke('get_stock_hk_spot');
  }

  static async getStockHotRank(): Promise<AKShareResponse> {
    return await invoke('get_stock_hot_rank');
  }

  static async getStockIndustryList(): Promise<AKShareResponse> {
    return await invoke('get_stock_industry_list');
  }

  static async getStockConceptList(): Promise<AKShareResponse> {
    return await invoke('get_stock_concept_list');
  }

  static async getStockHSGTData(): Promise<AKShareResponse> {
    return await invoke('get_stock_hsgt_data');
  }

  static async getStockIndustryPE(): Promise<AKShareResponse> {
    return await invoke('get_stock_industry_pe');
  }

  // ==================== ECONOMICS ====================

  static async getMacroChinaGDP(): Promise<AKShareResponse> {
    return await invoke('get_macro_china_gdp');
  }

  static async getMacroChinaCPI(): Promise<AKShareResponse> {
    return await invoke('get_macro_china_cpi');
  }

  static async getMacroChinaPMI(): Promise<AKShareResponse> {
    return await invoke('get_macro_china_pmi');
  }

  // ==================== BONDS ====================

  static async getBondChinaYield(): Promise<AKShareResponse> {
    return await invoke('get_bond_china_yield');
  }

  // ==================== FUNDS ====================

  static async getFundETFSpot(): Promise<AKShareResponse> {
    return await invoke('get_fund_etf_spot');
  }

  static async getFundRanking(): Promise<AKShareResponse> {
    return await invoke('get_fund_ranking');
  }

  // ==================== COMPANY INFORMATION ====================

  static async getStockCNInfo(symbol: string): Promise<AKShareResponse> {
    return await invoke('get_stock_cn_info', { symbol });
  }

  static async getStockCNProfile(symbol: string): Promise<AKShareResponse> {
    return await invoke('get_stock_cn_profile', { symbol });
  }

  static async getStockHKProfile(symbol: string): Promise<AKShareResponse> {
    return await invoke('get_stock_hk_profile', { symbol });
  }

  static async getStockUSInfo(symbol: string): Promise<AKShareResponse> {
    return await invoke('get_stock_us_info', { symbol });
  }

  // ==================== QUICK ACCESS METHODS ====================

  // China Economics shortcuts
  static async getChinaCPI(): Promise<AKShareResponse> {
    return this.query('akshare_economics_china.py', 'cpi');
  }

  static async getChinaGDP(): Promise<AKShareResponse> {
    return this.query('akshare_economics_china.py', 'gdp');
  }

  static async getChinaPMI(): Promise<AKShareResponse> {
    return this.query('akshare_economics_china.py', 'pmi');
  }

  static async getChinaM2(): Promise<AKShareResponse> {
    return this.query('akshare_economics_china.py', 'm2');
  }

  static async getChinaShibor(): Promise<AKShareResponse> {
    return this.query('akshare_economics_china.py', 'shibor');
  }

  static async getChinaLPR(): Promise<AKShareResponse> {
    return this.query('akshare_economics_china.py', 'lpr');
  }

  // Global Economics shortcuts
  static async getUSGDP(): Promise<AKShareResponse> {
    return this.query('akshare_economics_global.py', 'us_gdp');
  }

  static async getUSCPI(): Promise<AKShareResponse> {
    return this.query('akshare_economics_global.py', 'us_cpi');
  }

  static async getUSUnemployment(): Promise<AKShareResponse> {
    return this.query('akshare_economics_global.py', 'us_unemployment');
  }

  static async getVIX(): Promise<AKShareResponse> {
    return this.query('akshare_economics_global.py', 'vix');
  }

  static async getDollarIndex(): Promise<AKShareResponse> {
    return this.query('akshare_economics_global.py', 'dollar_index');
  }

  // Bonds shortcuts
  static async getBondYieldCurve(): Promise<AKShareResponse> {
    return this.query('akshare_bonds.py', 'bond_yield');
  }

  static async getConvertibleBonds(): Promise<AKShareResponse> {
    return this.query('akshare_bonds.py', 'bond_convert');
  }

  // Derivatives shortcuts
  static async getOptions(): Promise<AKShareResponse> {
    return this.query('akshare_derivatives.py', 'option_em');
  }

  static async getOptionsGreeks(): Promise<AKShareResponse> {
    return this.query('akshare_derivatives.py', 'option_greeks');
  }

  // Alternative data shortcuts
  static async getAirQuality(): Promise<AKShareResponse> {
    return this.query('akshare_alternative.py', 'air_quality');
  }

  static async getCarbonPrice(): Promise<AKShareResponse> {
    return this.query('akshare_alternative.py', 'carbon_price');
  }

  static async getRealEstate(): Promise<AKShareResponse> {
    return this.query('akshare_alternative.py', 'real_estate');
  }

  static async getCarSales(): Promise<AKShareResponse> {
    return this.query('akshare_alternative.py', 'car_sales');
  }

  // ==================== TRANSLATION ====================

  static async translateBatch(texts: string[], targetLang: string): Promise<any> {
    return await invoke('translate_batch', { texts, targetLang });
  }
}
