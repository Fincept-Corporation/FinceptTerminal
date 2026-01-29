/**
 * Custom Index Types - Fincept Terminal
 * Types and interfaces for the aggregate index feature
 */

// Index calculation methods from most common to least used
export type IndexCalculationMethod =
  | 'price_weighted'           // Dow Jones style - sum of prices / divisor
  | 'market_cap_weighted'      // S&P 500 style - weighted by market cap
  | 'equal_weighted'           // Each constituent has equal weight
  | 'float_adjusted'           // Weighted by free-float market cap
  | 'fundamental_weighted'     // Weighted by fundamental factors (earnings, revenue)
  | 'modified_market_cap'      // Capped market cap weighting
  | 'factor_weighted'          // Factor-based (momentum, value, quality)
  | 'risk_parity'              // Inverse volatility weighting
  | 'geometric_mean'           // Geometric average of returns
  | 'capped_weighted';         // Market cap with maximum weight cap

export interface IndexConstituentConfig {
  symbol: string;
  shares?: number;            // For price-weighted: shares outstanding
  weight?: number;            // For custom weighting (0-100)
  marketCap?: number;         // For market cap weighted
  fundamentalScore?: number;  // For fundamental weighted
  customPrice?: number;       // Manual price override
  priceDate?: string;         // Date for historical price lookup (YYYY-MM-DD)
}

export interface IndexConstituent extends IndexConstituentConfig {
  id: string;
  index_id: string;
  current_price: number;
  previous_close: number;
  market_value: number;
  effective_weight: number;  // Calculated weight after method applied
  day_change: number;
  day_change_percent: number;
  contribution: number;      // Contribution to index change
  added_at: string;
}

export interface CustomIndex {
  id: string;
  name: string;
  description?: string;
  calculation_method: IndexCalculationMethod;
  base_value: number;              // Starting index value (e.g., 100, 1000)
  base_date: string;               // Date when index started
  divisor: number;                 // Used for price-weighted calculations
  current_value: number;           // Current calculated index value
  previous_close: number;          // Previous day close value
  cap_weight?: number;             // Max weight per constituent (for capped methods)
  currency: string;
  portfolio_id?: string;           // Optional: linked portfolio
  is_active: boolean;
  created_at: string;
  updated_at: string;
}

export interface IndexSnapshot {
  id: string;
  index_id: string;
  index_value: number;
  day_change: number;
  day_change_percent: number;
  total_market_value: number;
  divisor: number;
  constituents_data: string;       // JSON snapshot of all constituents
  snapshot_date: string;
  created_at: string;
}

export interface IndexSummary {
  index: CustomIndex;
  constituents: IndexConstituent[];
  total_market_value: number;
  index_value: number;
  day_change: number;
  day_change_percent: number;
  ytd_change?: number;
  ytd_change_percent?: number;
  last_updated: string;
}

export interface IndexPerformanceData {
  date: string;
  value: number;
  change: number;
  change_percent: number;
}

// Index calculation method descriptions
export const INDEX_METHOD_INFO: Record<IndexCalculationMethod, {
  name: string;
  description: string;
  formula: string;
  examples: string[];
  complexity: 'simple' | 'moderate' | 'complex';
}> = {
  price_weighted: {
    name: 'Price-Weighted',
    description: 'Sum of constituent prices divided by a divisor. Higher-priced stocks have more influence.',
    formula: 'Index = Σ(Prices) / Divisor',
    examples: ['Dow Jones Industrial Average (DJIA)', 'Nikkei 225'],
    complexity: 'simple',
  },
  market_cap_weighted: {
    name: 'Market Cap Weighted',
    description: 'Weighted by market capitalization. Larger companies have more influence.',
    formula: 'Index = Σ(Price × Shares) / Base Market Cap × Base Value',
    examples: ['S&P 500', 'NASDAQ Composite', 'FTSE 100'],
    complexity: 'moderate',
  },
  equal_weighted: {
    name: 'Equal-Weighted',
    description: 'Each constituent has the same weight regardless of size or price.',
    formula: 'Index = (1/n) × Σ(Price Changes) × Base Value',
    examples: ['S&P 500 Equal Weight Index', 'Value Line Geometric'],
    complexity: 'simple',
  },
  float_adjusted: {
    name: 'Float-Adjusted',
    description: 'Weighted by free-float market cap (excludes closely-held shares).',
    formula: 'Index = Σ(Price × Float Shares) / Float Market Cap × Base Value',
    examples: ['S&P 500 (modern)', 'MSCI Indices', 'Russell Indices'],
    complexity: 'moderate',
  },
  fundamental_weighted: {
    name: 'Fundamental-Weighted',
    description: 'Weighted by fundamental factors like earnings, revenue, book value.',
    formula: 'Weight = Fundamental Score / Total Fundamental Scores',
    examples: ['FTSE RAFI Index', 'WisdomTree Indices'],
    complexity: 'complex',
  },
  modified_market_cap: {
    name: 'Modified Market Cap',
    description: 'Market cap weighted with adjustments for sector balance or liquidity.',
    formula: 'Index = Adjusted Market Cap Weighted with Constraints',
    examples: ['Many sector-specific indices'],
    complexity: 'moderate',
  },
  factor_weighted: {
    name: 'Factor-Weighted',
    description: 'Weighted by factor exposures (momentum, value, quality, volatility).',
    formula: 'Weight = Factor Score / Total Factor Scores',
    examples: ['MSCI Factor Indices', 'Smart Beta ETFs'],
    complexity: 'complex',
  },
  risk_parity: {
    name: 'Risk Parity',
    description: 'Weights based on inverse volatility - lower volatility stocks get higher weight.',
    formula: 'Weight = (1/σ) / Σ(1/σ)',
    examples: ['Risk Parity Funds', 'Low Volatility Indices'],
    complexity: 'complex',
  },
  geometric_mean: {
    name: 'Geometric Mean',
    description: 'Uses geometric average of price relatives, better for long-term comparison.',
    formula: 'Index = Base × (Π(P₁/P₀))^(1/n)',
    examples: ['Value Line Arithmetic Index', 'Financial Times Ordinary Index'],
    complexity: 'moderate',
  },
  capped_weighted: {
    name: 'Capped-Weighted',
    description: 'Market cap weighted with maximum weight limits per constituent.',
    formula: 'Index = Market Cap Weighted with max(weight) = Cap%',
    examples: ['CAC 40 Capped', 'Many ETF benchmarks'],
    complexity: 'moderate',
  },
};

// Form state for creating/editing an index
export interface IndexFormState {
  name: string;
  description: string;
  calculation_method: IndexCalculationMethod;
  base_value: number;
  cap_weight: number;
  currency: string;
  constituents: IndexConstituentConfig[];
}

// Default form state
export const DEFAULT_INDEX_FORM: IndexFormState = {
  name: '',
  description: '',
  calculation_method: 'equal_weighted',
  base_value: 100,
  cap_weight: 0,  // 0 means no cap
  currency: 'USD',
  constituents: [],
};
