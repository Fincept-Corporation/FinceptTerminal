/**
 * BenchmarkInput Component
 * Input for benchmark symbol
 */

import React from 'react';
import { FINCEPT, COMMON_BENCHMARKS } from '../constants';
import type { BenchmarkInputProps } from '../types';

export function BenchmarkInput({
  benchmarkSymbol,
  setBenchmarkSymbol
}: BenchmarkInputProps) {
  return (
    <div className="px-2 py-2">
      <label className="block text-xs font-mono mb-2" style={{ color: FINCEPT.GRAY }}>
        BENCHMARK SYMBOL
      </label>
      <input
        type="text"
        value={benchmarkSymbol}
        onChange={(e) => setBenchmarkSymbol(e.target.value.toUpperCase())}
        placeholder="SPY"
        className="w-full p-2 rounded text-xs font-mono"
        style={{
          backgroundColor: FINCEPT.DARK_BG,
          color: FINCEPT.WHITE,
          border: `1px solid ${FINCEPT.BORDER}`
        }}
      />
      <p className="text-xs font-mono mt-1" style={{ color: FINCEPT.MUTED }}>
        Common: {COMMON_BENCHMARKS.join(', ')}
      </p>
    </div>
  );
}
