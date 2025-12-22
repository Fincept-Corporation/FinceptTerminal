/**
 * AI Quant Lab Tab
 * Complete Microsoft Qlib + RD-Agent integration
 * Bloomberg-style professional design
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
  Sparkles
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

// Bloomberg Professional Color Palette - Matches TradingTab
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

  // Load status on mount
  useEffect(() => {
    loadStatus();
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
      console.error('Failed to load status:', error);
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
      console.error('Failed to initialize:', error);
    } finally {
      setIsLoading(false);
    }
  };

  const navItems = [
    { id: 'factor_discovery', label: 'Factor Discovery', icon: Sparkles, description: 'AI-powered factor mining' },
    { id: 'model_library', label: 'Model Library', icon: Brain, description: '20+ pre-trained models' },
    { id: 'backtesting', label: 'Backtesting', icon: BarChart3, description: 'Strategy validation' },
    { id: 'live_signals', label: 'Live Signals', icon: Activity, description: 'Real-time predictions' }
  ];

  return (
    <div className="flex flex-col h-full" style={{ backgroundColor: BLOOMBERG.DARK_BG }}>
      {/* Header */}
      <div
        className="flex items-center justify-between px-6 py-4 border-b"
        style={{ backgroundColor: BLOOMBERG.HEADER_BG, borderColor: BLOOMBERG.BORDER }}
      >
        <div className="flex items-center gap-3">
          <Brain size={24} color={BLOOMBERG.ORANGE} />
          <div>
            <h1 className="text-xl font-bold" style={{ color: BLOOMBERG.WHITE }}>
              AI QUANT LAB
            </h1>
            <p className="text-sm" style={{ color: BLOOMBERG.GRAY }}>
              Microsoft Qlib + RD-Agent • Autonomous Factor Mining & Model Library
            </p>
          </div>
        </div>

        <div className="flex items-center gap-3">
          {/* Status Indicators */}
          <div className="flex items-center gap-2 px-3 py-1.5 rounded" style={{ backgroundColor: BLOOMBERG.PANEL_BG }}>
            <div className={`w-2 h-2 rounded-full ${qlibStatus.available ? 'bg-green-500' : 'bg-red-500'}`} />
            <span className="text-xs" style={{ color: BLOOMBERG.GRAY }}>QLIB</span>
          </div>
          <div className="flex items-center gap-2 px-3 py-1.5 rounded" style={{ backgroundColor: BLOOMBERG.PANEL_BG }}>
            <div className={`w-2 h-2 rounded-full ${rdAgentStatus.available ? 'bg-green-500' : 'bg-red-500'}`} />
            <span className="text-xs" style={{ color: BLOOMBERG.GRAY }}>RD-AGENT</span>
          </div>

          {/* Actions */}
          <button
            onClick={loadStatus}
            className="p-2 rounded hover:bg-opacity-80 transition-colors"
            style={{ backgroundColor: BLOOMBERG.PANEL_BG }}
            disabled={isLoading}
          >
            <RefreshCw size={18} color={BLOOMBERG.ORANGE} className={isLoading ? 'animate-spin' : ''} />
          </button>
          <button
            onClick={() => setShowSettings(!showSettings)}
            className="p-2 rounded hover:bg-opacity-80 transition-colors"
            style={{ backgroundColor: BLOOMBERG.PANEL_BG }}
          >
            <Settings size={18} color={BLOOMBERG.GRAY} />
          </button>
        </div>
      </div>

      {/* Navigation Bar */}
      <div
        className="flex items-center gap-1 px-6 py-3 border-b"
        style={{ backgroundColor: BLOOMBERG.PANEL_BG, borderColor: BLOOMBERG.BORDER }}
      >
        {navItems.map((item) => {
          const Icon = item.icon;
          const isActive = activeView === item.id;

          return (
            <button
              key={item.id}
              onClick={() => setActiveView(item.id as ViewMode)}
              className="flex items-center gap-2 px-4 py-2 rounded transition-all hover:bg-opacity-80"
              style={{
                backgroundColor: isActive ? BLOOMBERG.ORANGE : 'transparent',
                color: isActive ? BLOOMBERG.DARK_BG : BLOOMBERG.GRAY,
                fontWeight: isActive ? '600' : '400',
              }}
              title={item.description}
            >
              <Icon size={18} />
              <span className="text-sm uppercase">{item.label}</span>
            </button>
          );
        })}
      </div>

      {/* Main Content Area */}
      <div className="flex-1 overflow-auto">
        {!qlibStatus.available && !rdAgentStatus.available ? (
          <InstallationGuide />
        ) : !qlibStatus.initialized || !rdAgentStatus.initialized ? (
          <InitializationPrompt onInitialize={initializeServices} isLoading={isLoading} />
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

// Installation Guide Component
function InstallationGuide() {
  return (
    <div className="flex items-center justify-center h-full p-8">
      <div className="max-w-2xl text-center space-y-6">
        <Brain size={64} color={BLOOMBERG.ORANGE} className="mx-auto" />
        <h2 className="text-2xl font-bold uppercase" style={{ color: BLOOMBERG.WHITE }}>
          Install AI Quant Lab Dependencies
        </h2>
        <p style={{ color: BLOOMBERG.GRAY }}>
          To use AI Quant Lab, you need to install Microsoft Qlib and RD-Agent Python packages.
        </p>
        <div className="space-y-4 text-left">
          <div className="p-4 rounded border" style={{ backgroundColor: BLOOMBERG.PANEL_BG, borderColor: BLOOMBERG.BORDER }}>
            <h3 className="font-semibold mb-2 uppercase" style={{ color: BLOOMBERG.ORANGE }}>1. Install Qlib</h3>
            <code className="block p-3 rounded text-sm font-mono" style={{ backgroundColor: BLOOMBERG.DARK_BG, color: BLOOMBERG.WHITE }}>
              pip install pyqlib
            </code>
          </div>
          <div className="p-4 rounded border" style={{ backgroundColor: BLOOMBERG.PANEL_BG, borderColor: BLOOMBERG.BORDER }}>
            <h3 className="font-semibold mb-2 uppercase" style={{ color: BLOOMBERG.ORANGE }}>2. Install RD-Agent</h3>
            <code className="block p-3 rounded text-sm font-mono" style={{ backgroundColor: BLOOMBERG.DARK_BG, color: BLOOMBERG.WHITE }}>
              pip install rdagent
            </code>
          </div>
          <div className="p-4 rounded border" style={{ backgroundColor: BLOOMBERG.PANEL_BG, borderColor: BLOOMBERG.BORDER }}>
            <h3 className="font-semibold mb-2 uppercase" style={{ color: BLOOMBERG.ORANGE }}>3. Download Qlib Data</h3>
            <code className="block p-3 rounded text-sm font-mono" style={{ backgroundColor: BLOOMBERG.DARK_BG, color: BLOOMBERG.WHITE }}>
              python -m qlib.run.get_data qlib_data --target_dir ~/.qlib/qlib_data/cn_data --region cn
            </code>
          </div>
        </div>
        <button
          onClick={() => window.location.reload()}
          className="px-6 py-3 rounded font-semibold uppercase hover:bg-opacity-90 transition-colors"
          style={{ backgroundColor: BLOOMBERG.ORANGE, color: BLOOMBERG.DARK_BG }}
        >
          Reload After Installation
        </button>
      </div>
    </div>
  );
}

// Initialization Prompt Component
function InitializationPrompt({
  onInitialize,
  isLoading
}: {
  onInitialize: () => void;
  isLoading: boolean;
}) {
  return (
    <div className="flex items-center justify-center h-full p-8">
      <div className="max-w-xl text-center space-y-6">
        <Zap size={64} color={BLOOMBERG.ORANGE} className="mx-auto" />
        <h2 className="text-2xl font-bold uppercase" style={{ color: BLOOMBERG.WHITE }}>
          Initialize AI Quant Lab
        </h2>
        <p style={{ color: BLOOMBERG.GRAY }}>
          Qlib and RD-Agent are installed but need to be initialized.
          This will set up data providers and configure the services.
        </p>
        <button
          onClick={onInitialize}
          disabled={isLoading}
          className="px-8 py-4 rounded-lg font-semibold uppercase hover:bg-opacity-90 transition-colors flex items-center gap-3 mx-auto"
          style={{ backgroundColor: BLOOMBERG.ORANGE, color: BLOOMBERG.DARK_BG }}
        >
          {isLoading ? (
            <>
              <RefreshCw size={20} className="animate-spin" />
              Initializing...
            </>
          ) : (
            <>
              <Play size={20} />
              Initialize Services
            </>
          )}
        </button>
      </div>
    </div>
  );
}
