/**
 * TypeScript types for Alternative Investments Analytics
 * Maps to Python backend analyzer types
 */

// ============================================================================
// Base Types
// ============================================================================

export type AssetClass =
  | 'private_equity'
  | 'private_debt'
  | 'real_estate'
  | 'reit'
  | 'infrastructure'
  | 'commodities'
  | 'timberland'
  | 'farmland'
  | 'raw_land'
  | 'hedge_fund'
  | 'digital_assets'
  | 'fixed_income'
  | 'equity'
  | 'alternative';

export type InvestmentMethod = 'direct' | 'co_investment' | 'fund';

export type CreditRating = 'AAA' | 'AA' | 'A' | 'BBB' | 'BB' | 'B' | 'CCC' | 'CC' | 'C' | 'D';

export type HedgeFundStrategy =
  | 'long_short_equity'
  | 'equity_market_neutral'
  | 'dedicated_short_bias'
  | 'merger_arbitrage'
  | 'distressed_securities'
  | 'activist'
  | 'special_situations'
  | 'fixed_income_arbitrage'
  | 'convertible_arbitrage'
  | 'asset_backed_securities'
  | 'volatility_arbitrage'
  | 'global_macro'
  | 'cta_managed_futures'
  | 'reinsurance'
  | 'structured_credit'
  | 'multi_strategy'
  | 'fund_of_funds';

export type RealEstateType =
  | 'office'
  | 'retail'
  | 'industrial'
  | 'multifamily'
  | 'hotel'
  | 'mixed_use'
  | 'land';

export type CommoditySector = 'energy' | 'metals' | 'agriculture' | 'livestock';

export type MetalType = 'gold' | 'silver' | 'platinum' | 'palladium';

// ============================================================================
// Request/Response Types
// ============================================================================

export interface ApiResponse<T> {
  success: boolean;
  data?: T;
  error?: string;
  status_code?: number;
}

export interface AnalysisResult {
  success: boolean;
  method: string;
  metrics: any;
  error?: string;
}

// ============================================================================
// Asset Parameters
// ============================================================================

export interface AssetParameters {
  asset_class: AssetClass;
  ticker?: string;
  name?: string;
  currency?: string;
  market_region?: string;
  inception_date?: string;
  management_fee?: number;
  performance_fee?: number;
  hurdle_rate?: number;
  high_water_mark?: boolean;
  lock_up_period?: number; // months
  redemption_frequency?: string;
  minimum_investment?: number;
}

// ============================================================================
// Bonds
// ============================================================================

export interface HighYieldBondParams extends AssetParameters {
  face_value?: number;
  coupon_rate: number;
  current_price: number;
  credit_rating: CreditRating;
  maturity_years: number;
  sector?: string;
}

export interface EmergingMarketBondParams extends AssetParameters {
  face_value?: number;
  coupon_rate: number;
  current_price: number;
  maturity_years: number;
  country: string;
  credit_rating?: CreditRating;
  currency_denomination?: string;
}

export interface ConvertibleBondParams extends AssetParameters {
  bond_price: number;
  conversion_ratio: number;
  stock_price: number;
  conversion_premium?: number;
  yield_to_maturity?: number;
  maturity_years: number;
}

export interface PreferredStockParams extends AssetParameters {
  dividend_rate: number;
  par_value: number;
  current_price: number;
  callable?: boolean;
  call_price?: number;
  cumulative?: boolean;
}

// ============================================================================
// Real Estate
// ============================================================================

export interface RealEstateParams extends AssetParameters {
  property_type: RealEstateType;
  acquisition_price: number;
  current_market_value?: number;
  annual_noi?: number; // Net Operating Income
  cap_rate?: number;
  ltv_ratio?: number; // Loan-to-Value
  debt_service?: number;
}

export interface REITParams extends AssetParameters {
  share_price: number;
  dividend_yield: number;
  ffo_per_share?: number; // Funds From Operations
  nav_per_share?: number; // Net Asset Value
  occupancy_rate?: number;
  debt_to_equity?: number;
}

// ============================================================================
// Hedge Funds
// ============================================================================

export interface HedgeFundParams extends AssetParameters {
  strategy: HedgeFundStrategy;
  gross_exposure?: number;
  net_exposure?: number;
  leverage?: number;
  lock_up_period: number;
  redemption_frequency: string;
}

export interface ManagedFuturesParams extends AssetParameters {
  strategy_type: 'systematic' | 'discretionary' | 'hybrid';
  leverage_ratio?: number;
  average_holding_period?: number; // days
  market_sectors?: string[];
}

// ============================================================================
// Commodities
// ============================================================================

export interface CommodityParams extends AssetParameters {
  commodity_type: string;
  sector: CommoditySector;
  spot_price: number;
  futures_price?: number;
  storage_cost?: number;
  convenience_yield?: number;
}

export interface PreciousMetalsParams extends AssetParameters {
  metal_type: 'gold' | 'silver' | 'platinum' | 'palladium';
  investment_method: 'physical' | 'etf' | 'mining_stocks' | 'futures';
  spot_price?: number;
}

export interface NaturalResourceParams extends AssetParameters {
  resource_type: string;
  purchase_price: number;
  current_value: number;
  annual_yield: number;
}

export interface MarketNeutralParams extends AssetParameters {
  annual_return: number;
  volatility: number;
  market_beta: number;
  management_fee: number;
}

export interface InflationProtectedParams extends AssetParameters {
  bond_type: string;
  face_value: number;
  real_yield?: number;
  maturity_years?: number;
  current_cpi?: number;
  fixed_rate?: number;
  current_inflation_rate?: number;
  holding_period_months?: number;
  credited_rate?: number;
  book_value?: number;
  market_value?: number;
}

export interface StableValueParams extends AssetParameters {
  face_value: number;
  credited_rate: number;
  book_value: number;
  market_value: number;
}

// ============================================================================
// Private Capital
// ============================================================================

export interface PrivateEquityParams extends AssetParameters {
  investment_type: 'venture' | 'growth' | 'buyout' | 'distressed';
  fund_size?: number;
  vintage_year?: number;
  capital_called?: number;
  distributions?: number;
  nav?: number; // Net Asset Value
}

// ============================================================================
// Annuities
// ============================================================================

export interface FixedAnnuityParams extends AssetParameters {
  premium: number;
  fixed_rate: number;
  term_years: number;
  surrender_period?: number;
  surrender_charge?: number;
}

export interface VariableAnnuityParams extends AssetParameters {
  premium: number;
  me_fee: number; // Mortality & Expense
  investment_fee: number;
  admin_fee: number;
  surrender_period: number;
}

export interface EquityIndexedAnnuityParams extends AssetParameters {
  premium: number;
  participation_rate: number;
  cap_rate?: number;
  floor_rate?: number;
  term_years: number;
}

// ============================================================================
// Structured Products
// ============================================================================

export interface StructuredProductParams extends AssetParameters {
  principal: number;
  participation_rate: number;
  cap_rate?: number;
  maturity_years: number;
  underlying_index?: string;
  protection_level?: number; // % of principal protected
}

export interface LeveragedFundParams extends AssetParameters {
  leverage_multiple: number; // 2x, 3x, etc.
  daily_volatility: number;
  expense_ratio: number;
  underlying_index: string;
}

// ============================================================================
// Inflation Protected
// ============================================================================

export interface TIPSParams extends AssetParameters {
  face_value: number;
  real_yield: number;
  inflation_rate: number;
  maturity_years: number;
  current_price?: number;
}

export interface IBondParams extends AssetParameters {
  purchase_amount: number;
  fixed_rate: number;
  inflation_rate: number;
  composite_rate?: number;
  holding_years?: number;
}

// ============================================================================
// Strategies
// ============================================================================

export interface CoveredCallParams extends AssetParameters {
  stock_price: number;
  shares_owned: number;
  strike_price: number;
  option_premium: number;
  days_to_expiration: number;
}

export interface AssetLocationParams {
  tax_bracket: number;
  portfolio_value: number;
  taxable_account_value?: number;
  tax_deferred_value?: number;
  tax_free_value?: number;
}

export interface SRIFundParams extends AssetParameters {
  expense_ratio: number;
  benchmark_expense?: number;
  fund_return?: number;
  benchmark_return?: number;
  num_holdings?: number;
  benchmark_holdings?: number;
  negative_screens?: string[];
  positive_screens?: string[];
}

// ============================================================================
// Crypto/Digital Assets
// ============================================================================

export interface DigitalAssetParams extends AssetParameters {
  asset_type: 'cryptocurrency' | 'token' | 'nft' | 'defi';
  current_price: number;
  market_cap?: number;
  circulating_supply?: number;
  max_supply?: number;
  blockchain?: string;
}

// ============================================================================
// Analysis Results
// ============================================================================

export interface InvestmentVerdict {
  category: 'THE GOOD' | 'THE BAD' | 'THE FLAWED' | 'THE UGLY' | 'MIXED';
  rating: string; // e.g., "8/10"
  analysis_summary: string;
  key_findings: string[];
  recommendation: string;
  when_acceptable?: string;
  better_alternatives?: {
    portfolio: string;
    benefits: string[];
  };
}

export interface PerformanceMetrics {
  average_return?: number;
  volatility?: number;
  sharpe_ratio?: number;
  max_drawdown?: number;
  alpha?: number;
  beta?: number;
}

export interface RiskMetrics {
  var_95?: number; // Value at Risk
  cvar_95?: number; // Conditional VaR
  volatility?: number;
  downside_deviation?: number;
  sortino_ratio?: number;
}

export interface FeeAnalysis {
  management_fee: number;
  performance_fee?: number;
  total_expense_ratio?: number;
  fee_impact_10yr?: number;
  comparison_to_alternatives?: any;
}

// ============================================================================
// Category Definitions
// ============================================================================

export interface InvestmentCategory {
  id: string;
  name: string;
  description: string;
  icon: string;
  analyzers: string[];
  color: string;
}

export const INVESTMENT_CATEGORIES: InvestmentCategory[] = [
  {
    id: 'bonds',
    name: 'Bonds & Fixed Income',
    description: 'High-yield, emerging market, convertible, and preferred stock analysis',
    icon: 'TrendingUp',
    analyzers: ['high-yield', 'em-bonds', 'convertible-bonds', 'preferred-stocks'],
    color: 'blue',
  },
  {
    id: 'real-estate',
    name: 'Real Estate',
    description: 'Direct real estate, REITs, and infrastructure investments',
    icon: 'Building',
    analyzers: ['real-estate', 'reit', 'infrastructure'],
    color: 'green',
  },
  {
    id: 'hedge-funds',
    name: 'Hedge Funds',
    description: 'Hedge funds, managed futures, and market-neutral strategies',
    icon: 'Shield',
    analyzers: ['hedge-funds', 'managed-futures', 'market-neutral'],
    color: 'purple',
  },
  {
    id: 'commodities',
    name: 'Commodities',
    description: 'Precious metals, natural resources, and commodity investments',
    icon: 'Gem',
    analyzers: ['precious-metals', 'commodities'],
    color: 'yellow',
  },
  {
    id: 'private-capital',
    name: 'Private Capital',
    description: 'Private equity, venture capital, and private debt',
    icon: 'Lock',
    analyzers: ['private-equity'],
    color: 'red',
  },
  {
    id: 'annuities',
    name: 'Annuities',
    description: 'Fixed, variable, and equity-indexed annuities',
    icon: 'Calendar',
    analyzers: ['fixed-annuities', 'variable-annuities', 'equity-indexed-annuities'],
    color: 'orange',
  },
  {
    id: 'structured',
    name: 'Structured Products',
    description: 'Structured products and leveraged funds',
    icon: 'Package',
    analyzers: ['structured-products', 'leveraged-funds'],
    color: 'pink',
  },
  {
    id: 'inflation-protected',
    name: 'Inflation Protected',
    description: 'TIPS and I Bonds for inflation protection',
    icon: 'Shield',
    analyzers: ['tips', 'i-bonds'],
    color: 'cyan',
  },
  {
    id: 'strategies',
    name: 'Strategies',
    description: 'Covered calls, asset location, and SRI funds',
    icon: 'Target',
    analyzers: ['covered-calls', 'asset-location', 'sri-funds'],
    color: 'indigo',
  },
  {
    id: 'crypto',
    name: 'Digital Assets',
    description: 'Cryptocurrency and digital asset analysis',
    icon: 'Bitcoin',
    analyzers: ['digital-assets'],
    color: 'violet',
  },
];
