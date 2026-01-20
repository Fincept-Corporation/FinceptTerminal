/**
 * DataSourceSelector Component
 * Selector for choosing between sample data and user portfolios
 */

import React from 'react';
import { Database, Briefcase, RefreshCw } from 'lucide-react';
import { FINCEPT } from '../constants';
import type { DataSourceSelectorProps } from '../types';

export const DataSourceSelector: React.FC<DataSourceSelectorProps> = ({
  dataSource,
  setDataSource,
  userPortfolios,
  selectedPortfolioId,
  setSelectedPortfolioId,
  selectedPortfolioSummary,
  portfolioReturnsData,
  historicalDays,
  setHistoricalDays,
  isLoadingPortfolio,
  loadPortfolioData
}) => {
  return (
    <div
      className="p-3 rounded border"
      style={{ backgroundColor: FINCEPT.PANEL_BG, borderColor: FINCEPT.BORDER }}
    >
      <div className="flex items-center justify-between">
        <div className="flex items-center gap-3">
          <Database size={16} color={FINCEPT.CYAN} />
          <span
            className="text-xs font-bold uppercase"
            style={{ color: FINCEPT.WHITE }}
          >
            DATA SOURCE
          </span>
        </div>
        <div className="flex items-center gap-2">
          <div
            className="flex rounded border overflow-hidden"
            style={{ borderColor: FINCEPT.BORDER }}
          >
            <button
              onClick={() => setDataSource('sample')}
              className="px-3 py-1.5 text-xs font-bold uppercase transition-all"
              style={{
                backgroundColor:
                  dataSource === 'sample' ? FINCEPT.ORANGE : FINCEPT.DARK_BG,
                color: dataSource === 'sample' ? FINCEPT.DARK_BG : FINCEPT.GRAY
              }}
            >
              SAMPLE DATA
            </button>
            <button
              onClick={() => setDataSource('portfolio')}
              className="px-3 py-1.5 text-xs font-bold uppercase transition-all flex items-center gap-1"
              style={{
                backgroundColor:
                  dataSource === 'portfolio' ? FINCEPT.CYAN : FINCEPT.DARK_BG,
                color: dataSource === 'portfolio' ? FINCEPT.DARK_BG : FINCEPT.GRAY
              }}
            >
              <Briefcase size={12} />
              MY PORTFOLIOS
            </button>
          </div>
        </div>
      </div>

      {/* Portfolio Selector (shown when portfolio data source is selected) */}
      {dataSource === 'portfolio' && (
        <div className="mt-3 pt-3 border-t" style={{ borderColor: FINCEPT.BORDER }}>
          <div className="grid grid-cols-3 gap-4">
            <div>
              <label
                className="block text-xs font-mono mb-1"
                style={{ color: FINCEPT.GRAY }}
              >
                SELECT PORTFOLIO
              </label>
              <select
                value={selectedPortfolioId || ''}
                onChange={(e) => setSelectedPortfolioId(e.target.value || null)}
                className="w-full px-3 py-2 rounded text-sm font-mono border"
                style={{
                  backgroundColor: FINCEPT.DARK_BG,
                  borderColor: FINCEPT.BORDER,
                  color: FINCEPT.WHITE
                }}
              >
                <option value="">-- Select Portfolio --</option>
                {userPortfolios.map((p) => (
                  <option key={p.id} value={p.id}>
                    {p.name}
                  </option>
                ))}
              </select>
            </div>
            <div>
              <label
                className="block text-xs font-mono mb-1"
                style={{ color: FINCEPT.GRAY }}
              >
                HISTORICAL DAYS
              </label>
              <input
                type="number"
                value={historicalDays}
                onChange={(e) => setHistoricalDays(parseInt(e.target.value))}
                min="30"
                max="1000"
                className="w-full px-3 py-2 rounded text-sm font-mono border"
                style={{
                  backgroundColor: FINCEPT.DARK_BG,
                  borderColor: FINCEPT.BORDER,
                  color: FINCEPT.WHITE
                }}
              />
            </div>
            <div className="flex items-end">
              <button
                onClick={() =>
                  selectedPortfolioId && loadPortfolioData(selectedPortfolioId)
                }
                disabled={!selectedPortfolioId || isLoadingPortfolio}
                className="px-4 py-2 rounded font-bold uppercase tracking-wide text-xs hover:bg-opacity-90 transition-colors flex items-center gap-2 disabled:opacity-50"
                style={{ backgroundColor: FINCEPT.CYAN, color: FINCEPT.DARK_BG }}
              >
                {isLoadingPortfolio ? (
                  <RefreshCw size={14} className="animate-spin" />
                ) : (
                  <RefreshCw size={14} />
                )}
                LOAD DATA
              </button>
            </div>
          </div>

          {/* Portfolio Summary */}
          {selectedPortfolioSummary && portfolioReturnsData && (
            <div
              className="mt-3 p-3 rounded"
              style={{ backgroundColor: FINCEPT.DARK_BG }}
            >
              <div className="flex items-center gap-6 text-xs font-mono">
                <span style={{ color: FINCEPT.GRAY }}>
                  Portfolio:{' '}
                  <span style={{ color: FINCEPT.WHITE }}>
                    {selectedPortfolioSummary.portfolio.name}
                  </span>
                </span>
                <span style={{ color: FINCEPT.GRAY }}>
                  Holdings:{' '}
                  <span style={{ color: FINCEPT.CYAN }}>
                    {selectedPortfolioSummary.holdings.length}
                  </span>
                </span>
                <span style={{ color: FINCEPT.GRAY }}>
                  Data Points:{' '}
                  <span style={{ color: FINCEPT.CYAN }}>
                    {portfolioReturnsData.nScenarios}
                  </span>
                </span>
                <span style={{ color: FINCEPT.GRAY }}>
                  Period:{' '}
                  <span style={{ color: FINCEPT.WHITE }}>
                    {portfolioReturnsData.startDate} to{' '}
                    {portfolioReturnsData.endDate}
                  </span>
                </span>
              </div>
              <div className="mt-2 flex flex-wrap gap-2">
                {portfolioReturnsData.assets.map((asset) => (
                  <span
                    key={asset}
                    className="px-2 py-0.5 rounded text-xs font-mono"
                    style={{
                      backgroundColor: FINCEPT.BORDER,
                      color: FINCEPT.WHITE
                    }}
                  >
                    {asset}: {(portfolioReturnsData.weights[asset] * 100).toFixed(1)}%
                  </span>
                ))}
              </div>
            </div>
          )}

          {/* No portfolios message */}
          {userPortfolios.length === 0 && (
            <div
              className="mt-3 p-3 rounded text-center"
              style={{ backgroundColor: FINCEPT.DARK_BG }}
            >
              <p className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>
                No portfolios found. Create a portfolio in the Portfolio tab first.
              </p>
            </div>
          )}
        </div>
      )}
    </div>
  );
};
