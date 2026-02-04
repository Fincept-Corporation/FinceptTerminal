/**
 * FFNAnalyticsPanel - Main Component
 * Portfolio Performance Analysis
 * Integrated with FFN library via PyO3
 * GREEN THEME
 */

import React from 'react';
import { TrendingUp } from 'lucide-react';
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
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%', backgroundColor: FINCEPT.DARK_BG }}>
      {/* Terminal-style Header */}
      <div style={{
        padding: '12px 16px',
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        backgroundColor: FINCEPT.PANEL_BG,
        display: 'flex',
        alignItems: 'center',
        gap: '12px'
      }}>
        <TrendingUp size={16} color={FINCEPT.GREEN} />
        <span style={{
          color: FINCEPT.GREEN,
          fontSize: '12px',
          fontWeight: 700,
          letterSpacing: '0.5px',
          fontFamily: 'monospace'
        }}>
          FFN PORTFOLIO ANALYTICS
        </span>
        <div style={{ flex: 1 }} />
        {hasResults && (
          <div style={{
            fontSize: '10px',
            fontFamily: 'monospace',
            padding: '3px 8px',
            backgroundColor: FINCEPT.GREEN + '20',
            border: `1px solid ${FINCEPT.GREEN}`,
            color: FINCEPT.GREEN
          }}>
            RESULTS READY
          </div>
        )}
      </div>

      <div style={{ display: 'flex', flex: 1, overflow: 'hidden' }}>
        {/* Left Panel - Input */}
        <div style={{
          width: '320px',
          borderRight: `1px solid ${FINCEPT.BORDER}`,
          backgroundColor: FINCEPT.PANEL_BG,
          display: 'flex',
          flexDirection: 'column',
          overflowY: 'auto'
        }}>
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
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', backgroundColor: FINCEPT.DARK_BG }}>
          <div style={{
            padding: '10px 16px',
            borderBottom: `1px solid ${FINCEPT.BORDER}`,
            fontSize: '10px',
            fontWeight: 700,
            color: FINCEPT.GREEN,
            fontFamily: 'monospace',
            letterSpacing: '0.5px',
            backgroundColor: FINCEPT.PANEL_BG
          }}>
            ANALYSIS RESULTS
          </div>

          <div style={{ flex: 1, overflowY: 'auto', padding: '16px' }}>
            {state.error && <ErrorDisplay error={state.error} />}

            {!hasResults && !state.error && (
              <EmptyState analysisMode={state.analysisMode} />
            )}

            {state.analysisResult && (
              <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
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
      </div>
    </div>
  );
}

export default FFNAnalyticsPanel;
