/**
 * EntropyPoolingMode Component
 * Entropy pooling configuration and results
 */

import React from 'react';
import { Layers, RefreshCw } from 'lucide-react';
import { FINCEPT } from '../constants';
import type { EntropyPoolingModeProps } from '../types';

export const EntropyPoolingMode: React.FC<EntropyPoolingModeProps> = ({
  nScenarios,
  setNScenarios,
  maxProbability,
  setMaxProbability,
  isLoading,
  entropyResult,
  runEntropyPooling
}) => {
  return (
    <div
      className="rounded border"
      style={{ backgroundColor: FINCEPT.PANEL_BG, borderColor: FINCEPT.BORDER }}
    >
      <div className="p-4 space-y-4">
        <div className="grid grid-cols-2 gap-4">
          <div>
            <label
              className="block text-xs font-mono mb-1"
              style={{ color: FINCEPT.GRAY }}
            >
              NUMBER OF SCENARIOS
            </label>
            <input
              type="number"
              value={nScenarios}
              onChange={(e) => setNScenarios(parseInt(e.target.value))}
              min="10"
              max="10000"
              className="w-full px-3 py-2 rounded text-sm font-mono border"
              style={{
                backgroundColor: FINCEPT.DARK_BG,
                borderColor: FINCEPT.BORDER,
                color: FINCEPT.WHITE
              }}
            />
          </div>
          <div>
            <label
              className="block text-xs font-mono mb-1"
              style={{ color: FINCEPT.GRAY }}
            >
              MAX PROBABILITY (%)
            </label>
            <input
              type="number"
              value={maxProbability * 100}
              onChange={(e) => setMaxProbability(parseFloat(e.target.value) / 100)}
              step="0.5"
              min="0.5"
              max="100"
              className="w-full px-3 py-2 rounded text-sm font-mono border"
              style={{
                backgroundColor: FINCEPT.DARK_BG,
                borderColor: FINCEPT.BORDER,
                color: FINCEPT.WHITE
              }}
            />
          </div>
        </div>

        <button
          onClick={runEntropyPooling}
          disabled={isLoading}
          className="w-full py-2.5 rounded font-bold uppercase tracking-wide text-sm hover:bg-opacity-90 transition-colors flex items-center justify-center gap-2 disabled:opacity-50"
          style={{ backgroundColor: FINCEPT.ORANGE, color: FINCEPT.DARK_BG }}
        >
          {isLoading ? (
            <RefreshCw size={16} className="animate-spin" />
          ) : (
            <Layers size={16} />
          )}
          APPLY ENTROPY POOLING
        </button>

        {/* Entropy Results */}
        {entropyResult && entropyResult.success && (
          <div className="grid grid-cols-2 gap-4 mt-4">
            <div className="p-3 rounded" style={{ backgroundColor: FINCEPT.DARK_BG }}>
              <p className="text-xs font-mono mb-2" style={{ color: FINCEPT.GRAY }}>
                PRIOR
              </p>
              <div className="space-y-1">
                <p className="text-xs font-mono" style={{ color: FINCEPT.WHITE }}>
                  Effective Scenarios:{' '}
                  {entropyResult.effective_scenarios_prior?.toFixed(1)}
                </p>
              </div>
            </div>
            <div className="p-3 rounded" style={{ backgroundColor: FINCEPT.DARK_BG }}>
              <p className="text-xs font-mono mb-2" style={{ color: FINCEPT.CYAN }}>
                POSTERIOR
              </p>
              <div className="space-y-1">
                <p className="text-xs font-mono" style={{ color: FINCEPT.WHITE }}>
                  Effective Scenarios:{' '}
                  {entropyResult.effective_scenarios_posterior?.toFixed(1)}
                </p>
                <p className="text-xs font-mono" style={{ color: FINCEPT.WHITE }}>
                  Max Prob: {(entropyResult.max_probability * 100).toFixed(2)}%
                </p>
                <p className="text-xs font-mono" style={{ color: FINCEPT.WHITE }}>
                  Min Prob: {(entropyResult.min_probability * 100).toFixed(4)}%
                </p>
              </div>
            </div>
          </div>
        )}
      </div>
    </div>
  );
};
