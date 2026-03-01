import React from 'react';
import { LineChart as RechartsLine, Line, XAxis, YAxis, CartesianGrid, Tooltip, ResponsiveContainer } from 'recharts';
import { FINCEPT, TYPOGRAPHY, SPACING, BORDERS, COMMON_STYLES } from '../../../finceptStyles';

interface RiskDecomp {
  marginal_contribution: Record<string, number>;
  component_contribution: Record<string, number>;
  percentage_contribution: Record<string, number>;
  portfolio_volatility: number;
}

interface SensitivityResults {
  parameter: string;
  results: Array<Record<string, number>>;
}

interface RiskTabProps {
  riskDecomp: RiskDecomp | null;
  sensitivityResults: SensitivityResults | null;
  sensitivityParam: string;
  setSensitivityParam: (p: string) => void;
  loadingAction: string | null;
  handleRiskDecomposition: () => void;
  handleSensitivityAnalysis: () => void;
}

export const RiskTab: React.FC<RiskTabProps> = ({
  riskDecomp,
  sensitivityResults,
  sensitivityParam,
  setSensitivityParam,
  loadingAction,
  handleRiskDecomposition,
  handleSensitivityAnalysis,
}) => {
  return (
    <div>
      {/* Action Buttons */}
      <div style={{ display: 'flex', gap: SPACING.DEFAULT, marginBottom: SPACING.XLARGE }}>
        <button
          onClick={handleRiskDecomposition}
          disabled={loadingAction === 'risk'}
          style={{
            ...COMMON_STYLES.buttonPrimary,
            padding: `${SPACING.DEFAULT} ${SPACING.XLARGE}`,
            backgroundColor: loadingAction === 'risk' ? FINCEPT.MUTED : FINCEPT.ORANGE,
            color: loadingAction === 'risk' ? FINCEPT.GRAY : FINCEPT.DARK_BG,
            cursor: loadingAction === 'risk' ? 'not-allowed' : 'pointer',
            opacity: loadingAction === 'risk' ? 0.6 : 1,
          }}
        >
          {loadingAction === 'risk' ? 'ANALYZING...' : 'RUN RISK DECOMPOSITION'}
        </button>
        <div style={{ display: 'flex', alignItems: 'center', gap: SPACING.SMALL }}>
          <select
            value={sensitivityParam}
            onChange={(e) => setSensitivityParam(e.target.value)}
            style={{ ...COMMON_STYLES.inputField, width: '180px' }}
          >
            <option value="risk_free_rate">Risk-Free Rate</option>
            <option value="gamma">L2 Regularization</option>
            <option value="risk_aversion">Risk Aversion</option>
          </select>
          <button
            onClick={handleSensitivityAnalysis}
            disabled={loadingAction === 'sensitivity'}
            style={{
              ...COMMON_STYLES.buttonSecondary,
              padding: `${SPACING.DEFAULT} ${SPACING.LARGE}`,
              color: loadingAction === 'sensitivity' ? FINCEPT.GRAY : FINCEPT.CYAN,
              borderColor: loadingAction === 'sensitivity' ? FINCEPT.GRAY : FINCEPT.CYAN,
              cursor: loadingAction === 'sensitivity' ? 'not-allowed' : 'pointer',
            }}
          >
            {loadingAction === 'sensitivity' ? 'RUNNING...' : 'SENSITIVITY ANALYSIS'}
          </button>
        </div>
      </div>

      {/* Risk Decomposition Results */}
      {riskDecomp && (
        <div style={{ marginBottom: SPACING.XLARGE }}>
          <div style={{ ...COMMON_STYLES.sectionHeader, borderBottom: `1px solid ${FINCEPT.ORANGE}`, paddingBottom: SPACING.MEDIUM, marginBottom: SPACING.LARGE }}>
            RISK DECOMPOSITION
          </div>
          <div style={{ display: 'grid', gridTemplateColumns: '1fr', gap: SPACING.DEFAULT, marginBottom: SPACING.LARGE }}>
            <div style={{ ...COMMON_STYLES.metricCard, padding: SPACING.LARGE }}>
              <div style={{ ...COMMON_STYLES.dataLabel }}>Portfolio Volatility</div>
              <div style={{ color: FINCEPT.YELLOW, fontSize: '18px', fontWeight: TYPOGRAPHY.BOLD }}>
                {(riskDecomp.portfolio_volatility * 100).toFixed(2)}%
              </div>
            </div>
          </div>

          {/* Risk contribution table */}
          <div style={{ backgroundColor: FINCEPT.PANEL_BG, border: BORDERS.STANDARD, borderRadius: '2px', overflow: 'hidden' }}>
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr 1fr', ...COMMON_STYLES.tableHeader }}>
              <span>Asset</span>
              <span>Marginal</span>
              <span>Component</span>
              <span>% Contribution</span>
            </div>
            {Object.keys(riskDecomp.percentage_contribution)
              .sort((a, b) => (riskDecomp.percentage_contribution[b] || 0) - (riskDecomp.percentage_contribution[a] || 0))
              .map((asset, idx) => (
                <div
                  key={asset}
                  style={{
                    display: 'grid',
                    gridTemplateColumns: '1fr 1fr 1fr 1fr',
                    padding: `${SPACING.MEDIUM} ${SPACING.DEFAULT}`,
                    borderBottom: BORDERS.STANDARD,
                    fontSize: TYPOGRAPHY.SMALL,
                    backgroundColor: idx % 2 === 0 ? 'transparent' : 'rgba(255,255,255,0.02)',
                  }}
                >
                  <span style={{ color: FINCEPT.CYAN, fontWeight: TYPOGRAPHY.BOLD }}>{asset}</span>
                  <span style={{ color: FINCEPT.WHITE }}>{(riskDecomp.marginal_contribution[asset] * 100).toFixed(4)}%</span>
                  <span style={{ color: FINCEPT.WHITE }}>{(riskDecomp.component_contribution[asset] * 100).toFixed(4)}%</span>
                  <span style={{ color: FINCEPT.ORANGE, fontWeight: TYPOGRAPHY.BOLD }}>
                    {(riskDecomp.percentage_contribution[asset] * 100).toFixed(2)}%
                  </span>
                </div>
              ))}
          </div>
        </div>
      )}

      {/* Sensitivity Analysis Results */}
      {sensitivityResults && (
        <div>
          <div style={{ ...COMMON_STYLES.sectionHeader, borderBottom: `1px solid ${FINCEPT.CYAN}`, paddingBottom: SPACING.MEDIUM, marginBottom: SPACING.LARGE }}>
            SENSITIVITY ANALYSIS: {sensitivityResults.parameter.toUpperCase().replace(/_/g, ' ')}
          </div>

          {/* Chart */}
          {sensitivityResults.results.length > 0 && (
            <div style={{ backgroundColor: FINCEPT.PANEL_BG, padding: SPACING.LARGE, border: BORDERS.STANDARD, borderRadius: '2px', marginBottom: SPACING.LARGE }}>
              <ResponsiveContainer width="100%" height={300}>
                <RechartsLine data={sensitivityResults.results}>
                  <CartesianGrid strokeDasharray="3 3" stroke="#333" strokeOpacity={0.3} />
                  <XAxis dataKey={sensitivityResults.parameter} stroke={FINCEPT.GRAY} tick={{ fill: FINCEPT.GRAY, fontSize: 10 }} />
                  <YAxis stroke={FINCEPT.GRAY} tick={{ fill: FINCEPT.GRAY, fontSize: 10 }} />
                  <Tooltip
                    contentStyle={{ backgroundColor: FINCEPT.HEADER_BG, border: `1px solid ${FINCEPT.CYAN}`, borderRadius: '2px', fontSize: TYPOGRAPHY.SMALL }}
                    labelStyle={{ color: FINCEPT.CYAN }}
                  />
                  <Line type="monotone" dataKey="expected_return" stroke={FINCEPT.GREEN} dot={false} name="Expected Return" />
                  <Line type="monotone" dataKey="volatility" stroke={FINCEPT.YELLOW} dot={false} name="Volatility" />
                  <Line type="monotone" dataKey="sharpe_ratio" stroke={FINCEPT.ORANGE} dot={false} name="Sharpe Ratio" />
                </RechartsLine>
              </ResponsiveContainer>
            </div>
          )}

          {/* Table */}
          <div style={{ backgroundColor: FINCEPT.PANEL_BG, border: BORDERS.STANDARD, borderRadius: '2px', overflow: 'auto', maxHeight: '300px' }}>
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr 1fr', ...COMMON_STYLES.tableHeader }}>
              <span>{sensitivityResults.parameter.replace(/_/g, ' ')}</span>
              <span>Expected Return</span>
              <span>Volatility</span>
              <span>Sharpe Ratio</span>
            </div>
            {sensitivityResults.results.map((row, idx) => (
              <div
                key={idx}
                style={{
                  display: 'grid',
                  gridTemplateColumns: '1fr 1fr 1fr 1fr',
                  padding: `${SPACING.MEDIUM} ${SPACING.DEFAULT}`,
                  borderBottom: BORDERS.STANDARD,
                  fontSize: TYPOGRAPHY.SMALL,
                  backgroundColor: idx % 2 === 0 ? 'transparent' : 'rgba(255,255,255,0.02)',
                }}
              >
                <span style={{ color: FINCEPT.CYAN }}>{(row[sensitivityResults.parameter] ?? 0).toFixed(4)}</span>
                <span style={{ color: FINCEPT.GREEN }}>{((row.expected_return ?? 0) * 100).toFixed(2)}%</span>
                <span style={{ color: FINCEPT.YELLOW }}>{((row.volatility ?? 0) * 100).toFixed(2)}%</span>
                <span style={{ color: FINCEPT.ORANGE }}>{(row.sharpe_ratio ?? 0).toFixed(3)}</span>
              </div>
            ))}
          </div>
        </div>
      )}

      {!riskDecomp && !sensitivityResults && (
        <div style={{ padding: '48px', textAlign: 'center', color: FINCEPT.GRAY, fontSize: TYPOGRAPHY.BODY, backgroundColor: FINCEPT.PANEL_BG, border: BORDERS.STANDARD, borderRadius: '2px' }}>
          Run risk decomposition or sensitivity analysis to see results. Run optimization first if you haven't already.
        </div>
      )}
    </div>
  );
};
