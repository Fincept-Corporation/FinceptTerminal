/**
 * AI Quant Lab Tab - Bloomberg Professional Design
 * Complete Microsoft Qlib + RD-Agent integration
 * Full-featured quantitative research platform
 */

import React, { useState, useEffect } from 'react';
import {
  Brain,
  Zap,
  TrendingUp,
  Database,
  Bot,
  Play,
  Pause,
  RefreshCw,
  Settings,
  FileText,
  BarChart3,
  Activity,
  Sparkles,
  AlertCircle,
  CheckCircle2,
  Clock
} from 'lucide-react';
import { TabFooter } from '@/components/common/TabFooter';
import { qlibService } from '@/services/aiQuantLab/qlibService';
import { rdAgentService } from '@/services/aiQuantLab/rdAgentService';

// Sub-components
import { FactorDiscoveryPanel } from './FactorDiscoveryPanel';
import { ModelLibraryPanel } from './ModelLibraryPanel';
import { BacktestingPanel } from './BacktestingPanel';
import { LiveSignalsPanel } from './LiveSignalsPanel';
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

type ViewMode = 'factor_discovery' | 'model_library' | 'backtesting' | 'live_signals';

export default function AIQuantLabTab() {
  // State
  const [activeView, setActiveView] = useState<ViewMode>('factor_discovery');
  const [qlibStatus, setQlibStatus] = useState({ available: false, initialized: false });
  const [rdAgentStatus, setRDAgentStatus] = useState({ available: false, initialized: false });
  const [isLoading, setIsLoading] = useState(true);
  const [showSettings, setShowSettings] = useState(false);
  const [autoInitAttempted, setAutoInitAttempted] = useState(false);

  // Load status on mount and auto-initialize
  useEffect(() => {
    loadStatusAndAutoInit();
  }, []);

  const loadStatus = async () => {
    setIsLoading(true);
    try {
      // Check Qlib
      const qlibRes = await qlibService.checkStatus();
      setQlibStatus({
        available: qlibRes.qlib_available || false,
        initialized: qlibRes.initialized || false
      });

      // Check RD-Agent
      const rdAgentRes = await rdAgentService.checkStatus();
      setRDAgentStatus({
        available: rdAgentRes.rdagent_available || false,
        initialized: rdAgentRes.initialized || false
      });
    } catch (error) {
      console.error('[AI Quant Lab] Failed to load status:', error);
    } finally {
      setIsLoading(false);
    }
  };

  const loadStatusAndAutoInit = async () => {
    console.log('[AI Quant Lab] Starting loadStatusAndAutoInit...');
    setIsLoading(true);
    try {
      // Check Qlib
      console.log('[AI Quant Lab] Checking Qlib status...');
      const qlibRes = await qlibService.checkStatus();
      console.log('[AI Quant Lab] Qlib status response:', qlibRes);
      const qlibAvailable = qlibRes.qlib_available || false;
      const qlibInitialized = qlibRes.initialized || false;

      // Check RD-Agent
      console.log('[AI Quant Lab] Checking RD-Agent status...');
      const rdAgentRes = await rdAgentService.checkStatus();
      console.log('[AI Quant Lab] RD-Agent status response:', rdAgentRes);
      const rdAgentAvailable = rdAgentRes.rdagent_available || false;
      const rdAgentInitialized = rdAgentRes.initialized || false;

      // Auto-initialize if available but not initialized
      let needsReload = false;

      if (qlibAvailable && !qlibInitialized && !autoInitAttempted) {
        console.log('[AI Quant Lab] Auto-initializing Qlib...');
        try {
          const initResult = await qlibService.initialize();
          console.log('[AI Quant Lab] Qlib init result:', initResult);
          needsReload = true;
        } catch (error) {
          console.error('[AI Quant Lab] Auto-init Qlib failed:', error);
        }
      }

      if (rdAgentAvailable && !rdAgentInitialized && !autoInitAttempted) {
        console.log('[AI Quant Lab] Auto-initializing RD-Agent...');
        try {
          const initResult = await rdAgentService.initialize();
          console.log('[AI Quant Lab] RD-Agent init result:', initResult);
          needsReload = true;
        } catch (error) {
          console.error('[AI Quant Lab] Auto-init RD-Agent failed:', error);
        }
      }

      setAutoInitAttempted(true);

      // Reload status after auto-init
      if (needsReload) {
        console.log('[AI Quant Lab] Reloading status after auto-init...');
        const qlibRes2 = await qlibService.checkStatus();
        setQlibStatus({
          available: qlibRes2.qlib_available || false,
          initialized: qlibRes2.initialized || false
        });

        const rdAgentRes2 = await rdAgentService.checkStatus();
        setRDAgentStatus({
          available: rdAgentRes2.rdagent_available || false,
          initialized: rdAgentRes2.initialized || false
        });
      } else {
        setQlibStatus({
          available: qlibAvailable,
          initialized: qlibInitialized
        });
        setRDAgentStatus({
          available: rdAgentAvailable,
          initialized: rdAgentInitialized
        });
      }
    } catch (error) {
      console.error('[AI Quant Lab] Failed to load status and auto-init:', error);
    } finally {
      setIsLoading(false);
    }
  };

  const initializeServices = async () => {
    setIsLoading(true);
    try {
      // Initialize Qlib
      if (qlibStatus.available && !qlibStatus.initialized) {
        await qlibService.initialize();
      }

      // Initialize RD-Agent
      if (rdAgentStatus.available && !rdAgentStatus.initialized) {
        await rdAgentService.initialize();
      }

      // Reload status
      await loadStatus();
    } catch (error) {
      console.error('[AI Quant Lab] Failed to initialize:', error);
    } finally {
      setIsLoading(false);
    }
  };

  const navItems = [
    { id: 'factor_discovery', label: 'FACTOR DISCOVERY', icon: Sparkles, description: 'AI-powered factor mining with RD-Agent' },
    { id: 'model_library', label: 'MODEL LIBRARY', icon: Brain, description: '15+ pre-trained Qlib models' },
    { id: 'backtesting', label: 'BACKTESTING', icon: BarChart3, description: 'Strategy validation & analysis' },
    { id: 'live_signals', label: 'LIVE SIGNALS', icon: Activity, description: 'Real-time predictions' }
  ];

  const bothInitialized = qlibStatus.initialized && rdAgentStatus.initialized;
  const bothAvailable = qlibStatus.available && rdAgentStatus.available;

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
              borderLeft: `2px solid ${qlibStatus.initialized ? BLOOMBERG.GREEN : qlibStatus.available ? BLOOMBERG.YELLOW : BLOOMBERG.RED}`
            }}
          >
            {qlibStatus.initialized ? (
              <CheckCircle2 size={12} color={BLOOMBERG.GREEN} />
            ) : qlibStatus.available ? (
              <Clock size={12} color={BLOOMBERG.YELLOW} />
            ) : (
              <AlertCircle size={12} color={BLOOMBERG.RED} />
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
              borderLeft: `2px solid ${rdAgentStatus.initialized ? BLOOMBERG.GREEN : rdAgentStatus.available ? BLOOMBERG.YELLOW : BLOOMBERG.RED}`
            }}
          >
            {rdAgentStatus.initialized ? (
              <CheckCircle2 size={12} color={BLOOMBERG.GREEN} />
            ) : rdAgentStatus.available ? (
              <Clock size={12} color={BLOOMBERG.YELLOW} />
            ) : (
              <AlertCircle size={12} color={BLOOMBERG.RED} />
            )}
            <span style={{ color: rdAgentStatus.initialized ? BLOOMBERG.GREEN : BLOOMBERG.GRAY }}>
              RD-AGENT
            </span>
          </div>

          <div className="h-6 w-px" style={{ backgroundColor: BLOOMBERG.BORDER }} />

          {/* Refresh Button */}
          <button
            onClick={loadStatus}
            className="p-1.5 hover:bg-opacity-80 transition-colors"
            style={{ backgroundColor: 'transparent' }}
            disabled={isLoading}
            title="Refresh status"
          >
            <RefreshCw size={14} color={BLOOMBERG.GRAY} className={isLoading ? 'animate-spin' : ''} />
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

      {/* Main Content Area */}
      <div className="flex-1 overflow-hidden relative">
        {!bothAvailable ? (
          <InstallationGuide />
        ) : !bothInitialized ? (
          isLoading ? (
            <div className="flex items-center justify-center h-full" style={{ backgroundColor: BLOOMBERG.DARK_BG }}>
              <div className="text-center space-y-4">
                <RefreshCw size={48} color={BLOOMBERG.ORANGE} className="animate-spin mx-auto" />
                <h3 className="text-base font-bold tracking-wide uppercase" style={{ color: BLOOMBERG.WHITE }}>
                  INITIALIZING AI QUANT LAB
                </h3>
                <p className="text-sm font-mono" style={{ color: BLOOMBERG.GRAY }}>
                  Setting up Qlib and RD-Agent (this may take 10-30 seconds)
                </p>
                <div className="flex justify-center gap-6 mt-6">
                  <div className="flex items-center gap-2">
                    {qlibStatus.initialized ? (
                      <CheckCircle2 size={20} color={BLOOMBERG.GREEN} />
                    ) : (
                      <RefreshCw size={20} color={BLOOMBERG.ORANGE} className="animate-spin" />
                    )}
                    <span className="text-sm font-mono" style={{ color: qlibStatus.initialized ? BLOOMBERG.GREEN : BLOOMBERG.GRAY }}>
                      QLIB
                    </span>
                  </div>
                  <div className="flex items-center gap-2">
                    {rdAgentStatus.initialized ? (
                      <CheckCircle2 size={20} color={BLOOMBERG.GREEN} />
                    ) : (
                      <RefreshCw size={20} color={BLOOMBERG.ORANGE} className="animate-spin" />
                    )}
                    <span className="text-sm font-mono" style={{ color: rdAgentStatus.initialized ? BLOOMBERG.GREEN : BLOOMBERG.GRAY }}>
                      RD-AGENT
                    </span>
                  </div>
                </div>
              </div>
            </div>
          ) : (
            <InitializationPrompt onInitialize={initializeServices} isLoading={isLoading} />
          )
        ) : (
          <>
            {activeView === 'factor_discovery' && <FactorDiscoveryPanel />}
            {activeView === 'model_library' && <ModelLibraryPanel />}
            {activeView === 'backtesting' && <BacktestingPanel />}
            {activeView === 'live_signals' && <LiveSignalsPanel />}
          </>
        )}
      </div>

      {/* Status Bar */}
      <StatusBar qlibStatus={qlibStatus} rdAgentStatus={rdAgentStatus} />

      {/* Footer */}
      <TabFooter tabName="AI Quant Lab" />
    </div>
  );
}

// Installation Guide Component - Bloomberg Style
function InstallationGuide() {
  return (
    <div className="flex items-center justify-center h-full p-8" style={{ backgroundColor: BLOOMBERG.DARK_BG }}>
      <div className="max-w-3xl">
        {/* Header */}
        <div className="text-center mb-8">
          <div className="inline-flex items-center justify-center gap-3 mb-4">
            <Brain size={48} color={BLOOMBERG.ORANGE} />
            <div className="h-12 w-px" style={{ backgroundColor: BLOOMBERG.BORDER }} />
            <AlertCircle size={48} color={BLOOMBERG.RED} />
          </div>
          <h2 className="text-2xl font-bold uppercase tracking-wide mb-2" style={{ color: BLOOMBERG.WHITE }}>
            DEPENDENCIES NOT INSTALLED
          </h2>
          <p className="text-sm font-mono" style={{ color: BLOOMBERG.GRAY }}>
            AI Quant Lab requires Microsoft Qlib and RD-Agent Python packages
          </p>
        </div>

        {/* Installation Steps */}
        <div className="space-y-4">
          <div
            className="p-6 border-l-4"
            style={{ backgroundColor: BLOOMBERG.PANEL_BG, borderColor: BLOOMBERG.ORANGE }}
          >
            <div className="flex items-center gap-3 mb-3">
              <div className="flex items-center justify-center w-8 h-8 rounded-full font-bold text-sm"
                style={{ backgroundColor: BLOOMBERG.ORANGE, color: BLOOMBERG.DARK_BG }}>
                1
              </div>
              <h3 className="font-bold uppercase text-sm tracking-wide" style={{ color: BLOOMBERG.ORANGE }}>
                INSTALL MICROSOFT QLIB
              </h3>
            </div>
            <code
              className="block p-4 rounded text-sm font-mono mt-3"
              style={{ backgroundColor: BLOOMBERG.DARK_BG, color: BLOOMBERG.WHITE }}
            >
              pip install pyqlib
            </code>
            <p className="text-xs font-mono mt-2" style={{ color: BLOOMBERG.GRAY }}>
              Quantitative investment platform with 15+ pre-trained models
            </p>
          </div>

          <div
            className="p-6 border-l-4"
            style={{ backgroundColor: BLOOMBERG.PANEL_BG, borderColor: BLOOMBERG.CYAN }}
          >
            <div className="flex items-center gap-3 mb-3">
              <div className="flex items-center justify-center w-8 h-8 rounded-full font-bold text-sm"
                style={{ backgroundColor: BLOOMBERG.CYAN, color: BLOOMBERG.DARK_BG }}>
                2
              </div>
              <h3 className="font-bold uppercase text-sm tracking-wide" style={{ color: BLOOMBERG.CYAN }}>
                INSTALL RD-AGENT
              </h3>
            </div>
            <code
              className="block p-4 rounded text-sm font-mono mt-3"
              style={{ backgroundColor: BLOOMBERG.DARK_BG, color: BLOOMBERG.WHITE }}
            >
              pip install rdagent
            </code>
            <p className="text-xs font-mono mt-2" style={{ color: BLOOMBERG.GRAY }}>
              Autonomous AI agent for factor mining and model optimization
            </p>
          </div>

          <div
            className="p-6 border-l-4"
            style={{ backgroundColor: BLOOMBERG.PANEL_BG, borderColor: BLOOMBERG.GREEN }}
          >
            <div className="flex items-center gap-3 mb-3">
              <div className="flex items-center justify-center w-8 h-8 rounded-full font-bold text-sm"
                style={{ backgroundColor: BLOOMBERG.GREEN, color: BLOOMBERG.DARK_BG }}>
                3
              </div>
              <h3 className="font-bold uppercase text-sm tracking-wide" style={{ color: BLOOMBERG.GREEN }}>
                DOWNLOAD QLIB DATA (OPTIONAL)
              </h3>
            </div>
            <code
              className="block p-4 rounded text-sm font-mono mt-3 overflow-x-auto"
              style={{ backgroundColor: BLOOMBERG.DARK_BG, color: BLOOMBERG.WHITE }}
            >
              python -m qlib.run.get_data qlib_data --target_dir ~/.qlib/qlib_data/cn_data --region cn
            </code>
            <p className="text-xs font-mono mt-2" style={{ color: BLOOMBERG.GRAY }}>
              Download historical market data for China (CN) or US markets
            </p>
          </div>
        </div>

        {/* Reload Button */}
        <div className="mt-8 text-center">
          <button
            onClick={() => window.location.reload()}
            className="px-8 py-3 rounded font-bold uppercase tracking-wide text-sm hover:bg-opacity-90 transition-colors inline-flex items-center gap-2"
            style={{ backgroundColor: BLOOMBERG.ORANGE, color: BLOOMBERG.DARK_BG }}
          >
            <RefreshCw size={16} />
            RELOAD AFTER INSTALLATION
          </button>
        </div>
      </div>
    </div>
  );
}

// Initialization Prompt Component - Bloomberg Style
function InitializationPrompt({
  onInitialize,
  isLoading
}: {
  onInitialize: () => void;
  isLoading: boolean;
}) {
  return (
    <div className="flex items-center justify-center h-full p-8" style={{ backgroundColor: BLOOMBERG.DARK_BG }}>
      <div className="max-w-xl text-center">
        <Zap size={64} color={BLOOMBERG.ORANGE} className="mx-auto mb-6" />
        <h2 className="text-2xl font-bold uppercase tracking-wide mb-4" style={{ color: BLOOMBERG.WHITE }}>
          INITIALIZE AI QUANT LAB
        </h2>
        <p className="text-sm font-mono mb-8" style={{ color: BLOOMBERG.GRAY }}>
          Qlib and RD-Agent are installed but need to be initialized.<br/>
          This will set up data providers and configure the services.
        </p>
        <button
          onClick={onInitialize}
          disabled={isLoading}
          className="px-8 py-4 rounded font-bold uppercase tracking-wide hover:bg-opacity-90 transition-colors inline-flex items-center gap-3 disabled:opacity-50 disabled:cursor-not-allowed"
          style={{ backgroundColor: BLOOMBERG.ORANGE, color: BLOOMBERG.DARK_BG }}
        >
          {isLoading ? (
            <>
              <RefreshCw size={20} className="animate-spin" />
              INITIALIZING...
            </>
          ) : (
            <>
              <Play size={20} />
              INITIALIZE SERVICES
            </>
          )}
        </button>
      </div>
    </div>
  );
}
