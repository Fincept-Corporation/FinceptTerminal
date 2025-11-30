/**
 * Optimization Node
 *
 * ReactFlow node for running parameter optimization in workflows.
 * Supports grid search and genetic algorithms.
 */

import React, { useState } from 'react';
import { Handle, Position } from 'reactflow';
import { Zap, Play, Settings, Check, AlertCircle, Loader, TrendingUp } from 'lucide-react';
import { backtestingService } from '@/services/backtesting/BacktestingService';
import { OptimizationRequest, OptimizationResult } from '@/services/backtesting/interfaces/types';

interface OptimizationNodeProps {
  data: {
    label: string;
    strategyCode?: string;
    optimizationTarget: 'sharpe_ratio' | 'total_return' | 'sortino_ratio';
    parameterRanges?: Record<string, { min: number; max: number; step: number }>;
    method: 'grid' | 'genetic';
    status: 'idle' | 'running' | 'completed' | 'error';
    result?: OptimizationResult;
    error?: string;
    onConfigChange?: (config: any) => void;
    onExecute?: () => Promise<void>;
  };
  selected: boolean;
}

const OptimizationNode: React.FC<OptimizationNodeProps> = ({ data, selected }) => {
  const [showSettings, setShowSettings] = useState(false);
  const [localConfig, setLocalConfig] = useState({
    target: data.optimizationTarget || 'sharpe_ratio',
    method: data.method || 'grid',
    maxIterations: 100,
  });

  const [paramRanges, setParamRanges] = useState<Record<string, { min: number; max: number; step: number }>>(
    data.parameterRanges || {
      fast_period: { min: 10, max: 50, step: 5 },
      slow_period: { min: 100, max: 200, step: 10 },
    }
  );

  const handleSaveSettings = () => {
    if (data.onConfigChange) {
      data.onConfigChange({ ...localConfig, parameterRanges: paramRanges });
    }
    setShowSettings(false);
  };

  const handleRunOptimization = async () => {
    if (data.onExecute) {
      await data.onExecute();
    }
  };

  const getStatusColor = () => {
    switch (data.status) {
      case 'running': return '#f59e0b';
      case 'completed': return '#10b981';
      case 'error': return '#ef4444';
      default: return '#6b7280';
    }
  };

  const getStatusIcon = () => {
    switch (data.status) {
      case 'running': return <Loader size={14} className="animate-spin" color="#f59e0b" />;
      case 'completed': return <Check size={14} color="#10b981" />;
      case 'error': return <AlertCircle size={14} color="#ef4444" />;
      default: return <Zap size={14} color="#6b7280" />;
    }
  };

  const formatBestParams = () => {
    if (!data.result || !data.result.bestParameters) return null;
    return Object.entries(data.result.bestParameters)
      .map(([key, value]) => `${key}: ${value}`)
      .join(', ');
  };

  const addParameterRange = () => {
    const paramName = prompt('Parameter name:');
    if (paramName) {
      setParamRanges({
        ...paramRanges,
        [paramName]: { min: 0, max: 100, step: 1 },
      });
    }
  };

  return (
    <div style={{
      background: selected ? '#252525' : '#1a1a1a',
      border: `2px solid ${selected ? '#f59e0b' : getStatusColor()}`,
      borderRadius: '8px',
      minWidth: '320px',
      maxWidth: '450px',
      fontFamily: 'Consolas, monospace',
      boxShadow: selected ? `0 0 16px #f59e0b60` : '0 2px 8px rgba(0,0,0,0.3)',
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

      {/* Output Handle - Best Parameters */}
      <Handle
        type="source"
        position={Position.Right}
        id="best_params"
        style={{
          background: '#f59e0b',
          width: '12px',
          height: '12px',
          border: '2px solid #1a1a1a',
          top: '30%'
        }}
      />

      {/* Output Handle - All Results */}
      <Handle
        type="source"
        position={Position.Right}
        id="all_results"
        style={{
          background: '#10b981',
          width: '12px',
          height: '12px',
          border: '2px solid #1a1a1a',
          top: '60%'
        }}
      />

      {/* Header */}
      <div style={{
        padding: '12px',
        borderBottom: '1px solid #2a2a2a',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        background: 'linear-gradient(135deg, #1f1f1f 0%, #2a1f0f 100%)'
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <Zap size={16} color={getStatusColor()} />
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
          background: '#1a1a1a',
          maxHeight: '400px',
          overflowY: 'auto'
        }}>
          <div style={{ marginBottom: '12px' }}>
            <label style={{ color: '#9ca3af', fontSize: '10px', display: 'block', marginBottom: '4px' }}>
              Optimization Target
            </label>
            <select
              value={localConfig.target}
              onChange={(e) => setLocalConfig({ ...localConfig, target: e.target.value as any })}
              style={{
                width: '100%',
                padding: '6px',
                background: '#252525',
                border: '1px solid #3a3a3a',
                borderRadius: '4px',
                color: '#e5e7eb',
                fontSize: '10px'
              }}
            >
              <option value="sharpe_ratio">Sharpe Ratio</option>
              <option value="total_return">Total Return</option>
              <option value="sortino_ratio">Sortino Ratio</option>
              <option value="calmar_ratio">Calmar Ratio</option>
              <option value="profit_factor">Profit Factor</option>
            </select>
          </div>

          <div style={{ marginBottom: '12px' }}>
            <label style={{ color: '#9ca3af', fontSize: '10px', display: 'block', marginBottom: '4px' }}>
              Method
            </label>
            <select
              value={localConfig.method}
              onChange={(e) => setLocalConfig({ ...localConfig, method: e.target.value as any })}
              style={{
                width: '100%',
                padding: '6px',
                background: '#252525',
                border: '1px solid #3a3a3a',
                borderRadius: '4px',
                color: '#e5e7eb',
                fontSize: '10px'
              }}
            >
              <option value="grid">Grid Search</option>
              <option value="genetic">Genetic Algorithm</option>
              <option value="bayesian">Bayesian Optimization</option>
            </select>
          </div>

          <div style={{ marginBottom: '12px' }}>
            <label style={{ color: '#9ca3af', fontSize: '10px', display: 'block', marginBottom: '4px' }}>
              Max Iterations
            </label>
            <input
              type="number"
              value={localConfig.maxIterations}
              onChange={(e) => setLocalConfig({ ...localConfig, maxIterations: Number(e.target.value) })}
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

          <div style={{ marginBottom: '12px' }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '6px' }}>
              <label style={{ color: '#9ca3af', fontSize: '10px' }}>
                Parameter Ranges
              </label>
              <button
                onClick={addParameterRange}
                style={{
                  padding: '2px 6px',
                  background: '#3b82f6',
                  color: 'white',
                  border: 'none',
                  borderRadius: '3px',
                  fontSize: '9px',
                  cursor: 'pointer'
                }}
              >
                + Add
              </button>
            </div>

            {Object.entries(paramRanges).map(([param, range]) => (
              <div key={param} style={{ marginBottom: '8px', padding: '8px', background: '#252525', borderRadius: '4px' }}>
                <div style={{ fontSize: '10px', color: '#e5e7eb', marginBottom: '4px', fontWeight: 'bold' }}>
                  {param}
                </div>
                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '4px' }}>
                  <div>
                    <label style={{ fontSize: '8px', color: '#6b7280' }}>Min</label>
                    <input
                      type="number"
                      value={range.min}
                      onChange={(e) => setParamRanges({
                        ...paramRanges,
                        [param]: { ...range, min: Number(e.target.value) }
                      })}
                      style={{
                        width: '100%',
                        padding: '4px',
                        background: '#1a1a1a',
                        border: '1px solid #3a3a3a',
                        borderRadius: '3px',
                        color: '#e5e7eb',
                        fontSize: '9px'
                      }}
                    />
                  </div>
                  <div>
                    <label style={{ fontSize: '8px', color: '#6b7280' }}>Max</label>
                    <input
                      type="number"
                      value={range.max}
                      onChange={(e) => setParamRanges({
                        ...paramRanges,
                        [param]: { ...range, max: Number(e.target.value) }
                      })}
                      style={{
                        width: '100%',
                        padding: '4px',
                        background: '#1a1a1a',
                        border: '1px solid #3a3a3a',
                        borderRadius: '3px',
                        color: '#e5e7eb',
                        fontSize: '9px'
                      }}
                    />
                  </div>
                  <div>
                    <label style={{ fontSize: '8px', color: '#6b7280' }}>Step</label>
                    <input
                      type="number"
                      value={range.step}
                      onChange={(e) => setParamRanges({
                        ...paramRanges,
                        [param]: { ...range, step: Number(e.target.value) }
                      })}
                      style={{
                        width: '100%',
                        padding: '4px',
                        background: '#1a1a1a',
                        border: '1px solid #3a3a3a',
                        borderRadius: '3px',
                        color: '#e5e7eb',
                        fontSize: '9px'
                      }}
                    />
                  </div>
                </div>
              </div>
            ))}
          </div>

          <button
            onClick={handleSaveSettings}
            style={{
              width: '100%',
              padding: '6px',
              background: '#f59e0b',
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
            Target: {localConfig.target.replace('_', ' ')}
          </div>
          <div style={{ fontSize: '9px', color: '#d1d5db', fontFamily: 'monospace' }}>
            Method: {localConfig.method}
          </div>
          <div style={{ fontSize: '9px', color: '#d1d5db', fontFamily: 'monospace' }}>
            Parameters: {Object.keys(paramRanges).length}
          </div>
        </div>

        {/* Results Display */}
        {data.result && data.status === 'completed' && (
          <div style={{
            padding: '8px',
            background: '#1f1f1f',
            borderRadius: '4px',
            marginBottom: '8px'
          }}>
            <div style={{ fontSize: '10px', color: '#9ca3af', marginBottom: '6px', display: 'flex', alignItems: 'center', gap: '4px' }}>
              <TrendingUp size={12} />
              Best Parameters:
            </div>
            <div style={{ fontSize: '9px', color: '#10b981', fontFamily: 'monospace', marginBottom: '6px' }}>
              {formatBestParams()}
            </div>
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '6px', fontSize: '9px' }}>
              <div>
                <div style={{ color: '#6b7280' }}>Iterations</div>
                <div style={{ color: '#e5e7eb', fontWeight: 'bold' }}>
                  {data.result.iterations}
                </div>
              </div>
              <div>
                <div style={{ color: '#6b7280' }}>Duration</div>
                <div style={{ color: '#e5e7eb', fontWeight: 'bold' }}>
                  {Math.round(data.result.duration / 1000)}s
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
          onClick={handleRunOptimization}
          disabled={data.status === 'running' || !data.strategyCode}
          style={{
            width: '100%',
            padding: '8px',
            background: data.status === 'running' ? '#6b7280' : '#f59e0b',
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
              Optimizing... ({data.result?.iterations || 0}/{localConfig.maxIterations})
            </>
          ) : (
            <>
              <Play size={14} />
              Run Optimization
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
        right: '-80px',
        top: '30%',
        transform: 'translateY(-50%)',
        fontSize: '9px',
        color: '#6b7280',
        whiteSpace: 'nowrap'
      }}>
        Best Params
      </div>
      <div style={{
        position: 'absolute',
        right: '-70px',
        top: '60%',
        transform: 'translateY(-50%)',
        fontSize: '9px',
        color: '#6b7280',
        whiteSpace: 'nowrap'
      }}>
        All Results
      </div>
    </div>
  );
};

export default OptimizationNode;
