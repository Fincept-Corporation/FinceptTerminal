// Forum Tab Constants

import type { ForumUser, ForumColors } from './types';

// Helper function to get forum colors from theme
export const getForumColors = (colors: any): ForumColors => ({
  ORANGE: colors.primary,
  WHITE: colors.text,
  RED: colors.alert,
  GREEN: colors.secondary,
  YELLOW: colors.warning,
  GRAY: colors.textMuted,
  BLUE: colors.info,
  PURPLE: colors.purple,
  CYAN: colors.accent,
  DARK_BG: colors.background,
  PANEL_BG: colors.panel
});

// Top contributors - static data
export const TOP_CONTRIBUTORS: ForumUser[] = [
  { username: 'QuantTrader_Pro', reputation: 8947, posts: 1234, joined: '2021-03-15', status: 'ONLINE' },
  { username: 'CryptoWhale_2024', reputation: 7823, posts: 892, joined: '2020-11-22', status: 'ONLINE' },
  { username: 'OptionsFlow_Alert', reputation: 7456, posts: 1567, joined: '2022-01-08', status: 'ONLINE' },
  { username: 'MacroHedgeFund', reputation: 6789, posts: 743, joined: '2021-07-19', status: 'OFFLINE' },
  { username: 'TechInvestor88', reputation: 6234, posts: 654, joined: '2022-05-12', status: 'ONLINE' },
  { username: 'EnergyAnalyst_TX', reputation: 5892, posts: 521, joined: '2021-09-30', status: 'ONLINE' }
];

// Category colors for mapping
export const CATEGORY_COLORS = (colors: ForumColors) => [
  colors.BLUE, colors.PURPLE, colors.CYAN, colors.YELLOW,
  colors.GREEN, colors.ORANGE, colors.RED, colors.PURPLE
];

// Helper functions for priority and sentiment colors
export const getPriorityColor = (priority: string, colors: ForumColors): string => {
  switch (priority) {
    case 'HOT': return colors.RED;
    case 'TRENDING': return colors.ORANGE;
    case 'NORMAL': return colors.GRAY;
    default: return colors.GRAY;
  }
};

export const getSentimentColor = (sentiment: string, colors: ForumColors): string => {
  switch (sentiment) {
    case 'BULLISH': return colors.GREEN;
    case 'BEARISH': return colors.RED;
    case 'NEUTRAL': return colors.YELLOW;
    default: return colors.GRAY;
  }
};
