/**
 * Predefined Workflow Templates
 *
 * Ready-to-use workflow templates that users can import to quickly
 * understand how the node editor works and start building their own workflows.
 */

import { Node, Edge, MarkerType } from 'reactflow';

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
];
