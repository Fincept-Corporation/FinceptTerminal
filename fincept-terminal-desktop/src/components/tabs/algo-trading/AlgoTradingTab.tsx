import React, { useState, useEffect } from 'react';
import {
  Bot, Wrench, Search, BarChart3, Layout, Activity, Library,
  Plus, Zap,
} from 'lucide-react';
import { listAlgoDeployments } from './services/algoTradingService';
import type { AlgoSubView, PythonStrategy } from './types';
import StrategyEditor from './components/StrategyEditor';
import StrategyManager from './components/StrategyManager';
import ScannerPanel from './components/ScannerPanel';
import MonitorDashboard from './components/MonitorDashboard';
import StrategyLibraryTab from './components/StrategyLibraryTab';
import PythonBacktestPanel from './components/PythonBacktestPanel';
import PythonStrategyEditor from './components/PythonStrategyEditor';
import PythonDeployPanel from './components/PythonDeployPanel';
import { F } from './constants/theme';

const font = '"IBM Plex Mono", "Consolas", monospace';

const NAV_ITEMS: { key: AlgoSubView; label: string; icon: React.ElementType }[] = [
  { key: 'builder', label: 'BUILDER', icon: Wrench },
  { key: 'strategies', label: 'MY STRATEGIES', icon: Layout },
  { key: 'library', label: 'LIBRARY', icon: Library },
  { key: 'scanner', label: 'SCANNER', icon: Search },
  { key: 'dashboard', label: 'DASHBOARD', icon: BarChart3 },
];

const AlgoTradingTab: React.FC = () => {
  const [activeView, setActiveView] = useState<AlgoSubView>('builder');
  const [editStrategyId, setEditStrategyId] = useState<string | null>(null);
  const [clonePythonStrategy, setClonePythonStrategy] = useState<PythonStrategy | null>(null);
  const [backtestPythonStrategy, setBacktestPythonStrategy] = useState<PythonStrategy | null>(null);
  const [deployPythonStrategy, setDeployPythonStrategy] = useState<PythonStrategy | null>(null);
  const [runningCount, setRunningCount] = useState(0);

  // Lightweight poll for status indicator (30s interval)
  useEffect(() => {
    const checkStatus = async () => {
      const result = await listAlgoDeployments();
      if (result.success && result.data) {
        setRunningCount(result.data.filter(d => d.status === 'running').length);
      }
    };
    checkStatus();
    const interval = setInterval(checkStatus, 30000);
    return () => clearInterval(interval);
  }, []);

  const handleEditStrategy = (id: string) => {
    setEditStrategyId(id);
    setActiveView('builder');
  };

  const handleNewStrategy = () => {
    setEditStrategyId(null);
    setActiveView('builder');
  };

  const handlePythonClone = (strategy: PythonStrategy) => {
    setClonePythonStrategy(strategy);
  };

  const handlePythonBacktest = (strategy: PythonStrategy) => {
    setBacktestPythonStrategy(strategy);
  };

  const handlePythonDeploy = (strategy: PythonStrategy) => {
    setDeployPythonStrategy(strategy);
  };

  const handlePythonEditorClose = () => {
    setClonePythonStrategy(null);
  };

  const handlePythonEditorSave = (_customId: string) => {
    setClonePythonStrategy(null);
    setActiveView('strategies');
  };

  const isBuilder = activeView === 'builder';

  return (
    <div style={{ width: '100%', height: '100%', display: 'flex', flexDirection: 'column', overflow: 'hidden', backgroundColor: F.DARK_BG, fontFamily: font, color: F.WHITE }}>

      {/* ═══════ UNIFIED TOP BAR ═══════ */}
      {/* Single-row header that merges logo/title, nav tabs, and contextual controls */}
      <div style={{ display: 'flex', alignItems: 'center', flexShrink: 0, backgroundColor: F.HEADER_BG, borderBottom: `2px solid ${F.ORANGE}`, boxShadow: `0 2px 8px ${F.ORANGE}20`, padding: '0 12px', height: '40px', gap: '0' }}>

        {/* Logo + Title (compact) */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', paddingRight: '12px', borderRight: `1px solid ${F.BORDER}`, height: '100%', flexShrink: 0 }}>
          <div style={{ width: '26px', height: '26px', borderRadius: '2px', display: 'flex', alignItems: 'center', justifyContent: 'center', backgroundColor: `${F.ORANGE}20` }}>
            <Bot size={14} style={{ color: F.ORANGE }} />
          </div>
          <div style={{ lineHeight: 1 }}>
            <div style={{ fontSize: '11px', fontWeight: 700, letterSpacing: '1px', color: F.WHITE }}>ALGO</div>
            <div style={{ fontSize: '8px', color: F.MUTED, letterSpacing: '0.5px' }}>TRADING</div>
          </div>
        </div>

        {/* Navigation Tabs */}
        <div style={{ display: 'flex', alignItems: 'stretch', height: '100%', gap: '0', flexShrink: 0 }}>
          {NAV_ITEMS.map(({ key, label, icon: Icon }) => {
            const isActive = activeView === key;
            const isDashboard = key === 'dashboard';
            return (
              <button
                key={key}
                onClick={() => setActiveView(key)}
                style={{
                  display: 'flex', alignItems: 'center', gap: '5px',
                  padding: '0 14px', border: 'none', cursor: 'pointer',
                  backgroundColor: isActive ? `${F.ORANGE}10` : 'transparent',
                  borderBottom: isActive ? `2px solid ${F.ORANGE}` : '2px solid transparent',
                  color: isActive ? F.ORANGE : F.GRAY,
                  fontSize: '10px', fontWeight: 700, letterSpacing: '0.5px',
                  fontFamily: font, transition: 'all 0.15s',
                }}
                onMouseEnter={e => { if (!isActive) { e.currentTarget.style.color = F.WHITE; e.currentTarget.style.backgroundColor = F.HOVER; } }}
                onMouseLeave={e => { if (!isActive) { e.currentTarget.style.color = F.GRAY; e.currentTarget.style.backgroundColor = 'transparent'; } }}
              >
                <Icon size={12} />
                {label}
                {isDashboard && runningCount > 0 && (
                  <span style={{ fontSize: '8px', fontWeight: 700, padding: '1px 5px', borderRadius: '2px', backgroundColor: F.GREEN, color: F.DARK_BG, marginLeft: '2px' }}>
                    {runningCount}
                  </span>
                )}
              </button>
            );
          })}
        </div>

        {/* Spacer */}
        <div style={{ flex: 1 }} />

        {/* Right-side contextual controls */}
        {runningCount > 0 && (
          <div style={{ display: 'flex', alignItems: 'center', gap: '5px', padding: '0 10px', marginRight: '8px' }}>
            <Activity size={10} style={{ color: F.GREEN }} />
            <span style={{ fontSize: '9px', fontWeight: 700, color: F.GREEN }}>{runningCount} LIVE</span>
          </div>
        )}

        {!isBuilder && (
          <button
            onClick={handleNewStrategy}
            style={{
              display: 'flex', alignItems: 'center', gap: '5px',
              padding: '5px 12px', border: 'none', borderRadius: '2px',
              backgroundColor: F.ORANGE, color: F.DARK_BG,
              fontSize: '9px', fontWeight: 700, letterSpacing: '0.5px',
              cursor: 'pointer', fontFamily: font, transition: 'opacity 0.15s',
            }}
            onMouseEnter={e => { e.currentTarget.style.opacity = '0.85'; }}
            onMouseLeave={e => { e.currentTarget.style.opacity = '1'; }}
          >
            <Plus size={12} />NEW
          </button>
        )}
      </div>

      {/* ─── VIEW CONTENT ─── */}
      <div style={{ flex: 1, overflow: 'hidden', minHeight: 0 }}>
        {activeView === 'builder' && (
          <StrategyEditor
            editStrategyId={editStrategyId}
            onSaved={() => setActiveView('strategies')}
            onCancel={() => {
              setEditStrategyId(null);
              setActiveView('strategies');
            }}
          />
        )}
        {activeView === 'strategies' && (
          <StrategyManager
            onEdit={handleEditStrategy}
            onNew={handleNewStrategy}
            onBacktestPython={handlePythonBacktest}
            onDeployPython={handlePythonDeploy}
            onDeployedJson={() => setActiveView('dashboard')}
          />
        )}
        {activeView === 'library' && (
          <StrategyLibraryTab
            onClone={handlePythonClone}
            onBacktest={handlePythonBacktest}
            onDeploy={handlePythonDeploy}
          />
        )}
        {activeView === 'scanner' && <ScannerPanel />}
        {activeView === 'dashboard' && <MonitorDashboard />}
      </div>

      {/* ─── STATUS BAR ─── */}
      <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', padding: '3px 16px', flexShrink: 0, backgroundColor: F.HEADER_BG, borderTop: `1px solid ${F.BORDER}`, fontSize: '9px', fontFamily: font, color: F.GRAY }}>
        <div style={{ display: 'flex', gap: '16px' }}>
          <span><span style={{ color: F.MUTED }}>ENGINE:</span> v1.0</span>
          <span>
            <span style={{ color: F.MUTED }}>DEPLOYMENTS:</span>{' '}
            <span style={{ color: runningCount > 0 ? F.GREEN : F.MUTED }}>{runningCount} active</span>
          </span>
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
          <Zap size={9} style={{ color: runningCount > 0 ? F.GREEN : F.MUTED }} />
          <span style={{ color: runningCount > 0 ? F.GREEN : F.MUTED }}>
            {runningCount > 0 ? 'ACTIVE' : 'IDLE'}
          </span>
        </div>
      </div>

      {/* ─── MODALS ─── */}
      {backtestPythonStrategy && (
        <PythonBacktestPanel
          strategy={backtestPythonStrategy}
          onClose={() => setBacktestPythonStrategy(null)}
        />
      )}

      {clonePythonStrategy && (
        <PythonStrategyEditor
          strategy={clonePythonStrategy}
          isCustom={false}
          onClose={handlePythonEditorClose}
          onSave={handlePythonEditorSave}
          onBacktest={handlePythonBacktest}
        />
      )}

      {deployPythonStrategy && (
        <PythonDeployPanel
          strategy={deployPythonStrategy}
          onClose={() => setDeployPythonStrategy(null)}
          onDeployed={() => {
            setDeployPythonStrategy(null);
            setActiveView('dashboard');
          }}
        />
      )}
    </div>
  );
};

export default AlgoTradingTab;
