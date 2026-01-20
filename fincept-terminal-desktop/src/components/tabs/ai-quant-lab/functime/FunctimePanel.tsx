/**
 * FunctimePanel - Main Component
 * Functime Analytics Panel - Time Series Forecasting
 * Integrated with functime library via PyO3
 */

import React from 'react';
import {
  Zap,
  Play,
  RefreshCw,
  AlertCircle,
  LineChart
} from 'lucide-react';
import { FINCEPT, SAMPLE_DATA } from './constants';
import { useFunctimeState } from './hooks';
import {
  LoadingState,
  UnavailableState,
  DataSourceSection,
  AnalysisModeSelector,
  ForecastConfigSection,
  AnomalyConfigSection,
  SeasonalityConfigSection
} from './components';
import {
  ForecastResults,
  AnomalyResults,
  SeasonalityResults
} from './results';

export function FunctimePanel() {
  const state = useFunctimeState();

  // Loading state
  if (state.isFunctimeAvailable === null) {
    return <LoadingState />;
  }

  // Not available state
  if (!state.isFunctimeAvailable) {
    return <UnavailableState onRetry={state.checkFunctimeStatus} />;
  }

  return (
    <div className="flex h-full" style={{ backgroundColor: FINCEPT.DARK_BG }}>
      {/* Left Panel - Configuration */}
      <div
        className="w-96 flex flex-col border-r"
        style={{ backgroundColor: FINCEPT.PANEL_BG, borderColor: FINCEPT.BORDER }}
      >
        {/* Input Header */}
        <div
          className="px-4 py-3 border-b flex items-center gap-2"
          style={{ backgroundColor: FINCEPT.HEADER_BG, borderColor: FINCEPT.BORDER }}
        >
          <Zap size={16} color={FINCEPT.ORANGE} />
          <span className="text-xs font-bold uppercase tracking-wide" style={{ color: FINCEPT.WHITE }}>
            Time Series Forecasting
          </span>
        </div>

        {/* Scrollable Content */}
        <div className="flex-1 overflow-auto p-4 space-y-4">
          {/* Data Source Selection */}
          <DataSourceSection
            dataSourceType={state.dataSourceType}
            setDataSourceType={state.setDataSourceType}
            portfolios={state.portfolios}
            selectedPortfolioId={state.selectedPortfolioId}
            setSelectedPortfolioId={state.setSelectedPortfolioId}
            symbolInput={state.symbolInput}
            setSymbolInput={state.setSymbolInput}
            historicalDays={state.historicalDays}
            setHistoricalDays={state.setHistoricalDays}
            priceDataLoading={state.priceDataLoading}
            priceData={state.priceData}
            loadDataFromSource={state.loadDataFromSource}
            expanded={state.expandedSections.dataSource}
            toggleSection={() => state.toggleSection('dataSource')}
          />

          {/* Analysis Mode Tabs */}
          <AnalysisModeSelector
            analysisMode={state.analysisMode}
            setAnalysisMode={state.setAnalysisMode}
          />

          {/* Data Input - Only for Manual */}
          {state.dataSourceType === 'manual' && (
            <div>
              <label className="block text-xs font-mono mb-2" style={{ color: FINCEPT.GRAY }}>
                PANEL DATA (JSON)
              </label>
              <textarea
                value={state.dataInput}
                onChange={(e) => state.setDataInput(e.target.value)}
                className="w-full h-32 p-3 rounded text-xs font-mono resize-none"
                style={{
                  backgroundColor: FINCEPT.DARK_BG,
                  color: FINCEPT.WHITE,
                  border: `1px solid ${FINCEPT.BORDER}`
                }}
                placeholder='[{"entity_id": "A", "time": "2024-01-01", "value": 100}, ...]'
              />
            </div>
          )}

          {/* Model Configuration - Shown for forecast mode */}
          {state.analysisMode === 'forecast' && (
            <ForecastConfigSection
              selectedModel={state.selectedModel}
              setSelectedModel={state.setSelectedModel}
              horizon={state.horizon}
              setHorizon={state.setHorizon}
              frequency={state.frequency}
              setFrequency={state.setFrequency}
              alpha={state.alpha}
              setAlpha={state.setAlpha}
              testSize={state.testSize}
              setTestSize={state.setTestSize}
              preprocess={state.preprocess}
              setPreprocess={state.setPreprocess}
              expanded={state.expandedSections.config}
              toggleSection={() => state.toggleSection('config')}
            />
          )}

          {/* Anomaly Detection Configuration */}
          {state.analysisMode === 'anomaly' && (
            <AnomalyConfigSection
              anomalyMethod={state.anomalyMethod}
              setAnomalyMethod={state.setAnomalyMethod}
              anomalyThreshold={state.anomalyThreshold}
              setAnomalyThreshold={state.setAnomalyThreshold}
            />
          )}

          {/* Seasonality Analysis Configuration */}
          {state.analysisMode === 'seasonality' && (
            <SeasonalityConfigSection />
          )}
        </div>

        {/* Run Button */}
        <div className="p-4 border-t" style={{ borderColor: FINCEPT.BORDER }}>
          <button
            onClick={state.runAnalysis}
            disabled={state.isLoading}
            className="w-full py-3 rounded font-bold text-sm uppercase flex items-center justify-center gap-2 disabled:opacity-50"
            style={{ backgroundColor: FINCEPT.ORANGE, color: FINCEPT.DARK_BG }}
          >
            {state.isLoading ? (
              <>
                <RefreshCw size={16} className="animate-spin" />
                {state.analysisMode === 'forecast' ? 'FORECASTING...' :
                 state.analysisMode === 'anomaly' ? 'DETECTING...' : 'ANALYZING...'}
              </>
            ) : (
              <>
                <Play size={16} />
                {state.analysisMode === 'forecast' ? 'RUN FORECAST' :
                 state.analysisMode === 'anomaly' ? 'DETECT ANOMALIES' : 'ANALYZE SEASONALITY'}
              </>
            )}
          </button>
        </div>
      </div>

      {/* Right Panel - Results */}
      <div className="flex-1 overflow-auto p-4">
        {state.error && (
          <div
            className="mb-4 p-4 rounded flex items-center gap-3"
            style={{ backgroundColor: FINCEPT.PANEL_BG, borderLeft: `3px solid ${FINCEPT.RED}` }}
          >
            <AlertCircle size={20} color={FINCEPT.RED} />
            <span className="text-sm font-mono" style={{ color: FINCEPT.RED }}>{state.error}</span>
          </div>
        )}

        {!state.analysisResult && !state.anomalyResult && !state.seasonalityResult && !state.error && (
          <div className="flex items-center justify-center h-full">
            <div className="text-center">
              <LineChart size={64} color={FINCEPT.MUTED} className="mx-auto mb-4" />
              <p className="text-sm font-mono" style={{ color: FINCEPT.GRAY }}>
                {state.analysisMode === 'forecast'
                  ? 'Configure model and click "Run Forecast" to see predictions'
                  : state.analysisMode === 'anomaly'
                  ? 'Click "Detect Anomalies" to find outliers in your data'
                  : 'Click "Analyze Seasonality" to discover patterns'}
              </p>
            </div>
          </div>
        )}

        {/* Anomaly Detection Results */}
        {state.anomalyResult && state.anomalyResult.success && (
          <AnomalyResults anomalyResult={state.anomalyResult} />
        )}

        {/* Seasonality Results */}
        {state.seasonalityResult && state.seasonalityResult.success && (
          <SeasonalityResults seasonalityResult={state.seasonalityResult} />
        )}

        {/* Forecast Results */}
        {state.analysisResult && (
          <ForecastResults
            analysisResult={state.analysisResult}
            expandedSections={state.expandedSections}
            toggleSection={state.toggleSection}
            historicalData={state.historicalData}
          />
        )}
      </div>
    </div>
  );
}

export default FunctimePanel;
