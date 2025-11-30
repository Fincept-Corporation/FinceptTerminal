/**
 * Backtest Node
 *
 * ReactFlow node for running backtests in the workflow editor.
 * Connects to strategy inputs and outputs backtest results.
 */

import React, { useState } from 'react';
import { Handle, Position } from 'reactflow';
import { Activity, Play, Settings, Check, AlertCircle, TrendingUp, Loader } from 'lucide-react';
import { backtestingService } from '@/services/backtesting/BacktestingService';
import { BacktestRequest, BacktestResult } from '@/services/backtesting/interfaces/types';

interface BacktestNodeProps {
  data: {
    label: string;
    strategyCode?: string;
    startDate?: string;
    endDate?: string;
    initialCapital?: number;
    status: 'idle' | 'running' | 'completed' | 'error';
    result?: BacktestResult;
    error?: string;
    onStrategyChange?: (code: string) => void;
    onConfigChange?: (config: any) => void;
    onExecute?: () => Promise<void>;
  };
  selected: boolean;
}

const BacktestNode: React.FC<BacktestNodeProps> = ({ data, selected }) => {
  const [showSettings, setShowSettings] = useState(false);
  const [localConfig, setLocalConfig] = useState({
    startDate: data.startDate || new Date(Date.now() - 365 * 24 * 60 * 60 * 1000).toISOString().split('T')[0],
    endDate: data.endDate || new Date().toISOString().split('T')[0],
    initialCapital: data.initialCapital || 100000,
  });

  const handleSaveSettings = () => {
    if (data.onConfigChange) {
      data.onConfigChange(localConfig);
    }
    setShowSettings(false);
  };

  const handleRunBacktest = async () => {
    if (data.onExecute) {
      await data.onExecute();
    }
  };

  const getStatusColor = () => {
    switch (data.status) {
      case 'running': return '#3b82f6';
      case 'completed': return '#10b981';
      case 'error': return '#ef4444';
      default: return '#6b7280';
    }
  };

  const getStatusIcon = () => {
    switch (data.status) {
      case 'running': return <Loader size={14} className="animate-spin" color="#3b82f6" />;
      case 'completed': return <Check size={14} color="#10b981" />;
      case 'error': return <AlertCircle size={14} color="#ef4444" />;
      default: return <Activity size={14} color="#6b7280" />;
    }
  };

  const formatPerformance = () => {
    if (!data.result) return null;
    const perf = data.result.performance;
    return {
      totalReturn: (perf.totalReturn * 100).toFixed(2) + '%',
      sharpe: perf.sharpeRatio.toFixed(2),
      maxDD: (perf.maxDrawdown * 100).toFixed(2) + '%',
      trades: perf.totalTrades,
    };
  };

  const performance = formatPerformance();

  return (
    <div style={{
      background: selected ? '#252525' : '#1a1a1a',
      border: `2px solid ${selected ? '#3b82f6' : getStatusColor()}`,
      borderRadius: '8px',
      minWidth: '300px',
      maxWidth: '400px',
      fontFamily: 'Consolas, monospace',
      boxShadow: selected ? `0 0 16px #3b82f660` : '0 2px 8px rgba(0,0,0,0.3)',
      position: 'relative'
    }}>
      {/* Input Handle - Strategy */}
      <Handle
        type="target"
        position={Position.Left}
        id="strategy"
        style={{
          background: '#8b5cf6',
          width: '12px',
          height: '12px',
          border: '2px solid #1a1a1a',
          top: '30%'
        }}
      />

      {/* Output Handle - Results */}
      <Handle
        type="source"
        position={Position.Right}
        id="results"
        style={{
          background: '#10b981',
          width: '12px',
          height: '12px',
          border: '2px solid #1a1a1a',
        }}
      />

      {/* Header */}
      <div style={{
        padding: '12px',
        borderBottom: '1px solid #2a2a2a',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        background: '#1f1f1f'
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <Activity size={16} color={getStatusColor()} />
          <span style={{ color: '#e5e7eb', fontSize: '12px', fontWeight: 'bold' }}>
            {data.label}
          </span>
        </div>
        <div style={{ display: 'flex', gap: '6px' }}>
          {getStatusIcon()}
          <Settings
            size={14}
            color="#9ca3af"
            style={{ cursor: 'pointer' }}
            onClick={() => setShowSettings(!showSettings)}
          />
        </div>
      </div>

      {/* Settings Panel */}
      {showSettings && (
        <div style={{
          padding: '12px',
          borderBottom: '1px solid #2a2a2a',
          background: '#1a1a1a'
        }}>
          <div style={{ marginBottom: '8px' }}>
            <label style={{ color: '#9ca3af', fontSize: '10px', display: 'block', marginBottom: '4px' }}>
              Start Date
            </label>
            <input
              type="date"
              value={localConfig.startDate}
              onChange={(e) => setLocalConfig({ ...localConfig, startDate: e.target.value })}
              style={{
                width: '100%',
                padding: '6px',
                background: '#252525',
                border: '1px solid #3a3a3a',
                borderRadius: '4px',
                color: '#e5e7eb',
                fontSize: '10px'
              }}
            />
          </div>

          <div style={{ marginBottom: '8px' }}>
            <label style={{ color: '#9ca3af', fontSize: '10px', display: 'block', marginBottom: '4px' }}>
              End Date
            </label>
            <input
              type="date"
              value={localConfig.endDate}
              onChange={(e) => setLocalConfig({ ...localConfig, endDate: e.target.value })}
              style={{
                width: '100%',
                padding: '6px',
                background: '#252525',
                border: '1px solid #3a3a3a',
                borderRadius: '4px',
                color: '#e5e7eb',
                fontSize: '10px'
              }}
            />
          </div>

          <div style={{ marginBottom: '8px' }}>
            <label style={{ color: '#9ca3af', fontSize: '10px', display: 'block', marginBottom: '4px' }}>
              Initial Capital ($)
            </label>
            <input
              type="number"
              value={localConfig.initialCapital}
              onChange={(e) => setLocalConfig({ ...localConfig, initialCapital: Number(e.target.value) })}
              style={{
                width: '100%',
                padding: '6px',
                background: '#252525',
                border: '1px solid #3a3a3a',
                borderRadius: '4px',
                color: '#e5e7eb',
                fontSize: '10px'
              }}
            />
          </div>

          <button
            onClick={handleSaveSettings}
            style={{
              width: '100%',
              padding: '6px',
              background: '#3b82f6',
              color: 'white',
              border: 'none',
              borderRadius: '4px',
              fontSize: '10px',
              cursor: 'pointer',
              fontWeight: 'bold'
            }}
          >
            Save Configuration
          </button>
        </div>
      )}

      {/* Content */}
      <div style={{ padding: '12px' }}>
        {/* Configuration Display */}
        <div style={{ marginBottom: '12px' }}>
          <div style={{ fontSize: '10px', color: '#9ca3af', marginBottom: '6px' }}>
            Configuration:
          </div>
          <div style={{ fontSize: '9px', color: '#d1d5db', fontFamily: 'monospace' }}>
            {localConfig.startDate} → {localConfig.endDate}
          </div>
          <div style={{ fontSize: '9px', color: '#d1d5db', fontFamily: 'monospace' }}>
            Capital: ${localConfig.initialCapital.toLocaleString()}
          </div>
        </div>

        {/* Results Display */}
        {performance && data.status === 'completed' && (
          <div style={{
            padding: '8px',
            background: '#1f1f1f',
            borderRadius: '4px',
            marginBottom: '8px'
          }}>
            <div style={{ fontSize: '10px', color: '#9ca3af', marginBottom: '6px', display: 'flex', alignItems: 'center', gap: '4px' }}>
              <TrendingUp size={12} />
              Results:
            </div>
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '6px', fontSize: '9px' }}>
              <div>
                <div style={{ color: '#6b7280' }}>Total Return</div>
                <div style={{ color: performance.totalReturn.startsWith('-') ? '#ef4444' : '#10b981', fontWeight: 'bold' }}>
                  {performance.totalReturn}
                </div>
              </div>
              <div>
                <div style={{ color: '#6b7280' }}>Sharpe Ratio</div>
                <div style={{ color: '#e5e7eb', fontWeight: 'bold' }}>
                  {performance.sharpe}
                </div>
              </div>
              <div>
                <div style={{ color: '#6b7280' }}>Max DD</div>
                <div style={{ color: '#ef4444', fontWeight: 'bold' }}>
                  {performance.maxDD}
                </div>
              </div>
              <div>
                <div style={{ color: '#6b7280' }}>Trades</div>
                <div style={{ color: '#e5e7eb', fontWeight: 'bold' }}>
                  {performance.trades}
                </div>
              </div>
            </div>
          </div>
        )}

        {/* Error Display */}
        {data.error && data.status === 'error' && (
          <div style={{
            padding: '8px',
            background: '#3a0a0a',
            border: '1px solid #ef4444',
            borderRadius: '4px',
            marginBottom: '8px'
          }}>
            <div style={{ fontSize: '10px', color: '#ef4444', marginBottom: '4px' }}>
              Error:
            </div>
            <div style={{ fontSize: '9px', color: '#fca5a5' }}>
              {data.error}
            </div>
          </div>
        )}

        {/* Run Button */}
        <button
          onClick={handleRunBacktest}
          disabled={data.status === 'running' || !data.strategyCode}
          style={{
            width: '100%',
            padding: '8px',
            background: data.status === 'running' ? '#6b7280' : '#3b82f6',
            color: 'white',
            border: 'none',
            borderRadius: '4px',
            fontSize: '11px',
            fontWeight: 'bold',
            cursor: data.status === 'running' || !data.strategyCode ? 'not-allowed' : 'pointer',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            gap: '6px',
            opacity: data.status === 'running' || !data.strategyCode ? 0.6 : 1
          }}
        >
          {data.status === 'running' ? (
            <>
              <Loader size={14} className="animate-spin" />
              Running Backtest...
            </>
          ) : (
            <>
              <Play size={14} />
              Run Backtest
            </>
          )}
        </button>
      </div>

      {/* Handle Labels */}
      <div style={{
        position: 'absolute',
        left: '-60px',
        top: '30%',
        transform: 'translateY(-50%)',
        fontSize: '9px',
        color: '#6b7280',
        whiteSpace: 'nowrap'
      }}>
        Strategy
      </div>
      <div style={{
        position: 'absolute',
        right: '-50px',
        top: '50%',
        transform: 'translateY(-50%)',
        fontSize: '9px',
        color: '#6b7280',
        whiteSpace: 'nowrap'
      }}>
        Results
      </div>
    </div>
  );
};

export default BacktestNode;
