// Constants for RelationshipMapTab
import { MarkerType } from 'reactflow';
import type { DataNodeType } from './types';

// Node type colors
export const NODE_TYPE_COLORS: Record<DataNodeType, string> = {
  events: '#ef4444',
  filings: '#3b82f6',
  financials: '#10b981',
  balance: '#f97316', // Will be overridden by theme accent
  analysts: '#8b5cf6',
  ownership: '#f59e0b',
  valuation: '#ec4899',
  info: '#14b8a6',
  executives: '#10b981',
};

// Node positions for the graph layout
export const NODE_POSITIONS = {
  company: { x: 600, y: 350 },
  events: { x: 50, y: 50 },
  filings: { x: 200, y: 80 },
  financials: { x: 500, y: 50 },
  balance: { x: 850, y: 80 },
  analysts: { x: 1100, y: 350 },
  ownership: { x: 850, y: 620 },
  valuation: { x: 450, y: 650 },
  info: { x: 100, y: 620 },
  executives: { x: 50, y: 350 },
} as const;

// Edge configuration
export const EDGE_CONFIG = {
  type: 'smoothstep' as const,
  animated: true,
  strokeWidth: 2,
  markerType: MarkerType.ArrowClosed,
};

// Cache duration in milliseconds (5 minutes)
export const CACHE_DURATION = 5 * 60 * 1000;

// Default ticker
export const DEFAULT_TICKER = 'AAPL';

// ReactFlow configuration
export const FLOW_CONFIG = {
  minZoom: 0.3,
  maxZoom: 2,
  defaultViewport: { x: 0, y: 0, zoom: 0.75 },
  fitView: true,
};

// Background configuration
export const BACKGROUND_CONFIG = {
  gap: 20,
  size: 1,
  color: '#1a1a1a',
  style: { background: '#0a0a0a' },
};
