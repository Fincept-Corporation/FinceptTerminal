import React from 'react';
import { FINCEPT, TYPOGRAPHY, SPACING, BORDERS, COMMON_STYLES } from '../../../finceptStyles';
import { OptimizationResults } from '../types';

interface AllocationTabProps {
  results: OptimizationResults | null;
  loadingAction: string | null;
  handleDiscreteAllocation: () => void;
}

export const AllocationTab: React.FC<AllocationTabProps> = ({
  results,
  loadingAction,
  handleDiscreteAllocation,
}) => {
  return (
    <div>
      <div style={{ display: 'flex', alignItems: 'center', gap: SPACING.DEFAULT, marginBottom: SPACING.XLARGE }}>
        <button
          onClick={handleDiscreteAllocation}
          disabled={loadingAction === 'allocation' || !results?.weights}
          style={{
            ...COMMON_STYLES.buttonPrimary,
            padding: `${SPACING.DEFAULT} ${SPACING.XLARGE}`,
            backgroundColor: (loadingAction === 'allocation' || !results?.weights) ? FINCEPT.MUTED : FINCEPT.ORANGE,
            color: (loadingAction === 'allocation' || !results?.weights) ? FINCEPT.GRAY : FINCEPT.DARK_BG,
            border: (loadingAction === 'allocation' || !results?.weights) ? `1px solid ${FINCEPT.GRAY}` : 'none',
            cursor: (loadingAction === 'allocation' || !results?.weights) ? 'not-allowed' : 'pointer',
            opacity: (loadingAction === 'allocation' || !results?.weights) ? 0.6 : 1,
          }}
        >
          {loadingAction === 'allocation' ? 'CALCULATING...' : 'CALCULATE DISCRETE ALLOCATION'}
        </button>
        {loadingAction === 'allocation' && (
          <div style={{ display: 'flex', alignItems: 'center', gap: SPACING.MEDIUM }}>
            <div style={{
              width: '16px',
              height: '16px',
              border: `2px solid ${FINCEPT.BORDER}`,
              borderTop: `2px solid ${FINCEPT.CYAN}`,
              borderRadius: '50%',
              animation: 'spin 0.8s linear infinite'
            }} />
            <span style={{ color: FINCEPT.CYAN, fontSize: TYPOGRAPHY.BODY }}>Calculating allocation...</span>
            <style>{`
              @keyframes spin {
                0% { transform: rotate(0deg); }
                100% { transform: rotate(360deg); }
              }
            `}</style>
          </div>
        )}
      </div>

      {!results?.weights && (
        <div style={{
          padding: SPACING.LARGE,
          backgroundColor: `${FINCEPT.ORANGE}1A`,
          border: BORDERS.ORANGE,
          color: FINCEPT.ORANGE,
          marginBottom: SPACING.XLARGE,
          fontSize: TYPOGRAPHY.BODY,
          borderRadius: '2px'
        }}>
          Please run optimization first to get portfolio weights
        </div>
      )}

      {results?.discrete_allocation && (
        <div>
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: SPACING.DEFAULT, marginBottom: SPACING.XLARGE }}>
            <div style={{ ...COMMON_STYLES.metricCard, padding: SPACING.LARGE }}>
              <div style={{ ...COMMON_STYLES.dataLabel }}>Total Value</div>
              <div style={{ color: FINCEPT.CYAN, fontSize: '18px', fontWeight: TYPOGRAPHY.BOLD }}>
                ${results.discrete_allocation.total_value.toLocaleString()}
              </div>
            </div>
            <div style={{ ...COMMON_STYLES.metricCard, padding: SPACING.LARGE }}>
              <div style={{ ...COMMON_STYLES.dataLabel }}>Allocated</div>
              <div style={{ color: FINCEPT.GREEN, fontSize: '18px', fontWeight: TYPOGRAPHY.BOLD }}>
                ${(results.discrete_allocation.total_value - results.discrete_allocation.leftover_cash).toLocaleString()}
              </div>
            </div>
            <div style={{ ...COMMON_STYLES.metricCard, padding: SPACING.LARGE }}>
              <div style={{ ...COMMON_STYLES.dataLabel }}>Leftover Cash</div>
              <div style={{ color: FINCEPT.YELLOW, fontSize: '18px', fontWeight: TYPOGRAPHY.BOLD }}>
                ${results.discrete_allocation.leftover_cash.toLocaleString()}
              </div>
            </div>
          </div>

          <div style={{ backgroundColor: FINCEPT.PANEL_BG, padding: SPACING.LARGE, border: BORDERS.STANDARD, borderRadius: '2px' }}>
            <div style={{ color: FINCEPT.CYAN, fontSize: TYPOGRAPHY.BODY, fontWeight: TYPOGRAPHY.BOLD, marginBottom: SPACING.DEFAULT }}>
              SHARE ALLOCATION
            </div>
            {Object.entries(results.discrete_allocation.allocation).map(([symbol, shares]) => (
              <div
                key={symbol}
                style={{
                  padding: `${SPACING.MEDIUM} 0`,
                  borderBottom: `1px solid ${FINCEPT.HEADER_BG}`,
                  display: 'flex',
                  justifyContent: 'space-between',
                  fontSize: TYPOGRAPHY.BODY,
                  transition: 'background-color 0.15s ease',
                }}
              >
                <span style={{ color: FINCEPT.CYAN, fontWeight: TYPOGRAPHY.BOLD }}>{symbol}</span>
                <span style={{ color: FINCEPT.WHITE }}>{shares} shares</span>
              </div>
            ))}
          </div>
        </div>
      )}
    </div>
  );
};
