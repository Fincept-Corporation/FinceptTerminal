// Notes Tab Constants

import {
  FileText, TrendingUp, BookOpen, PieChart,
  DollarSign, Globe, Briefcase
} from 'lucide-react';
import type { Category } from './types';

// Fincept Professional Color Palette
export const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  BLUE: '#0088FF',
  PURPLE: '#9D4EDD',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A'
} as const;

export const CATEGORIES: Category[] = [
  { id: 'ALL', label: 'ALL NOTES', icon: FileText },
  { id: 'TRADE_IDEA', label: 'TRADE IDEAS', icon: TrendingUp },
  { id: 'RESEARCH', label: 'RESEARCH', icon: BookOpen },
  { id: 'MARKET_ANALYSIS', label: 'MARKET ANALYSIS', icon: PieChart },
  { id: 'EARNINGS', label: 'EARNINGS', icon: DollarSign },
  { id: 'ECONOMIC', label: 'ECONOMIC', icon: Globe },
  { id: 'PORTFOLIO', label: 'PORTFOLIO', icon: Briefcase },
  { id: 'GENERAL', label: 'GENERAL', icon: FileText }
];

export const PRIORITIES = ['HIGH', 'MEDIUM', 'LOW'] as const;
export const SENTIMENTS = ['BULLISH', 'BEARISH', 'NEUTRAL'] as const;
export const COLOR_CODES = ['#FF8800', '#00D66F', '#FF3B3B', '#00E5FF', '#FFD700', '#9D4EDD', '#0088FF'] as const;

// Default editor values
export const DEFAULT_EDITOR_STATE = {
  title: '',
  content: '',
  category: 'GENERAL',
  priority: 'MEDIUM',
  sentiment: 'NEUTRAL',
  tags: '',
  tickers: '',
  colorCode: '#FF8800',
  reminderDate: ''
};

// Scrollbar styles
export const SCROLLBAR_STYLES = `
  @import url('https://fonts.googleapis.com/css2?family=IBM+Plex+Mono:wght@400;500;600;700&display=swap');

  *::-webkit-scrollbar { width: 6px; height: 6px; }
  *::-webkit-scrollbar-track { background: ${FINCEPT.DARK_BG}; }
  *::-webkit-scrollbar-thumb { background: ${FINCEPT.BORDER}; border-radius: 3px; }
  *::-webkit-scrollbar-thumb:hover { background: ${FINCEPT.MUTED}; }

  .terminal-glow {
    text-shadow: 0 0 10px ${FINCEPT.ORANGE}40;
  }
`;

// Helper functions
export const getPriorityColor = (priority: string): string => {
  switch (priority) {
    case 'HIGH': return FINCEPT.RED;
    case 'MEDIUM': return FINCEPT.YELLOW;
    case 'LOW': return FINCEPT.GREEN;
    default: return FINCEPT.GRAY;
  }
};

export const getSentimentColor = (sentiment: string): string => {
  switch (sentiment) {
    case 'BULLISH': return FINCEPT.GREEN;
    case 'BEARISH': return FINCEPT.RED;
    case 'NEUTRAL': return FINCEPT.YELLOW;
    default: return FINCEPT.GRAY;
  }
};
