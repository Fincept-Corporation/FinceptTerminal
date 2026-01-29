/**
 * COMPLETE WORKFLOW TEMPLATE - ALL 82 NODES
 *
 * Institutional-Grade Algorithmic Trading System
 * A comprehensive workflow demonstrating all 82 node types in a logical,
 * realistic trading system architecture.
 *
 * Architecture: 10-Phase Intelligent Trading Pipeline
 * 1. Trigger Layer - Multiple entry points
 * 2. Data Acquisition - Market data + company relationships
 * 3. Data Processing - Cleaning, transformation, utilities
 * 4. Analysis Layer - Technical + fundamental + risk analytics
 * 5. AI Decision Layer - Multi-agent strategy formation
 * 6. Risk Management - Safety checks and position sizing
 * 7. Execution Control - Flow control and error handling
 * 8. Trading Operations - Order execution and management
 * 9. Monitoring & Alerts - Multi-channel notifications
 * 10. Visualization - Results display and reporting
 */

import { Node, Edge, MarkerType } from 'reactflow';
import { WorkflowTemplate } from './workflowTemplates';

const edgeStyle = { stroke: '#ea580c', strokeWidth: 2 };
const markerEnd = { type: MarkerType.ArrowClosed, color: '#ea580c' };

function makeEdge(id: string, source: string, target: string, sourceHandle?: string, targetHandle?: string, label?: string): Edge {
  return {
    id,
    source,
    target,
    sourceHandle,
    targetHandle,
    animated: true,
    style: edgeStyle,
    markerEnd,
    label,
  };
}

// ============================================================================
// PHASE 1: TRIGGER LAYER (6 nodes) - Y: 0-150
// Horizontal spacing: 350px between nodes
// ============================================================================
const phase1Triggers: Node[] = [
  // Trigger 1: Manual
  {
    id: 'trigger_manual',
    type: 'system',
    position: { x: 200, y: 50 },
    data: {
      node: {
        type: 'ManualTriggerNode',
        typeVersion: 1,
        name: 'Manual Trigger',
        parameters: { enabled: true },
        disabled: false,
      },
    },
  },
  // Trigger 2: Schedule (Market Open)
  {
    id: 'trigger_schedule',
    type: 'system',
    position: { x: 550, y: 50 },
    data: {
      node: {
        type: 'ScheduleTriggerNode',
        typeVersion: 1,
        name: 'Market Open (9:30 AM)',
        parameters: { schedule: '30 9 * * 1-5', timezone: 'America/New_York' },
        disabled: false,
      },
    },
  },
  // Trigger 3: Price Alert
  {
    id: 'trigger_price',
    type: 'system',
    position: { x: 900, y: 50 },
    data: {
      node: {
        type: 'PriceAlertTriggerNode',
        typeVersion: 1,
        name: 'Price Break Alert',
        parameters: { symbol: 'SPY', condition: 'crosses_above', threshold: 450, operator: 'greater_than' },
        disabled: false,
      },
    },
  },
  // Trigger 4: News Event
  {
    id: 'trigger_news',
    type: 'system',
    position: { x: 1250, y: 50 },
    data: {
      node: {
        type: 'NewsEventTriggerNode',
        typeVersion: 1,
        name: 'Earnings/M&A News',
        parameters: { keywords: ['earnings', 'merger', 'acquisition'], sources: ['bloomberg', 'reuters'] },
        disabled: false,
      },
    },
  },
  // Trigger 5: Market Event (Volatility)
  {
    id: 'trigger_market',
    type: 'system',
    position: { x: 1600, y: 50 },
    data: {
      node: {
        type: 'MarketEventTriggerNode',
        typeVersion: 1,
        name: 'VIX Spike Alert',
        parameters: { eventType: 'volatilitySpike', threshold: 25, metric: 'VIX' },
        disabled: false,
      },
    },
  },
  // Trigger 6: Webhook (External Signal)
  {
    id: 'trigger_webhook',
    type: 'system',
    position: { x: 1950, y: 50 },
    data: {
      node: {
        type: 'WebhookTriggerNode',
        typeVersion: 1,
        name: 'External Signal API',
        parameters: { path: '/webhook/trading-signal', method: 'POST', authentication: 'api_key' },
        disabled: false,
      },
    },
  },
];

// ============================================================================
// PHASE 2: DATA ACQUISITION (15 nodes) - Y: 250-450
// Market Data (7) + Relationship Map (8)
// ============================================================================
const phase2DataAcquisition: Node[] = [
  // Control: Merge all triggers
  {
    id: 'merge_triggers',
    type: 'custom',
    position: { x: 1100, y: 250 },
    data: {
      label: 'Merge All Triggers',
      nodeTypeName: 'merge',
      color: '#f59e0b',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: { mergeMode: 'any', waitForAll: false },
    },
  },

  // Market Data Layer
  {
    id: 'market_quote',
    type: 'system',
    position: { x: 200, y: 450 },
    data: {
      node: {
        type: 'GetQuoteNode',
        typeVersion: 1,
        name: 'Real-Time Quotes',
        parameters: { symbols: ['AAPL', 'MSFT', 'GOOGL', 'AMZN', 'META'], provider: 'yahoo', updateFrequency: '1s' },
        disabled: false,
      },
    },
  },
  {
    id: 'market_historical',
    type: 'system',
    position: { x: 550, y: 450 },
    data: {
      node: {
        type: 'GetHistoricalDataNode',
        typeVersion: 1,
        name: 'Historical OHLCV',
        parameters: { symbols: 'AAPL,MSFT,GOOGL,AMZN,META', interval: '1d', period: '2y' },
        disabled: false,
      },
    },
  },
  {
    id: 'market_depth',
    type: 'system',
    position: { x: 900, y: 450 },
    data: {
      node: {
        type: 'GetMarketDepthNode',
        typeVersion: 1,
        name: 'L2 Order Book',
        parameters: { symbol: 'AAPL', levels: 20, exchange: 'NASDAQ' },
        disabled: false,
      },
    },
  },
  {
    id: 'market_stream',
    type: 'system',
    position: { x: 1250, y: 450 },
    data: {
      node: {
        type: 'StreamQuotesNode',
        typeVersion: 1,
        name: 'WebSocket Stream',
        parameters: { symbols: ['AAPL', 'SPY'], channels: ['trades', 'quotes'], provider: 'alpaca' },
        disabled: false,
      },
    },
  },
  {
    id: 'market_fundamentals',
    type: 'system',
    position: { x: 1600, y: 450 },
    data: {
      node: {
        type: 'GetFundamentalsNode',
        typeVersion: 1,
        name: 'Company Fundamentals',
        parameters: { symbol: 'AAPL', metrics: ['pe_ratio', 'eps', 'revenue', 'profit_margin'] },
        disabled: false,
      },
    },
  },
  {
    id: 'market_stats',
    type: 'system',
    position: { x: 1950, y: 450 },
    data: {
      node: {
        type: 'GetTickerStatsNode',
        typeVersion: 1,
        name: 'Ticker Statistics',
        parameters: { symbol: 'AAPL', stats: ['volume', 'avg_volume', '52w_high', '52w_low'] },
        disabled: false,
      },
    },
  },
  {
    id: 'market_yfinance',
    type: 'system',
    position: { x: 2300, y: 450 },
    data: {
      node: {
        type: 'YFinanceNode',
        typeVersion: 1,
        name: 'YFinance Data',
        parameters: { symbol: 'AAPL', period: '1y', interval: '1d', metrics: 'all' },
        disabled: false,
      },
    },
  },

  // Relationship Map Layer (Company Intelligence)
  {
    id: 'rel_company',
    type: 'custom',
    position: { x: 200, y: 700 },
    data: {
      label: 'Company Entity (AAPL)',
      nodeTypeName: 'companyNode',
      color: '#3b82f6',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: { symbol: 'AAPL', name: 'Apple Inc.', sector: 'Technology' },
    },
  },
  {
    id: 'rel_peers',
    type: 'custom',
    position: { x: 550, y: 700 },
    data: {
      label: 'Peer Companies',
      nodeTypeName: 'peerNode',
      color: '#3b82f6',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: { baseSymbol: 'AAPL', peers: ['MSFT', 'GOOGL', 'META', 'AMZN'] },
    },
  },
  {
    id: 'rel_institutional',
    type: 'custom',
    position: { x: 900, y: 700 },
    data: {
      label: 'Institutional Holders',
      nodeTypeName: 'institutionalHolderNode',
      color: '#3b82f6',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: { symbol: 'AAPL', minOwnership: 1, topN: 10 },
    },
  },
  {
    id: 'rel_funds',
    type: 'custom',
    position: { x: 1250, y: 700 },
    data: {
      label: 'Fund Families',
      nodeTypeName: 'fundFamilyNode',
      color: '#3b82f6',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: { symbol: 'AAPL', families: ['Vanguard', 'BlackRock', 'State Street'] },
    },
  },
  {
    id: 'rel_insiders',
    type: 'custom',
    position: { x: 1600, y: 700 },
    data: {
      label: 'Company Insiders',
      nodeTypeName: 'insiderNode',
      color: '#3b82f6',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: { symbol: 'AAPL', includeOfficers: true, includeDirectors: true },
    },
  },
  {
    id: 'rel_events',
    type: 'custom',
    position: { x: 1950, y: 700 },
    data: {
      label: 'Corporate Events',
      nodeTypeName: 'eventNode',
      color: '#3b82f6',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: { symbol: 'AAPL', eventTypes: ['earnings', 'dividends', 'splits', 'buybacks'] },
    },
  },
  {
    id: 'rel_metrics',
    type: 'custom',
    position: { x: 2300, y: 700 },
    data: {
      label: 'Metrics Cluster',
      nodeTypeName: 'metricsClusterNode',
      color: '#3b82f6',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: { symbol: 'AAPL', clusterType: 'valuation', metrics: ['PE', 'PB', 'PS', 'EV/EBITDA'] },
    },
  },
  {
    id: 'rel_supply_chain',
    type: 'custom',
    position: { x: 2650, y: 700 },
    data: {
      label: 'Supply Chain Map',
      nodeTypeName: 'supplyChainNode',
      color: '#3b82f6',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: { symbol: 'AAPL', depth: 2, includeSuppliers: true, includeCustomers: true },
    },
  },
];

// ============================================================================
// PHASE 3: DATA PROCESSING (13 nodes) - Y: 600-750
// Transform (8) + Core Utilities (5)
// ============================================================================
const phase3DataProcessing: Node[] = [
  // Merge market data and relationship data
  {
    id: 'merge_data_sources',
    type: 'custom',
    position: { x: 1100, y: 950 },
    data: {
      label: 'Merge Data Sources',
      nodeTypeName: 'merge',
      color: '#f59e0b',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: { mergeMode: 'all', waitForAll: true },
    },
  },

  // Transform Layer
  {
    id: 'transform_filter',
    type: 'system',
    position: { x: 200, y: 1150 },
    data: {
      node: {
        type: 'FilterTransformNode',
        typeVersion: 1,
        name: 'Filter High Volume',
        parameters: { field: 'volume', operator: 'greaterThan', value: 5000000, description: 'Only high-liquidity stocks' },
        disabled: false,
      },
    },
  },
  {
    id: 'transform_map',
    type: 'system',
    position: { x: 550, y: 1150 },
    data: {
      node: {
        type: 'MapNode',
        typeVersion: 1,
        name: 'Calculate Returns',
        parameters: { expression: '(close - open) / open * 100', outputField: 'daily_return' },
        disabled: false,
      },
    },
  },
  {
    id: 'transform_aggregate',
    type: 'system',
    position: { x: 900, y: 1150 },
    data: {
      node: {
        type: 'AggregateNode',
        typeVersion: 1,
        name: 'Total Volume',
        parameters: { operation: 'sum', field: 'volume', groupBy: 'symbol' },
        disabled: false,
      },
    },
  },
  {
    id: 'transform_sort',
    type: 'system',
    position: { x: 1250, y: 1150 },
    data: {
      node: {
        type: 'SortNode',
        typeVersion: 1,
        name: 'Rank by Performance',
        parameters: { field: 'daily_return', direction: 'descending', limit: 10 },
        disabled: false,
      },
    },
  },
  {
    id: 'transform_groupby',
    type: 'system',
    position: { x: 1600, y: 1150 },
    data: {
      node: {
        type: 'GroupByNode',
        typeVersion: 1,
        name: 'Group by Sector',
        parameters: { field: 'sector', aggregations: { avg_return: 'mean', total_volume: 'sum' } },
        disabled: false,
      },
    },
  },
  {
    id: 'transform_join',
    type: 'system',
    position: { x: 1950, y: 1150 },
    data: {
      node: {
        type: 'JoinNode',
        typeVersion: 1,
        name: 'Join Price + Fundamentals',
        parameters: { joinKey: 'symbol', joinType: 'inner', leftSource: 'prices', rightSource: 'fundamentals' },
        disabled: false,
      },
    },
  },
  {
    id: 'transform_dedupe',
    type: 'system',
    position: { x: 2300, y: 1150 },
    data: {
      node: {
        type: 'DeduplicateNode',
        typeVersion: 1,
        name: 'Remove Duplicates',
        parameters: { uniqueField: 'symbol', keepFirst: true },
        disabled: false,
      },
    },
  },
  {
    id: 'transform_reshape',
    type: 'system',
    position: { x: 2650, y: 1150 },
    data: {
      node: {
        type: 'ReshapeNode',
        typeVersion: 1,
        name: 'Format for Analytics',
        parameters: { outputFormat: 'timeseries', indexField: 'date', columnsField: 'symbol' },
        disabled: false,
      },
    },
  },

  // Core Utility Nodes
  {
    id: 'util_code',
    type: 'custom',
    position: { x: 200, y: 1400 },
    data: {
      label: 'Custom Code Logic',
      nodeTypeName: 'code',
      color: '#64748b',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: { language: 'javascript', code: 'return items.map(item => ({ ...item, processed: true }));' },
    },
  },
  {
    id: 'util_filter',
    type: 'custom',
    position: { x: 550, y: 1400 },
    data: {
      label: 'Utility Filter',
      nodeTypeName: 'filterUtil',
      color: '#64748b',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: { conditions: [{ field: 'price', op: '>', value: 50 }] },
    },
  },
  {
    id: 'util_merge',
    type: 'custom',
    position: { x: 900, y: 1400 },
    data: {
      label: 'Utility Merge',
      nodeTypeName: 'mergeUtil',
      color: '#64748b',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: { strategy: 'concat' },
    },
  },
  {
    id: 'util_set',
    type: 'custom',
    position: { x: 1250, y: 1400 },
    data: {
      label: 'Set Variables',
      nodeTypeName: 'set',
      color: '#64748b',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: { variables: { portfolio_size: 100000, max_position: 0.1 } },
    },
  },
  {
    id: 'util_switch',
    type: 'custom',
    position: { x: 1600, y: 1400 },
    data: {
      label: 'Utility Switch',
      nodeTypeName: 'switchUtil',
      color: '#64748b',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: { field: 'market_condition', cases: { bullish: 'path1', bearish: 'path2', neutral: 'path3' } },
    },
  },
];

// ============================================================================
// PHASE 4: ANALYSIS LAYER (7 nodes) - Y: 950-1100
// ============================================================================
const phase4Analytics: Node[] = [
  // Merge processed data
  {
    id: 'merge_processed',
    type: 'custom',
    position: { x: 1100, y: 1650 },
    data: {
      label: 'Merge Processed Data',
      nodeTypeName: 'merge',
      color: '#f59e0b',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: { mergeMode: 'all' },
    },
  },

  {
    id: 'analytics_indicators',
    type: 'technical-indicator',
    position: { x: 200, y: 1850 },
    data: {
      label: 'Technical Indicators',
      dataSource: 'yfinance',
      symbol: 'AAPL',
      period: '1y',
      csvPath: '',
      jsonData: '',
      categories: ['momentum', 'trend', 'volatility', 'volume'],
      status: 'idle',
      result: undefined,
      error: undefined,
      onExecute: () => {},
      onParameterChange: () => {},
    },
  },
  {
    id: 'analytics_portfolio_opt',
    type: 'system',
    position: { x: 550, y: 1850 },
    data: {
      node: {
        type: 'PortfolioOptimizationNode',
        typeVersion: 1,
        name: 'MPT Optimization',
        parameters: { method: 'max_sharpe', constraints: { max_weight: 0.3, min_weight: 0.05 }, riskFreeRate: 0.05 },
        disabled: false,
      },
    },
  },
  {
    id: 'analytics_backtest',
    type: 'backtest',
    position: { x: 900, y: 1850 },
    data: {
      label: 'Strategy Backtest',
      color: '#06b6d4',
      status: 'idle',
      parameters: {
        strategy: 'MA_Crossover',
        initialCapital: 100000,
        commission: 0.001,
        shortWindow: 20,
        longWindow: 50,
        slippage: 0.0005,
      },
    },
  },
  {
    id: 'analytics_risk',
    type: 'system',
    position: { x: 1250, y: 1850 },
    data: {
      node: {
        type: 'RiskAnalysisNode',
        typeVersion: 1,
        name: 'Risk Metrics (VaR/CVaR)',
        parameters: { confidenceLevel: 0.95, timeHorizon: '1d', riskMetrics: ['var', 'cvar', 'maxDrawdown', 'sharpe', 'sortino'] },
        disabled: false,
      },
    },
  },
  {
    id: 'analytics_performance',
    type: 'system',
    position: { x: 1600, y: 1850 },
    data: {
      node: {
        type: 'PerformanceMetricsNode',
        typeVersion: 1,
        name: 'Performance vs Benchmark',
        parameters: { benchmarkSymbol: 'SPY', riskFreeRate: 0.05, metrics: ['alpha', 'beta', 'sharpe', 'information_ratio'] },
        disabled: false,
      },
    },
  },
  {
    id: 'analytics_correlation',
    type: 'system',
    position: { x: 1950, y: 1850 },
    data: {
      node: {
        type: 'CorrelationMatrixNode',
        typeVersion: 1,
        name: 'Correlation Matrix',
        parameters: { method: 'pearson', minCorrelation: 0.5, visualize: true },
        disabled: false,
      },
    },
  },
];

// ============================================================================
// PHASE 5: AI DECISION LAYER (6 nodes) - Y: 1200-1350
// ============================================================================
const phase5AIDecision: Node[] = [
  // Merge analytics
  {
    id: 'merge_analytics',
    type: 'custom',
    position: { x: 1100, y: 2100 },
    data: {
      label: 'Merge Analytics Results',
      nodeTypeName: 'merge',
      color: '#f59e0b',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: { mergeMode: 'all' },
    },
  },

  {
    id: 'agent_python',
    type: 'python-agent',
    position: { x: 200, y: 2300 },
    data: {
      agentType: 'portfolio_analysis',
      agentCategory: 'financial',
      label: 'Python Quant Agent',
      icon: 'brain',
      color: '#8b5cf6',
      parameters: { analysisType: 'comprehensive', model: 'advanced', outputFormat: 'json' },
      selectedLLM: 'active',
      status: 'idle',
      result: undefined,
      error: undefined,
      onExecute: () => {},
      onParameterChange: () => {},
      onLLMChange: () => {},
    },
  },
  {
    id: 'agent_geopolitics',
    type: 'system',
    position: { x: 550, y: 2300 },
    data: {
      node: {
        type: 'GeopoliticsAgentNode',
        typeVersion: 1,
        name: 'Geopolitics Risk Agent',
        parameters: { region: 'global', analysisType: 'macro_risk', factors: ['trade', 'sanctions', 'conflicts'] },
        disabled: false,
      },
    },
  },
  {
    id: 'agent_hedgefund',
    type: 'system',
    position: { x: 900, y: 2300 },
    data: {
      node: {
        type: 'HedgeFundAgentNode',
        typeVersion: 1,
        name: 'Hedge Fund Strategy',
        parameters: { strategy: 'long_short_equity', leverage: 2.0, hedgeRatio: 0.7 },
        disabled: false,
      },
    },
  },
  {
    id: 'agent_investor',
    type: 'system',
    position: { x: 1250, y: 2300 },
    data: {
      node: {
        type: 'InvestorAgentNode',
        typeVersion: 1,
        name: 'Investor Persona (Buffett)',
        parameters: { persona: 'warren_buffett', strategy: 'value_investing', timeHorizon: 'long_term' },
        disabled: false,
      },
    },
  },
  {
    id: 'agent_multi',
    type: 'system',
    position: { x: 1600, y: 2300 },
    data: {
      node: {
        type: 'MultiAgentNode',
        typeVersion: 1,
        name: 'Multi-Agent Committee',
        parameters: { agents: ['technical_analyst', 'fundamental_analyst', 'risk_manager', 'portfolio_manager'], votingMethod: 'weighted' },
        disabled: false,
      },
    },
  },
  {
    id: 'agent_mediator',
    type: 'agent-mediator',
    position: { x: 1100, y: 2550 },
    data: {
      label: 'AI Decision Mediator',
      selectedProvider: undefined,
      customPrompt: 'Synthesize all agent recommendations and provide final trading decision with confidence score',
      status: 'idle',
      result: undefined,
      error: undefined,
      inputData: undefined,
      onExecute: () => {},
      onConfigChange: () => {},
    },
  },
];

// ============================================================================
// PHASE 6: RISK MANAGEMENT (4 nodes) - Y: 1500-1650
// ============================================================================
const phase6RiskManagement: Node[] = [
  {
    id: 'safety_risk_check',
    type: 'system',
    position: { x: 200, y: 2750 },
    data: {
      node: {
        type: 'RiskCheckNode',
        typeVersion: 1,
        name: 'Pre-Trade Risk Check',
        parameters: { maxRiskPerTrade: 0.02, maxPortfolioRisk: 0.1, checkCorrelation: true },
        disabled: false,
      },
    },
  },
  {
    id: 'safety_position_size',
    type: 'system',
    position: { x: 550, y: 2750 },
    data: {
      node: {
        type: 'PositionSizeLimitNode',
        typeVersion: 1,
        name: 'Kelly Criterion Sizing',
        parameters: { method: 'kelly', maxPositionSize: 20000, minPositionSize: 1000, fractionalKelly: 0.25 },
        disabled: false,
      },
    },
  },
  {
    id: 'safety_loss_limit',
    type: 'system',
    position: { x: 900, y: 2750 },
    data: {
      node: {
        type: 'LossLimitNode',
        typeVersion: 1,
        name: 'Loss Limits & Circuit Breaker',
        parameters: { maxDailyLoss: 2000, maxWeeklyLoss: 5000, stopAllOnBreach: true },
        disabled: false,
      },
    },
  },
  {
    id: 'safety_trading_hours',
    type: 'system',
    position: { x: 1250, y: 2750 },
    data: {
      node: {
        type: 'TradingHoursCheckNode',
        typeVersion: 1,
        name: 'Trading Hours Gate',
        parameters: { marketHours: '09:30-16:00', timezone: 'America/New_York', allowPreMarket: false, allowAfterHours: false },
        disabled: false,
      },
    },
  },
];

// ============================================================================
// PHASE 7: EXECUTION CONTROL (7 nodes) - Y: 1750-1900
// ============================================================================
const phase7ExecutionControl: Node[] = [
  {
    id: 'control_ifelse',
    type: 'system',
    position: { x: 200, y: 3000 },
    data: {
      node: {
        type: 'IfElseNode',
        typeVersion: 1,
        name: 'Trade or Hold Decision',
        parameters: { condition: 'confidence_score > 0.75 && risk_score < 0.3', trueLabel: 'EXECUTE', falseLabel: 'HOLD' },
        disabled: false,
      },
    },
  },
  {
    id: 'control_switch',
    type: 'system',
    position: { x: 550, y: 3000 },
    data: {
      node: {
        type: 'SwitchNode',
        typeVersion: 1,
        name: 'Strategy Router',
        parameters: { field: 'strategy_type', cases: { momentum: 'momentum_execution', mean_reversion: 'mean_reversion_execution', breakout: 'breakout_execution' } },
        disabled: false,
      },
    },
  },
  {
    id: 'control_loop',
    type: 'system',
    position: { x: 900, y: 3000 },
    data: {
      node: {
        type: 'LoopNode',
        typeVersion: 1,
        name: 'Order Slicing Loop',
        parameters: { maxIterations: 10, breakCondition: 'order_filled || attempts > 10', loopVariable: 'slice_index' },
        disabled: false,
      },
    },
  },
  {
    id: 'control_wait',
    type: 'system',
    position: { x: 1250, y: 3000 },
    data: {
      node: {
        type: 'WaitNode',
        typeVersion: 1,
        name: 'Order Execution Delay',
        parameters: { duration: 2000, unit: 'milliseconds', reason: 'rate_limiting' },
        disabled: false,
      },
    },
  },
  {
    id: 'control_split',
    type: 'system',
    position: { x: 200, y: 3250 },
    data: {
      node: {
        type: 'SplitNode',
        typeVersion: 1,
        name: 'Split Order Flow',
        parameters: { splitCount: 3, splitBy: 'symbol', parallel: true },
        disabled: false,
      },
    },
  },
  {
    id: 'control_merge2',
    type: 'system',
    position: { x: 550, y: 3250 },
    data: {
      node: {
        type: 'MergeNode',
        typeVersion: 1,
        name: 'Merge Execution Paths',
        parameters: { mergeMode: 'all', waitForAll: true },
        disabled: false,
      },
    },
  },
  {
    id: 'control_error',
    type: 'system',
    position: { x: 900, y: 3250 },
    data: {
      node: {
        type: 'ErrorHandlerNode',
        typeVersion: 1,
        name: 'Error Handler & Retry',
        parameters: { retryCount: 3, retryDelay: 1000, fallbackBehavior: 'cancel_order', logErrors: true },
        disabled: false,
      },
    },
  },
];

// ============================================================================
// PHASE 8: TRADING OPERATIONS (8 nodes) - Y: 2100-2300
// ============================================================================
const phase8TradingOps: Node[] = [
  {
    id: 'trading_place_order',
    type: 'system',
    position: { x: 200, y: 3500 },
    data: {
      node: {
        type: 'PlaceOrderNode',
        typeVersion: 1,
        name: 'Place Smart Order',
        parameters: { symbol: 'AAPL', side: 'buy', quantity: 100, orderType: 'limit', timeInForce: 'day', smartRouting: true },
        disabled: false,
      },
    },
  },
  {
    id: 'trading_modify_order',
    type: 'system',
    position: { x: 550, y: 3500 },
    data: {
      node: {
        type: 'ModifyOrderNode',
        typeVersion: 1,
        name: 'Modify Price/Quantity',
        parameters: { orderId: 'dynamic', newPrice: 'nbbo_mid', newQuantity: 'partial_fill_remainder' },
        disabled: false,
      },
    },
  },
  {
    id: 'trading_cancel_order',
    type: 'system',
    position: { x: 900, y: 3500 },
    data: {
      node: {
        type: 'CancelOrderNode',
        typeVersion: 1,
        name: 'Cancel Unfilled Orders',
        parameters: { orderId: 'all_pending', cancelAfter: 300, reason: 'timeout' },
        disabled: false,
      },
    },
  },
  {
    id: 'trading_get_positions',
    type: 'system',
    position: { x: 1250, y: 3500 },
    data: {
      node: {
        type: 'GetPositionsNode',
        typeVersion: 1,
        name: 'Get Open Positions',
        parameters: { account: 'main', includeUnrealizedPnL: true, filterSymbols: [] },
        disabled: false,
      },
    },
  },
  {
    id: 'trading_get_holdings',
    type: 'system',
    position: { x: 1600, y: 3500 },
    data: {
      node: {
        type: 'GetHoldingsNode',
        typeVersion: 1,
        name: 'Portfolio Holdings',
        parameters: { account: 'main', includeMarketValue: true, includeCostBasis: true },
        disabled: false,
      },
    },
  },
  {
    id: 'trading_get_orders',
    type: 'system',
    position: { x: 1950, y: 3500 },
    data: {
      node: {
        type: 'GetOrdersNode',
        typeVersion: 1,
        name: 'Order History',
        parameters: { status: 'all', startDate: 'today', limit: 100 },
        disabled: false,
      },
    },
  },
  {
    id: 'trading_get_balance',
    type: 'system',
    position: { x: 200, y: 3750 },
    data: {
      node: {
        type: 'GetBalanceNode',
        typeVersion: 1,
        name: 'Account Balance & Buying Power',
        parameters: { account: 'main', includeBuyingPower: true, includeMarginUsed: true },
        disabled: false,
      },
    },
  },
  {
    id: 'trading_close_position',
    type: 'system',
    position: { x: 550, y: 3750 },
    data: {
      node: {
        type: 'ClosePositionNode',
        typeVersion: 1,
        name: 'Close Position (SL/TP)',
        parameters: { symbol: 'AAPL', closeType: 'market', reason: 'stop_loss_hit', percentage: 100 },
        disabled: false,
      },
    },
  },
];

// ============================================================================
// PHASE 9: MONITORING & ALERTS (6 nodes) - Y: 2450-2600
// ============================================================================
const phase9Monitoring: Node[] = [
  // Merge trading results
  {
    id: 'merge_trading',
    type: 'custom',
    position: { x: 1100, y: 4000 },
    data: {
      label: 'Merge Trading Results',
      nodeTypeName: 'merge',
      color: '#f59e0b',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: { mergeMode: 'all' },
    },
  },

  {
    id: 'notify_webhook',
    type: 'system',
    position: { x: 200, y: 4200 },
    data: {
      node: {
        type: 'WebhookNotificationNode',
        typeVersion: 1,
        name: 'Webhook to Portfolio Tracker',
        parameters: { url: 'https://api.portfolio-tracker.com/webhook', method: 'POST', headers: { 'Authorization': 'Bearer token' } },
        disabled: false,
      },
    },
  },
  {
    id: 'notify_email',
    type: 'system',
    position: { x: 550, y: 4200 },
    data: {
      node: {
        type: 'EmailNode',
        typeVersion: 1,
        name: 'Email Alert',
        parameters: { to: 'trader@fund.com', subject: 'Trade Execution Alert', template: 'trade_confirmation', priority: 'high' },
        disabled: false,
      },
    },
  },
  {
    id: 'notify_telegram',
    type: 'system',
    position: { x: 900, y: 4200 },
    data: {
      node: {
        type: 'TelegramNode',
        typeVersion: 1,
        name: 'Telegram to Trading Team',
        parameters: { chatId: '123456789', botToken: 'env.TELEGRAM_BOT_TOKEN', parseMode: 'markdown' },
        disabled: false,
      },
    },
  },
  {
    id: 'notify_discord',
    type: 'system',
    position: { x: 1250, y: 4200 },
    data: {
      node: {
        type: 'DiscordNode',
        typeVersion: 1,
        name: 'Discord Trading Channel',
        parameters: { webhookUrl: 'https://discord.com/api/webhooks/...', username: 'TradingBot', embeds: true },
        disabled: false,
      },
    },
  },
  {
    id: 'notify_slack',
    type: 'system',
    position: { x: 1600, y: 4200 },
    data: {
      node: {
        type: 'SlackNode',
        typeVersion: 1,
        name: 'Slack #algo-trading',
        parameters: { channel: '#algo-trading', webhookUrl: 'env.SLACK_WEBHOOK', attachments: true },
        disabled: false,
      },
    },
  },
  {
    id: 'notify_sms',
    type: 'system',
    position: { x: 1950, y: 4200 },
    data: {
      node: {
        type: 'SMSNode',
        typeVersion: 1,
        name: 'SMS for Critical Alerts',
        parameters: { phoneNumber: '+1234567890', service: 'twilio', onlyCritical: true },
        disabled: false,
      },
    },
  },
];

// ============================================================================
// PHASE 10: VISUALIZATION & REPORTING (10 nodes) - Y: 2700-2900
// ============================================================================
const phase10Visualization: Node[] = [
  // Merge notifications
  {
    id: 'merge_notifications',
    type: 'custom',
    position: { x: 1100, y: 4450 },
    data: {
      label: 'Merge Notifications',
      nodeTypeName: 'merge',
      color: '#f59e0b',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: { mergeMode: 'all' },
    },
  },

  {
    id: 'ui_datasource',
    type: 'data-source',
    position: { x: 200, y: 4650 },
    data: {
      label: 'Data Source Connector',
      dataSource: 'yfinance',
      symbol: 'AAPL',
      period: '1y',
      status: 'idle',
      result: undefined,
      error: undefined,
      onExecute: () => {},
      onParameterChange: () => {},
    },
  },
  {
    id: 'ui_mcp',
    type: 'mcp-tool',
    position: { x: 550, y: 4650 },
    data: {
      label: 'MCP Financial API',
      serverName: 'financial-data-api',
      toolName: 'get_comprehensive_data',
      parameters: { symbols: ['AAPL'], metrics: 'all' },
      status: 'idle',
      result: undefined,
      error: undefined,
      onExecute: () => {},
      onParameterChange: () => {},
    },
  },
  {
    id: 'ui_technical_indicator',
    type: 'technical-indicator',
    position: { x: 900, y: 4650 },
    data: {
      label: 'Indicator Dashboard',
      dataSource: 'yfinance',
      symbol: 'AAPL',
      period: '1y',
      csvPath: '',
      jsonData: '',
      categories: ['momentum', 'trend', 'volatility', 'volume', 'others'],
      status: 'idle',
      result: undefined,
      error: undefined,
      onExecute: () => {},
      onParameterChange: () => {},
    },
  },
  {
    id: 'ui_backtest',
    type: 'backtest',
    position: { x: 1250, y: 4650 },
    data: {
      label: 'Backtest Results',
      color: '#06b6d4',
      status: 'idle',
      parameters: { displayMetrics: ['sharpe', 'returns', 'drawdown', 'win_rate'] },
    },
  },
  {
    id: 'ui_optimization',
    type: 'optimization',
    position: { x: 1600, y: 4650 },
    data: {
      label: 'Parameter Optimization Results',
      color: '#a855f7',
      status: 'idle',
      parameters: { displayBest: 10, visualizeSpace: true },
    },
  },
  {
    id: 'ui_agent_mediator',
    type: 'agent-mediator',
    position: { x: 1950, y: 4650 },
    data: {
      label: 'AI Insights Summary',
      selectedProvider: undefined,
      customPrompt: 'Generate executive summary of trading session',
      status: 'idle',
      result: undefined,
      error: undefined,
      inputData: undefined,
      onExecute: () => {},
      onConfigChange: () => {},
    },
  },
  {
    id: 'ui_system',
    type: 'custom',
    position: { x: 200, y: 4900 },
    data: {
      label: 'System Metrics Logger',
      nodeTypeName: 'system',
      color: '#64748b',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: { action: 'log_and_save', logLevel: 'info', saveToDatabase: true },
    },
  },
  {
    id: 'ui_results_summary',
    type: 'results-display',
    position: { x: 550, y: 4900 },
    data: {
      label: 'Trading Session Summary',
      color: '#10b981',
      inputData: undefined,
    },
  },

  // Final comprehensive results
  {
    id: 'final_merge',
    type: 'custom',
    position: { x: 1100, y: 5150 },
    data: {
      label: 'Final Comprehensive Merge',
      nodeTypeName: 'merge',
      color: '#ea580c',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: { mergeMode: 'all', includeMetadata: true },
    },
  },
  {
    id: 'final_results',
    type: 'results-display',
    position: { x: 1100, y: 5400 },
    data: {
      label: 'COMPLETE WORKFLOW RESULTS',
      color: '#ea580c',
      inputData: undefined,
    },
  },
];

// ============================================================================
// COMBINE ALL PHASES
// ============================================================================
export const completeWorkflowAllNodesV2: Node[] = [
  ...phase1Triggers,
  ...phase2DataAcquisition,
  ...phase3DataProcessing,
  ...phase4Analytics,
  ...phase5AIDecision,
  ...phase6RiskManagement,
  ...phase7ExecutionControl,
  ...phase8TradingOps,
  ...phase9Monitoring,
  ...phase10Visualization,
];

// ============================================================================
// INTELLIGENT EDGE CONNECTIONS
// ============================================================================
export const completeWorkflowEdgesV2: Edge[] = [
  // ========== PHASE 1 → PHASE 2: Triggers to Data Merge ==========
  makeEdge('e1_1', 'trigger_manual', 'merge_triggers'),
  makeEdge('e1_2', 'trigger_schedule', 'merge_triggers'),
  makeEdge('e1_3', 'trigger_price', 'merge_triggers'),
  makeEdge('e1_4', 'trigger_news', 'merge_triggers'),
  makeEdge('e1_5', 'trigger_market', 'merge_triggers'),
  makeEdge('e1_6', 'trigger_webhook', 'merge_triggers'),

  // ========== PHASE 2: Trigger Merge to Data Acquisition ==========
  // Market Data Branch
  makeEdge('e2_1', 'merge_triggers', 'market_quote', undefined, undefined, 'real-time'),
  makeEdge('e2_2', 'merge_triggers', 'market_historical', undefined, undefined, 'historical'),
  makeEdge('e2_3', 'merge_triggers', 'market_depth', undefined, undefined, 'L2'),
  makeEdge('e2_4', 'merge_triggers', 'market_stream', undefined, undefined, 'stream'),
  makeEdge('e2_5', 'merge_triggers', 'market_fundamentals', undefined, undefined, 'fundamentals'),
  makeEdge('e2_6', 'merge_triggers', 'market_stats', undefined, undefined, 'stats'),
  makeEdge('e2_7', 'merge_triggers', 'market_yfinance', undefined, undefined, 'yfinance'),

  // Relationship Map Branch (parallel to market data)
  makeEdge('e2_8', 'merge_triggers', 'rel_company'),
  makeEdge('e2_9', 'rel_company', 'rel_peers'),
  makeEdge('e2_10', 'rel_peers', 'rel_institutional'),
  makeEdge('e2_11', 'rel_institutional', 'rel_funds'),
  makeEdge('e2_12', 'rel_funds', 'rel_insiders'),
  makeEdge('e2_13', 'rel_insiders', 'rel_events'),
  makeEdge('e2_14', 'rel_events', 'rel_metrics'),
  makeEdge('e2_15', 'rel_metrics', 'rel_supply_chain'),

  // ========== PHASE 2 → PHASE 3: Data Acquisition to Processing ==========
  // Market data to merge
  makeEdge('e3_1', 'market_quote', 'merge_data_sources'),
  makeEdge('e3_2', 'market_historical', 'merge_data_sources'),
  makeEdge('e3_3', 'market_depth', 'merge_data_sources'),
  makeEdge('e3_4', 'market_stream', 'merge_data_sources'),
  makeEdge('e3_5', 'market_fundamentals', 'merge_data_sources'),
  makeEdge('e3_6', 'market_stats', 'merge_data_sources'),
  makeEdge('e3_7', 'market_yfinance', 'merge_data_sources'),
  makeEdge('e3_8', 'rel_supply_chain', 'merge_data_sources', undefined, undefined, 'relationships'),

  // ========== PHASE 3: Data Processing Pipeline ==========
  // Transform chain
  makeEdge('e3_9', 'merge_data_sources', 'transform_filter', undefined, undefined, 'raw'),
  makeEdge('e3_10', 'transform_filter', 'transform_map', undefined, undefined, 'filtered'),
  makeEdge('e3_11', 'transform_map', 'transform_aggregate', undefined, undefined, 'mapped'),
  makeEdge('e3_12', 'transform_aggregate', 'transform_sort', undefined, undefined, 'aggregated'),
  makeEdge('e3_13', 'transform_sort', 'transform_groupby', undefined, undefined, 'sorted'),
  makeEdge('e3_14', 'transform_groupby', 'transform_join', undefined, undefined, 'grouped'),
  makeEdge('e3_15', 'transform_join', 'transform_dedupe', undefined, undefined, 'joined'),
  makeEdge('e3_16', 'transform_dedupe', 'transform_reshape', undefined, undefined, 'clean'),

  // Core utilities (parallel processing)
  makeEdge('e3_17', 'transform_reshape', 'util_code'),
  makeEdge('e3_18', 'util_code', 'util_filter'),
  makeEdge('e3_19', 'util_filter', 'util_merge'),
  makeEdge('e3_20', 'util_merge', 'util_set'),
  makeEdge('e3_21', 'util_set', 'util_switch'),

  // ========== PHASE 3 → PHASE 4: Processing to Analytics ==========
  makeEdge('e4_1', 'util_switch', 'merge_processed', undefined, undefined, 'processed'),
  makeEdge('e4_2', 'transform_reshape', 'merge_processed', undefined, undefined, 'reshaped'),

  // Analytics pipeline
  makeEdge('e4_3', 'merge_processed', 'analytics_indicators', undefined, undefined, 'data'),
  makeEdge('e4_4', 'merge_processed', 'analytics_portfolio_opt', undefined, undefined, 'portfolio'),
  makeEdge('e4_5', 'analytics_indicators', 'analytics_backtest', undefined, undefined, 'signals'),
  makeEdge('e4_6', 'analytics_portfolio_opt', 'analytics_risk', undefined, undefined, 'weights'),
  makeEdge('e4_7', 'analytics_backtest', 'analytics_performance', undefined, undefined, 'results'),
  makeEdge('e4_8', 'analytics_risk', 'analytics_correlation', undefined, undefined, 'risk'),
  makeEdge('e4_9', 'analytics_performance', 'analytics_correlation', undefined, undefined, 'metrics'),

  // ========== PHASE 4 → PHASE 5: Analytics to AI Decision ==========
  makeEdge('e5_1', 'analytics_correlation', 'merge_analytics', undefined, undefined, 'analytics'),
  makeEdge('e5_2', 'analytics_performance', 'merge_analytics'),
  makeEdge('e5_3', 'analytics_risk', 'merge_analytics'),

  // AI Agents (parallel analysis)
  makeEdge('e5_4', 'merge_analytics', 'agent_python', undefined, undefined, 'quant'),
  makeEdge('e5_5', 'merge_analytics', 'agent_geopolitics', undefined, undefined, 'macro'),
  makeEdge('e5_6', 'merge_analytics', 'agent_hedgefund', undefined, undefined, 'strategy'),
  makeEdge('e5_7', 'merge_analytics', 'agent_investor', undefined, undefined, 'value'),
  makeEdge('e5_8', 'merge_analytics', 'agent_multi', undefined, undefined, 'committee'),

  // All agents to mediator
  makeEdge('e5_9', 'agent_python', 'agent_mediator'),
  makeEdge('e5_10', 'agent_geopolitics', 'agent_mediator'),
  makeEdge('e5_11', 'agent_hedgefund', 'agent_mediator'),
  makeEdge('e5_12', 'agent_investor', 'agent_mediator'),
  makeEdge('e5_13', 'agent_multi', 'agent_mediator'),

  // ========== PHASE 5 → PHASE 6: AI Decision to Risk Management ==========
  makeEdge('e6_1', 'agent_mediator', 'safety_risk_check', undefined, undefined, 'decision'),
  makeEdge('e6_2', 'safety_risk_check', 'safety_position_size', undefined, undefined, 'approved'),
  makeEdge('e6_3', 'safety_position_size', 'safety_loss_limit', undefined, undefined, 'sized'),
  makeEdge('e6_4', 'safety_loss_limit', 'safety_trading_hours', undefined, undefined, 'checked'),

  // ========== PHASE 6 → PHASE 7: Risk Management to Execution Control ==========
  makeEdge('e7_1', 'safety_trading_hours', 'control_ifelse', undefined, undefined, 'safe'),
  makeEdge('e7_2', 'control_ifelse', 'control_switch', undefined, undefined, 'execute'),
  makeEdge('e7_3', 'control_switch', 'control_loop', undefined, undefined, 'route'),
  makeEdge('e7_4', 'control_loop', 'control_wait', undefined, undefined, 'slice'),
  makeEdge('e7_5', 'control_wait', 'control_split', undefined, undefined, 'delayed'),
  makeEdge('e7_6', 'control_split', 'control_merge2', undefined, undefined, 'parallel'),
  makeEdge('e7_7', 'control_merge2', 'control_error', undefined, undefined, 'merged'),

  // ========== PHASE 7 → PHASE 8: Execution Control to Trading Ops ==========
  makeEdge('e8_1', 'control_error', 'trading_place_order', undefined, undefined, 'orders'),
  makeEdge('e8_2', 'trading_place_order', 'trading_modify_order', undefined, undefined, 'placed'),
  makeEdge('e8_3', 'trading_modify_order', 'trading_cancel_order', undefined, undefined, 'modified'),
  makeEdge('e8_4', 'trading_cancel_order', 'trading_get_positions', undefined, undefined, 'managed'),
  makeEdge('e8_5', 'trading_get_positions', 'trading_get_holdings', undefined, undefined, 'positions'),
  makeEdge('e8_6', 'trading_get_holdings', 'trading_get_orders', undefined, undefined, 'holdings'),
  makeEdge('e8_7', 'trading_get_orders', 'trading_get_balance', undefined, undefined, 'orders'),
  makeEdge('e8_8', 'trading_get_balance', 'trading_close_position', undefined, undefined, 'balance'),

  // ========== PHASE 8 → PHASE 9: Trading Ops to Monitoring ==========
  makeEdge('e9_1', 'trading_close_position', 'merge_trading', undefined, undefined, 'trades'),
  makeEdge('e9_2', 'trading_get_orders', 'merge_trading'),
  makeEdge('e9_3', 'trading_get_balance', 'merge_trading'),

  // Notifications (parallel alerts)
  makeEdge('e9_4', 'merge_trading', 'notify_webhook', undefined, undefined, 'webhook'),
  makeEdge('e9_5', 'merge_trading', 'notify_email', undefined, undefined, 'email'),
  makeEdge('e9_6', 'merge_trading', 'notify_telegram', undefined, undefined, 'telegram'),
  makeEdge('e9_7', 'merge_trading', 'notify_discord', undefined, undefined, 'discord'),
  makeEdge('e9_8', 'merge_trading', 'notify_slack', undefined, undefined, 'slack'),
  makeEdge('e9_9', 'merge_trading', 'notify_sms', undefined, undefined, 'sms'),

  // ========== PHASE 9 → PHASE 10: Monitoring to Visualization ==========
  makeEdge('e10_1', 'notify_webhook', 'merge_notifications'),
  makeEdge('e10_2', 'notify_email', 'merge_notifications'),
  makeEdge('e10_3', 'notify_telegram', 'merge_notifications'),
  makeEdge('e10_4', 'notify_discord', 'merge_notifications'),
  makeEdge('e10_5', 'notify_slack', 'merge_notifications'),
  makeEdge('e10_6', 'notify_sms', 'merge_notifications'),

  // Visualization nodes
  makeEdge('e10_7', 'merge_notifications', 'ui_datasource', undefined, undefined, 'display'),
  makeEdge('e10_8', 'ui_datasource', 'ui_mcp'),
  makeEdge('e10_9', 'ui_mcp', 'ui_technical_indicator'),
  makeEdge('e10_10', 'ui_technical_indicator', 'ui_backtest'),
  makeEdge('e10_11', 'ui_backtest', 'ui_optimization'),
  makeEdge('e10_12', 'ui_optimization', 'ui_agent_mediator'),
  makeEdge('e10_13', 'ui_agent_mediator', 'ui_system'),
  makeEdge('e10_14', 'ui_system', 'ui_results_summary'),

  // ========== FINAL MERGE: Aggregate all key results ==========
  makeEdge('e11_1', 'ui_results_summary', 'final_merge', undefined, undefined, 'ui'),
  makeEdge('e11_2', 'merge_trading', 'final_merge', undefined, undefined, 'trading'),
  makeEdge('e11_3', 'merge_analytics', 'final_merge', undefined, undefined, 'analytics'),
  makeEdge('e11_4', 'agent_mediator', 'final_merge', undefined, undefined, 'ai'),
  makeEdge('e11_5', 'merge_data_sources', 'final_merge', undefined, undefined, 'data'),

  // Final results display
  makeEdge('e_final', 'final_merge', 'final_results', undefined, undefined, 'COMPLETE'),
];

// Export as template
export const COMPLETE_WORKFLOW_ALL_82_NODES: WorkflowTemplate = {
  id: 'complete-institutional-trading-system-all-82-nodes',
  name: 'Complete Institutional Trading System - ALL 82 NODES',
  description: 'A comprehensive, production-grade algorithmic trading system demonstrating ALL 82 node types organized in 10 logical phases: (1) Multi-Trigger Entry Points, (2) Market Data + Company Intelligence Acquisition, (3) Data Processing & Transformation with Utilities, (4) Multi-Layered Analytics (Technical/Fundamental/Risk), (5) AI-Powered Multi-Agent Decision Making, (6) Pre-Trade Risk Management & Safety Checks, (7) Intelligent Execution Control Flow, (8) Full Trading Operations Lifecycle, (9) Multi-Channel Monitoring & Alerts, (10) Comprehensive Visualization & Reporting. This workflow represents a real institutional-grade trading pipeline with proper data flow, error handling, and intelligent connections.',
  category: 'advanced',
  tags: [
    'complete',
    'all-nodes',
    'institutional',
    'algorithmic-trading',
    'multi-agent-ai',
    'risk-management',
    'production-grade',
    'comprehensive',
    '10-phase-pipeline',
    '82-nodes',
    'enterprise',
  ],
  nodes: completeWorkflowAllNodesV2,
  edges: completeWorkflowEdgesV2,
};
