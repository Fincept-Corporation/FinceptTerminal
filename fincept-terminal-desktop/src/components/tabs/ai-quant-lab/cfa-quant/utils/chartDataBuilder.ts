/**
 * Chart Data Builder Utilities
 * Builds chart data for different analysis types
 */

import type { ChartDataPoint } from '../types';
import { calculateRollingStats, calculateZScores, createHistogramBins } from './calculations';
import { formatNumber } from './formatters';

export function buildResultChartData(
  selectedAnalysis: string,
  result: Record<string, unknown>,
  chartData: ChartDataPoint[],
  priceData: number[]
): any[] {
  if (selectedAnalysis === 'trend_analysis') {
    return buildTrendAnalysisChartData(result, chartData, priceData);
  } else if (selectedAnalysis === 'stationarity_test') {
    return buildStationarityChartData(chartData, priceData);
  } else if (selectedAnalysis === 'arima_model') {
    return buildArimaChartData(result, chartData, priceData);
  } else if (selectedAnalysis === 'forecasting') {
    return buildForecastingChartData(result, chartData, priceData);
  } else if (selectedAnalysis === 'supervised_learning') {
    return buildSupervisedLearningChartData(result, chartData, priceData);
  } else if (selectedAnalysis === 'unsupervised_learning') {
    return buildUnsupervisedLearningChartData(result, chartData);
  } else if (selectedAnalysis === 'resampling_methods') {
    return buildResamplingChartData(result, chartData);
  } else if (selectedAnalysis === 'validate_data') {
    return buildValidateDataChartData(chartData, priceData);
  }
  return chartData;
}

function buildTrendAnalysisChartData(
  result: Record<string, unknown>,
  chartData: ChartDataPoint[],
  priceData: number[]
): any[] {
  const meta = result.metadata as Record<string, unknown>;
  const value = result.value as Record<string, unknown>;
  const fitted = meta?.fitted_values as number[];
  const residuals = meta?.residuals as number[];
  const slope = value?.slope as number;
  const intercept = value?.intercept as number;

  if (fitted && fitted.length > 0) {
    const n = priceData.length;
    const residualsArr = residuals || priceData.map((v, i) => v - fitted[i]);
    const mse = residualsArr.reduce((sum, r) => sum + r * r, 0) / (n - 2);
    const se = Math.sqrt(mse);
    const meanX = (n - 1) / 2;
    const sumXX = Array.from({ length: n }, (_, i) => Math.pow(i - meanX, 2)).reduce((a, b) => a + b, 0);

    const enhancedData = chartData.map((d, i) => {
      const sePred = se * Math.sqrt(1 + 1/n + Math.pow(i - meanX, 2) / sumXX);
      const ciWidth = 1.96 * sePred;

      return {
        ...d,
        fitted: fitted[i],
        residual: residualsArr[i],
        upperBand: fitted[i] + ciWidth,
        lowerBand: fitted[i] - ciWidth,
      };
    });

    if (slope !== undefined && intercept !== undefined) {
      const projectionPeriods = 5;
      for (let i = 0; i < projectionPeriods; i++) {
        const futureIndex = n + i;
        const projectedValue = intercept + slope * futureIndex;
        const sePred = se * Math.sqrt(1 + 1/n + Math.pow(futureIndex - meanX, 2) / sumXX);
        const ciWidth = 1.96 * sePred;

        enhancedData.push({
          index: futureIndex,
          value: null as unknown as number,
          date: `+${i + 1}d`,
          fitted: null as unknown as number,
          residual: null as unknown as number,
          upperBand: null as unknown as number,
          lowerBand: null as unknown as number,
          projection: projectedValue,
          projectionUpper: projectedValue + ciWidth,
          projectionLower: projectedValue - ciWidth,
        } as any);
      }
    }

    return enhancedData;
  }
  return chartData;
}

function buildStationarityChartData(
  chartData: ChartDataPoint[],
  priceData: number[]
): any[] {
  const windowSize = Math.min(20, Math.floor(priceData.length / 4));
  if (windowSize < 2) return chartData;

  const { rollingMean, upperBand, lowerBand } = calculateRollingStats(priceData, windowSize);
  const overallMean = priceData.reduce((a, b) => a + b, 0) / priceData.length;

  return chartData.map((d, i) => ({
    ...d,
    rollingMean: rollingMean[i],
    upperBand: upperBand[i],
    lowerBand: lowerBand[i],
    overallMean,
  }));
}

function buildArimaChartData(
  result: Record<string, unknown>,
  chartData: ChartDataPoint[],
  priceData: number[]
): any[] {
  const value = result.value as Record<string, unknown>;
  const fitted = value?.fitted_values as number[];
  const residuals = value?.residuals as number[];
  const forecasts = value?.forecasts as number[];
  const forecastCI = value?.forecast_confidence_interval as { lower: number[]; upper: number[] };

  if (fitted && fitted.length > 0) {
    const n = priceData.length;
    const residualsArr = residuals || priceData.map((v, i) => v - (fitted[i] || v));
    const residMean = residualsArr.reduce((a, b) => a + b, 0) / residualsArr.length;
    const residStd = Math.sqrt(residualsArr.reduce((a, b) => a + Math.pow(b - residMean, 2), 0) / residualsArr.length);

    const enhancedData = chartData.map((d, i) => {
      const fittedVal = fitted[i];
      const residual = residualsArr[i];
      const ciWidth = 1.96 * residStd;

      return {
        ...d,
        fitted: fittedVal,
        residual: residual,
        upperBand: fittedVal !== undefined ? fittedVal + ciWidth : null,
        lowerBand: fittedVal !== undefined ? fittedVal - ciWidth : null,
        residualBar: residual,
      };
    });

    if (forecasts && forecasts.length > 0) {
      forecasts.forEach((forecast, i) => {
        const ciLower = forecastCI?.lower?.[i];
        const ciUpper = forecastCI?.upper?.[i];
        const defaultCiWidth = 1.96 * residStd * Math.sqrt(1 + (i + 1) * 0.1);

        enhancedData.push({
          index: n + i,
          value: null as unknown as number,
          date: `+${i + 1}d`,
          fitted: null as unknown as number,
          residual: null as unknown as number,
          upperBand: null as unknown as number,
          lowerBand: null as unknown as number,
          residualBar: null as unknown as number,
          forecast: forecast,
          forecastUpper: ciUpper ?? (forecast + defaultCiWidth),
          forecastLower: ciLower ?? (forecast - defaultCiWidth),
        } as any);
      });
    } else {
      const lastFitted = fitted[fitted.length - 1];
      const forecastHorizon = 5;

      for (let i = 0; i < forecastHorizon; i++) {
        const forecastVal = lastFitted;
        const ciWidth = 1.96 * residStd * Math.sqrt(1 + (i + 1) * 0.2);

        enhancedData.push({
          index: n + i,
          value: null as unknown as number,
          date: `+${i + 1}d`,
          fitted: null as unknown as number,
          residual: null as unknown as number,
          upperBand: null as unknown as number,
          lowerBand: null as unknown as number,
          residualBar: null as unknown as number,
          forecast: forecastVal,
          forecastUpper: forecastVal + ciWidth,
          forecastLower: forecastVal - ciWidth,
        } as any);
      }
    }

    return enhancedData;
  }
  return chartData;
}

function buildForecastingChartData(
  result: Record<string, unknown>,
  chartData: ChartDataPoint[],
  priceData: number[]
): any[] {
  const forecasts = result.value as number[];
  const meta = result.metadata as Record<string, unknown>;
  const trainSize = (meta?.train_data_size as number) || Math.floor(chartData.length * 0.8);
  const rmse = meta?.rmse as number;

  if (forecasts && forecasts.length > 0) {
    const forecastValue = forecasts[0];
    const trainData = priceData.slice(0, trainSize);
    const trainMean = trainData.reduce((a, b) => a + b, 0) / trainData.length;
    const trainStd = Math.sqrt(trainData.reduce((a, b) => a + Math.pow(b - trainMean, 2), 0) / trainData.length);
    const errorStd = rmse || trainStd * 0.3;
    const testData = priceData.slice(trainSize);
    const testErrors = testData.map(actual => actual - forecastValue);

    const enhancedData = chartData.map((d, i) => {
      const isTrainData = i < trainSize;
      const isTestData = i >= trainSize;
      const testIdx = i - trainSize;
      const error = isTestData && testIdx < testErrors.length ? testErrors[testIdx] : null;
      const absError = error !== null ? Math.abs(error) : null;

      return {
        ...d,
        trainValue: isTrainData ? d.value : null,
        testActual: isTestData ? d.value : null,
        forecastLevel: isTestData ? forecastValue : null,
        forecastUpper: isTestData ? forecastValue + 1.96 * errorStd : null,
        forecastLower: isTestData ? forecastValue - 1.96 * errorStd : null,
        forecastError: error,
        absError: absError,
      };
    });

    const futureForecasts = forecasts.map((v, i) => {
      const ciWidth = 1.96 * errorStd * Math.sqrt(1 + (i + 1) * 0.15);

      return {
        index: chartData.length + i,
        value: null,
        trainValue: null,
        testActual: null,
        forecastLevel: null,
        forecastUpper: null,
        forecastLower: null,
        forecastError: null,
        absError: null,
        forecast: v,
        futureForecastUpper: v + ciWidth,
        futureForecastLower: v - ciWidth,
        date: `+${i + 1}d`,
      };
    });

    if (enhancedData.length > 0 && trainSize > 0) {
      const lastTrainIdx = trainSize - 1;
      enhancedData[lastTrainIdx] = {
        ...enhancedData[lastTrainIdx],
        forecastStart: enhancedData[lastTrainIdx].value,
      } as any;
    }

    return [...enhancedData, ...futureForecasts];
  }
  return chartData;
}

function buildSupervisedLearningChartData(
  result: Record<string, unknown>,
  chartData: ChartDataPoint[],
  priceData: number[]
): any[] {
  const meta = result.metadata as Record<string, unknown>;
  const value = result.value as Record<string, Record<string, unknown>>;
  const bestAlgo = meta?.best_algorithm as string;

  if (bestAlgo && value[bestAlgo]) {
    const predictions = value[bestAlgo].predictions as number[];
    const yTest = meta?.y_test as number[];
    const rmse = value[bestAlgo].rmse as number;

    if (predictions && predictions.length > 0) {
      const nLags = 5;
      const totalSamples = chartData.length - nLags;
      const trainSize = (meta?.train_size as number) || Math.floor(totalSamples * 0.8);
      const testStartIndex = nLags + trainSize;

      const predictionErrors = yTest ? yTest.map((actual, i) => actual - predictions[i]) : [];
      const predStd = rmse || (predictionErrors.length > 0 ?
        Math.sqrt(predictionErrors.reduce((sum, e) => sum + e * e, 0) / predictionErrors.length) : 0);

      return chartData.map((d, i) => {
        const testIdx = i - testStartIndex;
        const isTestPoint = testIdx >= 0 && testIdx < predictions.length;
        const isTrainPoint = i >= nLags && i < testStartIndex;
        const pred = isTestPoint ? predictions[testIdx] : null;
        const actual = isTestPoint && yTest ? yTest[testIdx] : null;
        const error = (pred !== null && actual !== null) ? actual - pred : null;
        const ciWidth = 1.96 * predStd;

        return {
          ...d,
          trainValue: isTrainPoint ? d.value : null,
          prediction: pred,
          testActual: actual,
          predictionError: error,
          upperBand: pred !== null ? pred + ciWidth : null,
          lowerBand: pred !== null ? pred - ciWidth : null,
          zeroLine: isTestPoint ? 0 : null,
        };
      });
    }
  }
  return chartData;
}

function buildUnsupervisedLearningChartData(
  result: Record<string, unknown>,
  chartData: ChartDataPoint[]
): any[] {
  const value = result.value as Record<string, Record<string, unknown>>;

  if (value?.kmeans) {
    const clusterLabels = value.kmeans.cluster_labels as number[];
    if (clusterLabels && clusterLabels.length > 0) {
      return chartData.map((d, i) => ({
        ...d,
        cluster: i < clusterLabels.length ? clusterLabels[i] : 0,
      }));
    }
  }
  return chartData;
}

function buildResamplingChartData(
  result: Record<string, unknown>,
  chartData: ChartDataPoint[]
): any[] {
  const value = result.value as Record<string, Record<string, unknown>>;

  if (value?.bootstrap) {
    const bootstrap = value.bootstrap;
    const resampledStats = bootstrap.resampled_statistics as number[];
    const mean = bootstrap.mean as number;
    const ci = bootstrap.confidence_interval as [number, number];

    if (resampledStats && resampledStats.length > 0) {
      const sortedStats = [...resampledStats].sort((a, b) => a - b);
      const min = sortedStats[0];
      const max = sortedStats[sortedStats.length - 1];
      const binCount = Math.min(30, Math.ceil(Math.sqrt(sortedStats.length)));
      const binWidth = (max - min) / binCount;

      const findBinIndex = (val: number) => {
        const binIdx = Math.floor((val - min) / binWidth);
        return Math.max(0, Math.min(binCount - 1, binIdx));
      };

      const meanBinIdx = findBinIndex(mean);
      const ciLowIdx = ci ? findBinIndex(ci[0]) : 0;
      const ciHighIdx = ci ? findBinIndex(ci[1]) : binCount - 1;

      const bins: any[] = [];
      for (let i = 0; i < binCount; i++) {
        const binStart = min + i * binWidth;
        const binEnd = binStart + binWidth;
        const binCenter = (binStart + binEnd) / 2;
        const count = sortedStats.filter(v => v >= binStart && (i === binCount - 1 ? v <= binEnd : v < binEnd)).length;
        const inCI = i >= ciLowIdx && i <= ciHighIdx;
        const isMean = i === meanBinIdx;
        bins.push({
          index: i,
          value: count,
          binCenter,
          frequency: count / sortedStats.length,
          inCI,
          isMean,
          date: formatNumber(binCenter, 2),
          meanBinIdx,
          ciLowIdx,
          ciHighIdx,
        });
      }

      return bins;
    }
  }
  return chartData;
}

function buildValidateDataChartData(
  chartData: ChartDataPoint[],
  priceData: number[]
): any[] {
  if (priceData.length < 3) return chartData;

  const { zScores, mean } = calculateZScores(priceData);
  const outlierThreshold = 3.0;

  return chartData.map((d, i) => {
    const zScore = zScores[i] || 0;
    const isOutlier = zScore > outlierThreshold;
    const isSuspicious = zScore > 2.0 && zScore <= outlierThreshold;

    return {
      ...d,
      zScore,
      isOutlier,
      isSuspicious,
      normalValue: !isOutlier && !isSuspicious ? d.value : null,
      suspiciousValue: isSuspicious ? d.value : null,
      outlierValue: isOutlier ? d.value : null,
      mean,
    };
  });
}
