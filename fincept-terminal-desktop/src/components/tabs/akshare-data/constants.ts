/**
 * AkShare Data Tab — Constants
 *
 * Design tokens, style strings, and static data extracted from AkShareDataTab.tsx.
 */

import {
  Landmark, LineChart, TrendingUp, Globe, PieChart, Layers, BarChart3,
  Building2, Activity, DollarSign, Zap, Bitcoin, Newspaper, Building,
  Clock, FileText, Users, ArrowRightLeft, LayoutGrid, Percent, Flame,
} from 'lucide-react';

export const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  BLUE: '#0088FF',
  PURPLE: '#9D4EDD',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
} as const;

export const TERMINAL_STYLES = `
  .akshare-scrollbar::-webkit-scrollbar { width: 6px; height: 6px; }
  .akshare-scrollbar::-webkit-scrollbar-track { background: ${FINCEPT.DARK_BG}; }
  .akshare-scrollbar::-webkit-scrollbar-thumb { background: ${FINCEPT.BORDER}; border-radius: 3px; }
  .akshare-scrollbar::-webkit-scrollbar-thumb:hover { background: ${FINCEPT.MUTED}; }
  .akshare-hover-row:hover { background: ${FINCEPT.HOVER} !important; }
`;

export const API_TIMEOUT = 120_000; // Increased to 120s for slow endpoints

export const IconMap: Record<string, React.FC<{ size?: number; color?: string }>> = {
  Landmark, LineChart, TrendingUp, Globe, PieChart, Layers, BarChart3,
  Building2, Activity, DollarSign, Zap, Bitcoin, Newspaper, Building,
  Clock, FileText, Users, ArrowRightLeft, LayoutGrid, Percent, Flame,
};
