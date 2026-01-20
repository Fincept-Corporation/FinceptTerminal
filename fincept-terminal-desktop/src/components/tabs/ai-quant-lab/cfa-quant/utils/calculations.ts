/**
 * CFA Quant Calculation Utilities
 */

import type { DataStats } from '../types';

export const calculateStats = (data: number[]): DataStats | null => {
  if (data.length === 0) return null;
  const mean = data.reduce((a, b) => a + b, 0) / data.length;
  const variance = data.reduce((a, b) => a + Math.pow(b - mean, 2), 0) / data.length;
  const first = data[0];
  const last = data[data.length - 1];
  const change = last - first;
  const changePercent = (change / first) * 100;
  return {
    min: Math.min(...data),
    max: Math.max(...data),
    mean,
    std: Math.sqrt(variance),
    change,
    changePercent,
  };
};

export const calculateRollingStats = (
  data: number[],
  windowSize: number
): {
  rollingMean: (number | null)[];
  rollingStd: (number | null)[];
  upperBand: (number | null)[];
  lowerBand: (number | null)[];
} => {
  const rollingMean: (number | null)[] = [];
  const rollingStd: (number | null)[] = [];
  const upperBand: (number | null)[] = [];
  const lowerBand: (number | null)[] = [];

  for (let i = 0; i < data.length; i++) {
    if (i < windowSize - 1) {
      rollingMean.push(null);
      rollingStd.push(null);
      upperBand.push(null);
      lowerBand.push(null);
    } else {
      const windowData = data.slice(i - windowSize + 1, i + 1);
      const mean = windowData.reduce((a, b) => a + b, 0) / windowData.length;
      const variance = windowData.reduce((a, b) => a + Math.pow(b - mean, 2), 0) / windowData.length;
      const std = Math.sqrt(variance);

      rollingMean.push(mean);
      rollingStd.push(std);
      upperBand.push(mean + 2 * std);
      lowerBand.push(mean - 2 * std);
    }
  }

  return { rollingMean, rollingStd, upperBand, lowerBand };
};

export const calculateZScores = (
  data: number[]
): { zScores: number[]; mean: number; std: number } => {
  if (data.length < 3) return { zScores: [], mean: 0, std: 0 };

  const mean = data.reduce((a, b) => a + b, 0) / data.length;
  const variance = data.reduce((a, b) => a + Math.pow(b - mean, 2), 0) / data.length;
  const std = Math.sqrt(variance);

  const zScores = data.map(value => std > 0 ? Math.abs((value - mean) / std) : 0);

  return { zScores, mean, std };
};

export const createHistogramBins = (
  data: number[],
  binCount: number
): { binCenter: number; count: number; frequency: number }[] => {
  if (data.length === 0) return [];

  const sortedData = [...data].sort((a, b) => a - b);
  const min = sortedData[0];
  const max = sortedData[sortedData.length - 1];
  const binWidth = (max - min) / binCount;

  const bins: { binCenter: number; count: number; frequency: number }[] = [];

  for (let i = 0; i < binCount; i++) {
    const binStart = min + i * binWidth;
    const binEnd = binStart + binWidth;
    const binCenter = (binStart + binEnd) / 2;
    const count = sortedData.filter(v =>
      v >= binStart && (i === binCount - 1 ? v <= binEnd : v < binEnd)
    ).length;

    bins.push({
      binCenter,
      count,
      frequency: count / sortedData.length,
    });
  }

  return bins;
};
