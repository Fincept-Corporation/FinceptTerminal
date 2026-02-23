// Internal Terminal MCP Provider - Type Definitions

export interface InternalToolSchema {
  type: 'object';
  properties: Record<string, {
    type: string;
    description: string;
    enum?: string[];
    default?: any;
    items?: { type: string };
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
  suggestion?: string;
  count?: number;
  // Additional metadata for financial tools
  chart_data?: any;
  ticker?: string;
  company?: string;
  content?: string;
}

export interface TerminalContexts {
  // Navigation
  setActiveTab?: (tab: string) => void;
  getActiveTab?: () => string;
  navigateToScreen?: (screen: string) => void;

  // Trading / Brokers (Crypto)
  placeTrade?: (params: any) => Promise<any>;
  cancelOrder?: (orderId: string) => Promise<any>;
  modifyOrder?: (orderId: string, updates: any) => Promise<any>;
  getPositions?: () => Promise<any>;
  getBalance?: () => Promise<any>;
  getOrders?: (filters?: any) => Promise<any>;
  getHoldings?: () => Promise<any>;
  closePosition?: (symbol: string) => Promise<any>;

  // Stock Trading
  placeStockTrade?: (params: any) => Promise<any>;
  cancelStockOrder?: (orderId: string, variety?: string) => Promise<any>;
  modifyStockOrder?: (params: any) => Promise<any>;
  getStockPositions?: () => Promise<any>;
  getStockHoldings?: () => Promise<any>;
  getStockFunds?: () => Promise<any>;
  getStockOrders?: (filters?: any) => Promise<any>;
  getStockTrades?: () => Promise<any>;
  getStockMarginRequired?: (params: any) => Promise<any>;
  convertStockPosition?: (params: any) => Promise<any>;
  exitStockPosition?: (params: any) => Promise<any>;
  getStockQuote?: (params: any) => Promise<any>;
  getStockMarketDepth?: (params: any) => Promise<any>;
  searchStockInstruments?: (query: string, exchange?: string) => Promise<any[]>;
  getStockHistoricalData?: (params: any) => Promise<any>;

  // Portfolio - Core CRUD
  getPortfolios?: () => Promise<any>;
  createPortfolio?: (params: any) => Promise<any>;
  deletePortfolio?: (id: string) => Promise<any>;
  getPortfolio?: (portfolioId: string) => Promise<any>;

  // Portfolio - Asset Management
  addAsset?: (portfolioId: string, asset: any) => Promise<any>;
  removeAsset?: (portfolioId: string, symbol: string, sellPrice?: number) => Promise<any>;
  sellAsset?: (portfolioId: string, symbol: string, quantity: number, price: number) => Promise<any>;
  getPortfolioAssets?: (portfolioId: string) => Promise<any[]>;

  // Portfolio - Transactions
  addTransaction?: (portfolioId: string, tx: any) => Promise<any>;
  getPortfolioTransactions?: (portfolioId: string, limit?: number) => Promise<any[]>;
  getSymbolTransactions?: (portfolioId: string, symbol: string) => Promise<any[]>;

  // Portfolio - Summary & Analytics
  getPortfolioSummary?: (portfolioId: string) => Promise<any>;
  calculateAdvancedMetrics?: (portfolioId: string) => Promise<any>;
  calculateRiskMetrics?: (portfolioId: string) => Promise<any>;

  // Portfolio - Optimization
  optimizePortfolioWeights?: (portfolioId: string, method?: string) => Promise<any>;
  getRebalancingRecommendations?: (portfolioId: string, targetAllocations: Record<string, number>) => Promise<any[]>;

  // Portfolio - Analysis
  analyzeDiversification?: (portfolioId: string) => Promise<any>;
  getPortfolioPerformanceHistory?: (portfolioId: string, limit?: number) => Promise<any[]>;

  // Portfolio - Transaction management
  updateTransaction?: (transactionId: string, quantity: number, price: number, transactionDate: string, notes?: string) => Promise<void>;
  deleteTransaction?: (transactionId: string) => Promise<void>;
  getTransactionById?: (transactionId: string) => Promise<any | null>;

  // Portfolio - Export / Import
  exportPortfolioCSV?: (portfolioId: string) => Promise<string>;
  exportPortfolioJSON?: (portfolioId: string) => Promise<string>;
  importPortfolio?: (data: any, mode: 'new' | 'merge', mergeTargetId?: string) => Promise<any>;

  // Portfolio - Snapshot
  savePortfolioSnapshot?: (portfolioId: string) => Promise<any>;

  // Portfolio - Snapshot (rich combined context for agents)
  getPortfolioSnapshot?: (portfolioId: string) => Promise<{ summary: any; metrics: any; diversification: any; fetched_at: string }>;

  // Custom Index
  createCustomIndex?: (params: any) => Promise<any>;
  getAllCustomIndices?: () => Promise<any[]>;
  getCustomIndexSummary?: (indexId: string) => Promise<any>;
  deleteCustomIndex?: (indexId: string) => Promise<void>;
  addIndexConstituent?: (indexId: string, config: any) => Promise<any>;
  removeIndexConstituent?: (indexId: string, symbol: string) => Promise<void>;
  getIndexConstituents?: (indexId: string) => Promise<any[]>;
  saveIndexSnapshot?: (indexId: string) => Promise<any>;
  getIndexSnapshots?: (indexId: string, limit?: number) => Promise<any[]>;
  createIndexFromPortfolio?: (portfolioId: string, indexName: string, method: string, baseValue: number) => Promise<any>;
  rebaseCustomIndex?: (indexId: string, newBaseValue: number) => Promise<void>;

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
  resolveSymbol?: (query: string) => Promise<{ symbol: string; price?: number; matches: string[] }>;

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

  // Node Editor - Node Discovery
  listAvailableNodes?: (category?: string, search?: string) => Promise<NodeTypeInfo[]>;
  getNodeDetails?: (nodeType: string) => Promise<NodeTypeDetails | null>;
  getNodeCategories?: () => Promise<NodeCategoryInfo[]>;

  // Node Editor - Workflow Creation
  createNodeWorkflow?: (params: CreateWorkflowParams) => Promise<WorkflowInfo>;
  addNodeToWorkflow?: (params: AddNodeParams) => Promise<WorkflowInfo>;
  connectNodes?: (params: ConnectNodesParams) => Promise<WorkflowInfo>;
  configureNode?: (params: ConfigureNodeParams) => Promise<WorkflowInfo>;

  // Node Editor - Workflow Management
  listNodeWorkflows?: (status?: string) => Promise<WorkflowInfo[]>;
  getNodeWorkflow?: (workflowId: string) => Promise<WorkflowInfo | null>;
  updateNodeWorkflow?: (params: UpdateWorkflowParams) => Promise<WorkflowInfo>;
  deleteNodeWorkflow?: (workflowId: string) => Promise<void>;
  duplicateNodeWorkflow?: (workflowId: string, newName: string) => Promise<WorkflowInfo>;

  // Node Editor - Workflow Execution
  executeNodeWorkflow?: (workflowId: string, inputData?: any) => Promise<WorkflowExecutionResult>;
  stopNodeWorkflow?: (workflowId: string) => Promise<void>;
  getWorkflowResults?: (workflowId: string) => Promise<WorkflowExecutionResult | null>;
  validateNodeWorkflow?: (workflowId: string) => Promise<WorkflowValidation>;

  // Node Editor - Helpers
  openWorkflowInEditor?: (workflowId: string) => Promise<void>;
  exportNodeWorkflow?: (workflowId: string) => Promise<any>;
  importNodeWorkflow?: (workflowData: any) => Promise<WorkflowInfo>;

  // Node Editor - Quick Edit Operations
  removeNodeFromWorkflow?: (workflowId: string, nodeId: string) => Promise<WorkflowInfo>;
  disconnectNodes?: (params: DisconnectNodesParams) => Promise<WorkflowInfo>;
  renameNode?: (workflowId: string, nodeId: string, newLabel: string) => Promise<WorkflowInfo>;
  clearWorkflow?: (workflowId: string) => Promise<WorkflowInfo>;
  cloneNode?: (workflowId: string, nodeId: string, newLabel?: string) => Promise<{ workflow: WorkflowInfo; newNodeId: string }>;
  moveNode?: (params: MoveNodeParams) => Promise<{ workflow: WorkflowInfo; newPosition: { x: number; y: number } }>;
  getWorkflowStats?: (workflowId: string) => Promise<WorkflowStats>;
  autoLayoutWorkflow?: (params: AutoLayoutParams) => Promise<{ workflow: WorkflowInfo; nodesRepositioned: number }>;

  // ============================================================================
  // News - Fetching & Filtering
  // ============================================================================
  fetchAllNews?: (forceRefresh?: boolean) => Promise<NewsArticleInfo[]>;
  getNewsByCategory?: (category: string) => Promise<NewsArticleInfo[]>;
  getNewsBySource?: (source: string) => Promise<NewsArticleInfo[]>;
  searchNews?: (query: string) => Promise<NewsArticleInfo[]>;
  getBreakingNews?: () => Promise<NewsArticleInfo[]>;
  getNewsBySentiment?: (sentiment: 'BULLISH' | 'BEARISH' | 'NEUTRAL') => Promise<NewsArticleInfo[]>;
  getNewsByPriority?: (priority: 'FLASH' | 'URGENT' | 'BREAKING' | 'ROUTINE') => Promise<NewsArticleInfo[]>;
  getNewsByTicker?: (ticker: string) => Promise<NewsArticleInfo[]>;

  // News - Providers / Sources
  getActiveSources?: () => Promise<string[]>;
  getRSSFeedCount?: () => Promise<number>;
  getUserRSSFeeds?: () => Promise<UserRSSFeedInfo[]>;
  getDefaultFeeds?: () => Promise<RSSFeedInfo[]>;
  getAllRSSFeeds?: () => Promise<RSSFeedInfo[]>;

  // News - Feed Management
  addUserRSSFeed?: (params: AddRSSFeedParams) => Promise<UserRSSFeedInfo>;
  updateUserRSSFeed?: (id: string, params: UpdateRSSFeedParams) => Promise<void>;
  deleteUserRSSFeed?: (id: string) => Promise<void>;
  toggleUserRSSFeed?: (id: string, enabled: boolean) => Promise<void>;
  toggleDefaultRSSFeed?: (feedId: string, enabled: boolean) => Promise<void>;
  deleteDefaultRSSFeed?: (feedId: string) => Promise<void>;
  restoreDefaultRSSFeed?: (feedId: string) => Promise<void>;
  restoreAllDefaultFeeds?: () => Promise<void>;
  testRSSFeedUrl?: (url: string) => Promise<boolean>;

  // News - Analysis
  analyzeNewsArticle?: (url: string) => Promise<NewsAnalysisResult>;

  // News - Statistics
  getNewsSentimentStats?: () => Promise<NewsSentimentStats>;
  getNewsCacheStats?: () => Promise<NewsCacheStats>;
  getAvailableCategories?: () => string[];
  getAvailableRegions?: () => string[];

  // ============================================================================
  // QuantLib API
  // ============================================================================
  getQuantLibApiKey?: () => string | null;
  setQuantLibApiKey?: (key: string | null) => void;

  // ============================================================================
  // Economics / Fincept Macro API
  // ============================================================================

  // CEIC Economic Series
  getFinceptCeicCountries?: () => Promise<Record<string, any>[]>;
  getFinceptCeicIndicators?: (countrySlug: string) => Promise<Record<string, any>[]>;
  getFinceptCeicSeries?: (params: {
    country: string;
    indicator?: string;
    yearFrom?: number;
    yearTo?: number;
    limit?: number;
  }) => Promise<Record<string, any>[]>;

  // Economic Calendar
  getFinceptEconomicCalendar?: (params: {
    startDate?: string;
    endDate?: string;
    limit?: number;
  }) => Promise<Record<string, any>[]>;
  getFinceptUpcomingEvents?: (limit?: number) => Promise<Record<string, any>[]>;

  // World Government Bonds
  getFinceptCentralBankRates?: () => Promise<Record<string, any>[]>;
  getFinceptCreditRatings?: () => Promise<Record<string, any>[]>;
  getFinceptSovereignCds?: () => Promise<Record<string, any>[]>;
  getFinceptBondSpreads?: () => Promise<Record<string, any>[]>;
  getFinceptInvertedYields?: () => Promise<Record<string, any>[]>;

  // ============================================================================
  // Data Mapping
  // ============================================================================

  // CRUD
  listDataMappings?: () => Promise<DataMappingSummary[]>;
  getDataMapping?: (id: string) => Promise<DataMappingDetail | null>;
  createDataMapping?: (params: CreateDataMappingParams) => Promise<DataMappingSummary>;
  updateDataMapping?: (id: string, updates: Partial<CreateDataMappingParams>) => Promise<DataMappingSummary>;
  deleteDataMapping?: (id: string) => Promise<void>;
  duplicateDataMapping?: (id: string) => Promise<DataMappingSummary>;

  // Execution
  testDataMapping?: (id: string, parameters?: Record<string, string>) => Promise<DataMappingExecutionResult>;
  executeDataMapping?: (id: string, parameters?: Record<string, string>) => Promise<DataMappingExecutionResult>;

  // Templates
  listMappingTemplates?: () => Promise<DataMappingTemplateSummary[]>;
  applyMappingTemplate?: (templateId: string, overrides?: { name?: string; description?: string }) => Promise<DataMappingSummary>;

  // Cache
  clearDataMappingCache?: (mappingId?: string) => Promise<void>;

  // Schemas
  listMappingSchemas?: () => Promise<DataMappingSchemaSummary[]>;
}

// ============================================================================
// Node Editor Types
// ============================================================================

export interface NodeTypeInfo {
  name: string;
  displayName: string;
  description: string;
  category: string;
  icon?: string;
  color?: string;
  inputs: string[];
  outputs: string[];
}

export interface NodeTypeDetails extends NodeTypeInfo {
  version: number;
  properties: NodePropertyInfo[];
  hints?: string[];
  usableAsTool?: boolean;
}

export interface NodePropertyInfo {
  name: string;
  displayName: string;
  type: string;
  default?: any;
  description?: string;
  required?: boolean;
  options?: Array<{ name: string; value: any; description?: string }>;
}

export interface NodeCategoryInfo {
  name: string;
  displayName: string;
  description: string;
  nodeCount: number;
  icon?: string;
  color?: string;
}

export interface CreateWorkflowParams {
  name: string;
  description?: string;
  nodes: WorkflowNodeInput[];
  edges: WorkflowEdgeInput[];
}

export interface WorkflowNodeInput {
  id: string;
  type: string;
  position: { x: number; y: number };
  parameters?: Record<string, any>;
  label?: string;
}

export interface WorkflowEdgeInput {
  source: string;
  target: string;
  sourceHandle?: string;
  targetHandle?: string;
}

export interface AddNodeParams {
  workflowId: string;
  nodeId: string;
  nodeType: string;
  position: { x: number; y: number };
  parameters?: Record<string, any>;
  label?: string;
}

export interface ConnectNodesParams {
  workflowId: string;
  sourceNodeId: string;
  targetNodeId: string;
  sourceHandle?: string;
  targetHandle?: string;
}

export interface ConfigureNodeParams {
  workflowId: string;
  nodeId: string;
  parameters: Record<string, any>;
}

export interface DisconnectNodesParams {
  workflowId: string;
  sourceNodeId: string;
  targetNodeId: string;
  sourceHandle?: string;
}

export interface MoveNodeParams {
  workflowId: string;
  nodeId: string;
  x?: number;
  y?: number;
  relative?: boolean;
  direction?: 'up' | 'down' | 'left' | 'right' | '';
}

export interface AutoLayoutParams {
  workflowId: string;
  direction?: 'horizontal' | 'vertical';
  spacingX?: number;
  spacingY?: number;
}

export interface WorkflowStats {
  workflowId: string;
  totalRuns: number;
  successfulRuns: number;
  failedRuns: number;
  successRate: number;
  lastRunAt?: string;
  lastStatus?: string;
  averageDuration?: number;
}

export interface UpdateWorkflowParams {
  workflowId: string;
  name?: string;
  description?: string;
  nodes?: WorkflowNodeInput[];
  edges?: WorkflowEdgeInput[];
}

export interface WorkflowInfo {
  id: string;
  name: string;
  description?: string;
  status: 'idle' | 'running' | 'completed' | 'error' | 'draft';
  nodes: WorkflowNodeInput[];
  edges: WorkflowEdgeInput[];
  createdAt?: string;
  updatedAt?: string;
}

export interface WorkflowExecutionResult {
  success: boolean;
  workflowId: string;
  results?: Record<string, NodeExecutionResultInfo>;
  error?: string;
  duration?: number;
}

export interface NodeExecutionResultInfo {
  success: boolean;
  data?: any;
  error?: string;
  duration?: number;
}

export interface WorkflowValidation {
  valid: boolean;
  errors: string[];
  warnings?: string[];
}

export const INTERNAL_SERVER_ID = 'fincept-terminal';
export const INTERNAL_SERVER_NAME = 'Fincept Terminal';

// ============================================================================
// News Types
// ============================================================================

export interface NewsArticleInfo {
  id: string;
  time: string;
  priority: 'FLASH' | 'URGENT' | 'BREAKING' | 'ROUTINE';
  category: string;
  headline: string;
  summary: string;
  source: string;
  region: string;
  sentiment: 'BULLISH' | 'BEARISH' | 'NEUTRAL';
  impact: 'HIGH' | 'MEDIUM' | 'LOW';
  tickers: string[];
  classification: string;
  link?: string;
  pubDate?: string;
}

export interface RSSFeedInfo {
  id: string | null;
  name: string;
  url: string;
  category: string;
  region: string;
  source: string;
  enabled: boolean;
  is_default: boolean;
}

export interface UserRSSFeedInfo {
  id: string;
  name: string;
  url: string;
  category: string;
  region: string;
  source_name: string;
  enabled: boolean;
  is_default: boolean;
  created_at: string;
  updated_at: string;
}

export interface AddRSSFeedParams {
  name: string;
  url: string;
  category: string;
  region: string;
  source_name: string;
}

export interface UpdateRSSFeedParams {
  name: string;
  url: string;
  category: string;
  region: string;
  source_name: string;
  enabled: boolean;
}

export interface NewsAnalysisResult {
  success: boolean;
  data?: NewsAnalysisData;
  error?: string;
}

export interface NewsAnalysisData {
  article_id: string;
  source: string;
  url: string;
  published_at: string;
  ingested_at: string;
  processing_version: string;
  content: {
    headline: string;
    word_count: number;
  };
  analysis: {
    sentiment: {
      score: number;
      intensity: number;
      confidence: number;
    };
    market_impact: {
      urgency: 'LOW' | 'MEDIUM' | 'HIGH';
      prediction: string;
    };
    entities: {
      organizations: Array<{
        name: string;
        ticker: string | null;
        sector: string;
        sentiment: number;
      }>;
      people: Array<{
        name: string;
        ticker: string | null;
        sector: string;
        sentiment: number;
      }>;
      locations: Array<{
        name: string;
        ticker: string | null;
        sector: string;
        sentiment: number;
      }>;
    };
    keywords: string[];
    topics: string[];
    risk_signals: {
      regulatory: { level: string; details: string };
      geopolitical: { level: string; details: string };
      operational: { level: string; details: string };
      market: { level: string; details: string };
    };
    summary: string;
    key_points: string[];
  };
  credits_used: number;
  credits_remaining: number;
}

export interface NewsSentimentStats {
  bullish: number;
  bearish: number;
  neutral: number;
  total: number;
  netScore: number;
  alertCount: number;
}

export interface NewsCacheStats {
  hasCachedData: boolean;
  articleCount: number;
  cacheAge: number | null;
}

// ============================================================================
// Data Mapping Types
// ============================================================================

export interface DataMappingSummary {
  id: string;
  name: string;
  description: string;
  schemaType: string;
  schema?: string;
  method: string;
  baseUrl: string;
  endpoint: string;
  fieldCount: number;
  cacheEnabled: boolean;
  createdAt: string;
  updatedAt: string;
  tags: string[];
}

export interface DataMappingDetail extends DataMappingSummary {
  authentication: {
    type: string;
  };
  headers: Record<string, string>;
  queryParams: Record<string, string>;
  fieldMappings: Array<{
    targetField: string;
    sourceExpression: string;
    parser: string;
  }>;
  extraction: {
    engine: string;
    rootPath?: string;
    isArray: boolean;
  };
  validation: {
    enabled: boolean;
    strictMode: boolean;
  };
}

export interface CreateDataMappingParams {
  name: string;
  description?: string;
  method?: 'GET' | 'POST' | 'PUT' | 'DELETE' | 'PATCH';
  baseUrl: string;
  endpoint: string;
  authType?: 'none' | 'apikey' | 'bearer' | 'basic';
  authToken?: string;
  authKeyName?: string;
  authKeyLocation?: 'header' | 'query';
  authUsername?: string;
  authPassword?: string;
  headers?: Record<string, string>;
  queryParams?: Record<string, string>;
  schemaType?: 'predefined' | 'custom';
  schema?: string;
  rootPath?: string;
  isArray?: boolean;
  cacheEnabled?: boolean;
  cacheTTL?: number;
  tags?: string[];
}

export interface DataMappingExecutionResult {
  success: boolean;
  data?: any;
  rawData?: any;
  errors?: string[];
  warnings?: string[];
  recordsProcessed: number;
  recordsReturned: number;
  duration: number;
  executedAt: string;
}

export interface DataMappingTemplateSummary {
  id: string;
  name: string;
  description: string;
  category: string;
  tags: string[];
  fieldCount: number;
  schema?: string;
  verified: boolean;
  official: boolean;
  rating?: number;
  usageCount?: number;
  instructions?: string;
}

export interface DataMappingSchemaSummary {
  name: string;
  category: string;
  description: string;
  fieldCount: number;
  requiredFields: string[];
}
