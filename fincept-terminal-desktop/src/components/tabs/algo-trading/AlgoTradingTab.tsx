import React, { useState, useEffect } from 'react';
import {
  Bot, Wrench, Search, BarChart3, Layout, Activity, Library,
  Plus, Zap, ChevronRight,
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

const NAV_ITEMS: { key: AlgoSubView; label: string; icon: React.ElementType; description: string }[] = [
  { key: 'builder', label: 'STRATEGY BUILDER', icon: Wrench, description: 'Create & edit strategies' },
  { key: 'strategies', label: 'MY STRATEGIES', icon: Layout, description: 'Saved strategies & custom' },
  { key: 'library', label: 'STRATEGY LIBRARY', icon: Library, description: 'Pre-built Python strategies' },
  { key: 'scanner', label: 'MARKET SCANNER', icon: Search, description: 'Scan across symbols' },
  { key: 'dashboard', label: 'LIVE DASHBOARD', icon: BarChart3, description: 'Monitor deployments' },
];

const AlgoTradingTab: React.FC = () => {
  const [activeView, setActiveView] = useState<AlgoSubView>('builder');
  const [editStrategyId, setEditStrategyId] = useState<string | null>(null);
  const [clonePythonStrategy, setClonePythonStrategy] = useState<PythonStrategy | null>(null);
  const [backtestPythonStrategy, setBacktestPythonStrategy] = useState<PythonStrategy | null>(null);
  const [deployPythonStrategy, setDeployPythonStrategy] = useState<PythonStrategy | null>(null);
  const [runningCount, setRunningCount] = useState(0);

  // Lightweight poll just for the sidebar status indicator (30s interval, not 5s)
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

  return (
    <div style={{
      display: 'flex',
      height: '100%',
      backgroundColor: F.DARK_BG,
      fontFamily: '"IBM Plex Mono", "Consolas", monospace',
      color: F.WHITE,
    }}>
      {/* ─── LEFT SIDEBAR NAVIGATION ─── */}
      <div style={{
        width: '220px',
        backgroundColor: F.PANEL_BG,
        borderRight: `1px solid ${F.BORDER}`,
        display: 'flex',
        flexDirection: 'column',
        flexShrink: 0,
      }}>
        {/* Logo / Header */}
        <div style={{
          padding: '16px',
          borderBottom: `1px solid ${F.BORDER}`,
          display: 'flex',
          alignItems: 'center',
          gap: '10px',
        }}>
          <div style={{
            width: '32px',
            height: '32px',
            borderRadius: '2px',
            backgroundColor: `${F.ORANGE}20`,
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
          }}>
            <Bot size={18} style={{ color: F.ORANGE }} />
          </div>
          <div>
            <div style={{ fontSize: '11px', fontWeight: 700, letterSpacing: '1px' }}>
              ALGO TRADING
            </div>
            <div style={{ fontSize: '8px', color: F.MUTED, marginTop: '2px' }}>
              STRATEGY ENGINE v1.0
            </div>
          </div>
        </div>

        {/* Quick Action */}
        <div style={{ padding: '12px' }}>
          <button
            onClick={handleNewStrategy}
            style={{
              width: '100%',
              padding: '10px 12px',
              backgroundColor: F.ORANGE,
              color: F.DARK_BG,
              border: 'none',
              borderRadius: '2px',
              fontSize: '9px',
              fontWeight: 700,
              letterSpacing: '0.5px',
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              gap: '6px',
              transition: 'all 0.2s',
            }}
            onMouseEnter={e => { e.currentTarget.style.opacity = '0.9'; }}
            onMouseLeave={e => { e.currentTarget.style.opacity = '1'; }}
          >
            <Plus size={12} />
            NEW STRATEGY
          </button>
        </div>

        {/* Navigation Items */}
        <div style={{ flex: 1, overflowY: 'auto', padding: '4px 8px' }}>
          {NAV_ITEMS.map(({ key, label, icon: Icon, description }) => {
            const isActive = activeView === key;
            return (
              <button
                key={key}
                onClick={() => setActiveView(key)}
                style={{
                  width: '100%',
                  padding: '10px 12px',
                  backgroundColor: isActive ? `${F.ORANGE}15` : 'transparent',
                  borderLeft: isActive ? `2px solid ${F.ORANGE}` : '2px solid transparent',
                  border: 'none',
                  borderRadius: '2px',
                  cursor: 'pointer',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '10px',
                  marginBottom: '2px',
                  transition: 'all 0.2s',
                  textAlign: 'left',
                }}
                onMouseEnter={e => {
                  if (!isActive) e.currentTarget.style.backgroundColor = F.HOVER;
                }}
                onMouseLeave={e => {
                  if (!isActive) e.currentTarget.style.backgroundColor = 'transparent';
                }}
              >
                <Icon size={14} style={{ color: isActive ? F.ORANGE : F.MUTED, flexShrink: 0 }} />
                <div style={{ flex: 1, minWidth: 0 }}>
                  <div style={{
                    fontSize: '9px',
                    fontWeight: 700,
                    letterSpacing: '0.5px',
                    color: isActive ? F.WHITE : F.GRAY,
                  }}>
                    {label}
                  </div>
                  <div style={{
                    fontSize: '8px',
                    color: F.MUTED,
                    marginTop: '2px',
                  }}>
                    {description}
                  </div>
                </div>
                {isActive && <ChevronRight size={10} style={{ color: F.ORANGE }} />}
              </button>
            );
          })}
        </div>

        {/* Sidebar Footer - Status */}
        <div style={{
          padding: '12px 16px',
          borderTop: `1px solid ${F.BORDER}`,
        }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '6px', marginBottom: '8px' }}>
            <Zap size={10} style={{ color: F.YELLOW }} />
            <span style={{ fontSize: '8px', color: F.GRAY, letterSpacing: '0.5px' }}>DEPLOYMENTS</span>
          </div>
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
              <span style={{
                width: '6px', height: '6px', borderRadius: '50%',
                backgroundColor: runningCount > 0 ? F.GREEN : F.MUTED,
                display: 'inline-block',
              }} />
              <span style={{ fontSize: '8px', color: runningCount > 0 ? F.GREEN : F.MUTED }}>
                {runningCount > 0 ? `${runningCount} RUNNING` : 'IDLE'}
              </span>
            </div>
          </div>
        </div>
      </div>

      {/* ─── MAIN CONTENT AREA ─── */}
      <div style={{
        flex: 1,
        display: 'flex',
        flexDirection: 'column',
        overflow: 'hidden',
        minWidth: 0,
      }}>
        {/* Top Breadcrumb Bar */}
        <div style={{
          padding: '8px 20px',
          backgroundColor: F.HEADER_BG,
          borderBottom: `2px solid ${F.ORANGE}`,
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
          boxShadow: `0 2px 8px ${F.ORANGE}20`,
          flexShrink: 0,
        }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <span style={{ fontSize: '9px', color: F.MUTED }}>ALGO TRADING</span>
            <ChevronRight size={10} style={{ color: F.MUTED }} />
            <span style={{ fontSize: '10px', fontWeight: 700, color: F.ORANGE, letterSpacing: '0.5px' }}>
              {NAV_ITEMS.find(n => n.key === activeView)?.label}
            </span>
          </div>
          {runningCount > 0 && (
            <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
              <Activity size={10} style={{ color: F.GREEN }} />
              <span style={{ fontSize: '8px', color: F.GREEN }}>{runningCount} LIVE</span>
            </div>
          )}
        </div>

        {/* View Content */}
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

        {/* Bottom Status Bar */}
        <div style={{
          backgroundColor: F.HEADER_BG,
          borderTop: `1px solid ${F.BORDER}`,
          padding: '4px 20px',
          fontSize: '9px',
          color: F.GRAY,
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center',
          flexShrink: 0,
        }}>
          <div style={{ display: 'flex', gap: '16px' }}>
            <span><span style={{ color: F.MUTED }}>ENGINE:</span> v1.0</span>
            <span><span style={{ color: F.MUTED }}>DEPLOYMENTS:</span> <span style={{ color: runningCount > 0 ? F.GREEN : F.MUTED }}>{runningCount} active</span></span>
          </div>
          <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
            <span style={{ width: '6px', height: '6px', borderRadius: '50%', backgroundColor: runningCount > 0 ? F.GREEN : F.MUTED }} />
            <span style={{ color: runningCount > 0 ? F.GREEN : F.MUTED }}>{runningCount > 0 ? 'ACTIVE' : 'IDLE'}</span>
          </div>
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
