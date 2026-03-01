/**
 * AkShare Data Tab — State Types & Reducer
 *
 * useReducer state, actions, and reducer logic extracted from AkShareDataTab.tsx.
 */

import type { AKShareDataSource } from '@/services/akshareApi';

// ============================================================================
// Types
// ============================================================================

export type AsyncStatus = 'idle' | 'loading' | 'success' | 'error';

export interface QueryHistoryItem {
  id: string;
  script: string;
  endpoint: string;
  timestamp: number;
  success: boolean;
  count?: number;
}

export interface FavoriteEndpoint {
  script: string;
  endpoint: string;
  name: string;
}

export interface AkShareState {
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

export type AkShareAction =
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

export const initialState: AkShareState = {
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

export function akshareReducer(state: AkShareState, action: AkShareAction): AkShareState {
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
