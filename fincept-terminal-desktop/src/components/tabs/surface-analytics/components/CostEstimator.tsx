/**
 * Surface Analytics - Cost Estimator Component
 * Estimate query costs before execution
 */

import React, { useState, useCallback } from 'react';
import { DollarSign, Calculator, AlertTriangle, CheckCircle, RefreshCw, FileText } from 'lucide-react';
import { invoke } from '@tauri-apps/api/core';
import { FINCEPT_COLORS } from '../constants';

interface CostEstimate {
  dataset: string;
  symbols: string[];
  schema: string;
  record_count: number;
  cost_usd: number;
}

interface CostEstimatorProps {
  apiKey: string | null;
  dataset: string;
  symbols: string[];
  schema: string;
  startDate: string;
  endDate: string;
  accentColor: string;
  textColor: string;
  onEstimate?: (estimate: CostEstimate) => void;
}

// Schema descriptions
const SCHEMA_INFO: Record<string, { name: string; description: string; costLevel: 'low' | 'medium' | 'high' }> = {
  'ohlcv-1d': { name: 'Daily OHLCV', description: 'Daily open/high/low/close/volume bars', costLevel: 'low' },
  'ohlcv-1h': { name: 'Hourly OHLCV', description: 'Hourly bars', costLevel: 'low' },
  'ohlcv-1m': { name: 'Minute OHLCV', description: 'Minute-level bars', costLevel: 'medium' },
  'ohlcv-1s': { name: 'Second OHLCV', description: 'Second-level bars', costLevel: 'high' },
  'trades': { name: 'Trades', description: 'Individual trade messages', costLevel: 'high' },
  'mbp-1': { name: 'Top of Book', description: 'Best bid/ask quotes', costLevel: 'medium' },
  'mbp-10': { name: '10-Level Book', description: '10 price levels', costLevel: 'high' },
  'mbo': { name: 'Full Order Book', description: 'Every order book event', costLevel: 'high' },
  'tbbo': { name: 'TBBO', description: 'Top BBO with trades', costLevel: 'medium' },
  'definition': { name: 'Definitions', description: 'Instrument metadata', costLevel: 'low' },
};

export const CostEstimator: React.FC<CostEstimatorProps> = ({
  apiKey,
  dataset,
  symbols,
  schema,
  startDate,
  endDate,
  accentColor,
  textColor,
  onEstimate,
}) => {
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [estimate, setEstimate] = useState<CostEstimate | null>(null);

  const fetchEstimate = useCallback(async () => {
    if (!apiKey || !dataset || symbols.length === 0) {
      setError('Missing required parameters');
      return;
    }

    setLoading(true);
    setError(null);

    try {
      const result = await invoke<string>('databento_get_cost_estimate', {
        apiKey: apiKey,
        dataset: dataset,
        symbols: symbols,
        schema: schema,
        startDate: startDate,
        endDate: endDate,
      });

      const parsed = JSON.parse(result);

      if (parsed.error) {
        setError(parsed.message);
      } else {
        const costEstimate: CostEstimate = {
          dataset: parsed.dataset,
          symbols: parsed.symbols,
          schema: parsed.schema,
          record_count: parsed.record_count || 0,
          cost_usd: parsed.cost_usd || 0,
        };
        setEstimate(costEstimate);
        onEstimate?.(costEstimate);
      }
    } catch (err) {
      setError(`Failed to estimate cost: ${err}`);
    } finally {
      setLoading(false);
    }
  }, [apiKey, dataset, symbols, schema, startDate, endDate, onEstimate]);

  const schemaInfo = SCHEMA_INFO[schema] || { name: schema, description: '', costLevel: 'medium' };

  const getCostLevelColor = (level: 'low' | 'medium' | 'high') => {
    switch (level) {
      case 'low': return FINCEPT_COLORS.GREEN;
      case 'medium': return FINCEPT_COLORS.YELLOW;
      case 'high': return FINCEPT_COLORS.RED;
    }
  };

  const formatCost = (cost: number) => {
    if (cost === 0) return 'FREE';
    if (cost < 0.01) return '< $0.01';
    return `$${cost.toFixed(2)}`;
  };

  const formatRecordCount = (count: number) => {
    if (count >= 1000000) return `${(count / 1000000).toFixed(1)}M`;
    if (count >= 1000) return `${(count / 1000).toFixed(1)}K`;
    return count.toString();
  };

  if (!apiKey) {
    return null;
  }

  return (
    <div
      style={{
        backgroundColor: FINCEPT_COLORS.DARK_BG,
        border: `1px solid ${FINCEPT_COLORS.BORDER}`,
        borderRadius: '2px',
        padding: '12px',
      }}
    >
      {/* Header */}
      <div className="flex items-center justify-between mb-3">
        <div className="flex items-center gap-2">
          <Calculator size={14} style={{ color: accentColor }} />
          <span className="text-xs font-bold" style={{ color: accentColor, letterSpacing: '0.5px' }}>
            COST ESTIMATOR
          </span>
        </div>
        <button
          onClick={fetchEstimate}
          disabled={loading || !dataset || symbols.length === 0}
          className="flex items-center gap-1 px-2 py-1 text-xs"
          style={{
            backgroundColor: accentColor,
            color: FINCEPT_COLORS.BLACK,
            border: 'none',
            borderRadius: '2px',
            opacity: loading || !dataset || symbols.length === 0 ? 0.5 : 1,
          }}
        >
          {loading ? (
            <RefreshCw size={10} className="animate-spin" />
          ) : (
            <DollarSign size={10} />
          )}
          ESTIMATE
        </button>
      </div>

      {/* Query Parameters Summary */}
      <div
        className="grid grid-cols-2 gap-2 mb-3 p-2 text-xs"
        style={{
          backgroundColor: FINCEPT_COLORS.BLACK,
          border: `1px solid ${FINCEPT_COLORS.BORDER}`,
          borderRadius: '2px',
        }}
      >
        <div>
          <span style={{ color: FINCEPT_COLORS.MUTED }}>Dataset: </span>
          <span style={{ color: textColor }}>{dataset || 'Not selected'}</span>
        </div>
        <div>
          <span style={{ color: FINCEPT_COLORS.MUTED }}>Schema: </span>
          <span style={{ color: getCostLevelColor(schemaInfo.costLevel) }}>{schemaInfo.name}</span>
        </div>
        <div>
          <span style={{ color: FINCEPT_COLORS.MUTED }}>Symbols: </span>
          <span style={{ color: textColor }}>{symbols.length > 0 ? symbols.join(', ') : 'None'}</span>
        </div>
        <div>
          <span style={{ color: FINCEPT_COLORS.MUTED }}>Range: </span>
          <span style={{ color: textColor }}>{startDate} → {endDate}</span>
        </div>
      </div>

      {/* Cost Level Warning */}
      {schemaInfo.costLevel === 'high' && (
        <div
          className="flex items-center gap-2 mb-3 p-2 text-xs"
          style={{
            backgroundColor: `${FINCEPT_COLORS.YELLOW}15`,
            border: `1px solid ${FINCEPT_COLORS.YELLOW}50`,
            borderRadius: '2px',
            color: FINCEPT_COLORS.YELLOW,
          }}
        >
          <AlertTriangle size={12} />
          <span>High-frequency data schema - may incur significant costs</span>
        </div>
      )}

      {/* Error */}
      {error && (
        <div
          className="flex items-center gap-2 mb-3 p-2 text-xs"
          style={{
            backgroundColor: `${FINCEPT_COLORS.RED}15`,
            border: `1px solid ${FINCEPT_COLORS.RED}50`,
            borderRadius: '2px',
            color: FINCEPT_COLORS.RED,
          }}
        >
          <AlertTriangle size={12} />
          <span>{error}</span>
        </div>
      )}

      {/* Estimate Result */}
      {estimate && (
        <div
          className="p-3"
          style={{
            backgroundColor: FINCEPT_COLORS.BLACK,
            border: `1px solid ${FINCEPT_COLORS.GREEN}`,
            borderRadius: '2px',
          }}
        >
          <div className="flex items-center justify-between mb-2">
            <div className="flex items-center gap-2">
              <CheckCircle size={14} style={{ color: FINCEPT_COLORS.GREEN }} />
              <span className="text-xs font-bold" style={{ color: FINCEPT_COLORS.GREEN }}>
                ESTIMATE READY
              </span>
            </div>
          </div>

          <div className="grid grid-cols-2 gap-4">
            {/* Record Count */}
            <div className="text-center">
              <div className="flex items-center justify-center gap-1 mb-1">
                <FileText size={12} style={{ color: FINCEPT_COLORS.CYAN }} />
                <span className="text-xs" style={{ color: FINCEPT_COLORS.MUTED }}>
                  RECORDS
                </span>
              </div>
              <div className="text-lg font-bold" style={{ color: FINCEPT_COLORS.CYAN }}>
                {formatRecordCount(estimate.record_count)}
              </div>
            </div>

            {/* Cost */}
            <div className="text-center">
              <div className="flex items-center justify-center gap-1 mb-1">
                <DollarSign size={12} style={{ color: FINCEPT_COLORS.GREEN }} />
                <span className="text-xs" style={{ color: FINCEPT_COLORS.MUTED }}>
                  ESTIMATED COST
                </span>
              </div>
              <div
                className="text-lg font-bold"
                style={{ color: estimate.cost_usd === 0 ? FINCEPT_COLORS.GREEN : FINCEPT_COLORS.YELLOW }}
              >
                {formatCost(estimate.cost_usd)}
              </div>
            </div>
          </div>
        </div>
      )}
    </div>
  );
};
