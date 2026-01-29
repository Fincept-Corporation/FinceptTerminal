/**
 * Surface Analytics - Demo Data Generator
 * Generates realistic synthetic data for visualization when API is unavailable
 * Used as fallback when Databento API fails or for demo purposes
 */

import { useCallback } from 'react';
import type {
  VolatilitySurfaceData,
  CorrelationMatrixData,
  YieldCurveData,
  PCAData,
} from '../types';

/**
 * Generate realistic volatility smile/skew pattern
 */
function generateVolSmile(
  moneyness: number,  // Strike / Spot
  dte: number,        // Days to expiration
  baseVol: number = 0.20
): number {
  // ATM vol decreases with time (term structure)
  const atmVol = baseVol * (1 + 0.1 * Math.exp(-dte / 60));

  // Skew: OTM puts have higher IV (negative skew)
  const skew = -0.15 * (1 - moneyness);

  // Smile: both wings have elevated IV
  const smile = 0.08 * Math.pow(moneyness - 1, 2) * (1 + 30 / dte);

  // Random noise
  const noise = (Math.random() - 0.5) * 0.02;

  return Math.max(0.05, atmVol + skew + smile + noise);
}

/**
 * Generate demo volatility surface data
 */
export function generateDemoVolatilitySurface(
  symbol: string = 'SPY',
  spotPrice: number = 450
): VolatilitySurfaceData {
  // Generate strikes around spot price (80% to 120%)
  const numStrikes = 15;
  const strikes: number[] = [];
  for (let i = 0; i < numStrikes; i++) {
    const pct = 0.80 + (i / (numStrikes - 1)) * 0.40;
    strikes.push(Math.round(spotPrice * pct));
  }

  // Generate expirations (7 DTE to 180 DTE)
  const expirations = [7, 14, 21, 30, 45, 60, 90, 120, 180];

  // Generate IV surface
  const z: number[][] = [];
  for (const dte of expirations) {
    const row: number[] = [];
    for (const strike of strikes) {
      const moneyness = strike / spotPrice;
      const iv = generateVolSmile(moneyness, dte, 0.18);
      row.push(iv * 100); // Convert to percentage
    }
    z.push(row);
  }

  return {
    strikes,
    expirations,
    z,
    underlying: symbol,
    spotPrice,
    timestamp: Date.now(),
  };
}

/**
 * Generate demo correlation matrix with realistic asset correlations
 */
export function generateDemoCorrelationMatrix(
  assets: string[] = ['SPY', 'QQQ', 'IWM', 'GLD', 'TLT', 'VIX']
): CorrelationMatrixData {
  // Base correlation structure (realistic for these assets)
  const baseCorrelations: Record<string, Record<string, number>> = {
    SPY: { SPY: 1.0, QQQ: 0.92, IWM: 0.88, GLD: 0.05, TLT: -0.35, VIX: -0.75 },
    QQQ: { SPY: 0.92, QQQ: 1.0, IWM: 0.82, GLD: 0.02, TLT: -0.38, VIX: -0.72 },
    IWM: { SPY: 0.88, QQQ: 0.82, IWM: 1.0, GLD: 0.08, TLT: -0.30, VIX: -0.68 },
    GLD: { SPY: 0.05, QQQ: 0.02, IWM: 0.08, GLD: 1.0, TLT: 0.35, VIX: 0.10 },
    TLT: { SPY: -0.35, QQQ: -0.38, IWM: -0.30, GLD: 0.35, TLT: 1.0, VIX: 0.20 },
    VIX: { SPY: -0.75, QQQ: -0.72, IWM: -0.68, GLD: 0.10, TLT: 0.20, VIX: 1.0 },
  };

  // Generate time-varying correlations (30 time points)
  const numTimePoints = 30;
  const timePoints: number[] = [];
  const z: number[][] = [];

  for (let t = 0; t < numTimePoints; t++) {
    timePoints.push(t);
    const row: number[] = [];

    // Correlation regime changes over time
    const regimeShift = Math.sin(t / 10) * 0.1;

    for (let i = 0; i < assets.length; i++) {
      for (let j = 0; j < assets.length; j++) {
        const asset1 = assets[i];
        const asset2 = assets[j];

        let corr = baseCorrelations[asset1]?.[asset2] ?? 0.5;

        // Add time variation
        if (i !== j) {
          corr += regimeShift * (1 - Math.abs(corr));
          // Add noise
          corr += (Math.random() - 0.5) * 0.05;
          // Clamp to valid range
          corr = Math.max(-0.99, Math.min(0.99, corr));
        }

        row.push(corr);
      }
    }
    z.push(row);
  }

  return {
    assets,
    timePoints,
    z,
    window: 30,
    timestamp: Date.now(),
  };
}

/**
 * Generate demo yield curve evolution
 */
export function generateDemoYieldCurve(): YieldCurveData {
  // Treasury maturities in months
  const maturities = [1, 3, 6, 12, 24, 36, 60, 84, 120, 240, 360];

  // Generate historical yield curves (60 days)
  const numDays = 60;
  const timePoints: number[] = [];
  const z: number[][] = [];

  // Base yield curve (as of late 2024 - inverted)
  const baseYields = [5.25, 5.30, 5.15, 4.85, 4.30, 4.15, 4.10, 4.20, 4.35, 4.50, 4.55];

  for (let day = 0; day < numDays; day++) {
    timePoints.push(day);
    const row: number[] = [];

    // Simulate yield changes over time
    const trendShift = (day - numDays / 2) / numDays * 0.3;
    const volatility = Math.sin(day / 5) * 0.1;

    for (let i = 0; i < maturities.length; i++) {
      let yield_ = baseYields[i] + trendShift + volatility * (1 - i / maturities.length);
      yield_ += (Math.random() - 0.5) * 0.05;
      yield_ = Math.max(0.1, yield_);
      row.push(yield_);
    }
    z.push(row);
  }

  return {
    maturities,
    timePoints,
    z,
    source: 'DEMO',
    timestamp: Date.now(),
  };
}

/**
 * Generate demo PCA factor loadings
 */
export function generateDemoPCA(
  assets: string[] = ['SPY', 'QQQ', 'IWM', 'GLD', 'TLT', 'VIX', 'EEM', 'HYG']
): PCAData {
  const numFactors = Math.min(assets.length, 5);
  const factors = Array.from({ length: numFactors }, (_, i) => `PC${i + 1}`);

  // Realistic factor loadings
  // PC1: Market factor (all equities positive, bonds/gold negative)
  // PC2: Value vs Growth
  // PC3: Volatility factor
  // etc.
  const factorPatterns = [
    // PC1: Market
    { SPY: 0.95, QQQ: 0.90, IWM: 0.92, GLD: 0.15, TLT: -0.30, VIX: -0.80, EEM: 0.85, HYG: 0.60 },
    // PC2: Growth vs Value
    { SPY: 0.10, QQQ: 0.70, IWM: -0.50, GLD: -0.05, TLT: 0.05, VIX: -0.20, EEM: -0.30, HYG: -0.15 },
    // PC3: Risk-off
    { SPY: -0.15, QQQ: -0.20, IWM: -0.10, GLD: 0.65, TLT: 0.70, VIX: 0.30, EEM: -0.25, HYG: -0.35 },
    // PC4: EM specific
    { SPY: 0.05, QQQ: 0.00, IWM: 0.10, GLD: 0.20, TLT: -0.05, VIX: -0.10, EEM: 0.80, HYG: 0.25 },
    // PC5: Credit
    { SPY: 0.10, QQQ: 0.05, IWM: 0.15, GLD: -0.10, TLT: -0.25, VIX: -0.20, EEM: 0.15, HYG: 0.75 },
  ];

  const z: number[][] = [];
  for (const asset of assets) {
    const row: number[] = [];
    for (let f = 0; f < numFactors; f++) {
      const pattern = factorPatterns[f] || {};
      let loading = pattern[asset as keyof typeof pattern] ?? (Math.random() - 0.5) * 0.3;
      // Add small noise
      loading += (Math.random() - 0.5) * 0.05;
      row.push(loading);
    }
    z.push(row);
  }

  // Variance explained (typical PCA decay)
  const varianceExplained = [0.55, 0.18, 0.12, 0.08, 0.04].slice(0, numFactors);

  return {
    factors,
    assets,
    z,
    varianceExplained,
    timestamp: Date.now(),
  };
}

/**
 * Hook to generate demo data
 */
export function useDemoData() {
  const generateVolatilitySurface = useCallback((symbol: string, spotPrice: number) => {
    return generateDemoVolatilitySurface(symbol, spotPrice);
  }, []);

  const generateCorrelationMatrix = useCallback((assets: string[]) => {
    return generateDemoCorrelationMatrix(assets);
  }, []);

  const generateYieldCurve = useCallback(() => {
    return generateDemoYieldCurve();
  }, []);

  const generatePCA = useCallback((assets: string[]) => {
    return generateDemoPCA(assets);
  }, []);

  return {
    generateVolatilitySurface,
    generateCorrelationMatrix,
    generateYieldCurve,
    generatePCA,
  };
}
