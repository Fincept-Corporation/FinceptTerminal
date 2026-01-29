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

// ALL VERIFIED WORKING FILES
export const AKSHARE_DATA_SOURCES: AKShareDataSource[] = [
  {
    id: 'data',
    name: 'Market Data',
    script: 'akshare_data.py',
    description: 'Stock quotes, indices, forex, futures, ETFs - 14 endpoints',
    icon: 'BarChart3',
    color: '#06B6D4',
    categories: ['Stocks', 'Funds', 'Macro', 'Bonds', 'Forex', 'Futures']
  },
  {
    id: 'index',
    name: 'Index Data',
    script: 'akshare_index.py',
    description: 'Index constituents, weights, historical - 74 endpoints',
    icon: 'TrendingUp',
    color: '#6366F1',
    categories: ['Chinese Index', 'Shenwan Index', 'CNI Index', 'Global Index']
  },
  {
    id: 'futures',
    name: 'Futures',
    script: 'akshare_futures.py',
    description: 'Futures contracts, positions, spot prices - 59 endpoints',
    icon: 'Activity',
    color: '#F97316',
    categories: ['Contract Info', 'Realtime', 'Historical', 'Global', 'Warehouse', 'Positions']
  },
  {
    id: 'bonds',
    name: 'Bonds',
    script: 'akshare_bonds.py',
    description: 'Bond markets, yield curves, government & corporate - 28 endpoints',
    icon: 'Landmark',
    color: '#3B82F6',
    categories: ['Chinese Bond Market', 'Yield Curves', 'Government Bonds', 'Corporate Bonds', 'Convertible Bonds']
  },
  {
    id: 'currency',
    name: 'Currency/Forex',
    script: 'akshare_currency.py',
    description: 'Exchange rates, currency pairs - 8 endpoints',
    icon: 'DollarSign',
    color: '#22C55E',
    categories: ['Exchange Rates', 'Currency Data']
  },
  {
    id: 'energy',
    name: 'Energy',
    script: 'akshare_energy.py',
    description: 'Carbon trading, oil prices - 8 endpoints',
    icon: 'Zap',
    color: '#FBBF24',
    categories: ['Carbon Trading', 'Oil']
  },
  {
    id: 'crypto',
    name: 'Crypto',
    script: 'akshare_crypto.py',
    description: 'Bitcoin CME futures, crypto spot - 3 endpoints',
    icon: 'Bitcoin',
    color: '#F7931A',
    categories: ['Bitcoin', 'Spot']
  },
  {
    id: 'news',
    name: 'News',
    script: 'akshare_news.py',
    description: 'CCTV news, economic news - 5 endpoints',
    icon: 'Newspaper',
    color: '#64748B',
    categories: ['News Feeds', 'Trade Notifications']
  },
  {
    id: 'reits',
    name: 'REITs',
    script: 'akshare_reits.py',
    description: 'Real Estate Investment Trusts - 3 endpoints',
    icon: 'Building',
    color: '#0EA5E9',
    categories: ['REITs']
  },
  {
    id: 'stocks_realtime',
    name: 'Stocks: Realtime',
    script: 'akshare_stocks_realtime.py',
    description: 'Realtime A-shares, B-shares, HK, US - 49 endpoints',
    icon: 'Activity',
    color: '#10B981',
    categories: ['A-Shares Realtime', 'B-Shares', 'HK Stocks', 'US Stocks']
  },
  {
    id: 'stocks_historical',
    name: 'Stocks: Historical',
    script: 'akshare_stocks_historical.py',
    description: 'Historical daily/minute/tick data - 52 endpoints',
    icon: 'Clock',
    color: '#3B82F6',
    categories: ['A-Shares Historical', 'HK Historical', 'US Historical', 'Intraday']
  },
  {
    id: 'stocks_financial',
    name: 'Stocks: Financial',
    script: 'akshare_stocks_financial.py',
    description: 'Financial statements, earnings - 45 endpoints',
    icon: 'FileText',
    color: '#8B5CF6',
    categories: ['Balance Sheet', 'Income Statement', 'Cash Flow', 'Earnings']
  },
  {
    id: 'stocks_holders',
    name: 'Stocks: Shareholders',
    script: 'akshare_stocks_holders.py',
    description: 'Shareholder data, holdings - 49 endpoints',
    icon: 'Users',
    color: '#F59E0B',
    categories: ['Major Shareholders', 'Fund Holdings', 'Institutional']
  },
  {
    id: 'stocks_funds',
    name: 'Stocks: Fund Flows',
    script: 'akshare_stocks_funds.py',
    description: 'Fund flows, block trades, IPO - 56 endpoints',
    icon: 'ArrowRightLeft',
    color: '#EF4444',
    categories: ['Fund Flow', 'Block Trade', 'LHB', 'IPO']
  },
  {
    id: 'stocks_board',
    name: 'Stocks: Boards',
    script: 'akshare_stocks_board.py',
    description: 'Industry/concept boards - 49 endpoints',
    icon: 'LayoutGrid',
    color: '#6366F1',
    categories: ['Concept Board', 'Industry Board', 'Rankings']
  },
  {
    id: 'stocks_margin',
    name: 'Stocks: Margin & HSGT',
    script: 'akshare_stocks_margin.py',
    description: 'Margin trading, HSGT - 46 endpoints',
    icon: 'Percent',
    color: '#EC4899',
    categories: ['Margin Trading', 'HSGT', 'PE/PB']
  },
  {
    id: 'stocks_hot',
    name: 'Stocks: Hot & News',
    script: 'akshare_stocks_hot.py',
    description: 'Hot stocks, news, sentiment - 55 endpoints',
    icon: 'Flame',
    color: '#F97316',
    categories: ['Hot Stocks', 'News', 'ESG', 'Sentiment']
  },
  {
    id: 'alternative',
    name: 'Alternative Data',
    script: 'akshare_alternative.py',
    description: 'Air quality, carbon, real estate - 14 endpoints',
    icon: 'Layers',
    color: '#EC4899',
    categories: ['Air Quality', 'Carbon', 'Energy', 'Movies']
  },
  {
    id: 'analysis',
    name: 'Market Analysis',
    script: 'akshare_analysis.py',
    description: 'Technical analysis, market breadth',
    icon: 'BarChart3',
    color: '#14B8A6',
    categories: ['Technical', 'Breadth', 'Fund Flow']
  },
  {
    id: 'derivatives',
    name: 'Derivatives',
    script: 'akshare_derivatives.py',
    description: 'Options and derivatives data - 46 endpoints',
    icon: 'TrendingUp',
    color: '#8B5CF6',
    categories: ['CFFEX Options', 'SSE Options', 'Commodity Options', 'Historical']
  },
  {
    id: 'economics_china',
    name: 'China Economics',
    script: 'akshare_economics_china.py',
    description: 'Chinese economic indicators - 85 endpoints',
    icon: 'LineChart',
    color: '#DC2626',
    categories: ['Core Indicators', 'Monetary', 'Trade', 'Industry', 'Real Estate']
  },
  {
    id: 'economics_global',
    name: 'Global Economics',
    script: 'akshare_economics_global.py',
    description: 'International economic indicators - 35 endpoints',
    icon: 'Globe',
    color: '#059669',
    categories: ['USA', 'Eurozone', 'UK', 'Japan', 'Canada', 'Australia']
  },
  {
    id: 'funds_expanded',
    name: 'Funds Expanded',
    script: 'akshare_funds_expanded.py',
    description: 'Comprehensive fund data - 70 endpoints',
    icon: 'Wallet',
    color: '#7C3AED',
    categories: ['Fund Rankings', 'Fund Info', 'Holdings', 'Managers', 'Flows']
  },
  {
    id: 'company_info',
    name: 'Company Info',
    script: 'akshare_company_info.py',
    description: 'Company profiles and details',
    icon: 'Building2',
    color: '#0891B2',
    categories: ['CN Stocks', 'HK Stocks', 'US Stocks']
  },
  {
    id: 'macro',
    name: 'Macro Global',
    script: 'akshare_macro.py',
    description: 'Global macro economic data - 96 endpoints',
    icon: 'TrendingUp',
    color: '#C026D3',
    categories: ['Australia', 'Brazil', 'India', 'Russia', 'Canada', 'Euro', 'Germany', 'Japan', 'Switzerland', 'UK', 'USA']
  },
  {
    id: 'misc',
    name: 'Miscellaneous',
    script: 'akshare_misc.py',
    description: 'Misc data sources - 129 endpoints',
    icon: 'Package',
    color: '#EA580C',
    categories: ['AMAC', 'Air Quality', 'Car Sales', 'Movies', 'FRED', 'Migration']
  },
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
