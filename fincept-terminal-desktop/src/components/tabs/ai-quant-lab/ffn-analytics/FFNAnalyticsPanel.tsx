/**
 * FFNAnalyticsPanel - Main Component
 * Portfolio Performance Analysis
 * Integrated with FFN library via PyO3
 */

import React from 'react';
import { FINCEPT } from './constants';
import { useFFNState } from './hooks';
import {
  LoadingState,
  UnavailableState,
  ErrorDisplay,
  EmptyState,
  DataSourceSection,
  AnalysisModeSelector,
  OptimizationMethodSelector,
  BenchmarkInput,
  PriceInputSection,
  ConfigSection,
  RunButton
} from './components';
import {
  PerformanceResults,
  DrawdownResults,
  RiskResults,
  MonthlyResults,
  RollingResults,
  ComparisonResults,
  OptimizationResults,
  BenchmarkResults
} from './results';

export function FFNAnalyticsPanel() {
  const state = useFFNState();

  // Loading state
  if (state.isFFNAvailable === null) {
    return <LoadingState />;
  }

  // Not available state
  if (!state.isFFNAvailable) {
    return <UnavailableState onRetry={state.checkFFNStatus} />;
  }

  const hasResults = state.analysisResult && (
    state.analysisResult.performance ||
    state.analysisResult.monthlyReturns ||
    state.analysisResult.rollingMetrics ||
    state.analysisResult.comparison ||
    state.analysisResult.optimization ||
    state.analysisResult.benchmark
  );

  return (
    <div className="flex h-full" style={{ backgroundColor: FINCEPT.DARK_BG }}>
      {/* Left Panel - Input */}
      <div
        className="w-80 flex flex-col border-r overflow-auto"
        style={{ backgroundColor: FINCEPT.PANEL_BG, borderColor: FINCEPT.BORDER }}
      >
        {/* Data Source Section */}
        <DataSourceSection
          dataSourceType={state.dataSourceType}
          setDataSourceType={state.setDataSourceType}
          portfolios={state.portfolios}
          selectedPortfolioId={state.selectedPortfolioId}
          setSelectedPortfolioId={state.setSelectedPortfolioId}
          symbolInput={state.symbolInput}
          setSymbolInput={state.setSymbolInput}
          symbolsInput={state.symbolsInput}
          setSymbolsInput={state.setSymbolsInput}
          analysisMode={state.analysisMode}
          historicalDays={state.historicalDays}
          setHistoricalDays={state.setHistoricalDays}
          priceDataLoading={state.priceDataLoading}
          fetchData={state.fetchData}
          expanded={state.expandedSections.dataSource}
          toggleSection={() => state.toggleSection('dataSource')}
        />

        {/* Analysis Mode */}
        <AnalysisModeSelector
          analysisMode={state.analysisMode}
          setAnalysisMode={state.setAnalysisMode}
        />

        {/* Optimization Method Selector */}
        {state.analysisMode === 'optimize' && (
          <OptimizationMethodSelector
            optimizationMethod={state.optimizationMethod}
            setOptimizationMethod={state.setOptimizationMethod}
          />
        )}

        {/* Benchmark Symbol Input */}
        {state.analysisMode === 'benchmark' && (
          <BenchmarkInput
            benchmarkSymbol={state.benchmarkSymbol}
            setBenchmarkSymbol={state.setBenchmarkSymbol}
          />
        )}

        {/* Price Input (for manual mode) */}
        {state.dataSourceType === 'manual' && (
          <PriceInputSection
            priceInput={state.priceInput}
            setPriceInput={state.setPriceInput}
          />
        )}

        {/* Config Options */}
        <ConfigSection
          config={state.config}
          setConfig={state.setConfig}
        />

        {/* Run Button */}
        <RunButton
          isLoading={state.isLoading}
          onClick={state.runAnalysis}
        />
      </div>

      {/* Right Panel - Results */}
      <div className="flex-1 overflow-auto p-4">
        {state.error && <ErrorDisplay error={state.error} />}

        {!hasResults && !state.error && (
          <EmptyState analysisMode={state.analysisMode} />
        )}

        {state.analysisResult && (
          <div className="space-y-4">
            {/* Performance Metrics Section */}
            {state.analysisResult.performance && (
              <PerformanceResults
                performance={state.analysisResult.performance}
                expanded={state.expandedSections.performance}
                toggleSection={() => state.toggleSection('performance')}
              />
            )}

            {/* Drawdowns Section */}
            {state.analysisResult.drawdowns && (
              <DrawdownResults
                drawdowns={state.analysisResult.drawdowns}
                expanded={state.expandedSections.drawdowns}
                toggleSection={() => state.toggleSection('drawdowns')}
              />
            )}

            {/* Risk Metrics Section */}
            {state.analysisResult.riskMetrics && (
              <RiskResults
                riskMetrics={state.analysisResult.riskMetrics}
                expanded={state.expandedSections.risk}
                toggleSection={() => state.toggleSection('risk')}
              />
            )}

            {/* Monthly Returns Heatmap */}
            {state.analysisResult.monthlyReturns && (
              <MonthlyResults
                monthlyReturns={state.analysisResult.monthlyReturns}
                expanded={state.expandedSections.monthly}
                toggleSection={() => state.toggleSection('monthly')}
              />
            )}

            {/* Rolling Metrics */}
            {state.analysisResult.rollingMetrics && (
              <RollingResults
                rollingMetrics={state.analysisResult.rollingMetrics}
                expanded={state.expandedSections.rolling}
                toggleSection={() => state.toggleSection('rolling')}
              />
            )}

            {/* Asset Comparison */}
            {state.analysisResult.comparison && state.analysisResult.comparison.success && (
              <ComparisonResults
                comparison={state.analysisResult.comparison}
                expanded={state.expandedSections.comparison}
                toggleSection={() => state.toggleSection('comparison')}
              />
            )}

            {/* Portfolio Optimization Results */}
            {state.analysisResult.optimization && state.analysisResult.optimization.success && (
              <OptimizationResults
                optimization={state.analysisResult.optimization}
                expanded={state.expandedSections.optimization}
                toggleSection={() => state.toggleSection('optimization')}
              />
            )}

            {/* Benchmark Comparison Results */}
            {state.analysisResult.benchmark && state.analysisResult.benchmark.success && (
              <BenchmarkResults
                benchmark={state.analysisResult.benchmark}
                expanded={state.expandedSections.benchmark}
                toggleSection={() => state.toggleSection('benchmark')}
              />
            )}
          </div>
        )}
      </div>
    </div>
  );
}

export default FFNAnalyticsPanel;
