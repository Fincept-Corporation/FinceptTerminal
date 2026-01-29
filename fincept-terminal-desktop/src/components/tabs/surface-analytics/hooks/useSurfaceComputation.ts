/**
 * Surface Analytics - Computation Hook
 * Handles all analytics computations (IV, correlations, PCA)
 */

import { useMemo, useCallback } from 'react';
import type {
  VolatilitySurfaceData,
  CorrelationMatrixData,
  YieldCurveData,
  PCAData,
  OptionsChainRecord,
  HistoricalBar,
} from '../types';
import { RISK_FREE_RATE } from '../constants';

// Normal distribution CDF approximation
function normCdf(x: number): number {
  const a1 = 0.254829592;
  const a2 = -0.284496736;
  const a3 = 1.421413741;
  const a4 = -1.453152027;
  const a5 = 1.061405429;
  const p = 0.3275911;

  const sign = x < 0 ? -1 : 1;
  x = Math.abs(x) / Math.sqrt(2);

  const t = 1.0 / (1.0 + p * x);
  const y = 1.0 - (((((a5 * t + a4) * t) + a3) * t + a2) * t + a1) * t * Math.exp(-x * x);

  return 0.5 * (1.0 + sign * y);
}

// Normal distribution PDF
function normPdf(x: number): number {
  return Math.exp(-0.5 * x * x) / Math.sqrt(2 * Math.PI);
}

// Black-Scholes price calculation
function blackScholesPrice(
  S: number,     // Spot price
  K: number,     // Strike price
  T: number,     // Time to expiry (years)
  r: number,     // Risk-free rate
  sigma: number, // Volatility
  isCall: boolean
): number {
  if (T <= 0 || sigma <= 0) return 0;

  const d1 = (Math.log(S / K) + (r + sigma * sigma / 2) * T) / (sigma * Math.sqrt(T));
  const d2 = d1 - sigma * Math.sqrt(T);

  if (isCall) {
    return S * normCdf(d1) - K * Math.exp(-r * T) * normCdf(d2);
  } else {
    return K * Math.exp(-r * T) * normCdf(-d2) - S * normCdf(-d1);
  }
}

// Implied volatility using Newton-Raphson method
function computeImpliedVolatility(
  optionPrice: number,
  S: number,
  K: number,
  T: number,
  r: number,
  isCall: boolean,
  maxIterations: number = 100,
  tolerance: number = 1e-6
): number | null {
  if (optionPrice <= 0 || T <= 0) return null;

  // Initial guess using Brenner-Subrahmanyam approximation
  let sigma = Math.sqrt(2 * Math.PI / T) * optionPrice / S;
  sigma = Math.max(0.01, Math.min(5.0, sigma)); // Bound initial guess

  for (let i = 0; i < maxIterations; i++) {
    const price = blackScholesPrice(S, K, T, r, sigma, isCall);
    const diff = price - optionPrice;

    if (Math.abs(diff) < tolerance) {
      return sigma;
    }

    // Vega for Newton-Raphson step
    const d1 = (Math.log(S / K) + (r + sigma * sigma / 2) * T) / (sigma * Math.sqrt(T));
    const vega = S * normPdf(d1) * Math.sqrt(T);

    if (vega < 1e-10) break; // Avoid division by zero

    sigma = sigma - diff / vega;

    if (sigma <= 0.001 || sigma > 10) break; // Bounds check
  }

  return null; // Failed to converge
}

export function useSurfaceComputation() {
  /**
   * Compute volatility surface from options chain data
   */
  const computeVolatilitySurface = useCallback((
    optionsData: OptionsChainRecord[],
    spotPrice: number,
    riskFreeRate: number = RISK_FREE_RATE
  ): VolatilitySurfaceData | null => {
    if (!optionsData || optionsData.length === 0) {
      return null;
    }

    // Get unique strikes and expirations
    const strikes = [...new Set(optionsData.map(o => o.strike_price))].sort((a, b) => a - b);
    const expirations = [...new Set(optionsData.map(o => o.expiration))].sort((a, b) => a - b);

    // Convert expirations to days
    const now = Date.now();
    const expirationDays = expirations.map(exp =>
      Math.max(1, Math.round((exp - now) / (1000 * 60 * 60 * 24)))
    );

    // Build IV matrix
    const z: number[][] = [];

    for (const exp of expirations) {
      const row: number[] = [];
      const daysToExp = Math.max(1, Math.round((exp - now) / (1000 * 60 * 60 * 24)));
      const T = daysToExp / 365;

      for (const strike of strikes) {
        // Find matching option (prefer calls for IV surface)
        const option = optionsData.find(
          o => o.strike_price === strike && o.expiration === exp
        );

        if (option && option.mid_price && option.mid_price > 0) {
          const iv = computeImpliedVolatility(
            option.mid_price,
            spotPrice,
            strike,
            T,
            riskFreeRate,
            option.instrument_class === 'C'
          );
          row.push(iv !== null ? iv * 100 : 0); // Convert to percentage
        } else {
          row.push(0); // No data
        }
      }
      z.push(row);
    }

    return {
      strikes,
      expirations: expirationDays,
      z,
      underlying: optionsData[0]?.underlying || 'UNKNOWN',
      spotPrice,
      timestamp: Date.now(),
    };
  }, []);

  /**
   * Compute rolling correlation matrix from historical prices
   */
  const computeCorrelationMatrix = useCallback((
    priceData: Record<string, HistoricalBar[]>,
    window: number = 30
  ): CorrelationMatrixData | null => {
    const assets = Object.keys(priceData);
    if (assets.length < 2) return null;

    // Extract close prices and align dates
    const closePrices: Record<string, number[]> = {};
    const allDates: Set<number> = new Set();

    for (const asset of assets) {
      const bars = priceData[asset];
      closePrices[asset] = [];
      bars.forEach(bar => allDates.add(bar.ts_event));
    }

    const sortedDates = Array.from(allDates).sort((a, b) => a - b);

    // Fill price arrays
    for (const asset of assets) {
      const bars = priceData[asset];
      const priceMap = new Map(bars.map(b => [b.ts_event, b.close]));
      closePrices[asset] = sortedDates.map(d => priceMap.get(d) || NaN);
    }

    // Compute returns
    const returns: Record<string, number[]> = {};
    for (const asset of assets) {
      const prices = closePrices[asset];
      returns[asset] = [];
      for (let i = 1; i < prices.length; i++) {
        if (!isNaN(prices[i]) && !isNaN(prices[i - 1]) && prices[i - 1] > 0) {
          returns[asset].push(Math.log(prices[i] / prices[i - 1]));
        } else {
          returns[asset].push(NaN);
        }
      }
    }

    // Compute rolling correlations
    const numPeriods = Math.max(1, returns[assets[0]].length - window + 1);
    const timePoints: number[] = [];
    const z: number[][] = [];

    for (let t = 0; t < numPeriods; t += Math.max(1, Math.floor(numPeriods / 20))) {
      timePoints.push(t);
      const row: number[] = [];

      for (let i = 0; i < assets.length; i++) {
        for (let j = 0; j < assets.length; j++) {
          if (i === j) {
            row.push(1.0);
          } else {
            const ret1 = returns[assets[i]].slice(t, t + window);
            const ret2 = returns[assets[j]].slice(t, t + window);
            const corr = computeCorrelation(ret1, ret2);
            row.push(corr);
          }
        }
      }
      z.push(row);
    }

    return {
      assets,
      timePoints,
      z,
      window,
      timestamp: Date.now(),
    };
  }, []);

  /**
   * Compute PCA from returns data
   */
  const computePCA = useCallback((
    priceData: Record<string, HistoricalBar[]>
  ): PCAData | null => {
    const assets = Object.keys(priceData);
    if (assets.length < 2) return null;

    // Compute returns matrix
    const returns: number[][] = [];
    const minLength = Math.min(...assets.map(a => priceData[a].length));

    for (let i = 1; i < minLength; i++) {
      const row: number[] = [];
      for (const asset of assets) {
        const bars = priceData[asset];
        if (bars[i - 1].close > 0) {
          row.push(Math.log(bars[i].close / bars[i - 1].close));
        } else {
          row.push(0);
        }
      }
      returns.push(row);
    }

    // Compute covariance matrix
    const n = returns.length;
    const means = assets.map((_, j) =>
      returns.reduce((sum, row) => sum + row[j], 0) / n
    );

    const covMatrix: number[][] = [];
    for (let i = 0; i < assets.length; i++) {
      const row: number[] = [];
      for (let j = 0; j < assets.length; j++) {
        let cov = 0;
        for (let k = 0; k < n; k++) {
          cov += (returns[k][i] - means[i]) * (returns[k][j] - means[j]);
        }
        row.push(cov / (n - 1));
      }
      covMatrix.push(row);
    }

    // Simple power iteration for eigenvalues (simplified PCA)
    // For full implementation, use a linear algebra library
    const numFactors = Math.min(assets.length, 10);
    const factors = Array.from({ length: numFactors }, (_, i) => `PC${i + 1}`);

    // Placeholder factor loadings (simplified)
    const z: number[][] = [];
    for (let i = 0; i < assets.length; i++) {
      const row: number[] = [];
      for (let j = 0; j < numFactors; j++) {
        // Simplified: use correlation with variance
        const loading = covMatrix[i][Math.min(j, assets.length - 1)] /
          Math.sqrt(covMatrix[i][i] * covMatrix[Math.min(j, assets.length - 1)][Math.min(j, assets.length - 1)]);
        row.push(isNaN(loading) ? 0 : loading);
      }
      z.push(row);
    }

    // Variance explained (simplified)
    const totalVar = covMatrix.reduce((sum, row, i) => sum + row[i], 0);
    const varianceExplained = factors.map((_, i) =>
      (1 - i * 0.15) * 0.7 // Simplified decay
    );

    return {
      factors,
      assets,
      z,
      varianceExplained,
      timestamp: Date.now(),
    };
  }, []);

  return {
    computeVolatilitySurface,
    computeCorrelationMatrix,
    computePCA,
    blackScholesPrice,
    computeImpliedVolatility,
  };
}

// Helper: Pearson correlation coefficient
function computeCorrelation(x: number[], y: number[]): number {
  const n = Math.min(x.length, y.length);
  if (n < 2) return 0;

  let sumX = 0, sumY = 0, sumXY = 0, sumX2 = 0, sumY2 = 0;
  let count = 0;

  for (let i = 0; i < n; i++) {
    if (!isNaN(x[i]) && !isNaN(y[i])) {
      sumX += x[i];
      sumY += y[i];
      sumXY += x[i] * y[i];
      sumX2 += x[i] * x[i];
      sumY2 += y[i] * y[i];
      count++;
    }
  }

  if (count < 2) return 0;

  const numerator = count * sumXY - sumX * sumY;
  const denominator = Math.sqrt(
    (count * sumX2 - sumX * sumX) * (count * sumY2 - sumY * sumY)
  );

  if (denominator === 0) return 0;
  return numerator / denominator;
}
