/**
 * ConfigSection Component
 * Risk-free rate and drawdown threshold inputs
 */

import React from 'react';
import { FINCEPT } from '../constants';
import type { ConfigSectionProps } from '../types';

export function ConfigSection({
  config,
  setConfig
}: ConfigSectionProps) {
  return (
    <div className="p-2 space-y-3">
      <div>
        <label className="block text-xs font-mono mb-1" style={{ color: FINCEPT.GRAY }}>
          RISK-FREE RATE
        </label>
        <input
          type="number"
          step="0.01"
          value={config.risk_free_rate}
          onChange={(e) => setConfig({ ...config, risk_free_rate: parseFloat(e.target.value) || 0 })}
          className="w-full p-2 rounded text-xs font-mono"
          style={{
            backgroundColor: FINCEPT.DARK_BG,
            color: FINCEPT.WHITE,
            border: `1px solid ${FINCEPT.BORDER}`
          }}
        />
      </div>

      <div>
        <label className="block text-xs font-mono mb-1" style={{ color: FINCEPT.GRAY }}>
          DRAWDOWN THRESHOLD
        </label>
        <input
          type="number"
          step="0.01"
          value={config.drawdown_threshold}
          onChange={(e) => setConfig({ ...config, drawdown_threshold: parseFloat(e.target.value) || 0.05 })}
          className="w-full p-2 rounded text-xs font-mono"
          style={{
            backgroundColor: FINCEPT.DARK_BG,
            color: FINCEPT.WHITE,
            border: `1px solid ${FINCEPT.BORDER}`
          }}
        />
      </div>
    </div>
  );
}
