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
