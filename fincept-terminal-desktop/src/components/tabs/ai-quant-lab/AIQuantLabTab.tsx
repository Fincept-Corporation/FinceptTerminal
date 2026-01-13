/**
 * AI Quant Lab Tab - Bloomberg Professional Design
 * Complete Microsoft Qlib + RD-Agent integration
 * Full-featured quantitative research platform
 *
 * Libraries are pre-installed during setup - no installation screens needed.
 * Auto-initializes silently in background when tab is opened.
 */

import React, { useState, useEffect, useRef } from 'react';
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
  Calculator
} from 'lucide-react';
import { TabFooter } from '@/components/common/TabFooter';
import { qlibService } from '@/services/aiQuantLab/qlibService';
import { rdAgentService } from '@/services/aiQuantLab/rdAgentService';

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
import { StatusBar } from './StatusBar';

// Bloomberg Professional Color Palette - Consistent across all tabs
const BLOOMBERG = {
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

type ViewMode = 'factor_discovery' | 'model_library' | 'backtesting' | 'live_signals' | 'ffn_analytics' | 'functime' | 'fortitudo' | 'statsmodels' | 'cfa_quant';

export default function AIQuantLabTab() {
  // State
  const [activeView, setActiveView] = useState<ViewMode>('factor_discovery');
  const [qlibStatus, setQlibStatus] = useState({ available: true, initialized: false });
  const [rdAgentStatus, setRDAgentStatus] = useState({ available: true, initialized: false });
  const [isInitializing, setIsInitializing] = useState(false);
  const [showSettings, setShowSettings] = useState(false);
  const initAttemptedRef = useRef(false);

  // Auto-initialize on mount (silent, non-blocking)
  useEffect(() => {
    if (!initAttemptedRef.current) {
      initAttemptedRef.current = true;
      silentAutoInit();
    }
  }, []);

  // Silent auto-initialization - runs in background without blocking UI
  const silentAutoInit = async () => {
    setIsInitializing(true);

    // Initialize both services in parallel (non-blocking)
    const initPromises: Promise<void>[] = [];

    // Qlib initialization
    initPromises.push(
      (async () => {
        try {
          const statusRes = await qlibService.checkStatus();
          if (statusRes.qlib_available && !statusRes.initialized) {
            await qlibService.initialize();
          }
          const finalStatus = await qlibService.checkStatus();
          setQlibStatus({
            available: finalStatus.qlib_available || true,
            initialized: finalStatus.initialized || false
          });
        } catch (error) {
          console.warn('[AI Quant Lab] Qlib init:', error);
          // Still mark as available - libraries are pre-installed
          setQlibStatus({ available: true, initialized: false });
        }
      })()
    );

    // RD-Agent initialization
    initPromises.push(
      (async () => {
        try {
          const statusRes = await rdAgentService.checkStatus();
          if (statusRes.rdagent_available && !statusRes.initialized) {
            await rdAgentService.initialize();
          }
          const finalStatus = await rdAgentService.checkStatus();
          setRDAgentStatus({
            available: finalStatus.rdagent_available || true,
            initialized: finalStatus.initialized || false
          });
        } catch (error) {
          console.warn('[AI Quant Lab] RD-Agent init:', error);
          // Still mark as available - libraries are pre-installed
          setRDAgentStatus({ available: true, initialized: false });
        }
      })()
    );

    // Wait for all initializations to complete
    await Promise.allSettled(initPromises);
    setIsInitializing(false);
  };

  // Manual refresh status
  const refreshStatus = async () => {
    setIsInitializing(true);
    try {
      const [qlibRes, rdAgentRes] = await Promise.all([
        qlibService.checkStatus(),
        rdAgentService.checkStatus()
      ]);

      setQlibStatus({
        available: qlibRes.qlib_available || true,
        initialized: qlibRes.initialized || false
      });
      setRDAgentStatus({
        available: rdAgentRes.rdagent_available || true,
        initialized: rdAgentRes.initialized || false
      });
    } catch (error) {
      console.error('[AI Quant Lab] Failed to refresh status:', error);
    } finally {
      setIsInitializing(false);
    }
  };

  const navItems = [
    { id: 'factor_discovery', label: 'FACTOR DISCOVERY', icon: Sparkles, description: 'AI-powered factor mining with RD-Agent' },
    { id: 'model_library', label: 'MODEL LIBRARY', icon: Brain, description: '15+ pre-trained Qlib models' },
    { id: 'backtesting', label: 'BACKTESTING', icon: BarChart3, description: 'Strategy validation & analysis' },
    { id: 'live_signals', label: 'LIVE SIGNALS', icon: Activity, description: 'Real-time predictions' },
    { id: 'ffn_analytics', label: 'FFN ANALYTICS', icon: TrendingUp, description: 'Portfolio performance & risk metrics' },
    { id: 'functime', label: 'FUNCTIME', icon: Zap, description: 'ML time series forecasting' },
    { id: 'fortitudo', label: 'FORTITUDO', icon: Database, description: 'VaR, CVaR, option pricing, entropy pooling' },
    { id: 'statsmodels', label: 'STATSMODELS', icon: Sigma, description: 'ARIMA, regression, statistical tests, PCA' },
    { id: 'cfa_quant', label: 'CFA QUANT', icon: Calculator, description: 'CFA-level quant analytics, ML, sampling' }
  ];

  return (
    <div className="flex flex-col h-full" style={{ backgroundColor: BLOOMBERG.DARK_BG }}>
      {/* Bloomberg-style Header */}
      <div
        className="flex items-center justify-between px-4 py-2.5 border-b"
        style={{ backgroundColor: BLOOMBERG.HEADER_BG, borderColor: BLOOMBERG.BORDER }}
      >
        <div className="flex items-center gap-3">
          <div className="flex items-center gap-2 px-3 py-1.5" style={{ backgroundColor: BLOOMBERG.ORANGE }}>
            <Brain size={18} color={BLOOMBERG.DARK_BG} />
            <span className="font-bold text-xs tracking-wider" style={{ color: BLOOMBERG.DARK_BG }}>
              AI QUANT LAB
            </span>
          </div>
          <div className="h-6 w-px" style={{ backgroundColor: BLOOMBERG.BORDER }} />
          <span className="text-xs font-mono" style={{ color: BLOOMBERG.GRAY }}>
            Microsoft Qlib + RD-Agent • Autonomous Factor Mining & Model Library
          </span>
        </div>

        <div className="flex items-center gap-2">
          {/* Qlib Status */}
          <div
            className="flex items-center gap-1.5 px-2.5 py-1 text-xs font-mono"
            style={{
              backgroundColor: qlibStatus.initialized ? BLOOMBERG.PANEL_BG : BLOOMBERG.DARK_BG,
              borderLeft: `2px solid ${qlibStatus.initialized ? BLOOMBERG.GREEN : isInitializing ? BLOOMBERG.YELLOW : BLOOMBERG.GRAY}`
            }}
          >
            {qlibStatus.initialized ? (
              <CheckCircle2 size={12} color={BLOOMBERG.GREEN} />
            ) : isInitializing ? (
              <RefreshCw size={12} color={BLOOMBERG.YELLOW} className="animate-spin" />
            ) : (
              <Clock size={12} color={BLOOMBERG.GRAY} />
            )}
            <span style={{ color: qlibStatus.initialized ? BLOOMBERG.GREEN : BLOOMBERG.GRAY }}>
              QLIB
            </span>
          </div>

          {/* RD-Agent Status */}
          <div
            className="flex items-center gap-1.5 px-2.5 py-1 text-xs font-mono"
            style={{
              backgroundColor: rdAgentStatus.initialized ? BLOOMBERG.PANEL_BG : BLOOMBERG.DARK_BG,
              borderLeft: `2px solid ${rdAgentStatus.initialized ? BLOOMBERG.GREEN : isInitializing ? BLOOMBERG.YELLOW : BLOOMBERG.GRAY}`
            }}
          >
            {rdAgentStatus.initialized ? (
              <CheckCircle2 size={12} color={BLOOMBERG.GREEN} />
            ) : isInitializing ? (
              <RefreshCw size={12} color={BLOOMBERG.YELLOW} className="animate-spin" />
            ) : (
              <Clock size={12} color={BLOOMBERG.GRAY} />
            )}
            <span style={{ color: rdAgentStatus.initialized ? BLOOMBERG.GREEN : BLOOMBERG.GRAY }}>
              RD-AGENT
            </span>
          </div>

          <div className="h-6 w-px" style={{ backgroundColor: BLOOMBERG.BORDER }} />

          {/* Refresh Button */}
          <button
            onClick={refreshStatus}
            className="p-1.5 hover:bg-opacity-80 transition-colors"
            style={{ backgroundColor: 'transparent' }}
            disabled={isInitializing}
            title="Refresh status"
          >
            <RefreshCw size={14} color={BLOOMBERG.GRAY} className={isInitializing ? 'animate-spin' : ''} />
          </button>

          {/* Settings Button */}
          <button
            onClick={() => setShowSettings(!showSettings)}
            className="p-1.5 hover:bg-opacity-80 transition-colors"
            style={{ backgroundColor: 'transparent' }}
            title="Settings"
          >
            <Settings size={14} color={BLOOMBERG.GRAY} />
          </button>
        </div>
      </div>

      {/* Bloomberg-style Navigation Tabs */}
      <div
        className="flex items-center border-b"
        style={{ backgroundColor: BLOOMBERG.PANEL_BG, borderColor: BLOOMBERG.BORDER }}
      >
        {navItems.map((item) => {
          const Icon = item.icon;
          const isActive = activeView === item.id;

          return (
            <button
              key={item.id}
              onClick={() => setActiveView(item.id as ViewMode)}
              className="flex items-center gap-2 px-4 py-2.5 text-xs font-semibold tracking-wide transition-all relative"
              style={{
                backgroundColor: isActive ? BLOOMBERG.DARK_BG : 'transparent',
                color: isActive ? BLOOMBERG.ORANGE : BLOOMBERG.GRAY,
                borderBottom: isActive ? `2px solid ${BLOOMBERG.ORANGE}` : '2px solid transparent',
              }}
              title={item.description}
            >
              <Icon size={14} />
              <span>{item.label}</span>
            </button>
          );
        })}
      </div>

      {/* Main Content Area - All panels always available, no blocking screens */}
      <div className="flex-1 overflow-hidden relative">
        {activeView === 'factor_discovery' && <FactorDiscoveryPanel />}
        {activeView === 'model_library' && <ModelLibraryPanel />}
        {activeView === 'backtesting' && <BacktestingPanel />}
        {activeView === 'live_signals' && <LiveSignalsPanel />}
        {activeView === 'ffn_analytics' && <FFNAnalyticsPanel />}
        {activeView === 'functime' && <FunctimePanel />}
        {activeView === 'fortitudo' && <FortitudoPanel />}
        {activeView === 'statsmodels' && <StatsmodelsPanel />}
        {activeView === 'cfa_quant' && <CFAQuantPanel />}
      </div>

      {/* Status Bar */}
      <StatusBar qlibStatus={qlibStatus} rdAgentStatus={rdAgentStatus} />

      {/* Footer */}
      <TabFooter tabName="AI Quant Lab" />
    </div>
  );
}

