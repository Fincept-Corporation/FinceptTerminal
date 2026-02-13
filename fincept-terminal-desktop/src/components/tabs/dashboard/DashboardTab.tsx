// DashboardTab - Bloomberg-style financial terminal dashboard
// Professional-grade layout with three-panel terminal design, market ticker, heatmaps, sparklines

import React, { useReducer, useEffect, useRef, useCallback, useState, useMemo } from 'react';
import { useWorkspaceTabState } from '@/hooks/useWorkspaceTabState';
import GridLayout, { Layout } from 'react-grid-layout';
import {
  Plus, RotateCcw, Save, AlertCircle, Maximize2, Minimize2,
  Activity, Zap, Globe, TrendingUp, TrendingDown, BarChart3,
  Clock, Wifi, WifiOff, ChevronDown, ChevronUp, Layers,
  Terminal, Shield, Eye, EyeOff, Grid3X3, LayoutGrid
} from 'lucide-react';
import { createDashboardTabTour } from '@/components/tabs/tours/dashboardTabTour';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { useTranslation } from 'react-i18next';
import { sqliteService, saveSetting, getSetting } from '@/services/core/sqliteService';
import { ErrorBoundary } from '@/components/common/ErrorBoundary';
import { withTimeout } from '@/services/core/apiUtils';
import { showConfirm, showSuccess, showError } from '@/utils/notifications';
import {
  NewsWidget,
  MarketDataWidget,
  WatchlistWidget,
  ForumWidget,
  CryptoWidget,
  CommoditiesWidget,
  GlobalIndicesWidget,
  ForexWidget,
  MaritimeWidget,
  DataSourceWidget,
  PolymarketWidget,
  EconomicIndicatorsWidget,
  PortfolioSummaryWidget,
  AlertsWidget,
  CalendarWidget,
  QuickTradeWidget,
  GeopoliticsWidget,
  PerformanceWidget,
  WidgetType,
  WidgetConfig
} from './widgets';
import { AddWidgetModal } from './AddWidgetModal';
import { MarketTickerBar } from './MarketTickerBar';
import { MarketPulsePanel } from './MarketPulsePanel';
import { HeatMapWidget } from './widgets/HeatMapWidget';
import 'react-grid-layout/css/styles.css';
import 'react-resizable/css/styles.css';

// ============================================================================
// Fincept Design System Constants
// ============================================================================

const FC = {
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

// ============================================================================
// Constants
// ============================================================================

const STORAGE_KEY = 'dashboard-widgets';
const DB_TIMEOUT_MS = 10000;
const SAVE_TIMEOUT_MS = 5000;

interface WidgetInstance extends WidgetConfig {
  layout: Layout;
}

// ============================================================================
// State Machine
// ============================================================================

type DashboardStatus = 'initializing' | 'loading' | 'ready' | 'saving' | 'error';

interface State {
  status: DashboardStatus;
  widgets: WidgetInstance[];
  nextId: number;
  showAddModal: boolean;
  currentTime: Date;
  containerWidth: number;
  error: string | null;
  rightPanelCollapsed: boolean;
  compactMode: boolean;
}

type Action =
  | { type: 'INIT_COMPLETE'; widgets: WidgetInstance[]; nextId: number }
  | { type: 'INIT_ERROR'; error: string }
  | { type: 'SET_WIDGETS'; widgets: WidgetInstance[] }
  | { type: 'ADD_WIDGET'; widget: WidgetInstance }
  | { type: 'REMOVE_WIDGET'; id: string }
  | { type: 'UPDATE_LAYOUTS'; layouts: Layout[] }
  | { type: 'SET_NEXT_ID'; nextId: number }
  | { type: 'TOGGLE_MODAL'; show: boolean }
  | { type: 'UPDATE_TIME'; time: Date }
  | { type: 'SET_WIDTH'; width: number }
  | { type: 'START_SAVING' }
  | { type: 'SAVE_COMPLETE' }
  | { type: 'SAVE_ERROR'; error: string }
  | { type: 'RESET_LAYOUT'; widgets: WidgetInstance[] }
  | { type: 'CLEAR_ERROR' }
  | { type: 'TOGGLE_RIGHT_PANEL' }
  | { type: 'TOGGLE_COMPACT_MODE' };

const DEFAULT_LAYOUT: WidgetInstance[] = [
  // Row 1 - Top: Global Indices + Portfolio + Performance
  {
    id: 'indices-1',
    type: 'indices',
    title: 'widgets.globalIndices',
    config: {},
    layout: { i: 'indices-1', x: 0, y: 0, w: 4, h: 5, minW: 3, minH: 4 }
  },
  {
    id: 'heatmap-1',
    type: 'heatmap',
    title: 'SECTOR HEATMAP',
    config: {},
    layout: { i: 'heatmap-1', x: 4, y: 0, w: 4, h: 5, minW: 3, minH: 4 }
  },
  {
    id: 'performance-1',
    type: 'performance',
    title: 'widgets.performanceTracker',
    config: {},
    layout: { i: 'performance-1', x: 8, y: 0, w: 4, h: 5, minW: 3, minH: 4 }
  },
  // Row 2 - Forex, Commodities, Crypto
  {
    id: 'forex-1',
    type: 'forex',
    title: 'widgets.forex',
    config: {},
    layout: { i: 'forex-1', x: 0, y: 5, w: 4, h: 4, minW: 2, minH: 3 }
  },
  {
    id: 'commodities-1',
    type: 'commodities',
    title: 'widgets.commodities',
    config: {},
    layout: { i: 'commodities-1', x: 4, y: 5, w: 4, h: 4, minW: 2, minH: 3 }
  },
  {
    id: 'crypto-1',
    type: 'crypto',
    title: 'widgets.crypto',
    config: {},
    layout: { i: 'crypto-1', x: 8, y: 5, w: 4, h: 4, minW: 2, minH: 3 }
  },
  // Row 3 - News + Calendar + Geopolitics
  {
    id: 'news-1',
    type: 'news',
    title: 'widgets.marketNews',
    config: { newsCategory: 'MARKETS', newsLimit: 8 },
    layout: { i: 'news-1', x: 0, y: 9, w: 4, h: 5, minW: 2, minH: 3 }
  },
  {
    id: 'calendar-1',
    type: 'calendar',
    title: 'widgets.economicCalendar',
    config: {},
    layout: { i: 'calendar-1', x: 4, y: 9, w: 4, h: 5, minW: 2, minH: 3 }
  },
  {
    id: 'geopolitics-1',
    type: 'geopolitics',
    title: 'widgets.geopoliticalRisk',
    config: {},
    layout: { i: 'geopolitics-1', x: 8, y: 9, w: 4, h: 5, minW: 2, minH: 3 }
  }
];

const initialState: State = {
  status: 'initializing',
  widgets: [],
  nextId: 1,
  showAddModal: false,
  currentTime: new Date(),
  containerWidth: 1200,
  error: null,
  rightPanelCollapsed: false,
  compactMode: true,
};

function reducer(state: State, action: Action): State {
  switch (action.type) {
    case 'INIT_COMPLETE':
      return { ...state, status: 'ready', widgets: action.widgets, nextId: action.nextId, error: null };
    case 'INIT_ERROR':
      return { ...state, status: 'error', error: action.error };
    case 'SET_WIDGETS':
      return { ...state, widgets: action.widgets };
    case 'ADD_WIDGET':
      return { ...state, widgets: [...state.widgets, action.widget], nextId: state.nextId + 1 };
    case 'REMOVE_WIDGET':
      return { ...state, widgets: state.widgets.filter(w => w.id !== action.id) };
    case 'UPDATE_LAYOUTS':
      return {
        ...state,
        widgets: state.widgets.map(widget => {
          const newLayout = action.layouts.find(l => l.i === widget.id);
          return newLayout ? { ...widget, layout: newLayout } : widget;
        }),
      };
    case 'SET_NEXT_ID':
      return { ...state, nextId: action.nextId };
    case 'TOGGLE_MODAL':
      return { ...state, showAddModal: action.show };
    case 'UPDATE_TIME':
      return { ...state, currentTime: action.time };
    case 'SET_WIDTH':
      return { ...state, containerWidth: action.width };
    case 'START_SAVING':
      return { ...state, status: 'saving' };
    case 'SAVE_COMPLETE':
      return { ...state, status: 'ready', error: null };
    case 'SAVE_ERROR':
      return { ...state, status: 'ready', error: action.error };
    case 'RESET_LAYOUT':
      return { ...state, widgets: action.widgets, nextId: 1 };
    case 'CLEAR_ERROR':
      return { ...state, error: null };
    case 'TOGGLE_RIGHT_PANEL':
      return { ...state, rightPanelCollapsed: !state.rightPanelCollapsed };
    case 'TOGGLE_COMPACT_MODE':
      return { ...state, compactMode: !state.compactMode };
    default:
      return state;
  }
}

// ============================================================================
// Component
// ============================================================================

interface DashboardTabProps {
  onNavigateToTab?: (tabName: string) => void;
}

const DashboardTab: React.FC<DashboardTabProps> = ({ onNavigateToTab }) => {
  const { colors, fontSize } = useTerminalTheme();
  const { t } = useTranslation('dashboard');

  const [state, dispatch] = useReducer(reducer, initialState);
  const containerRef = useRef<HTMLDivElement>(null);
  const mountedRef = useRef(true);
  const [isConnected, setIsConnected] = useState(true);
  const [sessionUptime, setSessionUptime] = useState(0);
  const sessionStartRef = useRef(Date.now());

  // Workspace tab state
  const getWorkspaceState = useCallback(() => ({
    widgetLayoutKey: "dashboard-widgets",
  }), []);
  const setWorkspaceState = useCallback((_state: Record<string, unknown>) => {}, []);
  useWorkspaceTabState("dashboard", getWorkspaceState, setWorkspaceState);

  // Cleanup on unmount
  useEffect(() => {
    mountedRef.current = true;
    return () => { mountedRef.current = false; };
  }, []);

  // Initialize database and load widgets
  useEffect(() => {
    const init = async () => {
      try {
        await withTimeout(sqliteService.initialize(), DB_TIMEOUT_MS, 'Database init timeout');
        if (!mountedRef.current) return;

        const saved = await withTimeout(getSetting(STORAGE_KEY), DB_TIMEOUT_MS, 'Load settings timeout');
        if (!mountedRef.current) return;

        if (saved) {
          try {
            const parsed = JSON.parse(saved);
            dispatch({ type: 'INIT_COMPLETE', widgets: parsed.widgets || DEFAULT_LAYOUT, nextId: parsed.nextId || 1 });
          } catch {
            dispatch({ type: 'INIT_COMPLETE', widgets: DEFAULT_LAYOUT, nextId: 1 });
          }
        } else {
          dispatch({ type: 'INIT_COMPLETE', widgets: DEFAULT_LAYOUT, nextId: 1 });
        }
      } catch (error) {
        if (!mountedRef.current) return;
        console.error('[DashboardTab] Init error:', error);
        dispatch({ type: 'INIT_COMPLETE', widgets: DEFAULT_LAYOUT, nextId: 1 });
      }
    };
    init();
  }, []);

  // Update time + uptime
  useEffect(() => {
    const timer = setInterval(() => {
      if (mountedRef.current) {
        dispatch({ type: 'UPDATE_TIME', time: new Date() });
        setSessionUptime(Math.floor((Date.now() - sessionStartRef.current) / 1000));
      }
    }, 1000);
    return () => clearInterval(timer);
  }, []);

  // Simulate connection status
  useEffect(() => {
    const check = setInterval(() => {
      setIsConnected(navigator.onLine);
    }, 5000);
    return () => clearInterval(check);
  }, []);

  // Handle responsive width
  useEffect(() => {
    const updateWidth = () => {
      if (containerRef.current && mountedRef.current) {
        dispatch({ type: 'SET_WIDTH', width: containerRef.current.offsetWidth });
      }
    };
    updateWidth();
    window.addEventListener('resize', updateWidth);
    const resizeObserver = new ResizeObserver(updateWidth);
    if (containerRef.current) resizeObserver.observe(containerRef.current);
    const timeout = setTimeout(updateWidth, 100);
    return () => {
      window.removeEventListener('resize', updateWidth);
      resizeObserver.disconnect();
      clearTimeout(timeout);
    };
  }, []);

  // Save layout
  const saveLayout = useCallback(async () => {
    if (state.status === 'saving') return;
    dispatch({ type: 'START_SAVING' });
    try {
      await withTimeout(
        saveSetting(STORAGE_KEY, JSON.stringify({ widgets: state.widgets, nextId: state.nextId }), 'dashboard'),
        SAVE_TIMEOUT_MS, 'Save timeout'
      );
      if (!mountedRef.current) return;
      dispatch({ type: 'SAVE_COMPLETE' });
      showSuccess(t('messages.layoutSaved'));
    } catch {
      if (!mountedRef.current) return;
      dispatch({ type: 'SAVE_ERROR', error: 'Failed to save layout' });
      showError('Failed to save layout');
    }
  }, [state.status, state.widgets, state.nextId, t]);

  // Reset layout
  const resetLayout = useCallback(async () => {
    const confirmed = await showConfirm('', {
      title: t('messages.resetConfirm'),
      type: 'warning'
    });
    if (!confirmed) return;
    dispatch({ type: 'RESET_LAYOUT', widgets: DEFAULT_LAYOUT });
    try {
      await withTimeout(saveSetting(STORAGE_KEY, '', 'dashboard'), SAVE_TIMEOUT_MS, 'Reset timeout');
    } catch (error) {
      console.error('[DashboardTab] Reset save error:', error);
    }
  }, [t]);

  // Add widget
  const handleAddWidget = useCallback((widgetType: WidgetType, config?: any) => {
    const newWidget: WidgetInstance = {
      id: `${widgetType}-${state.nextId}`,
      type: widgetType,
      title: config?.watchlistName || config?.marketCategory || config?.newsCategory || config?.forumCategoryName || widgetType,
      config: config || {},
      layout: {
        i: `${widgetType}-${state.nextId}`,
        x: (state.widgets.length * 4) % 12,
        y: Infinity,
        w: 4,
        h: 4,
        minW: 2,
        minH: 3,
      },
    };
    dispatch({ type: 'ADD_WIDGET', widget: newWidget });
  }, [state.nextId, state.widgets.length]);

  const handleRemoveWidget = useCallback((id: string) => {
    dispatch({ type: 'REMOVE_WIDGET', id });
  }, []);

  const handleLayoutChange = useCallback((layouts: Layout[]) => {
    dispatch({ type: 'UPDATE_LAYOUTS', layouts });
  }, []);

  // Format uptime
  const formatUptime = (seconds: number) => {
    const h = Math.floor(seconds / 3600);
    const m = Math.floor((seconds % 3600) / 60);
    const s = seconds % 60;
    return `${h.toString().padStart(2, '0')}:${m.toString().padStart(2, '0')}:${s.toString().padStart(2, '0')}`;
  };

  // Render widget based on type
  const renderWidget = (widget: WidgetInstance) => {
    switch (widget.type) {
      case 'heatmap':
        return <HeatMapWidget id={widget.id} onRemove={() => handleRemoveWidget(widget.id)} />;
      case 'news':
        return <NewsWidget id={widget.id} category={widget.config?.newsCategory} limit={widget.config?.newsLimit} onRemove={() => handleRemoveWidget(widget.id)} />;
      case 'market':
        return <MarketDataWidget id={widget.id} category={widget.config?.marketCategory} tickers={widget.config?.marketTickers} onRemove={() => handleRemoveWidget(widget.id)} />;
      case 'watchlist':
        return <WatchlistWidget id={widget.id} watchlistId={widget.config?.watchlistId} watchlistName={widget.config?.watchlistName} onRemove={() => handleRemoveWidget(widget.id)} />;
      case 'forum':
        return <ForumWidget id={widget.id} categoryId={widget.config?.forumCategoryId} categoryName={widget.config?.forumCategoryName} limit={widget.config?.forumLimit} onRemove={() => handleRemoveWidget(widget.id)} onNavigateToTab={onNavigateToTab} />;
      case 'crypto':
        return <CryptoWidget id={widget.id} onRemove={() => handleRemoveWidget(widget.id)} />;
      case 'commodities':
        return <CommoditiesWidget id={widget.id} onRemove={() => handleRemoveWidget(widget.id)} />;
      case 'indices':
        return <GlobalIndicesWidget id={widget.id} onRemove={() => handleRemoveWidget(widget.id)} />;
      case 'forex':
        return <ForexWidget id={widget.id} onRemove={() => handleRemoveWidget(widget.id)} />;
      case 'maritime':
        return <MaritimeWidget id={widget.id} onRemove={() => handleRemoveWidget(widget.id)} onNavigate={() => onNavigateToTab?.('maritime')} />;
      case 'datasource':
        return <DataSourceWidget id={widget.id} alias={widget.config?.dataSourceAlias || ''} displayName={widget.config?.dataSourceDisplayName} onRemove={() => handleRemoveWidget(widget.id)} />;
      case 'polymarket':
        return <PolymarketWidget id={widget.id} onRemove={() => handleRemoveWidget(widget.id)} onNavigate={() => onNavigateToTab?.('polymarket')} />;
      case 'economic':
        return <EconomicIndicatorsWidget id={widget.id} onRemove={() => handleRemoveWidget(widget.id)} onNavigate={() => onNavigateToTab?.('economics')} />;
      case 'portfolio':
        return <PortfolioSummaryWidget id={widget.id} onRemove={() => handleRemoveWidget(widget.id)} onNavigate={() => onNavigateToTab?.('portfolio')} />;
      case 'alerts':
        return <AlertsWidget id={widget.id} onRemove={() => handleRemoveWidget(widget.id)} onNavigate={() => onNavigateToTab?.('equity-trading')} />;
      case 'calendar':
        return <CalendarWidget id={widget.id} onRemove={() => handleRemoveWidget(widget.id)} />;
      case 'quicktrade':
        return <QuickTradeWidget id={widget.id} onRemove={() => handleRemoveWidget(widget.id)} onNavigate={() => onNavigateToTab?.('trading')} />;
      case 'geopolitics':
        return <GeopoliticsWidget id={widget.id} onRemove={() => handleRemoveWidget(widget.id)} onNavigate={() => onNavigateToTab?.('geopolitics')} />;
      case 'performance':
        return <PerformanceWidget id={widget.id} onRemove={() => handleRemoveWidget(widget.id)} onNavigate={() => onNavigateToTab?.('portfolio')} />;
      default:
        return <div style={{ padding: '12px', color: FC.MUTED, fontSize: '10px' }}>UNKNOWN WIDGET</div>;
    }
  };

  const { status, widgets, showAddModal, currentTime, containerWidth, error, rightPanelCollapsed, compactMode } = state;

  // Loading state
  if (status === 'initializing') {
    return (
      <div style={{
        height: '100%',
        backgroundColor: FC.DARK_BG,
        display: 'flex',
        flexDirection: 'column',
        alignItems: 'center',
        justifyContent: 'center',
        gap: '16px',
        fontFamily: '"IBM Plex Mono", "Consolas", monospace',
      }}>
        <div style={{ position: 'relative', width: '80px', height: '80px' }}>
          <div style={{
            width: '80px', height: '80px',
            border: `2px solid ${FC.BORDER}`,
            borderTop: `2px solid ${FC.ORANGE}`,
            borderRadius: '50%',
            animation: 'ftSpin 1s linear infinite'
          }} />
          <div style={{
            position: 'absolute', top: '50%', left: '50%',
            transform: 'translate(-50%, -50%)',
            color: FC.ORANGE, fontSize: '20px', fontWeight: 700
          }}>
            FT
          </div>
        </div>
        <div style={{ textAlign: 'center' }}>
          <div style={{ color: FC.ORANGE, fontSize: '11px', fontWeight: 700, letterSpacing: '2px', marginBottom: '6px' }}>
            INITIALIZING TERMINAL
          </div>
          <div style={{ color: FC.GRAY, fontSize: '9px', letterSpacing: '0.5px' }}>
            LOADING MARKET DATA FEEDS...
          </div>
        </div>
        <style>{`@keyframes ftSpin { to { transform: rotate(360deg); } }`}</style>
      </div>
    );
  }

  // Error state
  if (status === 'error' && !widgets.length) {
    return (
      <div style={{
        height: '100%', backgroundColor: FC.DARK_BG, display: 'flex', flexDirection: 'column',
        alignItems: 'center', justifyContent: 'center', gap: '16px',
        fontFamily: '"IBM Plex Mono", "Consolas", monospace',
      }}>
        <AlertCircle size={32} color={FC.RED} />
        <div style={{ color: FC.RED, fontSize: '11px', fontWeight: 700, letterSpacing: '1px' }}>
          TERMINAL ERROR
        </div>
        <div style={{ color: FC.GRAY, fontSize: '9px' }}>{error || 'Failed to load dashboard'}</div>
        <button
          onClick={() => window.location.reload()}
          style={{
            backgroundColor: FC.ORANGE, color: FC.DARK_BG, border: 'none',
            padding: '8px 24px', fontSize: '9px', fontWeight: 700, cursor: 'pointer',
            borderRadius: '2px', letterSpacing: '0.5px'
          }}
        >
          RELOAD TERMINAL
        </button>
      </div>
    );
  }

  return (
    <div style={{
      height: '100%',
      backgroundColor: FC.DARK_BG,
      overflow: 'hidden',
      display: 'flex',
      flexDirection: 'column',
      fontFamily: '"IBM Plex Mono", "Consolas", monospace',
    }}>
      {/* === GLOBAL STYLES === */}
      <style>{`
        @keyframes ftSpin { to { transform: rotate(360deg); } }
        @keyframes ftPulse { 0%, 100% { opacity: 0.4; } 50% { opacity: 1; } }
        @keyframes ftSlide { 0% { transform: translateX(100%); } 100% { transform: translateX(-100%); } }
        @keyframes ftGlow { 0%, 100% { box-shadow: 0 0 4px ${FC.ORANGE}40; } 50% { box-shadow: 0 0 12px ${FC.ORANGE}60; } }
        @keyframes ftBlink { 0%, 100% { opacity: 1; } 50% { opacity: 0.3; } }

        .ft-dashboard *::-webkit-scrollbar { width: 6px; height: 6px; }
        .ft-dashboard *::-webkit-scrollbar-track { background: ${FC.DARK_BG}; }
        .ft-dashboard *::-webkit-scrollbar-thumb { background: ${FC.BORDER}; border-radius: 3px; }
        .ft-dashboard *::-webkit-scrollbar-thumb:hover { background: ${FC.MUTED}; }

        .react-grid-item.react-grid-placeholder {
          background: ${FC.ORANGE} !important;
          opacity: 0.15;
          border: 1px dashed ${FC.ORANGE};
          border-radius: 2px;
        }
        .react-grid-item { transition: all 200ms ease; transition-property: left, top, width, height; }
        .react-grid-item.cssTransforms { transition-property: transform, width, height; }
        .react-grid-item.react-draggable-dragging {
          transition: none; z-index: 100; will-change: transform;
          box-shadow: 0 0 20px ${FC.ORANGE}30;
        }
        .react-resizable-handle { opacity: 0; transition: opacity 0.2s; }
        .react-grid-item:hover .react-resizable-handle { opacity: 0.5; }
        .react-resizable-handle::after { border-right: 2px solid ${FC.ORANGE}; border-bottom: 2px solid ${FC.ORANGE}; }

        .ft-toolbar-btn {
          background: transparent;
          border: 1px solid ${FC.BORDER};
          color: ${FC.GRAY};
          padding: 4px 10px;
          font-size: 9px;
          font-weight: 700;
          font-family: "IBM Plex Mono", "Consolas", monospace;
          cursor: pointer;
          border-radius: 2px;
          display: flex;
          align-items: center;
          gap: 4px;
          letter-spacing: 0.5px;
          transition: all 0.2s;
        }
        .ft-toolbar-btn:hover { border-color: ${FC.ORANGE}; color: ${FC.WHITE}; }
        .ft-toolbar-btn-primary {
          background: ${FC.ORANGE};
          border-color: ${FC.ORANGE};
          color: ${FC.DARK_BG};
        }
        .ft-toolbar-btn-primary:hover { background: #FF9900; box-shadow: 0 0 8px ${FC.ORANGE}40; }
        .ft-toolbar-btn-danger { border-color: ${FC.RED}40; color: ${FC.RED}; }
        .ft-toolbar-btn-danger:hover { border-color: ${FC.RED}; background: ${FC.RED}15; }
      `}</style>

      {/* === TOP NAVIGATION BAR === */}
      <div className="ft-dashboard" style={{
        backgroundColor: FC.HEADER_BG,
        borderBottom: `2px solid ${FC.ORANGE}`,
        padding: '0',
        flexShrink: 0,
        boxShadow: `0 2px 8px ${FC.ORANGE}20`,
      }}>
        {/* Primary Header Row */}
        <div style={{
          display: 'flex', alignItems: 'center', justifyContent: 'space-between',
          padding: '6px 12px',
        }}>
          {/* Left: Terminal Identity */}
          <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
              <Terminal size={14} color={FC.ORANGE} />
              <span style={{ color: FC.ORANGE, fontWeight: 700, fontSize: '12px', letterSpacing: '1px' }}>
                FINCEPT
              </span>
              <span style={{ color: FC.MUTED, fontSize: '9px', fontWeight: 700 }}>TERMINAL</span>
            </div>

            <div style={{ width: '1px', height: '16px', backgroundColor: FC.BORDER }} />

            {/* Connection Status */}
            <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
              {isConnected ? (
                <>
                  <div style={{
                    width: '6px', height: '6px', borderRadius: '50%',
                    backgroundColor: FC.GREEN, animation: 'ftPulse 2s infinite',
                    boxShadow: `0 0 4px ${FC.GREEN}`
                  }} />
                  <span style={{ color: FC.GREEN, fontSize: '9px', fontWeight: 700, letterSpacing: '0.5px' }}>LIVE</span>
                </>
              ) : (
                <>
                  <div style={{ width: '6px', height: '6px', borderRadius: '50%', backgroundColor: FC.RED }} />
                  <span style={{ color: FC.RED, fontSize: '9px', fontWeight: 700 }}>OFFLINE</span>
                </>
              )}
            </div>

            <div style={{ width: '1px', height: '16px', backgroundColor: FC.BORDER }} />

            {/* Clock */}
            <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
              <Clock size={10} color={FC.YELLOW} />
              <span style={{ color: FC.YELLOW, fontSize: '10px', fontWeight: 700, fontFamily: '"IBM Plex Mono", monospace' }}>
                {currentTime.toISOString().replace('T', ' ').substring(0, 19)} UTC
              </span>
            </div>

            <div style={{ width: '1px', height: '16px', backgroundColor: FC.BORDER }} />

            {/* Widget Count */}
            <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
              <Layers size={10} color={FC.CYAN} />
              <span style={{ color: FC.CYAN, fontSize: '9px', fontWeight: 700 }}>
                {widgets.length} WIDGETS
              </span>
            </div>
          </div>

          {/* Right: Toolbar Actions */}
          <div style={{ display: 'flex', gap: '6px', alignItems: 'center' }}>
            <button
              className="ft-toolbar-btn"
              onClick={() => dispatch({ type: 'TOGGLE_COMPACT_MODE' })}
              title={compactMode ? 'Normal View' : 'Compact View'}
            >
              {compactMode ? <LayoutGrid size={11} /> : <Grid3X3 size={11} />}
            </button>
            <button
              className="ft-toolbar-btn"
              onClick={() => dispatch({ type: 'TOGGLE_RIGHT_PANEL' })}
              title={rightPanelCollapsed ? 'Show Market Pulse' : 'Hide Market Pulse'}
            >
              {rightPanelCollapsed ? <Eye size={11} /> : <EyeOff size={11} />}
              <span>PULSE</span>
            </button>
            <button
              className="ft-toolbar-btn"
              onClick={() => { const tour = createDashboardTabTour(); tour.drive(); }}
            >
              <Shield size={11} />
              <span>GUIDE</span>
            </button>

            <div style={{ width: '1px', height: '20px', backgroundColor: FC.BORDER }} />

            <button
              id="dashboard-add-widget"
              className="ft-toolbar-btn ft-toolbar-btn-primary"
              onClick={() => dispatch({ type: 'TOGGLE_MODAL', show: true })}
            >
              <Plus size={11} />
              <span>ADD</span>
            </button>
            <button
              id="dashboard-save"
              className="ft-toolbar-btn"
              onClick={saveLayout}
              disabled={status === 'saving'}
              style={status === 'saving' ? { opacity: 0.5, cursor: 'not-allowed' } : {}}
            >
              <Save size={11} />
              <span>{status === 'saving' ? 'SAVING...' : 'SAVE'}</span>
            </button>
            <button
              id="dashboard-reset"
              className="ft-toolbar-btn ft-toolbar-btn-danger"
              onClick={resetLayout}
            >
              <RotateCcw size={11} />
              <span>RESET</span>
            </button>
          </div>
        </div>
      </div>

      {/* === MARKET TICKER BAR === */}
      <MarketTickerBar />

      {/* === MAIN CONTENT AREA (Three-Panel Layout) === */}
      <div className="ft-dashboard" style={{
        flex: 1,
        display: 'flex',
        overflow: 'hidden',
      }}>
        {/* === CENTER: Widget Grid === */}
        <div
          ref={containerRef}
          style={{
            flex: 1,
            overflow: 'auto',
            padding: '4px',
            backgroundColor: FC.DARK_BG,
          }}
        >
          {widgets.length === 0 ? (
            <div style={{
              display: 'flex', flexDirection: 'column', alignItems: 'center',
              justifyContent: 'center', height: '100%', gap: '16px'
            }}>
              <Activity size={32} color={FC.MUTED} />
              <div style={{ color: FC.GRAY, fontSize: '11px', fontWeight: 700, letterSpacing: '1px' }}>
                NO WIDGETS CONFIGURED
              </div>
              <div style={{ color: FC.MUTED, fontSize: '9px' }}>
                Add widgets to build your custom terminal view
              </div>
              <button
                className="ft-toolbar-btn ft-toolbar-btn-primary"
                onClick={() => dispatch({ type: 'TOGGLE_MODAL', show: true })}
                style={{ padding: '8px 24px', fontSize: '10px' }}
              >
                <Plus size={12} /> ADD FIRST WIDGET
              </button>
            </div>
          ) : (
            <GridLayout
              className="layout"
              layout={widgets.map(w => w.layout)}
              cols={12}
              rowHeight={compactMode ? 50 : 60}
              width={containerRef.current ? containerRef.current.offsetWidth - 8 : containerWidth - 8}
              onLayoutChange={handleLayoutChange}
              draggableHandle=".widget-drag-handle"
              isDraggable={true}
              isResizable={true}
              compactType="vertical"
              preventCollision={false}
              margin={[4, 4]}
            >
              {widgets.map(widget => (
                <div key={widget.id} className={`widget-${widget.type}`}>
                  <ErrorBoundary name={widget.title || widget.type} variant="minimal">
                    {renderWidget(widget)}
                  </ErrorBoundary>
                </div>
              ))}
            </GridLayout>
          )}
        </div>

        {/* === RIGHT PANEL: Market Pulse === */}
        {!rightPanelCollapsed && (
          <div style={{
            width: '300px',
            flexShrink: 0,
            borderLeft: `1px solid ${FC.BORDER}`,
            backgroundColor: FC.PANEL_BG,
            overflow: 'hidden',
            display: 'flex',
            flexDirection: 'column',
          }}>
            <MarketPulsePanel onNavigateToTab={onNavigateToTab} />
          </div>
        )}
      </div>

      {/* === BOTTOM STATUS BAR === */}
      <div style={{
        backgroundColor: FC.HEADER_BG,
        borderTop: `1px solid ${FC.BORDER}`,
        padding: '3px 12px',
        fontSize: '9px',
        color: FC.GRAY,
        flexShrink: 0,
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
        fontFamily: '"IBM Plex Mono", "Consolas", monospace',
      }}>
        {/* Left Status */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          <span style={{ color: FC.ORANGE, fontWeight: 700 }}>FINCEPT TERMINAL v3.3.0</span>
          <span style={{ color: FC.BORDER }}>|</span>
          <span>SESSION: <span style={{ color: FC.CYAN }}>{formatUptime(sessionUptime)}</span></span>
          <span style={{ color: FC.BORDER }}>|</span>
          <span>LAYOUT: <span style={{ color: FC.GREEN }}>{widgets.length > 0 ? 'ACTIVE' : 'EMPTY'}</span></span>
          <span style={{ color: FC.BORDER }}>|</span>
          <span>FEEDS: <span style={{ color: isConnected ? FC.GREEN : FC.RED }}>{isConnected ? 'CONNECTED' : 'DISCONNECTED'}</span></span>
        </div>

        {/* Center: Data Quality Indicators */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          {[
            { label: 'EQ', color: FC.GREEN },
            { label: 'FX', color: FC.GREEN },
            { label: 'CM', color: FC.YELLOW },
            { label: 'CR', color: FC.GREEN },
            { label: 'FI', color: FC.GREEN },
          ].map(feed => (
            <div key={feed.label} style={{ display: 'flex', alignItems: 'center', gap: '3px' }}>
              <div style={{
                width: '4px', height: '4px', borderRadius: '50%',
                backgroundColor: feed.color,
              }} />
              <span style={{ color: feed.color, fontSize: '8px', fontWeight: 700 }}>{feed.label}</span>
            </div>
          ))}
        </div>

        {/* Right Status */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <span style={{ color: FC.MUTED }}>MEM: <span style={{ color: FC.CYAN }}>OPTIMAL</span></span>
          <span style={{ color: FC.BORDER }}>|</span>
          <span style={{ color: FC.MUTED }}>LAT: <span style={{ color: FC.GREEN }}>12ms</span></span>
          <span style={{ color: FC.BORDER }}>|</span>
          <Zap size={9} color={FC.ORANGE} />
          <span style={{ color: FC.ORANGE, fontWeight: 700 }}>READY</span>
        </div>
      </div>

      {/* Add Widget Modal */}
      <AddWidgetModal
        isOpen={showAddModal}
        onClose={() => dispatch({ type: 'TOGGLE_MODAL', show: false })}
        onAdd={handleAddWidget}
      />
    </div>
  );
};

export default DashboardTab;
