// Stock Screener Types
// TypeScript types matching Rust screener service

export type FilterOperator =
  | 'GreaterThan'
  | 'LessThan'
  | 'GreaterThanOrEqual'
  | 'LessThanOrEqual'
  | 'Equal'
  | 'NotEqual'
  | 'Between'
  | 'InList';

export type MetricType =
  // Valuation
  | 'PeRatio'
  | 'PbRatio'
  | 'PsRatio'
  | 'PcfRatio'
  | 'EvToEbitda'
  | 'PegRatio'
  // Profitability
  | 'Roe'
  | 'Roa'
  | 'Roic'
  | 'GrossMargin'
  | 'OperatingMargin'
  | 'NetMargin'
  // Growth
  | 'RevenueGrowth'
  | 'EarningsGrowth'
  | 'FcfGrowth'
  // Leverage
  | 'DebtToEquity'
  | 'DebtToAssets'
  | 'InterestCoverage'
  // Market Data
  | 'MarketCap'
  | 'Price'
  | 'Volume'
  | 'Beta'
  // Dividend
  | 'DividendYield'
  | 'PayoutRatio'
  | 'DividendGrowth';

export type FilterValue =
  | { Single: number }
  | { Range: { min: number; max: number } }
  | { List: string[] };

export interface FilterCondition {
  metric: MetricType;
  operator: FilterOperator;
  value: FilterValue;
}

export interface ScreenCriteria {
  name: string;
  description: string;
  conditions: FilterCondition[];
  sectorFilter?: string[];
  industryFilter?: string[];
}

export interface StockData {
  symbol: string;
  name: string;
  sector: string;
  industry: string;
  metrics: Record<string, number>;
}

export interface ScreenResult {
  totalStocksScreened: number;
  matchingStocks: StockData[];
  criteriaUsed: ScreenCriteria;
}

export interface SavedScreen {
  id: string;
  name: string;
  description: string;
  criteria: ScreenCriteria;
  createdAt: number;
  updatedAt: number;
}

export interface MetricInfo {
  id: string;
  name: string;
  category: 'Valuation' | 'Profitability' | 'Growth' | 'Leverage' | 'Market Data' | 'Dividend';
  description: string;
  type: MetricType;
}

// Helper functions to create filter values
export const createSingleValue = (value: number): FilterValue => ({
  Single: value,
});

export const createRangeValue = (min: number, max: number): FilterValue => ({
  Range: { min, max },
});

export const createListValue = (values: string[]): FilterValue => ({
  List: values,
});

// Helper function to format metric display names
export const getMetricDisplayName = (metricType: MetricType): string => {
  const nameMap: Record<MetricType, string> = {
    PeRatio: 'P/E Ratio',
    PbRatio: 'P/B Ratio',
    PsRatio: 'P/S Ratio',
    PcfRatio: 'P/CF Ratio',
    EvToEbitda: 'EV/EBITDA',
    PegRatio: 'PEG Ratio',
    Roe: 'ROE',
    Roa: 'ROA',
    Roic: 'ROIC',
    GrossMargin: 'Gross Margin',
    OperatingMargin: 'Operating Margin',
    NetMargin: 'Net Margin',
    RevenueGrowth: 'Revenue Growth',
    EarningsGrowth: 'Earnings Growth',
    FcfGrowth: 'FCF Growth',
    DebtToEquity: 'Debt/Equity',
    DebtToAssets: 'Debt/Assets',
    InterestCoverage: 'Interest Coverage',
    MarketCap: 'Market Cap',
    Price: 'Price',
    Volume: 'Volume',
    Beta: 'Beta',
    DividendYield: 'Dividend Yield',
    PayoutRatio: 'Payout Ratio',
    DividendGrowth: 'Dividend Growth',
  };
  return nameMap[metricType] || metricType;
};

// Helper function to get operator display name
export const getOperatorDisplayName = (operator: FilterOperator): string => {
  const nameMap: Record<FilterOperator, string> = {
    GreaterThan: '>',
    LessThan: '<',
    GreaterThanOrEqual: '≥',
    LessThanOrEqual: '≤',
    Equal: '=',
    NotEqual: '≠',
    Between: 'Between',
    InList: 'In List',
  };
  return nameMap[operator] || operator;
};
