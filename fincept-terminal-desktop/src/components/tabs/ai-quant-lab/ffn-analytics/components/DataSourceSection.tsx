/**
 * DataSourceSection Component
 * Data source selection and configuration
 * GREEN THEME
 */

import React from 'react';
import {
  Database,
  Briefcase,
  Search,
  RefreshCw,
  ChevronUp,
  ChevronDown
} from 'lucide-react';
import { FINCEPT } from '../constants';
import type { DataSourceSectionProps } from '../types';

export function DataSourceSection({
  dataSourceType,
  setDataSourceType,
  portfolios,
  selectedPortfolioId,
  setSelectedPortfolioId,
  symbolInput,
  setSymbolInput,
  symbolsInput,
  setSymbolsInput,
  analysisMode,
  historicalDays,
  setHistoricalDays,
  priceDataLoading,
  fetchData,
  expanded,
  toggleSection
}: DataSourceSectionProps) {
  const isMultiSymbolMode = analysisMode === 'comparison' || analysisMode === 'optimize';

  return (
    <div style={{ borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
      <button
        onClick={toggleSection}
        style={{
          width: '100%',
          padding: '10px 12px',
          borderBottom: `1px solid ${FINCEPT.BORDER}`,
          backgroundColor: FINCEPT.PANEL_BG,
          border: 'none',
          cursor: 'pointer',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
          transition: 'background-color 0.15s'
        }}
        onMouseEnter={(e) => {
          e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
        }}
        onMouseLeave={(e) => {
          e.currentTarget.style.backgroundColor = FINCEPT.PANEL_BG;
        }}
      >
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <Database size={14} color={FINCEPT.GREEN} />
          <span style={{
            fontSize: '10px',
            fontWeight: 700,
            fontFamily: 'monospace',
            color: FINCEPT.GREEN,
            letterSpacing: '0.5px'
          }}>
            DATA SOURCE
          </span>
        </div>
        {expanded ? (
          <ChevronUp size={14} color={FINCEPT.WHITE} style={{ opacity: 0.5 }} />
        ) : (
          <ChevronDown size={14} color={FINCEPT.WHITE} style={{ opacity: 0.5 }} />
        )}
      </button>

      {expanded && (
        <div style={{ padding: '12px' }}>
          {/* Data source type buttons - Joined Design */}
          <div style={{ display: 'flex', gap: '0', marginBottom: '12px' }}>
            <button
              onClick={() => setDataSourceType('manual')}
              style={{
                flex: 1,
                padding: '8px 12px',
                backgroundColor: dataSourceType === 'manual' ? FINCEPT.GREEN : FINCEPT.DARK_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                color: dataSourceType === 'manual' ? '#000000' : FINCEPT.WHITE,
                opacity: dataSourceType === 'manual' ? 1 : 0.7,
                fontSize: '10px',
                fontWeight: 700,
                fontFamily: 'monospace',
                letterSpacing: '0.5px',
                cursor: 'pointer',
                transition: 'all 0.15s'
              }}
            >
              MANUAL
            </button>
            <button
              onClick={() => setDataSourceType('portfolio')}
              style={{
                flex: 1,
                padding: '8px 12px',
                backgroundColor: dataSourceType === 'portfolio' ? FINCEPT.GREEN : FINCEPT.DARK_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderLeft: '0',
                marginLeft: '-1px',
                color: dataSourceType === 'portfolio' ? '#000000' : FINCEPT.WHITE,
                opacity: dataSourceType === 'portfolio' ? 1 : 0.7,
                fontSize: '10px',
                fontWeight: 700,
                fontFamily: 'monospace',
                letterSpacing: '0.5px',
                cursor: 'pointer',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                gap: '6px',
                transition: 'all 0.15s'
              }}
            >
              <Briefcase size={12} />
              PORTFOLIO
            </button>
            <button
              onClick={() => setDataSourceType('symbol')}
              style={{
                flex: 1,
                padding: '8px 12px',
                backgroundColor: dataSourceType === 'symbol' ? FINCEPT.GREEN : FINCEPT.DARK_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderLeft: '0',
                marginLeft: '-1px',
                color: dataSourceType === 'symbol' ? '#000000' : FINCEPT.WHITE,
                opacity: dataSourceType === 'symbol' ? 1 : 0.7,
                fontSize: '10px',
                fontWeight: 700,
                fontFamily: 'monospace',
                letterSpacing: '0.5px',
                cursor: 'pointer',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                gap: '6px',
                transition: 'all 0.15s'
              }}
            >
              <Search size={12} />
              SYMBOL
            </button>
          </div>

          {/* Portfolio selector */}
          {dataSourceType === 'portfolio' && (
            <div>
              <label style={{
                display: 'block',
                fontSize: '9px',
                fontFamily: 'monospace',
                color: FINCEPT.WHITE,
                opacity: 0.5,
                marginBottom: '8px',
                letterSpacing: '0.5px'
              }}>
                SELECT PORTFOLIO
              </label>
              <select
                value={selectedPortfolioId}
                onChange={(e) => setSelectedPortfolioId(e.target.value)}
                style={{
                  width: '100%',
                  padding: '10px 12px',
                  backgroundColor: FINCEPT.DARK_BG,
                  color: FINCEPT.WHITE,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  fontSize: '10px',
                  fontFamily: 'monospace',
                  outline: 'none',
                  cursor: 'pointer'
                }}
              >
                {portfolios.map(p => (
                  <option key={p.id} value={p.id}>{p.name}</option>
                ))}
              </select>
            </div>
          )}

          {/* Symbol input */}
          {dataSourceType === 'symbol' && (
            <div>
              <label style={{
                display: 'block',
                fontSize: '9px',
                fontFamily: 'monospace',
                color: FINCEPT.WHITE,
                opacity: 0.5,
                marginBottom: '8px',
                letterSpacing: '0.5px'
              }}>
                {isMultiSymbolMode ? 'SYMBOLS (COMMA SEPARATED)' : 'SYMBOL'}
              </label>
              <input
                type="text"
                value={isMultiSymbolMode ? symbolsInput : symbolInput}
                onChange={(e) => isMultiSymbolMode
                  ? setSymbolsInput(e.target.value.toUpperCase())
                  : setSymbolInput(e.target.value.toUpperCase())
                }
                placeholder={isMultiSymbolMode ? 'AAPL,MSFT,GOOGL' : 'AAPL'}
                style={{
                  width: '100%',
                  padding: '10px 12px',
                  backgroundColor: FINCEPT.DARK_BG,
                  color: FINCEPT.WHITE,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  fontSize: '10px',
                  fontFamily: 'monospace',
                  fontWeight: 700,
                  outline: 'none'
                }}
              />
            </div>
          )}

          {/* Historical days */}
          {dataSourceType !== 'manual' && (
            <div>
              <label style={{
                display: 'block',
                fontSize: '9px',
                fontFamily: 'monospace',
                color: FINCEPT.WHITE,
                opacity: 0.5,
                marginBottom: '8px',
                letterSpacing: '0.5px'
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
                  padding: '10px 12px',
                  backgroundColor: FINCEPT.DARK_BG,
                  color: FINCEPT.WHITE,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  fontSize: '10px',
                  fontFamily: 'monospace',
                  fontWeight: 700,
                  outline: 'none'
                }}
              />
            </div>
          )}

          {/* Fetch data button */}
          {dataSourceType !== 'manual' && (
            <button
              onClick={fetchData}
              disabled={priceDataLoading}
              style={{
                width: '100%',
                padding: '10px 12px',
                backgroundColor: priceDataLoading ? FINCEPT.DARK_BG : FINCEPT.GREEN,
                border: 'none',
                color: priceDataLoading ? FINCEPT.WHITE : '#000000',
                opacity: priceDataLoading ? 0.5 : 1,
                fontSize: '10px',
                fontWeight: 700,
                fontFamily: 'monospace',
                letterSpacing: '0.5px',
                cursor: priceDataLoading ? 'not-allowed' : 'pointer',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                gap: '8px',
                transition: 'all 0.15s'
              }}
              onMouseEnter={(e) => {
                if (!priceDataLoading) {
                  e.currentTarget.style.opacity = '0.9';
                }
              }}
              onMouseLeave={(e) => {
                if (!priceDataLoading) {
                  e.currentTarget.style.opacity = '1';
                }
              }}
            >
              {priceDataLoading ? (
                <>
                  <RefreshCw size={12} style={{ animation: 'spin 1s linear infinite' }} />
                  LOADING...
                </>
              ) : (
                <>
                  <Database size={12} />
                  FETCH DATA
                </>
              )}
            </button>
          )}
        </div>
      )}
    </div>
  );
}

// Add keyframe animation for spinner
const style = document.createElement('style');
style.textContent = `
  @keyframes spin {
    from { transform: rotate(0deg); }
    to { transform: rotate(360deg); }
  }
`;
if (!document.querySelector('[data-ffn-spin-animation]')) {
  style.setAttribute('data-ffn-spin-animation', 'true');
  document.head.appendChild(style);
}
