/**
 * Predefined Workflow Templates
 *
 * Ready-to-use workflow templates that users can import to quickly
 * understand how the node editor works and start building their own workflows.
 */

import { Node, Edge, MarkerType } from 'reactflow';
import { COMPLETE_WORKFLOW_ALL_82_NODES } from './workflowTemplates-COMPLETE-ALL-82-NODES';

export interface WorkflowTemplate {
  id: string;
  name: string;
  description: string;
  category: 'beginner' | 'intermediate' | 'advanced';
  tags: string[];
  nodes: Node[];
  edges: Edge[];
}

const edgeStyle = { stroke: '#ea580c', strokeWidth: 2 };
const markerEnd = { type: MarkerType.ArrowClosed, color: '#ea580c' };

function makeEdge(id: string, source: string, target: string, sourceHandle?: string, targetHandle?: string): Edge {
  return {
    id,
    source,
    target,
    sourceHandle,
    targetHandle,
    animated: true,
    style: edgeStyle,
    markerEnd,
  };
}

// ─── Template 1: Hello World - Simple Data Pipeline ────────────────────────
const helloWorldNodes: Node[] = [
  {
    id: 'tpl_hw_1',
    type: 'custom',
    position: { x: 80, y: 200 },
    data: {
      label: 'Manual Trigger',
      nodeTypeName: 'manualTrigger',
      color: '#a855f7',
      hasInput: false,
      hasOutput: true,
      status: 'idle',
      parameters: {},
    },
  },
  {
    id: 'tpl_hw_2',
    type: 'technical-indicator',
    position: { x: 400, y: 200 },
    data: {
      label: 'Technical Indicators',
      dataSource: 'yfinance',
      symbol: 'AAPL',
      period: '1y',
      csvPath: '',
      jsonData: '',
      categories: ['momentum', 'volume', 'volatility', 'trend', 'others'],
      status: 'idle',
      result: undefined,
      error: undefined,
      onExecute: () => {},
      onParameterChange: () => {},
    },
  },
  {
    id: 'tpl_hw_3',
    type: 'results-display',
    position: { x: 720, y: 200 },
    data: {
      label: 'Results Display',
      color: '#f59e0b',
      inputData: undefined,
    },
  },
];

const helloWorldEdges: Edge[] = [
  makeEdge('tpl_hw_e1', 'tpl_hw_1', 'tpl_hw_2'),
  makeEdge('tpl_hw_e2', 'tpl_hw_2', 'tpl_hw_3'),
];

// ─── Template 2: Stock Quote Lookup ────────────────────────────────────────
const stockQuoteNodes: Node[] = [
  {
    id: 'tpl_sq_1',
    type: 'custom',
    position: { x: 80, y: 200 },
    data: {
      label: 'Manual Trigger',
      nodeTypeName: 'manualTrigger',
      color: '#a855f7',
      hasInput: false,
      hasOutput: true,
      status: 'idle',
      parameters: {},
    },
  },
  {
    id: 'tpl_sq_2',
    type: 'custom',
    position: { x: 350, y: 120 },
    data: {
      label: 'Get AAPL Quote',
      nodeTypeName: 'getQuote',
      color: '#22c55e',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: {
        symbol: 'AAPL',
        provider: 'yahoo',
      },
    },
  },
  {
    id: 'tpl_sq_3',
    type: 'custom',
    position: { x: 350, y: 280 },
    data: {
      label: 'Get MSFT Quote',
      nodeTypeName: 'getQuote',
      color: '#22c55e',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: {
        symbol: 'MSFT',
        provider: 'yahoo',
      },
    },
  },
  {
    id: 'tpl_sq_4',
    type: 'results-display',
    position: { x: 650, y: 120 },
    data: {
      label: 'AAPL Results',
      color: '#f59e0b',
      inputData: undefined,
    },
  },
  {
    id: 'tpl_sq_5',
    type: 'results-display',
    position: { x: 650, y: 280 },
    data: {
      label: 'MSFT Results',
      color: '#f59e0b',
      inputData: undefined,
    },
  },
];

const stockQuoteEdges: Edge[] = [
  makeEdge('tpl_sq_e1', 'tpl_sq_1', 'tpl_sq_2'),
  makeEdge('tpl_sq_e2', 'tpl_sq_1', 'tpl_sq_3'),
  makeEdge('tpl_sq_e3', 'tpl_sq_2', 'tpl_sq_4'),
  makeEdge('tpl_sq_e4', 'tpl_sq_3', 'tpl_sq_5'),
];

// ─── Template 3: Technical Analysis Pipeline ───────────────────────────────
const technicalAnalysisNodes: Node[] = [
  {
    id: 'tpl_ta_1',
    type: 'custom',
    position: { x: 80, y: 200 },
    data: {
      label: 'Manual Trigger',
      nodeTypeName: 'manualTrigger',
      color: '#a855f7',
      hasInput: false,
      hasOutput: true,
      status: 'idle',
      parameters: {},
    },
  },
  {
    id: 'tpl_ta_2',
    type: 'custom',
    position: { x: 330, y: 200 },
    data: {
      label: 'Get Historical Data',
      nodeTypeName: 'getHistoricalData',
      color: '#22c55e',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: {
        symbols: 'AAPL',
        interval: '1d',
        period: '6mo',
      },
    },
  },
  {
    id: 'tpl_ta_3',
    type: 'technical-indicator',
    position: { x: 600, y: 200 },
    data: {
      label: 'Calculate Indicators',
      dataSource: 'yfinance',
      symbol: 'AAPL',
      period: '6mo',
      csvPath: '',
      jsonData: '',
      categories: ['momentum', 'trend'],
      status: 'idle',
      result: undefined,
      error: undefined,
      onExecute: () => {},
      onParameterChange: () => {},
    },
  },
  {
    id: 'tpl_ta_4',
    type: 'results-display',
    position: { x: 900, y: 200 },
    data: {
      label: 'Analysis Results',
      color: '#f59e0b',
      inputData: undefined,
    },
  },
];

const technicalAnalysisEdges: Edge[] = [
  makeEdge('tpl_ta_e1', 'tpl_ta_1', 'tpl_ta_2'),
  makeEdge('tpl_ta_e2', 'tpl_ta_2', 'tpl_ta_3'),
  makeEdge('tpl_ta_e3', 'tpl_ta_3', 'tpl_ta_4'),
];

// ─── Template 4: Backtesting Workflow ──────────────────────────────────────
const backtestingNodes: Node[] = [
  {
    id: 'tpl_bt_1',
    type: 'custom',
    position: { x: 80, y: 200 },
    data: {
      label: 'Manual Trigger',
      nodeTypeName: 'manualTrigger',
      color: '#a855f7',
      hasInput: false,
      hasOutput: true,
      status: 'idle',
      parameters: {},
    },
  },
  {
    id: 'tpl_bt_2',
    type: 'custom',
    position: { x: 330, y: 200 },
    data: {
      label: 'Get Historical Data',
      nodeTypeName: 'getHistoricalData',
      color: '#22c55e',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: {
        symbols: 'AAPL,MSFT,GOOGL',
        interval: '1d',
        period: '1y',
      },
    },
  },
  {
    id: 'tpl_bt_3',
    type: 'technical-indicator',
    position: { x: 580, y: 100 },
    data: {
      label: 'Compute Indicators',
      dataSource: 'yfinance',
      symbol: 'AAPL',
      period: '1y',
      csvPath: '',
      jsonData: '',
      categories: ['momentum', 'trend', 'volatility'],
      status: 'idle',
      result: undefined,
      error: undefined,
      onExecute: () => {},
      onParameterChange: () => {},
    },
  },
  {
    id: 'tpl_bt_4',
    type: 'backtest',
    position: { x: 580, y: 300 },
    data: {
      label: 'Run Backtest',
      color: '#06b6d4',
      status: 'idle',
      parameters: {
        strategy: 'MA_Crossover',
        initialCapital: 100000,
        commission: 0.001,
        shortWindow: 20,
        longWindow: 50,
      },
    },
  },
  {
    id: 'tpl_bt_5',
    type: 'results-display',
    position: { x: 880, y: 100 },
    data: {
      label: 'Indicator Results',
      color: '#f59e0b',
      inputData: undefined,
    },
  },
  {
    id: 'tpl_bt_6',
    type: 'results-display',
    position: { x: 880, y: 300 },
    data: {
      label: 'Backtest Results',
      color: '#f59e0b',
      inputData: undefined,
    },
  },
];

const backtestingEdges: Edge[] = [
  makeEdge('tpl_bt_e1', 'tpl_bt_1', 'tpl_bt_2'),
  makeEdge('tpl_bt_e2', 'tpl_bt_2', 'tpl_bt_3'),
  makeEdge('tpl_bt_e3', 'tpl_bt_2', 'tpl_bt_4'),
  makeEdge('tpl_bt_e4', 'tpl_bt_3', 'tpl_bt_5'),
  makeEdge('tpl_bt_e5', 'tpl_bt_4', 'tpl_bt_6'),
];

// ─── Template 5: AI-Powered Stock Analysis ─────────────────────────────────
const aiAnalysisNodes: Node[] = [
  {
    id: 'tpl_ai_1',
    type: 'custom',
    position: { x: 80, y: 200 },
    data: {
      label: 'Get TSLA Data',
      nodeTypeName: 'getHistoricalData',
      color: '#22c55e',
      hasInput: false,
      hasOutput: true,
      status: 'idle',
      parameters: {
        symbols: 'TSLA',
        interval: '1d',
        period: '6mo',
      },
    },
  },
  {
    id: 'tpl_ai_2',
    type: 'technical-indicator',
    position: { x: 370, y: 100 },
    data: {
      label: 'Technical Indicators',
      dataSource: 'yfinance',
      symbol: 'TSLA',
      period: '6mo',
      csvPath: '',
      jsonData: '',
      categories: ['momentum', 'trend', 'volatility'],
      status: 'idle',
      result: undefined,
      error: undefined,
      onExecute: () => {},
      onParameterChange: () => {},
    },
  },
  {
    id: 'tpl_ai_3',
    type: 'agent-mediator',
    position: { x: 370, y: 300 },
    data: {
      label: 'AI Agent Mediator',
      selectedProvider: undefined,
      customPrompt: 'Analyze the stock data provided and give a buy/sell/hold recommendation with reasoning.',
      status: 'idle',
      result: undefined,
      error: undefined,
      inputData: undefined,
      onExecute: () => {},
      onConfigChange: () => {},
    },
  },
  {
    id: 'tpl_ai_4',
    type: 'results-display',
    position: { x: 700, y: 100 },
    data: {
      label: 'Technical Results',
      color: '#f59e0b',
      inputData: undefined,
    },
  },
  {
    id: 'tpl_ai_5',
    type: 'results-display',
    position: { x: 700, y: 300 },
    data: {
      label: 'AI Recommendation',
      color: '#8b5cf6',
      inputData: undefined,
    },
  },
];

const aiAnalysisEdges: Edge[] = [
  makeEdge('tpl_ai_e1', 'tpl_ai_1', 'tpl_ai_2'),
  makeEdge('tpl_ai_e2', 'tpl_ai_1', 'tpl_ai_3'),
  makeEdge('tpl_ai_e3', 'tpl_ai_2', 'tpl_ai_4'),
  makeEdge('tpl_ai_e4', 'tpl_ai_3', 'tpl_ai_5'),
];

// ─── Template 6: Data Transform Pipeline ───────────────────────────────────
const dataTransformNodes: Node[] = [
  {
    id: 'tpl_dt_1',
    type: 'custom',
    position: { x: 80, y: 200 },
    data: {
      label: 'Manual Trigger',
      nodeTypeName: 'manualTrigger',
      color: '#a855f7',
      hasInput: false,
      hasOutput: true,
      status: 'idle',
      parameters: {},
    },
  },
  {
    id: 'tpl_dt_2',
    type: 'custom',
    position: { x: 330, y: 200 },
    data: {
      label: 'Get Historical Data',
      nodeTypeName: 'getHistoricalData',
      color: '#22c55e',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: {
        symbols: 'AAPL',
        interval: '1d',
        period: '3mo',
      },
    },
  },
  {
    id: 'tpl_dt_3',
    type: 'custom',
    position: { x: 580, y: 120 },
    data: {
      label: 'Filter Positive Days',
      nodeTypeName: 'filter',
      color: '#ec4899',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: {
        field: 'change',
        operator: 'greaterThan',
        value: 0,
      },
    },
  },
  {
    id: 'tpl_dt_4',
    type: 'custom',
    position: { x: 580, y: 280 },
    data: {
      label: 'Sort by Volume',
      nodeTypeName: 'sort',
      color: '#ec4899',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: {
        field: 'volume',
        direction: 'descending',
      },
    },
  },
  {
    id: 'tpl_dt_5',
    type: 'results-display',
    position: { x: 860, y: 120 },
    data: {
      label: 'Positive Days',
      color: '#10b981',
      inputData: undefined,
    },
  },
  {
    id: 'tpl_dt_6',
    type: 'results-display',
    position: { x: 860, y: 280 },
    data: {
      label: 'Volume Ranked',
      color: '#f59e0b',
      inputData: undefined,
    },
  },
];

const dataTransformEdges: Edge[] = [
  makeEdge('tpl_dt_e1', 'tpl_dt_1', 'tpl_dt_2'),
  makeEdge('tpl_dt_e2', 'tpl_dt_2', 'tpl_dt_3'),
  makeEdge('tpl_dt_e3', 'tpl_dt_2', 'tpl_dt_4'),
  makeEdge('tpl_dt_e4', 'tpl_dt_3', 'tpl_dt_5'),
  makeEdge('tpl_dt_e5', 'tpl_dt_4', 'tpl_dt_6'),
];

// ─── Template 7: Portfolio Risk Analysis ────────────────────────────────────
const portfolioRiskNodes: Node[] = [
  {
    id: 'tpl_pr_1',
    type: 'custom',
    position: { x: 80, y: 200 },
    data: {
      label: 'Manual Trigger',
      nodeTypeName: 'manualTrigger',
      color: '#a855f7',
      hasInput: false,
      hasOutput: true,
      status: 'idle',
      parameters: {},
    },
  },
  {
    id: 'tpl_pr_2',
    type: 'custom',
    position: { x: 330, y: 200 },
    data: {
      label: 'Get Portfolio Data',
      nodeTypeName: 'getHistoricalData',
      color: '#22c55e',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: {
        symbols: 'AAPL,MSFT,GOOGL,AMZN,META',
        interval: '1d',
        period: '1y',
      },
    },
  },
  {
    id: 'tpl_pr_3',
    type: 'custom',
    position: { x: 600, y: 80 },
    data: {
      label: 'Risk Analysis',
      nodeTypeName: 'riskAnalysis',
      color: '#06b6d4',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: {
        confidenceLevel: 0.95,
        riskMetrics: ['var', 'cvar', 'maxDrawdown', 'sharpe'],
      },
    },
  },
  {
    id: 'tpl_pr_4',
    type: 'custom',
    position: { x: 600, y: 220 },
    data: {
      label: 'Correlation Matrix',
      nodeTypeName: 'correlationMatrix',
      color: '#06b6d4',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: {
        method: 'pearson',
      },
    },
  },
  {
    id: 'tpl_pr_5',
    type: 'custom',
    position: { x: 600, y: 360 },
    data: {
      label: 'Performance Metrics',
      nodeTypeName: 'performanceMetrics',
      color: '#06b6d4',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: {
        benchmarkSymbol: 'SPY',
        riskFreeRate: 0.05,
      },
    },
  },
  {
    id: 'tpl_pr_6',
    type: 'results-display',
    position: { x: 900, y: 80 },
    data: {
      label: 'Risk Report',
      color: '#ef4444',
      inputData: undefined,
    },
  },
  {
    id: 'tpl_pr_7',
    type: 'results-display',
    position: { x: 900, y: 220 },
    data: {
      label: 'Correlations',
      color: '#f59e0b',
      inputData: undefined,
    },
  },
  {
    id: 'tpl_pr_8',
    type: 'results-display',
    position: { x: 900, y: 360 },
    data: {
      label: 'Performance',
      color: '#10b981',
      inputData: undefined,
    },
  },
];

const portfolioRiskEdges: Edge[] = [
  makeEdge('tpl_pr_e1', 'tpl_pr_1', 'tpl_pr_2'),
  makeEdge('tpl_pr_e2', 'tpl_pr_2', 'tpl_pr_3'),
  makeEdge('tpl_pr_e3', 'tpl_pr_2', 'tpl_pr_4'),
  makeEdge('tpl_pr_e4', 'tpl_pr_2', 'tpl_pr_5'),
  makeEdge('tpl_pr_e5', 'tpl_pr_3', 'tpl_pr_6'),
  makeEdge('tpl_pr_e6', 'tpl_pr_4', 'tpl_pr_7'),
  makeEdge('tpl_pr_e7', 'tpl_pr_5', 'tpl_pr_8'),
];

// ─── Template 8: Optimization Workflow ──────────────────────────────────────
const optimizationNodes: Node[] = [
  {
    id: 'tpl_op_1',
    type: 'custom',
    position: { x: 80, y: 200 },
    data: {
      label: 'Get Historical Data',
      nodeTypeName: 'getHistoricalData',
      color: '#22c55e',
      hasInput: false,
      hasOutput: true,
      status: 'idle',
      parameters: {
        symbols: 'AAPL',
        interval: '1d',
        period: '1y',
      },
    },
  },
  {
    id: 'tpl_op_2',
    type: 'backtest',
    position: { x: 370, y: 200 },
    data: {
      label: 'Strategy Backtest',
      color: '#06b6d4',
      status: 'idle',
      parameters: {
        strategy: 'MA_Crossover',
        initialCapital: 100000,
        commission: 0.001,
      },
    },
  },
  {
    id: 'tpl_op_3',
    type: 'optimization',
    position: { x: 660, y: 200 },
    data: {
      label: 'Optimize Parameters',
      color: '#a855f7',
      status: 'idle',
      parameters: {
        method: 'grid',
        objective: 'sharpe_ratio',
        parameterRanges: {
          shortWindow: { min: 5, max: 50, step: 5 },
          longWindow: { min: 20, max: 200, step: 10 },
        },
      },
    },
  },
  {
    id: 'tpl_op_4',
    type: 'results-display',
    position: { x: 960, y: 200 },
    data: {
      label: 'Optimal Parameters',
      color: '#f59e0b',
      inputData: undefined,
    },
  },
];

const optimizationEdges: Edge[] = [
  makeEdge('tpl_op_e1', 'tpl_op_1', 'tpl_op_2'),
  makeEdge('tpl_op_e2', 'tpl_op_2', 'tpl_op_3'),
  makeEdge('tpl_op_e3', 'tpl_op_3', 'tpl_op_4'),
];

// ─── Template 9: Complete Financial Workflow - ALL NODES ───────────────────
const completeFinancialNodes: Node[] = [
  // ========== TRIGGER NODES (Row 1) ==========
  // 1. ManualTriggerNode
  {
    id: 'complete_trigger_manual',
    type: 'custom',
    position: { x: 50, y: 50 },
    data: {
      label: 'Manual Trigger',
      nodeTypeName: 'manualTrigger',
      color: '#a855f7',
      hasInput: false,
      hasOutput: true,
      status: 'idle',
      parameters: {},
    },
  },
  // 2. ScheduleTriggerNode
  {
    id: 'complete_trigger_schedule',
    type: 'custom',
    position: { x: 250, y: 50 },
    data: {
      label: 'Schedule Trigger',
      nodeTypeName: 'scheduleTrigger',
      color: '#a855f7',
      hasInput: false,
      hasOutput: true,
      status: 'idle',
      parameters: {
        schedule: '0 9 * * 1-5', // Weekdays at 9 AM
      },
    },
  },
  // 3. PriceAlertTriggerNode
  {
    id: 'complete_trigger_price',
    type: 'custom',
    position: { x: 450, y: 50 },
    data: {
      label: 'Price Alert',
      nodeTypeName: 'priceAlertTrigger',
      color: '#a855f7',
      hasInput: false,
      hasOutput: true,
      status: 'idle',
      parameters: {
        symbol: 'AAPL',
        condition: 'above',
        threshold: 180,
      },
    },
  },
  // 4. NewsEventTriggerNode
  {
    id: 'complete_trigger_news',
    type: 'custom',
    position: { x: 650, y: 50 },
    data: {
      label: 'News Event',
      nodeTypeName: 'newsEventTrigger',
      color: '#a855f7',
      hasInput: false,
      hasOutput: true,
      status: 'idle',
      parameters: {
        keywords: ['earnings', 'merger'],
      },
    },
  },
  // 5. MarketEventTriggerNode
  {
    id: 'complete_trigger_market',
    type: 'custom',
    position: { x: 850, y: 50 },
    data: {
      label: 'Market Event',
      nodeTypeName: 'marketEventTrigger',
      color: '#a855f7',
      hasInput: false,
      hasOutput: true,
      status: 'idle',
      parameters: {
        eventType: 'volatilitySpike',
      },
    },
  },
  // 6. WebhookTriggerNode
  {
    id: 'complete_trigger_webhook',
    type: 'custom',
    position: { x: 1050, y: 50 },
    data: {
      label: 'Webhook Trigger',
      nodeTypeName: 'webhookTrigger',
      color: '#a855f7',
      hasInput: false,
      hasOutput: true,
      status: 'idle',
      parameters: {
        path: '/webhook/trading-signal',
      },
    },
  },

  // ========== CONTROL FLOW - MERGE (Row 2) ==========
  // Merge all triggers
  {
    id: 'complete_control_merge1',
    type: 'custom',
    position: { x: 550, y: 200 },
    data: {
      label: 'Merge Triggers',
      nodeTypeName: 'merge',
      color: '#f59e0b',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: {
        mergeMode: 'all',
      },
    },
  },

  // ========== MARKET DATA NODES (Row 3) ==========
  // 7. GetQuoteNode
  {
    id: 'complete_market_quote',
    type: 'custom',
    position: { x: 50, y: 350 },
    data: {
      label: 'Get Quote',
      nodeTypeName: 'getQuote',
      color: '#22c55e',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: {
        symbol: 'AAPL',
        provider: 'yahoo',
      },
    },
  },
  // 8. GetHistoricalDataNode
  {
    id: 'complete_market_historical',
    type: 'custom',
    position: { x: 250, y: 350 },
    data: {
      label: 'Historical Data',
      nodeTypeName: 'getHistoricalData',
      color: '#22c55e',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: {
        symbols: 'AAPL,MSFT,GOOGL,AMZN,META',
        interval: '1d',
        period: '1y',
      },
    },
  },
  // 9. GetMarketDepthNode
  {
    id: 'complete_market_depth',
    type: 'custom',
    position: { x: 450, y: 350 },
    data: {
      label: 'Market Depth',
      nodeTypeName: 'getMarketDepth',
      color: '#22c55e',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: {
        symbol: 'AAPL',
        levels: 10,
      },
    },
  },
  // 10. StreamQuotesNode
  {
    id: 'complete_market_stream',
    type: 'custom',
    position: { x: 650, y: 350 },
    data: {
      label: 'Stream Quotes',
      nodeTypeName: 'streamQuotes',
      color: '#22c55e',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: {
        symbols: ['AAPL', 'MSFT'],
      },
    },
  },
  // 11. GetFundamentalsNode
  {
    id: 'complete_market_fundamentals',
    type: 'custom',
    position: { x: 850, y: 350 },
    data: {
      label: 'Fundamentals',
      nodeTypeName: 'getFundamentals',
      color: '#22c55e',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: {
        symbol: 'AAPL',
      },
    },
  },
  // 12. GetTickerStatsNode
  {
    id: 'complete_market_stats',
    type: 'custom',
    position: { x: 1050, y: 350 },
    data: {
      label: 'Ticker Stats',
      nodeTypeName: 'getTickerStats',
      color: '#22c55e',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: {
        symbol: 'AAPL',
      },
    },
  },
  // 13. YFinanceNode
  {
    id: 'complete_market_yfinance',
    type: 'custom',
    position: { x: 1250, y: 350 },
    data: {
      label: 'YFinance Data',
      nodeTypeName: 'yfinance',
      color: '#22c55e',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: {
        symbol: 'AAPL',
        period: '1y',
      },
    },
  },

  // ========== TRANSFORM NODES (Row 4) ==========
  // 14. FilterNode
  {
    id: 'complete_transform_filter',
    type: 'custom',
    position: { x: 50, y: 550 },
    data: {
      label: 'Filter Data',
      nodeTypeName: 'filter',
      color: '#ec4899',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: {
        field: 'volume',
        operator: 'greaterThan',
        value: 1000000,
      },
    },
  },
  // 15. MapNode
  {
    id: 'complete_transform_map',
    type: 'custom',
    position: { x: 250, y: 550 },
    data: {
      label: 'Map Transform',
      nodeTypeName: 'map',
      color: '#ec4899',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: {
        expression: 'item.close * 1.05',
      },
    },
  },
  // 16. AggregateNode
  {
    id: 'complete_transform_aggregate',
    type: 'custom',
    position: { x: 450, y: 550 },
    data: {
      label: 'Aggregate',
      nodeTypeName: 'aggregate',
      color: '#ec4899',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: {
        operation: 'sum',
        field: 'volume',
      },
    },
  },
  // 17. SortNode
  {
    id: 'complete_transform_sort',
    type: 'custom',
    position: { x: 650, y: 550 },
    data: {
      label: 'Sort Data',
      nodeTypeName: 'sort',
      color: '#ec4899',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: {
        field: 'volume',
        direction: 'descending',
      },
    },
  },
  // 18. GroupByNode
  {
    id: 'complete_transform_groupby',
    type: 'custom',
    position: { x: 850, y: 550 },
    data: {
      label: 'Group By',
      nodeTypeName: 'groupBy',
      color: '#ec4899',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: {
        field: 'sector',
      },
    },
  },
  // 19. JoinNode
  {
    id: 'complete_transform_join',
    type: 'custom',
    position: { x: 1050, y: 550 },
    data: {
      label: 'Join Datasets',
      nodeTypeName: 'join',
      color: '#ec4899',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: {
        joinKey: 'symbol',
        joinType: 'inner',
      },
    },
  },
  // 20. DeduplicateNode
  {
    id: 'complete_transform_dedupe',
    type: 'custom',
    position: { x: 1250, y: 550 },
    data: {
      label: 'Deduplicate',
      nodeTypeName: 'deduplicate',
      color: '#ec4899',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: {
        uniqueField: 'symbol',
      },
    },
  },
  // 21. ReshapeNode
  {
    id: 'complete_transform_reshape',
    type: 'custom',
    position: { x: 1450, y: 550 },
    data: {
      label: 'Reshape',
      nodeTypeName: 'reshape',
      color: '#ec4899',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: {
        outputFormat: 'array',
      },
    },
  },

  // ========== ANALYTICS NODES (Row 5) ==========
  // 22. TechnicalIndicatorsNode
  {
    id: 'complete_analytics_indicators',
    type: 'technical-indicator',
    position: { x: 50, y: 750 },
    data: {
      label: 'Tech Indicators',
      dataSource: 'yfinance',
      symbol: 'AAPL',
      period: '1y',
      csvPath: '',
      jsonData: '',
      categories: ['momentum', 'trend', 'volatility'],
      status: 'idle',
      result: undefined,
      error: undefined,
      onExecute: () => {},
      onParameterChange: () => {},
    },
  },
  // 23. PortfolioOptimizationNode
  {
    id: 'complete_analytics_portfolio_opt',
    type: 'custom',
    position: { x: 300, y: 750 },
    data: {
      label: 'Portfolio Optimization',
      nodeTypeName: 'portfolioOptimization',
      color: '#06b6d4',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: {
        method: 'max_sharpe',
        riskFreeRate: 0.05,
      },
    },
  },
  // 24. BacktestEngineNode
  {
    id: 'complete_analytics_backtest',
    type: 'backtest',
    position: { x: 550, y: 750 },
    data: {
      label: 'Backtest Engine',
      color: '#06b6d4',
      status: 'idle',
      parameters: {
        strategy: 'MA_Crossover',
        initialCapital: 100000,
        commission: 0.001,
        shortWindow: 20,
        longWindow: 50,
      },
    },
  },
  // 25. RiskAnalysisNode
  {
    id: 'complete_analytics_risk',
    type: 'custom',
    position: { x: 800, y: 750 },
    data: {
      label: 'Risk Analysis',
      nodeTypeName: 'riskAnalysis',
      color: '#06b6d4',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: {
        confidenceLevel: 0.95,
        riskMetrics: ['var', 'cvar', 'maxDrawdown'],
      },
    },
  },
  // 26. PerformanceMetricsNode
  {
    id: 'complete_analytics_performance',
    type: 'custom',
    position: { x: 1050, y: 750 },
    data: {
      label: 'Performance Metrics',
      nodeTypeName: 'performanceMetrics',
      color: '#06b6d4',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: {
        benchmarkSymbol: 'SPY',
        riskFreeRate: 0.05,
      },
    },
  },
  // 27. CorrelationMatrixNode
  {
    id: 'complete_analytics_correlation',
    type: 'custom',
    position: { x: 1300, y: 750 },
    data: {
      label: 'Correlation Matrix',
      nodeTypeName: 'correlationMatrix',
      color: '#06b6d4',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: {
        method: 'pearson',
      },
    },
  },

  // ========== CONTROL FLOW NODES (Row 6) ==========
  // 28. IfElseNode
  {
    id: 'complete_control_ifelse',
    type: 'custom',
    position: { x: 50, y: 950 },
    data: {
      label: 'If-Else Branch',
      nodeTypeName: 'ifElse',
      color: '#f59e0b',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: {
        condition: 'sharpe_ratio > 1.5',
      },
    },
  },
  // 29. SwitchNode
  {
    id: 'complete_control_switch',
    type: 'custom',
    position: { x: 300, y: 950 },
    data: {
      label: 'Switch',
      nodeTypeName: 'switch',
      color: '#f59e0b',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: {
        cases: ['bullish', 'bearish', 'neutral'],
      },
    },
  },
  // 30. LoopNode
  {
    id: 'complete_control_loop',
    type: 'custom',
    position: { x: 550, y: 950 },
    data: {
      label: 'Loop',
      nodeTypeName: 'loop',
      color: '#f59e0b',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: {
        maxIterations: 100,
      },
    },
  },
  // 31. WaitNode
  {
    id: 'complete_control_wait',
    type: 'custom',
    position: { x: 800, y: 950 },
    data: {
      label: 'Wait/Delay',
      nodeTypeName: 'wait',
      color: '#f59e0b',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: {
        duration: 5000, // 5 seconds
      },
    },
  },
  // 32. SplitNode
  {
    id: 'complete_control_split',
    type: 'custom',
    position: { x: 1050, y: 950 },
    data: {
      label: 'Split Data',
      nodeTypeName: 'split',
      color: '#f59e0b',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: {
        splitCount: 3,
      },
    },
  },
  // 33. ErrorHandlerNode
  {
    id: 'complete_control_error',
    type: 'custom',
    position: { x: 1300, y: 950 },
    data: {
      label: 'Error Handler',
      nodeTypeName: 'errorHandler',
      color: '#ef4444',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: {
        retryCount: 3,
        fallbackBehavior: 'continue',
      },
    },
  },

  // ========== SAFETY NODES (Row 7) ==========
  // 34. RiskCheckNode
  {
    id: 'complete_safety_risk_check',
    type: 'custom',
    position: { x: 50, y: 1150 },
    data: {
      label: 'Risk Check',
      nodeTypeName: 'riskCheck',
      color: '#ef4444',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: {
        maxRiskPerTrade: 0.02,
      },
    },
  },
  // 35. PositionSizeLimitNode
  {
    id: 'complete_safety_position_size',
    type: 'custom',
    position: { x: 350, y: 1150 },
    data: {
      label: 'Position Size Limit',
      nodeTypeName: 'positionSizeLimit',
      color: '#ef4444',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: {
        maxPositionSize: 10000,
      },
    },
  },
  // 36. LossLimitNode
  {
    id: 'complete_safety_loss_limit',
    type: 'custom',
    position: { x: 650, y: 1150 },
    data: {
      label: 'Loss Limit',
      nodeTypeName: 'lossLimit',
      color: '#ef4444',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: {
        maxDailyLoss: 1000,
      },
    },
  },
  // 37. TradingHoursCheckNode
  {
    id: 'complete_safety_trading_hours',
    type: 'custom',
    position: { x: 950, y: 1150 },
    data: {
      label: 'Trading Hours Check',
      nodeTypeName: 'tradingHoursCheck',
      color: '#ef4444',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: {
        marketHours: '09:30-16:00',
      },
    },
  },

  // ========== TRADING NODES (Row 8) ==========
  // 38. PlaceOrderNode
  {
    id: 'complete_trading_place_order',
    type: 'custom',
    position: { x: 50, y: 1350 },
    data: {
      label: 'Place Order',
      nodeTypeName: 'placeOrder',
      color: '#10b981',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: {
        symbol: 'AAPL',
        side: 'buy',
        quantity: 100,
        orderType: 'limit',
        price: 180,
      },
    },
  },
  // 39. ModifyOrderNode
  {
    id: 'complete_trading_modify_order',
    type: 'custom',
    position: { x: 300, y: 1350 },
    data: {
      label: 'Modify Order',
      nodeTypeName: 'modifyOrder',
      color: '#10b981',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: {
        orderId: '12345',
        newPrice: 185,
      },
    },
  },
  // 40. CancelOrderNode
  {
    id: 'complete_trading_cancel_order',
    type: 'custom',
    position: { x: 550, y: 1350 },
    data: {
      label: 'Cancel Order',
      nodeTypeName: 'cancelOrder',
      color: '#10b981',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: {
        orderId: '12345',
      },
    },
  },
  // 41. GetPositionsNode
  {
    id: 'complete_trading_get_positions',
    type: 'custom',
    position: { x: 800, y: 1350 },
    data: {
      label: 'Get Positions',
      nodeTypeName: 'getPositions',
      color: '#10b981',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: {},
    },
  },
  // 42. GetHoldingsNode
  {
    id: 'complete_trading_get_holdings',
    type: 'custom',
    position: { x: 1050, y: 1350 },
    data: {
      label: 'Get Holdings',
      nodeTypeName: 'getHoldings',
      color: '#10b981',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: {},
    },
  },
  // 43. GetOrdersNode
  {
    id: 'complete_trading_get_orders',
    type: 'custom',
    position: { x: 1300, y: 1350 },
    data: {
      label: 'Get Orders',
      nodeTypeName: 'getOrders',
      color: '#10b981',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: {},
    },
  },
  // 44. GetBalanceNode
  {
    id: 'complete_trading_get_balance',
    type: 'custom',
    position: { x: 50, y: 1500 },
    data: {
      label: 'Get Balance',
      nodeTypeName: 'getBalance',
      color: '#10b981',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: {},
    },
  },
  // 45. ClosePositionNode
  {
    id: 'complete_trading_close_position',
    type: 'custom',
    position: { x: 300, y: 1500 },
    data: {
      label: 'Close Position',
      nodeTypeName: 'closePosition',
      color: '#10b981',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: {
        symbol: 'AAPL',
      },
    },
  },

  // ========== AGENT NODES (Row 9) ==========
  // 46. AgentMediatorNode
  {
    id: 'complete_agent_mediator',
    type: 'agent-mediator',
    position: { x: 550, y: 1500 },
    data: {
      label: 'Agent Mediator',
      selectedProvider: undefined,
      customPrompt: 'Synthesize all analysis results',
      status: 'idle',
      result: undefined,
      error: undefined,
      inputData: undefined,
      onExecute: () => {},
      onConfigChange: () => {},
    },
  },
  // 47. PythonAgentNode
  {
    id: 'complete_agent_python',
    type: 'python-agent',
    position: { x: 800, y: 1500 },
    data: {
      agentType: 'portfolio_analysis',
      agentCategory: 'financial',
      label: 'Python Agent',
      icon: 'brain',
      color: '#8b5cf6',
      parameters: {
        analysisType: 'risk',
        timeframe: '1y',
      },
      selectedLLM: 'active',
      status: 'idle',
      result: undefined,
      error: undefined,
      onExecute: () => {},
      onParameterChange: () => {},
      onLLMChange: () => {},
    },
  },
  // 48. GeopoliticsAgentNode
  {
    id: 'complete_agent_geopolitics',
    type: 'custom',
    position: { x: 1050, y: 1500 },
    data: {
      label: 'Geopolitics Agent',
      nodeTypeName: 'geopoliticsAgent',
      color: '#8b5cf6',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: {
        region: 'global',
        analysisType: 'risk',
      },
    },
  },
  // 49. HedgeFundAgentNode
  {
    id: 'complete_agent_hedgefund',
    type: 'custom',
    position: { x: 1300, y: 1500 },
    data: {
      label: 'Hedge Fund Agent',
      nodeTypeName: 'hedgeFundAgent',
      color: '#8b5cf6',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: {
        strategy: 'long_short_equity',
      },
    },
  },
  // 50. InvestorAgentNode
  {
    id: 'complete_agent_investor',
    type: 'custom',
    position: { x: 550, y: 1650 },
    data: {
      label: 'Investor Agent',
      nodeTypeName: 'investorAgent',
      color: '#8b5cf6',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: {
        persona: 'warren_buffett',
      },
    },
  },
  // 51. MultiAgentNode
  {
    id: 'complete_agent_multi',
    type: 'custom',
    position: { x: 800, y: 1650 },
    data: {
      label: 'Multi-Agent',
      nodeTypeName: 'multiAgent',
      color: '#8b5cf6',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: {
        agents: ['analyst', 'trader', 'risk_manager'],
      },
    },
  },

  // ========== NOTIFICATION NODES (Row 10) ==========
  // 52. WebhookNotificationNode
  {
    id: 'complete_notify_webhook',
    type: 'custom',
    position: { x: 50, y: 1800 },
    data: {
      label: 'Webhook Notify',
      nodeTypeName: 'webhookNotification',
      color: '#06b6d4',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: {
        url: 'https://example.com/webhook',
      },
    },
  },
  // 53. EmailNode
  {
    id: 'complete_notify_email',
    type: 'custom',
    position: { x: 300, y: 1800 },
    data: {
      label: 'Email Notify',
      nodeTypeName: 'email',
      color: '#06b6d4',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: {
        to: 'trader@example.com',
        subject: 'Trading Alert',
      },
    },
  },
  // 54. TelegramNode
  {
    id: 'complete_notify_telegram',
    type: 'custom',
    position: { x: 550, y: 1800 },
    data: {
      label: 'Telegram',
      nodeTypeName: 'telegram',
      color: '#06b6d4',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: {
        chatId: '123456789',
      },
    },
  },
  // 55. DiscordNode
  {
    id: 'complete_notify_discord',
    type: 'custom',
    position: { x: 800, y: 1800 },
    data: {
      label: 'Discord',
      nodeTypeName: 'discord',
      color: '#06b6d4',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: {
        webhookUrl: 'https://discord.com/webhook',
      },
    },
  },
  // 56. SlackNode
  {
    id: 'complete_notify_slack',
    type: 'custom',
    position: { x: 1050, y: 1800 },
    data: {
      label: 'Slack',
      nodeTypeName: 'slack',
      color: '#06b6d4',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: {
        channel: '#trading-alerts',
      },
    },
  },
  // 57. SMSNode
  {
    id: 'complete_notify_sms',
    type: 'custom',
    position: { x: 1300, y: 1800 },
    data: {
      label: 'SMS',
      nodeTypeName: 'sms',
      color: '#06b6d4',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: {
        phoneNumber: '+1234567890',
      },
    },
  },

  // ========== UI/VISUAL NODES (Row 11) ==========
  // 58. DataSourceNode
  {
    id: 'complete_ui_datasource',
    type: 'data-source',
    position: { x: 50, y: 2000 },
    data: {
      label: 'Data Source',
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
  // 59. MCPToolNode
  {
    id: 'complete_ui_mcp',
    type: 'mcp-tool',
    position: { x: 300, y: 2000 },
    data: {
      label: 'MCP Tool',
      serverName: 'financial-api',
      toolName: 'fetch_market_data',
      parameters: {},
      status: 'idle',
      result: undefined,
      error: undefined,
      onExecute: () => {},
      onParameterChange: () => {},
    },
  },
  // 60. OptimizationNode
  {
    id: 'complete_ui_optimization',
    type: 'optimization',
    position: { x: 550, y: 2000 },
    data: {
      label: 'Optimization',
      color: '#a855f7',
      status: 'idle',
      parameters: {
        method: 'bayesian',
        objective: 'sharpe_ratio',
      },
    },
  },
  // 61. SystemNode
  {
    id: 'complete_ui_system',
    type: 'custom',
    position: { x: 800, y: 2000 },
    data: {
      label: 'System Node',
      nodeTypeName: 'system',
      color: '#64748b',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: {
        action: 'log',
      },
    },
  },
  // 62. ResultsDisplayNode
  {
    id: 'complete_ui_results',
    type: 'results-display',
    position: { x: 1050, y: 2000 },
    data: {
      label: 'Results Display',
      color: '#f59e0b',
      inputData: undefined,
    },
  },

  // ========== FINAL MERGE & DISPLAY (Row 12) ==========
  {
    id: 'complete_final_merge',
    type: 'custom',
    position: { x: 550, y: 2200 },
    data: {
      label: 'Final Merge',
      nodeTypeName: 'merge',
      color: '#f59e0b',
      hasInput: true,
      hasOutput: true,
      status: 'idle',
      parameters: {},
    },
  },
  {
    id: 'complete_final_results',
    type: 'results-display',
    position: { x: 550, y: 2400 },
    data: {
      label: 'Complete Workflow Results',
      color: '#10b981',
      inputData: undefined,
    },
  },
];

const completeFinancialEdges: Edge[] = [
  // Connect all triggers to first merge
  makeEdge('e_trig_1', 'complete_trigger_manual', 'complete_control_merge1'),
  makeEdge('e_trig_2', 'complete_trigger_schedule', 'complete_control_merge1'),
  makeEdge('e_trig_3', 'complete_trigger_price', 'complete_control_merge1'),
  makeEdge('e_trig_4', 'complete_trigger_news', 'complete_control_merge1'),
  makeEdge('e_trig_5', 'complete_trigger_market', 'complete_control_merge1'),
  makeEdge('e_trig_6', 'complete_trigger_webhook', 'complete_control_merge1'),

  // Connect merge to market data nodes
  makeEdge('e_md_1', 'complete_control_merge1', 'complete_market_quote'),
  makeEdge('e_md_2', 'complete_control_merge1', 'complete_market_historical'),
  makeEdge('e_md_3', 'complete_control_merge1', 'complete_market_depth'),
  makeEdge('e_md_4', 'complete_control_merge1', 'complete_market_stream'),
  makeEdge('e_md_5', 'complete_control_merge1', 'complete_market_fundamentals'),
  makeEdge('e_md_6', 'complete_control_merge1', 'complete_market_stats'),
  makeEdge('e_md_7', 'complete_control_merge1', 'complete_market_yfinance'),

  // Connect market data to transform nodes
  makeEdge('e_tf_1', 'complete_market_historical', 'complete_transform_filter'),
  makeEdge('e_tf_2', 'complete_market_historical', 'complete_transform_map'),
  makeEdge('e_tf_3', 'complete_transform_filter', 'complete_transform_aggregate'),
  makeEdge('e_tf_4', 'complete_transform_map', 'complete_transform_sort'),
  makeEdge('e_tf_5', 'complete_transform_aggregate', 'complete_transform_groupby'),
  makeEdge('e_tf_6', 'complete_transform_sort', 'complete_transform_join'),
  makeEdge('e_tf_7', 'complete_transform_groupby', 'complete_transform_dedupe'),
  makeEdge('e_tf_8', 'complete_transform_join', 'complete_transform_reshape'),

  // Connect transform to analytics nodes
  makeEdge('e_an_1', 'complete_transform_reshape', 'complete_analytics_indicators'),
  makeEdge('e_an_2', 'complete_transform_reshape', 'complete_analytics_portfolio_opt'),
  makeEdge('e_an_3', 'complete_analytics_indicators', 'complete_analytics_backtest'),
  makeEdge('e_an_4', 'complete_analytics_portfolio_opt', 'complete_analytics_risk'),
  makeEdge('e_an_5', 'complete_analytics_backtest', 'complete_analytics_performance'),
  makeEdge('e_an_6', 'complete_analytics_risk', 'complete_analytics_correlation'),

  // Connect analytics to control flow
  makeEdge('e_cf_1', 'complete_analytics_correlation', 'complete_control_ifelse'),
  makeEdge('e_cf_2', 'complete_control_ifelse', 'complete_control_switch'),
  makeEdge('e_cf_3', 'complete_control_switch', 'complete_control_loop'),
  makeEdge('e_cf_4', 'complete_control_loop', 'complete_control_wait'),
  makeEdge('e_cf_5', 'complete_control_wait', 'complete_control_split'),
  makeEdge('e_cf_6', 'complete_control_split', 'complete_control_error'),

  // Connect control flow to safety nodes
  makeEdge('e_sf_1', 'complete_control_error', 'complete_safety_risk_check'),
  makeEdge('e_sf_2', 'complete_safety_risk_check', 'complete_safety_position_size'),
  makeEdge('e_sf_3', 'complete_safety_position_size', 'complete_safety_loss_limit'),
  makeEdge('e_sf_4', 'complete_safety_loss_limit', 'complete_safety_trading_hours'),

  // Connect safety to trading nodes
  makeEdge('e_tr_1', 'complete_safety_trading_hours', 'complete_trading_place_order'),
  makeEdge('e_tr_2', 'complete_trading_place_order', 'complete_trading_modify_order'),
  makeEdge('e_tr_3', 'complete_trading_modify_order', 'complete_trading_cancel_order'),
  makeEdge('e_tr_4', 'complete_trading_cancel_order', 'complete_trading_get_positions'),
  makeEdge('e_tr_5', 'complete_trading_get_positions', 'complete_trading_get_holdings'),
  makeEdge('e_tr_6', 'complete_trading_get_holdings', 'complete_trading_get_orders'),
  makeEdge('e_tr_7', 'complete_trading_get_orders', 'complete_trading_get_balance'),
  makeEdge('e_tr_8', 'complete_trading_get_balance', 'complete_trading_close_position'),

  // Connect trading to agent nodes
  makeEdge('e_ag_1', 'complete_trading_close_position', 'complete_agent_mediator'),
  makeEdge('e_ag_2', 'complete_agent_mediator', 'complete_agent_python'),
  makeEdge('e_ag_3', 'complete_agent_python', 'complete_agent_geopolitics'),
  makeEdge('e_ag_4', 'complete_agent_geopolitics', 'complete_agent_hedgefund'),
  makeEdge('e_ag_5', 'complete_agent_hedgefund', 'complete_agent_investor'),
  makeEdge('e_ag_6', 'complete_agent_investor', 'complete_agent_multi'),

  // Connect agents to notifications
  makeEdge('e_nt_1', 'complete_agent_multi', 'complete_notify_webhook'),
  makeEdge('e_nt_2', 'complete_notify_webhook', 'complete_notify_email'),
  makeEdge('e_nt_3', 'complete_notify_email', 'complete_notify_telegram'),
  makeEdge('e_nt_4', 'complete_notify_telegram', 'complete_notify_discord'),
  makeEdge('e_nt_5', 'complete_notify_discord', 'complete_notify_slack'),
  makeEdge('e_nt_6', 'complete_notify_slack', 'complete_notify_sms'),

  // Connect notifications to UI nodes
  makeEdge('e_ui_1', 'complete_notify_sms', 'complete_ui_datasource'),
  makeEdge('e_ui_2', 'complete_ui_datasource', 'complete_ui_mcp'),
  makeEdge('e_ui_3', 'complete_ui_mcp', 'complete_ui_optimization'),
  makeEdge('e_ui_4', 'complete_ui_optimization', 'complete_ui_system'),
  makeEdge('e_ui_5', 'complete_ui_system', 'complete_ui_results'),

  // Connect UI results to final merge
  makeEdge('e_final_1', 'complete_ui_results', 'complete_final_merge'),
  // Also merge in some earlier results
  makeEdge('e_final_2', 'complete_market_quote', 'complete_final_merge'),
  makeEdge('e_final_3', 'complete_analytics_performance', 'complete_final_merge'),
  makeEdge('e_final_4', 'complete_trading_get_balance', 'complete_final_merge'),

  // Final display
  makeEdge('e_final_display', 'complete_final_merge', 'complete_final_results'),
];

// ─── Export All Templates ───────────────────────────────────────────────────
export const WORKFLOW_TEMPLATES: WorkflowTemplate[] = [
  {
    id: 'hello-world',
    name: 'Hello World - Data Pipeline',
    description: 'A simple 3-node pipeline: trigger the workflow, calculate technical indicators for AAPL via yfinance, and display results. Great starting point to understand node connections.',
    category: 'beginner',
    tags: ['trigger', 'indicators', 'basics'],
    nodes: helloWorldNodes,
    edges: helloWorldEdges,
  },
  {
    id: 'stock-quote-lookup',
    name: 'Multi-Stock Quote Lookup',
    description: 'Fetch real-time quotes for multiple stocks (AAPL & MSFT) in parallel from a single trigger. Demonstrates branching workflows.',
    category: 'beginner',
    tags: ['quotes', 'parallel', 'market-data'],
    nodes: stockQuoteNodes,
    edges: stockQuoteEdges,
  },
  {
    id: 'technical-analysis',
    name: 'Technical Analysis Pipeline',
    description: 'Trigger historical data fetch, compute momentum and trend indicators, then display results. A linear workflow demonstrating the analysis pipeline.',
    category: 'intermediate',
    tags: ['technical-analysis', 'indicators', 'historical-data'],
    nodes: technicalAnalysisNodes,
    edges: technicalAnalysisEdges,
  },
  {
    id: 'backtesting-strategy',
    name: 'Backtesting Strategy',
    description: 'Fetch historical data for multiple stocks, compute indicators and run a moving average crossover backtest in parallel. View both indicator and backtest results.',
    category: 'intermediate',
    tags: ['backtest', 'strategy', 'moving-average'],
    nodes: backtestingNodes,
    edges: backtestingEdges,
  },
  {
    id: 'ai-stock-analysis',
    name: 'AI-Powered Stock Analysis',
    description: 'Fetch TSLA historical data, then feed it into both a technical indicator calculator and an AI agent mediator for a buy/sell/hold recommendation. Compare quantitative and AI analysis.',
    category: 'intermediate',
    tags: ['ai', 'agent', 'analysis', 'recommendation'],
    nodes: aiAnalysisNodes,
    edges: aiAnalysisEdges,
  },
  {
    id: 'data-transform',
    name: 'Data Transform & Filter',
    description: 'Fetch historical data, then split into two branches: filter for positive days and sort by volume. Demonstrates data transformation nodes.',
    category: 'beginner',
    tags: ['filter', 'sort', 'transform', 'branching'],
    nodes: dataTransformNodes,
    edges: dataTransformEdges,
  },
  {
    id: 'portfolio-risk',
    name: 'Portfolio Risk Analysis',
    description: 'Analyze a 5-stock portfolio (AAPL, MSFT, GOOGL, AMZN, META) with risk metrics, correlation matrix, and performance benchmarking against SPY.',
    category: 'advanced',
    tags: ['portfolio', 'risk', 'correlation', 'performance'],
    nodes: portfolioRiskNodes,
    edges: portfolioRiskEdges,
  },
  {
    id: 'strategy-optimization',
    name: 'Strategy Parameter Optimization',
    description: 'Fetch AAPL historical data, run a backtest, then optimize strategy parameters using grid search to find the best Sharpe ratio. Full optimization pipeline.',
    category: 'advanced',
    tags: ['optimization', 'grid-search', 'sharpe-ratio'],
    nodes: optimizationNodes,
    edges: optimizationEdges,
  },
  {
    id: 'complete-financial-workflow',
    name: 'Complete Financial Workflow - ALL NODES',
    description: 'ULTIMATE DEMO: A comprehensive workflow showcasing ALL 60+ node types in the platform. Includes triggers, market data, transforms, analytics, control flow, safety checks, trading execution, AI agents, notifications, and results display. This template demonstrates the full power of the workflow system and serves as a complete reference for all available nodes.',
    category: 'advanced',
    tags: ['complete', 'all-nodes', 'comprehensive', 'showcase', 'demo', 'reference', 'enterprise'],
    nodes: completeFinancialNodes,
    edges: completeFinancialEdges,
  },
  // NEW: Comprehensive 82-node institutional trading system
  COMPLETE_WORKFLOW_ALL_82_NODES,
];
