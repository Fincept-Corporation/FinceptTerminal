import React from 'react';
import { FINCEPT, TYPOGRAPHY, SPACING, BORDERS, COMMON_STYLES } from '../../../finceptStyles';
import { OptimizationResults } from '../types';

interface BacktestTabProps {
  results: OptimizationResults | null;
  loadingAction: string | null;
  handleBacktest: () => void;
}

export const BacktestTab: React.FC<BacktestTabProps> = ({
  results,
  loadingAction,
  handleBacktest,
}) => {
  return (
    <div>
      <div style={{ display: 'flex', alignItems: 'center', gap: SPACING.DEFAULT, marginBottom: SPACING.XLARGE }}>
        <button
          onClick={handleBacktest}
          disabled={loadingAction === 'backtest'}
          style={{
            ...COMMON_STYLES.buttonPrimary,
            padding: `${SPACING.DEFAULT} ${SPACING.XLARGE}`,
            backgroundColor: loadingAction === 'backtest' ? FINCEPT.MUTED : FINCEPT.ORANGE,
            color: loadingAction === 'backtest' ? FINCEPT.GRAY : FINCEPT.DARK_BG,
            border: loadingAction === 'backtest' ? `1px solid ${FINCEPT.GRAY}` : 'none',
            cursor: loadingAction === 'backtest' ? 'not-allowed' : 'pointer',
            opacity: loadingAction === 'backtest' ? 0.6 : 1,
          }}
        >
          {loadingAction === 'backtest' ? 'RUNNING BACKTEST...' : 'RUN BACKTEST'}
        </button>
        {loadingAction === 'backtest' && (
          <div style={{ display: 'flex', alignItems: 'center', gap: SPACING.MEDIUM }}>
            <div style={{
              width: '16px',
              height: '16px',
              border: `2px solid ${FINCEPT.BORDER}`,
              borderTop: `2px solid ${FINCEPT.CYAN}`,
              borderRadius: '50%',
              animation: 'spin 0.8s linear infinite'
            }} />
            <span style={{ color: FINCEPT.CYAN, fontSize: TYPOGRAPHY.BODY }}>Running backtest...</span>
            <style>{`
              @keyframes spin {
                0% { transform: rotate(0deg); }
                100% { transform: rotate(360deg); }
              }
            `}</style>
          </div>
        )}
      </div>

      {results?.backtest_results && (
        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(5, 1fr)', gap: SPACING.DEFAULT }}>
          {[
            { label: 'Annual Return', value: results.backtest_results.annual_return, color: FINCEPT.GREEN, format: '%' },
            { label: 'Annual Volatility', value: results.backtest_results.annual_volatility, color: FINCEPT.YELLOW, format: '%' },
            { label: 'Sharpe Ratio', value: results.backtest_results.sharpe_ratio, color: FINCEPT.ORANGE, format: '' },
            { label: 'Max Drawdown', value: results.backtest_results.max_drawdown, color: FINCEPT.RED, format: '%' },
            { label: 'Calmar Ratio', value: results.backtest_results.calmar_ratio, color: FINCEPT.CYAN, format: '' },
          ].map(metric => (
            <div key={metric.label} style={{ ...COMMON_STYLES.metricCard, padding: SPACING.LARGE }}>
              <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.SMALL }}>{metric.label}</div>
              <div style={{ color: metric.color, fontSize: '18px', fontWeight: TYPOGRAPHY.BOLD }}>
                {metric.format === '%'
                  ? `${(metric.value * 100).toFixed(2)}%`
                  : metric.value.toFixed(3)
                }
              </div>
            </div>
          ))}
        </div>
      )}
    </div>
  );
};
