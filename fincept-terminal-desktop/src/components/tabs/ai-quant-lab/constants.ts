/**
 * AI Quant Lab - Constants and Configurations
 *
 * Node configurations, category settings, and other constants
 */

import React from 'react';
import {
  Database,
  Zap,
  Monitor,
  Brain,
  BarChart2,
  Activity,
  GitBranch,
  Filter,
  Shield,
  Bell,
  TrendingUp,
  Workflow,
} from 'lucide-react';
import type { NodeConfig, CategoryConfig } from './types';

// Built-in UI node configs (these have custom React components)
export const BUILTIN_NODE_CONFIGS: NodeConfig[] = [
  {
    type: 'data-source',
    label: 'Data Source',
    color: '#3b82f6',
    category: 'Input',
    hasInput: false,
    hasOutput: true,
    description: 'Connect to databases and fetch data',
  },
  {
    type: 'technical-indicator',
    label: 'Technical Indicators',
    color: '#10b981',
    category: 'Processing',
    hasInput: true,
    hasOutput: true,
    description: 'Calculate technical indicators on market data',
  },
  {
    type: 'agent-mediator',
    label: 'Agent Mediator',
    color: '#8b5cf6',
    category: 'AI',
    hasInput: true,
    hasOutput: true,
    description: 'AI-powered data analysis and decision making',
  },
  {
    type: 'results-display',
    label: 'Results Display',
    color: '#f59e0b',
    category: 'Output',
    hasInput: true,
    hasOutput: false,
    description: 'Display workflow results with formatting',
  },
  {
    type: 'backtest',
    label: 'Backtest',
    color: '#06b6d4',
    category: 'Analytics',
    hasInput: true,
    hasOutput: true,
    description: 'Run backtests on trading strategies',
  },
  {
    type: 'optimization',
    label: 'Optimization',
    color: '#a855f7',
    category: 'Analytics',
    hasInput: true,
    hasOutput: true,
    description: 'Optimize strategy parameters using grid/genetic algorithms',
  },
];

// Category icons and colors for the palette
export const CATEGORY_CONFIG: Record<string, CategoryConfig> = {
  'Core': { icon: React.createElement(Workflow, { size: 14 }), color: '#6b7280' },
  'Input': { icon: React.createElement(Database, { size: 14 }), color: '#3b82f6' },
  'Processing': { icon: React.createElement(Zap, { size: 14 }), color: '#10b981' },
  'Output': { icon: React.createElement(Monitor, { size: 14 }), color: '#f59e0b' },
  'AI': { icon: React.createElement(Brain, { size: 14 }), color: '#8b5cf6' },
  'Analytics': { icon: React.createElement(BarChart2, { size: 14 }), color: '#06b6d4' },
  'Market Data': { icon: React.createElement(Activity, { size: 14 }), color: '#22c55e' },
  'Control Flow': { icon: React.createElement(GitBranch, { size: 14 }), color: '#eab308' },
  'Transform': { icon: React.createElement(Filter, { size: 14 }), color: '#ec4899' },
  'Safety': { icon: React.createElement(Shield, { size: 14 }), color: '#ef4444' },
  'Notifications': { icon: React.createElement(Bell, { size: 14 }), color: '#f97316' },
  'Triggers': { icon: React.createElement(Zap, { size: 14 }), color: '#a855f7' },
  'Trading': { icon: React.createElement(TrendingUp, { size: 14 }), color: '#14b8a6' },
  'MCP Tools': { icon: React.createElement(Zap, { size: 14 }), color: '#ea580c' },
  'Python Agents': { icon: React.createElement(Brain, { size: 14 }), color: '#22c55e' },
  // Lowercase variants for registry nodes
  'agents': { icon: React.createElement(Brain, { size: 14 }), color: '#8b5cf6' },
  'marketData': { icon: React.createElement(Activity, { size: 14 }), color: '#22c55e' },
  'trading': { icon: React.createElement(TrendingUp, { size: 14 }), color: '#14b8a6' },
  'triggers': { icon: React.createElement(Zap, { size: 14 }), color: '#a855f7' },
  'controlFlow': { icon: React.createElement(GitBranch, { size: 14 }), color: '#eab308' },
  'transform': { icon: React.createElement(Filter, { size: 14 }), color: '#ec4899' },
  'safety': { icon: React.createElement(Shield, { size: 14 }), color: '#ef4444' },
  'notifications': { icon: React.createElement(Bell, { size: 14 }), color: '#f97316' },
  'analytics': { icon: React.createElement(BarChart2, { size: 14 }), color: '#06b6d4' },
};

// Fincept color palette
export const FINCEPT_COLORS = {
  primary: '#FF8800',
  orange: '#FFA500',
  white: '#FFFFFF',
  gray: '#787878',
  darkBg: '#0a0a0a',
  panelBg: '#1a1a1a',
  border: '#2d2d2d',
  green: '#10b981',
  red: '#ef4444',
  blue: '#3b82f6',
  purple: '#8b5cf6',
  cyan: '#06b6d4',
  yellow: '#f59e0b',
};

// Default edge options for ReactFlow
export const DEFAULT_EDGE_OPTIONS = {
  animated: true,
  style: { stroke: '#ea580c', strokeWidth: 2 },
};

// ReactFlow background settings
export const FLOW_BACKGROUND_CONFIG = {
  color: '#2d2d2d',
  gap: 20,
  size: 1,
  style: { backgroundColor: '#0a0a0a' },
};
