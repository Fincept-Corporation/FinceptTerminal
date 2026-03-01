import React from 'react';
import { Scatter, ScatterChart, XAxis, YAxis, CartesianGrid, Tooltip, ResponsiveContainer, ZAxis } from 'recharts';
import { FINCEPT, TYPOGRAPHY, SPACING, BORDERS, COMMON_STYLES, EFFECTS } from '../../../finceptStyles';
import { OptimizationConfig, OptimizationResults, SKFOLIO_RISK_MEASURES } from '../types';

interface FrontierTabProps {
  library: 'pyportfolioopt' | 'skfolio';
  config: OptimizationConfig;
  setConfig: React.Dispatch<React.SetStateAction<OptimizationConfig>>;
  skfolioConfig: { risk_measure: string; [key: string]: any };
  setSkfolioConfig: React.Dispatch<React.SetStateAction<any>>;
  results: OptimizationResults | null;
  loadingAction: string | null;
  handleGenerateFrontier: () => void;
}

export const FrontierTab: React.FC<FrontierTabProps> = ({
  library,
  config,
  setConfig,
  skfolioConfig,
  setSkfolioConfig,
  results,
  loadingAction,
  handleGenerateFrontier,
}) => {
  return (
    <div>
      {/* Configuration Panel */}
      <div style={{ backgroundColor: FINCEPT.PANEL_BG, padding: SPACING.LARGE, border: BORDERS.STANDARD, borderRadius: '2px', marginBottom: SPACING.LARGE }}>
        <div style={{ color: FINCEPT.CYAN, fontSize: TYPOGRAPHY.BODY, fontWeight: TYPOGRAPHY.BOLD, marginBottom: SPACING.DEFAULT }}>
          FRONTIER PARAMETERS
        </div>

        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.DEFAULT }}>
          {/* Number of portfolios */}
          <div>
            <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: SPACING.SMALL }}>
              NUMBER OF POINTS
            </label>
            <input
              type="text"
              inputMode="numeric"
              value={config.total_portfolio_value}
              onChange={(e) => { const v = e.target.value; if (v === '' || /^\d+$/.test(v)) setConfig({ ...config, total_portfolio_value: parseInt(v) || 100 }); }}
              style={{
                ...COMMON_STYLES.inputField,
                transition: EFFECTS.TRANSITION_STANDARD,
              }}
            />
          </div>

          {/* Risk Measure for skfolio */}
          {library === 'skfolio' && (
            <div>
              <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: SPACING.SMALL }}>
                RISK MEASURE
              </label>
              <select
                value={skfolioConfig.risk_measure}
                onChange={(e) => setSkfolioConfig({ ...skfolioConfig, risk_measure: e.target.value })}
                style={{
                  ...COMMON_STYLES.inputField,
                  transition: EFFECTS.TRANSITION_STANDARD,
                }}
              >
                {SKFOLIO_RISK_MEASURES.map(rm => (
                  <option key={rm.value} value={rm.value}>{rm.label}</option>
                ))}
              </select>
            </div>
          )}
        </div>
      </div>

      <div style={{ display: 'flex', alignItems: 'center', gap: SPACING.DEFAULT, marginBottom: SPACING.XLARGE }}>
        <button
          onClick={handleGenerateFrontier}
          disabled={loadingAction === 'frontier'}
          style={{
            ...COMMON_STYLES.buttonPrimary,
            padding: `${SPACING.DEFAULT} ${SPACING.XLARGE}`,
            backgroundColor: loadingAction === 'frontier' ? FINCEPT.MUTED : FINCEPT.ORANGE,
            color: loadingAction === 'frontier' ? FINCEPT.GRAY : FINCEPT.DARK_BG,
            border: loadingAction === 'frontier' ? `1px solid ${FINCEPT.GRAY}` : 'none',
            cursor: loadingAction === 'frontier' ? 'not-allowed' : 'pointer',
            opacity: loadingAction === 'frontier' ? 0.6 : 1,
          }}
        >
          {loadingAction === 'frontier' ? 'GENERATING...' : 'GENERATE EFFICIENT FRONTIER'}
        </button>
        {loadingAction === 'frontier' && (
          <div style={{ display: 'flex', alignItems: 'center', gap: SPACING.MEDIUM }}>
            <div style={{
              width: '16px',
              height: '16px',
              border: `2px solid ${FINCEPT.BORDER}`,
              borderTop: `2px solid ${FINCEPT.CYAN}`,
              borderRadius: '50%',
              animation: 'spin 0.8s linear infinite'
            }} />
            <span style={{ color: FINCEPT.CYAN, fontSize: TYPOGRAPHY.BODY }}>Generating frontier...</span>
            <style>{`
              @keyframes spin {
                0% { transform: rotate(0deg); }
                100% { transform: rotate(360deg); }
              }
            `}</style>
          </div>
        )}
      </div>

      {results?.efficient_frontier && results.efficient_frontier.returns && (
        <div style={{ backgroundColor: FINCEPT.PANEL_BG, padding: SPACING.XLARGE, border: BORDERS.STANDARD, borderRadius: '2px', marginTop: SPACING.LARGE }}>
          <div style={{ color: FINCEPT.CYAN, fontSize: TYPOGRAPHY.SUBHEADING, fontWeight: TYPOGRAPHY.BOLD, marginBottom: SPACING.LARGE }}>
            EFFICIENT FRONTIER ({results.efficient_frontier.returns?.length || 0} points)
          </div>

          {/* Prepare chart data */}
          {(() => {
            const chartData = results.efficient_frontier!.returns.map((ret: number, idx: number) => ({
              volatility: parseFloat((results.efficient_frontier!.volatility[idx] * 100).toFixed(2)),
              returns: parseFloat((ret * 100).toFixed(2)),
              sharpe: parseFloat(results.efficient_frontier!.sharpe_ratios?.[idx]?.toFixed(3) || '0'),
            }));

            // Custom dot component for better visualization
            const CustomDot = (props: any) => {
              const { cx, cy, payload } = props;
              const sharpe = payload.sharpe || 0;

              // Color gradient based on Sharpe ratio (green for high, yellow for medium, red for low)
              let fillColor: string = FINCEPT.RED; // Red for low Sharpe
              if (sharpe > 1.5) fillColor = FINCEPT.GREEN; // Green for high Sharpe
              else if (sharpe > 0.8) fillColor = FINCEPT.YELLOW; // Gold for medium Sharpe
              else if (sharpe > 0.3) fillColor = FINCEPT.ORANGE; // Orange for okay Sharpe

              return (
                <circle
                  cx={cx}
                  cy={cy}
                  r={1.5}
                  fill={fillColor}
                  fillOpacity={0.8}
                  stroke={fillColor}
                  strokeWidth={0.5}
                />
              );
            };

            return (
              <ResponsiveContainer width="100%" height={450}>
                <ScatterChart margin={{ top: 20, right: 30, bottom: 40, left: 60 }}>
                  <defs>
                    <linearGradient id="frontierGradient" x1="0" y1="0" x2="1" y2="1">
                      <stop offset="0%" stopColor={FINCEPT.RED} stopOpacity={0.8} />
                      <stop offset="50%" stopColor={FINCEPT.YELLOW} stopOpacity={0.8} />
                      <stop offset="100%" stopColor={FINCEPT.GREEN} stopOpacity={0.8} />
                    </linearGradient>
                  </defs>
                  <CartesianGrid strokeDasharray="3 3" stroke="#333" strokeOpacity={0.3} />
                  <XAxis
                    type="number"
                    dataKey="volatility"
                    name="Volatility"
                    unit="%"
                    stroke={FINCEPT.GRAY}
                    tick={{ fill: FINCEPT.GRAY, fontSize: 10 }}
                    label={{ value: 'Annual Volatility (%)', position: 'insideBottom', offset: -15, fill: FINCEPT.GRAY, fontSize: 11 }}
                  />
                  <YAxis
                    type="number"
                    dataKey="returns"
                    name="Returns"
                    unit="%"
                    stroke={FINCEPT.GRAY}
                    tick={{ fill: FINCEPT.GRAY, fontSize: 10 }}
                    label={{ value: 'Annual Returns (%)', angle: -90, position: 'insideLeft', offset: -40, fill: FINCEPT.GRAY, fontSize: 11 }}
                  />
                  <Tooltip
                    contentStyle={{
                      backgroundColor: FINCEPT.HEADER_BG,
                      border: `1px solid ${FINCEPT.CYAN}`,
                      borderRadius: '2px',
                      fontSize: TYPOGRAPHY.SMALL,
                      padding: SPACING.MEDIUM
                    }}
                    labelStyle={{ color: FINCEPT.CYAN }}
                    itemStyle={{ color: FINCEPT.WHITE }}
                    formatter={(value: any, name: string) => {
                      if (name === 'Volatility') return [`${value}%`, 'Volatility'];
                      if (name === 'Returns') return [`${value}%`, 'Returns'];
                      if (name === 'sharpe') return [value, 'Sharpe Ratio'];
                      return [value, name];
                    }}
                  />
                  <Scatter
                    name="Efficient Frontier"
                    data={chartData}
                    fill="url(#frontierGradient)"
                    line={{ stroke: FINCEPT.CYAN, strokeWidth: 1, strokeOpacity: 0.5 }}
                    shape={<CustomDot />}
                    isAnimationActive={false}
                  />
                </ScatterChart>
              </ResponsiveContainer>
            );
          })()}

          {/* Legend */}
          <div style={{ marginTop: SPACING.LARGE, fontSize: TYPOGRAPHY.SMALL, color: FINCEPT.GRAY, textAlign: 'center' }}>
            Each point represents an optimal portfolio. Color indicates Sharpe ratio:
            <span style={{ color: FINCEPT.GREEN, marginLeft: SPACING.MEDIUM }}>● High {'>1.5'}</span>
            <span style={{ color: FINCEPT.YELLOW, marginLeft: SPACING.MEDIUM }}>● Good (0.8-1.5)</span>
            <span style={{ color: FINCEPT.ORANGE, marginLeft: SPACING.MEDIUM }}>● Medium (0.3-0.8)</span>
            <span style={{ color: FINCEPT.RED, marginLeft: SPACING.MEDIUM }}>● Low {'<0.3'}</span>
          </div>
        </div>
      )}
    </div>
  );
};
