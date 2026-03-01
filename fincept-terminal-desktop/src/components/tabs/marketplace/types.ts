// Design System Colors
export const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  BLUE: '#0088FF',
  PURPLE: '#9D4EDD',
};

export const FONT_FAMILY = '"IBM Plex Mono", "Consolas", monospace';

export const API_TIMEOUT_MS = 30000;

export type ScreenType = 'BROWSE' | 'UPLOAD' | 'MY_PURCHASES' | 'MY_DATASETS' | 'ANALYTICS' | 'PRICING' | 'ADMIN_STATS' | 'ADMIN_PENDING' | 'ADMIN_REVENUE';

// ══════════════════════════════════════════════════════════════
// STATE MACHINE & TYPES
// ══════════════════════════════════════════════════════════════
export type MarketplaceStatus = 'idle' | 'loading' | 'ready' | 'error';

export interface MarketplaceState {
  status: MarketplaceStatus;
  currentScreen: ScreenType;
  datasets: any[];
  myUploadedDatasets: any[];
  selectedDataset: any | null;
  selectedCategory: string;
  searchQuery: string;
  currentPage: number;
  totalPages: number;
  error: string | null;
}

export type MarketplaceAction =
  | { type: 'SET_LOADING' }
  | { type: 'SET_READY' }
  | { type: 'SET_ERROR'; error: string }
  | { type: 'SET_SCREEN'; screen: ScreenType }
  | { type: 'SET_DATASETS'; datasets: any[]; totalPages: number }
  | { type: 'SET_MY_DATASETS'; datasets: any[] }
  | { type: 'SET_SELECTED_DATASET'; dataset: any | null }
  | { type: 'SET_CATEGORY'; category: string }
  | { type: 'SET_SEARCH'; query: string }
  | { type: 'SET_PAGE'; page: number }
  | { type: 'RESET_ERROR' };

export const initialState: MarketplaceState = {
  status: 'idle',
  currentScreen: 'BROWSE',
  datasets: [],
  myUploadedDatasets: [],
  selectedDataset: null,
  selectedCategory: 'ALL',
  searchQuery: '',
  currentPage: 1,
  totalPages: 1,
  error: null
};

export function marketplaceReducer(state: MarketplaceState, action: MarketplaceAction): MarketplaceState {
  switch (action.type) {
    case 'SET_LOADING':
      return { ...state, status: 'loading', error: null };
    case 'SET_READY':
      return { ...state, status: 'ready', error: null };
    case 'SET_ERROR':
      return { ...state, status: 'error', error: action.error };
    case 'SET_SCREEN':
      return { ...state, currentScreen: action.screen };
    case 'SET_DATASETS':
      return { ...state, datasets: action.datasets, totalPages: action.totalPages };
    case 'SET_MY_DATASETS':
      return { ...state, myUploadedDatasets: action.datasets };
    case 'SET_SELECTED_DATASET':
      return { ...state, selectedDataset: action.dataset };
    case 'SET_CATEGORY':
      return { ...state, selectedCategory: action.category, currentPage: 1 };
    case 'SET_SEARCH':
      return { ...state, searchQuery: action.query, currentPage: 1 };
    case 'SET_PAGE':
      return { ...state, currentPage: action.page };
    case 'RESET_ERROR':
      return { ...state, error: null };
    default:
      return state;
  }
}
