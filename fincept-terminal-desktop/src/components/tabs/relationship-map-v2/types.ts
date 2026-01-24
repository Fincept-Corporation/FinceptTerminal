// Relationship Map V2 - Type Definitions
// Multi-layer corporate intelligence visualization

// ============================================================================
// DATA SOURCE TYPES
// ============================================================================

export interface CompanyInfoV2 {
  ticker: string;
  name: string;
  cik: string;
  industry: string;
  sic: string;
  sector: string;
  address: string;
  exchange: string;
  employees: number;
  website: string;
  description: string;
  market_cap: number;
  current_price: number;
  // Valuation metrics
  pe_ratio: number;
  forward_pe: number;
  price_to_book: number;
  price_to_sales: number;
  peg_ratio: number;
  enterprise_value: number;
  enterprise_to_revenue: number;
  // Profitability
  profit_margins: number;
  operating_margins: number;
  roe: number;
  roa: number;
  // Growth
  revenue_growth: number;
  earnings_growth: number;
  // Financials
  revenue: number;
  gross_profit: number;
  ebitda: number;
  free_cashflow: number;
  total_cash: number;
  total_debt: number;
  // Ownership
  insider_percent: number;
  institutional_percent: number;
  shares_outstanding: number;
  float_shares: number;
  // Analyst
  recommendation: string;
  target_price: number;
  analyst_count: number;
}

export interface InstitutionalHolder {
  name: string;
  shares: number;
  value: number;
  percentage: number;
  change_shares?: number;
  change_percent?: number;
  filing_date?: string;
  fund_family?: string;
}

export interface FundFamily {
  name: string;
  total_shares: number;
  total_value: number;
  total_percentage: number;
  funds: InstitutionalHolder[];
  change_trend: 'increasing' | 'decreasing' | 'stable';
}

export interface InsiderHolder {
  name: string;
  title: string;
  shares: number;
  value: number;
  percentage: number;
  last_transaction_date?: string;
  last_transaction_type?: 'buy' | 'sell' | 'option_exercise';
}

export interface InsiderTransaction {
  name: string;
  title: string;
  transaction_type: 'buy' | 'sell' | 'option_exercise';
  shares: number;
  value: number;
  price: number;
  date: string;
  shares_remaining: number;
}

export interface PeerCompany {
  ticker: string;
  name: string;
  market_cap: number;
  pe_ratio: number;
  price_to_book: number;
  price_to_sales: number;
  roe: number;
  revenue_growth: number;
  profit_margins: number;
  current_price: number;
  sector: string;
  industry: string;
}

export interface ValuationSignal {
  status: 'UNDERVALUED' | 'POTENTIALLY_UNDERVALUED' | 'FAIRLY_VALUED' | 'POTENTIALLY_OVERVALUED' | 'OVERVALUED';
  action: 'BUY' | 'HOLD' | 'SELL';
  confidence: 'HIGH' | 'MEDIUM' | 'LOW';
  score: number; // -3 to +3
  reasoning: string[];
  percentiles: {
    pe: number;
    pb: number;
    ps: number;
    roe: number;
    growth: number;
  };
}

export interface CorporateEventV2 {
  filing_date: string;
  period_of_report?: string;
  form: string;
  filing_url?: string;
  accession_number: string;
  items: string[];
  description: string;
  category: 'earnings' | 'management' | 'merger' | 'restructuring' | 'governance' | 'other';
}

export interface FilingV2 {
  form: string;
  filing_date: string;
  description: string;
  url?: string;
  period?: string;
}

export interface Executive {
  name: string;
  role: string;
  compensation?: number;
}

// ============================================================================
// GRAPH NODE TYPES
// ============================================================================

export type NodeCategory =
  | 'company'
  | 'peer'
  | 'institutional'
  | 'fund_family'
  | 'insider'
  | 'event'
  | 'supply_chain'
  | 'metrics';

export interface GraphNodeData {
  id: string;
  category: NodeCategory;
  label: string;
  // Company node data
  company?: CompanyInfoV2;
  valuation?: ValuationSignal;
  executives?: Executive[];
  // Peer node data
  peer?: PeerCompany;
  peerValuation?: ValuationSignal;
  // Institutional node data
  holder?: InstitutionalHolder;
  // Fund family node data
  fundFamily?: FundFamily;
  // Insider node data
  insider?: InsiderHolder;
  transactions?: InsiderTransaction[];
  // Event node data
  event?: CorporateEventV2;
  // Metrics cluster data
  metrics?: Record<string, number | string>;
  metricsType?: 'financials' | 'valuation' | 'ownership' | 'balance_sheet';
}

// ============================================================================
// GRAPH EDGE TYPES
// ============================================================================

export type EdgeCategory =
  | 'ownership'
  | 'peer'
  | 'supply_chain'
  | 'event'
  | 'internal';

export interface GraphEdgeData {
  category: EdgeCategory;
  label?: string;
  // Ownership edge
  percentage?: number;
  shares?: number;
  value?: number;
  change?: number;
  // Peer edge
  correlation?: number;
  // Supply chain edge
  relationship?: 'customer' | 'supplier';
}

// ============================================================================
// LAYOUT TYPES
// ============================================================================

export type LayoutMode =
  | 'layered'
  | 'force'
  | 'hierarchical'
  | 'circular'
  | 'radial'
  | 'custom';

export interface LayoutConfig {
  mode: LayoutMode;
  centerX: number;
  centerY: number;
  width: number;
  height: number;
  padding: number;
}

// ============================================================================
// FILTER TYPES
// ============================================================================

export interface FilterState {
  showInstitutional: boolean;
  showInsiders: boolean;
  showPeers: boolean;
  showEvents: boolean;
  showMetrics: boolean;
  showFundFamilies: boolean;
  minOwnership: number; // 0-100%
  peerFilter: 'all' | 'undervalued' | 'overvalued';
  eventTypes: string[];
}

export const DEFAULT_FILTERS: FilterState = {
  showInstitutional: true,
  showInsiders: true,
  showPeers: true,
  showEvents: true,
  showMetrics: true,
  showFundFamilies: true,
  minOwnership: 0,
  peerFilter: 'all',
  eventTypes: [],
};

// ============================================================================
// COMPLETE DATA STRUCTURE
// ============================================================================

export interface RelationshipMapDataV2 {
  company: CompanyInfoV2;
  executives: Executive[];
  institutionalHolders: InstitutionalHolder[];
  fundFamilies: FundFamily[];
  insiderHolders: InsiderHolder[];
  insiderTransactions: InsiderTransaction[];
  peers: PeerCompany[];
  peerValuations: Map<string, ValuationSignal>;
  corporateEvents: CorporateEventV2[];
  filings: FilingV2[];
  companyValuation: ValuationSignal;
  timestamp: string;
  dataQuality: DataQualityScore;
}

export interface DataQualityScore {
  overall: number; // 0-100
  sources: {
    edgar_company: boolean;
    edgar_filings: boolean;
    edgar_proxy: boolean;
    edgar_institutional: boolean;
    edgar_insider: boolean;
    yfinance: boolean;
    peers: boolean;
  };
  completeness: number; // 0-100 based on how many fields have data
}

// ============================================================================
// LOADING STATE
// ============================================================================

export interface LoadingPhase {
  phase: 'idle' | 'company' | 'ownership' | 'peers' | 'supplementary' | 'complete' | 'error';
  progress: number; // 0-100
  message: string;
  partialData?: Partial<RelationshipMapDataV2>;
}

// ============================================================================
// EXPORT TYPES
// ============================================================================

export type ExportFormat = 'svg' | 'png' | 'csv' | 'json';
