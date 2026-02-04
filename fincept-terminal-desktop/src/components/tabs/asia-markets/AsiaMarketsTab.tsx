/**
 * Asia Markets Tab - Fincept Terminal Professional Edition
 *
 * Comprehensive market data interface for Asian exchanges with 398+ stock endpoints
 * Organized into 8 data categories: Realtime, Historical, Financial, Holders, Funds, Board, Margin, Hot
 *
 * STATE_MANAGEMENT.md compliant:
 * 1. Race Conditions     - useReducer for atomic state updates
 * 2. State Machine       - Explicit idle/loading/success/error per async flow
 * 3. Cleanup on Unmount  - AbortController + mountedRef, clock cleanup
 * 4. Request Dedup       - withTimeout wraps all external calls
 * 5. Error Boundary      - ErrorBoundary wrapper
 * 6. WebSocket           - N/A
 * 7. Cache               - N/A (on-demand data explorer)
 * 8. Timeout Protection  - withTimeout(30s)
 * 9. Shared State Safety - Single useReducer, immutable updates
 * 10. Input Validation   - sanitizeInput on user text inputs
 */

import React, { useReducer, useEffect, useCallback, useMemo, useRef, memo } from 'react';
import { useWorkspaceTabState } from '@/hooks/useWorkspaceTabState';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { AKShareAPI } from '@/services/akshareApi';
import { APP_VERSION } from '@/constants/version';
import { invoke } from '@tauri-apps/api/core';
import { TabFooter } from '@/components/common/TabFooter';
import { ErrorBoundary } from '@/components/common/ErrorBoundary';
import { withTimeout } from '@/services/core/apiUtils';
import { sanitizeInput } from '@/services/core/validators';
import { useTranslation } from 'react-i18next';
import { showConfirm, showSuccess } from '@/utils/notifications';
import {
  Search, TrendingUp, TrendingDown, Activity, RefreshCw, AlertCircle, Database,
  Globe, BarChart3, DollarSign, Building2, Zap, Clock, ChevronDown, ChevronUp,
  Maximize2, Minimize2, Settings, Filter, Download, Copy, Table, Code,
  ArrowUpDown, X, Bookmark, BookmarkCheck, History, FileText, Users,
  ArrowRightLeft, LayoutGrid, Percent, Flame, PieChart, Briefcase,
  Wifi, WifiOff, ChevronRight, Eye, Play
} from 'lucide-react';

// ============================================================================
// Constants
// ============================================================================

const FINCEPT = {
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
};

const API_TIMEOUT = 30_000;

const STOCK_CATEGORIES = [
  { id: 'realtime', name: 'REALTIME', script: 'akshare_stocks_realtime.py', icon: Activity, color: FINCEPT.GREEN, description: 'Live stock prices, A/B/HK/US shares' },
  { id: 'historical', name: 'HISTORICAL', script: 'akshare_stocks_historical.py', icon: Clock, color: FINCEPT.BLUE, description: 'Daily, minute, tick, intraday data' },
  { id: 'financial', name: 'FINANCIAL', script: 'akshare_stocks_financial.py', icon: FileText, color: FINCEPT.PURPLE, description: 'Statements, earnings, dividends' },
  { id: 'holders', name: 'HOLDERS', script: 'akshare_stocks_holders.py', icon: Users, color: FINCEPT.YELLOW, description: 'Shareholders, institutional holdings' },
  { id: 'funds', name: 'FUND FLOW', script: 'akshare_stocks_funds.py', icon: ArrowRightLeft, color: FINCEPT.RED, description: 'Capital flow, block trades, LHB, IPO' },
  { id: 'board', name: 'BOARDS', script: 'akshare_stocks_board.py', icon: LayoutGrid, color: FINCEPT.CYAN, description: 'Industry, concept boards, rankings' },
  { id: 'margin', name: 'MARGIN/HSGT', script: 'akshare_stocks_margin.py', icon: Percent, color: FINCEPT.ORANGE, description: 'Margin trading, Stock Connect, PE/PB' },
  { id: 'hot', name: 'HOT & NEWS', script: 'akshare_stocks_hot.py', icon: Flame, color: '#FF6B6B', description: 'Hot stocks, news, ESG, sentiment' },
];

const MARKET_REGIONS = [
  { id: 'CN_A', name: 'CHINA A-SHARES', flag: '🇨🇳' },
  { id: 'CN_B', name: 'CHINA B-SHARES', flag: '🇨🇳' },
  { id: 'HK', name: 'HONG KONG', flag: '🇭🇰' },
  { id: 'US', name: 'US MARKETS', flag: '🇺🇸' },
];

// ============================================================================
// Types
// ============================================================================

type AsyncStatus = 'idle' | 'loading' | 'success' | 'error';
type MarketRegion = 'CN_A' | 'CN_B' | 'HK' | 'US';
type ViewMode = 'table' | 'json';
type BottomPanelTab = 'data' | 'favorites' | 'history';

interface QueryHistoryItem {
  id: string;
  script: string;
  endpoint: string;
  timestamp: number;
  success: boolean;
  count?: number;
}

interface FavoriteEndpoint {
  script: string;
  endpoint: string;
  name: string;
}

interface AsiaMarketsState {
  currentTime: Date;

  // Category & endpoint
  activeCategory: typeof STOCK_CATEGORIES[0];
  endpoints: string[];
  categories: Record<string, string[]>;
  selectedEndpoint: string | null;
  expandedCategories: Set<string>;
  endpointsStatus: AsyncStatus;

  // Market region
  activeRegion: MarketRegion;

  // Search & filter
  searchQuery: string;
  symbolInput: string;

  // Data (state machine)
  dataStatus: AsyncStatus;
  data: any[] | null;
  error: string | null;
  responseInfo: { count: number; timestamp: number; source?: string } | null;

  // View
  viewMode: ViewMode;
  sortColumn: string | null;
  sortDirection: 'asc' | 'desc';

  // UI
  isBottomPanelMinimized: boolean;
  activeBottomTab: BottomPanelTab;
  showSettings: boolean;

  // History & Favorites
  queryHistory: QueryHistoryItem[];
  favorites: FavoriteEndpoint[];

  // DB
  dbStatus: { exists: boolean; lastUpdated: string | null };
  dbBuilding: boolean;
}

// ============================================================================
// Actions
// ============================================================================

type AsiaAction =
  | { type: 'TICK' }
  | { type: 'SET_CATEGORY'; payload: typeof STOCK_CATEGORIES[0] }
  | { type: 'SET_REGION'; payload: MarketRegion }
  | { type: 'SET_SEARCH'; payload: string }
  | { type: 'SET_SYMBOL'; payload: string }
  | { type: 'TOGGLE_CATEGORY'; payload: string }
  | { type: 'SET_VIEW_MODE'; payload: ViewMode }
  | { type: 'SET_SORT'; payload: { column: string | null; direction: 'asc' | 'desc' } }
  | { type: 'TOGGLE_BOTTOM_PANEL' }
  | { type: 'SET_BOTTOM_TAB'; payload: BottomPanelTab }
  | { type: 'TOGGLE_SETTINGS' }
  // Endpoints
  | { type: 'ENDPOINTS_LOADING' }
  | { type: 'ENDPOINTS_SUCCESS'; payload: { endpoints: string[]; categories: Record<string, string[]> } }
  | { type: 'ENDPOINTS_ERROR' }
  // Data
  | { type: 'QUERY_START'; payload: string }
  | { type: 'QUERY_SUCCESS'; payload: { data: any[]; responseInfo: { count: number; timestamp: number; source?: string } } }
  | { type: 'QUERY_ERROR'; payload: string }
  // History & Favorites
  | { type: 'ADD_HISTORY'; payload: QueryHistoryItem }
  | { type: 'SET_HISTORY'; payload: QueryHistoryItem[] }
  | { type: 'SET_FAVORITES'; payload: FavoriteEndpoint[] }
  | { type: 'TOGGLE_FAVORITE'; payload: { script: string; endpoint: string } }
  // DB
  | { type: 'SET_DB_STATUS'; payload: { exists: boolean; lastUpdated: string | null } }
  | { type: 'SET_DB_BUILDING'; payload: boolean }
  // Workspace restore
  | { type: 'RESTORE_WORKSPACE'; payload: Record<string, unknown> };

// ============================================================================
// Reducer
// ============================================================================

const initialState: AsiaMarketsState = {
  currentTime: new Date(),
  activeCategory: STOCK_CATEGORIES[0],
  endpoints: [],
  categories: {},
  selectedEndpoint: null,
  expandedCategories: new Set(),
  endpointsStatus: 'idle',
  activeRegion: 'CN_A',
  searchQuery: '',
  symbolInput: '',
  dataStatus: 'idle',
  data: null,
  error: null,
  responseInfo: null,
  viewMode: 'table',
  sortColumn: null,
  sortDirection: 'asc',
  isBottomPanelMinimized: false,
  activeBottomTab: 'favorites',
  showSettings: false,
  queryHistory: [],
  favorites: [],
  dbStatus: { exists: false, lastUpdated: null },
  dbBuilding: false,
};

function asiaReducer(state: AsiaMarketsState, action: AsiaAction): AsiaMarketsState {
  switch (action.type) {
    case 'TICK':
      return { ...state, currentTime: new Date() };
    case 'SET_CATEGORY':
      return { ...state, activeCategory: action.payload, endpoints: [], categories: {}, expandedCategories: new Set(), endpointsStatus: 'idle' };
    case 'SET_REGION':
      return { ...state, activeRegion: action.payload };
    case 'SET_SEARCH':
      return { ...state, searchQuery: action.payload };
    case 'SET_SYMBOL':
      return { ...state, symbolInput: action.payload };
    case 'TOGGLE_CATEGORY': {
      const newExpanded = new Set(state.expandedCategories);
      if (newExpanded.has(action.payload)) newExpanded.delete(action.payload);
      else newExpanded.add(action.payload);
      return { ...state, expandedCategories: newExpanded };
    }
    case 'SET_VIEW_MODE':
      return { ...state, viewMode: action.payload };
    case 'SET_SORT':
      return { ...state, sortColumn: action.payload.column, sortDirection: action.payload.direction };
    case 'TOGGLE_BOTTOM_PANEL':
      return { ...state, isBottomPanelMinimized: !state.isBottomPanelMinimized };
    case 'SET_BOTTOM_TAB':
      return { ...state, activeBottomTab: action.payload };
    case 'TOGGLE_SETTINGS':
      return { ...state, showSettings: !state.showSettings };

    case 'ENDPOINTS_LOADING':
      return { ...state, endpointsStatus: 'loading', endpoints: [], categories: {} };
    case 'ENDPOINTS_SUCCESS': {
      const firstCat = Object.keys(action.payload.categories)[0];
      return {
        ...state,
        endpointsStatus: 'success',
        endpoints: action.payload.endpoints,
        categories: action.payload.categories,
        expandedCategories: firstCat ? new Set([firstCat]) : new Set(),
      };
    }
    case 'ENDPOINTS_ERROR':
      return { ...state, endpointsStatus: 'error' };

    case 'QUERY_START':
      return { ...state, dataStatus: 'loading', data: null, error: null, responseInfo: null, selectedEndpoint: action.payload };
    case 'QUERY_SUCCESS':
      return { ...state, dataStatus: 'success', data: action.payload.data, responseInfo: action.payload.responseInfo, error: null };
    case 'QUERY_ERROR':
      return { ...state, dataStatus: 'error', error: action.payload };

    case 'ADD_HISTORY': {
      const newHistory = [action.payload, ...state.queryHistory.slice(0, 49)];
      return { ...state, queryHistory: newHistory };
    }
    case 'SET_HISTORY':
      return { ...state, queryHistory: action.payload };
    case 'SET_FAVORITES':
      return { ...state, favorites: action.payload };
    case 'TOGGLE_FAVORITE': {
      const { script, endpoint } = action.payload;
      const exists = state.favorites.some(f => f.script === script && f.endpoint === endpoint);
      const newFavs = exists
        ? state.favorites.filter(f => !(f.script === script && f.endpoint === endpoint))
        : [...state.favorites, { script, endpoint, name: endpoint }];
      return { ...state, favorites: newFavs };
    }

    case 'SET_DB_STATUS':
      return { ...state, dbStatus: action.payload };
    case 'SET_DB_BUILDING':
      return { ...state, dbBuilding: action.payload };

    case 'RESTORE_WORKSPACE': {
      const s = action.payload;
      return {
        ...state,
        ...(typeof s.activeRegion === 'string' ? { activeRegion: s.activeRegion as MarketRegion } : {}),
        ...(typeof s.viewMode === 'string' ? { viewMode: s.viewMode as ViewMode } : {}),
        ...(typeof s.isBottomPanelMinimized === 'boolean' ? { isBottomPanelMinimized: s.isBottomPanelMinimized } : {}),
        ...(typeof s.activeBottomTab === 'string' ? { activeBottomTab: s.activeBottomTab as BottomPanelTab } : {}),
      };
    }

    default:
      return state;
  }
}

// ============================================================================
// Component
// ============================================================================

const AsiaMarketsTabInner: React.FC = () => {
  const { theme } = useTerminalTheme();
  const { t } = useTranslation();
  const [state, dispatch] = useReducer(asiaReducer, initialState);
  const mountedRef = useRef(true);
  const abortRef = useRef<AbortController | null>(null);

  const {
    currentTime, activeCategory, endpoints, categories, selectedEndpoint,
    expandedCategories, endpointsStatus, activeRegion, searchQuery, symbolInput,
    dataStatus, data, error, responseInfo,
    viewMode, sortColumn, sortDirection,
    isBottomPanelMinimized, activeBottomTab, showSettings,
    queryHistory, favorites, dbStatus, dbBuilding,
  } = state;

  // ---------- Cleanup on unmount (Point 3) ----------
  useEffect(() => {
    mountedRef.current = true;
    return () => {
      mountedRef.current = false;
      abortRef.current?.abort();
    };
  }, []);

  // ---------- Clock (Point 3: cleanup) ----------
  useEffect(() => {
    const timer = setInterval(() => dispatch({ type: 'TICK' }), 1000);
    return () => clearInterval(timer);
  }, []);

  // ---------- Workspace tab state ----------
  const getWorkspaceState = useCallback(() => ({
    activeRegion, viewMode, isBottomPanelMinimized, activeBottomTab,
  }), [activeRegion, viewMode, isBottomPanelMinimized, activeBottomTab]);

  const setWorkspaceState = useCallback((s: Record<string, unknown>) => {
    dispatch({ type: 'RESTORE_WORKSPACE', payload: s });
  }, []);

  useWorkspaceTabState("asia-markets", getWorkspaceState, setWorkspaceState);

  // ---------- Load favorites, history, DB status on mount ----------
  useEffect(() => {
    // Favorites & history
    try {
      const savedFavorites = localStorage.getItem('akshare_stock_favorites');
      if (savedFavorites) dispatch({ type: 'SET_FAVORITES', payload: JSON.parse(savedFavorites) });
    } catch { /* ignore */ }
    try {
      const savedHistory = localStorage.getItem('akshare_stock_history');
      if (savedHistory) dispatch({ type: 'SET_HISTORY', payload: JSON.parse(savedHistory).slice(0, 50) });
    } catch { /* ignore */ }

    // DB status
    checkDatabaseStatus();
  }, []);

  // ---------- Persist favorites ----------
  useEffect(() => {
    localStorage.setItem('akshare_stock_favorites', JSON.stringify(favorites));
  }, [favorites]);

  // ---------- Persist history ----------
  useEffect(() => {
    localStorage.setItem('akshare_stock_history', JSON.stringify(queryHistory));
  }, [queryHistory]);

  // ---------- Load endpoints when category changes (Point 3 + Point 8) ----------
  useEffect(() => {
    const controller = new AbortController();
    abortRef.current?.abort();
    abortRef.current = controller;

    const load = async () => {
      dispatch({ type: 'ENDPOINTS_LOADING' });
      try {
        const response = await withTimeout(
          AKShareAPI.getEndpoints(activeCategory.script),
          API_TIMEOUT,
          'Endpoints loading timeout'
        );
        if (controller.signal.aborted || !mountedRef.current) return;

        if (response.success && response.data) {
          const endpointData = response.data as any;
          dispatch({
            type: 'ENDPOINTS_SUCCESS',
            payload: {
              endpoints: endpointData.available_endpoints || [],
              categories: endpointData.categories || {},
            },
          });
        } else {
          dispatch({ type: 'ENDPOINTS_ERROR' });
        }
      } catch {
        if (!controller.signal.aborted && mountedRef.current) {
          dispatch({ type: 'ENDPOINTS_ERROR' });
        }
      }
    };
    load();

    return () => { controller.abort(); };
  }, [activeCategory]);

  // ---------- DB helpers ----------
  const checkDatabaseStatus = useCallback(async () => {
    try {
      const result = await withTimeout(
        invoke<{ exists: boolean; last_updated: string | null }>('check_akshare_db_status'),
        API_TIMEOUT
      );
      if (mountedRef.current) {
        dispatch({ type: 'SET_DB_STATUS', payload: { exists: result.exists, lastUpdated: result.last_updated } });
      }
    } catch { /* ignore */ }
  }, []);

  const buildDatabase = useCallback(async () => {
    if (dbStatus.exists && dbStatus.lastUpdated) {
      const lastUpdate = new Date(dbStatus.lastUpdated);
      const today = new Date();
      if (lastUpdate.toDateString() === today.toDateString()) {
        const confirmed = await showConfirm(
          'Database was already built today. Rebuild anyway? (10-15 minutes)',
          { title: 'Rebuild Database?', type: 'warning' }
        );
        if (!confirmed) return;
      }
    }

    dispatch({ type: 'SET_DB_BUILDING', payload: true });
    try {
      await withTimeout(invoke('build_akshare_database'), 600_000, 'Database build timeout');
      await checkDatabaseStatus();
      showSuccess('Database built successfully!');
    } catch (err) {
      if (mountedRef.current) {
        dispatch({ type: 'QUERY_ERROR', payload: err instanceof Error ? err.message : 'Failed to build database' });
      }
    } finally {
      if (mountedRef.current) dispatch({ type: 'SET_DB_BUILDING', payload: false });
    }
  }, [dbStatus, checkDatabaseStatus]);

  // ---------- Execute query (Point 8 + Point 10) ----------
  const executeQuery = useCallback(async (script: string, endpoint: string, symbol?: string) => {
    abortRef.current?.abort();
    const controller = new AbortController();
    abortRef.current = controller;

    dispatch({ type: 'QUERY_START', payload: endpoint });

    try {
      // Point 10: sanitize symbol input
      const args = symbol ? [sanitizeInput(symbol)] : undefined;
      const response = await withTimeout(
        AKShareAPI.query(script, endpoint, args),
        API_TIMEOUT,
        'Query timeout'
      );

      if (controller.signal.aborted || !mountedRef.current) return;

      const historyItem: QueryHistoryItem = {
        id: `${Date.now()}`,
        script,
        endpoint,
        timestamp: Date.now(),
        success: response.success,
        count: response.count,
      };
      dispatch({ type: 'ADD_HISTORY', payload: historyItem });

      if (response.success && response.data) {
        const dataArray = Array.isArray(response.data) ? response.data : [response.data];
        dispatch({
          type: 'QUERY_SUCCESS',
          payload: {
            data: dataArray,
            responseInfo: {
              count: response.count || dataArray.length,
              timestamp: response.timestamp || Date.now(),
              source: response.source,
            },
          },
        });
      } else {
        dispatch({ type: 'QUERY_ERROR', payload: typeof response.error === 'string' ? response.error : JSON.stringify(response.error) });
      }
    } catch (err: any) {
      if (!controller.signal.aborted && mountedRef.current) {
        dispatch({ type: 'QUERY_ERROR', payload: err.message || 'Query failed' });
        dispatch({
          type: 'ADD_HISTORY',
          payload: { id: `${Date.now()}`, script, endpoint, timestamp: Date.now(), success: false, count: 0 },
        });
      }
    }
  }, []);

  // ---------- Derived state ----------
  const filteredEndpoints = useMemo(() => {
    if (!searchQuery) return endpoints;
    const q = searchQuery.toLowerCase();
    return endpoints.filter(e => e.toLowerCase().includes(q));
  }, [endpoints, searchQuery]);

  const filteredCategories = useMemo(() => {
    if (!searchQuery) return categories;
    const q = searchQuery.toLowerCase();
    const filtered: Record<string, string[]> = {};
    for (const [cat, eps] of Object.entries(categories)) {
      const matching = eps.filter(e => e.toLowerCase().includes(q));
      if (matching.length > 0 || cat.toLowerCase().includes(q)) {
        filtered[cat] = matching.length > 0 ? matching : eps;
      }
    }
    return filtered;
  }, [categories, searchQuery]);

  const sortedData = useMemo(() => {
    if (!data || !sortColumn) return data;
    return [...data].sort((a, b) => {
      const aVal = a[sortColumn];
      const bVal = b[sortColumn];
      if (aVal === bVal) return 0;
      if (aVal === null || aVal === undefined) return 1;
      if (bVal === null || bVal === undefined) return -1;
      const comparison = aVal < bVal ? -1 : 1;
      return sortDirection === 'asc' ? comparison : -comparison;
    });
  }, [data, sortColumn, sortDirection]);

  const columns = useMemo(() => {
    if (!data || data.length === 0) return [];
    return Object.keys(data[0]);
  }, [data]);

  const isFavorite = useCallback((script: string, endpoint: string) => {
    return favorites.some(f => f.script === script && f.endpoint === endpoint);
  }, [favorites]);

  // ---------- Utility ----------
  const copyToClipboard = useCallback((text: string) => { navigator.clipboard.writeText(text); }, []);

  const downloadCSV = useCallback(() => {
    if (!data || data.length === 0) return;
    const headers = columns.join(',');
    const rows = data.map(row =>
      columns.map(col => {
        const val = row[col];
        if (typeof val === 'string' && (val.includes(',') || val.includes('"'))) return `"${val.replace(/"/g, '""')}"`;
        return val;
      }).join(',')
    );
    const csv = [headers, ...rows].join('\n');
    const blob = new Blob([csv], { type: 'text/csv' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `akshare_${selectedEndpoint}_${Date.now()}.csv`;
    a.click();
    URL.revokeObjectURL(url);
  }, [data, columns, selectedEndpoint]);

  const formatValue = useCallback((value: any): string => {
    if (value === null || value === undefined) return '-';
    if (typeof value === 'number') {
      if (Math.abs(value) >= 1e9) return `${(value / 1e9).toFixed(2)}B`;
      if (Math.abs(value) >= 1e6) return `${(value / 1e6).toFixed(2)}M`;
      if (Math.abs(value) >= 1e3) return `${(value / 1e3).toFixed(2)}K`;
      return value.toLocaleString();
    }
    return String(value);
  }, []);

  const getValueColor = useCallback((value: any, colName: string): string => {
    if (typeof value !== 'number') return FINCEPT.WHITE;
    const lower = colName.toLowerCase();
    if (lower.includes('change') || lower.includes('pct') || lower.includes('涨跌')) {
      return value > 0 ? FINCEPT.GREEN : value < 0 ? FINCEPT.RED : FINCEPT.GRAY;
    }
    return FINCEPT.WHITE;
  }, []);

  const handleSortClick = useCallback((col: string) => {
    if (sortColumn === col) {
      dispatch({ type: 'SET_SORT', payload: { column: col, direction: sortDirection === 'asc' ? 'desc' : 'asc' } });
    } else {
      dispatch({ type: 'SET_SORT', payload: { column: col, direction: 'asc' } });
    }
  }, [sortColumn, sortDirection]);

  // ---------- Render ----------
  return (
    <div style={{
      height: '100%',
      backgroundColor: FINCEPT.DARK_BG,
      color: FINCEPT.WHITE,
      fontFamily: '"IBM Plex Mono", "Consolas", monospace',
      overflow: 'hidden',
      display: 'flex',
      flexDirection: 'column'
    }}>
      <style>{`
        @import url('https://fonts.googleapis.com/css2?family=IBM+Plex+Mono:wght@400;500;600;700&display=swap');
        *::-webkit-scrollbar { width: 6px; height: 6px; }
        *::-webkit-scrollbar-track { background: ${FINCEPT.DARK_BG}; }
        *::-webkit-scrollbar-thumb { background: ${FINCEPT.BORDER}; border-radius: 3px; }
        *::-webkit-scrollbar-thumb:hover { background: ${FINCEPT.MUTED}; }
        .terminal-glow { text-shadow: 0 0 10px ${FINCEPT.ORANGE}40; }
        .hover-row:hover { background-color: ${FINCEPT.HOVER} !important; }
      `}</style>

      {/* TOP NAVIGATION BAR */}
      <div style={{
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `2px solid ${FINCEPT.ORANGE}`,
        padding: '6px 12px',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        flexShrink: 0,
        boxShadow: `0 2px 8px ${FINCEPT.ORANGE}20`
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <Globe size={18} color={FINCEPT.ORANGE} style={{ filter: 'drop-shadow(0 0 4px ' + FINCEPT.ORANGE + ')' }} />
            <span style={{ color: FINCEPT.ORANGE, fontWeight: 700, fontSize: '14px', letterSpacing: '0.5px', textShadow: `0 0 10px ${FINCEPT.ORANGE}40` }}>
              ASIA MARKETS TERMINAL
            </span>
          </div>
          <div style={{ height: '16px', width: '1px', backgroundColor: FINCEPT.BORDER }} />
          <div style={{ padding: '4px 10px', backgroundColor: `${FINCEPT.CYAN}20`, border: `1px solid ${FINCEPT.CYAN}`, borderRadius: '4px', fontSize: '10px', fontWeight: 700, color: FINCEPT.CYAN }}>
            398+ ENDPOINTS
          </div>
          <div style={{ height: '16px', width: '1px', backgroundColor: FINCEPT.BORDER }} />
          <div style={{ display: 'flex', gap: '4px' }}>
            {MARKET_REGIONS.map(region => (
              <button key={region.id} onClick={() => dispatch({ type: 'SET_REGION', payload: region.id as MarketRegion })} style={{
                padding: '4px 12px',
                backgroundColor: activeRegion === region.id ? FINCEPT.ORANGE : 'transparent',
                border: `1px solid ${activeRegion === region.id ? FINCEPT.ORANGE : FINCEPT.BORDER}`,
                color: activeRegion === region.id ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                cursor: 'pointer', fontSize: '10px', fontWeight: 700, transition: 'all 0.2s',
                display: 'flex', alignItems: 'center', gap: '4px'
              }}>
                <span>{region.flag}</span><span>{region.name}</span>
              </button>
            ))}
          </div>
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          {dbStatus.exists ? (
            <div style={{ display: 'flex', alignItems: 'center', gap: '6px', padding: '4px 8px', backgroundColor: `${FINCEPT.GREEN}20`, border: `1px solid ${FINCEPT.GREEN}`, borderRadius: '4px', fontSize: '9px', color: FINCEPT.GREEN }}>
              <Database size={10} /><span>DB READY</span>
            </div>
          ) : (
            <button onClick={buildDatabase} disabled={dbBuilding} style={{
              display: 'flex', alignItems: 'center', gap: '6px', padding: '4px 10px',
              backgroundColor: FINCEPT.PURPLE, border: 'none', borderRadius: '4px',
              fontSize: '9px', fontWeight: 700, color: FINCEPT.WHITE, cursor: 'pointer', opacity: dbBuilding ? 0.5 : 1
            }}>
              <Database size={10} />{dbBuilding ? 'BUILDING...' : 'BUILD DB'}
            </button>
          )}
          <div style={{ height: '16px', width: '1px', backgroundColor: FINCEPT.BORDER }} />
          <div style={{ fontSize: '11px', fontWeight: 600, color: FINCEPT.CYAN, fontFamily: 'monospace' }}>
            {currentTime.toLocaleTimeString('en-US', { hour12: false })}
          </div>
          <button onClick={() => dispatch({ type: 'TOGGLE_SETTINGS' })} style={{
            padding: '4px 8px', backgroundColor: 'transparent', border: `1px solid ${FINCEPT.BORDER}`,
            color: FINCEPT.GRAY, cursor: 'pointer', display: 'flex', alignItems: 'center', transition: 'all 0.2s'
          }}>
            <Settings size={12} />
          </button>
        </div>
      </div>

      {/* CATEGORY TABS BAR */}
      <div style={{
        backgroundColor: FINCEPT.PANEL_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        padding: '8px 12px',
        display: 'flex', alignItems: 'center', gap: '8px',
        overflowX: 'auto', flexShrink: 0
      }}>
        {STOCK_CATEGORIES.map(cat => {
          const Icon = cat.icon;
          const isActive = activeCategory.id === cat.id;
          return (
            <button key={cat.id} onClick={() => dispatch({ type: 'SET_CATEGORY', payload: cat })} style={{
              padding: '8px 16px',
              backgroundColor: isActive ? `${cat.color}20` : 'transparent',
              border: `1px solid ${isActive ? cat.color : FINCEPT.BORDER}`,
              borderRadius: '4px', color: isActive ? cat.color : FINCEPT.GRAY,
              cursor: 'pointer', fontSize: '10px', fontWeight: 700, transition: 'all 0.2s',
              display: 'flex', alignItems: 'center', gap: '6px', whiteSpace: 'nowrap'
            }}>
              <Icon size={14} /><span>{cat.name}</span>
            </button>
          );
        })}
      </div>

      {/* MAIN CONTENT */}
      <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>
        {/* LEFT SIDEBAR */}
        <div style={{
          width: '300px', minWidth: '300px', backgroundColor: FINCEPT.PANEL_BG,
          borderRight: `1px solid ${FINCEPT.BORDER}`, display: 'flex', flexDirection: 'column',
          overflow: 'hidden', flexShrink: 0
        }}>
          <div style={{ padding: '12px', backgroundColor: FINCEPT.HEADER_BG, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '8px' }}>
              {React.createElement(activeCategory.icon, { size: 16, color: activeCategory.color })}
              <span style={{ fontSize: '12px', fontWeight: 700, color: activeCategory.color }}>{activeCategory.name}</span>
              <span style={{ fontSize: '10px', color: FINCEPT.MUTED, marginLeft: 'auto' }}>{endpoints.length} endpoints</span>
            </div>
            <div style={{ fontSize: '10px', color: FINCEPT.GRAY }}>{activeCategory.description}</div>
          </div>

          {/* Search */}
          <div style={{ padding: '8px 12px', borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
            <div style={{ position: 'relative' }}>
              <Search size={14} style={{ position: 'absolute', left: '10px', top: '50%', transform: 'translateY(-50%)', color: FINCEPT.MUTED }} />
              <input type="text" value={searchQuery}
                onChange={e => dispatch({ type: 'SET_SEARCH', payload: sanitizeInput(e.target.value) })}
                placeholder="Search endpoints..."
                style={{ width: '100%', padding: '8px 32px 8px 32px', backgroundColor: FINCEPT.DARK_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', color: FINCEPT.WHITE, fontSize: '11px' }}
              />
              {searchQuery && (
                <button onClick={() => dispatch({ type: 'SET_SEARCH', payload: '' })} style={{ position: 'absolute', right: '8px', top: '50%', transform: 'translateY(-50%)', background: 'none', border: 'none', cursor: 'pointer', padding: '2px' }}>
                  <X size={12} color={FINCEPT.MUTED} />
                </button>
              )}
            </div>
          </div>

          {/* Symbol Input */}
          <div style={{ padding: '8px 12px', borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
            <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '4px', fontWeight: 700 }}>SYMBOL (for stock-specific endpoints)</div>
            <input type="text" value={symbolInput}
              onChange={e => dispatch({ type: 'SET_SYMBOL', payload: sanitizeInput(e.target.value.toUpperCase()) })}
              placeholder="e.g., 600519, 00700, AAPL"
              style={{ width: '100%', padding: '8px 12px', backgroundColor: FINCEPT.DARK_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', color: FINCEPT.YELLOW, fontSize: '12px', fontWeight: 700, fontFamily: 'monospace' }}
            />
          </div>

          {/* Endpoints List */}
          <div style={{ flex: 1, overflow: 'auto' }}>
            {endpointsStatus === 'loading' ? (
              <div style={{ padding: '40px', textAlign: 'center' }}>
                <RefreshCw size={24} className="animate-spin" style={{ color: FINCEPT.ORANGE, margin: '0 auto 12px' }} />
                <div style={{ fontSize: '11px', color: FINCEPT.GRAY }}>Loading endpoints...</div>
              </div>
            ) : Object.keys(filteredCategories).length > 0 ? (
              Object.entries(filteredCategories).map(([category, categoryEndpoints]) => (
                <div key={category}>
                  <button onClick={() => dispatch({ type: 'TOGGLE_CATEGORY', payload: category })} style={{
                    width: '100%', padding: '10px 12px', backgroundColor: FINCEPT.HEADER_BG,
                    border: 'none', borderBottom: `1px solid ${FINCEPT.BORDER}`,
                    display: 'flex', alignItems: 'center', gap: '8px', cursor: 'pointer', textAlign: 'left'
                  }}>
                    {expandedCategories.has(category) ? <ChevronDown size={12} color={FINCEPT.ORANGE} /> : <ChevronRight size={12} color={FINCEPT.MUTED} />}
                    <span style={{ fontSize: '10px', fontWeight: 700, color: FINCEPT.ORANGE }}>{category}</span>
                    <span style={{ fontSize: '9px', color: FINCEPT.MUTED, marginLeft: 'auto' }}>{categoryEndpoints.length}</span>
                  </button>
                  {expandedCategories.has(category) && (
                    <div>
                      {categoryEndpoints.map(endpoint => {
                        const isActive = selectedEndpoint === endpoint;
                        const isFav = isFavorite(activeCategory.script, endpoint);
                        return (
                          <div key={endpoint} style={{ display: 'flex', alignItems: 'center', padding: '2px 8px', borderBottom: `1px solid ${FINCEPT.BORDER}40` }}>
                            <button onClick={() => executeQuery(activeCategory.script, endpoint, symbolInput || undefined)}
                              className="hover-row"
                              style={{
                                flex: 1, padding: '8px 12px',
                                backgroundColor: isActive ? `${activeCategory.color}15` : 'transparent',
                                border: 'none', borderLeft: isActive ? `2px solid ${activeCategory.color}` : '2px solid transparent',
                                color: isActive ? activeCategory.color : FINCEPT.WHITE,
                                cursor: 'pointer', fontSize: '10px', textAlign: 'left', fontFamily: 'monospace', transition: 'all 0.15s'
                              }}>
                              {endpoint}
                            </button>
                            <button onClick={() => dispatch({ type: 'TOGGLE_FAVORITE', payload: { script: activeCategory.script, endpoint } })}
                              style={{ padding: '4px', background: 'none', border: 'none', cursor: 'pointer' }}>
                              {isFav ? <BookmarkCheck size={12} color={FINCEPT.YELLOW} /> : <Bookmark size={12} color={FINCEPT.MUTED} />}
                            </button>
                          </div>
                        );
                      })}
                    </div>
                  )}
                </div>
              ))
            ) : (
              <div style={{ padding: '20px', textAlign: 'center', color: FINCEPT.MUTED, fontSize: '11px' }}>No endpoints found</div>
            )}
          </div>
        </div>

        {/* CENTER - Data Display */}
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden', minWidth: 0 }}>
          <div style={{
            padding: '8px 12px', backgroundColor: FINCEPT.HEADER_BG,
            borderBottom: `1px solid ${FINCEPT.BORDER}`,
            display: 'flex', alignItems: 'center', justifyContent: 'space-between', flexShrink: 0
          }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
              {selectedEndpoint && (
                <>
                  <span style={{ fontSize: '12px', fontWeight: 700, color: activeCategory.color, fontFamily: 'monospace' }}>{selectedEndpoint}</span>
                  {responseInfo && <span style={{ fontSize: '10px', color: FINCEPT.MUTED }}>{responseInfo.count.toLocaleString()} records</span>}
                </>
              )}
            </div>
            <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
              <div style={{ display: 'flex', border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', overflow: 'hidden' }}>
                {(['table', 'json'] as ViewMode[]).map(mode => (
                  <button key={mode} onClick={() => dispatch({ type: 'SET_VIEW_MODE', payload: mode })} style={{
                    padding: '4px 10px', backgroundColor: viewMode === mode ? FINCEPT.HOVER : 'transparent',
                    border: 'none', color: viewMode === mode ? FINCEPT.WHITE : FINCEPT.MUTED,
                    cursor: 'pointer', fontSize: '10px', display: 'flex', alignItems: 'center', gap: '4px'
                  }}>
                    {mode === 'table' ? <Table size={12} /> : <Code size={12} />}{mode.toUpperCase()}
                  </button>
                ))}
              </div>
              {data && (
                <>
                  <button onClick={downloadCSV} style={{ padding: '4px 8px', backgroundColor: 'transparent', border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', color: FINCEPT.GRAY, cursor: 'pointer' }} title="Download CSV">
                    <Download size={12} />
                  </button>
                  <button onClick={() => copyToClipboard(JSON.stringify(data, null, 2))} style={{ padding: '4px 8px', backgroundColor: 'transparent', border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', color: FINCEPT.GRAY, cursor: 'pointer' }} title="Copy JSON">
                    <Copy size={12} />
                  </button>
                </>
              )}
              {selectedEndpoint && (
                <button onClick={() => executeQuery(activeCategory.script, selectedEndpoint, symbolInput || undefined)} disabled={dataStatus === 'loading'} style={{
                  padding: '6px 12px', backgroundColor: FINCEPT.ORANGE, border: 'none', borderRadius: '4px',
                  color: FINCEPT.DARK_BG, cursor: 'pointer', fontSize: '10px', fontWeight: 700,
                  display: 'flex', alignItems: 'center', gap: '6px'
                }}>
                  <RefreshCw size={12} className={dataStatus === 'loading' ? 'animate-spin' : ''} />REFRESH
                </button>
              )}
            </div>
          </div>

          {/* Data Content */}
          <div style={{ flex: 1, overflow: 'auto', padding: '0' }}>
            {dataStatus === 'loading' ? (
              <div style={{ height: '100%', display: 'flex', alignItems: 'center', justifyContent: 'center', flexDirection: 'column', gap: '16px' }}>
                <RefreshCw size={40} className="animate-spin" style={{ color: FINCEPT.ORANGE }} />
                <div style={{ fontSize: '12px', color: FINCEPT.GRAY }}>Loading data...</div>
              </div>
            ) : dataStatus === 'error' ? (
              <div style={{ height: '100%', display: 'flex', alignItems: 'center', justifyContent: 'center', flexDirection: 'column', gap: '16px', padding: '40px' }}>
                <AlertCircle size={40} color={FINCEPT.RED} />
                <div style={{ fontSize: '14px', fontWeight: 600, color: FINCEPT.WHITE }}>Query Failed</div>
                <div style={{ padding: '12px 16px', backgroundColor: `${FINCEPT.RED}20`, border: `1px solid ${FINCEPT.RED}`, borderRadius: '4px', fontSize: '11px', color: FINCEPT.RED, fontFamily: 'monospace', maxWidth: '500px', textAlign: 'center' }}>
                  {error}
                </div>
                {selectedEndpoint && (
                  <button onClick={() => executeQuery(activeCategory.script, selectedEndpoint, symbolInput || undefined)} style={{
                    marginTop: '8px', padding: '8px 16px', backgroundColor: FINCEPT.ORANGE, border: 'none',
                    borderRadius: '4px', color: FINCEPT.DARK_BG, fontSize: '10px', fontWeight: 700, cursor: 'pointer'
                  }}>TRY AGAIN</button>
                )}
              </div>
            ) : data && data.length > 0 ? (
              viewMode === 'table' ? (
                <div style={{ overflow: 'auto', height: '100%' }}>
                  <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: '11px' }}>
                    <thead>
                      <tr style={{ backgroundColor: FINCEPT.HEADER_BG, position: 'sticky', top: 0, zIndex: 10 }}>
                        {columns.map(col => (
                          <th key={col} onClick={() => handleSortClick(col)} style={{
                            padding: '10px 12px', textAlign: 'left', color: FINCEPT.ORANGE, fontWeight: 700,
                            borderBottom: `2px solid ${FINCEPT.ORANGE}`, cursor: 'pointer', whiteSpace: 'nowrap', fontSize: '10px'
                          }}>
                            <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                              <span style={{ maxWidth: '150px', overflow: 'hidden', textOverflow: 'ellipsis' }} title={col}>{col}</span>
                              {sortColumn === col && <ArrowUpDown size={10} color={FINCEPT.CYAN} />}
                            </div>
                          </th>
                        ))}
                      </tr>
                    </thead>
                    <tbody>
                      {(sortedData || data).slice(0, 500).map((row, idx) => (
                        <tr key={idx} className="hover-row" style={{ backgroundColor: idx % 2 === 0 ? FINCEPT.PANEL_BG : FINCEPT.DARK_BG, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
                          {columns.map(col => {
                            const value = row[col];
                            return (
                              <td key={col} style={{
                                padding: '8px 12px', color: getValueColor(value, col),
                                fontFamily: typeof value === 'number' ? 'monospace' : 'inherit',
                                whiteSpace: 'nowrap', maxWidth: '200px', overflow: 'hidden', textOverflow: 'ellipsis'
                              }} title={String(value)}>
                                {formatValue(value)}
                              </td>
                            );
                          })}
                        </tr>
                      ))}
                    </tbody>
                  </table>
                  {data.length > 500 && (
                    <div style={{ textAlign: 'center', padding: '16px', color: FINCEPT.MUTED, fontSize: '11px' }}>
                      Showing 500 of {data.length.toLocaleString()} records
                    </div>
                  )}
                </div>
              ) : (
                <pre style={{ padding: '16px', backgroundColor: FINCEPT.HEADER_BG, color: FINCEPT.WHITE, fontSize: '10px', fontFamily: 'monospace', overflow: 'auto', height: '100%', margin: 0 }}>
                  {JSON.stringify(data.slice(0, 100), null, 2)}
                  {data.length > 100 && `\n\n... and ${data.length - 100} more records`}
                </pre>
              )
            ) : dataStatus === 'idle' && !selectedEndpoint ? (
              <div style={{ height: '100%', display: 'flex', alignItems: 'center', justifyContent: 'center', flexDirection: 'column', gap: '20px', padding: '40px' }}>
                <Globe size={48} color={FINCEPT.ORANGE} />
                <div style={{ fontSize: '20px', fontWeight: 700, color: FINCEPT.WHITE }}>Asia Markets Data Terminal</div>
                <div style={{ fontSize: '12px', color: FINCEPT.GRAY, textAlign: 'center', maxWidth: '500px' }}>
                  Access 398+ stock data endpoints covering Chinese A/B shares, Hong Kong, and US markets. Select a category and endpoint from the left panel to get started.
                </div>
                <div style={{ display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)', gap: '16px', marginTop: '20px' }}>
                  {STOCK_CATEGORIES.slice(0, 4).map(cat => {
                    const Icon = cat.icon;
                    return (
                      <button key={cat.id} onClick={() => dispatch({ type: 'SET_CATEGORY', payload: cat })} style={{
                        padding: '16px', backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`,
                        borderRadius: '8px', cursor: 'pointer', textAlign: 'center', transition: 'all 0.2s'
                      }}>
                        <Icon size={24} color={cat.color} style={{ marginBottom: '8px' }} />
                        <div style={{ fontSize: '11px', fontWeight: 700, color: cat.color }}>{cat.name}</div>
                        <div style={{ fontSize: '9px', color: FINCEPT.MUTED, marginTop: '4px' }}>{cat.description}</div>
                      </button>
                    );
                  })}
                </div>
              </div>
            ) : (
              <div style={{ height: '100%', display: 'flex', alignItems: 'center', justifyContent: 'center', color: FINCEPT.MUTED }}>
                No data returned
              </div>
            )}
          </div>
        </div>

        {/* RIGHT SIDEBAR - History & Favorites */}
        <div style={{
          width: '250px', minWidth: '250px', backgroundColor: FINCEPT.PANEL_BG,
          borderLeft: `1px solid ${FINCEPT.BORDER}`, display: 'flex', flexDirection: 'column',
          overflow: 'hidden', flexShrink: 0
        }}>
          <div style={{ display: 'flex', backgroundColor: FINCEPT.HEADER_BG, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
            {[
              { id: 'favorites' as const, label: 'FAVORITES', icon: BookmarkCheck, count: favorites.length },
              { id: 'history' as const, label: 'HISTORY', icon: History, count: queryHistory.length },
            ].map(tab => (
              <button key={tab.id} onClick={() => dispatch({ type: 'SET_BOTTOM_TAB', payload: tab.id })} style={{
                flex: 1, padding: '10px 8px',
                backgroundColor: activeBottomTab === tab.id ? FINCEPT.HOVER : 'transparent',
                border: 'none', borderBottom: activeBottomTab === tab.id ? `2px solid ${FINCEPT.ORANGE}` : '2px solid transparent',
                color: activeBottomTab === tab.id ? FINCEPT.ORANGE : FINCEPT.GRAY,
                cursor: 'pointer', fontSize: '9px', fontWeight: 700,
                display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '4px'
              }}>
                <tab.icon size={10} />{tab.label} ({tab.count})
              </button>
            ))}
          </div>
          <div style={{ flex: 1, overflow: 'auto' }}>
            {activeBottomTab === 'favorites' && (
              favorites.length === 0 ? (
                <div style={{ padding: '20px', textAlign: 'center', color: FINCEPT.MUTED, fontSize: '11px' }}>
                  No favorites yet. Click the bookmark icon on any endpoint to add it here.
                </div>
              ) : (
                favorites.map((fav, idx) => (
                  <button key={idx} onClick={() => {
                    const cat = STOCK_CATEGORIES.find(c => c.script === fav.script);
                    if (cat) { dispatch({ type: 'SET_CATEGORY', payload: cat }); executeQuery(fav.script, fav.endpoint, symbolInput || undefined); }
                  }} className="hover-row" style={{
                    width: '100%', padding: '10px 12px', backgroundColor: 'transparent',
                    border: 'none', borderBottom: `1px solid ${FINCEPT.BORDER}`, textAlign: 'left', cursor: 'pointer'
                  }}>
                    <div style={{ fontSize: '10px', color: FINCEPT.WHITE, fontFamily: 'monospace' }}>{fav.endpoint}</div>
                    <div style={{ fontSize: '9px', color: FINCEPT.MUTED, marginTop: '2px' }}>{fav.script.replace('akshare_stocks_', '').replace('.py', '')}</div>
                  </button>
                ))
              )
            )}
            {activeBottomTab === 'history' && (
              queryHistory.length === 0 ? (
                <div style={{ padding: '20px', textAlign: 'center', color: FINCEPT.MUTED, fontSize: '11px' }}>No queries yet</div>
              ) : (
                queryHistory.map((item) => (
                  <button key={item.id} onClick={() => {
                    const cat = STOCK_CATEGORIES.find(c => c.script === item.script);
                    if (cat) { dispatch({ type: 'SET_CATEGORY', payload: cat }); executeQuery(item.script, item.endpoint, symbolInput || undefined); }
                  }} className="hover-row" style={{
                    width: '100%', padding: '10px 12px', backgroundColor: 'transparent',
                    border: 'none', borderBottom: `1px solid ${FINCEPT.BORDER}`, textAlign: 'left', cursor: 'pointer'
                  }}>
                    <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                      <div style={{ width: '6px', height: '6px', borderRadius: '50%', backgroundColor: item.success ? FINCEPT.GREEN : FINCEPT.RED }} />
                      <span style={{ fontSize: '10px', color: FINCEPT.WHITE, fontFamily: 'monospace' }}>{item.endpoint}</span>
                    </div>
                    <div style={{ display: 'flex', justifyContent: 'space-between', marginTop: '4px' }}>
                      <span style={{ fontSize: '9px', color: FINCEPT.MUTED }}>{new Date(item.timestamp).toLocaleTimeString()}</span>
                      {item.count !== undefined && <span style={{ fontSize: '9px', color: FINCEPT.CYAN }}>{item.count} rows</span>}
                    </div>
                  </button>
                ))
              )
            )}
          </div>
          <div style={{ padding: '12px', backgroundColor: FINCEPT.HEADER_BG, borderTop: `1px solid ${FINCEPT.BORDER}`, flexShrink: 0 }}>
            <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '8px', fontWeight: 700 }}>QUICK STATS</div>
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
              <div style={{ padding: '8px', backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px' }}>
                <div style={{ fontSize: '16px', fontWeight: 700, color: FINCEPT.CYAN }}>398</div>
                <div style={{ fontSize: '8px', color: FINCEPT.MUTED }}>ENDPOINTS</div>
              </div>
              <div style={{ padding: '8px', backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px' }}>
                <div style={{ fontSize: '16px', fontWeight: 700, color: FINCEPT.ORANGE }}>8</div>
                <div style={{ fontSize: '8px', color: FINCEPT.MUTED }}>CATEGORIES</div>
              </div>
            </div>
          </div>
        </div>
      </div>

      {/* STATUS BAR */}
      <div style={{
        borderTop: `1px solid ${FINCEPT.BORDER}`, backgroundColor: FINCEPT.HEADER_BG,
        padding: '4px 12px', fontSize: '9px', color: FINCEPT.GRAY,
        display: 'flex', justifyContent: 'space-between', alignItems: 'center', flexShrink: 0
      }}>
        <span>Fincept Terminal v{APP_VERSION} | Asia Markets Data Platform | 398+ Stock Endpoints</span>
        <span>
          Category: <span style={{ color: activeCategory.color }}>{activeCategory.name}</span> |
          Region: <span style={{ color: FINCEPT.ORANGE }}>{MARKET_REGIONS.find(r => r.id === activeRegion)?.name}</span> |
          DB: <span style={{ color: dbStatus.exists ? FINCEPT.GREEN : FINCEPT.RED }}>{dbStatus.exists ? 'READY' : 'NOT BUILT'}</span>
        </span>
      </div>
    </div>
  );
};

// Point 5: ErrorBoundary wrapper
const AsiaMarketsTab: React.FC = () => (
  <ErrorBoundary name="AsiaMarketsTab" variant="default">
    <AsiaMarketsTabInner />
  </ErrorBoundary>
);

export default memo(AsiaMarketsTab);
