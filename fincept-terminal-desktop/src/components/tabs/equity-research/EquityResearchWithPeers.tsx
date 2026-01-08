// Equity Research with Peer Comparison - Complete Integration
// Combines all peer analysis components into a comprehensive research tool

import React, { useState } from 'react';
import { PeerComparisonPanel } from './components/PeerComparisonPanel';
import { SectorHeatmap, SectorPerformance } from './components/SectorHeatmap';
import { PercentileRankChart, PercentileTable } from './components/PercentileRankChart';
import { StockScreenerPanel } from './components/StockScreenerPanel';
import { usePeerComparison } from '../../../hooks/usePeerComparison';

interface EquityResearchWithPeersProps {
  symbol: string;
}

type TabView = 'overview' | 'peers' | 'rankings' | 'sectors' | 'screener';

export const EquityResearchWithPeers: React.FC<EquityResearchWithPeersProps> = ({ symbol }) => {
  const [activeTab, setActiveTab] = useState<TabView>('overview');
  const [searchSymbol, setSearchSymbol] = useState(symbol);

  const {
    targetSymbol,
    selectedPeers,
    availablePeers,
    comparisonData,
    loading,
    error,
    setTargetSymbol,
    fetchPeers,
    runComparison,
  } = usePeerComparison(symbol);

  const handleSearch = () => {
    if (searchSymbol) {
      setTargetSymbol(searchSymbol);
    }
  };

  const tabs = [
    { id: 'overview' as TabView, label: 'Overview', icon: '📊' },
    { id: 'peers' as TabView, label: 'Peer Comparison', icon: '👥' },
    { id: 'rankings' as TabView, label: 'Percentile Rankings', icon: '📈' },
    { id: 'sectors' as TabView, label: 'Sector Analysis', icon: '🗺️' },
    { id: 'screener' as TabView, label: 'Stock Screener', icon: '🔍' },
  ];

  // Extract percentile rankings from comparison data
  const percentileRankings = comparisonData?.percentileRanks
    ? Object.values(comparisonData.percentileRanks)
    : [];

  return (
    <div className="min-h-screen bg-gray-950 p-6">
      <div className="max-w-7xl mx-auto space-y-6">
        {/* Header */}
        <div className="bg-gradient-to-r from-blue-600 to-purple-600 rounded-lg p-6 shadow-xl">
          <h1 className="text-3xl font-bold text-white mb-2">
            Equity Research & Peer Analysis
          </h1>
          <p className="text-blue-100">
            CFA Institute-compliant peer comparison and benchmarking
          </p>
        </div>

        {/* Search Bar */}
        <div className="bg-gray-900 rounded-lg p-4 border border-gray-700">
          <div className="flex gap-3">
            <input
              type="text"
              value={searchSymbol}
              onChange={(e) => setSearchSymbol(e.target.value.toUpperCase())}
              onKeyPress={(e) => e.key === 'Enter' && handleSearch()}
              placeholder="Enter stock symbol (e.g., AAPL, MSFT)"
              className="flex-1 px-4 py-2 bg-gray-800 border border-gray-700 rounded-lg text-white placeholder-gray-500 focus:outline-none focus:ring-2 focus:ring-blue-500"
            />
            <button
              onClick={handleSearch}
              disabled={loading}
              className="px-6 py-2 bg-blue-600 hover:bg-blue-700 disabled:bg-gray-700 text-white rounded-lg transition-colors font-medium"
            >
              {loading ? 'Searching...' : 'Search'}
            </button>
          </div>
        </div>

        {/* Tab Navigation */}
        <div className="bg-gray-900 rounded-lg border border-gray-700 overflow-hidden">
          <div className="flex border-b border-gray-700">
            {tabs.map((tab) => (
              <button
                key={tab.id}
                onClick={() => setActiveTab(tab.id)}
                className={`flex-1 px-6 py-4 font-medium transition-colors ${
                  activeTab === tab.id
                    ? 'bg-blue-600 text-white'
                    : 'text-gray-400 hover:text-gray-300 hover:bg-gray-800'
                }`}
              >
                <span className="mr-2">{tab.icon}</span>
                {tab.label}
              </button>
            ))}
          </div>

          <div className="p-6">
            {/* Overview Tab */}
            {activeTab === 'overview' && (
              <div className="space-y-6">
                <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
                  {/* Company Info Card */}
                  <div className="bg-gray-800 rounded-lg p-6 border border-gray-700">
                    <h3 className="text-xl font-semibold text-white mb-4">
                      {targetSymbol || 'Select a Symbol'}
                    </h3>
                    {comparisonData?.target && (
                      <div className="space-y-3">
                        <div className="flex justify-between">
                          <span className="text-gray-400">P/E Ratio:</span>
                          <span className="text-white font-semibold">
                            {comparisonData.target.valuation.peRatio?.toFixed(2) || 'N/A'}
                          </span>
                        </div>
                        <div className="flex justify-between">
                          <span className="text-gray-400">ROE:</span>
                          <span className="text-white font-semibold">
                            {comparisonData.target.profitability.roe
                              ? `${(comparisonData.target.profitability.roe * 100).toFixed(2)}%`
                              : 'N/A'}
                          </span>
                        </div>
                        <div className="flex justify-between">
                          <span className="text-gray-400">Revenue Growth:</span>
                          <span className="text-white font-semibold">
                            {comparisonData.target.growth.revenueGrowth
                              ? `${(comparisonData.target.growth.revenueGrowth * 100).toFixed(2)}%`
                              : 'N/A'}
                          </span>
                        </div>
                      </div>
                    )}
                  </div>

                  {/* Quick Stats */}
                  <div className="bg-gray-800 rounded-lg p-6 border border-gray-700">
                    <h3 className="text-xl font-semibold text-white mb-4">Peer Summary</h3>
                    <div className="space-y-3">
                      <div className="flex justify-between">
                        <span className="text-gray-400">Available Peers:</span>
                        <span className="text-white font-semibold">{availablePeers.length}</span>
                      </div>
                      <div className="flex justify-between">
                        <span className="text-gray-400">Selected Peers:</span>
                        <span className="text-white font-semibold">{selectedPeers.length}</span>
                      </div>
                      <div className="flex justify-between">
                        <span className="text-gray-400">Avg Similarity:</span>
                        <span className="text-white font-semibold">
                          {selectedPeers.length > 0
                            ? `${(
                                (selectedPeers.reduce((sum, p) => sum + p.similarityScore, 0) /
                                  selectedPeers.length) *
                                100
                              ).toFixed(1)}%`
                            : 'N/A'}
                        </span>
                      </div>
                    </div>
                  </div>
                </div>

                {/* Valuation Status */}
                {comparisonData?.summary && (
                  <div className="bg-gray-800 rounded-lg p-6 border border-gray-700">
                    <h3 className="text-xl font-semibold text-white mb-4">Overall Rating</h3>
                    <div className="grid grid-cols-1 md:grid-cols-4 gap-4">
                      <ScoreCard
                        title="Valuation"
                        score={comparisonData.summary.valuationScore}
                        maxScore={2}
                      />
                      <ScoreCard
                        title="Profitability"
                        score={comparisonData.summary.profitabilityScore}
                        maxScore={2}
                      />
                      <ScoreCard
                        title="Growth"
                        score={comparisonData.summary.growthScore}
                        maxScore={2}
                      />
                      <ScoreCard
                        title="Overall"
                        score={comparisonData.summary.overallRating}
                        maxScore={6}
                      />
                    </div>
                  </div>
                )}
              </div>
            )}

            {/* Peer Comparison Tab */}
            {activeTab === 'peers' && <PeerComparisonPanel initialSymbol={targetSymbol} />}

            {/* Percentile Rankings Tab */}
            {activeTab === 'rankings' && (
              <div className="space-y-6">
                {percentileRankings.length > 0 ? (
                  <>
                    <PercentileRankChart rankings={percentileRankings} orientation="horizontal" />
                    <PercentileTable rankings={percentileRankings} />
                  </>
                ) : (
                  <div className="bg-gray-800 rounded-lg p-8 text-center border border-gray-700">
                    <p className="text-gray-400">
                      Run a peer comparison to see percentile rankings
                    </p>
                    <button
                      onClick={() => setActiveTab('peers')}
                      className="mt-4 px-4 py-2 bg-blue-600 hover:bg-blue-700 text-white rounded-lg transition-colors"
                    >
                      Go to Peer Comparison
                    </button>
                  </div>
                )}
              </div>
            )}

            {/* Sector Analysis Tab */}
            {activeTab === 'sectors' && (
              <div className="space-y-6">
                {availablePeers.length > 0 ? (
                  <>
                    <SectorHeatmap peers={availablePeers} />
                    {comparisonData?.benchmarks && (
                      <SectorPerformance
                        sector={comparisonData.benchmarks.sector}
                        metrics={{
                          avgPE: comparisonData.benchmarks.medianPe,
                          avgROE: comparisonData.benchmarks.medianRoe,
                          avgGrowth: comparisonData.benchmarks.medianRevenueGrowth,
                        }}
                      />
                    )}
                  </>
                ) : (
                  <div className="bg-gray-800 rounded-lg p-8 text-center border border-gray-700">
                    <p className="text-gray-400">
                      Search for a symbol to see sector analysis
                    </p>
                  </div>
                )}
              </div>
            )}

            {/* Stock Screener Tab */}
            {activeTab === 'screener' && (
              <StockScreenerPanel />
            )}
          </div>
        </div>

        {/* Error Display */}
        {error && (
          <div className="bg-red-900/20 border border-red-700 rounded-lg p-4">
            <div className="flex items-center gap-3">
              <svg className="w-5 h-5 text-red-400" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M12 8v4m0 4h.01M21 12a9 9 0 11-18 0 9 9 0 0118 0z" />
              </svg>
              <p className="text-red-400">{error}</p>
            </div>
          </div>
        )}
      </div>
    </div>
  );
};

// Score Card Component
interface ScoreCardProps {
  title: string;
  score: number;
  maxScore: number;
}

const ScoreCard: React.FC<ScoreCardProps> = ({ title, score, maxScore }) => {
  const percentage = (score / maxScore) * 100;
  const color =
    percentage >= 75 ? '#10b981' :
    percentage >= 50 ? '#3b82f6' :
    percentage >= 25 ? '#f59e0b' :
    '#ef4444';

  return (
    <div className="bg-gray-900 rounded-lg p-4 border border-gray-700">
      <h4 className="text-sm text-gray-400 mb-2">{title}</h4>
      <div className="flex items-baseline gap-2">
        <span className="text-2xl font-bold text-white">{score}</span>
        <span className="text-sm text-gray-500">/ {maxScore}</span>
      </div>
      <div className="mt-2 w-full bg-gray-700 rounded-full h-2">
        <div
          className="h-2 rounded-full transition-all duration-300"
          style={{
            width: `${percentage}%`,
            backgroundColor: color,
          }}
        />
      </div>
    </div>
  );
};

export default EquityResearchWithPeers;
