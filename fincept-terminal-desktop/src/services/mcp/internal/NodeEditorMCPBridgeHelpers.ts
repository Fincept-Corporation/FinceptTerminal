/**
 * Node Editor MCP Bridge - Helpers and Constants
 */

/**
 * Category metadata for better descriptions
 */
export const CATEGORY_METADATA: Record<string, { displayName: string; description: string; icon: string; color: string }> = {
  trigger: {
    displayName: 'Triggers',
    description: 'Workflow entry points - manual, scheduled, price alerts, market events',
    icon: 'Zap',
    color: '#a855f7',
  },
  marketData: {
    displayName: 'Market Data',
    description: 'Fetch real-time and historical market data from various sources',
    icon: 'Activity',
    color: '#22c55e',
  },
  trading: {
    displayName: 'Trading',
    description: 'Execute and manage trades - orders, positions, balance',
    icon: 'TrendingUp',
    color: '#14b8a6',
  },
  analytics: {
    displayName: 'Analytics',
    description: 'Financial analysis - technical indicators, portfolio optimization, backtesting',
    icon: 'BarChart2',
    color: '#06b6d4',
  },
  controlFlow: {
    displayName: 'Control Flow',
    description: 'Workflow logic - conditions, loops, branching, merging',
    icon: 'GitBranch',
    color: '#eab308',
  },
  transform: {
    displayName: 'Transform',
    description: 'Data manipulation - filter, map, aggregate, sort, join',
    icon: 'Filter',
    color: '#ec4899',
  },
  safety: {
    displayName: 'Safety',
    description: 'Risk management - position limits, loss limits, trading hours',
    icon: 'Shield',
    color: '#ef4444',
  },
  notifications: {
    displayName: 'Notifications',
    description: 'Send alerts - email, Telegram, Discord, Slack, SMS, webhooks',
    icon: 'Bell',
    color: '#f97316',
  },
  agents: {
    displayName: 'AI Agents',
    description: 'AI-powered analysis - investor personas, geopolitics, hedge fund strategies',
    icon: 'Brain',
    color: '#8b5cf6',
  },
  core: {
    displayName: 'Core',
    description: 'Basic workflow operations - filter, merge, set, switch, code',
    icon: 'Workflow',
    color: '#6b7280',
  },
};

/**
 * Map node group names to our categories
 */
export function mapGroupToCategory(groups: string[] | undefined): string {
  if (!groups || !Array.isArray(groups) || groups.length === 0) {
    return 'core';
  }
  const groupLower = groups.map(g => (g || '').toLowerCase());

  if (groupLower.some(g => g.includes('trigger'))) return 'trigger';
  if (groupLower.some(g => g.includes('market') || g.includes('data') || g.includes('yfinance'))) return 'marketData';
  if (groupLower.some(g => g.includes('trading') || g.includes('order') || g.includes('position'))) return 'trading';
  if (groupLower.some(g => g.includes('analytic') || g.includes('indicator') || g.includes('backtest'))) return 'analytics';
  if (groupLower.some(g => g.includes('control') || g.includes('flow') || g.includes('logic'))) return 'controlFlow';
  if (groupLower.some(g => g.includes('transform') || g.includes('filter') || g.includes('map'))) return 'transform';
  if (groupLower.some(g => g.includes('safety') || g.includes('risk'))) return 'safety';
  if (groupLower.some(g => g.includes('notification') || g.includes('alert') || g.includes('message'))) return 'notifications';
  if (groupLower.some(g => g.includes('agent') || g.includes('ai'))) return 'agents';

  return 'core';
}
