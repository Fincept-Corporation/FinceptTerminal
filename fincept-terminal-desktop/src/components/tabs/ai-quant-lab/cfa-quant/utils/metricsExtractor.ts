/**
 * Metrics Extraction Utilities
 * Extracts key metrics from analysis results
 */

import { BB } from '../constants';
import { formatNumber, formatPrice } from './formatters';
import type { KeyMetric } from '../types';
import {
  ArrowUpRight,
  ArrowDownRight,
  Minus,
  CheckCircle2,
  AlertCircle,
  TrendingUp,
  Sparkles,
  Activity,
} from 'lucide-react';

export function extractKeyMetrics(
  selectedAnalysis: string,
  result: Record<string, unknown>,
  priceData: number[]
): KeyMetric[] {
  const metrics: KeyMetric[] = [];

  if (selectedAnalysis === 'trend_analysis' && result.metadata) {
    const meta = result.metadata as Record<string, unknown>;
    const value = result.value as Record<string, unknown>;
    const slope = value?.slope as number;
    const intercept = value?.intercept as number;
    const rSquared = meta?.r_squared as number;
    const significant = meta?.trend_significant as boolean;
    const direction = meta?.trend_direction as string;
    const pValue = meta?.slope_p_value as number;
    const tStat = meta?.slope_t_statistic as number;

    metrics.push({
      label: 'DIRECTION',
      value: direction === 'upward' ? 'UP' : direction === 'downward' ? 'DOWN' : 'FLAT',
      color: direction === 'upward' ? BB.green : direction === 'downward' ? BB.red : BB.textMuted,
      icon: direction === 'upward' ? ArrowUpRight : direction === 'downward' ? ArrowDownRight : Minus,
    });

    metrics.push({
      label: 'SIGNIFICANT',
      value: significant ? 'YES' : 'NO',
      color: significant ? BB.green : BB.textMuted,
      icon: significant ? CheckCircle2 : undefined
    });

    metrics.push({
      label: 'R-SQUARED',
      value: `${(rSquared * 100).toFixed(1)}%`,
      color: rSquared > 0.7 ? BB.green : rSquared > 0.4 ? BB.amber : BB.textMuted
    });

    if (pValue !== undefined && pValue !== null) {
      metrics.push({
        label: 'P-VALUE',
        value: pValue < 0.001 ? '<0.001' : formatNumber(pValue, 4),
        color: pValue < 0.01 ? BB.green : pValue < 0.05 ? BB.greenDim : pValue < 0.1 ? BB.amber : BB.red
      });
    }

    if (tStat !== undefined && tStat !== null) {
      metrics.push({
        label: 'T-STATISTIC',
        value: formatNumber(tStat, 2),
        color: Math.abs(tStat) > 2 ? BB.green : BB.textMuted
      });
    }

    metrics.push({ label: 'SLOPE', value: formatNumber(slope, 6), color: BB.textPrimary });
    if (intercept !== undefined && intercept !== null) {
      metrics.push({ label: 'INTERCEPT', value: formatNumber(intercept, 2), color: BB.textSecondary });
    }
  } else if (selectedAnalysis === 'stationarity_test' && result.value) {
    const value = result.value as Record<string, unknown>;
    const meta = result.metadata as Record<string, unknown>;
    const isStationary = value?.is_stationary as boolean;
    const pValue = value?.p_value as number;
    const testStat = value?.test_statistic as number;
    const criticalValues = value?.critical_values as Record<string, number>;

    metrics.push({
      label: 'STATIONARY',
      value: isStationary ? 'YES' : 'NO',
      color: isStationary ? BB.green : BB.red,
      icon: isStationary ? CheckCircle2 : AlertCircle
    });

    metrics.push({ label: 'TEST STAT', value: formatNumber(testStat, 4), color: BB.textPrimary });
    metrics.push({
      label: 'P-VALUE',
      value: formatNumber(pValue, 4),
      color: pValue < 0.01 ? BB.green : pValue < 0.05 ? BB.greenDim : pValue < 0.1 ? BB.amber : BB.red
    });

    if (criticalValues) {
      const cv1 = criticalValues['1%'];
      const cv5 = criticalValues['5%'];
      const cv10 = criticalValues['10%'];

      if (cv1 !== undefined) {
        const beats1 = testStat < cv1;
        metrics.push({ label: 'CV 1%', value: `${formatNumber(cv1, 2)} ${beats1 ? 'PASS' : ''}`, color: beats1 ? BB.green : BB.textMuted });
      }
      if (cv5 !== undefined) {
        const beats5 = testStat < cv5;
        metrics.push({ label: 'CV 5%', value: `${formatNumber(cv5, 2)} ${beats5 ? 'PASS' : ''}`, color: beats5 ? BB.green : BB.textMuted });
      }
      if (cv10 !== undefined) {
        const beats10 = testStat < cv10;
        metrics.push({ label: 'CV 10%', value: `${formatNumber(cv10, 2)} ${beats10 ? 'PASS' : ''}`, color: beats10 ? BB.amber : BB.textMuted });
      }
    }

    if (meta) {
      const rollingMeanStability = meta.rolling_mean_stability as number;
      const rollingStdStability = meta.rolling_std_stability as number;
      const meanReversionStrength = meta.mean_reversion_strength as number;

      if (rollingMeanStability !== undefined) {
        metrics.push({
          label: 'MEAN STABILITY',
          value: formatNumber(rollingMeanStability, 2),
          color: rollingMeanStability < 1 ? BB.green : rollingMeanStability < 5 ? BB.amber : BB.red
        });
      }
      if (rollingStdStability !== undefined) {
        metrics.push({
          label: 'VOL STABILITY',
          value: formatNumber(rollingStdStability, 2),
          color: rollingStdStability < 0.5 ? BB.green : rollingStdStability < 2 ? BB.amber : BB.red
        });
      }
      if (meanReversionStrength !== undefined) {
        metrics.push({
          label: 'MEAN REVERSION',
          value: `${(meanReversionStrength * 100).toFixed(1)}%`,
          color: meanReversionStrength > 0.3 ? BB.green : meanReversionStrength > 0.1 ? BB.amber : BB.textMuted
        });
      }
    }
  } else if (selectedAnalysis === 'arima_model' && result.metadata) {
    const meta = result.metadata as Record<string, unknown>;
    const params = result.parameters as Record<string, unknown>;
    const value = result.value as Record<string, unknown>;
    const residuals = value?.residuals as number[];

    const order = params?.order as number[];
    if (order && order.length === 3) {
      metrics.push({
        label: 'MODEL ORDER',
        value: `(${order[0]},${order[1]},${order[2]})`,
        color: BB.amber
      });
    }

    const seasonalOrder = params?.seasonal_order as number[];
    if (seasonalOrder && seasonalOrder.length === 4) {
      metrics.push({
        label: 'SEASONAL',
        value: `(${seasonalOrder[0]},${seasonalOrder[1]},${seasonalOrder[2]},${seasonalOrder[3]})`,
        color: BB.blueLight
      });
    }

    const aic = meta?.aic as number;
    const bic = meta?.bic as number;
    metrics.push({ label: 'AIC', value: formatNumber(aic, 2), color: BB.textPrimary });
    metrics.push({ label: 'BIC', value: formatNumber(bic, 2), color: BB.textPrimary });

    const logLik = meta?.log_likelihood as number;
    if (logLik !== undefined && logLik !== null) {
      metrics.push({ label: 'LOG-LIKELIHOOD', value: formatNumber(logLik, 2), color: BB.textSecondary });
    }

    const ljungBoxPValue = meta?.ljung_box_p_value as number;
    const residualsAutocorrelated = meta?.residuals_autocorrelated as boolean;

    metrics.push({
      label: 'RESIDUALS OK',
      value: !residualsAutocorrelated ? 'YES' : 'NO',
      color: !residualsAutocorrelated ? BB.green : BB.red,
      icon: !residualsAutocorrelated ? CheckCircle2 : AlertCircle
    });

    if (ljungBoxPValue !== undefined && ljungBoxPValue !== null) {
      metrics.push({
        label: 'LJUNG-BOX P',
        value: formatNumber(ljungBoxPValue, 4),
        color: ljungBoxPValue > 0.05 ? BB.green : ljungBoxPValue > 0.01 ? BB.amber : BB.red
      });
    }

    if (residuals && residuals.length > 0) {
      const residMean = residuals.reduce((a, b) => a + b, 0) / residuals.length;
      const residStd = Math.sqrt(residuals.reduce((a, b) => a + Math.pow(b - residMean, 2), 0) / residuals.length);
      metrics.push({ label: 'RESID MEAN', value: formatNumber(residMean, 4), color: Math.abs(residMean) < 0.01 ? BB.green : BB.textMuted });
      metrics.push({ label: 'RESID STD', value: formatNumber(residStd, 4), color: BB.textSecondary });
    }
  } else if (selectedAnalysis === 'forecasting' && result.metadata) {
    const meta = result.metadata as Record<string, unknown>;
    const params = result.parameters as Record<string, unknown>;
    const forecasts = result.value as number[];
    const trainSize = meta?.train_data_size as number;
    const testSize = meta?.test_data_size as number;
    const horizon = meta?.forecast_horizon as number;
    const method = params?.method as string;
    const mae = meta?.mae as number;
    const rmse = meta?.rmse as number;
    const mape = meta?.mape as number;

    if (method) {
      const methodLabels: Record<string, string> = {
        'simple_exponential': 'EXP SMOOTH',
        'linear_trend': 'LINEAR TREND',
        'moving_average': 'MOVING AVG'
      };
      metrics.push({
        label: 'METHOD',
        value: methodLabels[method] || method.toUpperCase(),
        color: BB.amber,
        icon: method === 'linear_trend' ? TrendingUp : method === 'simple_exponential' ? Sparkles : Activity
      });
    }

    if (mape !== undefined && mape !== null) {
      let accuracy = 'EXCELLENT';
      let accuracyColor = BB.green;
      if (mape > 30) { accuracy = 'POOR'; accuracyColor = BB.red; }
      else if (mape > 20) { accuracy = 'FAIR'; accuracyColor = BB.amber; }
      else if (mape > 10) { accuracy = 'GOOD'; accuracyColor = BB.greenDim; }
      metrics.push({ label: 'ACCURACY', value: accuracy, color: accuracyColor, icon: mape <= 10 ? CheckCircle2 : mape <= 20 ? AlertCircle : undefined });
    }

    if (forecasts && forecasts.length > 0) {
      const forecastValue = forecasts[0];
      metrics.push({ label: 'FORECAST VALUE', value: formatPrice(forecastValue), color: BB.green });

      if (testSize > 0 && priceData.length > 0) {
        const lastActual = priceData[priceData.length - 1];
        const diff = forecastValue - lastActual;
        const diffPct = (diff / lastActual) * 100;
        metrics.push({
          label: 'VS LAST ACTUAL',
          value: `${diffPct >= 0 ? '+' : ''}${diffPct.toFixed(2)}%`,
          color: diffPct >= 0 ? BB.green : BB.red,
          icon: diffPct >= 0 ? ArrowUpRight : ArrowDownRight
        });
      }
    }

    if (trainSize && testSize) {
      const trainPct = ((trainSize / (trainSize + testSize)) * 100).toFixed(0);
      metrics.push({ label: 'TRAIN/TEST', value: `${trainPct}/${100 - parseInt(trainPct)}%`, color: BB.blueLight });
    }
    if (trainSize) metrics.push({ label: 'TRAIN SIZE', value: String(trainSize), color: BB.textSecondary });
    if (testSize) metrics.push({ label: 'TEST SIZE', value: String(testSize), color: BB.textSecondary });
    if (horizon) metrics.push({ label: 'HORIZON', value: `${horizon} periods`, color: BB.green });

    if (mae !== undefined && mae !== null) {
      metrics.push({ label: 'MAE', value: formatNumber(mae, 2), color: BB.textPrimary });
    }
    if (rmse !== undefined && rmse !== null) {
      metrics.push({ label: 'RMSE', value: formatNumber(rmse, 2), color: BB.textPrimary });
      if (mae !== undefined && mae !== null && mae > 0) {
        const rmseToMae = rmse / mae;
        metrics.push({
          label: 'RMSE/MAE',
          value: formatNumber(rmseToMae, 2),
          color: rmseToMae < 1.2 ? BB.green : rmseToMae < 1.5 ? BB.amber : BB.red
        });
      }
    }
    if (mape !== undefined && mape !== null) {
      metrics.push({
        label: 'MAPE',
        value: `${formatNumber(mape, 2)}%`,
        color: mape < 10 ? BB.green : mape < 20 ? BB.amber : BB.red
      });
    }
  } else if (selectedAnalysis === 'supervised_learning' && result.value) {
    const meta = result.metadata as Record<string, unknown>;
    const value = result.value as Record<string, Record<string, unknown>>;
    const bestAlgo = meta?.best_algorithm as string;
    const nFeatures = meta?.n_features as number;

    if (bestAlgo) {
      metrics.push({ label: 'BEST MODEL', value: bestAlgo.toUpperCase().replace('_', ' '), color: BB.green, icon: CheckCircle2 });
    }

    if (bestAlgo && value[bestAlgo]) {
      const bestResult = value[bestAlgo];
      if (bestResult.r2_score !== undefined) {
        const r2 = bestResult.r2_score as number;
        const r2Quality = r2 > 0.8 ? 'Excellent' : r2 > 0.6 ? 'Good' : r2 > 0.4 ? 'Fair' : 'Poor';
        metrics.push({ label: 'R² SCORE', value: `${(r2 * 100).toFixed(1)}%`, color: r2 > 0.7 ? BB.green : r2 > 0.5 ? BB.amber : BB.red });
        metrics.push({ label: 'FIT QUALITY', value: r2Quality.toUpperCase(), color: r2 > 0.7 ? BB.green : r2 > 0.5 ? BB.amber : BB.red });
      }
      if (bestResult.rmse !== undefined) {
        metrics.push({ label: 'RMSE', value: formatNumber(bestResult.rmse as number, 4), color: BB.textPrimary });
      }
      if (bestResult.mse !== undefined) {
        metrics.push({ label: 'MSE', value: formatNumber(bestResult.mse as number, 4), color: BB.textSecondary });
      }
    }

    const algoNames = Object.keys(value).filter(k => !value[k].error);
    if (algoNames.length > 1) {
      metrics.push({ label: 'MODELS TESTED', value: String(algoNames.length), color: BB.blueLight });

      algoNames.forEach(algo => {
        if (algo !== bestAlgo && value[algo]?.r2_score !== undefined) {
          const r2 = value[algo].r2_score as number;
          metrics.push({
            label: algo.toUpperCase().replace('_', ' ').substring(0, 12),
            value: `R²: ${(r2 * 100).toFixed(1)}%`,
            color: BB.textMuted
          });
        }
      });
    }

    if (meta?.train_size) {
      metrics.push({ label: 'TRAIN SIZE', value: String(meta.train_size), color: BB.textMuted });
    }
    if (meta?.test_size) {
      metrics.push({ label: 'TEST SIZE', value: String(meta.test_size), color: BB.textMuted });
    }
    if (nFeatures) {
      metrics.push({ label: 'FEATURES', value: String(nFeatures), color: BB.textMuted });
    }
  } else if (selectedAnalysis === 'unsupervised_learning' && result.value) {
    const value = result.value as Record<string, Record<string, unknown>>;
    const meta = result.metadata as Record<string, unknown>;

    if (value.pca) {
      const pca = value.pca;
      const explainedVariance = pca.explained_variance_ratio as number[];
      const componentsFor95 = pca.components_for_95_variance as number;

      if (explainedVariance && explainedVariance.length > 0) {
        const firstComponentVar = (explainedVariance[0] * 100).toFixed(1);
        const totalComponents = explainedVariance.length;
        metrics.push({ label: 'PC1 VARIANCE', value: `${firstComponentVar}%`, color: BB.green });
        metrics.push({ label: 'COMPONENTS (95%)', value: String(componentsFor95), color: BB.amber });
        metrics.push({ label: 'TOTAL PCs', value: String(totalComponents), color: BB.textSecondary });
      }
    }

    if (value.kmeans) {
      const kmeans = value.kmeans;
      const optimalK = kmeans.optimal_k as number;
      const silhouetteByK = kmeans.silhouette_by_k as [number, number][];

      metrics.push({ label: 'OPTIMAL K', value: String(optimalK), color: BB.green });

      if (silhouetteByK && silhouetteByK.length > 0) {
        const optimalSilhouette = silhouetteByK.find(([k]) => k === optimalK);
        if (optimalSilhouette) {
          const score = optimalSilhouette[1];
          const quality = score > 0.7 ? 'STRONG' : score > 0.5 ? 'GOOD' : score > 0.25 ? 'FAIR' : 'WEAK';
          metrics.push({ label: 'SILHOUETTE', value: formatNumber(score, 3), color: score > 0.5 ? BB.green : BB.amber });
          metrics.push({ label: 'CLUSTERING', value: quality, color: score > 0.5 ? BB.green : BB.amber });
        }
      }

      const clusterLabels = kmeans.cluster_labels as number[];
      if (clusterLabels) {
        metrics.push({ label: 'DATA POINTS', value: String(clusterLabels.length), color: BB.textSecondary });
      }
    }

    if (meta?.n_samples) {
      metrics.push({ label: 'SAMPLES', value: String(meta.n_samples), color: BB.textMuted });
    }
    if (meta?.n_features) {
      metrics.push({ label: 'FEATURES', value: String(meta.n_features), color: BB.textMuted });
    }
  } else if (selectedAnalysis === 'resampling_methods' && result.value) {
    const value = result.value as Record<string, Record<string, unknown>>;
    const meta = result.metadata as Record<string, unknown>;

    if (value.bootstrap) {
      const bootstrap = value.bootstrap;
      const mean = bootstrap.mean as number;
      const std = bootstrap.std as number;
      const bias = bootstrap.bias as number;
      const ci = bootstrap.confidence_interval as [number, number];

      metrics.push({ label: 'BOOT MEAN', value: formatNumber(mean, 4), color: BB.green });
      metrics.push({ label: 'BOOT STD', value: formatNumber(std, 4), color: BB.textPrimary });
      if (bias !== undefined && bias !== null) {
        metrics.push({ label: 'BIAS', value: formatNumber(bias, 6), color: Math.abs(bias) < 0.01 ? BB.green : BB.amber });
      }
      if (ci && ci.length === 2) {
        metrics.push({ label: '95% CI LOW', value: formatNumber(ci[0], 4), color: BB.textSecondary });
        metrics.push({ label: '95% CI HIGH', value: formatNumber(ci[1], 4), color: BB.textSecondary });
      }
    }

    if (value.jackknife) {
      const jackknife = value.jackknife;
      const biasCorr = jackknife.bias_corrected_estimate as number;
      const stdErr = jackknife.std_error as number;
      const jackBias = jackknife.bias as number;

      if (biasCorr !== undefined) {
        metrics.push({ label: 'JACK ESTIMATE', value: formatNumber(biasCorr, 4), color: BB.amber });
      }
      if (stdErr !== undefined) {
        metrics.push({ label: 'STD ERROR', value: formatNumber(stdErr, 4), color: BB.textPrimary });
      }
      if (jackBias !== undefined) {
        metrics.push({ label: 'JACK BIAS', value: formatNumber(jackBias, 6), color: Math.abs(jackBias) < 0.01 ? BB.green : BB.amber });
      }
    }

    if (value.permutation) {
      const perm = value.permutation;
      const pValue = perm.p_value as number;
      const significant = perm.significant as boolean;

      metrics.push({ label: 'P-VALUE', value: formatNumber(pValue, 4), color: pValue < 0.05 ? BB.green : BB.textMuted });
      metrics.push({ label: 'SIGNIFICANT', value: significant ? 'YES' : 'NO', color: significant ? BB.green : BB.textMuted });
    }

    if (meta?.n_bootstrap) {
      metrics.push({ label: 'ITERATIONS', value: String(meta.n_bootstrap), color: BB.textMuted });
    }
    if (meta?.data_size) {
      metrics.push({ label: 'DATA SIZE', value: String(meta.data_size), color: BB.textMuted });
    }
  } else if (selectedAnalysis === 'validate_data' && result) {
    const qualityScore = result.quality_score as number;
    const issues = result.issues as { type: string; severity: string; description: string }[];
    const warnings = result.warnings as { type: string; message: string }[];
    const statistics = result.statistics as Record<string, unknown>;
    const recommendations = result.recommendations as string[];

    if (qualityScore !== undefined && qualityScore !== null) {
      let scoreColor = BB.green;
      let scoreLabel = 'EXCELLENT';
      if (qualityScore < 50) { scoreColor = BB.red; scoreLabel = 'POOR'; }
      else if (qualityScore < 70) { scoreColor = BB.amber; scoreLabel = 'FAIR'; }
      else if (qualityScore < 90) { scoreColor = BB.green; scoreLabel = 'GOOD'; }
      metrics.push({ label: 'QUALITY SCORE', value: `${qualityScore.toFixed(0)}/100`, color: scoreColor });
      metrics.push({ label: 'STATUS', value: scoreLabel, color: scoreColor });
    }

    if (statistics) {
      if (statistics.total_observations !== undefined) {
        metrics.push({ label: 'OBSERVATIONS', value: String(statistics.total_observations), color: BB.textPrimary });
      }
      if (statistics.missing_data_ratio !== undefined) {
        const missingRatio = statistics.missing_data_ratio as number;
        const missingPct = (missingRatio * 100).toFixed(1);
        metrics.push({
          label: 'MISSING DATA',
          value: `${missingPct}%`,
          color: missingRatio > 0.05 ? BB.red : missingRatio > 0.01 ? BB.amber : BB.green
        });
      }
      if (statistics.mean !== undefined) {
        metrics.push({ label: 'MEAN', value: formatNumber(statistics.mean as number, 2), color: BB.textSecondary });
      }
      if (statistics.std !== undefined) {
        metrics.push({ label: 'STD DEV', value: formatNumber(statistics.std as number, 2), color: BB.textSecondary });
      }
    }

    if (issues && issues.length > 0) {
      const criticalCount = issues.filter(i => i.severity === 'critical').length;
      const highCount = issues.filter(i => i.severity === 'high').length;
      const totalIssues = issues.length;

      metrics.push({ label: 'TOTAL ISSUES', value: String(totalIssues), color: totalIssues > 0 ? BB.amber : BB.green });
      if (criticalCount > 0) {
        metrics.push({ label: 'CRITICAL', value: String(criticalCount), color: BB.red });
      }
      if (highCount > 0) {
        metrics.push({ label: 'HIGH SEV', value: String(highCount), color: BB.red });
      }
    } else {
      metrics.push({ label: 'ISSUES', value: 'NONE', color: BB.green });
    }

    if (warnings && warnings.length > 0) {
      metrics.push({ label: 'WARNINGS', value: String(warnings.length), color: BB.amber });
    }

    if (recommendations && recommendations.length > 0) {
      metrics.push({ label: 'RECOMMENDATIONS', value: String(recommendations.length), color: BB.blueLight });
    }
  }

  return metrics;
}
