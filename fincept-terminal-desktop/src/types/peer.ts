// TypeScript types for Peer Comparison & Benchmarking
// CFA-compliant peer analysis types

// ============================================================================
// GICS Classification Types
// ============================================================================

export type GICSSector =
  | 'Energy'
  | 'Materials'
  | 'Industrials'
  | 'Consumer Discretionary'
  | 'Consumer Staples'
  | 'Health Care'
  | 'Financials'
  | 'Information Technology'
  | 'Communication Services'
  | 'Utilities'
  | 'Real Estate';

export type MarketCapCategory =
  | 'Mega Cap'    // > $200B
  | 'Large Cap'   // $10B - $200B
  | 'Mid Cap'     // $2B - $10B
  | 'Small Cap'   // $300M - $2B
  | 'Micro Cap'   // $50M - $300M
  | 'Nano Cap';   // < $50M

export type MetricCategory =
  | 'valuation'
  | 'profitability'
  | 'growth'
  | 'leverage'
  | 'liquidity'
  | 'efficiency';

// ============================================================================
// Peer Company Types
// ============================================================================

export interface PeerCompany {
  symbol: string;
  name: string;
  sector: string;
  industry: string;
  marketCap: number;
  marketCapCategory: MarketCapCategory;
  similarityScore: number; // 0.0 to 1.0
  matchDetails: MatchDetails;
}

export interface MatchDetails {
  sectorMatch: boolean;
  industryMatch: boolean;
  marketCapSimilarity: number; // 0.0 to 1.0
  geographicMatch: boolean;
}

// ============================================================================
// Financial Metrics Types
// ============================================================================

export interface ValuationMetrics {
  peRatio?: number;
  pbRatio?: number;
  psRatio?: number;
  pcfRatio?: number;
  evToEbitda?: number;
  pegRatio?: number;
}

export interface ProfitabilityMetrics {
  roe?: number;          // Return on Equity
  roa?: number;          // Return on Assets
  roic?: number;         // Return on Invested Capital
  grossMargin?: number;
  operatingMargin?: number;
  netMargin?: number;
}

export interface GrowthMetrics {
  revenueGrowth?: number;
  earningsGrowth?: number;
  fcfGrowth?: number;
}

export interface LeverageMetrics {
  debtToEquity?: number;
  debtToAssets?: number;
  interestCoverage?: number;
}

export interface CompanyMetrics {
  symbol: string;
  name: string;
  valuation: ValuationMetrics;
  profitability: ProfitabilityMetrics;
  growth: GrowthMetrics;
  leverage: LeverageMetrics;
}

// ============================================================================
// Comparison & Benchmarking Types
// ============================================================================

export interface PeerComparisonResult {
  target: CompanyMetrics;
  peers: CompanyMetrics[];
  benchmarks: SectorBenchmarks;
  percentileRanks?: Record<string, PercentileRanking>;
  summary?: ComparisonSummary;
}

export interface SectorBenchmarks {
  sector: string;
  medianPe?: number;
  medianPb?: number;
  medianRoe?: number;
  medianRevenueGrowth?: number;
  medianDebtToEquity?: number;
}

export interface MetricBenchmarks {
  metricName: string;
  sectorMedian: number;
  sectorMean: number;
  sectorStdDev: number;
  topQuartile: number;
  bottomQuartile: number;
}

export interface PercentileRanking {
  metricName: string;
  value: number;
  percentile: number;      // 0.0 to 100.0
  zScore: number;          // Standard deviations from mean
  peerMedian: number;
  peerMean: number;
  peerStdDev: number;
}

export interface ValuationStatus {
  status: 'Undervalued' | 'Fairly Valued' | 'Overvalued' | 'Potentially Undervalued' | 'Potentially Overvalued';
  confidence: number; // 0.0 to 1.0
  reasoning: string[];
}

export interface ComparisonSummary {
  valuationScore: number;
  profitabilityScore: number;
  growthScore: number;
  overallRating: number;
}

// ============================================================================
// Request/Response Types for Tauri Commands
// ============================================================================

export interface PeerSearchInput {
  symbol: string;
  sector: string;
  industry: string;
  marketCap: number;
  minSimilarity?: number;
  maxPeers?: number;
}

export interface ComparisonInput {
  targetSymbol: string;
  peerSymbols: string[];
  metrics: string[];
}

export interface PercentileInput {
  symbol: string;
  metricName: string;
  metricValue: number;
  peerValues: number[];
}

export interface PeerCommandResponse<T> {
  success: boolean;
  data?: T;
  error?: string;
}

export interface MetricRanking {
  symbol: string;
  ranking: PercentileRanking;
  valuationStatus?: ValuationStatus;
}

// ============================================================================
// UI State Types
// ============================================================================

export interface PeerComparisonState {
  targetSymbol: string;
  selectedPeers: PeerCompany[];
  availablePeers: PeerCompany[];
  comparisonData?: PeerComparisonResult;
  loading: boolean;
  error?: string;
}

export type ViewMode = 'table' | 'chart' | 'radar' | 'heatmap';

export interface ChartDataPoint {
  metric: string;
  target: number;
  peer: number;
  benchmark: number;
}

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * Format a metric value for display
 */
export function formatMetricValue(value: number | undefined, format: 'percentage' | 'ratio' | 'currency' | 'number'): string {
  if (value === undefined || value === null) return 'N/A';

  switch (format) {
    case 'percentage':
      return `${(value * 100).toFixed(2)}%`;
    case 'ratio':
      return value.toFixed(2);
    case 'currency':
      return new Intl.NumberFormat('en-US', {
        style: 'currency',
        currency: 'USD',
        notation: value >= 1e9 ? 'compact' : 'standard',
        maximumFractionDigits: 2,
      }).format(value);
    case 'number':
      return value.toLocaleString('en-US', { maximumFractionDigits: 2 });
    default:
      return value.toString();
  }
}

/**
 * Get color based on percentile ranking
 */
export function getPercentileColor(percentile: number): string {
  if (percentile >= 75) return '#10b981'; // green-500
  if (percentile >= 50) return '#3b82f6'; // blue-500
  if (percentile >= 25) return '#f59e0b'; // amber-500
  return '#ef4444'; // red-500
}

/**
 * Get market cap category from value
 */
export function getMarketCapCategory(marketCap: number): MarketCapCategory {
  if (marketCap > 200_000_000_000) return 'Mega Cap';
  if (marketCap > 10_000_000_000) return 'Large Cap';
  if (marketCap > 2_000_000_000) return 'Mid Cap';
  if (marketCap > 300_000_000) return 'Small Cap';
  if (marketCap > 50_000_000) return 'Micro Cap';
  return 'Nano Cap';
}

/**
 * Get all metric names from a company
 */
export function getAllMetricNames(company: CompanyMetrics): string[] {
  const metrics: string[] = [];

  // Valuation metrics
  if (company.valuation.peRatio) metrics.push('P/E Ratio');
  if (company.valuation.pbRatio) metrics.push('P/B Ratio');
  if (company.valuation.psRatio) metrics.push('P/S Ratio');
  if (company.valuation.evToEbitda) metrics.push('EV/EBITDA');

  // Profitability metrics
  if (company.profitability.roe) metrics.push('ROE');
  if (company.profitability.roa) metrics.push('ROA');
  if (company.profitability.netMargin) metrics.push('Net Margin');

  // Growth metrics
  if (company.growth.revenueGrowth) metrics.push('Revenue Growth');
  if (company.growth.earningsGrowth) metrics.push('Earnings Growth');

  // Leverage metrics
  if (company.leverage.debtToEquity) metrics.push('Debt/Equity');

  return metrics;
}

/**
 * Extract metric value by name
 */
export function getMetricValue(company: CompanyMetrics, metricName: string): number | undefined {
  const metricMap: Record<string, number | undefined> = {
    'P/E Ratio': company.valuation.peRatio,
    'P/B Ratio': company.valuation.pbRatio,
    'P/S Ratio': company.valuation.psRatio,
    'EV/EBITDA': company.valuation.evToEbitda,
    'PEG Ratio': company.valuation.pegRatio,
    'ROE': company.profitability.roe,
    'ROA': company.profitability.roa,
    'ROIC': company.profitability.roic,
    'Gross Margin': company.profitability.grossMargin,
    'Operating Margin': company.profitability.operatingMargin,
    'Net Margin': company.profitability.netMargin,
    'Revenue Growth': company.growth.revenueGrowth,
    'Earnings Growth': company.growth.earningsGrowth,
    'FCF Growth': company.growth.fcfGrowth,
    'Debt/Equity': company.leverage.debtToEquity,
    'Debt/Assets': company.leverage.debtToAssets,
    'Interest Coverage': company.leverage.interestCoverage,
  };

  return metricMap[metricName];
}
