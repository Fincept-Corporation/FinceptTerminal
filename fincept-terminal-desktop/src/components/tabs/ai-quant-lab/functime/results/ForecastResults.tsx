/**
 * ForecastResults Component
 * Displays forecast analysis results including summary, metrics, chart, and table
 */

import React, { useState } from 'react';
import {
  Database,
  Layers,
  GitBranch,
  Target,
  BarChart2,
  Activity,
  TrendingUp,
  LineChart,
  ChevronUp,
  ChevronDown,
  CheckCircle2,
  Zap
} from 'lucide-react';
import { FINCEPT, INITIAL_ROWS, EXPANDED_ROWS } from '../constants';
import { formatMetric } from '../utils';
import { MetricCard, ForecastChart } from '../components';
import type { ForecastAnalysisResult, ExpandedSections } from '../types';

interface ForecastResultsProps {
  analysisResult: ForecastAnalysisResult;
  expandedSections: ExpandedSections;
  toggleSection: (section: keyof ExpandedSections) => void;
  historicalData: { time: string; value: number }[];
}

export const ForecastResults: React.FC<ForecastResultsProps> = ({
  analysisResult,
  expandedSections,
  toggleSection,
  historicalData
}) => {
  const [forecastTableExpanded, setForecastTableExpanded] = useState(false);

  return (
    <div className="space-y-4">
      {/* Data Summary Section */}
      {analysisResult.data_summary && (
        <div
          className="rounded overflow-hidden"
          style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}` }}
        >
          <button
            onClick={() => toggleSection('summary')}
            className="w-full px-4 py-3 flex items-center justify-between"
            style={{ backgroundColor: FINCEPT.HEADER_BG }}
          >
            <div className="flex items-center gap-2">
              <Database size={16} color={FINCEPT.CYAN} />
              <span className="text-xs font-bold uppercase tracking-wide" style={{ color: FINCEPT.WHITE }}>
                Data Summary
              </span>
            </div>
            {expandedSections.summary ? (
              <ChevronUp size={16} color={FINCEPT.GRAY} />
            ) : (
              <ChevronDown size={16} color={FINCEPT.GRAY} />
            )}
          </button>

          {expandedSections.summary && (
            <div className="p-4 grid grid-cols-4 gap-4">
              <MetricCard
                label="Total Rows"
                value={String(analysisResult.data_summary.total_rows)}
                color={FINCEPT.WHITE}
                icon={<Database size={14} />}
              />
              <MetricCard
                label="Entities"
                value={String(analysisResult.data_summary.entities)}
                color={FINCEPT.CYAN}
                icon={<Layers size={14} />}
              />
              <MetricCard
                label="Train Size"
                value={String(analysisResult.data_summary.train_size)}
                color={FINCEPT.GREEN}
                icon={<GitBranch size={14} />}
              />
              <MetricCard
                label="Test Size"
                value={String(analysisResult.data_summary.test_size)}
                color={FINCEPT.YELLOW}
                icon={<Target size={14} />}
              />
            </div>
          )}
        </div>
      )}

      {/* Evaluation Metrics Section */}
      {analysisResult.evaluation_metrics && (
        <div
          className="rounded overflow-hidden"
          style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}` }}
        >
          <button
            onClick={() => toggleSection('metrics')}
            className="w-full px-4 py-3 flex items-center justify-between"
            style={{ backgroundColor: FINCEPT.HEADER_BG }}
          >
            <div className="flex items-center gap-2">
              <Target size={16} color={FINCEPT.GREEN} />
              <span className="text-xs font-bold uppercase tracking-wide" style={{ color: FINCEPT.WHITE }}>
                Evaluation Metrics
              </span>
            </div>
            {expandedSections.metrics ? (
              <ChevronUp size={16} color={FINCEPT.GRAY} />
            ) : (
              <ChevronDown size={16} color={FINCEPT.GRAY} />
            )}
          </button>

          {expandedSections.metrics && (
            <div className="p-4 grid grid-cols-3 gap-4">
              <MetricCard
                label="MAE"
                value={formatMetric(analysisResult.evaluation_metrics.mae)}
                color={FINCEPT.CYAN}
                icon={<BarChart2 size={14} />}
              />
              <MetricCard
                label="RMSE"
                value={formatMetric(analysisResult.evaluation_metrics.rmse)}
                color={FINCEPT.YELLOW}
                icon={<Activity size={14} />}
              />
              <MetricCard
                label="SMAPE"
                value={formatMetric(analysisResult.evaluation_metrics.smape)}
                color={FINCEPT.PURPLE}
                icon={<TrendingUp size={14} />}
              />
            </div>
          )}
        </div>
      )}

      {/* Forecast Chart Visualization */}
      {analysisResult.forecast && analysisResult.forecast.length > 0 && (
        <div
          className="rounded overflow-hidden"
          style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}` }}
        >
          <button
            onClick={() => toggleSection('chart')}
            className="w-full px-4 py-3 flex items-center justify-between"
            style={{ backgroundColor: FINCEPT.HEADER_BG }}
          >
            <div className="flex items-center gap-2">
              <LineChart size={16} color={FINCEPT.CYAN} />
              <span className="text-xs font-bold uppercase tracking-wide" style={{ color: FINCEPT.WHITE }}>
                Forecast Chart
              </span>
            </div>
            {expandedSections.chart ? (
              <ChevronUp size={16} color={FINCEPT.GRAY} />
            ) : (
              <ChevronDown size={16} color={FINCEPT.GRAY} />
            )}
          </button>

          {expandedSections.chart && (
            <div className="p-4">
              <ForecastChart
                historicalData={historicalData}
                forecastData={analysisResult.forecast}
                height={350}
              />
            </div>
          )}
        </div>
      )}

      {/* Forecast Results Section */}
      <div
        className="rounded overflow-hidden"
        style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}` }}
      >
        <button
          onClick={() => toggleSection('forecast')}
          className="w-full px-4 py-3 flex items-center justify-between"
          style={{ backgroundColor: FINCEPT.HEADER_BG }}
        >
          <div className="flex items-center gap-2">
            <TrendingUp size={16} color={FINCEPT.ORANGE} />
            <span className="text-xs font-bold uppercase tracking-wide" style={{ color: FINCEPT.WHITE }}>
              Forecast Results
            </span>
            {analysisResult.model && (
              <span
                className="ml-2 px-2 py-0.5 rounded text-xs font-mono"
                style={{ backgroundColor: FINCEPT.ORANGE, color: FINCEPT.DARK_BG }}
              >
                {analysisResult.model.toUpperCase()}
              </span>
            )}
          </div>
          {expandedSections.forecast ? (
            <ChevronUp size={16} color={FINCEPT.GRAY} />
          ) : (
            <ChevronDown size={16} color={FINCEPT.GRAY} />
          )}
        </button>

        {expandedSections.forecast && analysisResult.forecast && (
          <div className="p-4">
            {/* Forecast info */}
            <div className="mb-4 flex gap-4 text-xs font-mono" style={{ color: FINCEPT.GRAY }}>
              <span>Horizon: <span style={{ color: FINCEPT.WHITE }}>{analysisResult.horizon}</span></span>
              <span>Frequency: <span style={{ color: FINCEPT.WHITE }}>{analysisResult.frequency}</span></span>
              {analysisResult.best_params && (
                <span>Auto-tuned: <CheckCircle2 size={12} color={FINCEPT.GREEN} className="inline ml-1" /></span>
              )}
            </div>

            {/* Forecast table */}
            <div
              className="rounded overflow-hidden"
              style={{ border: `1px solid ${FINCEPT.BORDER}` }}
            >
              <table className="w-full text-xs font-mono">
                <thead>
                  <tr style={{ backgroundColor: FINCEPT.HEADER_BG }}>
                    <th className="px-3 py-2 text-left" style={{ color: FINCEPT.GRAY }}>ENTITY</th>
                    <th className="px-3 py-2 text-left" style={{ color: FINCEPT.GRAY }}>TIME</th>
                    <th className="px-3 py-2 text-right" style={{ color: FINCEPT.GRAY }}>FORECAST</th>
                  </tr>
                </thead>
                <tbody>
                  {analysisResult.forecast.slice(0, forecastTableExpanded ? EXPANDED_ROWS : INITIAL_ROWS).map((row, idx) => (
                    <tr
                      key={idx}
                      style={{
                        backgroundColor: idx % 2 === 0 ? FINCEPT.DARK_BG : FINCEPT.PANEL_BG
                      }}
                    >
                      <td className="px-3 py-2" style={{ color: FINCEPT.CYAN }}>
                        {row.entity_id}
                      </td>
                      <td className="px-3 py-2" style={{ color: FINCEPT.WHITE }}>
                        {row.time}
                      </td>
                      <td className="px-3 py-2 text-right" style={{ color: FINCEPT.GREEN }}>
                        {typeof row.value === 'number' ? row.value.toFixed(2) : row.value}
                      </td>
                    </tr>
                  ))}
                </tbody>
              </table>
              {analysisResult.forecast.length > INITIAL_ROWS && (
                <button
                  onClick={() => setForecastTableExpanded(!forecastTableExpanded)}
                  className="w-full px-3 py-2 text-xs font-mono text-center flex items-center justify-center gap-2 hover:opacity-80 transition-opacity"
                  style={{ backgroundColor: FINCEPT.HEADER_BG, color: FINCEPT.ORANGE }}
                >
                  {forecastTableExpanded ? (
                    <>
                      <ChevronUp size={12} />
                      Show Less ({INITIAL_ROWS} rows)
                    </>
                  ) : (
                    <>
                      <ChevronDown size={12} />
                      Show More ({Math.min(analysisResult.forecast.length, EXPANDED_ROWS)} of {analysisResult.forecast.length} rows)
                    </>
                  )}
                </button>
              )}
            </div>
          </div>
        )}
      </div>

      {/* Best Params (if auto-tuned) */}
      {analysisResult.best_params && Object.keys(analysisResult.best_params).length > 0 && (
        <div
          className="rounded p-4"
          style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}` }}
        >
          <div className="flex items-center gap-2 mb-3">
            <Zap size={16} color={FINCEPT.YELLOW} />
            <span className="text-xs font-bold uppercase" style={{ color: FINCEPT.WHITE }}>
              Best Parameters (Auto-tuned)
            </span>
          </div>
          <div className="grid grid-cols-3 gap-2">
            {Object.entries(analysisResult.best_params).map(([key, value]) => (
              <div key={key} className="flex justify-between p-2 rounded" style={{ backgroundColor: FINCEPT.DARK_BG }}>
                <span className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>{key}</span>
                <span className="text-xs font-mono" style={{ color: FINCEPT.CYAN }}>
                  {typeof value === 'number' ? value.toFixed(4) : String(value)}
                </span>
              </div>
            ))}
          </div>
        </div>
      )}
    </div>
  );
};
