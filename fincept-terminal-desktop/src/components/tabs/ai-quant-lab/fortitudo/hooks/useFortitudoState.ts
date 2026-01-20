/**
 * useFortitudoState Hook
 * Manages all state for the Fortitudo Panel
 */

import { useState, useEffect } from 'react';
import {
  fortitudoService,
  type OptionGreeksResponse,
  type OptimizationResult,
  type EfficientFrontierResponse,
  type OptimizationObjective
} from '@/services/aiQuantLab/fortitudoService';
import {
  portfolioFortitudoService,
  type PortfolioReturnsData
} from '@/services/aiQuantLab/portfolioFortitudoService';
import type { Portfolio, PortfolioSummary } from '@/services/portfolio/portfolioService';
import { SAMPLE_RETURNS } from '../constants';
import type {
  AnalysisMode,
  DataSourceType,
  OptimizationType,
  AnalysisResult,
  ExpandedSections
} from '../types';

export function useFortitudoState() {
  // Status
  const [isFortitudoAvailable, setIsFortitudoAvailable] = useState<boolean | null>(null);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [activeMode, setActiveMode] = useState<AnalysisMode>('portfolio');

  // Portfolio config
  const [weights, setWeights] = useState('[0.4, 0.3, 0.2, 0.1]');
  const [alpha, setAlpha] = useState(0.05);
  const [halfLife, setHalfLife] = useState(252);
  const [analysisResult, setAnalysisResult] = useState<AnalysisResult | null>(null);

  // Option pricing config
  const [spotPrice, setSpotPrice] = useState(100);
  const [strike, setStrike] = useState(100);
  const [volatility, setVolatility] = useState(0.25);
  const [riskFreeRate, setRiskFreeRate] = useState(0.05);
  const [dividendYield, setDividendYield] = useState(0.02);
  const [timeToMaturity, setTimeToMaturity] = useState(1.0);
  const [optionResult, setOptionResult] = useState<any>(null);
  const [greeksResult, setGreeksResult] = useState<OptionGreeksResponse | null>(null);

  // Entropy pooling config
  const [nScenarios, setNScenarios] = useState(100);
  const [maxProbability, setMaxProbability] = useState(0.05);
  const [entropyResult, setEntropyResult] = useState<any>(null);

  // Optimization config
  const [optimizationObjective, setOptimizationObjective] = useState<OptimizationObjective>('min_variance');
  const [optimizationType, setOptimizationType] = useState<OptimizationType>('mv');
  const [optLongOnly, setOptLongOnly] = useState(true);
  const [optMaxWeight, setOptMaxWeight] = useState(0.5);
  const [optMinWeight, setOptMinWeight] = useState(0.0);
  const [optTargetReturn, setOptTargetReturn] = useState(0.08);
  const [optAlpha, setOptAlpha] = useState(0.05);
  const [optimizationResult, setOptimizationResult] = useState<OptimizationResult | null>(null);
  const [frontierResult, setFrontierResult] = useState<EfficientFrontierResponse | null>(null);
  const [nFrontierPoints, setNFrontierPoints] = useState(20);
  const [selectedFrontierIndex, setSelectedFrontierIndex] = useState<number | null>(null);

  // User portfolio
  const [userPortfolios, setUserPortfolios] = useState<Portfolio[]>([]);
  const [selectedPortfolioId, setSelectedPortfolioId] = useState<string | null>(null);
  const [selectedPortfolioSummary, setSelectedPortfolioSummary] = useState<PortfolioSummary | null>(null);
  const [portfolioReturnsData, setPortfolioReturnsData] = useState<PortfolioReturnsData | null>(null);
  const [dataSource, setDataSource] = useState<DataSourceType>('sample');
  const [isLoadingPortfolio, setIsLoadingPortfolio] = useState(false);
  const [historicalDays, setHistoricalDays] = useState(252);

  // UI state
  const [expandedSections, setExpandedSections] = useState<ExpandedSections>({
    config: true,
    metrics: true,
    comparison: true,
    matrix: false
  });

  // Check fortitudo availability on mount
  useEffect(() => {
    checkFortitudoStatus();
    loadUserPortfolios();
  }, []);

  // Load portfolio summary and returns when selected
  useEffect(() => {
    if (selectedPortfolioId && dataSource === 'portfolio') {
      loadPortfolioData(selectedPortfolioId);
    }
  }, [selectedPortfolioId, dataSource]);

  const loadUserPortfolios = async () => {
    try {
      const portfolios = await portfolioFortitudoService.getAvailablePortfolios();
      setUserPortfolios(portfolios);
    } catch (err) {
      console.error('[FortitudoPanel] Failed to load portfolios:', err);
    }
  };

  const loadPortfolioData = async (portfolioId: string) => {
    setIsLoadingPortfolio(true);
    setError(null);
    try {
      const summary = await portfolioFortitudoService.getPortfolioSummary(portfolioId);
      setSelectedPortfolioSummary(summary);

      if (summary && summary.holdings.length > 0) {
        const returns = await portfolioFortitudoService.fetchHistoricalReturns(summary.holdings, historicalDays);
        setPortfolioReturnsData(returns);

        // Auto-update weights from portfolio
        if (returns) {
          setWeights(JSON.stringify(Object.values(returns.weights).map(w => Math.round(w * 1000) / 1000)));
        }
      }
    } catch (err) {
      setError(`Failed to load portfolio data: ${err}`);
    } finally {
      setIsLoadingPortfolio(false);
    }
  };

  const checkFortitudoStatus = async () => {
    try {
      const status = await fortitudoService.checkStatus();
      setIsFortitudoAvailable(status.available);
      if (!status.available) {
        setError('Fortitudo.tech library not installed. Run: pip install fortitudo.tech cvxopt');
      }
    } catch (err) {
      setIsFortitudoAvailable(false);
      setError('Failed to check fortitudo.tech status');
    }
  };

  const runPortfolioAnalysis = async () => {
    setIsLoading(true);
    setError(null);

    try {
      let returnsData: Record<string, Record<string, number>>;
      let weightsArray: number[];

      if (dataSource === 'portfolio' && portfolioReturnsData) {
        returnsData = portfolioReturnsData.returns;
        weightsArray = Object.values(portfolioReturnsData.weights);
      } else {
        returnsData = SAMPLE_RETURNS;
        weightsArray = JSON.parse(weights);
      }

      const result = await fortitudoService.fullAnalysis(
        returnsData,
        weightsArray,
        alpha,
        halfLife
      );

      if (result.success && result.analysis) {
        if (dataSource === 'portfolio' && portfolioReturnsData) {
          result.analysis.assets = portfolioReturnsData.assets;
        }
        setAnalysisResult(result.analysis);
      } else {
        setError(result.error || 'Analysis failed');
      }
    } catch (err) {
      setError(String(err));
    } finally {
      setIsLoading(false);
    }
  };

  const runOptionPricing = async () => {
    setIsLoading(true);
    setError(null);

    try {
      const result = await fortitudoService.optionPricing(
        spotPrice,
        strike,
        volatility,
        riskFreeRate,
        dividendYield,
        timeToMaturity
      );

      if (result.success) {
        setOptionResult(result);
      } else {
        setError(result.error || 'Option pricing failed');
      }
    } catch (err) {
      setError(String(err));
    } finally {
      setIsLoading(false);
    }
  };

  const runOptionGreeks = async () => {
    setIsLoading(true);
    setError(null);

    try {
      const result = await fortitudoService.optionGreeks(
        spotPrice,
        strike,
        volatility,
        riskFreeRate,
        dividendYield,
        timeToMaturity
      );

      if (result.success) {
        setGreeksResult(result);
      } else {
        setError(result.error || 'Greeks calculation failed');
      }
    } catch (err) {
      setError(String(err));
    } finally {
      setIsLoading(false);
    }
  };

  const runEntropyPooling = async () => {
    setIsLoading(true);
    setError(null);

    try {
      const result = await fortitudoService.entropyPooling(nScenarios, maxProbability);

      if (result.success) {
        setEntropyResult(result);
      } else {
        setError(result.error || 'Entropy pooling failed');
      }
    } catch (err) {
      setError(String(err));
    } finally {
      setIsLoading(false);
    }
  };

  const runOptimization = async () => {
    setIsLoading(true);
    setError(null);

    try {
      const returnsData = (dataSource === 'portfolio' && portfolioReturnsData)
        ? portfolioReturnsData.returns
        : SAMPLE_RETURNS;

      let result: OptimizationResult;

      if (optimizationType === 'mv') {
        result = await fortitudoService.optimizeMeanVariance(
          returnsData,
          optimizationObjective,
          optLongOnly,
          optMaxWeight,
          optMinWeight,
          riskFreeRate,
          optimizationObjective === 'target_return' ? optTargetReturn : undefined
        );
      } else {
        result = await fortitudoService.optimizeMeanCVaR(
          returnsData,
          optimizationObjective === 'min_variance' ? 'min_cvar' : optimizationObjective,
          optAlpha,
          optLongOnly,
          optMaxWeight,
          optMinWeight,
          riskFreeRate,
          optimizationObjective === 'target_return' ? optTargetReturn : undefined
        );
      }

      if (result.success && dataSource === 'portfolio' && portfolioReturnsData) {
        result.assets = portfolioReturnsData.assets;
      }

      if (result.success) {
        setOptimizationResult(result);
      } else {
        setError(result.error || 'Optimization failed');
      }
    } catch (err) {
      setError(String(err));
    } finally {
      setIsLoading(false);
    }
  };

  const runEfficientFrontier = async () => {
    setIsLoading(true);
    setError(null);

    try {
      const returnsData = (dataSource === 'portfolio' && portfolioReturnsData)
        ? portfolioReturnsData.returns
        : SAMPLE_RETURNS;

      let result: EfficientFrontierResponse;

      if (optimizationType === 'mv') {
        result = await fortitudoService.efficientFrontierMV(
          returnsData,
          nFrontierPoints,
          optLongOnly,
          optMaxWeight,
          riskFreeRate
        );
      } else {
        result = await fortitudoService.efficientFrontierCVaR(
          returnsData,
          nFrontierPoints,
          optAlpha,
          optLongOnly,
          optMaxWeight
        );
      }

      if (result.success && dataSource === 'portfolio' && portfolioReturnsData) {
        result.assets = portfolioReturnsData.assets;
      }

      if (result.success) {
        setFrontierResult(result);
        setSelectedFrontierIndex(null);
      } else {
        setError(result.error || 'Efficient frontier generation failed');
      }
    } catch (err) {
      setError(String(err));
    } finally {
      setIsLoading(false);
    }
  };

  const toggleSection = (section: keyof ExpandedSections) => {
    setExpandedSections(prev => ({ ...prev, [section]: !prev[section] }));
  };

  return {
    // Status
    isFortitudoAvailable,
    isLoading,
    error,
    activeMode,
    setActiveMode,

    // Portfolio risk
    weights,
    setWeights,
    alpha,
    setAlpha,
    halfLife,
    setHalfLife,
    analysisResult,

    // Option pricing
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
    optionResult,
    greeksResult,

    // Entropy pooling
    nScenarios,
    setNScenarios,
    maxProbability,
    setMaxProbability,
    entropyResult,

    // Optimization
    optimizationObjective,
    setOptimizationObjective,
    optimizationType,
    setOptimizationType,
    optLongOnly,
    setOptLongOnly,
    optMaxWeight,
    setOptMaxWeight,
    optMinWeight,
    setOptMinWeight,
    optTargetReturn,
    setOptTargetReturn,
    optAlpha,
    setOptAlpha,
    optimizationResult,
    frontierResult,
    nFrontierPoints,
    setNFrontierPoints,
    selectedFrontierIndex,
    setSelectedFrontierIndex,

    // User portfolio
    userPortfolios,
    selectedPortfolioId,
    setSelectedPortfolioId,
    selectedPortfolioSummary,
    portfolioReturnsData,
    dataSource,
    setDataSource,
    isLoadingPortfolio,
    historicalDays,
    setHistoricalDays,
    loadPortfolioData,

    // UI state
    expandedSections,
    toggleSection,

    // Actions
    checkFortitudoStatus,
    runPortfolioAnalysis,
    runOptionPricing,
    runOptionGreeks,
    runEntropyPooling,
    runOptimization,
    runEfficientFrontier
  };
}
