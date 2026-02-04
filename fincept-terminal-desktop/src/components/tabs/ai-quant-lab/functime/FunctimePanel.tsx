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
  LineChart,
  TrendingUp
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
    <div style={{
      height: '100%',
      display: 'flex',
      flexDirection: 'column',
      backgroundColor: FINCEPT.DARK_BG
    }}>
      {/* Terminal-style Header */}
      <div style={{
        padding: '12px 16px',
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        backgroundColor: FINCEPT.PANEL_BG,
        display: 'flex',
        alignItems: 'center',
        gap: '12px'
      }}>
        <Zap size={16} color={FINCEPT.YELLOW} />
        <span style={{
          color: FINCEPT.YELLOW,
          fontSize: '12px',
          fontWeight: 700,
          letterSpacing: '0.5px',
          fontFamily: 'monospace'
        }}>
          FUNCTIME FORECASTING
        </span>
        <div style={{ flex: 1 }} />
        <div style={{
          fontSize: '10px',
          fontFamily: 'monospace',
          padding: '3px 8px',
          backgroundColor: state.analysisResult ? FINCEPT.GREEN + '20' : FINCEPT.DARK_BG,
          border: `1px solid ${state.analysisResult ? FINCEPT.GREEN : FINCEPT.BORDER}`,
          color: state.analysisResult ? FINCEPT.GREEN : FINCEPT.MUTED
        }}>
          {state.analysisResult ? 'RESULTS READY' : 'IDLE'}
        </div>
      </div>

      <div style={{ display: 'flex', flex: 1, minHeight: 0 }}>
        {/* Left Sidebar - Configuration */}
        <div style={{
          width: '280px',
          borderRight: `1px solid ${FINCEPT.BORDER}`,
          backgroundColor: FINCEPT.PANEL_BG,
          display: 'flex',
          flexDirection: 'column'
        }}>
          {/* Scrollable Content */}
          <div style={{ flex: 1, overflowY: 'auto' }}>
            {/* Analysis Mode Selector */}
            <div style={{
              padding: '10px 12px',
              borderBottom: `1px solid ${FINCEPT.BORDER}`,
              fontSize: '10px',
              fontWeight: 700,
              color: FINCEPT.MUTED,
              fontFamily: 'monospace',
              letterSpacing: '0.5px'
            }}>
              ANALYSIS MODE
            </div>

            <div style={{ padding: '8px', display: 'flex', flexDirection: 'column', gap: '0' }}>
              {[
                { id: 'forecast' as const, label: 'Forecast', icon: TrendingUp },
                { id: 'anomaly' as const, label: 'Anomaly', icon: AlertCircle },
                { id: 'seasonality' as const, label: 'Seasonality', icon: LineChart }
              ].map(mode => {
                const isSelected = state.analysisMode === mode.id;
                const Icon = mode.icon;
                return (
                  <div
                    key={mode.id}
                    onClick={() => state.setAnalysisMode(mode.id)}
                    style={{
                      padding: '10px',
                      backgroundColor: isSelected ? FINCEPT.HOVER : 'transparent',
                      border: `1px solid ${isSelected ? FINCEPT.YELLOW : FINCEPT.BORDER}`,
                      borderTop: mode.id === 'forecast' ? `1px solid ${isSelected ? FINCEPT.YELLOW : FINCEPT.BORDER}` : '0',
                      cursor: 'pointer',
                      transition: 'all 0.15s',
                      marginTop: mode.id === 'forecast' ? '0' : '-1px'
                    }}
                    onMouseEnter={(e) => {
                      if (!isSelected) {
                        e.currentTarget.style.backgroundColor = FINCEPT.DARK_BG;
                        e.currentTarget.style.borderColor = FINCEPT.YELLOW;
                      }
                    }}
                    onMouseLeave={(e) => {
                      if (!isSelected) {
                        e.currentTarget.style.backgroundColor = 'transparent';
                        e.currentTarget.style.borderColor = FINCEPT.BORDER;
                      }
                    }}
                  >
                    <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                      <Icon size={14} style={{ color: FINCEPT.YELLOW }} />
                      <span style={{
                        color: FINCEPT.WHITE,
                        fontSize: '11px',
                        fontWeight: 600,
                        fontFamily: 'monospace'
                      }}>
                        {mode.label}
                      </span>
                    </div>
                  </div>
                );
              })}
            </div>

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
          <div style={{ borderTop: `1px solid ${FINCEPT.BORDER}`, padding: '12px' }}>
            <button
              onClick={state.runAnalysis}
              disabled={state.isLoading}
              style={{
                width: '100%',
                padding: '10px 16px',
                backgroundColor: FINCEPT.YELLOW,
                border: 'none',
                color: '#000000',
                fontSize: '11px',
                fontWeight: 700,
                fontFamily: 'monospace',
                cursor: state.isLoading ? 'not-allowed' : 'pointer',
                opacity: state.isLoading ? 0.6 : 1,
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                gap: '8px',
                letterSpacing: '0.5px',
                transition: 'all 0.15s'
              }}
            >
              {state.isLoading ? (
                <>
                  <RefreshCw size={14} className="animate-spin" />
                  {state.analysisMode === 'forecast' ? 'FORECASTING...' :
                   state.analysisMode === 'anomaly' ? 'DETECTING...' : 'ANALYZING...'}
                </>
              ) : (
                <>
                  <Play size={14} />
                  {state.analysisMode === 'forecast' ? 'RUN FORECAST' :
                   state.analysisMode === 'anomaly' ? 'DETECT ANOMALIES' : 'ANALYZE SEASONALITY'}
                </>
              )}
            </button>
          </div>
        </div>

        {/* Main Content Area */}
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', minWidth: 0 }}>
          {/* Results Section */}
          <div style={{ flex: 1, overflowY: 'auto', padding: '16px' }}>
            {/* Error Display */}
            {state.error && (
              <div style={{
                padding: '12px 14px',
                backgroundColor: FINCEPT.RED + '15',
                border: `1px solid ${FINCEPT.RED}`,
                borderLeft: `3px solid ${FINCEPT.RED}`,
                color: FINCEPT.RED,
                fontSize: '11px',
                display: 'flex',
                alignItems: 'center',
                gap: '10px',
                fontFamily: 'monospace',
                marginBottom: '16px',
                lineHeight: '1.5'
              }}>
                <AlertCircle size={16} style={{ flexShrink: 0 }} />
                {state.error}
              </div>
            )}

            {/* Empty State */}
            {!state.analysisResult && !state.anomalyResult && !state.seasonalityResult && !state.error && (
              <div style={{
                padding: '40px 20px',
                textAlign: 'center'
              }}>
                <LineChart size={48} color={FINCEPT.MUTED} style={{ margin: '0 auto 16px' }} />
                <div style={{ fontSize: '11px', color: FINCEPT.WHITE, marginBottom: '8px', fontFamily: 'monospace' }}>
                  {state.analysisMode === 'forecast' ? 'Forecast Ready' :
                   state.analysisMode === 'anomaly' ? 'Anomaly Detection Ready' : 'Seasonality Analysis Ready'}
                </div>
                <div style={{ fontSize: '10px', color: FINCEPT.GRAY, lineHeight: '1.5' }}>
                  {state.analysisMode === 'forecast'
                    ? 'Configure model and click "Run Forecast" to see predictions'
                    : state.analysisMode === 'anomaly'
                    ? 'Click "Detect Anomalies" to find outliers in your data'
                    : 'Click "Analyze Seasonality" to discover patterns'}
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
      </div>
    </div>
  );
}

export default FunctimePanel;
