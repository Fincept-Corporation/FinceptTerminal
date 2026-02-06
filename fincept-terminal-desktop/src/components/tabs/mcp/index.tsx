// MCP Integrations Tab - Main Component
// Three-panel terminal layout per Fincept UI Design System
// Production-ready: State machine, AbortController, deduplication, error boundary, timeouts, validation

import React, { useReducer, useEffect, useCallback, useRef } from 'react';
import { Plus, RefreshCw, Search, Play, Square, Trash2, Settings, Zap, CheckCircle, XCircle, AlertCircle, MessageSquare, Server, Wrench } from 'lucide-react';
import { mcpManager, MCPServerWithStats } from '../../../services/mcp/mcpManager';
import { sqliteService } from '../../../services/core/sqliteService';
import { useBrokerContext } from '../../../contexts/BrokerContext';
import { useStockBrokerContextOptional } from '../../../contexts/StockBrokerContext';
import { brokerMCPBridge } from '../../../services/mcp/internal/BrokerMCPBridge';
import { stockBrokerMCPBridge } from '../../../services/mcp/internal/StockBrokerMCPBridge';
import MCPMarketplace from './marketplace/MCPMarketplace';
import MCPAddServerModal from './MCPAddServerModal';
import MCPToolsManagement from './MCPToolsManagement';
import { showConfirm } from '@/utils/notifications';
import { ErrorBoundary } from '@/components/common/ErrorBoundary';
import { withTimeout, deduplicatedFetch } from '@/services/core/apiUtils';

// ============================================================================
// Types & State Machine
// ============================================================================

interface MCPTabProps {
  onNavigateToTab?: (tabName: string) => void;
}

// Explicit state machine states
type MCPStatus = 'idle' | 'loading' | 'success' | 'error';

interface MCPState {
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

type MCPAction =
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

const initialState: MCPState = {
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

function mcpReducer(state: MCPState, action: MCPAction): MCPState {
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

const TIMEOUT_MS = 30000; // 30 second timeout for operations
const POLL_INTERVAL_MS = 5000; // 5 second polling interval

// Fincept Design System Colors
const FINCEPT = {
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

const FONT_FAMILY = '"IBM Plex Mono", "Consolas", monospace';

// ============================================================================
// Main Component
// ============================================================================

const MCPTabInner: React.FC<MCPTabProps> = ({ onNavigateToTab }) => {
  const [state, dispatch] = useReducer(mcpReducer, initialState);

  // Refs for cleanup and deduplication
  const abortControllerRef = useRef<AbortController | null>(null);
  const mountedRef = useRef(true);
  const pendingOperationsRef = useRef<Set<string>>(new Set());

  // Crypto broker context for MCP bridge
  const { activeAdapter, tradingMode, activeBroker } = useBrokerContext();

  // Stock broker context for MCP bridge (optional)
  const stockBrokerCtx = useStockBrokerContextOptional();
  const stockAdapter = stockBrokerCtx?.adapter;
  const stockTradingMode = stockBrokerCtx?.tradingMode;
  const stockActiveBroker = stockBrokerCtx?.activeBroker;
  const stockIsAuthenticated = stockBrokerCtx?.isAuthenticated;

  // ============================================================================
  // Helper: Check if mounted before state update
  // ============================================================================
  const safeDispatch = useCallback((action: MCPAction) => {
    if (mountedRef.current) {
      dispatch(action);
    }
  }, []);

  // ============================================================================
  // Helper: Check if operation is already pending (deduplication)
  // ============================================================================
  const isOperationPending = useCallback((opKey: string): boolean => {
    return pendingOperationsRef.current.has(opKey);
  }, []);

  const markOperationStart = useCallback((opKey: string) => {
    pendingOperationsRef.current.add(opKey);
  }, []);

  const markOperationEnd = useCallback((opKey: string) => {
    pendingOperationsRef.current.delete(opKey);
  }, []);

  // ============================================================================
  // Load Data with deduplication, timeout, and abort support
  // ============================================================================
  const loadData = useCallback(async (silent = false) => {
    const opKey = 'loadData';

    // Deduplication: Skip if already loading
    if (isOperationPending(opKey)) {
      return;
    }

    // Check if aborted
    if (abortControllerRef.current?.signal.aborted) {
      return;
    }

    markOperationStart(opKey);

    if (!silent) {
      safeDispatch({ type: 'LOAD_START' });
      safeDispatch({ type: 'SET_STATUS_MESSAGE', payload: 'Loading servers...' });
    }

    try {
      // Use deduplicatedFetch and withTimeout for external calls
      const [serversData, toolsData] = await Promise.all([
        deduplicatedFetch('mcp:servers', () =>
          withTimeout(mcpManager.getServersWithStats(), TIMEOUT_MS, 'Server list timeout')
        ),
        deduplicatedFetch('mcp:tools', async () => {
          const { mcpToolService } = await import('../../../services/mcp/mcpToolService');
          return withTimeout(mcpToolService.getAllTools(), TIMEOUT_MS, 'Tools list timeout');
        }),
      ]);

      // Check abort after async operations
      if (abortControllerRef.current?.signal.aborted || !mountedRef.current) {
        return;
      }

      safeDispatch({ type: 'LOAD_SUCCESS', payload: { servers: serversData, tools: toolsData } });
    } catch (error) {
      // Don't report errors if aborted
      if (abortControllerRef.current?.signal.aborted || !mountedRef.current) {
        return;
      }

      const errorMessage = error instanceof Error ? error.message : 'Failed to load MCP data';
      console.error('Failed to load MCP data:', error);
      safeDispatch({ type: 'LOAD_ERROR', payload: errorMessage });
    } finally {
      markOperationEnd(opKey);
    }
  }, [isOperationPending, markOperationStart, markOperationEnd, safeDispatch]);

  // ============================================================================
  // Broker Bridge Effects
  // ============================================================================
  useEffect(() => {
    if (activeAdapter) {
      brokerMCPBridge.connect({
        activeAdapter,
        tradingMode,
        activeBroker,
      });
      console.log(`[MCP] Connected to crypto broker: ${activeBroker} (${tradingMode} mode)`);
    }

    return () => {
      brokerMCPBridge.disconnect();
    };
  }, [activeAdapter, tradingMode, activeBroker]);

  useEffect(() => {
    if (stockAdapter && stockIsAuthenticated && stockActiveBroker) {
      stockBrokerMCPBridge.connect({
        activeAdapter: stockAdapter,
        tradingMode: stockTradingMode || 'paper',
        activeBroker: stockActiveBroker,
        isAuthenticated: stockIsAuthenticated,
      });
      console.log(`[MCP] Connected to stock broker: ${stockActiveBroker} (${stockTradingMode} mode)`);
    }

    return () => {
      stockBrokerMCPBridge.disconnect();
    };
  }, [stockAdapter, stockIsAuthenticated, stockActiveBroker, stockTradingMode]);

  // ============================================================================
  // Initialization & Cleanup Effect
  // ============================================================================
  useEffect(() => {
    mountedRef.current = true;
    abortControllerRef.current = new AbortController();

    const initializeServers = async () => {
      try {
        await withTimeout(sqliteService.initialize(), TIMEOUT_MS, 'Database init timeout');
        const { mcpToolService } = await import('../../../services/mcp/mcpToolService');
        await withTimeout(mcpToolService.initialize(), TIMEOUT_MS, 'MCP Tool Service init timeout');
        await loadData();
      } catch (error) {
        if (!abortControllerRef.current?.signal.aborted && mountedRef.current) {
          console.error('Failed to initialize MCP tab:', error);
          safeDispatch({ type: 'LOAD_ERROR', payload: 'Initialization failed' });
        }
      }
    };

    initializeServers();

    // Polling interval with abort check
    const interval = setInterval(() => {
      if (!abortControllerRef.current?.signal.aborted && mountedRef.current) {
        loadData(true);
      }
    }, POLL_INTERVAL_MS);

    // Cleanup function
    return () => {
      mountedRef.current = false;
      abortControllerRef.current?.abort();
      clearInterval(interval);
      pendingOperationsRef.current.clear();
    };
  }, [loadData, safeDispatch]);

  // ============================================================================
  // Handlers with timeout and deduplication
  // ============================================================================
  const handleRefresh = useCallback(async () => {
    safeDispatch({ type: 'SET_REFRESHING', payload: true });
    await loadData();
    setTimeout(() => safeDispatch({ type: 'SET_REFRESHING', payload: false }), 500);
  }, [loadData, safeDispatch]);

  const handleInstallServer = useCallback(async (
    config: any,
    callbacks?: {
      onProgress?: (status: string) => void;
      onComplete?: (success: boolean, error?: string) => void;
    }
  ) => {
    const opKey = `install:${config.id}`;
    if (isOperationPending(opKey)) {
      return;
    }

    markOperationStart(opKey);
    safeDispatch({ type: 'SET_STATUS_MESSAGE', payload: 'Installing server...' });

    try {
      // Pass callbacks to mcpManager for non-blocking installation
      // The installServer call returns quickly after saving to DB
      // Background server startup continues and calls the callbacks
      await mcpManager.installServer(config, {
        onProgress: (status) => {
          safeDispatch({ type: 'SET_STATUS_MESSAGE', payload: status });
          callbacks?.onProgress?.(status);
        },
        onComplete: async (success, error) => {
          markOperationEnd(opKey);

          if (success) {
            safeDispatch({ type: 'SET_STATUS_MESSAGE', payload: 'Server installed and started successfully' });
            safeDispatch({ type: 'SET_VIEW', payload: 'installed' });
          } else {
            // Server saved but failed to start - still show in installed list
            safeDispatch({ type: 'SET_STATUS_MESSAGE', payload: error ? `Server installed but start failed: ${error}` : 'Server installed (not running)' });
            safeDispatch({ type: 'SET_VIEW', payload: 'installed' });
          }

          // Refresh data to show updated server list
          await loadData();
          callbacks?.onComplete?.(success, error);
        }
      });

      // Refresh immediately to show server in list (even before it's fully started)
      await loadData();

    } catch (error) {
      const errorMessage = error instanceof Error ? error.message : 'Unknown error';
      console.error('Failed to install server:', error);
      safeDispatch({ type: 'SET_STATUS_MESSAGE', payload: `Installation failed: ${errorMessage}` });
      markOperationEnd(opKey);
      callbacks?.onComplete?.(false, errorMessage);
    }
  }, [isOperationPending, markOperationStart, markOperationEnd, loadData, safeDispatch]);

  const handleStartServer = useCallback(async (serverId: string) => {
    const opKey = `start:${serverId}`;
    if (isOperationPending(opKey)) {
      return;
    }

    markOperationStart(opKey);
    safeDispatch({ type: 'SET_STATUS_MESSAGE', payload: 'Starting server...' });

    try {
      await withTimeout(mcpManager.startServer(serverId), TIMEOUT_MS, 'Start server timeout');
      await loadData();
    } catch (error) {
      const errorMessage = error instanceof Error ? error.message : 'Unknown error';
      console.error('Failed to start server:', error);
      safeDispatch({ type: 'SET_STATUS_MESSAGE', payload: `Start failed: ${errorMessage}` });
    } finally {
      markOperationEnd(opKey);
    }
  }, [isOperationPending, markOperationStart, markOperationEnd, loadData, safeDispatch]);

  const handleStopServer = useCallback(async (serverId: string) => {
    const opKey = `stop:${serverId}`;
    if (isOperationPending(opKey)) {
      return;
    }

    markOperationStart(opKey);
    safeDispatch({ type: 'SET_STATUS_MESSAGE', payload: 'Stopping server...' });

    try {
      await withTimeout(mcpManager.stopServer(serverId), TIMEOUT_MS, 'Stop server timeout');
      await loadData();
    } catch (error) {
      const errorMessage = error instanceof Error ? error.message : 'Unknown error';
      console.error('Failed to stop server:', error);
      safeDispatch({ type: 'SET_STATUS_MESSAGE', payload: `Stop failed: ${errorMessage}` });
    } finally {
      markOperationEnd(opKey);
    }
  }, [isOperationPending, markOperationStart, markOperationEnd, loadData, safeDispatch]);

  const handleRemoveServer = useCallback(async (serverId: string) => {
    const confirmed = await showConfirm('This will delete all associated data.', {
      title: 'Remove this MCP server?',
      type: 'danger'
    });
    if (!confirmed) {
      return;
    }

    const opKey = `remove:${serverId}`;
    if (isOperationPending(opKey)) {
      return;
    }

    markOperationStart(opKey);
    safeDispatch({ type: 'SET_STATUS_MESSAGE', payload: 'Removing server...' });

    try {
      await withTimeout(mcpManager.removeServer(serverId), TIMEOUT_MS, 'Remove server timeout');
      if (state.selectedServerId === serverId) {
        safeDispatch({ type: 'SET_SELECTED_SERVER', payload: null });
      }
      await loadData();
      safeDispatch({ type: 'SET_STATUS_MESSAGE', payload: 'Server removed' });
    } catch (error) {
      const errorMessage = error instanceof Error ? error.message : 'Unknown error';
      console.error('Failed to remove server:', error);
      safeDispatch({ type: 'SET_STATUS_MESSAGE', payload: `Remove failed: ${errorMessage}` });
    } finally {
      markOperationEnd(opKey);
    }
  }, [isOperationPending, markOperationStart, markOperationEnd, state.selectedServerId, loadData, safeDispatch]);

  const handleToggleAutoStart = useCallback(async (serverId: string, enabled: boolean) => {
    const opKey = `autostart:${serverId}`;
    if (isOperationPending(opKey)) {
      return;
    }

    markOperationStart(opKey);

    try {
      await withTimeout(
        mcpManager.updateServerConfig(serverId, { auto_start: enabled }),
        TIMEOUT_MS,
        'Update config timeout'
      );
      await loadData();
      safeDispatch({ type: 'SET_STATUS_MESSAGE', payload: enabled ? 'Auto-start enabled' : 'Auto-start disabled' });
    } catch (error) {
      const errorMessage = error instanceof Error ? error.message : 'Unknown error';
      console.error('Failed to toggle auto-start:', error);
      safeDispatch({ type: 'SET_STATUS_MESSAGE', payload: `Auto-start toggle failed: ${errorMessage}` });
    } finally {
      markOperationEnd(opKey);
    }
  }, [isOperationPending, markOperationStart, markOperationEnd, loadData, safeDispatch]);

  // ============================================================================
  // Computed Values
  // ============================================================================
  const getStatusIcon = (status: string) => {
    switch (status) {
      case 'running':
        return <CheckCircle size={10} color={FINCEPT.GREEN} />;
      case 'error':
        return <XCircle size={10} color={FINCEPT.RED} />;
      default:
        return <AlertCircle size={10} color={FINCEPT.GRAY} />;
    }
  };

  const getStatusColor = (status: string) => {
    switch (status) {
      case 'running': return FINCEPT.GREEN;
      case 'error': return FINCEPT.RED;
      default: return FINCEPT.GRAY;
    }
  };

  const runningServers = state.servers.filter(s => s.status === 'running').length;
  const filteredServers = state.servers.filter(s =>
    s.name.toLowerCase().includes(state.searchTerm.toLowerCase()) ||
    s.description.toLowerCase().includes(state.searchTerm.toLowerCase())
  );

  const selectedServer = state.selectedServerId
    ? state.servers.find(s => s.id === state.selectedServerId) || null
    : null;

  const internalToolCount = state.tools.filter((t: any) => t.serverId === 'fincept-terminal').length;
  const externalToolCount = state.tools.length - internalToolCount;

  const selectedServerTools = state.selectedServerId === 'internal'
    ? state.tools.filter((t: any) => t.serverId === 'fincept-terminal')
    : selectedServer
      ? state.tools.filter((t: any) => t.serverId === selectedServer.id)
      : [];

  // ============================================================================
  // Render
  // ============================================================================
  return (
    <div style={{
      width: '100%',
      height: '100%',
      backgroundColor: FINCEPT.DARK_BG,
      color: FINCEPT.WHITE,
      fontFamily: FONT_FAMILY,
      display: 'flex',
      flexDirection: 'column',
      fontSize: '11px',
      overflow: 'hidden',
    }}>
      <style>{`
        .mcp-scroll::-webkit-scrollbar { width: 6px; height: 6px; }
        .mcp-scroll::-webkit-scrollbar-track { background: ${FINCEPT.DARK_BG}; }
        .mcp-scroll::-webkit-scrollbar-thumb { background: ${FINCEPT.BORDER}; border-radius: 3px; }
        .mcp-scroll::-webkit-scrollbar-thumb:hover { background: ${FINCEPT.MUTED}; }
        @keyframes spin {
          from { transform: rotate(0deg); }
          to { transform: rotate(360deg); }
        }
      `}</style>

      {/* Top Navigation Bar */}
      <div style={{
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `2px solid ${FINCEPT.ORANGE}`,
        padding: '8px 16px',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        boxShadow: `0 2px 8px ${FINCEPT.ORANGE}20`,
        flexShrink: 0,
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          <Zap size={14} style={{ color: FINCEPT.ORANGE }} />
          <span style={{
            color: FINCEPT.ORANGE,
            fontSize: '12px',
            fontWeight: 700,
            letterSpacing: '0.5px',
          }}>
            MCP SERVERS
          </span>
          <div style={{ width: '1px', height: '16px', backgroundColor: FINCEPT.BORDER }} />
          <span style={{ color: FINCEPT.GRAY, fontSize: '9px', letterSpacing: '0.5px' }}>
            {state.servers.length} SERVERS | {runningServers} RUNNING | {state.tools.length} TOOLS
          </span>
        </div>

        <div style={{ display: 'flex', gap: '6px', alignItems: 'center' }}>
          {/* Tab Buttons */}
          {(['installed', 'marketplace', 'tools'] as const).map((tab) => {
            const labels: Record<string, string> = {
              installed: 'MY SERVERS',
              marketplace: 'MARKETPLACE',
              tools: 'ALL TOOLS',
            };
            return (
              <button
                key={tab}
                onClick={() => dispatch({ type: 'SET_VIEW', payload: tab })}
                style={{
                  padding: '6px 12px',
                  backgroundColor: state.view === tab ? FINCEPT.ORANGE : 'transparent',
                  color: state.view === tab ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                  border: 'none',
                  fontSize: '9px',
                  fontWeight: 700,
                  letterSpacing: '0.5px',
                  cursor: 'pointer',
                  borderRadius: '2px',
                  transition: 'all 0.2s',
                  fontFamily: FONT_FAMILY,
                }}
              >
                {labels[tab]}
              </button>
            );
          })}

          <div style={{ width: '1px', height: '16px', backgroundColor: FINCEPT.BORDER }} />

          {/* Action Buttons */}
          <button
            onClick={() => onNavigateToTab?.('chat')}
            style={{
              padding: '6px 10px',
              backgroundColor: 'transparent',
              border: `1px solid ${FINCEPT.BORDER}`,
              color: FINCEPT.GRAY,
              fontSize: '9px',
              fontWeight: 700,
              borderRadius: '2px',
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
              transition: 'all 0.2s',
              fontFamily: FONT_FAMILY,
            }}
          >
            <MessageSquare size={10} />
            AI CHAT
          </button>

          <button
            onClick={handleRefresh}
            disabled={state.isRefreshing || state.status === 'loading'}
            style={{
              padding: '6px 10px',
              backgroundColor: 'transparent',
              border: `1px solid ${FINCEPT.BORDER}`,
              color: FINCEPT.GRAY,
              fontSize: '9px',
              fontWeight: 700,
              borderRadius: '2px',
              cursor: state.isRefreshing || state.status === 'loading' ? 'not-allowed' : 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
              opacity: state.isRefreshing || state.status === 'loading' ? 0.5 : 1,
              transition: 'all 0.2s',
              fontFamily: FONT_FAMILY,
            }}
          >
            <RefreshCw size={10} style={{ animation: state.isRefreshing ? 'spin 1s linear infinite' : 'none' }} />
            REFRESH
          </button>

          <button
            onClick={() => dispatch({ type: 'OPEN_ADD_MODAL' })}
            style={{
              padding: '8px 16px',
              backgroundColor: FINCEPT.ORANGE,
              color: FINCEPT.DARK_BG,
              border: 'none',
              borderRadius: '2px',
              fontSize: '9px',
              fontWeight: 700,
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
              fontFamily: FONT_FAMILY,
            }}
          >
            <Plus size={10} />
            ADD SERVER
          </button>
        </div>
      </div>

      {/* Main Content - Three Panel Layout for installed view, full-width for marketplace/tools */}
      {state.view === 'installed' ? (
        <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>
          {/* Left Panel - Server List (280px) */}
          <div style={{
            width: '280px',
            flexShrink: 0,
            borderRight: `1px solid ${FINCEPT.BORDER}`,
            display: 'flex',
            flexDirection: 'column',
            overflow: 'hidden',
          }}>
            {/* Left Panel Header */}
            <div style={{
              padding: '12px',
              backgroundColor: FINCEPT.HEADER_BG,
              borderBottom: `1px solid ${FINCEPT.BORDER}`,
            }}>
              <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
                INSTALLED SERVERS
              </span>
            </div>

            {/* Search */}
            <div style={{ padding: '8px 12px', borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
              <div style={{ position: 'relative' }}>
                <Search size={10} style={{
                  position: 'absolute',
                  left: '8px',
                  top: '50%',
                  transform: 'translateY(-50%)',
                  color: FINCEPT.GRAY,
                }} />
                <input
                  type="text"
                  placeholder="Search servers..."
                  value={state.searchTerm}
                  onChange={(e) => dispatch({ type: 'SET_SEARCH_TERM', payload: e.target.value })}
                  style={{
                    width: '100%',
                    padding: '8px 10px 8px 28px',
                    backgroundColor: FINCEPT.DARK_BG,
                    color: FINCEPT.WHITE,
                    border: `1px solid ${FINCEPT.BORDER}`,
                    borderRadius: '2px',
                    fontSize: '10px',
                    fontFamily: FONT_FAMILY,
                    outline: 'none',
                  }}
                  onFocus={(e) => { e.currentTarget.style.borderColor = FINCEPT.ORANGE; }}
                  onBlur={(e) => { e.currentTarget.style.borderColor = FINCEPT.BORDER; }}
                />
              </div>
            </div>

            {/* Server List */}
            <div className="mcp-scroll" style={{ flex: 1, overflow: 'auto' }}>
              {/* Internal Tools Entry */}
              <div
                onClick={() => dispatch({ type: 'SET_SELECTED_SERVER', payload: 'internal' })}
                style={{
                  padding: '10px 12px',
                  backgroundColor: state.selectedServerId === 'internal' ? `${FINCEPT.ORANGE}15` : 'transparent',
                  borderLeft: state.selectedServerId === 'internal' ? `2px solid ${FINCEPT.ORANGE}` : '2px solid transparent',
                  cursor: 'pointer',
                  transition: 'all 0.2s',
                  borderBottom: `1px solid ${FINCEPT.BORDER}`,
                }}
                onMouseEnter={(e) => {
                  if (state.selectedServerId !== 'internal') e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                }}
                onMouseLeave={(e) => {
                  if (state.selectedServerId !== 'internal') e.currentTarget.style.backgroundColor = 'transparent';
                }}
              >
                <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '4px' }}>
                  <Zap size={12} style={{ color: FINCEPT.ORANGE }} />
                  <span style={{ fontSize: '10px', fontWeight: 700, color: FINCEPT.WHITE }}>
                    Fincept Terminal
                  </span>
                  <CheckCircle size={10} color={FINCEPT.GREEN} />
                </div>
                <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginLeft: '20px' }}>
                  <span style={{
                    padding: '2px 6px',
                    backgroundColor: `${FINCEPT.GREEN}20`,
                    color: FINCEPT.GREEN,
                    fontSize: '8px',
                    fontWeight: 700,
                    borderRadius: '2px',
                  }}>BUILT-IN</span>
                  <span style={{ fontSize: '9px', color: FINCEPT.GRAY }}>
                    {internalToolCount} tools
                  </span>
                </div>
              </div>

              {/* External Servers */}
              {filteredServers.length === 0 && !state.searchTerm ? (
                <div style={{
                  display: 'flex',
                  flexDirection: 'column',
                  alignItems: 'center',
                  justifyContent: 'center',
                  padding: '32px 16px',
                  color: FINCEPT.MUTED,
                  fontSize: '10px',
                  textAlign: 'center',
                }}>
                  <Server size={24} style={{ marginBottom: '8px', opacity: 0.5 }} />
                  <span>No external servers</span>
                  <span style={{ fontSize: '9px', marginTop: '4px' }}>Install from marketplace</span>
                </div>
              ) : (
                filteredServers.map(server => (
                  <div
                    key={server.id}
                    onClick={() => dispatch({ type: 'SET_SELECTED_SERVER', payload: server.id })}
                    style={{
                      padding: '10px 12px',
                      backgroundColor: state.selectedServerId === server.id ? `${FINCEPT.ORANGE}15` : 'transparent',
                      borderLeft: state.selectedServerId === server.id ? `2px solid ${FINCEPT.ORANGE}` : '2px solid transparent',
                      cursor: 'pointer',
                      transition: 'all 0.2s',
                      borderBottom: `1px solid ${FINCEPT.BORDER}`,
                    }}
                    onMouseEnter={(e) => {
                      if (state.selectedServerId !== server.id) e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                    }}
                    onMouseLeave={(e) => {
                      if (state.selectedServerId !== server.id) e.currentTarget.style.backgroundColor = 'transparent';
                    }}
                  >
                    <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '4px' }}>
                      <span style={{ fontSize: '14px' }}>{server.icon}</span>
                      <span style={{ fontSize: '10px', fontWeight: 700, color: FINCEPT.WHITE, flex: 1, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
                        {server.name}
                      </span>
                      {getStatusIcon(server.status)}
                    </div>
                    <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginLeft: '22px' }}>
                      <span style={{
                        padding: '2px 6px',
                        backgroundColor: `${getStatusColor(server.status)}20`,
                        color: getStatusColor(server.status),
                        fontSize: '8px',
                        fontWeight: 700,
                        borderRadius: '2px',
                        textTransform: 'uppercase',
                      }}>{server.status}</span>
                      <span style={{ fontSize: '9px', color: FINCEPT.GRAY }}>
                        {server.toolCount} tools
                      </span>
                      {server.auto_start && (
                        <Zap size={8} style={{ color: FINCEPT.ORANGE }} />
                      )}
                    </div>
                  </div>
                ))
              )}
            </div>
          </div>

          {/* Center Panel - Server Detail / Default View */}
          <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
            {/* Center Panel Header */}
            <div style={{
              padding: '12px',
              backgroundColor: FINCEPT.HEADER_BG,
              borderBottom: `1px solid ${FINCEPT.BORDER}`,
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'space-between',
            }}>
              <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
                {state.selectedServerId === 'internal'
                  ? 'FINCEPT TERMINAL (INTERNAL)'
                  : selectedServer
                    ? selectedServer.name.toUpperCase()
                    : 'SERVER DETAILS'}
              </span>
              {(selectedServer || state.selectedServerId === 'internal') && (
                <span style={{ fontSize: '9px', color: FINCEPT.CYAN }}>
                  {selectedServerTools.length} TOOLS AVAILABLE
                </span>
              )}
            </div>

            {/* Center Content */}
            <div className="mcp-scroll" style={{ flex: 1, overflow: 'auto', padding: '16px' }}>
              {state.selectedServerId === 'internal' ? (
                // Internal server detail
                <div>
                  <div style={{ display: 'flex', alignItems: 'center', gap: '12px', marginBottom: '16px' }}>
                    <Zap size={20} style={{ color: FINCEPT.ORANGE }} />
                    <div>
                      <div style={{ fontSize: '14px', fontWeight: 700, color: FINCEPT.WHITE }}>
                        Fincept Terminal
                      </div>
                      <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginTop: '2px' }}>
                        Built-in tools for terminal control, trading, portfolio management, backtesting, and more.
                      </div>
                    </div>
                  </div>

                  {/* Stats Row */}
                  <div style={{
                    display: 'flex',
                    gap: '16px',
                    marginBottom: '16px',
                    padding: '12px',
                    backgroundColor: FINCEPT.PANEL_BG,
                    border: `1px solid ${FINCEPT.BORDER}`,
                    borderRadius: '2px',
                  }}>
                    <div>
                      <div style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px', marginBottom: '4px' }}>STATUS</div>
                      <span style={{ padding: '2px 6px', backgroundColor: `${FINCEPT.GREEN}20`, color: FINCEPT.GREEN, fontSize: '8px', fontWeight: 700, borderRadius: '2px' }}>ACTIVE</span>
                    </div>
                    <div>
                      <div style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px', marginBottom: '4px' }}>TOOLS</div>
                      <span style={{ fontSize: '10px', color: FINCEPT.CYAN }}>{internalToolCount}</span>
                    </div>
                    <div>
                      <div style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px', marginBottom: '4px' }}>TYPE</div>
                      <span style={{ fontSize: '10px', color: FINCEPT.WHITE }}>Internal</span>
                    </div>
                  </div>

                  {/* Action */}
                  <button
                    onClick={() => dispatch({ type: 'SET_VIEW', payload: 'tools' })}
                    style={{
                      padding: '8px 16px',
                      backgroundColor: FINCEPT.ORANGE,
                      color: FINCEPT.DARK_BG,
                      border: 'none',
                      borderRadius: '2px',
                      fontSize: '9px',
                      fontWeight: 700,
                      cursor: 'pointer',
                      display: 'flex',
                      alignItems: 'center',
                      gap: '6px',
                      fontFamily: FONT_FAMILY,
                    }}
                  >
                    <Settings size={10} />
                    MANAGE TOOLS
                  </button>
                </div>
              ) : selectedServer ? (
                // External server detail
                <div>
                  <div style={{ display: 'flex', alignItems: 'center', gap: '12px', marginBottom: '16px' }}>
                    <span style={{ fontSize: '24px' }}>{selectedServer.icon}</span>
                    <div>
                      <div style={{ fontSize: '14px', fontWeight: 700, color: FINCEPT.WHITE }}>
                        {selectedServer.name}
                      </div>
                      <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginTop: '2px', lineHeight: '1.5' }}>
                        {selectedServer.description}
                      </div>
                    </div>
                  </div>

                  {/* Stats Row */}
                  <div style={{
                    display: 'flex',
                    gap: '16px',
                    marginBottom: '16px',
                    padding: '12px',
                    backgroundColor: FINCEPT.PANEL_BG,
                    border: `1px solid ${FINCEPT.BORDER}`,
                    borderRadius: '2px',
                    flexWrap: 'wrap',
                  }}>
                    <div>
                      <div style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px', marginBottom: '4px' }}>STATUS</div>
                      <span style={{
                        padding: '2px 6px',
                        backgroundColor: `${getStatusColor(selectedServer.status)}20`,
                        color: getStatusColor(selectedServer.status),
                        fontSize: '8px',
                        fontWeight: 700,
                        borderRadius: '2px',
                        textTransform: 'uppercase',
                      }}>{selectedServer.status}</span>
                    </div>
                    <div>
                      <div style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px', marginBottom: '4px' }}>TOOLS</div>
                      <span style={{ fontSize: '10px', color: FINCEPT.CYAN }}>{selectedServer.toolCount}</span>
                    </div>
                    <div>
                      <div style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px', marginBottom: '4px' }}>CALLS TODAY</div>
                      <span style={{ fontSize: '10px', color: FINCEPT.CYAN }}>{selectedServer.callsToday}</span>
                    </div>
                    <div>
                      <div style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px', marginBottom: '4px' }}>AUTO-START</div>
                      <span style={{ fontSize: '10px', color: selectedServer.auto_start ? FINCEPT.GREEN : FINCEPT.GRAY }}>
                        {selectedServer.auto_start ? 'ENABLED' : 'DISABLED'}
                      </span>
                    </div>
                  </div>

                  {/* Error Info */}
                  {(() => {
                    const healthInfo = mcpManager.getServerHealthInfo(selectedServer.id);
                    if (healthInfo && healthInfo.errorCount > 0) {
                      return (
                        <div style={{
                          padding: '8px 12px',
                          backgroundColor: `${FINCEPT.RED}20`,
                          border: `1px solid ${FINCEPT.RED}`,
                          borderRadius: '2px',
                          fontSize: '9px',
                          color: FINCEPT.RED,
                          marginBottom: '16px',
                        }}>
                          <strong>ERRORS: {healthInfo.errorCount}</strong>
                          {healthInfo.lastError && (
                            <div style={{ marginTop: '4px', color: FINCEPT.WHITE, opacity: 0.8 }}>{healthInfo.lastError}</div>
                          )}
                        </div>
                      );
                    }
                    return null;
                  })()}

                  {/* Actions */}
                  <div style={{ display: 'flex', gap: '8px', flexWrap: 'wrap' }}>
                    {selectedServer.status === 'running' ? (
                      <button
                        onClick={() => handleStopServer(selectedServer.id)}
                        style={{
                          padding: '6px 10px',
                          backgroundColor: 'transparent',
                          border: `1px solid ${FINCEPT.YELLOW}`,
                          color: FINCEPT.YELLOW,
                          fontSize: '9px',
                          fontWeight: 700,
                          borderRadius: '2px',
                          cursor: 'pointer',
                          display: 'flex',
                          alignItems: 'center',
                          gap: '4px',
                          fontFamily: FONT_FAMILY,
                        }}
                      >
                        <Square size={10} />
                        STOP
                      </button>
                    ) : (
                      <button
                        onClick={() => handleStartServer(selectedServer.id)}
                        style={{
                          padding: '8px 16px',
                          backgroundColor: FINCEPT.ORANGE,
                          color: FINCEPT.DARK_BG,
                          border: 'none',
                          borderRadius: '2px',
                          fontSize: '9px',
                          fontWeight: 700,
                          cursor: 'pointer',
                          display: 'flex',
                          alignItems: 'center',
                          gap: '4px',
                          fontFamily: FONT_FAMILY,
                        }}
                      >
                        <Play size={10} />
                        START
                      </button>
                    )}

                    <button
                      onClick={() => handleToggleAutoStart(selectedServer.id, !selectedServer.auto_start)}
                      style={{
                        padding: '6px 10px',
                        backgroundColor: 'transparent',
                        border: `1px solid ${selectedServer.auto_start ? FINCEPT.ORANGE : FINCEPT.BORDER}`,
                        color: selectedServer.auto_start ? FINCEPT.ORANGE : FINCEPT.GRAY,
                        fontSize: '9px',
                        fontWeight: 700,
                        borderRadius: '2px',
                        cursor: 'pointer',
                        display: 'flex',
                        alignItems: 'center',
                        gap: '4px',
                        fontFamily: FONT_FAMILY,
                      }}
                    >
                      <Zap size={10} />
                      AUTO-START
                    </button>

                    <button
                      onClick={() => handleRemoveServer(selectedServer.id)}
                      style={{
                        padding: '6px 10px',
                        backgroundColor: 'transparent',
                        border: `1px solid ${FINCEPT.RED}`,
                        color: FINCEPT.RED,
                        fontSize: '9px',
                        fontWeight: 700,
                        borderRadius: '2px',
                        cursor: 'pointer',
                        display: 'flex',
                        alignItems: 'center',
                        gap: '4px',
                        fontFamily: FONT_FAMILY,
                      }}
                    >
                      <Trash2 size={10} />
                      REMOVE
                    </button>
                  </div>
                </div>
              ) : (
                // No server selected - empty state
                <div style={{
                  display: 'flex',
                  flexDirection: 'column',
                  alignItems: 'center',
                  justifyContent: 'center',
                  height: '100%',
                  color: FINCEPT.MUTED,
                  fontSize: '10px',
                  textAlign: 'center',
                }}>
                  <Server size={24} style={{ marginBottom: '8px', opacity: 0.5 }} />
                  <span>Select a server to view details</span>
                </div>
              )}
            </div>
          </div>

          {/* Right Panel - Tools for selected server (300px) */}
          <div style={{
            width: '300px',
            flexShrink: 0,
            borderLeft: `1px solid ${FINCEPT.BORDER}`,
            display: 'flex',
            flexDirection: 'column',
            overflow: 'hidden',
          }}>
            {/* Right Panel Header */}
            <div style={{
              padding: '12px',
              backgroundColor: FINCEPT.HEADER_BG,
              borderBottom: `1px solid ${FINCEPT.BORDER}`,
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'space-between',
            }}>
              <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
                SERVER TOOLS
              </span>
              <span style={{ fontSize: '9px', color: FINCEPT.CYAN }}>
                {selectedServerTools.length}
              </span>
            </div>

            {/* Tools List */}
            <div className="mcp-scroll" style={{ flex: 1, overflow: 'auto' }}>
              {selectedServerTools.length === 0 ? (
                <div style={{
                  display: 'flex',
                  flexDirection: 'column',
                  alignItems: 'center',
                  justifyContent: 'center',
                  height: '100%',
                  color: FINCEPT.MUTED,
                  fontSize: '10px',
                  textAlign: 'center',
                  padding: '16px',
                }}>
                  <Wrench size={24} style={{ marginBottom: '8px', opacity: 0.5 }} />
                  <span>Select a server to see its tools</span>
                </div>
              ) : (
                selectedServerTools.map((tool: any, index: number) => (
                  <div
                    key={`${tool.serverId}__${tool.name}__${index}`}
                    style={{
                      padding: '10px 12px',
                      borderBottom: `1px solid ${FINCEPT.BORDER}`,
                      transition: 'all 0.2s',
                    }}
                    onMouseEnter={(e) => { e.currentTarget.style.backgroundColor = FINCEPT.HOVER; }}
                    onMouseLeave={(e) => { e.currentTarget.style.backgroundColor = 'transparent'; }}
                  >
                    <div style={{ display: 'flex', alignItems: 'center', gap: '6px', marginBottom: '2px' }}>
                      {tool.serverId === 'fincept-terminal' ? (
                        <Zap size={10} style={{ color: FINCEPT.GREEN, flexShrink: 0 }} />
                      ) : (
                        <Settings size={10} style={{ color: FINCEPT.ORANGE, flexShrink: 0 }} />
                      )}
                      <span style={{
                        color: FINCEPT.WHITE,
                        fontWeight: 700,
                        fontSize: '10px',
                        overflow: 'hidden',
                        textOverflow: 'ellipsis',
                        whiteSpace: 'nowrap',
                      }}>
                        {tool.name}
                      </span>
                    </div>
                    {tool.description && (
                      <div style={{
                        color: FINCEPT.GRAY,
                        fontSize: '9px',
                        marginLeft: '16px',
                        lineHeight: '1.4',
                        overflow: 'hidden',
                        textOverflow: 'ellipsis',
                        whiteSpace: 'nowrap',
                      }}>
                        {tool.description}
                      </div>
                    )}
                  </div>
                ))
              )}
            </div>

            {/* Quick Stats Footer */}
            <div style={{
              padding: '8px 12px',
              backgroundColor: FINCEPT.HEADER_BG,
              borderTop: `1px solid ${FINCEPT.BORDER}`,
              display: 'flex',
              gap: '12px',
              fontSize: '9px',
            }}>
              <span style={{ color: FINCEPT.GREEN }}>
                {internalToolCount} INTERNAL
              </span>
              <span style={{ color: FINCEPT.ORANGE }}>
                {externalToolCount} EXTERNAL
              </span>
            </div>
          </div>
        </div>
      ) : (
        /* Marketplace / Tools views - full width */
        <div className="mcp-scroll" style={{ flex: 1, overflow: 'auto', padding: '16px' }}>
          {state.view === 'marketplace' ? (
            <MCPMarketplace
              onInstall={handleInstallServer}
              installedServers={state.servers}
              installingServerIds={new Set([
                ...mcpManager.getInstallingServerIds(),
                ...Array.from(pendingOperationsRef.current).filter(op => op.startsWith('install:')).map(op => op.replace('install:', ''))
              ])}
            />
          ) : (
            <MCPToolsManagement />
          )}
        </div>
      )}

      {/* Status Bar (Bottom) */}
      <div style={{
        backgroundColor: FINCEPT.HEADER_BG,
        borderTop: `1px solid ${FINCEPT.BORDER}`,
        padding: '4px 16px',
        fontSize: '9px',
        color: FINCEPT.GRAY,
        flexShrink: 0,
        display: 'flex',
        alignItems: 'center',
        gap: '8px',
      }}>
        <span style={{ color: state.status === 'error' ? FINCEPT.RED : FINCEPT.ORANGE }}>●</span>
        <span>{state.statusMessage}</span>
        <div style={{ flex: 1 }} />
        <span style={{ color: FINCEPT.CYAN }}>MCP v1.0</span>
      </div>

      {/* Add Server Modal */}
      {state.isAddModalOpen && (
        <MCPAddServerModal
          onClose={() => dispatch({ type: 'CLOSE_ADD_MODAL' })}
          onAdd={(config) => {
            handleInstallServer(config);
            dispatch({ type: 'CLOSE_ADD_MODAL' });
          }}
        />
      )}
    </div>
  );
};

// ============================================================================
// Export with ErrorBoundary wrapper
// ============================================================================

const MCPTab: React.FC<MCPTabProps> = (props) => (
  <ErrorBoundary name="MCP Tab" variant="default">
    <MCPTabInner {...props} />
  </ErrorBoundary>
);

export default MCPTab;
