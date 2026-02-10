// TechnicalIndicatorNode.tsx - Node for calculating technical indicators
import React, { useState } from 'react';
import { Position } from 'reactflow';
import { Play, Settings, CheckCircle, AlertCircle, Loader, TrendingUp } from 'lucide-react';
import {
  FINCEPT,
  SPACING,
  FONT_FAMILY,
  BORDER_RADIUS,
  BaseNode,
  NodeHeader,
  IconButton,
  Button,
  SelectField,
  InputField,
  InfoPanel,
  StatusPanel,
  SettingsPanel,
  Tag,
  KeyValue,
  getStatusColor,
} from './shared';

interface TechnicalIndicatorNodeProps {
  id: string;
  data: {
    label: string;
    dataSource: 'yfinance' | 'csv' | 'json' | 'input';
    symbol?: string;
    period?: string;
    csvPath?: string;
    jsonData?: string;
    categories: string[]; // ['momentum', 'volume', 'volatility', 'trend', 'others']
    status: 'idle' | 'running' | 'completed' | 'error';
    result?: any;
    error?: string;
    onExecute?: (nodeId: string) => void;
    onParameterChange?: (params: Record<string, any>) => void;
  };
  selected: boolean;
}

const TechnicalIndicatorNode: React.FC<TechnicalIndicatorNodeProps> = ({ id, data, selected }) => {
  const [showSettings, setShowSettings] = useState(false);
  const [localParams, setLocalParams] = useState({
    dataSource: data.dataSource || 'yfinance',
    symbol: data.symbol || 'AAPL',
    period: data.period || '1y',
    csvPath: data.csvPath || '',
    jsonData: data.jsonData || '',
    categories: data.categories || ['momentum', 'volume', 'volatility', 'trend', 'others']
  });

  const statusColor = getStatusColor(data.status);

  const getStatusIcon = () => {
    switch (data.status) {
      case 'running': return <Loader size={14} className="animate-spin" color={FINCEPT.ORANGE} />;
      case 'completed': return <CheckCircle size={14} color={FINCEPT.GREEN} />;
      case 'error': return <AlertCircle size={14} color={FINCEPT.RED} />;
      default: return <TrendingUp size={14} color={FINCEPT.GRAY} />;
    }
  };

  const handleParamChange = (key: string, value: any) => {
    const newParams = { ...localParams, [key]: value };
    setLocalParams(newParams);
    data.onParameterChange?.(newParams);
  };

  const handleCategoryToggle = (category: string) => {
    const newCategories = localParams.categories.includes(category)
      ? localParams.categories.filter(c => c !== category)
      : [...localParams.categories, category];
    handleParamChange('categories', newCategories);
  };

  const availableCategories = ['momentum', 'volume', 'volatility', 'trend', 'others'];

  const periodOptions = [
    { value: '1d', label: '1 Day' },
    { value: '5d', label: '5 Days' },
    { value: '1mo', label: '1 Month' },
    { value: '3mo', label: '3 Months' },
    { value: '6mo', label: '6 Months' },
    { value: '1y', label: '1 Year' },
    { value: '2y', label: '2 Years' },
    { value: '5y', label: '5 Years' },
    { value: 'max', label: 'Max' },
  ];

  const dataSourceOptions = [
    { value: 'yfinance', label: 'Yahoo Finance' },
    { value: 'csv', label: 'CSV File' },
    { value: 'json', label: 'JSON Data' },
    { value: 'input', label: 'Upstream Input' },
  ];

  return (
    <BaseNode
      selected={selected}
      minWidth="280px"
      maxWidth="350px"
      borderColor={statusColor}
      handles={[
        { type: 'target', position: Position.Left, color: FINCEPT.ORANGE },
        { type: 'source', position: Position.Right, color: data.status === 'completed' ? FINCEPT.GREEN : FINCEPT.ORANGE },
      ]}
    >
      {/* Header */}
      <NodeHeader
        icon={<span style={{ fontSize: '20px' }}>📊</span>}
        title={data.label}
        subtitle="TECHNICAL ANALYSIS"
        color={FINCEPT.WHITE}
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
        {/* Data Source Selection */}
        <SelectField
          label="Data Source"
          value={localParams.dataSource}
          options={dataSourceOptions}
          onChange={(val) => handleParamChange('dataSource', val)}
        />

        {/* YFinance Params */}
        {localParams.dataSource === 'yfinance' && (
          <>
            <InputField
              label="Symbol"
              value={localParams.symbol}
              onChange={(val) => handleParamChange('symbol', val)}
              placeholder="AAPL"
            />
            <SelectField
              label="Period"
              value={localParams.period}
              options={periodOptions}
              onChange={(val) => handleParamChange('period', val)}
            />
          </>
        )}

        {/* CSV Path */}
        {localParams.dataSource === 'csv' && (
          <InputField
            label="CSV File Path"
            value={localParams.csvPath}
            onChange={(val) => handleParamChange('csvPath', val)}
            placeholder="/path/to/data.csv"
          />
        )}

        {/* Categories Selection */}
        <div style={{ marginBottom: SPACING.MD }}>
          <label style={{
            color: FINCEPT.GRAY,
            fontSize: SPACING.MD,
            display: 'block',
            marginBottom: SPACING.SM
          }}>
            Indicator Categories
          </label>
          {availableCategories.map(category => (
            <label key={category} style={{
              display: 'flex',
              alignItems: 'center',
              gap: SPACING.SM,
              marginBottom: SPACING.SM,
              cursor: 'pointer'
            }}>
              <input
                type="checkbox"
                checked={localParams.categories.includes(category)}
                onChange={() => handleCategoryToggle(category)}
                style={{
                  accentColor: FINCEPT.ORANGE
                }}
              />
              <span style={{
                color: FINCEPT.WHITE,
                fontSize: '8px',
                textTransform: 'capitalize'
              }}>
                {category}
              </span>
            </label>
          ))}
        </div>

        <Button
          label="CLOSE"
          onClick={() => setShowSettings(false)}
          variant="primary"
          fullWidth
        />
      </SettingsPanel>

      {/* Summary View */}
      {!showSettings && (
        <div style={{ padding: SPACING.LG }}>
          {/* Data Source Summary */}
          <InfoPanel title="DATA SOURCE">
            <KeyValue label="Type" value={localParams.dataSource.toUpperCase()} />
            {localParams.dataSource === 'yfinance' && (
              <>
                <KeyValue label="Symbol" value={localParams.symbol} />
                <KeyValue label="Period" value={localParams.period} />
              </>
            )}
            {localParams.dataSource === 'csv' && (
              <div style={{
                color: FINCEPT.WHITE,
                fontSize: '8px',
                overflow: 'hidden',
                textOverflow: 'ellipsis',
                whiteSpace: 'nowrap'
              }}>
                {localParams.csvPath || 'Not set'}
              </div>
            )}
            {localParams.dataSource === 'input' && (
              <div style={{ color: FINCEPT.GRAY, fontSize: '8px' }}>
                Waiting for upstream data...
              </div>
            )}
          </InfoPanel>

          {/* Categories Summary */}
          <InfoPanel title={`INDICATORS (${localParams.categories.length}/5)`}>
            <div style={{ display: 'flex', flexWrap: 'wrap', gap: SPACING.XS }}>
              {localParams.categories.map(cat => (
                <Tag key={cat} label={cat} color={FINCEPT.ORANGE} />
              ))}
            </div>
          </InfoPanel>

          {/* Result Preview */}
          {data.status === 'completed' && data.result && (
            <StatusPanel
              type="success"
              icon={<CheckCircle size={10} />}
              message={Array.isArray(data.result)
                ? `${data.result.length} data points calculated`
                : 'Analysis complete'}
            />
          )}

          {/* Error Display */}
          {data.status === 'error' && data.error && (
            <StatusPanel
              type="error"
              icon={<AlertCircle size={10} />}
              message={data.error}
            />
          )}

          {/* Action Buttons */}
          <Button
            label={data.status === 'running' ? 'RUNNING...' : 'CALCULATE'}
            icon={data.status === 'running' ? <Loader size={12} className="animate-spin" /> : <Play size={12} />}
            onClick={() => data.onExecute?.(id)}
            variant="primary"
            disabled={data.status === 'running'}
            fullWidth
          />
        </div>
      )}
    </BaseNode>
  );
};

export default TechnicalIndicatorNode;
