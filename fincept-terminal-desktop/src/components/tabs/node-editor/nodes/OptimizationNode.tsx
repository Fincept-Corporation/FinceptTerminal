/**
 * Optimization Node
 *
 * ReactFlow node for running parameter optimization in workflows.
 * Supports grid search and genetic algorithms.
 */

import React, { useState } from 'react';
import { Position } from 'reactflow';
import { Zap, Play, Settings, Check, AlertCircle, Loader, TrendingUp, Plus } from 'lucide-react';
import { showPrompt } from '@/utils/notifications';
import { backtestingService } from '@/services/backtesting/BacktestingService';
import { OptimizationRequest, OptimizationResult } from '@/services/backtesting/interfaces/types';
import {
  FINCEPT,
  SPACING,
  BORDER_RADIUS,
  BaseNode,
  NodeHeader,
  IconButton,
  Button,
  SelectField,
  InputField,
  InfoPanel,
  SettingsPanel,
  KeyValue,
  getStatusColor,
} from './shared';

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

  const statusColor = getStatusColor(data.status);

  const getStatusIcon = () => {
    switch (data.status) {
      case 'running': return <Loader size={14} className="animate-spin" color={FINCEPT.ORANGE} />;
      case 'completed': return <Check size={14} color={FINCEPT.GREEN} />;
      case 'error': return <AlertCircle size={14} color={FINCEPT.RED} />;
      default: return <Zap size={14} color={FINCEPT.GRAY} />;
    }
  };

  const formatBestParams = () => {
    if (!data.result || !data.result.bestParameters) return null;
    return Object.entries(data.result.bestParameters)
      .map(([key, value]) => `${key}: ${value}`)
      .join(', ');
  };

  const addParameterRange = async () => {
    const paramName = await showPrompt('Parameter name:', {
      title: 'Add Parameter',
      placeholder: 'e.g., stopLoss, takeProfit'
    });
    if (paramName) {
      setParamRanges({
        ...paramRanges,
        [paramName]: { min: 0, max: 100, step: 1 },
      });
    }
  };

  const targetOptions = [
    { value: 'sharpe_ratio', label: 'Sharpe Ratio' },
    { value: 'total_return', label: 'Total Return' },
    { value: 'sortino_ratio', label: 'Sortino Ratio' },
    { value: 'calmar_ratio', label: 'Calmar Ratio' },
    { value: 'profit_factor', label: 'Profit Factor' },
  ];

  const methodOptions = [
    { value: 'grid', label: 'Grid Search' },
    { value: 'genetic', label: 'Genetic Algorithm' },
    { value: 'bayesian', label: 'Bayesian Optimization' },
  ];

  return (
    <BaseNode
      selected={selected}
      minWidth="320px"
      maxWidth="450px"
      borderColor={statusColor}
      handles={[
        { type: 'target', position: Position.Left, id: 'strategy', color: FINCEPT.BLUE, top: '30%', label: 'Strategy' },
        { type: 'source', position: Position.Right, id: 'best_params', color: FINCEPT.ORANGE, top: '30%', label: 'Best Params' },
        { type: 'source', position: Position.Right, id: 'all_results', color: FINCEPT.GREEN, top: '60%', label: 'All Results' },
      ]}
    >
      {/* Header */}
      <NodeHeader
        icon={<Zap size={16} />}
        title={data.label}
        color={statusColor}
        rightActions={
          <>
            {getStatusIcon()}
            <IconButton
              icon={<Settings size={14} />}
              onClick={() => setShowSettings(!showSettings)}
              active={showSettings}
            />
          </>
        }
      />

      {/* Settings Panel */}
      <SettingsPanel isOpen={showSettings}>
        <SelectField
          label="Optimization Target"
          value={localConfig.target}
          options={targetOptions}
          onChange={(val) => setLocalConfig({ ...localConfig, target: val as any })}
        />

        <SelectField
          label="Method"
          value={localConfig.method}
          options={methodOptions}
          onChange={(val) => setLocalConfig({ ...localConfig, method: val as any })}
        />

        <InputField
          label="Max Iterations"
          type="number"
          value={String(localConfig.maxIterations)}
          onChange={(val) => setLocalConfig({ ...localConfig, maxIterations: Number(val) || 0 })}
        />

        {/* Parameter Ranges */}
        <div style={{ marginBottom: SPACING.MD }}>
          <div style={{
            display: 'flex',
            justifyContent: 'space-between',
            alignItems: 'center',
            marginBottom: SPACING.SM
          }}>
            <label style={{
              color: FINCEPT.GRAY,
              fontSize: '10px'
            }}>
              Parameter Ranges
            </label>
            <button
              onClick={addParameterRange}
              style={{
                padding: '2px 6px',
                background: FINCEPT.BLUE,
                color: FINCEPT.WHITE,
                border: 'none',
                borderRadius: BORDER_RADIUS.SM,
                fontSize: '9px',
                cursor: 'pointer',
                display: 'flex',
                alignItems: 'center',
                gap: '2px'
              }}
            >
              <Plus size={10} /> Add
            </button>
          </div>

          {Object.entries(paramRanges).map(([param, range]) => (
            <div
              key={param}
              style={{
                marginBottom: SPACING.MD,
                padding: SPACING.MD,
                background: FINCEPT.PANEL_BG,
                borderRadius: BORDER_RADIUS.LG
              }}
            >
              <div style={{
                fontSize: '10px',
                color: FINCEPT.WHITE,
                marginBottom: SPACING.XS,
                fontWeight: 700
              }}>
                {param}
              </div>
              <div style={{
                display: 'grid',
                gridTemplateColumns: '1fr 1fr 1fr',
                gap: SPACING.XS
              }}>
                <div>
                  <label style={{ fontSize: '8px', color: FINCEPT.GRAY }}>Min</label>
                  <input
                    type="text"
                    inputMode="decimal"
                    value={range.min.toString()}
                    onChange={(e) => {
                      const v = e.target.value;
                      if (v === '' || /^\d*\.?\d*$/.test(v)) {
                        setParamRanges({
                          ...paramRanges,
                          [param]: { ...range, min: parseFloat(v) || 0 }
                        });
                      }
                    }}
                    style={{
                      width: '100%',
                      padding: SPACING.XS,
                      background: FINCEPT.DARK_BG,
                      border: `1px solid ${FINCEPT.BORDER}`,
                      borderRadius: BORDER_RADIUS.SM,
                      color: FINCEPT.WHITE,
                      fontSize: '9px'
                    }}
                  />
                </div>
                <div>
                  <label style={{ fontSize: '8px', color: FINCEPT.GRAY }}>Max</label>
                  <input
                    type="text"
                    inputMode="decimal"
                    value={range.max.toString()}
                    onChange={(e) => {
                      const v = e.target.value;
                      if (v === '' || /^\d*\.?\d*$/.test(v)) {
                        setParamRanges({
                          ...paramRanges,
                          [param]: { ...range, max: parseFloat(v) || 0 }
                        });
                      }
                    }}
                    style={{
                      width: '100%',
                      padding: SPACING.XS,
                      background: FINCEPT.DARK_BG,
                      border: `1px solid ${FINCEPT.BORDER}`,
                      borderRadius: BORDER_RADIUS.SM,
                      color: FINCEPT.WHITE,
                      fontSize: '9px'
                    }}
                  />
                </div>
                <div>
                  <label style={{ fontSize: '8px', color: FINCEPT.GRAY }}>Step</label>
                  <input
                    type="text"
                    inputMode="decimal"
                    value={range.step.toString()}
                    onChange={(e) => {
                      const v = e.target.value;
                      if (v === '' || /^\d*\.?\d*$/.test(v)) {
                        setParamRanges({
                          ...paramRanges,
                          [param]: { ...range, step: parseFloat(v) || 0 }
                        });
                      }
                    }}
                    style={{
                      width: '100%',
                      padding: SPACING.XS,
                      background: FINCEPT.DARK_BG,
                      border: `1px solid ${FINCEPT.BORDER}`,
                      borderRadius: BORDER_RADIUS.SM,
                      color: FINCEPT.WHITE,
                      fontSize: '9px'
                    }}
                  />
                </div>
              </div>
            </div>
          ))}
        </div>

        <Button
          label="Save Configuration"
          onClick={handleSaveSettings}
          variant="primary"
          fullWidth
        />
      </SettingsPanel>

      {/* Content */}
      {!showSettings && (
        <div style={{ padding: SPACING.LG }}>
          {/* Configuration Display */}
          <InfoPanel title="Configuration">
            <KeyValue label="Target" value={localConfig.target.replace('_', ' ')} />
            <KeyValue label="Method" value={localConfig.method} />
            <KeyValue label="Parameters" value={String(Object.keys(paramRanges).length)} />
          </InfoPanel>

          {/* Results Display */}
          {data.result && data.status === 'completed' && (
            <div style={{
              padding: SPACING.MD,
              background: FINCEPT.HEADER_BG,
              borderRadius: BORDER_RADIUS.LG,
              marginBottom: SPACING.MD
            }}>
              <div style={{
                fontSize: '10px',
                color: FINCEPT.GRAY,
                marginBottom: SPACING.SM,
                display: 'flex',
                alignItems: 'center',
                gap: SPACING.XS
              }}>
                <TrendingUp size={12} />
                Best Parameters:
              </div>
              <div style={{
                fontSize: '9px',
                color: FINCEPT.GREEN,
                fontFamily: 'monospace',
                marginBottom: SPACING.SM
              }}>
                {formatBestParams()}
              </div>
              <div style={{
                display: 'grid',
                gridTemplateColumns: '1fr 1fr',
                gap: SPACING.SM,
                fontSize: '9px'
              }}>
                <KeyValue label="Iterations" value={String(data.result.iterations)} />
                <KeyValue label="Duration" value={`${Math.round(data.result.duration / 1000)}s`} />
              </div>
            </div>
          )}

          {/* Error Display */}
          {data.error && data.status === 'error' && (
            <div style={{
              padding: SPACING.MD,
              background: `${FINCEPT.RED}20`,
              border: `1px solid ${FINCEPT.RED}`,
              borderRadius: BORDER_RADIUS.LG,
              marginBottom: SPACING.MD
            }}>
              <div style={{
                fontSize: '10px',
                color: FINCEPT.RED,
                marginBottom: SPACING.XS
              }}>
                Error:
              </div>
              <div style={{ fontSize: '9px', color: FINCEPT.RED }}>
                {data.error}
              </div>
            </div>
          )}

          {/* Run Button */}
          <Button
            label={data.status === 'running'
              ? `Optimizing... (${data.result?.iterations || 0}/${localConfig.maxIterations})`
              : 'Run Optimization'}
            icon={data.status === 'running' ? <Loader size={14} className="animate-spin" /> : <Play size={14} />}
            onClick={handleRunOptimization}
            variant="primary"
            disabled={data.status === 'running' || !data.strategyCode}
            fullWidth
          />
        </div>
      )}
    </BaseNode>
  );
};

export default OptimizationNode;
