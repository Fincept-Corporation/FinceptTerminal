/**
 * DataSourceSection Component
 * Data source selection and configuration
 */

import React from 'react';
import { Database, Briefcase, Search, ChevronUp, ChevronDown, RefreshCw } from 'lucide-react';
import { FINCEPT } from '../constants';
import type { DataSourceSectionProps } from '../types';

export const DataSourceSection: React.FC<DataSourceSectionProps> = ({
  dataSourceType,
  setDataSourceType,
  portfolios,
  selectedPortfolioId,
  setSelectedPortfolioId,
  symbolInput,
  setSymbolInput,
  historicalDays,
  setHistoricalDays,
  priceDataLoading,
  priceData,
  loadDataFromSource,
  expanded,
  toggleSection
}) => {
  return (
    <div
      className="rounded overflow-hidden"
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
          {/* Source Type Selection */}
          <div className="flex gap-2">
            <button
              onClick={() => setDataSourceType('manual')}
              className={`flex-1 px-2 py-1.5 rounded text-xs font-mono ${dataSourceType === 'manual' ? 'border-2' : ''}`}
              style={{
                backgroundColor: dataSourceType === 'manual' ? FINCEPT.HEADER_BG : FINCEPT.DARK_BG,
                borderColor: FINCEPT.ORANGE,
                color: dataSourceType === 'manual' ? FINCEPT.ORANGE : FINCEPT.GRAY
              }}
            >
              Manual
            </button>
            <button
              onClick={() => setDataSourceType('portfolio')}
              className={`flex-1 px-2 py-1.5 rounded text-xs font-mono ${dataSourceType === 'portfolio' ? 'border-2' : ''}`}
              style={{
                backgroundColor: dataSourceType === 'portfolio' ? FINCEPT.HEADER_BG : FINCEPT.DARK_BG,
                borderColor: FINCEPT.ORANGE,
                color: dataSourceType === 'portfolio' ? FINCEPT.ORANGE : FINCEPT.GRAY
              }}
            >
              <Briefcase size={12} className="inline mr-1" />
              Portfolio
            </button>
            <button
              onClick={() => setDataSourceType('symbol')}
              className={`flex-1 px-2 py-1.5 rounded text-xs font-mono ${dataSourceType === 'symbol' ? 'border-2' : ''}`}
              style={{
                backgroundColor: dataSourceType === 'symbol' ? FINCEPT.HEADER_BG : FINCEPT.DARK_BG,
                borderColor: FINCEPT.ORANGE,
                color: dataSourceType === 'symbol' ? FINCEPT.ORANGE : FINCEPT.GRAY
              }}
            >
              <Search size={12} className="inline mr-1" />
              Symbol
            </button>
          </div>

          {/* Portfolio Selection */}
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
                {portfolios.length === 0 ? (
                  <option value="">No portfolios found</option>
                ) : (
                  portfolios.map((p) => (
                    <option key={p.id} value={p.id}>
                      {p.name}
                    </option>
                  ))
                )}
              </select>
            </div>
          )}

          {/* Symbol Input */}
          {dataSourceType === 'symbol' && (
            <div>
              <label className="block text-xs font-mono mb-1" style={{ color: FINCEPT.GRAY }}>
                TICKER SYMBOL
              </label>
              <input
                type="text"
                value={symbolInput}
                onChange={(e) => setSymbolInput(e.target.value.toUpperCase())}
                placeholder="AAPL"
                className="w-full p-2 rounded text-xs font-mono"
                style={{
                  backgroundColor: FINCEPT.DARK_BG,
                  color: FINCEPT.WHITE,
                  border: `1px solid ${FINCEPT.BORDER}`
                }}
              />
            </div>
          )}

          {/* Historical Days */}
          {dataSourceType !== 'manual' && (
            <div>
              <label className="block text-xs font-mono mb-1" style={{ color: FINCEPT.GRAY }}>
                HISTORICAL DAYS
              </label>
              <input
                type="text"
                inputMode="numeric"
                value={historicalDays}
                onChange={(e) => {
                  const v = e.target.value;
                  if (v === '' || /^\d+$/.test(v)) {
                    setHistoricalDays(parseInt(v) || 365);
                  }
                }}
                className="w-full p-2 rounded text-xs font-mono"
                style={{
                  backgroundColor: FINCEPT.DARK_BG,
                  color: FINCEPT.WHITE,
                  border: `1px solid ${FINCEPT.BORDER}`
                }}
              />
            </div>
          )}

          {/* Load Data Button */}
          {dataSourceType !== 'manual' && (
            <button
              onClick={loadDataFromSource}
              disabled={priceDataLoading}
              className="w-full py-2 rounded font-bold text-xs uppercase flex items-center justify-center gap-2"
              style={{ backgroundColor: FINCEPT.CYAN, color: FINCEPT.DARK_BG }}
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

          {/* Data Info */}
          {priceData && dataSourceType !== 'manual' && (
            <div className="p-2 rounded text-xs font-mono" style={{ backgroundColor: FINCEPT.DARK_BG }}>
              <div className="flex justify-between" style={{ color: FINCEPT.GRAY }}>
                <span>Assets:</span>
                <span style={{ color: FINCEPT.CYAN }}>{priceData.assets.length}</span>
              </div>
              <div className="flex justify-between" style={{ color: FINCEPT.GRAY }}>
                <span>Data Points:</span>
                <span style={{ color: FINCEPT.WHITE }}>{priceData.nDataPoints}</span>
              </div>
              <div className="flex justify-between" style={{ color: FINCEPT.GRAY }}>
                <span>Range:</span>
                <span style={{ color: FINCEPT.WHITE }}>{priceData.startDate} to {priceData.endDate}</span>
              </div>
            </div>
          )}
        </div>
      )}
    </div>
  );
};
