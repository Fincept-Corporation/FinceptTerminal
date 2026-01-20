// Type definitions for RelationshipMapTab

export interface NodeColors {
  accent: string;
  text: string;
  textMuted: string;
  success: string;
  alert: string;
  panel: string;
}

export interface CompanyNodeData {
  name: string;
  ticker: string;
  industry: string;
  sector: string;
  cik: string;
  exchange: string;
  employees?: number;
  market_cap?: number;
}

export interface DataNodeItem {
  label: string;
  value: string;
  description?: string;
  url?: string;
  allItems?: string[];
}

export type DataNodeType =
  | 'events'
  | 'filings'
  | 'financials'
  | 'balance'
  | 'analysts'
  | 'ownership'
  | 'valuation'
  | 'info'
  | 'executives';

export interface DataNodeData {
  type: DataNodeType;
  title: string;
  items: DataNodeItem[];
  colors: NodeColors;
}

export interface CompanyNodeProps {
  data: CompanyNodeData & { colors: NodeColors };
}

export interface DataNodeProps {
  data: DataNodeData;
}

// Graph builder types
export interface GraphData {
  company: CompanyNodeData;
  corporate_events?: CorporateEventData[];
  filings?: FilingData[];
  financials?: FinancialsData;
  balance_sheet?: BalanceSheetData;
  analysts?: AnalystsData;
  ownership?: OwnershipData;
  valuation?: ValuationData;
  executives?: ExecutiveData[];
}

export interface CorporateEventData {
  filing_date: string;
  items?: string[];
  description?: string;
  filing_url?: string;
}

export interface FilingData {
  form: string;
  description?: string;
  filing_date: string;
}

export interface FinancialsData {
  revenue: number;
  gross_profit: number;
  operating_cashflow: number;
  free_cashflow: number;
  ebitda_margins: number;
  profit_margins: number;
  revenue_growth: number;
  earnings_growth: number;
}

export interface BalanceSheetData {
  total_assets: number;
  total_liabilities: number;
  stockholders_equity: number;
  total_cash: number;
  total_debt: number;
  shares_outstanding: number;
}

export interface AnalystsData {
  recommendation: string;
  recommendation_mean?: number;
  target_price: number;
  target_high: number;
  target_low: number;
  analyst_count: number;
}

export interface OwnershipData {
  insider_percent: number;
  institutional_percent: number;
  float_shares: number;
  shares_outstanding: number;
}

export interface ValuationData {
  pe_ratio: number;
  forward_pe: number;
  price_to_book: number;
  enterprise_value: number;
  enterprise_to_revenue: number;
  peg_ratio: number;
}

export interface ExecutiveData {
  name: string;
  role: string;
}
