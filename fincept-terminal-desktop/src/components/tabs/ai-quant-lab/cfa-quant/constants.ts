/**
 * CFA Quant Panel Constants
 * Color palette and analysis configurations
 */

import {
  TrendingUp,
  BarChart3,
  LineChart,
  Sparkles,
  Brain,
  Target,
  Shuffle,
  CheckCircle2,
} from 'lucide-react';
import type { AnalysisConfig } from './types';

// Fincept-style color palette - AMBER THEME for CFA Quant
export const BB = {
  // Core Fincept colors
  black: '#000000',
  darkBg: '#0F0F0F',
  panelBg: '#0F0F0F',
  cardBg: '#1F1F1F',
  borderDark: '#2A2A2A',
  borderLight: '#3a3a3a',
  hover: '#1F1F1F',

  // Accent colors - AMBER THEME
  amber: '#FF8800',      // Primary theme color (matching FINCEPT.ORANGE)
  amberLight: '#FFA500',
  amberDim: '#CC7000',

  // Status colors
  green: '#00D66F',
  greenDim: '#00A040',
  red: '#FF3B3B',
  redDim: '#CC3000',

  // Text colors
  textPrimary: '#FFFFFF',
  textSecondary: '#B0B0B0',
  textMuted: '#787878',
  textAmber: '#FF8800',

  // Chart colors
  chartLine: '#00D4FF',
  chartArea: 'rgba(0, 212, 255, 0.1)',
  chartGrid: '#1f1f1f',

  // Fincept blue
  blue: '#0088FF',
  blueLight: '#0088FF',
};

export const analysisConfigs: AnalysisConfig[] = [
  {
    id: 'trend_analysis',
    name: 'Trend Analysis',
    shortName: 'TREND',
    description: 'Detect upward/downward trends',
    icon: TrendingUp,
    category: 'time_series',
    color: BB.green,
    minDataPoints: 5,
    helpText: 'Linear regression to identify significant trends in price movements.'
  },
  {
    id: 'stationarity_test',
    name: 'Stationarity Test',
    shortName: 'ADF',
    description: 'Unit root test (ADF)',
    icon: BarChart3,
    category: 'time_series',
    color: BB.blue,
    minDataPoints: 20,
    helpText: 'Tests if time series has constant mean/variance. Important for ARIMA modeling.'
  },
  {
    id: 'arima_model',
    name: 'ARIMA Forecast',
    shortName: 'ARIMA',
    description: 'Time series prediction',
    icon: LineChart,
    category: 'time_series',
    color: BB.amber,
    minDataPoints: 20,
    helpText: 'AutoRegressive Integrated Moving Average for forecasting.'
  },
  {
    id: 'forecasting',
    name: 'Exp. Smoothing',
    shortName: 'ETS',
    description: 'Simple forecasting',
    icon: Sparkles,
    category: 'time_series',
    color: BB.amberLight,
    minDataPoints: 10,
    helpText: 'Exponential smoothing methods for quick predictions.'
  },
  {
    id: 'supervised_learning',
    name: 'ML Prediction',
    shortName: 'ML',
    description: 'Machine learning models',
    icon: Brain,
    category: 'machine_learning',
    color: BB.blueLight,
    minDataPoints: 30,
    helpText: 'Compare Ridge, Lasso, Random Forest for predictions.'
  },
  {
    id: 'unsupervised_learning',
    name: 'Pattern Discovery',
    shortName: 'PCA/K',
    description: 'PCA and clustering',
    icon: Target,
    category: 'machine_learning',
    color: BB.amber,
    minDataPoints: 20,
    helpText: 'Find hidden patterns with PCA and K-Means clustering.'
  },
  {
    id: 'resampling_methods',
    name: 'Bootstrap',
    shortName: 'BOOT',
    description: 'Confidence intervals',
    icon: Shuffle,
    category: 'sampling',
    color: BB.green,
    minDataPoints: 10,
    helpText: 'Bootstrap/jackknife for robust statistical inference.'
  },
  {
    id: 'validate_data',
    name: 'Data Quality',
    shortName: 'QA',
    description: 'Validate data quality',
    icon: CheckCircle2,
    category: 'data_quality',
    color: BB.greenDim,
    minDataPoints: 5,
    helpText: 'Check for missing values, outliers, and data issues.'
  },
];
