/**
 * Intermediate Workflow Templates
 *
 * Templates for users with some familiarity with the node editor.
 */

import { Node, Edge, MarkerType } from 'reactflow';
import type { WorkflowTemplate } from '../workflowTemplates';

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

// ─── Export ─────────────────────────────────────────────────────────────────
export const INTERMEDIATE_TEMPLATES: WorkflowTemplate[] = [
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
];
