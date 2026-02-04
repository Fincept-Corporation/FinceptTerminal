/**
 * FortitudoPanel - Main Component
 * Fortitudo.tech Analytics Panel - Portfolio Risk Analytics
 * Integrated with fortitudo.tech library via PyO3
 */

import React from 'react';
import { FINCEPT } from './constants';
import { useFortitudoState } from './hooks';
import {
  Header,
  ModeSelector,
  DataSourceSelector,
  ErrorDisplay,
  LoadingState,
  UnavailableState
} from './components';
import {
  PortfolioRiskMode,
  OptionPricingMode,
  OptimizationMode,
  EntropyPoolingMode
} from './modes';

export function FortitudoPanel() {
  const state = useFortitudoState();

  // Render unavailable state
  if (state.isFortitudoAvailable === false) {
    return <UnavailableState onRetry={state.checkFortitudoStatus} />;
  }

  // Loading state
  if (state.isFortitudoAvailable === null) {
    return <LoadingState />;
  }

  return (
    <div style={{
      height: '100%',
      display: 'flex',
      flexDirection: 'column',
      backgroundColor: FINCEPT.DARK_BG
    }}>
      {/* Terminal-style Header */}
      <Header />

      <div style={{ display: 'flex', flex: 1, minHeight: 0 }}>
        {/* Left Sidebar - Mode Selector */}
        <div style={{
          width: '200px',
          borderRight: `1px solid ${FINCEPT.BORDER}`,
          backgroundColor: FINCEPT.PANEL_BG,
          display: 'flex',
          flexDirection: 'column'
        }}>
          <ModeSelector
            activeMode={state.activeMode}
            setActiveMode={state.setActiveMode}
          />
        </div>

        {/* Main Content Area */}
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', minWidth: 0 }}>
          {/* Data Source Configuration */}
          <div style={{
            padding: '16px',
            borderBottom: `1px solid ${FINCEPT.BORDER}`,
            backgroundColor: FINCEPT.PANEL_BG
          }}>
            <DataSourceSelector
              dataSource={state.dataSource}
              setDataSource={state.setDataSource}
              userPortfolios={state.userPortfolios}
              selectedPortfolioId={state.selectedPortfolioId}
              setSelectedPortfolioId={state.setSelectedPortfolioId}
              selectedPortfolioSummary={state.selectedPortfolioSummary}
              portfolioReturnsData={state.portfolioReturnsData}
              historicalDays={state.historicalDays}
              setHistoricalDays={state.setHistoricalDays}
              isLoadingPortfolio={state.isLoadingPortfolio}
              loadPortfolioData={state.loadPortfolioData}
            />
          </div>

          {/* Error Display */}
          {state.error && (
            <div style={{ padding: '0 16px' }}>
              <ErrorDisplay error={state.error} />
            </div>
          )}

          {/* Mode Content */}
          <div style={{ flex: 1, overflowY: 'auto', padding: '16px' }}>

            {/* Portfolio Risk Mode */}
            {state.activeMode === 'portfolio' && (
              <PortfolioRiskMode
                weights={state.weights}
                setWeights={state.setWeights}
                alpha={state.alpha}
                setAlpha={state.setAlpha}
                halfLife={state.halfLife}
                setHalfLife={state.setHalfLife}
                isLoading={state.isLoading}
                analysisResult={state.analysisResult}
                expandedSections={state.expandedSections}
                toggleSection={state.toggleSection}
                runPortfolioAnalysis={state.runPortfolioAnalysis}
              />
            )}

            {/* Option Pricing Mode */}
            {state.activeMode === 'options' && (
              <OptionPricingMode
                spotPrice={state.spotPrice}
                setSpotPrice={state.setSpotPrice}
                strike={state.strike}
                setStrike={state.setStrike}
                volatility={state.volatility}
                setVolatility={state.setVolatility}
                riskFreeRate={state.riskFreeRate}
                setRiskFreeRate={state.setRiskFreeRate}
                dividendYield={state.dividendYield}
                setDividendYield={state.setDividendYield}
                timeToMaturity={state.timeToMaturity}
                setTimeToMaturity={state.setTimeToMaturity}
                isLoading={state.isLoading}
                optionResult={state.optionResult}
                greeksResult={state.greeksResult}
                runOptionPricing={state.runOptionPricing}
                runOptionGreeks={state.runOptionGreeks}
              />
            )}

            {/* Optimization Mode */}
            {state.activeMode === 'optimization' && (
              <OptimizationMode
                optimizationType={state.optimizationType}
                setOptimizationType={state.setOptimizationType}
                optimizationObjective={state.optimizationObjective}
                setOptimizationObjective={state.setOptimizationObjective}
                optMaxWeight={state.optMaxWeight}
                setOptMaxWeight={state.setOptMaxWeight}
                optTargetReturn={state.optTargetReturn}
                setOptTargetReturn={state.setOptTargetReturn}
                optAlpha={state.optAlpha}
                setOptAlpha={state.setOptAlpha}
                optLongOnly={state.optLongOnly}
                setOptLongOnly={state.setOptLongOnly}
                riskFreeRate={state.riskFreeRate}
                isLoading={state.isLoading}
                optimizationResult={state.optimizationResult}
                frontierResult={state.frontierResult}
                selectedFrontierIndex={state.selectedFrontierIndex}
                setSelectedFrontierIndex={state.setSelectedFrontierIndex}
                runOptimization={state.runOptimization}
                runEfficientFrontier={state.runEfficientFrontier}
              />
            )}

            {/* Entropy Pooling Mode */}
            {state.activeMode === 'entropy' && (
              <EntropyPoolingMode
                nScenarios={state.nScenarios}
                setNScenarios={state.setNScenarios}
                maxProbability={state.maxProbability}
                setMaxProbability={state.setMaxProbability}
                isLoading={state.isLoading}
                entropyResult={state.entropyResult}
                runEntropyPooling={state.runEntropyPooling}
              />
            )}
          </div>
        </div>
      </div>
    </div>
  );
}

export default FortitudoPanel;
