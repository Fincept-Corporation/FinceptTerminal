/**
 * AI Quant Lab Tab - TERMINAL UI/UX (Crypto Trading Style)
 * Professional terminal-grade quantitative research platform
 * Grid layout with panels for maximum information density
 *
 * REFACTORED: Follows STATE_MANAGEMENT.md guidelines
 * - useReducer for atomic state updates
 * - AbortController for cleanup
 * - Simplified Python backend (check_availability.py)
 */

import React, { useState, useEffect, useReducer } from 'react';
import {
  Brain,
  Zap,
  TrendingUp,
  Database,
  RefreshCw,
  Settings,
  BarChart3,
  Activity,
  Sparkles,
  CheckCircle2,
  Clock,
  Sigma,
  Calculator,
  ChevronDown,
  ChevronUp,
  Minimize2,
  Maximize2,
  Target,
  Layers,
  AlertCircle,
  Cpu,
  Server
} from 'lucide-react';
import { TabFooter } from '@/components/common/TabFooter';

// Sub-components
import { FactorDiscoveryPanel } from './FactorDiscoveryPanel';
import { ModelLibraryPanel } from './ModelLibraryPanel';
import { BacktestingPanel } from './BacktestingPanel';
import { LiveSignalsPanel } from './LiveSignalsPanel';
import { FFNAnalyticsPanel } from './FFNAnalyticsPanel';
import { FunctimePanel } from './FunctimePanel';
import { FortitudoPanel } from './FortitudoPanel';
import { StatsmodelsPanel } from './StatsmodelsPanel';
import { CFAQuantPanel } from './CFAQuantPanel';
import { DeepAgentPanel, DeepAgentOutput, formatResultText } from './DeepAgentPanel';
import { RLTradingPanel } from './RLTradingPanel';
import { OnlineLearningPanel } from './OnlineLearningPanel';
import { HFTPanel } from './HFTPanel';
import { MetaLearningPanel } from './MetaLearningPanel';
import { RollingRetrainingPanel } from './RollingRetrainingPanel';
import { AdvancedModelsPanel } from './AdvancedModelsPanel';

// Fincept Terminal Color Palette
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
  MAGENTA: '#E91E8C',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A'
};

type ViewMode = 'factor_discovery' | 'model_library' | 'backtesting' | 'live_signals' | 'ffn_analytics' | 'functime' | 'fortitudo' | 'statsmodels' | 'cfa_quant' | 'deep_agent' | 'rl_trading' | 'online_learning' | 'hft' | 'meta_learning' | 'rolling_retraining' | 'advanced_models';

interface ModuleItem {
  id: ViewMode;
  label: string;
  shortLabel: string;
  icon: React.ComponentType<{ size?: number; color?: string }>;
  category: 'Core' | 'AI/ML' | 'Advanced' | 'Analytics';
  color: string;
}

// State management types
interface QlibStatus {
  available: boolean;
  version?: string;
  error?: string;
}

interface RDAgentStatus {
  available: boolean;
  error?: string | null;
  factor_loop_available: boolean;
  model_loop_available: boolean;
}

// State machine: idle → loading → ready | error
type SystemStatus =
  | { status: 'idle' }
  | { status: 'loading' }
  | { status: 'ready'; qlib: QlibStatus; rdagent: RDAgentStatus }
  | { status: 'error'; message: string };

interface AIQuantLabState {
  systemStatus: SystemStatus;
  activeView: ViewMode;
  showModuleDropdown: boolean;
  showSettings: boolean;
  isLeftPanelMinimized: boolean;
  isRightPanelMinimized: boolean;
  lastChecked: number | null;
}

// Action types for reducer
type AIQuantLabAction =
  | { type: 'START_LOADING' }
  | { type: 'LOAD_SUCCESS'; payload: { qlib: QlibStatus; rdagent: RDAgentStatus } }
  | { type: 'LOAD_ERROR'; payload: { message: string } }
  | { type: 'SET_ACTIVE_VIEW'; payload: ViewMode }
  | { type: 'TOGGLE_MODULE_DROPDOWN' }
  | { type: 'CLOSE_MODULE_DROPDOWN' }
  | { type: 'TOGGLE_SETTINGS' }
  | { type: 'TOGGLE_LEFT_PANEL' }
  | { type: 'TOGGLE_RIGHT_PANEL' };

// Reducer function
const initialState: AIQuantLabState = {
  systemStatus: {
    status: 'ready',
    qlib: { available: true }, // Assume available, operations will fail if not
    rdagent: { available: true, error: null, factor_loop_available: true, model_loop_available: true }
  },
  activeView: 'factor_discovery',
  showModuleDropdown: false,
  showSettings: false,
  isLeftPanelMinimized: false,
  isRightPanelMinimized: false,
  lastChecked: null,
};

function aiQuantLabReducer(state: AIQuantLabState, action: AIQuantLabAction): AIQuantLabState {
  switch (action.type) {
    case 'START_LOADING':
      return { ...state, systemStatus: { status: 'loading' } };

    case 'LOAD_SUCCESS':
      return {
        ...state,
        systemStatus: { status: 'ready', qlib: action.payload.qlib, rdagent: action.payload.rdagent },
        lastChecked: Date.now(),
      };

    case 'LOAD_ERROR':
      return {
        ...state,
        systemStatus: { status: 'error', message: action.payload.message },
        lastChecked: Date.now(),
      };

    case 'SET_ACTIVE_VIEW':
      return { ...state, activeView: action.payload, showModuleDropdown: false };

    case 'TOGGLE_MODULE_DROPDOWN':
      return { ...state, showModuleDropdown: !state.showModuleDropdown };

    case 'CLOSE_MODULE_DROPDOWN':
      return { ...state, showModuleDropdown: false };

    case 'TOGGLE_SETTINGS':
      return { ...state, showSettings: !state.showSettings };

    case 'TOGGLE_LEFT_PANEL':
      return { ...state, isLeftPanelMinimized: !state.isLeftPanelMinimized };

    case 'TOGGLE_RIGHT_PANEL':
      return { ...state, isRightPanelMinimized: !state.isRightPanelMinimized };

    default:
      return state;
  }
}

export default function AIQuantLabTab() {
  // State management with useReducer (atomic updates)
  const [state, dispatch] = useReducer(aiQuantLabReducer, initialState);
  const [currentTime, setCurrentTime] = useState(new Date());
  const [deepAgentOutput, setDeepAgentOutput] = useState<DeepAgentOutput | null>(null);

  // Modules organized by category
  const modules: ModuleItem[] = [
    // Core
    { id: 'factor_discovery', label: 'Factor Discovery', shortLabel: 'FACTOR', icon: Sparkles, category: 'Core', color: FINCEPT.PURPLE },
    { id: 'model_library', label: 'Model Library', shortLabel: 'MODELS', icon: Brain, category: 'Core', color: FINCEPT.BLUE },
    { id: 'backtesting', label: 'Backtesting', shortLabel: 'BACKTEST', icon: BarChart3, category: 'Core', color: FINCEPT.ORANGE },
    { id: 'live_signals', label: 'Live Signals', shortLabel: 'SIGNALS', icon: Activity, category: 'Core', color: FINCEPT.GREEN },

    // AI/ML
    { id: 'deep_agent', label: 'Deep Agent', shortLabel: 'AGENT', icon: Brain, category: 'AI/ML', color: FINCEPT.CYAN },
    { id: 'rl_trading', label: 'RL Trading', shortLabel: 'RL', icon: Target, category: 'AI/ML', color: FINCEPT.PURPLE },
    { id: 'online_learning', label: 'Online Learning', shortLabel: 'ONLINE', icon: Zap, category: 'AI/ML', color: FINCEPT.YELLOW },
    { id: 'meta_learning', label: 'Meta Learning', shortLabel: 'META', icon: Layers, category: 'AI/ML', color: FINCEPT.MAGENTA },

    // Advanced
    { id: 'hft', label: 'High Frequency Trading', shortLabel: 'HFT', icon: Activity, category: 'Advanced', color: FINCEPT.RED },
    { id: 'rolling_retraining', label: 'Rolling Retraining', shortLabel: 'RETRAIN', icon: RefreshCw, category: 'Advanced', color: FINCEPT.CYAN },
    { id: 'advanced_models', label: 'Advanced Models', shortLabel: 'ADV', icon: Brain, category: 'Advanced', color: FINCEPT.PURPLE },

    // Analytics
    { id: 'ffn_analytics', label: 'FFN Analytics', shortLabel: 'FFN', icon: TrendingUp, category: 'Analytics', color: FINCEPT.GREEN },
    { id: 'functime', label: 'Functime', shortLabel: 'FUNC', icon: Zap, category: 'Analytics', color: FINCEPT.YELLOW },
    { id: 'fortitudo', label: 'Fortitudo', shortLabel: 'FORT', icon: Database, category: 'Analytics', color: FINCEPT.BLUE },
    { id: 'statsmodels', label: 'Statsmodels', shortLabel: 'STATS', icon: Sigma, category: 'Analytics', color: FINCEPT.CYAN },
    { id: 'cfa_quant', label: 'CFA Quant', shortLabel: 'CFA', icon: Calculator, category: 'Analytics', color: FINCEPT.ORANGE }
  ];

  const activeModule = modules.find(m => m.id === state.activeView) || modules[0];

  // Clock update
  useEffect(() => {
    const timer = setInterval(() => setCurrentTime(new Date()), 1000);
    return () => clearInterval(timer);
  }, []);

  // Close dropdown on outside click
  useEffect(() => {
    if (state.showModuleDropdown) {
      const handleClick = () => dispatch({ type: 'CLOSE_MODULE_DROPDOWN' });
      document.addEventListener('click', handleClick);
      return () => document.removeEventListener('click', handleClick);
    }
  }, [state.showModuleDropdown]);

  return (
    <div className="flex flex-col h-full" style={{ backgroundColor: FINCEPT.DARK_BG, fontFamily: 'system-ui, -apple-system, sans-serif' }}>
      {/* ===== TOP NAVIGATION BAR ===== */}
      <div
        className="flex items-center justify-between px-3 py-2.5 border-b"
        style={{ backgroundColor: FINCEPT.HEADER_BG, borderColor: FINCEPT.BORDER, height: '48px' }}
      >
        {/* Left: Branding + Module Selector */}
        <div className="flex items-center gap-3">
          <div className="flex items-center gap-2 px-3 py-1.5" style={{ backgroundColor: FINCEPT.ORANGE }}>
            <Brain size={16} color={FINCEPT.DARK_BG} />
            <span className="font-bold text-xs tracking-wider" style={{ color: FINCEPT.DARK_BG }}>
              AI QUANT LAB
            </span>
          </div>

          <div className="h-5 w-px" style={{ backgroundColor: FINCEPT.BORDER }} />

          {/* Module Selector Dropdown */}
          <div className="relative">
            <button
              onClick={(e) => {
                e.stopPropagation();
                dispatch({ type: 'TOGGLE_MODULE_DROPDOWN' });
              }}
              className="flex items-center gap-2 px-3 py-1.5 text-xs font-semibold tracking-wide transition-colors"
              style={{
                backgroundColor: FINCEPT.PANEL_BG,
                color: activeModule.color,
                border: `1px solid ${FINCEPT.BORDER}`
              }}
              onMouseEnter={(e) => e.currentTarget.style.backgroundColor = FINCEPT.HOVER}
              onMouseLeave={(e) => e.currentTarget.style.backgroundColor = FINCEPT.PANEL_BG}
            >
              <activeModule.icon size={14} />
              <span>{activeModule.shortLabel}</span>
              <ChevronDown size={12} />
            </button>

            {/* Dropdown Menu */}
            {state.showModuleDropdown && (
              <div
                className="absolute top-full left-0 mt-1 z-50"
                style={{
                  backgroundColor: FINCEPT.PANEL_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  minWidth: '220px',
                  maxHeight: '400px',
                  overflowY: 'auto',
                  boxShadow: `0 4px 12px ${FINCEPT.DARK_BG}80`
                }}
              >
                {['Core', 'AI/ML', 'Advanced', 'Analytics'].map((category) => (
                  <div key={category}>
                    <div className="px-2 py-1.5 text-[10px] font-bold tracking-wider" style={{ color: FINCEPT.MUTED, backgroundColor: FINCEPT.DARK_BG }}>
                      {category}
                    </div>
                    {modules.filter(m => m.category === category).map((mod) => {
                      const ModIcon = mod.icon;
                      return (
                        <button
                          key={mod.id}
                          onClick={() => dispatch({ type: 'SET_ACTIVE_VIEW', payload: mod.id })}
                          className="w-full flex items-center gap-2 px-3 py-2 text-xs transition-colors"
                          style={{
                            backgroundColor: state.activeView === mod.id ? FINCEPT.HOVER : 'transparent',
                            color: state.activeView === mod.id ? mod.color : FINCEPT.GRAY,
                            borderLeft: state.activeView === mod.id ? `2px solid ${mod.color}` : '2px solid transparent'
                          }}
                          onMouseEnter={(e) => {
                            if (state.activeView !== mod.id) e.currentTarget.style.backgroundColor = `${FINCEPT.HOVER}80`;
                          }}
                          onMouseLeave={(e) => {
                            if (state.activeView !== mod.id) e.currentTarget.style.backgroundColor = 'transparent';
                          }}
                        >
                          <ModIcon size={14} />
                          <span className="flex-1 text-left">{mod.label}</span>
                        </button>
                      );
                    })}
                  </div>
                ))}
              </div>
            )}
          </div>

          <div className="text-[11px]" style={{ color: FINCEPT.GRAY }}>
            Microsoft Qlib + RD-Agent
          </div>
        </div>

        {/* Right: Status Indicators + Actions */}
        <div className="flex items-center gap-3">
          {/* Clock */}
          <div className="text-xs font-mono px-2" style={{ color: FINCEPT.GRAY }}>
            {currentTime.toLocaleTimeString('en-US', { hour12: false })}
          </div>

          <div className="h-5 w-px" style={{ backgroundColor: FINCEPT.BORDER }} />

          {/* System Status Badge */}
          {state.systemStatus.status === 'loading' && (
            <div className="flex items-center gap-1.5 px-2.5 py-1 text-xs font-mono" style={{ border: `1px solid ${FINCEPT.YELLOW}` }}>
              <RefreshCw size={12} color={FINCEPT.YELLOW} className="animate-spin" />
              <span style={{ color: FINCEPT.YELLOW }}>LOADING...</span>
            </div>
          )}
          {state.systemStatus.status === 'error' && (
            <div className="flex items-center gap-1.5 px-2.5 py-1 text-xs font-mono" style={{ border: `1px solid ${FINCEPT.RED}` }}>
              <AlertCircle size={12} color={FINCEPT.RED} />
              <span style={{ color: FINCEPT.RED }}>ERROR</span>
            </div>
          )}
          {state.systemStatus.status === 'ready' && (
            <>
              <div
                className="flex items-center gap-1.5 px-2.5 py-1 text-xs font-mono"
                style={{
                  backgroundColor: state.systemStatus.qlib.available ? `${FINCEPT.GREEN}20` : FINCEPT.DARK_BG,
                  border: `1px solid ${state.systemStatus.qlib.available ? FINCEPT.GREEN : FINCEPT.RED}`
                }}
              >
                {state.systemStatus.qlib.available ? (
                  <CheckCircle2 size={12} color={FINCEPT.GREEN} />
                ) : (
                  <AlertCircle size={12} color={FINCEPT.RED} />
                )}
                <span style={{ color: state.systemStatus.qlib.available ? FINCEPT.GREEN : FINCEPT.RED }}>
                  QLIB
                </span>
              </div>

              <div
                className="flex items-center gap-1.5 px-2.5 py-1 text-xs font-mono"
                style={{
                  backgroundColor: state.systemStatus.rdagent.available ? `${FINCEPT.CYAN}20` : FINCEPT.DARK_BG,
                  border: `1px solid ${state.systemStatus.rdagent.available ? FINCEPT.CYAN : FINCEPT.RED}`
                }}
              >
                {state.systemStatus.rdagent.available ? (
                  <CheckCircle2 size={12} color={FINCEPT.CYAN} />
                ) : (
                  <AlertCircle size={12} color={FINCEPT.RED} />
                )}
                <span style={{ color: state.systemStatus.rdagent.available ? FINCEPT.CYAN : FINCEPT.RED }}>
                  RD-AGENT
                </span>
              </div>
            </>
          )}

          {/* Settings Button */}
          <button
            onClick={() => dispatch({ type: 'TOGGLE_SETTINGS' })}
            className="p-1.5 transition-colors"
            style={{ backgroundColor: state.showSettings ? FINCEPT.HOVER : 'transparent' }}
            title="Settings"
            onMouseEnter={(e) => e.currentTarget.style.backgroundColor = FINCEPT.HOVER}
            onMouseLeave={(e) => e.currentTarget.style.backgroundColor = state.showSettings ? FINCEPT.HOVER : 'transparent'}
          >
            <Settings size={14} color={FINCEPT.GRAY} />
          </button>
        </div>
      </div>

      {/* ===== MAIN GRID LAYOUT ===== */}
      <div className="flex flex-1 overflow-hidden">
        {/* LEFT PANEL: Quick Navigation */}
        {!state.isLeftPanelMinimized && (
          <div
            className="flex flex-col border-r"
            style={{
              backgroundColor: FINCEPT.PANEL_BG,
              borderColor: FINCEPT.BORDER,
              width: '200px'
            }}
          >
            {/* Panel Header */}
            <div className="flex items-center justify-between px-3 py-2 border-b" style={{ borderColor: FINCEPT.BORDER }}>
              <span className="text-[11px] font-bold tracking-wider" style={{ color: FINCEPT.MUTED }}>
                MODULES
              </span>
              <button
                onClick={() => dispatch({ type: 'TOGGLE_LEFT_PANEL' })}
                className="p-0.5"
                title="Minimize panel"
              >
                <ChevronUp size={12} color={FINCEPT.GRAY} />
              </button>
            </div>

            {/* Module List */}
            <div className="flex-1 overflow-y-auto p-2">
              {['Core', 'AI/ML', 'Advanced', 'Analytics'].map((category) => (
                <div key={category} className="mb-3">
                  <div className="px-2 py-1 text-[10px] font-bold tracking-wider" style={{ color: FINCEPT.MUTED }}>
                    {category}
                  </div>
                  <div className="space-y-1">
                    {modules.filter(m => m.category === category).map((mod) => {
                      const ModIcon = mod.icon;
                      const isActive = state.activeView === mod.id;
                      return (
                        <button
                          key={mod.id}
                          onClick={() => dispatch({ type: 'SET_ACTIVE_VIEW', payload: mod.id })}
                          className="w-full flex items-center gap-2 px-2 py-1.5 text-xs transition-all"
                          style={{
                            backgroundColor: isActive ? `${mod.color}20` : 'transparent',
                            color: isActive ? mod.color : FINCEPT.GRAY,
                            borderLeft: isActive ? `2px solid ${mod.color}` : '2px solid transparent'
                          }}
                          onMouseEnter={(e) => {
                            if (!isActive) e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                          }}
                          onMouseLeave={(e) => {
                            if (!isActive) e.currentTarget.style.backgroundColor = 'transparent';
                          }}
                        >
                          <ModIcon size={13} />
                          <span className="flex-1 text-left truncate">{mod.label}</span>
                        </button>
                      );
                    })}
                  </div>
                </div>
              ))}
            </div>
          </div>
        )}

        {/* Left panel collapsed state */}
        {state.isLeftPanelMinimized && (
          <div
            className="flex flex-col items-center border-r"
            style={{
              backgroundColor: FINCEPT.PANEL_BG,
              borderColor: FINCEPT.BORDER,
              width: '32px'
            }}
          >
            <button
              onClick={() => dispatch({ type: 'TOGGLE_LEFT_PANEL' })}
              className="p-1 mt-1"
              title="Expand panel"
            >
              <ChevronDown size={12} color={FINCEPT.GRAY} />
            </button>
          </div>
        )}

        {/* CENTER: Main Content */}
        <div className="flex-1 flex flex-col overflow-hidden">
          {/* Content Header */}
          <div
            className="flex items-center justify-between px-3 py-2 border-b"
            style={{ backgroundColor: FINCEPT.PANEL_BG, borderColor: FINCEPT.BORDER }}
          >
            <div className="flex items-center gap-2">
              <activeModule.icon size={14} color={activeModule.color} />
              <span className="text-xs font-bold tracking-wide" style={{ color: FINCEPT.WHITE }}>
                {activeModule.label}
              </span>
            </div>
            <div className="text-[10px]" style={{ color: FINCEPT.MUTED }}>
              {activeModule.category}
            </div>
          </div>

          {/* Content Area */}
          <div className="flex-1 overflow-hidden">
            {state.activeView === 'factor_discovery' && <FactorDiscoveryPanel />}
            {state.activeView === 'deep_agent' && <DeepAgentPanel onOutputChange={setDeepAgentOutput} />}
            {state.activeView === 'model_library' && <ModelLibraryPanel />}
            {state.activeView === 'backtesting' && <BacktestingPanel />}
            {state.activeView === 'live_signals' && <LiveSignalsPanel />}
            {state.activeView === 'rl_trading' && <RLTradingPanel />}
            {state.activeView === 'online_learning' && <OnlineLearningPanel />}
            {state.activeView === 'hft' && <HFTPanel />}
            {state.activeView === 'meta_learning' && <MetaLearningPanel />}
            {state.activeView === 'rolling_retraining' && <RollingRetrainingPanel />}
            {state.activeView === 'advanced_models' && <AdvancedModelsPanel />}
            {state.activeView === 'ffn_analytics' && <FFNAnalyticsPanel />}
            {state.activeView === 'functime' && <FunctimePanel />}
            {state.activeView === 'fortitudo' && <FortitudoPanel />}
            {state.activeView === 'statsmodels' && <StatsmodelsPanel />}
            {state.activeView === 'cfa_quant' && <CFAQuantPanel />}
          </div>
        </div>

        {/* RIGHT PANEL: System Info */}
        {!state.isRightPanelMinimized && (
          <div
            className="flex flex-col border-l"
            style={{
              backgroundColor: FINCEPT.PANEL_BG,
              borderColor: FINCEPT.BORDER,
              width: state.activeView === 'deep_agent' ? '340px' : '220px',
              transition: 'width 0.2s ease'
            }}
          >
            {/* Panel Header */}
            <div className="flex items-center justify-between px-3 py-2 border-b" style={{ borderColor: FINCEPT.BORDER }}>
              <span className="text-[11px] font-bold tracking-wider" style={{ color: FINCEPT.MUTED }}>
                {state.activeView === 'deep_agent' ? 'AGENT OUTPUT' : 'SYSTEM STATUS'}
              </span>
              <button
                onClick={() => dispatch({ type: 'TOGGLE_RIGHT_PANEL' })}
                className="p-0.5"
                title="Minimize panel"
              >
                <ChevronUp size={12} color={FINCEPT.GRAY} />
              </button>
            </div>

            {/* Panel Content */}
            <div className="flex-1 overflow-y-auto p-3 space-y-3">
              {/* === DeepAgent Output (when deep_agent view is active) === */}
              {state.activeView === 'deep_agent' && (
                <>
                  {/* Execution status */}
                  {deepAgentOutput?.isExecuting && (
                    <div className="p-2.5" style={{ backgroundColor: FINCEPT.DARK_BG, border: `1px solid ${FINCEPT.YELLOW}` }}>
                      <div className="flex items-center gap-2">
                        <RefreshCw size={14} color={FINCEPT.YELLOW} className="animate-spin" />
                        <span className="text-xs font-mono" style={{ color: FINCEPT.YELLOW }}>Executing...</span>
                      </div>
                    </div>
                  )}

                  {/* Error */}
                  {deepAgentOutput?.error && (
                    <div className="p-2.5" style={{ backgroundColor: `${FINCEPT.RED}15`, border: `1px solid ${FINCEPT.RED}` }}>
                      <div className="flex items-center gap-2 mb-2">
                        <AlertCircle size={14} color={FINCEPT.RED} />
                        <span className="text-[11px] font-bold" style={{ color: FINCEPT.RED }}>ERROR</span>
                      </div>
                      <div className="text-[10px] font-mono" style={{ color: FINCEPT.GRAY, lineHeight: '1.4' }}>{deepAgentOutput.error}</div>
                    </div>
                  )}

                  {/* Execution Log */}
                  {deepAgentOutput && deepAgentOutput.executionLog.length > 0 && (
                    <div className="p-2.5" style={{ backgroundColor: FINCEPT.DARK_BG, border: `1px solid ${FINCEPT.CYAN}` }}>
                      <div className="flex items-center gap-2 mb-2">
                        <Cpu size={14} color={FINCEPT.CYAN} />
                        <span className="text-[11px] font-bold" style={{ color: FINCEPT.WHITE }}>Execution Log</span>
                      </div>
                      <div className="space-y-0.5" style={{ maxHeight: '120px', overflowY: 'auto' }}>
                        {deepAgentOutput.executionLog.map((log, idx) => (
                          <div key={idx} className="text-[10px] font-mono" style={{ color: FINCEPT.CYAN, lineHeight: '1.4' }}>
                            {log}
                          </div>
                        ))}
                      </div>
                    </div>
                  )}

                  {/* Result */}
                  {deepAgentOutput?.result && (
                    <div
                      className="p-2.5"
                      style={{
                        backgroundColor: FINCEPT.DARK_BG,
                        border: `1px solid ${FINCEPT.GREEN}`,
                        borderLeft: `3px solid ${FINCEPT.GREEN}`
                      }}
                    >
                      <div className="flex items-center gap-2 mb-2">
                        <Sparkles size={14} color={FINCEPT.GREEN} />
                        <span className="text-[11px] font-bold" style={{ color: FINCEPT.WHITE }}>Result</span>
                      </div>
                      <div className="text-[11px] font-mono" style={{ color: FINCEPT.WHITE, lineHeight: '1.6' }}>
                        {formatResultText(deepAgentOutput.result)}
                      </div>
                    </div>
                  )}

                  {/* Empty state */}
                  {!deepAgentOutput?.isExecuting && !deepAgentOutput?.result && !deepAgentOutput?.error && (
                    <div className="p-2.5" style={{ backgroundColor: FINCEPT.DARK_BG, border: `1px solid ${FINCEPT.BORDER}` }}>
                      <div className="flex items-center gap-2 mb-2">
                        <Brain size={14} color={FINCEPT.CYAN} />
                        <span className="text-[11px] font-bold" style={{ color: FINCEPT.WHITE }}>DeepAgent</span>
                      </div>
                      <div className="text-[10px]" style={{ color: FINCEPT.GRAY, lineHeight: '1.5' }}>
                        Select an agent type, enter a task, and click Execute to see results here.
                      </div>
                    </div>
                  )}
                </>
              )}

              {/* === System Status (for all other views) === */}
              {state.activeView !== 'deep_agent' && (
                <>
                  {/* Loading State */}
                  {state.systemStatus.status === 'loading' && (
                    <div className="p-2.5" style={{ backgroundColor: FINCEPT.DARK_BG, border: `1px solid ${FINCEPT.YELLOW}` }}>
                      <div className="flex items-center gap-2">
                        <RefreshCw size={14} color={FINCEPT.YELLOW} className="animate-spin" />
                        <span className="text-xs" style={{ color: FINCEPT.YELLOW }}>Loading...</span>
                      </div>
                    </div>
                  )}

                  {/* Error State */}
                  {state.systemStatus.status === 'error' && (
                    <div className="p-2.5" style={{ backgroundColor: `${FINCEPT.RED}15`, border: `1px solid ${FINCEPT.RED}` }}>
                      <div className="flex items-center gap-2 mb-2">
                        <AlertCircle size={14} color={FINCEPT.RED} />
                        <span className="text-[11px] font-bold" style={{ color: FINCEPT.RED }}>ERROR</span>
                      </div>
                      <div className="text-[10px]" style={{ color: FINCEPT.GRAY }}>{state.systemStatus.message}</div>
                    </div>
                  )}

                  {/* Ready State */}
                  {state.systemStatus.status === 'ready' && (
                    <>
                      {/* Qlib Status */}
                      <div
                        className="p-2.5"
                        style={{
                          backgroundColor: FINCEPT.DARK_BG,
                          border: `1px solid ${state.systemStatus.qlib.available ? FINCEPT.GREEN : FINCEPT.RED}`
                        }}
                      >
                        <div className="flex items-center gap-2 mb-2">
                          <Cpu size={14} color={state.systemStatus.qlib.available ? FINCEPT.GREEN : FINCEPT.RED} />
                          <span className="text-[11px] font-bold" style={{ color: FINCEPT.WHITE }}>
                            Microsoft Qlib
                          </span>
                        </div>
                        <div className="space-y-1">
                          <div className="flex justify-between text-[10px]">
                            <span style={{ color: FINCEPT.GRAY }}>Status:</span>
                            <span style={{ color: state.systemStatus.qlib.available ? FINCEPT.GREEN : FINCEPT.RED }}>
                              {state.systemStatus.qlib.available ? 'INSTALLED' : 'NOT INSTALLED'}
                            </span>
                          </div>
                          {state.systemStatus.qlib.version && (
                            <div className="flex justify-between text-[10px]">
                              <span style={{ color: FINCEPT.GRAY }}>Version:</span>
                              <span style={{ color: FINCEPT.WHITE }}>{state.systemStatus.qlib.version}</span>
                            </div>
                          )}
                          <div className="flex justify-between text-[10px]">
                            <span style={{ color: FINCEPT.GRAY }}>Models:</span>
                            <span style={{ color: FINCEPT.WHITE }}>15+</span>
                          </div>
                        </div>
                      </div>

                      {/* RD-Agent Status */}
                      <div
                        className="p-2.5"
                        style={{
                          backgroundColor: FINCEPT.DARK_BG,
                          border: `1px solid ${state.systemStatus.rdagent.available ? FINCEPT.CYAN : FINCEPT.RED}`
                        }}
                      >
                        <div className="flex items-center gap-2 mb-2">
                          <Server size={14} color={state.systemStatus.rdagent.available ? FINCEPT.CYAN : FINCEPT.RED} />
                          <span className="text-[11px] font-bold" style={{ color: FINCEPT.WHITE }}>
                            RD-Agent
                          </span>
                        </div>
                        <div className="space-y-1">
                          <div className="flex justify-between text-[10px]">
                            <span style={{ color: FINCEPT.GRAY }}>Status:</span>
                            <span style={{ color: state.systemStatus.rdagent.available ? FINCEPT.CYAN : FINCEPT.RED }}>
                              {state.systemStatus.rdagent.available ? 'INSTALLED' : 'NOT INSTALLED'}
                            </span>
                          </div>
                          <div className="flex justify-between text-[10px]">
                            <span style={{ color: FINCEPT.GRAY }}>Factor Loop:</span>
                            <span style={{ color: state.systemStatus.rdagent.factor_loop_available ? FINCEPT.GREEN : FINCEPT.RED }}>
                              {state.systemStatus.rdagent.factor_loop_available ? 'YES' : 'NO'}
                            </span>
                          </div>
                          <div className="flex justify-between text-[10px]">
                            <span style={{ color: FINCEPT.GRAY }}>Model Loop:</span>
                            <span style={{ color: state.systemStatus.rdagent.model_loop_available ? FINCEPT.GREEN : FINCEPT.RED }}>
                              {state.systemStatus.rdagent.model_loop_available ? 'YES' : 'NO'}
                            </span>
                          </div>
                        </div>
                      </div>
                    </>
                  )}

                  {/* Module Info */}
                  <div
                    className="p-2.5"
                    style={{
                      backgroundColor: FINCEPT.DARK_BG,
                      border: `1px solid ${activeModule.color}`
                    }}
                  >
                    <div className="flex items-center gap-2 mb-2">
                      <activeModule.icon size={14} color={activeModule.color} />
                      <span className="text-[11px] font-bold" style={{ color: FINCEPT.WHITE }}>
                        Active Module
                      </span>
                    </div>
                    <div className="space-y-1">
                      <div className="flex justify-between text-[10px]">
                        <span style={{ color: FINCEPT.GRAY }}>Name:</span>
                        <span style={{ color: activeModule.color }}>{activeModule.shortLabel}</span>
                      </div>
                      <div className="flex justify-between text-[10px]">
                        <span style={{ color: FINCEPT.GRAY }}>Category:</span>
                        <span style={{ color: FINCEPT.WHITE }}>{activeModule.category}</span>
                      </div>
                    </div>
                  </div>

                  {/* Quick Actions */}
                  <div className="pt-2 border-t" style={{ borderColor: FINCEPT.BORDER }}>
                    <div className="text-[10px] font-bold tracking-wider mb-2" style={{ color: FINCEPT.MUTED }}>
                      QUICK ACTIONS
                    </div>
                    <div className="space-y-1.5">
                      <button
                        onClick={() => dispatch({ type: 'TOGGLE_SETTINGS' })}
                        className="w-full flex items-center gap-2 px-2.5 py-1.5 text-[10px] transition-colors"
                        style={{
                          backgroundColor: FINCEPT.DARK_BG,
                          border: `1px solid ${FINCEPT.BORDER}`,
                          color: FINCEPT.WHITE
                        }}
                        onMouseEnter={(e) => e.currentTarget.style.backgroundColor = FINCEPT.HOVER}
                        onMouseLeave={(e) => e.currentTarget.style.backgroundColor = FINCEPT.DARK_BG}
                      >
                        <Settings size={12} />
                        <span>Configuration</span>
                      </button>
                    </div>
                  </div>
                </>
              )}
            </div>
          </div>
        )}

        {/* Right panel collapsed state */}
        {state.isRightPanelMinimized && (
          <div
            className="flex flex-col items-center border-l"
            style={{
              backgroundColor: FINCEPT.PANEL_BG,
              borderColor: FINCEPT.BORDER,
              width: '32px'
            }}
          >
            <button
              onClick={() => dispatch({ type: 'TOGGLE_RIGHT_PANEL' })}
              className="p-1 mt-1"
              title="Expand panel"
            >
              <ChevronDown size={12} color={FINCEPT.GRAY} />
            </button>
          </div>
        )}
      </div>

      {/* ===== BOTTOM STATUS BAR ===== */}
      <div
        className="flex items-center justify-between px-4 py-1.5 border-t text-[10px] font-mono"
        style={{
          backgroundColor: FINCEPT.HEADER_BG,
          borderColor: FINCEPT.BORDER,
          height: '28px'
        }}
      >
        <div className="flex items-center gap-4">
          <span style={{ color: FINCEPT.MUTED }}>Fincept AI Quant Lab v3.2.0</span>
          <span style={{ color: FINCEPT.GRAY }}>•</span>
          <span style={{ color: FINCEPT.GRAY }}>
            Qlib: {state.systemStatus.status === 'ready' && state.systemStatus.qlib.available ? (
              <span style={{ color: FINCEPT.GREEN }}>INSTALLED</span>
            ) : (
              <span style={{ color: state.systemStatus.status === 'loading' ? FINCEPT.YELLOW : FINCEPT.RED }}>
                {state.systemStatus.status === 'loading' ? 'LOADING...' : 'NOT INSTALLED'}
              </span>
            )}
          </span>
          <span style={{ color: FINCEPT.GRAY }}>•</span>
          <span style={{ color: FINCEPT.GRAY }}>
            RD-Agent: {state.systemStatus.status === 'ready' && state.systemStatus.rdagent.available ? (
              <span style={{ color: FINCEPT.CYAN }}>INSTALLED</span>
            ) : (
              <span style={{ color: state.systemStatus.status === 'loading' ? FINCEPT.YELLOW : FINCEPT.RED }}>
                {state.systemStatus.status === 'loading' ? 'LOADING...' : 'NOT INSTALLED'}
              </span>
            )}
          </span>
        </div>
        <div className="flex items-center gap-3">
          <span style={{ color: FINCEPT.GRAY }}>Module: <span style={{ color: activeModule.color }}>{activeModule.shortLabel}</span></span>
          <span style={{ color: FINCEPT.GRAY }}>•</span>
          <span style={{ color: FINCEPT.GRAY }}>{currentTime.toLocaleDateString()}</span>
        </div>
      </div>

      {/* Footer (hidden in terminal mode) */}
      <div style={{ display: 'none' }}>
        <TabFooter tabName="AI Quant Lab" />
      </div>
    </div>
  );
}
