/**
 * OptimizationMethodSelector Component
 * Grid of optimization method buttons
 */

import React from 'react';
import { FINCEPT } from '../constants';
import type { OptimizationMethodSelectorProps, OptimizationMethod } from '../types';

interface MethodOption {
  method: OptimizationMethod;
  label: string;
}

const METHOD_OPTIONS: MethodOption[] = [
  { method: 'erc', label: 'ERC' },
  { method: 'inv_vol', label: 'Inv Vol' },
  { method: 'mean_var', label: 'Mean-Var' },
  { method: 'equal', label: 'Equal Wt' }
];

export function OptimizationMethodSelector({
  optimizationMethod,
  setOptimizationMethod
}: OptimizationMethodSelectorProps) {
  return (
    <div className="px-2 py-2">
      <label className="block text-xs font-mono mb-2" style={{ color: FINCEPT.GRAY }}>
        OPTIMIZATION METHOD
      </label>
      <div className="grid grid-cols-2 gap-1">
        {METHOD_OPTIONS.map(({ method, label }) => (
          <button
            key={method}
            onClick={() => setOptimizationMethod(method)}
            className="px-2 py-1.5 rounded text-xs font-mono"
            style={{
              backgroundColor: optimizationMethod === method ? FINCEPT.CYAN : 'transparent',
              color: optimizationMethod === method ? FINCEPT.DARK_BG : FINCEPT.GRAY,
              border: `1px solid ${FINCEPT.BORDER}`
            }}
          >
            {label}
          </button>
        ))}
      </div>
    </div>
  );
}
