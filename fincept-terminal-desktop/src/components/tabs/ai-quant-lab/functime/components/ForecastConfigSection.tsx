/**
 * ForecastConfigSection Component
 * Model configuration for forecasting
 */

import React from 'react';
import { Settings, ChevronUp, ChevronDown } from 'lucide-react';
import { functimeService } from '@/services/aiQuantLab/functimeService';
import { FINCEPT } from '../constants';
import type { ForecastConfigProps } from '../types';

export const ForecastConfigSection: React.FC<ForecastConfigProps> = ({
  selectedModel,
  setSelectedModel,
  horizon,
  setHorizon,
  frequency,
  setFrequency,
  alpha,
  setAlpha,
  testSize,
  setTestSize,
  preprocess,
  setPreprocess,
  expanded,
  toggleSection
}) => {
  const models = functimeService.getAvailableModels();
  const frequencies = functimeService.getAvailableFrequencies();

  const showAlpha = ['lasso', 'ridge', 'elasticnet', 'auto_lasso', 'auto_ridge', 'auto_elasticnet'].includes(selectedModel);

  return (
    <div
      className="rounded overflow-hidden"
      style={{ border: `1px solid ${FINCEPT.BORDER}` }}
    >
      <button
        onClick={toggleSection}
        className="w-full px-3 py-2 flex items-center justify-between"
        style={{ backgroundColor: FINCEPT.HEADER_BG }}
      >
        <div className="flex items-center gap-2">
          <Settings size={14} color={FINCEPT.CYAN} />
          <span className="text-xs font-bold uppercase" style={{ color: FINCEPT.WHITE }}>
            Model Configuration
          </span>
        </div>
        {expanded ? (
          <ChevronUp size={14} color={FINCEPT.GRAY} />
        ) : (
          <ChevronDown size={14} color={FINCEPT.GRAY} />
        )}
      </button>

      {expanded && (
        <div className="p-3 space-y-3">
          {/* Model Selection */}
          <div>
            <label className="block text-xs font-mono mb-1" style={{ color: FINCEPT.GRAY }}>
              FORECAST MODEL
            </label>
            <select
              value={selectedModel}
              onChange={(e) => setSelectedModel(e.target.value)}
              className="w-full p-2 rounded text-xs font-mono"
              style={{
                backgroundColor: FINCEPT.DARK_BG,
                color: FINCEPT.WHITE,
                border: `1px solid ${FINCEPT.BORDER}`
              }}
            >
              {models.map((m) => (
                <option key={m.id} value={m.id}>
                  {m.name}
                </option>
              ))}
            </select>
          </div>

          {/* Horizon */}
          <div>
            <label className="block text-xs font-mono mb-1" style={{ color: FINCEPT.GRAY }}>
              FORECAST HORIZON
            </label>
            <input
              type="number"
              min={1}
              max={365}
              value={horizon}
              onChange={(e) => setHorizon(parseInt(e.target.value) || 7)}
              className="w-full p-2 rounded text-xs font-mono"
              style={{
                backgroundColor: FINCEPT.DARK_BG,
                color: FINCEPT.WHITE,
                border: `1px solid ${FINCEPT.BORDER}`
              }}
            />
          </div>

          {/* Frequency */}
          <div>
            <label className="block text-xs font-mono mb-1" style={{ color: FINCEPT.GRAY }}>
              DATA FREQUENCY
            </label>
            <select
              value={frequency}
              onChange={(e) => setFrequency(e.target.value)}
              className="w-full p-2 rounded text-xs font-mono"
              style={{
                backgroundColor: FINCEPT.DARK_BG,
                color: FINCEPT.WHITE,
                border: `1px solid ${FINCEPT.BORDER}`
              }}
            >
              {frequencies.map((f) => (
                <option key={f.id} value={f.id}>
                  {f.name}
                </option>
              ))}
            </select>
          </div>

          {/* Alpha (for regularized models) */}
          {showAlpha && (
            <div>
              <label className="block text-xs font-mono mb-1" style={{ color: FINCEPT.GRAY }}>
                ALPHA (REGULARIZATION)
              </label>
              <input
                type="number"
                step="0.1"
                min={0}
                value={alpha}
                onChange={(e) => setAlpha(parseFloat(e.target.value) || 1.0)}
                className="w-full p-2 rounded text-xs font-mono"
                style={{
                  backgroundColor: FINCEPT.DARK_BG,
                  color: FINCEPT.WHITE,
                  border: `1px solid ${FINCEPT.BORDER}`
                }}
              />
            </div>
          )}

          {/* Test Size */}
          <div>
            <label className="block text-xs font-mono mb-1" style={{ color: FINCEPT.GRAY }}>
              TEST SIZE (FOR EVALUATION)
            </label>
            <input
              type="number"
              min={1}
              value={testSize}
              onChange={(e) => setTestSize(parseInt(e.target.value) || 5)}
              className="w-full p-2 rounded text-xs font-mono"
              style={{
                backgroundColor: FINCEPT.DARK_BG,
                color: FINCEPT.WHITE,
                border: `1px solid ${FINCEPT.BORDER}`
              }}
            />
          </div>

          {/* Preprocessing */}
          <div>
            <label className="block text-xs font-mono mb-1" style={{ color: FINCEPT.GRAY }}>
              PREPROCESSING
            </label>
            <select
              value={preprocess}
              onChange={(e) => setPreprocess(e.target.value as 'none' | 'scale' | 'difference')}
              className="w-full p-2 rounded text-xs font-mono"
              style={{
                backgroundColor: FINCEPT.DARK_BG,
                color: FINCEPT.WHITE,
                border: `1px solid ${FINCEPT.BORDER}`
              }}
            >
              <option value="none">None</option>
              <option value="scale">Standard Scaling</option>
              <option value="difference">Differencing</option>
            </select>
          </div>
        </div>
      )}
    </div>
  );
};
