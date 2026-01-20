/**
 * DataSourceSection Component
 * Data source selection and configuration
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
    <div
      className="rounded overflow-hidden m-2"
      style={{ border: `1px solid ${FINCEPT.BORDER}` }}
    >
      <button
        onClick={toggleSection}
        className="w-full px-3 py-2 flex items-center justify-between"
        style={{ backgroundColor: FINCEPT.HEADER_BG }}
      >
        <div className="flex items-center gap-2">
          <Database size={14} color={FINCEPT.CYAN} />
          <span className="text-xs font-bold uppercase" style={{ color: FINCEPT.WHITE }}>
            Data Source
          </span>
        </div>
        {expanded ? (
          <ChevronUp size={14} color={FINCEPT.GRAY} />
        ) : (
          <ChevronDown size={14} color={FINCEPT.GRAY} />
        )}
      </button>

      {expanded && (
        <div className="p-3 space-y-3">
          {/* Data source type buttons */}
          <div className="flex gap-1">
            <button
              onClick={() => setDataSourceType('manual')}
              className="flex-1 px-2 py-1.5 rounded text-xs font-mono"
              style={{
                backgroundColor: dataSourceType === 'manual' ? FINCEPT.ORANGE : 'transparent',
                color: dataSourceType === 'manual' ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                border: `1px solid ${FINCEPT.BORDER}`
              }}
            >
              Manual
            </button>
            <button
              onClick={() => setDataSourceType('portfolio')}
              className="flex-1 px-2 py-1.5 rounded text-xs font-mono flex items-center justify-center gap-1"
              style={{
                backgroundColor: dataSourceType === 'portfolio' ? FINCEPT.ORANGE : 'transparent',
                color: dataSourceType === 'portfolio' ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                border: `1px solid ${FINCEPT.BORDER}`
              }}
            >
              <Briefcase size={12} />
              Portfolio
            </button>
            <button
              onClick={() => setDataSourceType('symbol')}
              className="flex-1 px-2 py-1.5 rounded text-xs font-mono flex items-center justify-center gap-1"
              style={{
                backgroundColor: dataSourceType === 'symbol' ? FINCEPT.ORANGE : 'transparent',
                color: dataSourceType === 'symbol' ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                border: `1px solid ${FINCEPT.BORDER}`
              }}
            >
              <Search size={12} />
              Symbol
            </button>
          </div>

          {/* Portfolio selector */}
          {dataSourceType === 'portfolio' && (
            <div>
              <label className="block text-xs font-mono mb-1" style={{ color: FINCEPT.GRAY }}>
                SELECT PORTFOLIO
              </label>
              <select
                value={selectedPortfolioId}
                onChange={(e) => setSelectedPortfolioId(e.target.value)}
                className="w-full p-2 rounded text-xs font-mono"
                style={{
                  backgroundColor: FINCEPT.DARK_BG,
                  color: FINCEPT.WHITE,
                  border: `1px solid ${FINCEPT.BORDER}`
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
              <label className="block text-xs font-mono mb-1" style={{ color: FINCEPT.GRAY }}>
                {isMultiSymbolMode ? 'SYMBOLS (comma-separated)' : 'SYMBOL'}
              </label>
              <input
                type="text"
                value={isMultiSymbolMode ? symbolsInput : symbolInput}
                onChange={(e) => isMultiSymbolMode
                  ? setSymbolsInput(e.target.value.toUpperCase())
                  : setSymbolInput(e.target.value.toUpperCase())
                }
                placeholder={isMultiSymbolMode ? 'AAPL,MSFT,GOOGL' : 'AAPL'}
                className="w-full p-2 rounded text-xs font-mono"
                style={{
                  backgroundColor: FINCEPT.DARK_BG,
                  color: FINCEPT.WHITE,
                  border: `1px solid ${FINCEPT.BORDER}`
                }}
              />
            </div>
          )}

          {/* Historical days */}
          {dataSourceType !== 'manual' && (
            <div>
              <label className="block text-xs font-mono mb-1" style={{ color: FINCEPT.GRAY }}>
                HISTORICAL DAYS
              </label>
              <input
                type="number"
                value={historicalDays}
                onChange={(e) => setHistoricalDays(parseInt(e.target.value) || 365)}
                className="w-full p-2 rounded text-xs font-mono"
                style={{
                  backgroundColor: FINCEPT.DARK_BG,
                  color: FINCEPT.WHITE,
                  border: `1px solid ${FINCEPT.BORDER}`
                }}
              />
            </div>
          )}

          {/* Fetch data button */}
          {dataSourceType !== 'manual' && (
            <button
              onClick={fetchData}
              disabled={priceDataLoading}
              className="w-full py-2 rounded text-xs font-bold uppercase flex items-center justify-center gap-2"
              style={{
                backgroundColor: FINCEPT.CYAN,
                color: FINCEPT.DARK_BG,
                opacity: priceDataLoading ? 0.5 : 1
              }}
            >
              {priceDataLoading ? (
                <>
                  <RefreshCw size={12} className="animate-spin" />
                  Loading...
                </>
              ) : (
                <>
                  <Database size={12} />
                  Fetch Data
                </>
              )}
            </button>
          )}
        </div>
      )}
    </div>
  );
}
