/**
 * CFA Quant Panel
 * Main component for CFA-level quantitative analytics
 *
 * This component has been refactored into smaller, more maintainable pieces.
 * See the cfa-quant folder for:
 * - components/ - UI components (Header, DataStep, AnalysisStep, ResultsStep, etc.)
 * - hooks/ - Custom hooks (useChartZoom, usePriceData)
 * - utils/ - Utility functions (formatters, calculations, metricsExtractor, chartDataBuilder)
 * - constants.ts - Color palette and analysis configurations
 * - types.ts - TypeScript interfaces
 */

import React, { useState, useMemo, useCallback } from 'react';
import { quantAnalyticsService, type QuantAnalyticsResult } from '@/services/aiQuantLab/quantAnalyticsService';
import { BB, analysisConfigs } from './constants';
import type { Step, DataSourceType } from './types';
import { useChartZoom, usePriceData } from './hooks';
import { Header, DataStep, AnalysisStep, ResultsStep } from './components';

export function CFAQuantPanel() {
  // Step navigation
  const [currentStep, setCurrentStep] = useState<Step>('data');

  // Loading and error states
  const [isLoading, setIsLoading] = useState(false);
  const [analysisResult, setAnalysisResult] = useState<QuantAnalyticsResult<Record<string, unknown>> | null>(null);

  // Data input state
  const [dataSourceType, setDataSourceType] = useState<DataSourceType>('symbol');
  const [symbolInput, setSymbolInput] = useState('AAPL');
  const [historicalDays, setHistoricalDays] = useState(365);
  const [manualData, setManualData] = useState('');

  // Analysis state
  const [selectedAnalysis, setSelectedAnalysis] = useState<string>('trend_analysis');

  // Price data hook
  const {
    priceData,
    priceDates,
    priceDataLoading,
    dataStats,
    chartData,
    fetchSymbolData: fetchSymbolDataBase,
    error,
    setError,
    getDataArray,
    getDateForIndex,
  } = usePriceData({ dataSourceType, manualData });

  // Chart zoom hook
  const {
    dataChartZoom,
    setDataChartZoom,
    resultsChartZoom,
    setResultsChartZoom,
    refAreaLeft,
    refAreaRight,
    isSelecting,
    activeChart,
    handleZoomIn,
    handleZoomOut,
    handleResetZoom,
    handlePanLeft,
    handlePanRight,
    canPanLeft,
    canPanRight,
    handleMouseDown,
    handleMouseMove,
    handleMouseUp,
    handleWheel,
  } = useChartZoom({
    priceDataLength: priceData.length,
    analysisResult,
    selectedAnalysis,
  });

  // Fetch symbol data wrapper
  const fetchSymbolData = useCallback(async () => {
    await fetchSymbolDataBase(symbolInput, historicalDays);
    setDataChartZoom(null);
  }, [fetchSymbolDataBase, symbolInput, historicalDays, setDataChartZoom]);

  // Zoomed chart data
  const zoomedChartData = useMemo(() => {
    if (!dataChartZoom) return chartData;
    return chartData.slice(dataChartZoom.startIndex, dataChartZoom.endIndex + 1);
  }, [chartData, dataChartZoom]);

  // Run analysis
  const runAnalysis = useCallback(async () => {
    setIsLoading(true);
    setError(null);
    setAnalysisResult(null);

    try {
      const data = getDataArray(priceData, manualData, dataSourceType);
      const config = analysisConfigs.find(a => a.id === selectedAnalysis);

      if (data.length === 0) {
        setError('No data available');
        setIsLoading(false);
        return;
      }

      if (config && data.length < config.minDataPoints) {
        setError(`Need ${config.minDataPoints} data points (have ${data.length})`);
        setIsLoading(false);
        return;
      }

      const params: Record<string, unknown> = { data };
      const result = await quantAnalyticsService.executeCustomCommand(selectedAnalysis, params);
      setAnalysisResult(result as QuantAnalyticsResult<Record<string, unknown>>);

      if (result.success) {
        setCurrentStep('results');
      } else if (result.error) {
        setError(result.error);
      }
    } catch (err) {
      setError(`Analysis failed: ${err}`);
    } finally {
      setIsLoading(false);
    }
  }, [priceData, manualData, dataSourceType, selectedAnalysis, getDataArray, setError]);

  return (
    <div className="h-full flex flex-col" style={{ backgroundColor: BB.black }}>
      <Header
        currentStep={currentStep}
        setCurrentStep={setCurrentStep}
        analysisResult={analysisResult}
        dataSourceType={dataSourceType}
        symbolInput={symbolInput}
        dataStats={dataStats}
      />
      <div className="flex-1 overflow-hidden">
        {currentStep === 'data' && (
          <DataStep
            dataSourceType={dataSourceType}
            setDataSourceType={setDataSourceType}
            symbolInput={symbolInput}
            setSymbolInput={setSymbolInput}
            historicalDays={historicalDays}
            setHistoricalDays={setHistoricalDays}
            manualData={manualData}
            setManualData={setManualData}
            priceData={priceData}
            priceDates={priceDates}
            priceDataLoading={priceDataLoading}
            dataStats={dataStats}
            error={error}
            chartData={chartData}
            zoomedChartData={zoomedChartData}
            dataChartZoom={dataChartZoom}
            setDataChartZoom={setDataChartZoom}
            fetchSymbolData={fetchSymbolData}
            setCurrentStep={setCurrentStep}
            getDateForIndex={getDateForIndex}
            handleWheel={handleWheel}
            handleMouseDown={handleMouseDown}
            handleMouseMove={handleMouseMove}
            handleMouseUp={handleMouseUp}
            isSelecting={isSelecting}
            activeChart={activeChart}
            refAreaLeft={refAreaLeft}
            refAreaRight={refAreaRight}
          />
        )}
        {currentStep === 'analysis' && (
          <AnalysisStep
            selectedAnalysis={selectedAnalysis}
            setSelectedAnalysis={setSelectedAnalysis}
            priceData={priceData}
            isLoading={isLoading}
            error={error}
            runAnalysis={runAnalysis}
            setCurrentStep={setCurrentStep}
            zoomedChartData={zoomedChartData}
            dataChartZoom={dataChartZoom}
            setDataChartZoom={setDataChartZoom}
          />
        )}
        {currentStep === 'results' && (
          <ResultsStep
            analysisResult={analysisResult}
            selectedAnalysis={selectedAnalysis}
            symbolInput={symbolInput}
            priceData={priceData}
            priceDates={priceDates}
            chartData={chartData}
            setCurrentStep={setCurrentStep}
            setAnalysisResult={setAnalysisResult}
            resultsChartZoom={resultsChartZoom}
            setResultsChartZoom={setResultsChartZoom}
            getDateForIndex={getDateForIndex}
            handleWheel={handleWheel}
            handleMouseDown={handleMouseDown}
            handleMouseMove={handleMouseMove}
            handleMouseUp={handleMouseUp}
            isSelecting={isSelecting}
            activeChart={activeChart}
            refAreaLeft={refAreaLeft}
            refAreaRight={refAreaRight}
          />
        )}
      </div>
    </div>
  );
}

export default CFAQuantPanel;
