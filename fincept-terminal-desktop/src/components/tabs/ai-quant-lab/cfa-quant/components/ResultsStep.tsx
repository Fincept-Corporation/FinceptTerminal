/**
 * Results Step Component
 * Displays analysis results with charts and metrics
 */

import React, { useMemo } from 'react';
import {
  CheckCircle2,
  ChevronLeft,
  Play,
  Copy,
  Download,
} from 'lucide-react';
import {
  ComposedChart,
  Line,
  Bar,
  Scatter,
  XAxis,
  YAxis,
  CartesianGrid,
  Tooltip,
  ResponsiveContainer,
  ReferenceLine,
  ReferenceArea,
  Brush,
} from 'recharts';
import { BB, analysisConfigs } from '../constants';
import { formatNumber, formatPrice, extractKeyMetrics, buildResultChartData } from '../utils';
import { ZoomControls } from './ZoomControls';
import { ChartLegend } from './ChartLegend';
import type { ResultsStepProps } from '../types';

export function ResultsStep({
  analysisResult,
  selectedAnalysis,
  symbolInput,
  priceData,
  chartData,
  setCurrentStep,
  setAnalysisResult,
  resultsChartZoom,
  setResultsChartZoom,
  getDateForIndex,
  handleWheel,
  handleMouseDown,
  handleMouseMove,
  handleMouseUp,
  isSelecting,
  activeChart,
  refAreaLeft,
  refAreaRight,
}: ResultsStepProps) {
  if (!analysisResult?.result) return null;

  const config = analysisConfigs.find(a => a.id === selectedAnalysis);
  const result = analysisResult.result as Record<string, unknown>;

  const keyMetrics = useMemo(() =>
    extractKeyMetrics(selectedAnalysis, result, priceData),
    [selectedAnalysis, result, priceData]
  );

  const resultChartData = useMemo(() =>
    buildResultChartData(selectedAnalysis, result, chartData, priceData),
    [selectedAnalysis, result, chartData, priceData]
  );

  // Compute cluster boundary for unsupervised learning
  const clusterBoundary = useMemo(() => {
    if (selectedAnalysis !== 'unsupervised_learning') return null;
    const value = result.value as Record<string, Record<string, unknown>>;
    if (value?.kmeans) {
      const clusterLabels = value.kmeans.cluster_labels as number[];
      if (clusterLabels && clusterLabels.length > 0 && priceData.length > 0) {
        const clusterSums: Record<number, { sum: number; count: number }> = {};
        priceData.forEach((price, i) => {
          const cluster = i < clusterLabels.length ? clusterLabels[i] : 0;
          if (!clusterSums[cluster]) clusterSums[cluster] = { sum: 0, count: 0 };
          clusterSums[cluster].sum += price;
          clusterSums[cluster].count += 1;
        });
        const means = Object.values(clusterSums).map(s => s.sum / s.count).sort((a, b) => a - b);
        if (means.length >= 2) {
          return (means[0] + means[1]) / 2;
        }
      }
    }
    return null;
  }, [selectedAnalysis, result, priceData]);

  // Bootstrap reference values
  const bootstrapRefs = useMemo(() => {
    if (selectedAnalysis !== 'resampling_methods') return null;
    const value = result.value as Record<string, Record<string, unknown>>;
    if (value?.bootstrap) {
      const mean = value.bootstrap.mean as number;
      const ci = value.bootstrap.confidence_interval as [number, number];
      if (resultChartData.length > 0 && (resultChartData[0] as any)?.meanBinIdx !== undefined) {
        return {
          mean,
          ciLow: ci?.[0],
          ciHigh: ci?.[1],
          meanIdx: (resultChartData[0] as any).meanBinIdx as number,
          ciLowIdx: (resultChartData[0] as any).ciLowIdx as number,
          ciHighIdx: (resultChartData[0] as any).ciHighIdx as number,
        };
      }
    }
    return null;
  }, [selectedAnalysis, result, resultChartData]);

  const exportResults = () => {
    if (!analysisResult) return;
    const content = JSON.stringify(analysisResult, null, 2);
    const blob = new Blob([content], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `${selectedAnalysis}_${symbolInput}_results.json`;
    a.click();
    URL.revokeObjectURL(url);
  };

  const copyToClipboard = (text: string) => {
    navigator.clipboard.writeText(text);
  };

  return (
    <div className="flex h-full">
      {/* Left Panel - Metrics */}
      <div
        className="w-80 flex-shrink-0 p-4 overflow-y-auto"
        style={{ backgroundColor: BB.panelBg, borderRight: `1px solid ${BB.borderDark}` }}
      >
        <div className="flex items-center justify-between mb-3">
          <div className="text-xs font-mono" style={{ color: BB.textAmber }}>RESULTS</div>
          <div className="flex gap-2">
            <button
              onClick={() => copyToClipboard(JSON.stringify(analysisResult, null, 2))}
              className="p-1.5"
              style={{ backgroundColor: BB.cardBg }}
            >
              <Copy size={14} style={{ color: BB.textMuted }} />
            </button>
            <button onClick={exportResults} className="p-1.5" style={{ backgroundColor: BB.cardBg }}>
              <Download size={14} style={{ color: BB.textMuted }} />
            </button>
          </div>
        </div>

        {/* Status Banner */}
        <div className="p-3 mb-4" style={{ backgroundColor: `${BB.green}10`, border: `1px solid ${BB.green}` }}>
          <div className="flex items-center gap-2">
            <CheckCircle2 size={16} style={{ color: BB.green }} />
            <span className="text-xs font-mono font-bold" style={{ color: BB.green }}>COMPLETE</span>
          </div>
          <div className="text-xs font-mono mt-1" style={{ color: BB.textMuted }}>
            {config?.name} • {analysisResult.timestamp ? new Date(analysisResult.timestamp).toLocaleTimeString() : 'Just now'}
          </div>
        </div>

        {/* Key Metrics */}
        {keyMetrics.length > 0 && (
          <div className="space-y-2 mb-4">
            {keyMetrics.map((metric, i) => (
              <div
                key={i}
                className="flex items-center justify-between p-3"
                style={{ backgroundColor: BB.cardBg, border: `1px solid ${BB.borderDark}` }}
              >
                <span className="text-xs font-mono" style={{ color: BB.textMuted }}>{metric.label}</span>
                <div className="flex items-center gap-2">
                  {metric.icon && <metric.icon size={14} style={{ color: metric.color }} />}
                  <span className="text-sm font-mono font-bold" style={{ color: metric.color }}>{metric.value}</span>
                </div>
              </div>
            ))}
          </div>
        )}

        {/* Raw Data Section */}
        <div className="mb-4">
          <div className="text-xs font-mono mb-2" style={{ color: BB.textAmber }}>RAW OUTPUT</div>
          <div
            className="p-3 text-xs font-mono overflow-x-auto"
            style={{ backgroundColor: BB.black, border: `1px solid ${BB.borderDark}`, maxHeight: 200, overflowY: 'auto' }}
          >
            {Object.entries(result).map(([key, value]) => {
              if (key === 'model_summary' || (Array.isArray(value) && value.length > 20)) return null;
              return (
                <div key={key} className="flex gap-2 py-1" style={{ borderBottom: `1px solid ${BB.borderDark}` }}>
                  <span style={{ color: BB.textMuted }}>{key}:</span>
                  <span style={{ color: BB.textPrimary }} className="truncate">
                    {typeof value === 'object' ? JSON.stringify(value).substring(0, 50) + '...' : String(value)}
                  </span>
                </div>
              );
            })}
          </div>
        </div>

        <div className="flex gap-2">
          <button
            onClick={() => setCurrentStep('analysis')}
            className="flex-1 py-2 text-xs font-mono"
            style={{ backgroundColor: BB.cardBg, color: BB.textSecondary, border: `1px solid ${BB.borderDark}` }}
          >
            <ChevronLeft size={12} className="inline" /> MODELS
          </button>
          <button
            onClick={() => {
              setCurrentStep('data');
              setAnalysisResult(null);
            }}
            className="flex-1 py-2 text-xs font-mono font-bold"
            style={{ backgroundColor: BB.amber, color: BB.black }}
          >
            NEW <Play size={12} className="inline ml-1" />
          </button>
        </div>
      </div>

      {/* Right Panel - Chart with Results */}
      <div className="flex-1 p-4" style={{ backgroundColor: BB.darkBg }}>
        <div className="h-full flex flex-col">
          <div className="flex items-center justify-between mb-2">
            <div className="flex items-center gap-3">
              <div className="text-xs font-mono" style={{ color: BB.textAmber }}>{config?.shortName} OUTPUT</div>
              {resultsChartZoom && (
                <div className="text-xs font-mono px-2 py-0.5" style={{ backgroundColor: BB.amber + '20', color: BB.amber }}>
                  {getDateForIndex(resultsChartZoom.startIndex)} - {getDateForIndex(resultsChartZoom.endIndex)}
                </div>
              )}
            </div>
            <div className="flex items-center gap-3">
              <ZoomControls
                chartType="results"
                isZoomed={resultsChartZoom !== null}
                onZoomIn={() => {}}
                onZoomOut={() => {}}
                onResetZoom={() => setResultsChartZoom(null)}
                onPanLeft={() => {}}
                onPanRight={() => {}}
                canPanLeft={resultsChartZoom !== null && resultsChartZoom.startIndex > 0}
                canPanRight={resultsChartZoom !== null && resultsChartZoom.endIndex < resultChartData.length - 1}
              />
              <div className="text-xs font-mono" style={{ color: BB.textMuted }}>{symbolInput.toUpperCase()}</div>
            </div>
          </div>
          <div
            className="flex-1"
            style={{ minHeight: 300 }}
            onWheel={(e) => handleWheel(e, 'results')}
          >
            <ResponsiveContainer width="100%" height="100%">
              <ComposedChart
                data={resultsChartZoom ? resultChartData.slice(resultsChartZoom.startIndex, resultsChartZoom.endIndex + 1) : resultChartData}
                margin={{ top: 10, right: 10, left: 0, bottom: 30 }}
                onMouseDown={(e) => handleMouseDown(e, 'results')}
                onMouseMove={handleMouseMove}
                onMouseUp={handleMouseUp}
                onMouseLeave={handleMouseUp}
              >
                <CartesianGrid strokeDasharray="3 3" stroke={BB.chartGrid} />
                <XAxis
                  dataKey={selectedAnalysis === 'resampling_methods' ? 'index' : 'date'}
                  tick={{ fill: BB.textMuted, fontSize: 9 }}
                  axisLine={{ stroke: BB.borderDark }}
                  interval="preserveStartEnd"
                  tickFormatter={(val) => {
                    if (selectedAnalysis === 'resampling_methods' && (resultChartData[val] as any)?.binCenter !== undefined) {
                      return formatNumber((resultChartData[val] as any).binCenter as number, 1);
                    }
                    return val;
                  }}
                />
                <YAxis
                  tick={{ fill: BB.textMuted, fontSize: 10 }}
                  axisLine={{ stroke: BB.borderDark }}
                  domain={['auto', 'auto']}
                  tickFormatter={(val) => selectedAnalysis === 'resampling_methods' ? String(Math.round(val)) : formatPrice(val)}
                  label={selectedAnalysis === 'resampling_methods' ? { value: 'COUNT', angle: -90, position: 'insideLeft', fill: BB.textMuted, fontSize: 9 } : undefined}
                />
                <Tooltip
                  contentStyle={{
                    backgroundColor: BB.cardBg,
                    border: `1px solid ${BB.borderLight}`,
                    borderRadius: 0,
                    fontSize: 11,
                  }}
                  labelFormatter={(label) => selectedAnalysis === 'resampling_methods' ? `Value: ${label}` : `Date: ${label}`}
                  labelStyle={{ color: BB.textAmber, fontWeight: 'bold' }}
                />

                {/* Render lines based on analysis type */}
                {selectedAnalysis !== 'forecasting' && selectedAnalysis !== 'unsupervised_learning' &&
                 selectedAnalysis !== 'resampling_methods' && selectedAnalysis !== 'validate_data' &&
                 selectedAnalysis !== 'stationarity_test' && (
                  <Line type="monotone" dataKey="value" stroke={BB.textSecondary} strokeWidth={1} dot={false} name="Actual" />
                )}

                {selectedAnalysis === 'stationarity_test' && (
                  <>
                    <Line type="monotone" dataKey="value" stroke={BB.textSecondary} strokeWidth={1} dot={false} name="Price" />
                    <Line type="monotone" dataKey="upperBand" stroke={BB.amber} strokeWidth={1} strokeDasharray="3 3" dot={false} connectNulls={false} />
                    <Line type="monotone" dataKey="lowerBand" stroke={BB.amber} strokeWidth={1} strokeDasharray="3 3" dot={false} connectNulls={false} />
                    <Line type="monotone" dataKey="rollingMean" stroke={BB.green} strokeWidth={2} dot={false} connectNulls={false} />
                    {resultChartData.length > 0 && (resultChartData[0] as any).overallMean !== undefined && (
                      <ReferenceLine y={(resultChartData[0] as any).overallMean} stroke={BB.blueLight} strokeWidth={2} strokeDasharray="5 5" />
                    )}
                  </>
                )}

                {selectedAnalysis === 'validate_data' && (
                  <>
                    <Line type="monotone" dataKey="normalValue" stroke={BB.green} strokeWidth={1.5} dot={false} connectNulls={false} />
                    <Scatter dataKey="suspiciousValue" fill={BB.amber} />
                    <Scatter dataKey="outlierValue" fill={BB.red} />
                    {resultChartData.length > 0 && (resultChartData[0] as any).mean !== undefined && (
                      <ReferenceLine y={(resultChartData[0] as any).mean} stroke={BB.blueLight} strokeWidth={2} strokeDasharray="5 5" />
                    )}
                  </>
                )}

                {selectedAnalysis === 'resampling_methods' && (
                  <>
                    {bootstrapRefs && (
                      <ReferenceArea x1={bootstrapRefs.ciLowIdx} x2={bootstrapRefs.ciHighIdx} fill={BB.blueLight} fillOpacity={0.15} />
                    )}
                    <Bar dataKey="value" name="Frequency" fill={BB.green} fillOpacity={0.7} />
                    {bootstrapRefs && (
                      <ReferenceLine x={bootstrapRefs.meanIdx} stroke={BB.amber} strokeWidth={2} />
                    )}
                  </>
                )}

                {selectedAnalysis === 'unsupervised_learning' && (
                  <>
                    {clusterBoundary !== null && (
                      <ReferenceLine y={clusterBoundary} stroke={BB.amber} strokeWidth={2} strokeDasharray="6 3" />
                    )}
                    <Line
                      type="monotone"
                      dataKey="value"
                      stroke={BB.textMuted}
                      strokeWidth={1}
                      strokeOpacity={0.5}
                      dot={(props) => {
                        const p = props as { cx: number; cy: number; payload: { cluster?: number } };
                        if (!p.cx || !p.cy) return <g />;
                        const colors = [BB.green, BB.amber, BB.blueLight, BB.red];
                        const c = p.payload?.cluster ?? 0;
                        return <circle cx={p.cx} cy={p.cy} r={4} fill={colors[c % colors.length]} />;
                      }}
                    />
                  </>
                )}

                <Line type="monotone" dataKey="trainValue" stroke={BB.textSecondary} dot={false} strokeWidth={1.5} />
                <Line type="monotone" dataKey="testActual" stroke={BB.amber} dot={{ fill: BB.amber, r: 2 }} strokeWidth={2} />
                <Line type="monotone" dataKey="forecastLevel" stroke={BB.blueLight} dot={false} strokeWidth={2} strokeDasharray="5 5" />

                {selectedAnalysis === 'forecasting' && (
                  <>
                    <Line type="monotone" dataKey="forecastUpper" stroke={BB.amber} strokeWidth={1} strokeDasharray="3 3" dot={false} connectNulls={false} />
                    <Line type="monotone" dataKey="forecastLower" stroke={BB.amber} strokeWidth={1} strokeDasharray="3 3" dot={false} connectNulls={false} />
                    <Line type="monotone" dataKey="futureForecastUpper" stroke={BB.greenDim} strokeWidth={1} strokeDasharray="3 3" dot={false} connectNulls={false} />
                    <Line type="monotone" dataKey="futureForecastLower" stroke={BB.greenDim} strokeWidth={1} strokeDasharray="3 3" dot={false} connectNulls={false} />
                  </>
                )}

                {selectedAnalysis === 'trend_analysis' && (
                  <>
                    <Line type="monotone" dataKey="upperBand" stroke={BB.amber} strokeWidth={1} strokeDasharray="3 3" dot={false} connectNulls={false} />
                    <Line type="monotone" dataKey="lowerBand" stroke={BB.amber} strokeWidth={1} strokeDasharray="3 3" dot={false} connectNulls={false} />
                    <Line type="monotone" dataKey="projection" stroke={BB.green} strokeWidth={2} dot={{ fill: BB.green, r: 4 }} connectNulls={false} />
                    <Line type="monotone" dataKey="projectionUpper" stroke={BB.greenDim} strokeWidth={1} strokeDasharray="3 3" dot={false} connectNulls={false} />
                    <Line type="monotone" dataKey="projectionLower" stroke={BB.greenDim} strokeWidth={1} strokeDasharray="3 3" dot={false} connectNulls={false} />
                  </>
                )}

                <Line type="monotone" dataKey="fitted" stroke={config?.color || BB.amber} dot={false} strokeWidth={2} strokeDasharray="5 5" />

                {selectedAnalysis === 'arima_model' && (
                  <>
                    <Line type="monotone" dataKey="upperBand" stroke={BB.amber} strokeWidth={1} strokeDasharray="3 3" dot={false} connectNulls={false} />
                    <Line type="monotone" dataKey="lowerBand" stroke={BB.amber} strokeWidth={1} strokeDasharray="3 3" dot={false} connectNulls={false} />
                    <Line type="monotone" dataKey="forecast" stroke={BB.green} strokeWidth={2} dot={{ fill: BB.green, r: 4 }} connectNulls={false} />
                    <Line type="monotone" dataKey="forecastUpper" stroke={BB.greenDim} strokeWidth={1} strokeDasharray="3 3" dot={false} connectNulls={false} />
                    <Line type="monotone" dataKey="forecastLower" stroke={BB.greenDim} strokeWidth={1} strokeDasharray="3 3" dot={false} connectNulls={false} />
                  </>
                )}

                <Line type="monotone" dataKey="forecast" stroke={BB.green} dot={{ fill: BB.green, r: 4 }} strokeWidth={2} />

                {selectedAnalysis === 'supervised_learning' && (
                  <>
                    <Line type="monotone" dataKey="upperBand" stroke={BB.amber} strokeWidth={1} strokeDasharray="3 3" dot={false} connectNulls={false} />
                    <Line type="monotone" dataKey="lowerBand" stroke={BB.amber} strokeWidth={1} strokeDasharray="3 3" dot={false} connectNulls={false} />
                    <Line type="monotone" dataKey="testActual" stroke={BB.green} strokeWidth={2} dot={{ fill: BB.green, r: 3 }} connectNulls={false} />
                    <Line type="monotone" dataKey="prediction" stroke={BB.blueLight} dot={{ fill: BB.blueLight, r: 3 }} strokeWidth={2} connectNulls={false} />
                  </>
                )}

                {!resultsChartZoom && (
                  <Brush
                    dataKey="index"
                    height={20}
                    stroke={BB.amber}
                    fill={BB.cardBg}
                    onChange={(e) => {
                      if (e.startIndex !== undefined && e.endIndex !== undefined && e.startIndex !== e.endIndex) {
                        setResultsChartZoom({ startIndex: e.startIndex, endIndex: e.endIndex });
                      }
                    }}
                  />
                )}

                {isSelecting && activeChart === 'results' && refAreaLeft !== null && refAreaRight !== null && (
                  <ReferenceArea x1={refAreaLeft} x2={refAreaRight} strokeOpacity={0.3} fill={BB.amber} fillOpacity={0.2} />
                )}
              </ComposedChart>
            </ResponsiveContainer>
          </div>

          {/* Legend */}
          <div className="flex items-center justify-between mt-2 px-2">
            <div className="flex items-center gap-4 flex-wrap">
              <ChartLegend selectedAnalysis={selectedAnalysis} />
            </div>
            <div className="text-xs font-mono" style={{ color: BB.textMuted }}>
              Scroll to pan | Ctrl+Scroll to zoom | Drag to select
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}
