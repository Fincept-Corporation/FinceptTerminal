import React, { useState } from 'react';
import { Bot, Wrench, Search, BarChart3, Layout, Activity, Library } from 'lucide-react';
import type { AlgoSubView, PythonStrategy } from './types';
import StrategyEditor from './components/StrategyEditor';
import StrategyManager from './components/StrategyManager';
import ScannerPanel from './components/ScannerPanel';
import MonitorDashboard from './components/MonitorDashboard';
import StrategyLibraryTab from './components/StrategyLibraryTab';
import PythonBacktestPanel from './components/PythonBacktestPanel';
import PythonStrategyEditor from './components/PythonStrategyEditor';
import PythonDeployPanel from './components/PythonDeployPanel';

const F = {
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

const SUB_VIEWS: { key: AlgoSubView; label: string; icon: React.ElementType }[] = [
  { key: 'builder', label: 'BUILDER', icon: Wrench },
  { key: 'strategies', label: 'STRATEGIES', icon: Layout },
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

  const handleEditStrategy = (id: string) => {
    setEditStrategyId(id);
    setActiveView('builder');
  };

  const handleNewStrategy = () => {
    setEditStrategyId(null);
    setActiveView('builder');
  };

  // Handle Python strategy actions from library
  const handlePythonClone = (strategy: PythonStrategy) => {
    setClonePythonStrategy(strategy);
  };

  const handlePythonBacktest = (strategy: PythonStrategy) => {
    setBacktestPythonStrategy(strategy);
  };

  const handlePythonDeploy = (strategy: PythonStrategy) => {
    setDeployPythonStrategy(strategy);
  };

  // Handle Python strategy editor close/save
  const handlePythonEditorClose = () => {
    setClonePythonStrategy(null);
  };

  const handlePythonEditorSave = (customId: string) => {
    console.log('Python strategy saved with ID:', customId);
    setClonePythonStrategy(null);
    // Navigate to strategies tab to show the saved custom strategy
    setActiveView('strategies');
  };

  return (
    <div style={{
      display: 'flex',
      flexDirection: 'column',
      height: '100%',
      backgroundColor: F.DARK_BG,
      fontFamily: '"IBM Plex Mono", "Consolas", monospace',
      color: F.WHITE,
    }}>
      {/* Top Nav Bar */}
      <div style={{
        backgroundColor: F.HEADER_BG,
        borderBottom: `2px solid ${F.ORANGE}`,
        padding: '8px 16px',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        boxShadow: `0 2px 8px ${F.ORANGE}20`,
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <Bot size={16} style={{ color: F.ORANGE }} />
          <span style={{ fontSize: '11px', fontWeight: 700, color: F.WHITE, letterSpacing: '0.5px' }}>
            ALGO TRADING
          </span>
          <Activity size={10} style={{ color: F.GREEN, marginLeft: '4px' }} />
        </div>

        <div style={{ display: 'flex', gap: '2px' }}>
          {SUB_VIEWS.map(({ key, label, icon: Icon }) => (
            <button
              key={key}
              onClick={() => setActiveView(key)}
              style={{
                padding: '6px 12px',
                backgroundColor: activeView === key ? F.ORANGE : 'transparent',
                color: activeView === key ? F.DARK_BG : F.GRAY,
                border: 'none',
                fontSize: '9px',
                fontWeight: 700,
                letterSpacing: '0.5px',
                cursor: 'pointer',
                display: 'flex',
                alignItems: 'center',
                gap: '4px',
                borderRadius: '2px',
                transition: 'all 0.2s',
              }}
              onMouseEnter={e => {
                if (activeView !== key) {
                  (e.target as HTMLElement).style.color = F.WHITE;
                  (e.target as HTMLElement).style.backgroundColor = F.HOVER;
                }
              }}
              onMouseLeave={e => {
                if (activeView !== key) {
                  (e.target as HTMLElement).style.color = F.GRAY;
                  (e.target as HTMLElement).style.backgroundColor = 'transparent';
                }
              }}
            >
              <Icon size={10} />
              {label}
            </button>
          ))}
        </div>
      </div>

      {/* Content */}
      <div style={{ flex: 1, overflow: 'auto', minHeight: 0 }}>
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

      {/* Status Bar */}
      <div style={{
        backgroundColor: F.HEADER_BG,
        borderTop: `1px solid ${F.BORDER}`,
        padding: '4px 16px',
        fontSize: '9px',
        color: F.GRAY,
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
      }}>
        <span>ALGO ENGINE v1.0</span>
        <span style={{ color: F.MUTED }}>
          VIEW: {activeView.toUpperCase()}
        </span>
      </div>

      {/* Python Backtest Modal */}
      {backtestPythonStrategy && (
        <PythonBacktestPanel
          strategy={backtestPythonStrategy}
          onClose={() => setBacktestPythonStrategy(null)}
        />
      )}

      {/* Python Strategy Editor (Clone from Library) */}
      {clonePythonStrategy && (
        <PythonStrategyEditor
          strategy={clonePythonStrategy}
          isCustom={false}
          onClose={handlePythonEditorClose}
          onSave={handlePythonEditorSave}
          onBacktest={handlePythonBacktest}
        />
      )}

      {/* Python Deploy Panel */}
      {deployPythonStrategy && (
        <PythonDeployPanel
          strategy={deployPythonStrategy}
          onClose={() => setDeployPythonStrategy(null)}
          onDeployed={(deployId) => {
            console.log('Strategy deployed:', deployId);
            setDeployPythonStrategy(null);
            setActiveView('dashboard');
          }}
        />
      )}
    </div>
  );
};

export default AlgoTradingTab;
