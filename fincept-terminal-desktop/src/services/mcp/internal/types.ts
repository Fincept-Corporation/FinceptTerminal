// Internal Terminal MCP Provider - Type Definitions

export interface InternalToolSchema {
  type: 'object';
  properties: Record<string, {
    type: string;
    description: string;
    enum?: string[];
    default?: any;
  }>;
  required?: string[];
}

export interface InternalTool {
  name: string;
  description: string;
  inputSchema: InternalToolSchema;
  handler: (args: Record<string, any>, contexts: TerminalContexts) => Promise<InternalToolResult>;
}

export interface InternalToolResult {
  success: boolean;
  data?: any;
  message?: string;
  error?: string;
}

export interface TerminalContexts {
  // Navigation
  setActiveTab?: (tab: string) => void;
  getActiveTab?: () => string;
  navigateToScreen?: (screen: string) => void;

  // Trading / Brokers
  placeTrade?: (params: any) => Promise<any>;
  cancelOrder?: (orderId: string) => Promise<any>;
  modifyOrder?: (orderId: string, updates: any) => Promise<any>;
  getPositions?: () => Promise<any>;
  getBalance?: () => Promise<any>;
  getOrders?: (filters?: any) => Promise<any>;
  getHoldings?: () => Promise<any>;
  closePosition?: (symbol: string) => Promise<any>;

  // Portfolio
  getPortfolios?: () => Promise<any>;
  createPortfolio?: (params: any) => Promise<any>;
  deletePortfolio?: (id: string) => Promise<any>;
  addAsset?: (portfolioId: string, asset: any) => Promise<any>;
  removeAsset?: (portfolioId: string, assetId: string) => Promise<any>;
  addTransaction?: (portfolioId: string, tx: any) => Promise<any>;
  getPortfolioSummary?: (portfolioId: string) => Promise<any>;

  // Dashboard
  addWidget?: (widgetType: string, config?: any) => void;
  removeWidget?: (widgetId: string) => void;
  getWidgets?: () => any[];
  configureWidget?: (widgetId: string, config: any) => void;

  // Settings
  setTheme?: (theme: string) => void;
  getTheme?: () => string;
  setLanguage?: (lang: string) => void;
  getLanguage?: () => string;
  setSetting?: (key: string, value: any) => Promise<void>;
  getSetting?: (key: string) => Promise<any>;

  // Workspace
  saveWorkspace?: (name: string) => Promise<any>;
  loadWorkspace?: (id: string) => Promise<void>;
  listWorkspaces?: () => Promise<any[]>;
  deleteWorkspace?: (id: string) => Promise<void>;

  // Backtesting
  runBacktest?: (params: any) => Promise<any>;
  optimizeStrategy?: (params: any) => Promise<any>;
  getHistoricalData?: (params: any) => Promise<any>;

  // Market Data
  getQuote?: (symbol: string) => Promise<any>;
  addToWatchlist?: (symbol: string) => Promise<void>;
  removeFromWatchlist?: (symbol: string) => Promise<void>;
  searchSymbol?: (query: string) => Promise<any[]>;

  // Workflow
  createWorkflow?: (params: any) => Promise<any>;
  runWorkflow?: (id: string) => Promise<any>;
  stopWorkflow?: (id: string) => Promise<void>;
  listWorkflows?: () => Promise<any[]>;

  // Notes
  createNote?: (title: string, content: string) => Promise<any>;
  listNotes?: () => Promise<any[]>;
  deleteNote?: (id: string) => Promise<void>;
  generateReport?: (params: any) => Promise<any>;

  // Report Builder
  createReportTemplate?: (params: { title: string; description: string; author?: string; company?: string }) => Promise<any>;
  addReportComponent?: (params: { type: string; content?: string; config?: any }) => Promise<any>;
  updateReportMetadata?: (metadata: Record<string, any>) => Promise<void>;
  saveReportTemplate?: (filename?: string) => Promise<any>;
  loadReportTemplate?: (path: string) => Promise<any>;
  exportReport?: (params: { format: string; filename?: string }) => Promise<any>;
  listReportComponents?: () => Promise<any[]>;
  deleteReportComponent?: (id: string) => Promise<void>;
  applyReportTheme?: (theme: string) => Promise<void>;
}

export const INTERNAL_SERVER_ID = 'fincept-terminal';
export const INTERNAL_SERVER_NAME = 'Fincept Terminal';
