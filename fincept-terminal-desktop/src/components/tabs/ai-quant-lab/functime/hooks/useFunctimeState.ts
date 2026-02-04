/**
 * useFunctimeState Hook
 * Manages all state for the Functime Panel
 */

import { useState, useEffect, useMemo, useCallback } from 'react';
import {
  functimeService,
  type AnomalyDetectionConfig,
  type SeasonalityConfig
} from '@/services/aiQuantLab/functimeService';
import {
  portfolioFunctimeService,
  type PortfolioPriceData,
  type PortfolioAnomalyResult,
  type PortfolioSeasonalityResult
} from '@/services/aiQuantLab/portfolioFunctimeService';
import type { Portfolio } from '@/services/portfolio/portfolioService';
import { parseHistoricalData } from '../utils';
import type {
  DataSourceType,
  AnalysisMode,
  PreprocessType,
  AnomalyMethod,
  ForecastAnalysisResult,
  ExpandedSections
} from '../types';

export function useFunctimeState() {
  // Status
  const [isFunctimeAvailable, setIsFunctimeAvailable] = useState<boolean | null>(null);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [analysisResult, setAnalysisResult] = useState<ForecastAnalysisResult | null>(null);
  const [dataInput, setDataInput] = useState('[]');

  // Data source configuration
  const [dataSourceType, setDataSourceType] = useState<DataSourceType>('symbol');
  const [portfolios, setPortfolios] = useState<Portfolio[]>([]);
  const [selectedPortfolioId, setSelectedPortfolioId] = useState<string>('');
  const [symbolInput, setSymbolInput] = useState('AAPL');
  const [historicalDays, setHistoricalDays] = useState(365);
  const [priceDataLoading, setPriceDataLoading] = useState(false);
  const [priceData, setPriceData] = useState<PortfolioPriceData | null>(null);

  // Analysis mode
  const [analysisMode, setAnalysisMode] = useState<AnalysisMode>('forecast');

  // Forecast configuration
  const [selectedModel, setSelectedModel] = useState('linear');
  const [horizon, setHorizon] = useState(7);
  const [frequency, setFrequency] = useState('1d');
  const [alpha, setAlpha] = useState(1.0);
  const [testSize, setTestSize] = useState(5);
  const [preprocess, setPreprocess] = useState<PreprocessType>('none');

  // Anomaly detection state
  const [anomalyMethod, setAnomalyMethod] = useState<AnomalyMethod>('zscore');
  const [anomalyThreshold, setAnomalyThreshold] = useState(3.0);
  const [anomalyResult, setAnomalyResult] = useState<PortfolioAnomalyResult | null>(null);

  // Seasonality state
  const [seasonalityResult, setSeasonalityResult] = useState<PortfolioSeasonalityResult | null>(null);

  // UI state
  const [expandedSections, setExpandedSections] = useState<ExpandedSections>({
    config: true,
    forecast: true,
    metrics: true,
    summary: true,
    chart: true,
    dataSource: true,
    anomaly: true,
    seasonality: true,
    backtest: true
  });

  // Check functime availability on mount
  useEffect(() => {
    checkFunctimeStatus();
    loadPortfolios();
    // Auto-fetch data on mount if using symbol mode
    if (dataSourceType === 'symbol') {
      loadDataFromSource();
    }
  }, []);

  // Auto-fetch data when switching to symbol mode
  useEffect(() => {
    if (dataSourceType === 'symbol' && symbolInput) {
      const timer = setTimeout(() => {
        loadDataFromSource();
      }, 500); // Debounce to avoid excessive fetching
      return () => clearTimeout(timer);
    }
  }, [dataSourceType]);

  const loadPortfolios = async () => {
    try {
      const portfolioList = await portfolioFunctimeService.getAvailablePortfolios();
      setPortfolios(portfolioList);
      if (portfolioList.length > 0 && !selectedPortfolioId) {
        setSelectedPortfolioId(portfolioList[0].id);
      }
    } catch (err) {
      console.error('Failed to load portfolios:', err);
    }
  };

  const checkFunctimeStatus = async () => {
    try {
      const status = await functimeService.checkStatus();
      setIsFunctimeAvailable(status.available);
      if (!status.available) {
        setError('Functime library not installed. Run: pip install functime polars');
      }
    } catch (err) {
      setIsFunctimeAvailable(false);
      setError('Failed to check functime status');
    }
  };

  // Load data based on source type
  const loadDataFromSource = useCallback(async () => {
    if (dataSourceType === 'manual') {
      return;
    }

    setPriceDataLoading(true);
    setError(null);

    try {
      if (dataSourceType === 'portfolio' && selectedPortfolioId) {
        const summary = await portfolioFunctimeService.getPortfolioSummary(selectedPortfolioId);
        if (!summary || summary.holdings.length === 0) {
          setError('Portfolio has no holdings');
          return;
        }

        const data = await portfolioFunctimeService.fetchHistoricalPrices(
          summary.holdings,
          historicalDays
        );

        if (data) {
          setPriceData(data);
          setDataInput(JSON.stringify(data.panelData, null, 2));
        } else {
          setError('Failed to fetch historical data for portfolio');
        }
      } else if (dataSourceType === 'symbol' && symbolInput) {
        const data = await portfolioFunctimeService.fetchSymbolData(symbolInput, historicalDays);
        if (data) {
          setDataInput(JSON.stringify(data, null, 2));
          setPriceData({
            panelData: data,
            assets: [symbolInput],
            nDataPoints: data.length,
            startDate: data[0]?.time || '',
            endDate: data[data.length - 1]?.time || ''
          });
        } else {
          setError(`Failed to fetch data for ${symbolInput}`);
        }
      }
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to load data');
    } finally {
      setPriceDataLoading(false);
    }
  }, [dataSourceType, selectedPortfolioId, symbolInput, historicalDays]);

  // Run analysis based on mode
  const runAnalysis = async () => {
    setIsLoading(true);
    setError(null);

    try {
      const data = JSON.parse(dataInput);

      if (analysisMode === 'forecast') {
        await runForecast();
      } else if (analysisMode === 'anomaly') {
        await runAnomalyDetection(data);
      } else if (analysisMode === 'seasonality') {
        await runSeasonalityAnalysis(data);
      } else if (analysisMode === 'backtest') {
        await runForecast();
      }
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Analysis failed');
    } finally {
      setIsLoading(false);
    }
  };

  // Anomaly detection
  const runAnomalyDetection = async (data: any[]) => {
    const config: AnomalyDetectionConfig = {
      method: anomalyMethod,
      threshold: anomalyThreshold
    };

    const result = await functimeService.detectAnomalies(data, config);

    if (!result.success) {
      setError(result.error || 'Anomaly detection failed');
      return;
    }

    // Format result for portfolio view
    const anomalies: Record<string, any> = {};
    if (result.results) {
      Object.entries(result.results).forEach(([symbol, data]) => {
        const anomalyList = data.anomalies?.filter((a: any) => a.is_anomaly) || [];
        anomalies[symbol] = {
          count: data.n_anomalies,
          rate: data.anomaly_rate,
          dates: anomalyList.map((a: any) => a.time),
          values: anomalyList.map((a: any) => a.value)
        };
      });
    }

    setAnomalyResult({
      success: true,
      portfolio: portfolios.find(p => p.id === selectedPortfolioId) || {} as Portfolio,
      anomalies
    });
  };

  // Seasonality analysis
  const runSeasonalityAnalysis = async (data: any[]) => {
    const config: SeasonalityConfig = {
      operation: 'metrics'
    };

    const result = await functimeService.analyzeSeasonality(data, config);

    if (!result.success) {
      setError(result.error || 'Seasonality analysis failed');
      return;
    }

    // Format result
    const seasonality: Record<string, any> = {};
    if (result.metrics) {
      Object.entries(result.metrics).forEach(([symbol, metrics]) => {
        seasonality[symbol] = {
          period: metrics.detected_period || 0,
          strength: metrics.seasonal_strength,
          is_seasonal: metrics.is_seasonal,
          trend_strength: metrics.trend_strength
        };
      });
    }

    setSeasonalityResult({
      success: true,
      portfolio: portfolios.find(p => p.id === selectedPortfolioId) || {} as Portfolio,
      seasonality
    });
  };

  const runForecast = async () => {
    setIsLoading(true);
    setError(null);

    try {
      const data = JSON.parse(dataInput);

      // Run full pipeline
      const result = await functimeService.fullPipeline(
        data,
        selectedModel,
        horizon,
        frequency,
        testSize,
        preprocess === 'none' ? undefined : preprocess
      );

      if (!result.success) {
        setError(result.error || 'Forecast failed');
        return;
      }

      setAnalysisResult({
        forecast: result.forecast,
        model: result.model,
        horizon: result.horizon,
        frequency: result.frequency,
        best_params: result.best_params,
        evaluation_metrics: result.evaluation_metrics,
        data_summary: result.data_summary
      });
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Forecast failed');
    } finally {
      setIsLoading(false);
    }
  };

  const toggleSection = (section: keyof ExpandedSections) => {
    setExpandedSections(prev => ({ ...prev, [section]: !prev[section] }));
  };

  // Parse historical data from input for chart visualization
  const historicalData = useMemo(() => {
    return parseHistoricalData(dataInput);
  }, [dataInput]);

  return {
    // Status
    isFunctimeAvailable,
    isLoading,
    error,

    // Data
    dataInput,
    setDataInput,
    analysisResult,
    historicalData,

    // Data source
    dataSourceType,
    setDataSourceType,
    portfolios,
    selectedPortfolioId,
    setSelectedPortfolioId,
    symbolInput,
    setSymbolInput,
    historicalDays,
    setHistoricalDays,
    priceDataLoading,
    priceData,
    loadDataFromSource,

    // Analysis mode
    analysisMode,
    setAnalysisMode,

    // Forecast config
    selectedModel,
    setSelectedModel,
    horizon,
    setHorizon,
    frequency,
    setFrequency,
    alpha,
    setAlpha,
    testSize,
    setTestSize,
    preprocess,
    setPreprocess,

    // Anomaly
    anomalyMethod,
    setAnomalyMethod,
    anomalyThreshold,
    setAnomalyThreshold,
    anomalyResult,

    // Seasonality
    seasonalityResult,

    // UI state
    expandedSections,
    toggleSection,

    // Actions
    checkFunctimeStatus,
    runAnalysis
  };
}
