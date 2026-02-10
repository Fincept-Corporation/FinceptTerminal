/**
 * Backtest Node
 *
 * ReactFlow node for running backtests in the workflow editor.
 * Connects to strategy inputs and outputs backtest results.
 */

import React, { useState } from 'react';
import { Position } from 'reactflow';
import { Activity, Play, Settings, Check, AlertCircle, TrendingUp, Loader } from 'lucide-react';
import { backtestingService } from '@/services/backtesting/BacktestingService';
import { BacktestRequest, BacktestResult } from '@/services/backtesting/interfaces/types';
import {
  FINCEPT,
  SPACING,
  BORDER_RADIUS,
  BaseNode,
  NodeHeader,
  IconButton,
  Button,
  InputField,
  InfoPanel,
  StatusPanel,
  SettingsPanel,
  KeyValue,
  getStatusColor,
} from './shared';

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

  const statusColor = getStatusColor(data.status);

  const getStatusIcon = () => {
    switch (data.status) {
      case 'running': return <Loader size={14} className="animate-spin" color={FINCEPT.BLUE} />;
      case 'completed': return <Check size={14} color={FINCEPT.GREEN} />;
      case 'error': return <AlertCircle size={14} color={FINCEPT.RED} />;
      default: return <Activity size={14} color={FINCEPT.GRAY} />;
    }
  };

  const formatPerformance = () => {
    if (!data.result || !data.result.performance) return null;
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
    <BaseNode
      selected={selected}
      minWidth="300px"
      maxWidth="400px"
      borderColor={statusColor}
      handles={[
        { type: 'target', position: Position.Left, id: 'strategy', color: FINCEPT.BLUE, top: '30%', label: 'Strategy' },
        { type: 'source', position: Position.Right, id: 'results', color: FINCEPT.GREEN, label: 'Results' },
      ]}
    >
      {/* Header */}
      <NodeHeader
        icon={<Activity size={16} />}
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
        <InputField
          label="Start Date"
          type="date"
          value={localConfig.startDate}
          onChange={(val) => setLocalConfig({ ...localConfig, startDate: val })}
        />

        <InputField
          label="End Date"
          type="date"
          value={localConfig.endDate}
          onChange={(val) => setLocalConfig({ ...localConfig, endDate: val })}
        />

        <InputField
          label="Initial Capital ($)"
          type="number"
          value={String(localConfig.initialCapital)}
          onChange={(val) => {
            const numVal = val === '' ? 0 : Number(val);
            setLocalConfig({ ...localConfig, initialCapital: numVal });
          }}
        />

        <Button
          label="SAVE CONFIGURATION"
          onClick={handleSaveSettings}
          variant="primary"
          fullWidth
        />
      </SettingsPanel>

      {/* Content */}
      <div style={{ padding: SPACING.LG }}>
        {/* Configuration Display */}
        <InfoPanel title="Configuration">
          <KeyValue label="Period" value={`${localConfig.startDate} → ${localConfig.endDate}`} />
          <KeyValue label="Capital" value={`$${localConfig.initialCapital.toLocaleString()}`} />
        </InfoPanel>

        {/* Results Display */}
        {performance && data.status === 'completed' && (
          <div style={{
            padding: SPACING.MD,
            background: FINCEPT.HEADER_BG,
            borderRadius: BORDER_RADIUS.LG,
            marginBottom: SPACING.MD
          }}>
            <div style={{ fontSize: '10px', color: FINCEPT.GRAY, marginBottom: SPACING.SM, display: 'flex', alignItems: 'center', gap: SPACING.XS }}>
              <TrendingUp size={12} />
              Results:
            </div>
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.SM, fontSize: '9px' }}>
              <KeyValue
                label="Total Return"
                value={performance.totalReturn}
                valueColor={performance.totalReturn.startsWith('-') ? FINCEPT.RED : FINCEPT.GREEN}
              />
              <KeyValue label="Sharpe Ratio" value={performance.sharpe} />
              <KeyValue label="Max DD" value={performance.maxDD} valueColor={FINCEPT.RED} />
              <KeyValue label="Trades" value={String(performance.trades)} />
            </div>
          </div>
        )}

        {/* Error Display */}
        {data.error && data.status === 'error' && (
          <StatusPanel
            type="error"
            icon={<AlertCircle size={12} />}
            message={data.error}
          />
        )}

        {/* Run Button */}
        <Button
          label={data.status === 'running' ? 'Running Backtest...' : 'Run Backtest'}
          icon={data.status === 'running' ? <Loader size={14} className="animate-spin" /> : <Play size={14} />}
          onClick={handleRunBacktest}
          variant="primary"
          disabled={data.status === 'running' || !data.strategyCode}
          fullWidth
        />
      </div>
    </BaseNode>
  );
};

export default BacktestNode;
