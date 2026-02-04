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
          type="text"
          inputMode="decimal"
          value={config.risk_free_rate}
          onChange={(e) => {
            const v = e.target.value;
            if (v === '' || /^\d*\.?\d*$/.test(v)) {
              setConfig({ ...config, risk_free_rate: parseFloat(v) || 0 });
            }
          }}
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
          type="text"
          inputMode="decimal"
          value={config.drawdown_threshold}
          onChange={(e) => {
            const v = e.target.value;
            if (v === '' || /^\d*\.?\d*$/.test(v)) {
              setConfig({ ...config, drawdown_threshold: parseFloat(v) || 0.05 });
            }
          }}
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
