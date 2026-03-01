/**
 * Beginner Workflow Templates
 *
 * Simple templates for users learning the node editor.
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

// ─── Export ─────────────────────────────────────────────────────────────────
export const BEGINNER_TEMPLATES: WorkflowTemplate[] = [
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
    id: 'data-transform',
    name: 'Data Transform & Filter',
    description: 'Fetch historical data, then split into two branches: filter for positive days and sort by volume. Demonstrates data transformation nodes.',
    category: 'beginner',
    tags: ['filter', 'sort', 'transform', 'branching'],
    nodes: dataTransformNodes,
    edges: dataTransformEdges,
  },
];
