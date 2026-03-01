/**
 * MCP Tab - Types, State Machine, and Constants
 */

import { MCPServerWithStats } from '../../../services/mcp/mcpManager';

// ============================================================================
// Types & State Machine
// ============================================================================

export interface MCPTabProps {
  onNavigateToTab?: (tabName: string) => void;
}

// Explicit state machine states
export type MCPStatus = 'idle' | 'loading' | 'success' | 'error';

export interface MCPState {
  status: MCPStatus;
  servers: MCPServerWithStats[];
  tools: any[];
  statusMessage: string;
  error: string | null;
  view: 'marketplace' | 'installed' | 'tools';
  isAddModalOpen: boolean;
  isRefreshing: boolean;
  searchTerm: string;
  selectedServerId: string | null;
}

export type MCPAction =
  | { type: 'SET_VIEW'; payload: 'marketplace' | 'installed' | 'tools' }
  | { type: 'LOAD_START' }
  | { type: 'LOAD_SUCCESS'; payload: { servers: MCPServerWithStats[]; tools: any[] } }
  | { type: 'LOAD_ERROR'; payload: string }
  | { type: 'SET_STATUS_MESSAGE'; payload: string }
  | { type: 'SET_REFRESHING'; payload: boolean }
  | { type: 'SET_SEARCH_TERM'; payload: string }
  | { type: 'SET_SELECTED_SERVER'; payload: string | null }
  | { type: 'OPEN_ADD_MODAL' }
  | { type: 'CLOSE_ADD_MODAL' }
  | { type: 'RESET_ERROR' };

export const initialState: MCPState = {
  status: 'idle',
  servers: [],
  tools: [],
  statusMessage: 'Initializing...',
  error: null,
  view: 'marketplace',
  isAddModalOpen: false,
  isRefreshing: false,
  searchTerm: '',
  selectedServerId: null,
};

export function mcpReducer(state: MCPState, action: MCPAction): MCPState {
  switch (action.type) {
    case 'SET_VIEW':
      return { ...state, view: action.payload };
    case 'LOAD_START':
      return { ...state, status: 'loading', error: null };
    case 'LOAD_SUCCESS': {
      const { servers, tools } = action.payload;
      const runningCount = servers.filter(s => s.status === 'running').length;
      const internalToolsCount = tools.filter((t: any) => t.serverId === 'fincept-terminal').length;
      const externalToolsCount = tools.length - internalToolsCount;
      return {
        ...state,
        status: 'success',
        servers,
        tools,
        statusMessage: `${runningCount} servers running | ${internalToolsCount} internal + ${externalToolsCount} external tools`,
        error: null,
      };
    }
    case 'LOAD_ERROR':
      return { ...state, status: 'error', error: action.payload, statusMessage: action.payload };
    case 'SET_STATUS_MESSAGE':
      return { ...state, statusMessage: action.payload };
    case 'SET_REFRESHING':
      return { ...state, isRefreshing: action.payload };
    case 'SET_SEARCH_TERM':
      return { ...state, searchTerm: action.payload };
    case 'SET_SELECTED_SERVER':
      return { ...state, selectedServerId: action.payload };
    case 'OPEN_ADD_MODAL':
      return { ...state, isAddModalOpen: true };
    case 'CLOSE_ADD_MODAL':
      return { ...state, isAddModalOpen: false };
    case 'RESET_ERROR':
      return { ...state, status: 'idle', error: null };
    default:
      return state;
  }
}

// ============================================================================
// Constants
// ============================================================================

export const TIMEOUT_MS = 30000; // 30 second timeout for operations
export const POLL_INTERVAL_MS = 5000; // 5 second polling interval

// Fincept Design System Colors
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
