/**
 * OptionPricingMode Component
 * Option pricing and Greeks calculation
 */

import React from 'react';
import { DollarSign, Activity, RefreshCw } from 'lucide-react';
import { FINCEPT } from '../constants';
import { GreekRow } from '../components';
import type { OptionPricingModeProps } from '../types';

export const OptionPricingMode: React.FC<OptionPricingModeProps> = ({
  spotPrice,
  setSpotPrice,
  strike,
  setStrike,
  volatility,
  setVolatility,
  riskFreeRate,
  setRiskFreeRate,
  dividendYield,
  setDividendYield,
  timeToMaturity,
  setTimeToMaturity,
  isLoading,
  optionResult,
  greeksResult,
  runOptionPricing,
  runOptionGreeks
}) => {
  return (
    <div
      className="rounded border"
      style={{ backgroundColor: FINCEPT.PANEL_BG, borderColor: FINCEPT.BORDER }}
    >
      <div className="p-4 space-y-4">
        <div className="grid grid-cols-6 gap-4">
          <div>
            <label
              className="block text-xs font-mono mb-1"
              style={{ color: FINCEPT.GRAY }}
            >
              SPOT PRICE
            </label>
            <input
              type="number"
              value={spotPrice}
              onChange={(e) => setSpotPrice(parseFloat(e.target.value))}
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
              STRIKE
            </label>
            <input
              type="number"
              value={strike}
              onChange={(e) => setStrike(parseFloat(e.target.value))}
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
              VOLATILITY
            </label>
            <input
              type="number"
              value={volatility}
              onChange={(e) => setVolatility(parseFloat(e.target.value))}
              step="0.01"
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
              RISK-FREE RATE
            </label>
            <input
              type="number"
              value={riskFreeRate}
              onChange={(e) => setRiskFreeRate(parseFloat(e.target.value))}
              step="0.01"
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
              DIV YIELD
            </label>
            <input
              type="number"
              value={dividendYield}
              onChange={(e) => setDividendYield(parseFloat(e.target.value))}
              step="0.01"
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
              TIME (YEARS)
            </label>
            <input
              type="number"
              value={timeToMaturity}
              onChange={(e) => setTimeToMaturity(parseFloat(e.target.value))}
              step="0.1"
              className="w-full px-3 py-2 rounded text-sm font-mono border"
              style={{
                backgroundColor: FINCEPT.DARK_BG,
                borderColor: FINCEPT.BORDER,
                color: FINCEPT.WHITE
              }}
            />
          </div>
        </div>

        <div className="grid grid-cols-2 gap-4">
          <button
            onClick={runOptionPricing}
            disabled={isLoading}
            className="py-2.5 rounded font-bold uppercase tracking-wide text-sm hover:bg-opacity-90 transition-colors flex items-center justify-center gap-2 disabled:opacity-50"
            style={{ backgroundColor: FINCEPT.ORANGE, color: FINCEPT.DARK_BG }}
          >
            {isLoading ? (
              <RefreshCw size={16} className="animate-spin" />
            ) : (
              <DollarSign size={16} />
            )}
            PRICE OPTIONS
          </button>
          <button
            onClick={runOptionGreeks}
            disabled={isLoading}
            className="py-2.5 rounded font-bold uppercase tracking-wide text-sm hover:bg-opacity-90 transition-colors flex items-center justify-center gap-2 disabled:opacity-50"
            style={{ backgroundColor: FINCEPT.CYAN, color: FINCEPT.DARK_BG }}
          >
            {isLoading ? (
              <RefreshCw size={16} className="animate-spin" />
            ) : (
              <Activity size={16} />
            )}
            CALCULATE GREEKS
          </button>
        </div>

        {/* Option Results */}
        {optionResult && optionResult.success && (
          <div className="grid grid-cols-4 gap-4 mt-4">
            <div className="p-3 rounded" style={{ backgroundColor: FINCEPT.DARK_BG }}>
              <p className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>
                FORWARD
              </p>
              <p className="text-lg font-bold" style={{ color: FINCEPT.WHITE }}>
                ${optionResult.forward_price?.toFixed(2)}
              </p>
            </div>
            <div className="p-3 rounded" style={{ backgroundColor: FINCEPT.DARK_BG }}>
              <p className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>
                CALL PRICE
              </p>
              <p className="text-lg font-bold" style={{ color: FINCEPT.GREEN }}>
                ${optionResult.call_price?.toFixed(4)}
              </p>
            </div>
            <div className="p-3 rounded" style={{ backgroundColor: FINCEPT.DARK_BG }}>
              <p className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>
                PUT PRICE
              </p>
              <p className="text-lg font-bold" style={{ color: FINCEPT.RED }}>
                ${optionResult.put_price?.toFixed(4)}
              </p>
            </div>
            <div className="p-3 rounded" style={{ backgroundColor: FINCEPT.DARK_BG }}>
              <p className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>
                STRADDLE
              </p>
              <p className="text-lg font-bold" style={{ color: FINCEPT.CYAN }}>
                ${optionResult.straddle?.straddle_price?.toFixed(4)}
              </p>
            </div>
          </div>
        )}

        {/* Greeks Results */}
        {greeksResult && greeksResult.success && (
          <div
            className="mt-4 p-4 rounded border"
            style={{ backgroundColor: FINCEPT.DARK_BG, borderColor: FINCEPT.BORDER }}
          >
            <h3
              className="text-sm font-bold uppercase mb-4"
              style={{ color: FINCEPT.CYAN }}
            >
              OPTION GREEKS
            </h3>
            <div className="grid grid-cols-2 gap-6">
              {/* Call Greeks */}
              <div>
                <h4
                  className="text-xs font-bold uppercase mb-3"
                  style={{ color: FINCEPT.GREEN }}
                >
                  CALL OPTION
                </h4>
                <div className="space-y-2">
                  <GreekRow
                    label="Delta (Δ)"
                    value={greeksResult.call?.delta}
                    description="Price sensitivity"
                  />
                  <GreekRow
                    label="Gamma (Γ)"
                    value={greeksResult.call?.gamma}
                    description="Delta sensitivity"
                  />
                  <GreekRow
                    label="Vega (ν)"
                    value={greeksResult.call?.vega}
                    description="Volatility sensitivity"
                  />
                  <GreekRow
                    label="Theta (Θ)"
                    value={greeksResult.call?.theta}
                    description="Time decay (daily)"
                  />
                  <GreekRow
                    label="Rho (ρ)"
                    value={greeksResult.call?.rho}
                    description="Rate sensitivity"
                  />
                </div>
              </div>
              {/* Put Greeks */}
              <div>
                <h4
                  className="text-xs font-bold uppercase mb-3"
                  style={{ color: FINCEPT.RED }}
                >
                  PUT OPTION
                </h4>
                <div className="space-y-2">
                  <GreekRow
                    label="Delta (Δ)"
                    value={greeksResult.put?.delta}
                    description="Price sensitivity"
                  />
                  <GreekRow
                    label="Gamma (Γ)"
                    value={greeksResult.put?.gamma}
                    description="Delta sensitivity"
                  />
                  <GreekRow
                    label="Vega (ν)"
                    value={greeksResult.put?.vega}
                    description="Volatility sensitivity"
                  />
                  <GreekRow
                    label="Theta (Θ)"
                    value={greeksResult.put?.theta}
                    description="Time decay (daily)"
                  />
                  <GreekRow
                    label="Rho (ρ)"
                    value={greeksResult.put?.rho}
                    description="Rate sensitivity"
                  />
                </div>
              </div>
            </div>
          </div>
        )}
      </div>
    </div>
  );
};
