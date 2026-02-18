export interface PortfolioFormState {
  name: string;
  owner: string;
  currency: string;
}

export interface AssetFormState {
  symbol: string;
  quantity: string;
  price: string;
}

export interface ModalState {
  showCreatePortfolio: boolean;
  showAddAsset: boolean;
  showSellAsset: boolean;
}

export interface PortfolioTabProps {}

// ─── New types for redesigned UI ───

export type HeatmapMode = 'pnl' | 'weight' | 'dayChg';

export type BottomPanelTab =
  | 'analytics-sectors' | 'perf-risk'
  | 'optimization' | 'quantstats' | 'reports-pme' | 'indices'
  | 'risk-mgmt' | 'planning' | 'economics';

export type DetailView =
  | 'analytics-sectors' | 'perf-risk'
  | 'optimization' | 'quantstats' | 'reports-pme' | 'indices'
  | 'risk-mgmt' | 'planning' | 'economics';

export type SortColumn = 'symbol' | 'price' | 'change' | 'pnl' | 'pnlPct' | 'weight' | 'market_value';

export type SortDirection = 'asc' | 'desc';

export interface ComputedMetrics {
  sharpe: number | null;
  beta: number | null;
  volatility: number | null;
  maxDrawdown: number | null;
  var95: number | null;
  riskScore: number | null;
  concentrationTop3: number | null;
}

// ─── Import / Export types ───

export interface PortfolioExportTransaction {
  date: string;
  symbol: string;
  type: 'BUY' | 'SELL' | 'DIVIDEND' | 'SPLIT';
  quantity: number;
  price: number;
  total_value: number;
  notes: string;
}

export interface PortfolioExportJSON {
  format_version: '1.0';
  portfolio_name: string;
  owner: string;
  currency: string;
  export_date: string;
  transactions: PortfolioExportTransaction[];
}

export type ImportMode = 'new' | 'merge';

export interface ImportResult {
  portfolioId: string;
  portfolioName: string;
  transactionsReplayed: number;
  errors: string[];
}
