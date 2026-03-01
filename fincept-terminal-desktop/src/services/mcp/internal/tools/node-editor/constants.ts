/**
 * Node Editor MCP Tools — Shared Constants
 *
 * NODE_CATEGORIES, AVAILABLE_NODE_TYPES, and getNodeColor helper used across
 * all node-editor tool group files.
 */

/**
 * Node categories for easier discovery
 */
export const NODE_CATEGORIES = [
  'trigger',      // Workflow entry points (ManualTrigger, ScheduleTrigger, PriceAlert, etc.)
  'marketData',   // Market data nodes (YFinance, GetQuote, GetHistoricalData, etc.)
  'trading',      // Trading nodes (PlaceOrder, CancelOrder, GetPositions, etc.)
  'analytics',    // Analytics nodes (TechnicalIndicators, PortfolioOptimization, Backtest, etc.)
  'controlFlow',  // Control flow (IfElse, Switch, Loop, Wait, Merge, Split, etc.)
  'transform',    // Data transformation (Filter, Map, Aggregate, Sort, GroupBy, Join, etc.)
  'safety',       // Risk management (RiskCheck, PositionSizeLimit, LossLimit, etc.)
  'notifications', // Notifications (Webhook, Email, Telegram, Discord, Slack, SMS)
  'agents',       // AI agents (AgentMediator, GeopoliticsAgent, HedgeFundAgent, etc.)
  'core',         // Core nodes (Filter, Merge, Set, Switch, Code)
];

/**
 * Available node types mapped to their internal names
 * This helps LLM know exactly which node types exist
 */
export const AVAILABLE_NODE_TYPES = {
  // Triggers
  manualTrigger: 'Start workflow manually',
  scheduleTrigger: 'Run on schedule (cron)',
  priceAlertTrigger: 'Trigger when price hits threshold',
  newsEventTrigger: 'Trigger on news events',
  marketEventTrigger: 'Trigger on market open/close/volatility',
  webhookTrigger: 'Trigger from external webhook',

  // Market Data
  yfinance: 'Fetch stock data from Yahoo Finance',
  getQuote: 'Get real-time price quote',
  getHistoricalData: 'Fetch historical OHLCV data',
  getMarketDepth: 'Get order book depth',
  streamQuotes: 'Stream real-time quotes (WebSocket)',
  getFundamentals: 'Fetch fundamental data (P/E, dividends)',
  getTickerStats: 'Get ticker statistics',
  stockData: 'General stock data node',

  // Trading
  placeOrder: 'Place buy/sell order',
  modifyOrder: 'Modify existing order',
  cancelOrder: 'Cancel open order',
  getPositions: 'Get current positions',
  getHoldings: 'Get portfolio holdings',
  getOrders: 'Get order history',
  getBalance: 'Get account balance',
  closePosition: 'Close a position',

  // Analytics
  technicalIndicators: 'Calculate technical indicators (RSI, MACD, SMA, EMA, BB, etc.)',
  portfolioOptimization: 'Optimize portfolio allocation (Mean-Variance)',
  backtestEngine: 'Backtest trading strategy',
  riskAnalysis: 'Calculate risk metrics (VaR, Sharpe, etc.)',
  performanceMetrics: 'Analyze performance (returns, drawdown)',
  correlationMatrix: 'Calculate asset correlations',

  // Control Flow
  ifElse: 'Conditional branching (if/else)',
  switch: 'Multi-way routing based on value',
  loop: 'Iterate over array items',
  wait: 'Pause execution for duration',
  merge: 'Combine multiple data streams',
  split: 'Split stream into multiple',
  errorHandler: 'Catch and handle errors',

  // Transform
  filterTransform: 'Filter items by condition',
  map: 'Transform each item',
  aggregate: 'Aggregate data (sum, avg, count)',
  sort: 'Sort items by field',
  groupBy: 'Group items by field',
  join: 'Join two datasets',
  deduplicate: 'Remove duplicates',
  reshape: 'Restructure data shape',

  // Safety
  riskCheck: 'Validate against risk limits',
  positionSizeLimit: 'Enforce position size limits',
  lossLimit: 'Enforce max loss limits',
  tradingHoursCheck: 'Validate trading hours',

  // Notifications
  webhookNotification: 'Send HTTP webhook',
  email: 'Send email notification',
  telegram: 'Send Telegram message',
  discord: 'Send Discord message',
  slack: 'Send Slack message',
  sms: 'Send SMS notification',

  // AI Agents
  agentMediator: 'Mediate between AI agents',
  agent: 'Custom AI agent',
  multiAgent: 'Orchestrate multiple agents',
  aiAgent: 'AI agent (geopolitics, hedge fund, investor, economic)',

  // Core
  filter: 'Basic filter node',
  set: 'Set/modify data fields',
  code: 'Execute custom JavaScript',
};

/** Helper function for node colors (used by add_node_by_description) */
export function getNodeColor(nodeType: string): string {
  const colorMap: Record<string, string> = {
    manualTrigger: '#10b981',
    scheduleTrigger: '#a855f7',
    yfinance: '#3b82f6',
    technicalIndicators: '#10b981',
    ifElse: '#eab308',
    telegram: '#f97316',
    discord: '#f97316',
    slack: '#f97316',
    email: '#f97316',
    placeOrder: '#14b8a6',
    riskCheck: '#ef4444',
    backtestEngine: '#06b6d4',
    resultsDisplay: '#f59e0b',
  };
  return colorMap[nodeType] || '#6b7280';
}
