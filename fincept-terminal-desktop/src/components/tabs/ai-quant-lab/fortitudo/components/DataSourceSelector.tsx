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
    <div style={{ padding: '14px', backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}` }}>
      <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '10px' }}>
          <Database size={16} color={FINCEPT.BLUE} />
          <span style={{
            fontSize: '10px',
            fontWeight: 700,
            letterSpacing: '0.5px',
            color: FINCEPT.WHITE,
            fontFamily: 'monospace'
          }}>
            DATA SOURCE
          </span>
        </div>
        <div style={{ display: 'flex', gap: '0' }}>
          <button
            onClick={() => setDataSource('sample')}
            style={{
              padding: '8px 14px',
              backgroundColor: dataSource === 'sample' ? FINCEPT.BLUE : FINCEPT.DARK_BG,
              border: `1px solid ${FINCEPT.BORDER}`,
              color: dataSource === 'sample' ? '#000000' : FINCEPT.WHITE,
              opacity: dataSource === 'sample' ? 1 : 0.7,
              fontSize: '10px',
              fontWeight: 700,
              fontFamily: 'monospace',
              letterSpacing: '0.5px',
              cursor: 'pointer',
              transition: 'all 0.15s'
            }}
          >
            SAMPLE
          </button>
          <button
            onClick={() => setDataSource('portfolio')}
            style={{
              padding: '8px 14px',
              backgroundColor: dataSource === 'portfolio' ? FINCEPT.BLUE : FINCEPT.DARK_BG,
              border: `1px solid ${FINCEPT.BORDER}`,
              borderLeft: '0',
              marginLeft: '-1px',
              color: dataSource === 'portfolio' ? '#000000' : FINCEPT.WHITE,
              opacity: dataSource === 'portfolio' ? 1 : 0.7,
              fontSize: '10px',
              fontWeight: 700,
              fontFamily: 'monospace',
              letterSpacing: '0.5px',
              cursor: 'pointer',
              transition: 'all 0.15s',
              display: 'flex',
              alignItems: 'center',
              gap: '6px'
            }}
          >
            <Briefcase size={12} />
            MY PORTFOLIOS
          </button>
        </div>
      </div>

      {/* Portfolio Selector (shown when portfolio data source is selected) */}
      {dataSource === 'portfolio' && (
        <div style={{ marginTop: '12px', paddingTop: '12px', borderTop: `1px solid ${FINCEPT.BORDER}` }}>
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '12px' }}>
            <div>
              <label style={{
                display: 'block',
                fontSize: '9px',
                marginBottom: '6px',
                color: FINCEPT.WHITE,
                fontFamily: 'monospace',
                letterSpacing: '0.5px',
                opacity: 0.7
              }}>
                SELECT PORTFOLIO
              </label>
              <select
                value={selectedPortfolioId || ''}
                onChange={(e) => setSelectedPortfolioId(e.target.value || null)}
                style={{
                  width: '100%',
                  padding: '8px 10px',
                  backgroundColor: FINCEPT.DARK_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  color: FINCEPT.WHITE,
                  fontSize: '11px',
                  fontFamily: 'monospace',
                  outline: 'none'
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
              <label style={{
                display: 'block',
                fontSize: '9px',
                marginBottom: '6px',
                color: FINCEPT.WHITE,
                fontFamily: 'monospace',
                letterSpacing: '0.5px',
                opacity: 0.7
              }}>
                HISTORICAL DAYS
              </label>
              <input
                type="text"
                inputMode="numeric"
                value={historicalDays}
                onChange={(e) => {
                  const v = e.target.value;
                  if (v === '' || /^\d+$/.test(v)) {
                    setHistoricalDays(v === '' ? 0 : parseInt(v));
                  }
                }}
                style={{
                  width: '100%',
                  padding: '8px 10px',
                  backgroundColor: FINCEPT.DARK_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  color: FINCEPT.WHITE,
                  fontSize: '11px',
                  fontFamily: 'monospace',
                  outline: 'none'
                }}
              />
            </div>
            <div style={{ display: 'flex', alignItems: 'flex-end' }}>
              <button
                onClick={() =>
                  selectedPortfolioId && loadPortfolioData(selectedPortfolioId)
                }
                disabled={!selectedPortfolioId || isLoadingPortfolio}
                style={{
                  width: '100%',
                  padding: '10px 14px',
                  backgroundColor: FINCEPT.BLUE,
                  border: 'none',
                  color: '#000000',
                  fontSize: '10px',
                  fontWeight: 700,
                  fontFamily: 'monospace',
                  letterSpacing: '0.5px',
                  cursor: selectedPortfolioId && !isLoadingPortfolio ? 'pointer' : 'not-allowed',
                  opacity: !selectedPortfolioId || isLoadingPortfolio ? 0.5 : 1,
                  transition: 'all 0.15s',
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                  gap: '8px'
                }}
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
            <div style={{
              marginTop: '12px',
              padding: '12px',
              backgroundColor: FINCEPT.DARK_BG,
              border: `1px solid ${FINCEPT.BORDER}`,
              borderLeft: `3px solid ${FINCEPT.BLUE}`
            }}>
              <div style={{ display: 'flex', alignItems: 'center', gap: '16px', fontSize: '10px', fontFamily: 'monospace' }}>
                <span style={{ color: FINCEPT.WHITE, opacity: 0.5 }}>
                  Portfolio:{' '}
                  <span style={{ color: FINCEPT.WHITE, opacity: 1 }}>
                    {selectedPortfolioSummary.portfolio.name}
                  </span>
                </span>
                <span style={{ color: FINCEPT.WHITE, opacity: 0.5 }}>
                  Holdings:{' '}
                  <span style={{ color: FINCEPT.BLUE }}>
                    {selectedPortfolioSummary.holdings.length}
                  </span>
                </span>
                <span style={{ color: FINCEPT.WHITE, opacity: 0.5 }}>
                  Data Points:{' '}
                  <span style={{ color: FINCEPT.BLUE }}>
                    {portfolioReturnsData.nScenarios}
                  </span>
                </span>
                <span style={{ color: FINCEPT.WHITE, opacity: 0.5 }}>
                  Period:{' '}
                  <span style={{ color: FINCEPT.WHITE, opacity: 1 }}>
                    {portfolioReturnsData.startDate} to{' '}
                    {portfolioReturnsData.endDate}
                  </span>
                </span>
              </div>
              <div style={{ marginTop: '8px', display: 'flex', flexWrap: 'wrap', gap: '6px' }}>
                {portfolioReturnsData.assets.map((asset) => (
                  <span
                    key={asset}
                    style={{
                      padding: '4px 8px',
                      backgroundColor: FINCEPT.BLUE + '20',
                      border: `1px solid ${FINCEPT.BLUE}`,
                      color: FINCEPT.BLUE,
                      fontSize: '9px',
                      fontFamily: 'monospace'
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
            <div style={{
              marginTop: '12px',
              padding: '12px',
              backgroundColor: FINCEPT.DARK_BG,
              border: `1px solid ${FINCEPT.BORDER}`,
              textAlign: 'center'
            }}>
              <p style={{ fontSize: '10px', fontFamily: 'monospace', color: FINCEPT.WHITE, opacity: 0.6 }}>
                No portfolios found. Create a portfolio in the Portfolio tab first.
              </p>
            </div>
          )}
        </div>
      )}
    </div>
  );
};
