/**
 * Data Step Component
 * Handles data input and visualization
 */

import React from 'react';
import {
  Database,
  Calculator,
  Search,
  RefreshCw,
  AlertCircle,
  ChevronRight,
} from 'lucide-react';
import {
  AreaChart,
  Area,
  XAxis,
  YAxis,
  CartesianGrid,
  Tooltip,
  ResponsiveContainer,
  ReferenceLine,
  Brush,
  ReferenceArea,
} from 'recharts';
import { BB } from '../constants';
import { formatPrice, formatNumber } from '../utils';
import { ZoomControls } from './ZoomControls';
import type { DataStepProps } from '../types';

export function DataStep({
  dataSourceType,
  setDataSourceType,
  symbolInput,
  setSymbolInput,
  historicalDays,
  setHistoricalDays,
  manualData,
  setManualData,
  priceData,
  priceDataLoading,
  dataStats,
  error,
  zoomedChartData,
  dataChartZoom,
  setDataChartZoom,
  fetchSymbolData,
  setCurrentStep,
  getDateForIndex,
  handleWheel,
  handleMouseDown,
  handleMouseMove,
  handleMouseUp,
  isSelecting,
  activeChart,
  refAreaLeft,
  refAreaRight,
}: DataStepProps) {
  return (
    <div className="flex h-full">
      {/* Left Panel - Input Controls */}
      <div
        className="w-80 flex-shrink-0 p-4 overflow-y-auto"
        style={{ backgroundColor: BB.panelBg, borderRight: `1px solid ${BB.borderDark}` }}
      >
        <div className="mb-4">
          <div className="text-xs font-mono mb-2" style={{ color: BB.textAmber }}>DATA SOURCE</div>
          <div className="flex gap-2">
            <button
              onClick={() => setDataSourceType('symbol')}
              className="flex-1 px-3 py-2 text-xs font-mono transition-colors"
              style={{
                backgroundColor: dataSourceType === 'symbol' ? BB.amber : BB.cardBg,
                color: dataSourceType === 'symbol' ? BB.black : BB.textSecondary,
                border: `1px solid ${dataSourceType === 'symbol' ? BB.amber : BB.borderDark}`,
              }}
            >
              <Database size={14} className="inline mr-2" />
              MARKET
            </button>
            <button
              onClick={() => setDataSourceType('manual')}
              className="flex-1 px-3 py-2 text-xs font-mono transition-colors"
              style={{
                backgroundColor: dataSourceType === 'manual' ? BB.amber : BB.cardBg,
                color: dataSourceType === 'manual' ? BB.black : BB.textSecondary,
                border: `1px solid ${dataSourceType === 'manual' ? BB.amber : BB.borderDark}`,
              }}
            >
              <Calculator size={14} className="inline mr-2" />
              MANUAL
            </button>
          </div>
        </div>

        {dataSourceType === 'symbol' ? (
          <div className="space-y-4">
            <div>
              <label className="block text-xs font-mono mb-1" style={{ color: BB.textMuted }}>TICKER</label>
              <input
                type="text"
                value={symbolInput}
                onChange={(e) => setSymbolInput(e.target.value.toUpperCase())}
                placeholder="AAPL"
                className="w-full px-3 py-2 text-sm font-mono"
                style={{
                  backgroundColor: BB.black,
                  color: BB.textPrimary,
                  border: `1px solid ${BB.borderLight}`,
                }}
              />
            </div>
            <div>
              <label className="block text-xs font-mono mb-1" style={{ color: BB.textMuted }}>LOOKBACK (DAYS)</label>
              <input
                type="number"
                value={historicalDays}
                onChange={(e) => setHistoricalDays(parseInt(e.target.value) || 365)}
                className="w-full px-3 py-2 text-sm font-mono"
                style={{
                  backgroundColor: BB.black,
                  color: BB.textPrimary,
                  border: `1px solid ${BB.borderLight}`,
                }}
              />
            </div>
            <button
              onClick={fetchSymbolData}
              disabled={priceDataLoading}
              className="w-full py-2 text-sm font-mono font-bold flex items-center justify-center gap-2"
              style={{
                backgroundColor: BB.amber,
                color: BB.black,
                opacity: priceDataLoading ? 0.7 : 1,
              }}
            >
              {priceDataLoading ? (
                <>
                  <RefreshCw size={14} className="animate-spin" />
                  LOADING...
                </>
              ) : (
                <>
                  <Search size={14} />
                  FETCH DATA
                </>
              )}
            </button>
          </div>
        ) : (
          <div>
            <label className="block text-xs font-mono mb-1" style={{ color: BB.textMuted }}>VALUES (COMMA SEP)</label>
            <textarea
              value={manualData}
              onChange={(e) => setManualData(e.target.value)}
              placeholder="100, 102, 98, 105..."
              rows={8}
              className="w-full px-3 py-2 text-sm font-mono"
              style={{
                backgroundColor: BB.black,
                color: BB.textPrimary,
                border: `1px solid ${BB.borderLight}`,
              }}
            />
          </div>
        )}

        {/* Statistics Panel */}
        {priceData.length > 0 && dataStats && (
          <div className="mt-4 p-3" style={{ backgroundColor: BB.cardBg, border: `1px solid ${BB.borderDark}` }}>
            <div className="text-xs font-mono mb-3" style={{ color: BB.textAmber }}>STATISTICS</div>
            <div className="grid grid-cols-2 gap-2 text-xs font-mono">
              <div>
                <div style={{ color: BB.textMuted }}>COUNT</div>
                <div style={{ color: BB.textPrimary }}>{priceData.length}</div>
              </div>
              <div>
                <div style={{ color: BB.textMuted }}>MEAN</div>
                <div style={{ color: BB.textPrimary }}>{formatPrice(dataStats.mean)}</div>
              </div>
              <div>
                <div style={{ color: BB.textMuted }}>LOW</div>
                <div style={{ color: BB.red }}>{formatPrice(dataStats.min)}</div>
              </div>
              <div>
                <div style={{ color: BB.textMuted }}>HIGH</div>
                <div style={{ color: BB.green }}>{formatPrice(dataStats.max)}</div>
              </div>
              <div>
                <div style={{ color: BB.textMuted }}>STD DEV</div>
                <div style={{ color: BB.textPrimary }}>{formatNumber(dataStats.std, 2)}</div>
              </div>
              <div>
                <div style={{ color: BB.textMuted }}>CHANGE</div>
                <div style={{ color: dataStats.changePercent >= 0 ? BB.green : BB.red }}>
                  {dataStats.changePercent >= 0 ? '+' : ''}{dataStats.changePercent.toFixed(2)}%
                </div>
              </div>
            </div>
          </div>
        )}

        {error && (
          <div
            className="mt-4 p-3 text-xs font-mono"
            style={{ backgroundColor: `${BB.red}20`, border: `1px solid ${BB.red}`, color: BB.red }}
          >
            <AlertCircle size={14} className="inline mr-2" />
            {error}
          </div>
        )}

        <button
          onClick={() => setCurrentStep('analysis')}
          disabled={priceData.length === 0}
          className="w-full mt-4 py-2 text-sm font-mono font-bold flex items-center justify-center gap-2"
          style={{
            backgroundColor: priceData.length > 0 ? BB.green : BB.cardBg,
            color: priceData.length > 0 ? BB.black : BB.textMuted,
            cursor: priceData.length > 0 ? 'pointer' : 'not-allowed',
          }}
        >
          CONTINUE <ChevronRight size={14} />
        </button>
      </div>

      {/* Right Panel - Chart */}
      <div className="flex-1 p-4" style={{ backgroundColor: BB.darkBg }}>
        {priceData.length > 0 ? (
          <div className="h-full flex flex-col">
            <div className="flex items-center justify-between mb-2">
              <div className="flex items-center gap-3">
                <div className="text-xs font-mono" style={{ color: BB.textAmber }}>PRICE CHART</div>
                {dataChartZoom && (
                  <div className="text-xs font-mono px-2 py-0.5" style={{ backgroundColor: BB.amber + '20', color: BB.amber }}>
                    {getDateForIndex(dataChartZoom.startIndex)} - {getDateForIndex(dataChartZoom.endIndex)}
                  </div>
                )}
              </div>
              <div className="flex items-center gap-3">
                <ZoomControls
                  chartType="data"
                  isZoomed={dataChartZoom !== null}
                  onZoomIn={() => {}}
                  onZoomOut={() => {}}
                  onResetZoom={() => setDataChartZoom(null)}
                  onPanLeft={() => {}}
                  onPanRight={() => {}}
                  canPanLeft={dataChartZoom !== null && dataChartZoom.startIndex > 0}
                  canPanRight={dataChartZoom !== null && dataChartZoom.endIndex < priceData.length - 1}
                />
                <div className="text-xs font-mono" style={{ color: BB.textMuted }}>{priceData.length} POINTS</div>
              </div>
            </div>
            <div
              className="flex-1"
              style={{ minHeight: 300 }}
              onWheel={(e) => handleWheel(e, 'data')}
            >
              <ResponsiveContainer width="100%" height="100%">
                <AreaChart
                  data={zoomedChartData}
                  margin={{ top: 10, right: 10, left: 0, bottom: 30 }}
                  onMouseDown={(e) => handleMouseDown(e, 'data')}
                  onMouseMove={handleMouseMove}
                  onMouseUp={handleMouseUp}
                  onMouseLeave={handleMouseUp}
                >
                  <defs>
                    <linearGradient id="colorValue" x1="0" y1="0" x2="0" y2="1">
                      <stop offset="5%" stopColor={dataStats && dataStats.changePercent >= 0 ? BB.green : BB.red} stopOpacity={0.3}/>
                      <stop offset="95%" stopColor={dataStats && dataStats.changePercent >= 0 ? BB.green : BB.red} stopOpacity={0}/>
                    </linearGradient>
                  </defs>
                  <CartesianGrid strokeDasharray="3 3" stroke={BB.chartGrid} />
                  <XAxis
                    dataKey="date"
                    tick={{ fill: BB.textMuted, fontSize: 9 }}
                    axisLine={{ stroke: BB.borderDark }}
                    tickLine={{ stroke: BB.borderDark }}
                    interval="preserveStartEnd"
                  />
                  <YAxis
                    tick={{ fill: BB.textMuted, fontSize: 10 }}
                    axisLine={{ stroke: BB.borderDark }}
                    tickLine={{ stroke: BB.borderDark }}
                    domain={['auto', 'auto']}
                    tickFormatter={(val) => formatPrice(val)}
                  />
                  <Tooltip
                    contentStyle={{
                      backgroundColor: BB.cardBg,
                      border: `1px solid ${BB.borderLight}`,
                      borderRadius: 0,
                    }}
                    labelFormatter={(label) => `Date: ${label}`}
                    labelStyle={{ color: BB.textAmber, fontSize: 11, fontWeight: 'bold' }}
                    itemStyle={{ color: BB.textPrimary, fontSize: 12 }}
                    formatter={(value: number) => [formatPrice(value), 'Price']}
                  />
                  <ReferenceLine y={dataStats?.mean} stroke={BB.amber} strokeDasharray="5 5" />
                  <Area
                    type="monotone"
                    dataKey="value"
                    stroke={dataStats && dataStats.changePercent >= 0 ? BB.green : BB.red}
                    fillOpacity={1}
                    fill="url(#colorValue)"
                    strokeWidth={1.5}
                  />
                  {!dataChartZoom && (
                    <Brush
                      dataKey="index"
                      height={20}
                      stroke={BB.amber}
                      fill={BB.cardBg}
                      tickFormatter={(val) => `${val}`}
                      onChange={(e) => {
                        if (e.startIndex !== undefined && e.endIndex !== undefined && e.startIndex !== e.endIndex) {
                          setDataChartZoom({ startIndex: e.startIndex, endIndex: e.endIndex });
                        }
                      }}
                    />
                  )}
                  {isSelecting && activeChart === 'data' && refAreaLeft !== null && refAreaRight !== null && (
                    <ReferenceArea
                      x1={refAreaLeft}
                      x2={refAreaRight}
                      strokeOpacity={0.3}
                      fill={BB.amber}
                      fillOpacity={0.2}
                    />
                  )}
                </AreaChart>
              </ResponsiveContainer>
            </div>
            <div className="text-xs font-mono mt-1 text-center" style={{ color: BB.textMuted }}>
              Scroll to pan | Ctrl+Scroll to zoom | Drag to select range
            </div>
          </div>
        ) : (
          <div className="h-full flex items-center justify-center">
            <div className="text-center">
              <Database size={48} style={{ color: BB.textMuted, margin: '0 auto 16px' }} />
              <div className="text-sm font-mono" style={{ color: BB.textMuted }}>NO DATA LOADED</div>
              <div className="text-xs font-mono mt-1" style={{ color: BB.textMuted }}>Fetch market data or enter values manually</div>
            </div>
          </div>
        )}
      </div>
    </div>
  );
}
