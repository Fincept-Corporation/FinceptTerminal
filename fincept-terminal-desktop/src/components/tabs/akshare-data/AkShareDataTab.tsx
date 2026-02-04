/**
 * AKShare Data Explorer Tab
 *
 * Comprehensive interface for exploring 26 AKShare data sources with 1000+ endpoints:
 * - Bonds (Chinese & International) - 28 endpoints
 * - Derivatives (Options, Futures) - 46 endpoints
 * - China Economics (GDP, CPI, PMI, etc.) - 85 endpoints
 * - Global Economics (US, EU, UK, Japan, etc.) - 35 endpoints
 * - Funds (ETFs, Mutual Funds) - 70+ endpoints
 * - Stocks (Realtime, Historical, Financial) - 150+ endpoints
 * - Alternative Data (Air Quality, Carbon, Real Estate, etc.) - 14 endpoints
 * - Macro Global (96 endpoints) - Multi-country economic indicators
 * - Miscellaneous (129 endpoints) - AMAC, FRED, Car Sales, Movies, etc.
 *
 * STATE_MANAGEMENT.md compliant:
 * 1. Race Conditions     - useReducer for atomic state updates
 * 2. State Machine       - Explicit idle/loading/success/error per async flow
 * 3. Cleanup on Unmount  - AbortController + mountedRef
 * 4. Request Dedup       - withTimeout wraps all external calls
 * 5. Error Boundary      - ErrorBoundary wrapper
 * 6. WebSocket           - N/A
 * 7. Cache               - N/A (on-demand data explorer)
 * 8. Timeout Protection  - withTimeout(30s)
 * 9. Shared State Safety - Single useReducer, immutable updates
 * 10. Input Validation   - sanitizeInput on user text inputs
 */

import React, { useReducer, useEffect, useCallback, useMemo, useRef, memo } from 'react';
import { useTranslation } from 'react-i18next';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { useLanguage } from '@/contexts/LanguageContext';
import { AKShareAPI, AKSHARE_DATA_SOURCES, AKShareDataSource } from '@/services/akshareApi';
import { parseAKShareResponse, isValidParsedData } from '@/lib/akshareDataParser';
import { ErrorBoundary } from '@/components/common/ErrorBoundary';
import { withTimeout } from '@/services/core/apiUtils';
import { sanitizeInput } from '@/services/core/validators';
import {
  Search, RefreshCw, Download, ChevronRight, ChevronDown, Database,
  Landmark, LineChart, TrendingUp, Globe, PieChart, Layers, BarChart3,
  AlertCircle, CheckCircle2, Clock, Filter, X, Copy, Table, Code,
  ArrowUpDown, Bookmark, BookmarkCheck, History, Zap, Building2,
  Activity, DollarSign, Newspaper, Building, Bitcoin, FileText, Users,
  ArrowRightLeft, LayoutGrid, Percent, Flame, ChevronLeft, PanelLeftClose, PanelLeftOpen
} from 'lucide-react';

// ============================================================================
// Constants
// ============================================================================

const FINCEPT = {
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
};

const TERMINAL_STYLES = `
  .akshare-scrollbar::-webkit-scrollbar { width: 6px; height: 6px; }
  .akshare-scrollbar::-webkit-scrollbar-track { background: ${FINCEPT.DARK_BG}; }
  .akshare-scrollbar::-webkit-scrollbar-thumb { background: ${FINCEPT.BORDER}; border-radius: 3px; }
  .akshare-scrollbar::-webkit-scrollbar-thumb:hover { background: ${FINCEPT.MUTED}; }
  .akshare-hover-row:hover { background: ${FINCEPT.HOVER} !important; }
`;

const API_TIMEOUT = 120_000; // Increased to 120s for slow endpoints

const IconMap: Record<string, React.FC<{ size?: number; color?: string }>> = {
  Landmark, LineChart, TrendingUp, Globe, PieChart, Layers, BarChart3,
  Building2, Activity, DollarSign, Zap, Bitcoin, Newspaper, Building,
  Clock, FileText, Users, ArrowRightLeft, LayoutGrid, Percent, Flame,
};

// ============================================================================
// Types
// ============================================================================

type AsyncStatus = 'idle' | 'loading' | 'success' | 'error';

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

interface AkShareState {
  // Source & endpoint selection
  selectedSource: AKShareDataSource | null;
  endpoints: string[];
  categories: Record<string, string[]>;
  selectedEndpoint: string | null;
  searchQuery: string;
  expandedCategories: Set<string>;

  // Data state (state machine)
  dataStatus: AsyncStatus;
  data: any[] | null;
  error: string | null;
  responseInfo: { count: number; timestamp: number; source?: string } | null;

  // Endpoints loading (state machine)
  endpointsStatus: AsyncStatus;


  // View state
  viewMode: 'table' | 'json';
  sortColumn: string | null;
  sortDirection: 'asc' | 'desc';

  // Pagination
  currentPage: number;
  pageSize: number;

  // Panel collapse
  isSourcesPanelCollapsed: boolean;
  isEndpointsPanelCollapsed: boolean;

  // History & Favorites
  queryHistory: QueryHistoryItem[];
  favorites: FavoriteEndpoint[];
  showHistory: boolean;
  showFavorites: boolean;

  // Query Parameters
  symbol: string;
  startDate: string;
  endDate: string;
  period: string;
  market: string;
  adjust: string;
  showParameters: boolean;
}

// ============================================================================
// Actions
// ============================================================================

type AkShareAction =
  | { type: 'SELECT_SOURCE'; payload: AKShareDataSource }
  | { type: 'SET_SEARCH_QUERY'; payload: string }
  | { type: 'TOGGLE_CATEGORY'; payload: string }
  | { type: 'SELECT_ENDPOINT'; payload: string }
  // Endpoints loading
  | { type: 'ENDPOINTS_LOADING' }
  | { type: 'ENDPOINTS_SUCCESS'; payload: { endpoints: string[]; categories: Record<string, string[]> } }
  | { type: 'ENDPOINTS_ERROR' }
  // Data loading
  | { type: 'QUERY_START'; payload: string }
  | { type: 'QUERY_SUCCESS'; payload: { data: any[]; responseInfo: { count: number; timestamp: number; source?: string } } }
  | { type: 'QUERY_ERROR'; payload: string }
  // View
  | { type: 'SET_VIEW_MODE'; payload: 'table' | 'json' }
  | { type: 'SET_SORT'; payload: { column: string | null; direction: 'asc' | 'desc' } }
  | { type: 'SET_PAGE'; payload: number }
  | { type: 'SET_PAGE_SIZE'; payload: number }
  // Panel
  | { type: 'TOGGLE_SOURCES_PANEL' }
  | { type: 'TOGGLE_ENDPOINTS_PANEL' }
  | { type: 'TOGGLE_HISTORY' }
  | { type: 'TOGGLE_FAVORITES' }
  | { type: 'TOGGLE_PARAMETERS' }
  // History & Favorites
  | { type: 'ADD_HISTORY'; payload: QueryHistoryItem }
  | { type: 'SET_HISTORY'; payload: QueryHistoryItem[] }
  | { type: 'SET_FAVORITES'; payload: FavoriteEndpoint[] }
  | { type: 'TOGGLE_FAVORITE'; payload: { script: string; endpoint: string } }
  // Query params
  | { type: 'SET_SYMBOL'; payload: string }
  | { type: 'SET_START_DATE'; payload: string }
  | { type: 'SET_END_DATE'; payload: string }
  | { type: 'SET_PERIOD'; payload: string }
  | { type: 'SET_MARKET'; payload: string }
  | { type: 'SET_ADJUST'; payload: string };

// ============================================================================
// Reducer
// ============================================================================

const initialState: AkShareState = {
  selectedSource: null,
  endpoints: [],
  categories: {},
  selectedEndpoint: null,
  searchQuery: '',
  expandedCategories: new Set(),
  dataStatus: 'idle',
  data: null,
  error: null,
  responseInfo: null,
  endpointsStatus: 'idle',
  viewMode: 'table',
  sortColumn: null,
  sortDirection: 'asc',
  currentPage: 1,
  pageSize: 10,
  isSourcesPanelCollapsed: false,
  isEndpointsPanelCollapsed: false,
  queryHistory: [],
  favorites: [],
  showHistory: false,
  showFavorites: false,
  symbol: '000001',
  startDate: '',
  endDate: '',
  period: 'daily',
  market: 'sh',
  adjust: '',
  showParameters: false,
};

function akshareReducer(state: AkShareState, action: AkShareAction): AkShareState {
  switch (action.type) {
    case 'SELECT_SOURCE':
      return {
        ...state,
        selectedSource: action.payload,
        endpoints: [],
        categories: {},
        expandedCategories: new Set(),
        endpointsStatus: 'idle',
      };
    case 'SET_SEARCH_QUERY':
      return { ...state, searchQuery: action.payload };
    case 'TOGGLE_CATEGORY': {
      const newExpanded = new Set(state.expandedCategories);
      if (newExpanded.has(action.payload)) {
        newExpanded.delete(action.payload);
      } else {
        newExpanded.add(action.payload);
      }
      return { ...state, expandedCategories: newExpanded };
    }
    case 'SELECT_ENDPOINT':
      return { ...state, selectedEndpoint: action.payload };

    // Endpoints loading state machine
    case 'ENDPOINTS_LOADING':
      return { ...state, endpointsStatus: 'loading', endpoints: [], categories: {} };
    case 'ENDPOINTS_SUCCESS': {
      const firstCategory = Object.keys(action.payload.categories)[0];
      return {
        ...state,
        endpointsStatus: 'success',
        endpoints: action.payload.endpoints,
        categories: action.payload.categories,
        expandedCategories: firstCategory ? new Set([firstCategory]) : new Set(),
      };
    }
    case 'ENDPOINTS_ERROR':
      return { ...state, endpointsStatus: 'error' };

    // Data loading state machine
    case 'QUERY_START':
      return {
        ...state,
        dataStatus: 'loading',
        data: null,
        error: null,
        responseInfo: null,
        selectedEndpoint: action.payload,
        currentPage: 1,
      };
    case 'QUERY_SUCCESS':
      return {
        ...state,
        dataStatus: 'success',
        data: action.payload.data,
        responseInfo: action.payload.responseInfo,
        error: null,
      };
    case 'QUERY_ERROR':
      return { ...state, dataStatus: 'error', error: action.payload };

    // View
    case 'SET_VIEW_MODE':
      return { ...state, viewMode: action.payload };
    case 'SET_SORT':
      return { ...state, sortColumn: action.payload.column, sortDirection: action.payload.direction };
    case 'SET_PAGE':
      return { ...state, currentPage: action.payload };
    case 'SET_PAGE_SIZE':
      return { ...state, pageSize: action.payload, currentPage: 1 };

    // Panel
    case 'TOGGLE_SOURCES_PANEL':
      return { ...state, isSourcesPanelCollapsed: !state.isSourcesPanelCollapsed };
    case 'TOGGLE_ENDPOINTS_PANEL':
      return { ...state, isEndpointsPanelCollapsed: !state.isEndpointsPanelCollapsed };
    case 'TOGGLE_HISTORY':
      return { ...state, showHistory: !state.showHistory };
    case 'TOGGLE_FAVORITES':
      return { ...state, showFavorites: !state.showFavorites };
    case 'TOGGLE_PARAMETERS':
      return { ...state, showParameters: !state.showParameters };

    // History & Favorites
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
      const newFavorites = exists
        ? state.favorites.filter(f => !(f.script === script && f.endpoint === endpoint))
        : [...state.favorites, { script, endpoint, name: endpoint }];
      return { ...state, favorites: newFavorites };
    }

    // Query params
    case 'SET_SYMBOL':
      return { ...state, symbol: action.payload };
    case 'SET_START_DATE':
      return { ...state, startDate: action.payload };
    case 'SET_END_DATE':
      return { ...state, endDate: action.payload };
    case 'SET_PERIOD':
      return { ...state, period: action.payload };
    case 'SET_MARKET':
      return { ...state, market: action.payload };
    case 'SET_ADJUST':
      return { ...state, adjust: action.payload };

    default:
      return state;
  }
}

// ============================================================================
// Component
// ============================================================================

const AkShareDataTabInner: React.FC = () => {
  const { t } = useTranslation();
  const { colors } = useTerminalTheme();
  const { currentLanguage } = useLanguage();
  const [state, dispatch] = useReducer(akshareReducer, initialState);
  const mountedRef = useRef(true);
  const abortRef = useRef<AbortController | null>(null);

  // Destructure for readability
  const {
    selectedSource, endpoints, categories, selectedEndpoint, searchQuery,
    expandedCategories, dataStatus, data, error, responseInfo,
    endpointsStatus, viewMode, sortColumn, sortDirection, currentPage, pageSize,
    isSourcesPanelCollapsed, isEndpointsPanelCollapsed,
    queryHistory, favorites, showHistory, showFavorites,
    symbol, startDate, endDate, period, market, adjust, showParameters,
  } = state;

  // ---------- Cleanup on unmount (Point 3) ----------
  useEffect(() => {
    mountedRef.current = true;
    return () => {
      mountedRef.current = false;
      abortRef.current?.abort();
    };
  }, []);

  // ---------- Load favorites & history from localStorage ----------
  useEffect(() => {
    try {
      const saved = localStorage.getItem('akshare_favorites');
      if (saved) dispatch({ type: 'SET_FAVORITES', payload: JSON.parse(saved) });
    } catch { /* ignore */ }
    try {
      const savedHistory = localStorage.getItem('akshare_history');
      if (savedHistory) dispatch({ type: 'SET_HISTORY', payload: JSON.parse(savedHistory).slice(0, 50) });
    } catch { /* ignore */ }
  }, []);

  // ---------- Persist favorites ----------
  useEffect(() => {
    localStorage.setItem('akshare_favorites', JSON.stringify(favorites));
  }, [favorites]);

  // ---------- Persist history ----------
  useEffect(() => {
    localStorage.setItem('akshare_history', JSON.stringify(queryHistory));
  }, [queryHistory]);

  // ---------- Load endpoints when source changes (Point 3: AbortController) ----------
  useEffect(() => {
    if (!selectedSource) return;
    const controller = new AbortController();
    abortRef.current?.abort();
    abortRef.current = controller;

    const load = async () => {
      dispatch({ type: 'ENDPOINTS_LOADING' });
      try {
        const response = await withTimeout(
          AKShareAPI.getEndpoints(selectedSource.script),
          API_TIMEOUT,
          'Endpoints loading timeout'
        );
        if (controller.signal.aborted || !mountedRef.current) return;

        if (response.success && response.data) {
          let endpointData = response.data as any;
          if (endpointData.data && typeof endpointData.data === 'object') {
            endpointData = endpointData.data;
          }
          const availableEndpoints = endpointData.available_endpoints || endpointData.endpoints || [];
          const categoriesData = endpointData.categories || {};
          dispatch({
            type: 'ENDPOINTS_SUCCESS',
            payload: {
              endpoints: Array.isArray(availableEndpoints) ? availableEndpoints : [],
              categories: categoriesData,
            },
          });
        } else {
          dispatch({ type: 'ENDPOINTS_ERROR' });
        }
      } catch (err) {
        if (!controller.signal.aborted && mountedRef.current) {
          dispatch({ type: 'ENDPOINTS_ERROR' });
        }
      }
    };
    load();

    return () => { controller.abort(); };
  }, [selectedSource]);

  // ---------- Build parameters for endpoint ----------
  const buildParametersForEndpoint = useCallback((endpoint: string): string[] | undefined => {
    const params: string[] = [];
    const needsSymbol = endpoint.includes('stock') || endpoint.includes('holder') ||
                       endpoint.includes('fund') || endpoint.includes('esg') ||
                       endpoint.includes('comment') || endpoint.includes('individual');
    const needsDates = endpoint.includes('hist') || endpoint.includes('historical');
    const needsPeriod = endpoint.includes('hist') && !endpoint.includes('min');
    const needsMarket = endpoint.includes('individual_fund_flow');

    // Point 10: sanitize user inputs before building params
    if (needsSymbol && symbol) params.push(sanitizeInput(symbol));
    if (needsPeriod && period) params.push(sanitizeInput(period));
    if (needsDates) {
      if (startDate) params.push(sanitizeInput(startDate));
      if (endDate) params.push(sanitizeInput(endDate));
    }
    if (adjust) params.push(sanitizeInput(adjust));
    if (needsMarket && market) params.push(sanitizeInput(market));

    return params.length > 0 ? params : undefined;
  }, [symbol, period, startDate, endDate, adjust, market]);

  // ---------- Execute query (Point 8: timeout, Point 10: sanitize) ----------
  const executeQuery = useCallback(async (script: string, endpoint: string, customParams?: string[]) => {
    // Abort previous request
    abortRef.current?.abort();
    const controller = new AbortController();
    abortRef.current = controller;

    dispatch({ type: 'QUERY_START', payload: endpoint });

    try {
      const params = customParams || buildParametersForEndpoint(endpoint);
      const response = await withTimeout(
        AKShareAPI.query(script, endpoint, params),
        API_TIMEOUT,
        'Query timeout'
      );

      if (controller.signal.aborted || !mountedRef.current) return;

      const parsed = parseAKShareResponse(response);

      if (!isValidParsedData(parsed)) {
        const errorMsg = parsed.warnings?.join(', ') || 'Failed to parse response data';
        dispatch({ type: 'QUERY_ERROR', payload: errorMsg });
        dispatch({
          type: 'ADD_HISTORY',
          payload: { id: `${Date.now()}`, script, endpoint, timestamp: Date.now(), success: false, count: 0 },
        });
        return;
      }

      dispatch({
        type: 'ADD_HISTORY',
        payload: { id: `${Date.now()}`, script, endpoint, timestamp: Date.now(), success: true, count: parsed.count },
      });

      dispatch({
        type: 'QUERY_SUCCESS',
        payload: {
          data: parsed.data,
          responseInfo: {
            count: parsed.count,
            timestamp: parsed.metadata?.timestamp || Date.now(),
            source: parsed.metadata?.source,
          },
        },
      });
    } catch (err: any) {
      if (!controller.signal.aborted && mountedRef.current) {
        dispatch({ type: 'QUERY_ERROR', payload: err.message || 'Query failed' });
        dispatch({
          type: 'ADD_HISTORY',
          payload: { id: `${Date.now()}`, script, endpoint, timestamp: Date.now(), success: false, count: 0 },
        });
      }
    }
  }, [buildParametersForEndpoint]);

  // ---------- Derived state (useMemo, not useEffect) ----------
  const filteredEndpoints = useMemo(() => {
    if (!searchQuery) return endpoints;
    const query = searchQuery.toLowerCase();
    return endpoints.filter(e => e.toLowerCase().includes(query));
  }, [endpoints, searchQuery]);

  const filteredCategories = useMemo(() => {
    if (!searchQuery) return categories;
    const query = searchQuery.toLowerCase();
    const filtered: Record<string, string[]> = {};
    for (const [cat, eps] of Object.entries(categories)) {
      const matchingEps = eps.filter(e => e.toLowerCase().includes(query));
      if (matchingEps.length > 0 || cat.toLowerCase().includes(query)) {
        filtered[cat] = matchingEps.length > 0 ? matchingEps : eps;
      }
    }
    return filtered;
  }, [categories, searchQuery]);

  const paginatedData = useMemo(() => {
    if (!data) return null;
    const startIdx = (currentPage - 1) * pageSize;
    const endIdx = startIdx + pageSize;
    return data.slice(startIdx, endIdx);
  }, [data, currentPage, pageSize]);

  const totalPages = useMemo(() => {
    if (!data) return 0;
    return Math.ceil(data.length / pageSize);
  }, [data, pageSize]);

  const sortedData = useMemo(() => {
    if (!paginatedData || !sortColumn) return paginatedData;
    return [...paginatedData].sort((a, b) => {
      const aVal = a[sortColumn];
      const bVal = b[sortColumn];
      if (aVal === bVal) return 0;
      if (aVal === null || aVal === undefined) return 1;
      if (bVal === null || bVal === undefined) return -1;
      const comparison = aVal < bVal ? -1 : 1;
      return sortDirection === 'asc' ? comparison : -comparison;
    });
  }, [paginatedData, sortColumn, sortDirection]);

  const columns = useMemo(() => {
    if (!paginatedData || paginatedData.length === 0) return [];
    return Object.keys(paginatedData[0]);
  }, [paginatedData]);

  // ---------- Utility handlers ----------
  const copyToClipboard = useCallback((text: string) => {
    navigator.clipboard.writeText(text);
  }, []);

  const downloadCSV = useCallback(() => {
    if (!data || data.length === 0) return;
    const allColumns = Object.keys(data[0]);
    const headers = allColumns.join(',');
    const rows = data.map(row =>
      allColumns.map(col => {
        const val = row[col];
        if (typeof val === 'string' && (val.includes(',') || val.includes('"'))) {
          return `"${val.replace(/"/g, '""')}"`;
        }
        return val;
      }).join(',')
    );
    const csv = [headers, ...rows].join('\n');
    const blob = new Blob([csv], { type: 'text/csv;charset=utf-8;' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `akshare_${selectedEndpoint}_${Date.now()}.csv`;
    a.click();
    URL.revokeObjectURL(url);
  }, [data, selectedEndpoint]);

  const downloadJSON = useCallback(() => {
    if (!data) return;
    const json = JSON.stringify(data, null, 2);
    const blob = new Blob([json], { type: 'application/json;charset=utf-8;' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `akshare_${selectedEndpoint}_${Date.now()}.json`;
    a.click();
    URL.revokeObjectURL(url);
  }, [data, selectedEndpoint]);

  const handleSortClick = useCallback((col: string) => {
    if (sortColumn === col) {
      dispatch({ type: 'SET_SORT', payload: { column: col, direction: sortDirection === 'asc' ? 'desc' : 'asc' } });
    } else {
      dispatch({ type: 'SET_SORT', payload: { column: col, direction: 'asc' } });
    }
  }, [sortColumn, sortDirection]);

  const isFavorite = useCallback((script: string, endpoint: string) => {
    return favorites.some(f => f.script === script && f.endpoint === endpoint);
  }, [favorites]);

  // ---------- Render ----------
  return (
    <div style={{
      display: 'flex',
      flexDirection: 'column',
      height: '100%',
      backgroundColor: FINCEPT.DARK_BG,
      fontFamily: '"IBM Plex Mono", "Consolas", monospace',
    }}>
      <style>{TERMINAL_STYLES}</style>

      {/* Top Navigation Bar */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        padding: '8px 16px',
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `2px solid ${FINCEPT.ORANGE}`,
        boxShadow: `0 2px 8px ${FINCEPT.ORANGE}20`,
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          <Database size={20} color={FINCEPT.ORANGE} />
          <span style={{ fontWeight: 700, color: FINCEPT.WHITE, fontSize: '12px', letterSpacing: '0.5px' }}>
            AKSHARE DATA EXPLORER
          </span>
          <span style={{
            padding: '2px 8px',
            backgroundColor: `${FINCEPT.ORANGE}20`,
            color: FINCEPT.ORANGE,
            fontSize: '9px',
            fontWeight: 700,
            borderRadius: '2px',
          }}>
            400+ ENDPOINTS
          </span>
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginLeft: '16px' }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
              <div style={{ width: '6px', height: '6px', borderRadius: '50%', backgroundColor: selectedSource ? FINCEPT.GREEN : FINCEPT.MUTED }} />
              <span style={{ fontSize: '9px', color: FINCEPT.GRAY }}>SOURCE</span>
            </div>
            <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
              <div style={{ width: '6px', height: '6px', borderRadius: '50%', backgroundColor: data ? FINCEPT.GREEN : FINCEPT.MUTED }} />
              <span style={{ fontSize: '9px', color: FINCEPT.GRAY }}>DATA</span>
            </div>
          </div>
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <button
            onClick={() => dispatch({ type: 'TOGGLE_FAVORITES' })}
            style={{
              padding: '6px',
              borderRadius: '2px',
              backgroundColor: showFavorites ? `${FINCEPT.YELLOW}15` : 'transparent',
              border: 'none',
              cursor: 'pointer',
              transition: 'all 0.2s',
            }}
            title="Favorites"
          >
            {showFavorites ? <BookmarkCheck size={16} color={FINCEPT.YELLOW} /> : <Bookmark size={16} color={FINCEPT.MUTED} />}
          </button>
          <button
            onClick={() => dispatch({ type: 'TOGGLE_HISTORY' })}
            style={{
              padding: '6px',
              borderRadius: '2px',
              backgroundColor: showHistory ? `${FINCEPT.CYAN}15` : 'transparent',
              border: 'none',
              cursor: 'pointer',
              transition: 'all 0.2s',
            }}
            title="History"
          >
            <History size={16} color={showHistory ? FINCEPT.CYAN : FINCEPT.MUTED} />
          </button>
        </div>
      </div>

      {/* Main Content Area */}
      <div style={{ display: 'flex', flex: 1, overflow: 'hidden' }}>
        {/* Left Panel - Data Sources */}
        {!isSourcesPanelCollapsed && (
          <div style={{
            width: '280px',
            borderRight: `1px solid ${FINCEPT.BORDER}`,
            display: 'flex',
            flexDirection: 'column',
            backgroundColor: FINCEPT.PANEL_BG,
          }}>
            <div style={{
              padding: '12px',
              backgroundColor: FINCEPT.HEADER_BG,
              borderBottom: `1px solid ${FINCEPT.BORDER}`,
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'space-between',
            }}>
              <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
                DATA SOURCES
              </span>
              <button
                onClick={() => dispatch({ type: 'TOGGLE_SOURCES_PANEL' })}
                style={{ padding: '4px', borderRadius: '2px', backgroundColor: 'transparent', border: 'none', cursor: 'pointer' }}
                title="Collapse panel"
              >
                <ChevronLeft size={14} color={FINCEPT.MUTED} />
              </button>
            </div>
            <div className="akshare-scrollbar" style={{ flex: 1, overflowY: 'auto' }}>
              {AKSHARE_DATA_SOURCES.map(source => {
                const IconComponent = IconMap[source.icon] || Database;
                const isSelected = selectedSource?.id === source.id;
                return (
                  <button
                    key={source.id}
                    onClick={() => dispatch({ type: 'SELECT_SOURCE', payload: source })}
                    style={{
                      width: '100%',
                      padding: '10px 12px',
                      display: 'flex',
                      alignItems: 'flex-start',
                      gap: '10px',
                      textAlign: 'left',
                      backgroundColor: isSelected ? `${FINCEPT.ORANGE}15` : 'transparent',
                      borderLeft: isSelected ? `2px solid ${FINCEPT.ORANGE}` : '2px solid transparent',
                      borderTop: 'none',
                      borderRight: 'none',
                      borderBottom: `1px solid ${FINCEPT.BORDER}`,
                      cursor: 'pointer',
                      transition: 'all 0.2s',
                    }}
                  >
                    <IconComponent size={16} color={isSelected ? FINCEPT.ORANGE : source.color} />
                    <div style={{ flex: 1, minWidth: 0 }}>
                      <div style={{ fontWeight: 600, color: FINCEPT.WHITE, fontSize: '10px' }}>{source.name}</div>
                      <div style={{
                        fontSize: '9px',
                        marginTop: '2px',
                        color: FINCEPT.MUTED,
                        overflow: 'hidden',
                        textOverflow: 'ellipsis',
                        display: '-webkit-box',
                        WebkitLineClamp: 2,
                        WebkitBoxOrient: 'vertical' as any,
                      }}>
                        {source.description}
                      </div>
                    </div>
                  </button>
                );
              })}
            </div>
          </div>
        )}

        {/* Collapsed Sources Panel Button */}
        {isSourcesPanelCollapsed && (
          <div style={{
            width: '36px',
            borderRight: `1px solid ${FINCEPT.BORDER}`,
            display: 'flex',
            flexDirection: 'column',
            backgroundColor: FINCEPT.PANEL_BG,
          }}>
            <button
              onClick={() => dispatch({ type: 'TOGGLE_SOURCES_PANEL' })}
              style={{ padding: '10px', backgroundColor: 'transparent', border: 'none', cursor: 'pointer' }}
              title="Expand Data Sources"
            >
              <ChevronRight size={14} color={FINCEPT.ORANGE} />
            </button>
          </div>
        )}

        {/* Middle Panel - Endpoints */}
        {!isEndpointsPanelCollapsed && (
          <div style={{
            width: '300px',
            borderRight: `1px solid ${FINCEPT.BORDER}`,
            display: 'flex',
            flexDirection: 'column',
            backgroundColor: FINCEPT.PANEL_BG,
          }}>
            {selectedSource ? (
              <>
                <div style={{
                  padding: '12px',
                  backgroundColor: FINCEPT.HEADER_BG,
                  borderBottom: `1px solid ${FINCEPT.BORDER}`,
                }}>
                  <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '8px' }}>
                    <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>ENDPOINTS</span>
                    <button
                      onClick={() => dispatch({ type: 'TOGGLE_ENDPOINTS_PANEL' })}
                      style={{ padding: '4px', borderRadius: '2px', backgroundColor: 'transparent', border: 'none', cursor: 'pointer' }}
                      title="Collapse panel"
                    >
                      <ChevronLeft size={14} color={FINCEPT.MUTED} />
                    </button>
                  </div>
                  <div style={{ position: 'relative' }}>
                    <Search size={14} style={{ position: 'absolute', left: '10px', top: '50%', transform: 'translateY(-50%)' }} color={FINCEPT.MUTED} />
                    <input
                      type="text"
                      value={searchQuery}
                      onChange={e => dispatch({ type: 'SET_SEARCH_QUERY', payload: sanitizeInput(e.target.value) })}
                      placeholder="Search endpoints..."
                      style={{
                        width: '100%',
                        padding: '8px 10px 8px 32px',
                        backgroundColor: FINCEPT.DARK_BG,
                        border: `1px solid ${FINCEPT.BORDER}`,
                        borderRadius: '2px',
                        color: FINCEPT.WHITE,
                        fontSize: '10px',
                        fontFamily: '"IBM Plex Mono", monospace',
                      }}
                    />
                    {searchQuery && (
                      <button
                        onClick={() => dispatch({ type: 'SET_SEARCH_QUERY', payload: '' })}
                        style={{
                          position: 'absolute',
                          right: '8px',
                          top: '50%',
                          transform: 'translateY(-50%)',
                          background: 'none',
                          border: 'none',
                          cursor: 'pointer',
                        }}
                      >
                        <X size={12} color={FINCEPT.MUTED} />
                      </button>
                    )}
                  </div>
                </div>
                <div style={{ padding: '8px 12px', fontSize: '9px', color: FINCEPT.MUTED, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
                  {filteredEndpoints.length} endpoints available
                </div>

                <div className="akshare-scrollbar" style={{ flex: 1, overflowY: 'auto' }}>
                  {endpointsStatus === 'loading' ? (
                    <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', padding: '32px' }}>
                      <RefreshCw size={18} className="animate-spin" color={FINCEPT.ORANGE} />
                    </div>
                  ) : Object.keys(filteredCategories).length > 0 ? (
                    Object.entries(filteredCategories).map(([category, categoryEndpoints]) => (
                      <div key={category}>
                        <button
                          onClick={() => dispatch({ type: 'TOGGLE_CATEGORY', payload: category })}
                          style={{
                            width: '100%',
                            padding: '8px 12px',
                            display: 'flex',
                            alignItems: 'center',
                            gap: '8px',
                            textAlign: 'left',
                            backgroundColor: FINCEPT.HEADER_BG,
                            border: 'none',
                            borderBottom: `1px solid ${FINCEPT.BORDER}`,
                            cursor: 'pointer',
                          }}
                        >
                          {expandedCategories.has(category) ? (
                            <ChevronDown size={12} color={FINCEPT.MUTED} />
                          ) : (
                            <ChevronRight size={12} color={FINCEPT.MUTED} />
                          )}
                          <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.ORANGE, letterSpacing: '0.5px' }}>
                            {category.toUpperCase()}
                          </span>
                          <span style={{ fontSize: '9px', marginLeft: 'auto', color: FINCEPT.MUTED }}>
                            {categoryEndpoints.length}
                          </span>
                        </button>

                        {expandedCategories.has(category) && (
                          <div style={{ padding: '4px 0' }}>
                            {categoryEndpoints.map(endpoint => {
                              const isActive = selectedEndpoint === endpoint;
                              const isFav = isFavorite(selectedSource.script, endpoint);
                              return (
                                <div key={endpoint} style={{ display: 'flex', alignItems: 'center', gap: '4px', padding: '0 8px' }}>
                                  <button
                                    onClick={() => executeQuery(selectedSource.script, endpoint)}
                                    style={{
                                      flex: 1,
                                      padding: '6px 10px',
                                      textAlign: 'left',
                                      fontSize: '10px',
                                      backgroundColor: isActive ? `${FINCEPT.ORANGE}15` : 'transparent',
                                      borderLeft: isActive ? `2px solid ${FINCEPT.ORANGE}` : '2px solid transparent',
                                      color: isActive ? FINCEPT.CYAN : FINCEPT.WHITE,
                                      borderTop: 'none',
                                      borderRight: 'none',
                                      borderBottom: 'none',
                                      borderRadius: '2px',
                                      cursor: 'pointer',
                                      transition: 'all 0.2s',
                                      overflow: 'hidden',
                                      textOverflow: 'ellipsis',
                                      whiteSpace: 'nowrap',
                                    }}
                                  >
                                    {endpoint}
                                  </button>
                                  <button
                                    onClick={() => dispatch({ type: 'TOGGLE_FAVORITE', payload: { script: selectedSource.script, endpoint } })}
                                    style={{ padding: '4px', backgroundColor: 'transparent', border: 'none', borderRadius: '2px', cursor: 'pointer' }}
                                  >
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
                    <div style={{ padding: '4px 0' }}>
                      {filteredEndpoints.map(endpoint => {
                        const isActive = selectedEndpoint === endpoint;
                        const isFav = isFavorite(selectedSource.script, endpoint);
                        return (
                          <div key={endpoint} style={{ display: 'flex', alignItems: 'center', gap: '4px', padding: '0 8px' }}>
                            <button
                              onClick={() => executeQuery(selectedSource.script, endpoint)}
                              style={{
                                flex: 1,
                                padding: '6px 10px',
                                textAlign: 'left',
                                fontSize: '10px',
                                backgroundColor: isActive ? `${FINCEPT.ORANGE}15` : 'transparent',
                                borderLeft: isActive ? `2px solid ${FINCEPT.ORANGE}` : '2px solid transparent',
                                color: isActive ? FINCEPT.CYAN : FINCEPT.WHITE,
                                borderTop: 'none',
                                borderRight: 'none',
                                borderBottom: 'none',
                                borderRadius: '2px',
                                cursor: 'pointer',
                                transition: 'all 0.2s',
                                overflow: 'hidden',
                                textOverflow: 'ellipsis',
                                whiteSpace: 'nowrap',
                              }}
                            >
                              {endpoint}
                            </button>
                            <button
                              onClick={() => dispatch({ type: 'TOGGLE_FAVORITE', payload: { script: selectedSource.script, endpoint } })}
                              style={{ padding: '4px', backgroundColor: 'transparent', border: 'none', borderRadius: '2px', cursor: 'pointer' }}
                            >
                              {isFav ? <BookmarkCheck size={12} color={FINCEPT.YELLOW} /> : <Bookmark size={12} color={FINCEPT.MUTED} />}
                            </button>
                          </div>
                        );
                      })}
                    </div>
                  )}
                </div>
              </>
            ) : (
              <div style={{ flex: 1, display: 'flex', alignItems: 'center', justifyContent: 'center', padding: '16px' }}>
                <div style={{ textAlign: 'center' }}>
                  <Database size={24} color={FINCEPT.MUTED} style={{ marginBottom: '12px' }} />
                  <div style={{ fontSize: '10px', color: FINCEPT.MUTED }}>
                    Select a data source to browse endpoints
                  </div>
                </div>
              </div>
            )}
          </div>
        )}

        {/* Collapsed Endpoints Panel Button */}
        {isEndpointsPanelCollapsed && (
          <div style={{
            width: '36px',
            borderRight: `1px solid ${FINCEPT.BORDER}`,
            display: 'flex',
            flexDirection: 'column',
            backgroundColor: FINCEPT.PANEL_BG,
          }}>
            <button
              onClick={() => dispatch({ type: 'TOGGLE_ENDPOINTS_PANEL' })}
              style={{ padding: '10px', backgroundColor: 'transparent', border: 'none', cursor: 'pointer' }}
              title="Expand Endpoints"
            >
              <ChevronRight size={14} color={FINCEPT.ORANGE} />
            </button>
          </div>
        )}

        {/* Right Panel - Data Display */}
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
          {/* Toolbar */}
          <div style={{
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'space-between',
            padding: '8px 16px',
            backgroundColor: FINCEPT.HEADER_BG,
            borderBottom: `1px solid ${FINCEPT.BORDER}`,
          }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
              <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                <button
                  onClick={() => dispatch({ type: 'TOGGLE_SOURCES_PANEL' })}
                  style={{ padding: '6px', borderRadius: '2px', backgroundColor: 'transparent', border: 'none', cursor: 'pointer' }}
                  title={isSourcesPanelCollapsed ? "Show Data Sources" : "Hide Data Sources"}
                >
                  {isSourcesPanelCollapsed ? <PanelLeftOpen size={14} color={FINCEPT.ORANGE} /> : <PanelLeftClose size={14} color={FINCEPT.MUTED} />}
                </button>
                <button
                  onClick={() => dispatch({ type: 'TOGGLE_ENDPOINTS_PANEL' })}
                  style={{ padding: '6px', borderRadius: '2px', backgroundColor: 'transparent', border: 'none', cursor: 'pointer' }}
                  title={isEndpointsPanelCollapsed ? "Show Endpoints" : "Hide Endpoints"}
                >
                  {isEndpointsPanelCollapsed ? <PanelLeftOpen size={14} color={FINCEPT.ORANGE} /> : <PanelLeftClose size={14} color={FINCEPT.MUTED} />}
                </button>
              </div>

              {selectedEndpoint && <div style={{ width: '1px', height: '20px', backgroundColor: FINCEPT.BORDER }} />}

              {selectedEndpoint && (
                <>
                  <span style={{ fontFamily: '"IBM Plex Mono", monospace', fontSize: '10px', color: FINCEPT.CYAN }}>
                    {selectedEndpoint}
                  </span>
                  {responseInfo && (
                    <span style={{
                      fontSize: '9px',
                      padding: '2px 6px',
                      backgroundColor: `${FINCEPT.GREEN}20`,
                      color: FINCEPT.GREEN,
                      borderRadius: '2px',
                    }}>
                      {responseInfo.count} RECORDS
                    </span>
                  )}
                </>
              )}
            </div>
            <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
              <div style={{ display: 'flex', alignItems: 'center', border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', overflow: 'hidden' }}>
                <button
                  onClick={() => dispatch({ type: 'SET_VIEW_MODE', payload: 'table' })}
                  style={{
                    padding: '6px 10px',
                    display: 'flex',
                    alignItems: 'center',
                    gap: '4px',
                    backgroundColor: viewMode === 'table' ? FINCEPT.ORANGE : 'transparent',
                    color: viewMode === 'table' ? FINCEPT.DARK_BG : FINCEPT.MUTED,
                    border: 'none',
                    fontSize: '9px',
                    fontWeight: 700,
                    cursor: 'pointer',
                  }}
                >
                  <Table size={12} />TABLE
                </button>
                <button
                  onClick={() => dispatch({ type: 'SET_VIEW_MODE', payload: 'json' })}
                  style={{
                    padding: '6px 10px',
                    display: 'flex',
                    alignItems: 'center',
                    gap: '4px',
                    backgroundColor: viewMode === 'json' ? FINCEPT.ORANGE : 'transparent',
                    color: viewMode === 'json' ? FINCEPT.DARK_BG : FINCEPT.MUTED,
                    border: 'none',
                    fontSize: '9px',
                    fontWeight: 700,
                    cursor: 'pointer',
                  }}
                >
                  <Code size={12} />JSON
                </button>
              </div>

              {data && (
                <>
                  <button onClick={downloadCSV} style={{ padding: '6px', borderRadius: '2px', backgroundColor: 'transparent', border: `1px solid ${FINCEPT.BORDER}`, cursor: 'pointer' }} title="Download CSV">
                    <Download size={14} color={FINCEPT.MUTED} />
                  </button>
                  <button onClick={() => copyToClipboard(JSON.stringify(data, null, 2))} style={{ padding: '6px', borderRadius: '2px', backgroundColor: 'transparent', border: `1px solid ${FINCEPT.BORDER}`, cursor: 'pointer' }} title="Copy JSON (All Data)">
                    <Copy size={14} color={FINCEPT.MUTED} />
                  </button>
                </>
              )}

              {selectedEndpoint && selectedSource && (
                <button
                  onClick={() => dispatch({ type: 'TOGGLE_PARAMETERS' })}
                  style={{
                    padding: '6px 12px',
                    borderRadius: '2px',
                    display: 'flex',
                    alignItems: 'center',
                    gap: '4px',
                    backgroundColor: showParameters ? FINCEPT.PURPLE : 'transparent',
                    color: showParameters ? FINCEPT.WHITE : FINCEPT.MUTED,
                    border: `1px solid ${showParameters ? FINCEPT.PURPLE : FINCEPT.BORDER}`,
                    fontSize: '9px',
                    fontWeight: 700,
                    cursor: 'pointer',
                  }}
                  title="Toggle Parameters"
                >
                  <Filter size={12} />PARAMS
                </button>
              )}

              {selectedEndpoint && selectedSource && (
                <button
                  onClick={() => executeQuery(selectedSource.script, selectedEndpoint)}
                  disabled={dataStatus === 'loading'}
                  style={{
                    padding: '8px 16px',
                    borderRadius: '2px',
                    display: 'flex',
                    alignItems: 'center',
                    gap: '6px',
                    backgroundColor: FINCEPT.ORANGE,
                    color: FINCEPT.DARK_BG,
                    border: 'none',
                    fontSize: '9px',
                    fontWeight: 700,
                    cursor: dataStatus === 'loading' ? 'not-allowed' : 'pointer',
                    opacity: dataStatus === 'loading' ? 0.7 : 1,
                  }}
                >
                  <RefreshCw size={12} className={dataStatus === 'loading' ? 'animate-spin' : ''} />
                  REFRESH
                </button>
              )}
            </div>
          </div>

          {/* Parameters Panel */}
          {showParameters && selectedEndpoint && (
            <div style={{
              backgroundColor: FINCEPT.PANEL_BG,
              borderTop: `1px solid ${FINCEPT.BORDER}`,
              borderBottom: `1px solid ${FINCEPT.BORDER}`,
              padding: '16px',
            }}>
              <div style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.ORANGE, marginBottom: '12px', letterSpacing: '0.5px' }}>
                QUERY PARAMETERS
              </div>
              <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(180px, 1fr))', gap: '12px' }}>
                <div>
                  <label style={{ display: 'block', fontSize: '9px', color: FINCEPT.MUTED, marginBottom: '6px', fontWeight: 600 }}>SYMBOL / STOCK</label>
                  <input
                    type="text"
                    value={symbol}
                    onChange={(e) => dispatch({ type: 'SET_SYMBOL', payload: sanitizeInput(e.target.value) })}
                    placeholder="e.g., 000001, 600000"
                    style={{ width: '100%', padding: '8px 10px', backgroundColor: FINCEPT.DARK_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', color: FINCEPT.WHITE, fontSize: '10px', fontFamily: '"IBM Plex Mono", monospace', outline: 'none' }}
                  />
                </div>
                <div>
                  <label style={{ display: 'block', fontSize: '9px', color: FINCEPT.MUTED, marginBottom: '6px', fontWeight: 600 }}>PERIOD</label>
                  <select value={period} onChange={(e) => dispatch({ type: 'SET_PERIOD', payload: e.target.value })} style={{ width: '100%', padding: '8px 10px', backgroundColor: FINCEPT.DARK_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', color: FINCEPT.WHITE, fontSize: '10px', fontFamily: '"IBM Plex Mono", monospace', outline: 'none', cursor: 'pointer' }}>
                    <option value="daily">Daily</option>
                    <option value="weekly">Weekly</option>
                    <option value="monthly">Monthly</option>
                  </select>
                </div>
                <div>
                  <label style={{ display: 'block', fontSize: '9px', color: FINCEPT.MUTED, marginBottom: '6px', fontWeight: 600 }}>MARKET</label>
                  <select value={market} onChange={(e) => dispatch({ type: 'SET_MARKET', payload: e.target.value })} style={{ width: '100%', padding: '8px 10px', backgroundColor: FINCEPT.DARK_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', color: FINCEPT.WHITE, fontSize: '10px', fontFamily: '"IBM Plex Mono", monospace', outline: 'none', cursor: 'pointer' }}>
                    <option value="sh">Shanghai (sh)</option>
                    <option value="sz">Shenzhen (sz)</option>
                  </select>
                </div>
                <div>
                  <label style={{ display: 'block', fontSize: '9px', color: FINCEPT.MUTED, marginBottom: '6px', fontWeight: 600 }}>ADJUST</label>
                  <select value={adjust} onChange={(e) => dispatch({ type: 'SET_ADJUST', payload: e.target.value })} style={{ width: '100%', padding: '8px 10px', backgroundColor: FINCEPT.DARK_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', color: FINCEPT.WHITE, fontSize: '10px', fontFamily: '"IBM Plex Mono", monospace', outline: 'none', cursor: 'pointer' }}>
                    <option value="">No Adjustment</option>
                    <option value="qfq">Forward (qfq)</option>
                    <option value="hfq">Backward (hfq)</option>
                  </select>
                </div>
                <div>
                  <label style={{ display: 'block', fontSize: '9px', color: FINCEPT.MUTED, marginBottom: '6px', fontWeight: 600 }}>START DATE</label>
                  <input type="date" value={startDate} onChange={(e) => dispatch({ type: 'SET_START_DATE', payload: e.target.value })} style={{ width: '100%', padding: '8px 10px', backgroundColor: FINCEPT.DARK_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', color: FINCEPT.WHITE, fontSize: '10px', fontFamily: '"IBM Plex Mono", monospace', outline: 'none' }} />
                </div>
                <div>
                  <label style={{ display: 'block', fontSize: '9px', color: FINCEPT.MUTED, marginBottom: '6px', fontWeight: 600 }}>END DATE</label>
                  <input type="date" value={endDate} onChange={(e) => dispatch({ type: 'SET_END_DATE', payload: e.target.value })} style={{ width: '100%', padding: '8px 10px', backgroundColor: FINCEPT.DARK_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', color: FINCEPT.WHITE, fontSize: '10px', fontFamily: '"IBM Plex Mono", monospace', outline: 'none' }} />
                </div>
              </div>
              <div style={{ marginTop: '12px', fontSize: '9px', color: FINCEPT.MUTED, fontStyle: 'italic' }}>
                Parameters are automatically selected based on endpoint. Not all parameters are used for every query.
              </div>
            </div>
          )}

          {/* Data Content */}
          <div className="akshare-scrollbar" style={{ flex: 1, overflowX: 'auto', overflowY: 'auto', padding: '16px' }}>
            {dataStatus === 'loading' ? (
              <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', height: '100%' }}>
                <div style={{ textAlign: 'center' }}>
                  <RefreshCw size={28} className="animate-spin" color={FINCEPT.ORANGE} style={{ marginBottom: '12px' }} />
                  <div style={{ fontSize: '10px', color: FINCEPT.MUTED }}>Loading data...</div>
                </div>
              </div>
            ) : dataStatus === 'error' ? (
              <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', height: '100%' }}>
                <div style={{ textAlign: 'center', maxWidth: '600px', padding: '20px' }}>
                  <AlertCircle size={28} color={FINCEPT.RED} style={{ marginBottom: '12px' }} />
                  <div style={{ fontSize: '11px', fontWeight: 600, color: FINCEPT.WHITE, marginBottom: '8px' }}>Query Failed</div>
                  <div style={{
                    fontSize: '10px',
                    padding: '12px',
                    borderRadius: '2px',
                    fontFamily: '"IBM Plex Mono", monospace',
                    backgroundColor: `${FINCEPT.RED}20`,
                    color: FINCEPT.RED,
                    marginBottom: '12px',
                    textAlign: 'left',
                    maxHeight: '200px',
                    overflow: 'auto',
                  }}>
                    {error}
                  </div>
                  <div style={{ fontSize: '9px', color: FINCEPT.MUTED, marginTop: '12px' }}>
                    <div style={{ marginBottom: '8px' }}>Common issues:</div>
                    <div style={{ textAlign: 'left', lineHeight: '1.6' }}>
                      - The endpoint may require additional parameters<br/>
                      - The data source might be temporarily unavailable<br/>
                      - The response format may have changed<br/>
                      - Network connectivity issues
                    </div>
                  </div>
                  {selectedEndpoint && selectedSource && (
                    <button
                      onClick={() => executeQuery(selectedSource.script, selectedEndpoint)}
                      style={{
                        marginTop: '16px',
                        padding: '8px 16px',
                        borderRadius: '2px',
                        backgroundColor: FINCEPT.ORANGE,
                        color: FINCEPT.DARK_BG,
                        border: 'none',
                        fontSize: '9px',
                        fontWeight: 700,
                        cursor: 'pointer',
                      }}
                    >
                      TRY AGAIN
                    </button>
                  )}
                </div>
              </div>
            ) : data && data.length > 0 ? (
              viewMode === 'table' ? (
                <div style={{ width: '100%', height: '100%', overflow: 'auto' }}>
                  <table style={{ minWidth: '100%', fontSize: '10px', borderCollapse: 'collapse' }}>
                    <thead style={{ position: 'sticky', top: 0, zIndex: 10 }}>
                      <tr style={{ backgroundColor: FINCEPT.HEADER_BG }}>
                        {columns.map(col => (
                          <th
                            key={col}
                            onClick={() => handleSortClick(col)}
                            style={{
                              padding: '10px 16px',
                              textAlign: 'left',
                              fontWeight: 700,
                              fontSize: '9px',
                              letterSpacing: '0.5px',
                              color: FINCEPT.GRAY,
                              borderBottom: `1px solid ${FINCEPT.BORDER}`,
                              minWidth: '120px',
                              whiteSpace: 'nowrap',
                              cursor: 'pointer',
                            }}
                          >
                            <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                              <span title={col}>{col.toUpperCase()}</span>
                              {sortColumn === col && <ArrowUpDown size={10} color={FINCEPT.ORANGE} />}
                            </div>
                          </th>
                        ))}
                      </tr>
                    </thead>
                    <tbody>
                      {(sortedData || []).map((row, idx) => (
                        <tr key={idx} className="akshare-hover-row" style={{ borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
                          {columns.map(col => {
                            const value = row[col];
                            const isNumber = typeof value === 'number';
                            const isNegative = isNumber && value < 0;
                            return (
                              <td
                                key={col}
                                style={{
                                  padding: '8px 16px',
                                  whiteSpace: 'normal',
                                  color: isNegative ? FINCEPT.RED : isNumber ? FINCEPT.CYAN : FINCEPT.WHITE,
                                  maxWidth: '400px',
                                  fontSize: '10px',
                                }}
                              >
                                <div style={{ wordBreak: 'break-word' }} title={String(value)}>
                                  {isNumber ? value.toLocaleString() : String(value ?? '-')}
                                </div>
                              </td>
                            );
                          })}
                        </tr>
                      ))}
                    </tbody>
                  </table>

                  {/* Pagination Controls */}
                  {data && data.length > 0 && (
                    <div style={{
                      position: 'sticky',
                      bottom: 0,
                      display: 'flex',
                      alignItems: 'center',
                      justifyContent: 'space-between',
                      padding: '10px 16px',
                      borderTop: `1px solid ${FINCEPT.BORDER}`,
                      backgroundColor: FINCEPT.HEADER_BG,
                    }}>
                      <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
                        <span style={{ fontSize: '9px', color: FINCEPT.MUTED }}>
                          Showing {((currentPage - 1) * pageSize) + 1} to {Math.min(currentPage * pageSize, data.length)} of {data.length} rows
                        </span>
                        <select
                          value={pageSize}
                          onChange={(e) => dispatch({ type: 'SET_PAGE_SIZE', payload: Number(e.target.value) })}
                          style={{ padding: '4px 8px', borderRadius: '2px', fontSize: '9px', backgroundColor: FINCEPT.DARK_BG, border: `1px solid ${FINCEPT.BORDER}`, color: FINCEPT.WHITE }}
                        >
                          <option value={10}>10 rows</option>
                          <option value={25}>25 rows</option>
                          <option value={50}>50 rows</option>
                          <option value={100}>100 rows</option>
                        </select>
                      </div>
                      <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                        {[
                          { label: 'FIRST', page: 1, disabled: currentPage === 1 },
                          { label: 'PREV', page: currentPage - 1, disabled: currentPage === 1 },
                        ].map(btn => (
                          <button key={btn.label} onClick={() => dispatch({ type: 'SET_PAGE', payload: btn.page })} disabled={btn.disabled} style={{
                            padding: '4px 10px', borderRadius: '2px', fontSize: '9px', fontWeight: 700,
                            backgroundColor: btn.disabled ? FINCEPT.MUTED : 'transparent',
                            color: btn.disabled ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                            border: `1px solid ${btn.disabled ? FINCEPT.MUTED : FINCEPT.BORDER}`,
                            opacity: btn.disabled ? 0.5 : 1,
                            cursor: btn.disabled ? 'not-allowed' : 'pointer',
                          }}>
                            {btn.label}
                          </button>
                        ))}
                        <span style={{ fontSize: '9px', padding: '0 12px', color: FINCEPT.WHITE }}>
                          Page <span style={{ color: FINCEPT.CYAN }}>{currentPage}</span> of {totalPages}
                        </span>
                        {[
                          { label: 'NEXT', page: currentPage + 1, disabled: currentPage === totalPages },
                          { label: 'LAST', page: totalPages, disabled: currentPage === totalPages },
                        ].map(btn => (
                          <button key={btn.label} onClick={() => dispatch({ type: 'SET_PAGE', payload: btn.page })} disabled={btn.disabled} style={{
                            padding: '4px 10px', borderRadius: '2px', fontSize: '9px', fontWeight: 700,
                            backgroundColor: btn.disabled ? FINCEPT.MUTED : 'transparent',
                            color: btn.disabled ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                            border: `1px solid ${btn.disabled ? FINCEPT.MUTED : FINCEPT.BORDER}`,
                            opacity: btn.disabled ? 0.5 : 1,
                            cursor: btn.disabled ? 'not-allowed' : 'pointer',
                          }}>
                            {btn.label}
                          </button>
                        ))}
                      </div>
                    </div>
                  )}
                </div>
              ) : (
                <div style={{ display: 'flex', flexDirection: 'column', height: '100%' }}>
                  <pre style={{ flex: 1, fontSize: '10px', fontFamily: '"IBM Plex Mono", monospace', padding: '16px', borderRadius: '2px', overflow: 'auto', backgroundColor: FINCEPT.HEADER_BG, color: FINCEPT.WHITE }}>
                    {JSON.stringify(paginatedData, null, 2)}
                  </pre>
                  {data && data.length > 0 && (
                    <div style={{
                      display: 'flex',
                      alignItems: 'center',
                      justifyContent: 'space-between',
                      padding: '10px 16px',
                      borderTop: `1px solid ${FINCEPT.BORDER}`,
                      backgroundColor: FINCEPT.HEADER_BG,
                    }}>
                      <span style={{ fontSize: '9px', color: FINCEPT.MUTED }}>
                        Showing page {currentPage} of {totalPages}
                      </span>
                      <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                        <button onClick={() => dispatch({ type: 'SET_PAGE', payload: Math.max(1, currentPage - 1) })} disabled={currentPage === 1} style={{
                          padding: '4px 12px', borderRadius: '2px', fontSize: '9px', fontWeight: 700,
                          backgroundColor: currentPage === 1 ? FINCEPT.MUTED : 'transparent',
                          color: currentPage === 1 ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                          border: `1px solid ${currentPage === 1 ? FINCEPT.MUTED : FINCEPT.BORDER}`,
                          opacity: currentPage === 1 ? 0.5 : 1,
                          cursor: currentPage === 1 ? 'not-allowed' : 'pointer',
                        }}>PREV</button>
                        <button onClick={() => dispatch({ type: 'SET_PAGE', payload: Math.min(totalPages, currentPage + 1) })} disabled={currentPage === totalPages} style={{
                          padding: '4px 12px', borderRadius: '2px', fontSize: '9px', fontWeight: 700,
                          backgroundColor: currentPage === totalPages ? FINCEPT.MUTED : 'transparent',
                          color: currentPage === totalPages ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                          border: `1px solid ${currentPage === totalPages ? FINCEPT.MUTED : FINCEPT.BORDER}`,
                          opacity: currentPage === totalPages ? 0.5 : 1,
                          cursor: currentPage === totalPages ? 'not-allowed' : 'pointer',
                        }}>NEXT</button>
                      </div>
                    </div>
                  )}
                </div>
              )
            ) : dataStatus === 'idle' && !selectedEndpoint ? (
              <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', height: '100%' }}>
                <div style={{ textAlign: 'center', maxWidth: '500px' }}>
                  <Zap size={40} color={FINCEPT.ORANGE} style={{ marginBottom: '16px' }} />
                  <div style={{ fontSize: '14px', fontWeight: 700, color: FINCEPT.WHITE, marginBottom: '8px' }}>
                    AKSHARE DATA EXPLORER
                  </div>
                  <div style={{ fontSize: '10px', marginBottom: '24px', color: FINCEPT.MUTED }}>
                    Access 1000+ financial data endpoints across 26 data sources covering bonds, derivatives, economics, stocks, funds, and alternative data from Chinese and global markets.
                  </div>
                  <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '12px' }}>
                    {AKSHARE_DATA_SOURCES.slice(0, 6).map(source => (
                      <button
                        key={source.id}
                        onClick={() => dispatch({ type: 'SELECT_SOURCE', payload: source })}
                        style={{ padding: '12px', borderRadius: '2px', textAlign: 'left', backgroundColor: 'transparent', border: `1px solid ${FINCEPT.BORDER}`, cursor: 'pointer', transition: 'all 0.2s' }}
                      >
                        <div style={{ fontWeight: 600, color: FINCEPT.WHITE, fontSize: '10px' }}>{source.name}</div>
                        <div style={{ color: FINCEPT.MUTED, fontSize: '9px', marginTop: '4px' }}>{source.categories.length} categories</div>
                      </button>
                    ))}
                  </div>
                </div>
              </div>
            ) : (
              <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', height: '100%' }}>
                <div style={{ textAlign: 'center' }}>
                  <CheckCircle2 size={28} color={FINCEPT.GREEN} style={{ marginBottom: '12px' }} />
                  <div style={{ fontSize: '10px', color: FINCEPT.MUTED }}>No data returned</div>
                </div>
              </div>
            )}
          </div>
        </div>

        {/* Favorites Panel */}
        {showFavorites && (
          <div style={{
            width: '280px',
            borderLeft: `1px solid ${FINCEPT.BORDER}`,
            display: 'flex',
            flexDirection: 'column',
            backgroundColor: FINCEPT.PANEL_BG,
          }}>
            <div style={{
              padding: '12px',
              backgroundColor: FINCEPT.HEADER_BG,
              borderBottom: `1px solid ${FINCEPT.BORDER}`,
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'space-between',
            }}>
              <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.YELLOW, letterSpacing: '0.5px' }}>FAVORITES</span>
              <button onClick={() => dispatch({ type: 'TOGGLE_FAVORITES' })} style={{ padding: '4px', backgroundColor: 'transparent', border: 'none', cursor: 'pointer' }}>
                <X size={14} color={FINCEPT.MUTED} />
              </button>
            </div>
            <div className="akshare-scrollbar" style={{ flex: 1, overflowY: 'auto' }}>
              {favorites.length === 0 ? (
                <div style={{ padding: '16px', textAlign: 'center', fontSize: '10px', color: FINCEPT.MUTED }}>
                  No favorites yet. Click the bookmark icon on any endpoint to add it here.
                </div>
              ) : (
                favorites.map((fav, idx) => (
                  <button
                    key={idx}
                    onClick={() => {
                      const source = AKSHARE_DATA_SOURCES.find(s => s.script === fav.script);
                      if (source) {
                        dispatch({ type: 'SELECT_SOURCE', payload: source });
                        executeQuery(fav.script, fav.endpoint);
                      }
                    }}
                    style={{
                      width: '100%',
                      padding: '10px 12px',
                      textAlign: 'left',
                      fontSize: '10px',
                      backgroundColor: 'transparent',
                      border: 'none',
                      borderBottom: `1px solid ${FINCEPT.BORDER}`,
                      cursor: 'pointer',
                      transition: 'all 0.2s',
                    }}
                  >
                    <div style={{ color: FINCEPT.WHITE, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>{fav.endpoint}</div>
                    <div style={{ color: FINCEPT.MUTED, fontSize: '9px', marginTop: '2px' }}>{fav.script.replace('akshare_', '').replace('.py', '')}</div>
                  </button>
                ))
              )}
            </div>
          </div>
        )}

        {/* History Panel */}
        {showHistory && (
          <div style={{
            width: '280px',
            borderLeft: `1px solid ${FINCEPT.BORDER}`,
            display: 'flex',
            flexDirection: 'column',
            backgroundColor: FINCEPT.PANEL_BG,
          }}>
            <div style={{
              padding: '12px',
              backgroundColor: FINCEPT.HEADER_BG,
              borderBottom: `1px solid ${FINCEPT.BORDER}`,
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'space-between',
            }}>
              <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.CYAN, letterSpacing: '0.5px' }}>QUERY HISTORY</span>
              <button onClick={() => dispatch({ type: 'TOGGLE_HISTORY' })} style={{ padding: '4px', backgroundColor: 'transparent', border: 'none', cursor: 'pointer' }}>
                <X size={14} color={FINCEPT.MUTED} />
              </button>
            </div>
            <div className="akshare-scrollbar" style={{ flex: 1, overflowY: 'auto' }}>
              {queryHistory.length === 0 ? (
                <div style={{ padding: '16px', textAlign: 'center', fontSize: '10px', color: FINCEPT.MUTED }}>No queries yet</div>
              ) : (
                queryHistory.map((item) => (
                  <button
                    key={item.id}
                    onClick={() => {
                      const source = AKSHARE_DATA_SOURCES.find(s => s.script === item.script);
                      if (source) {
                        dispatch({ type: 'SELECT_SOURCE', payload: source });
                        executeQuery(item.script, item.endpoint);
                      }
                    }}
                    style={{
                      width: '100%',
                      padding: '10px 12px',
                      textAlign: 'left',
                      fontSize: '10px',
                      backgroundColor: 'transparent',
                      border: 'none',
                      borderBottom: `1px solid ${FINCEPT.BORDER}`,
                      cursor: 'pointer',
                      transition: 'all 0.2s',
                    }}
                  >
                    <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                      {item.success ? <CheckCircle2 size={10} color={FINCEPT.GREEN} /> : <AlertCircle size={10} color={FINCEPT.RED} />}
                      <span style={{ color: FINCEPT.WHITE, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>{item.endpoint}</span>
                    </div>
                    <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginTop: '4px', color: FINCEPT.MUTED, fontSize: '9px' }}>
                      <span>{new Date(item.timestamp).toLocaleTimeString()}</span>
                      {item.count !== undefined && <span>{item.count} rows</span>}
                    </div>
                  </button>
                ))
              )}
            </div>
          </div>
        )}
      </div>

      {/* Status Bar */}
      <div style={{
        backgroundColor: FINCEPT.HEADER_BG,
        borderTop: `1px solid ${FINCEPT.BORDER}`,
        padding: '4px 16px',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        fontSize: '9px',
        color: FINCEPT.GRAY,
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
          <span>MODE: <span style={{ color: FINCEPT.ORANGE }}>DATA EXPLORER</span></span>
          <span>SOURCE: <span style={{ color: selectedSource ? FINCEPT.CYAN : FINCEPT.MUTED }}>{selectedSource?.name || 'NONE'}</span></span>
          <span>ENDPOINT: <span style={{ color: selectedEndpoint ? FINCEPT.CYAN : FINCEPT.MUTED }}>{selectedEndpoint || 'NONE'}</span></span>
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
          <span>FAVORITES: <span style={{ color: FINCEPT.YELLOW }}>{favorites.length}</span></span>
          <span>HISTORY: <span style={{ color: FINCEPT.CYAN }}>{queryHistory.length}</span></span>
          {data && <span>RECORDS: <span style={{ color: FINCEPT.GREEN }}>{data.length}</span></span>}
        </div>
      </div>
    </div>
  );
};

// Point 5: ErrorBoundary wrapper
const AkShareDataTab: React.FC = () => (
  <ErrorBoundary name="AkShareDataTab" variant="default">
    <AkShareDataTabInner />
  </ErrorBoundary>
);

export default memo(AkShareDataTab);
