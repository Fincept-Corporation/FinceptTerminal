import React from 'react';
import { GitCompare, Shield, Download } from 'lucide-react';
import { invoke } from '@tauri-apps/api/core';
import { FINCEPT, TYPOGRAPHY, SPACING, COMMON_STYLES } from '../../../finceptStyles';
import { OptResultGrid } from '../OptResultGrid';
import { OptimizationResults } from '../types';

interface CompareTabProps {
  assetSymbols: string;
  results: OptimizationResults | null;
  compareResults: any;
  riskAttribution: any;
  hypertuneResults: any;
  skfolioRiskMetrics: any;
  validateResults: any;
  skfolioConfig: { risk_measure: string; [key: string]: any };
  loadingAction: string | null;
  setError: (e: string | null) => void;
  handleCompareStrategies: () => void;
  handleRiskAttribution: () => void;
  handleHyperparameterTuning: () => void;
  handleSkfolioRiskMetrics: () => void;
}

export const CompareTab: React.FC<CompareTabProps> = ({
  assetSymbols,
  results,
  compareResults,
  riskAttribution,
  hypertuneResults,
  skfolioRiskMetrics,
  validateResults,
  loadingAction,
  setError,
  handleCompareStrategies,
  handleRiskAttribution,
  handleHyperparameterTuning,
  handleSkfolioRiskMetrics,
}) => {
  return (
    <div>
      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.XLARGE, marginBottom: SPACING.XLARGE }}>
        {/* Compare Strategies */}
        <div>
          <div style={{ ...COMMON_STYLES.sectionHeader, borderBottom: `1px solid ${FINCEPT.CYAN}`, paddingBottom: SPACING.MEDIUM }}>
            COMPARE STRATEGIES (skfolio)
          </div>
          <p style={{ color: FINCEPT.GRAY, fontSize: TYPOGRAPHY.SMALL, marginBottom: SPACING.LARGE }}>
            Compare mean-risk, risk parity, HRP, max diversification, equal weight, inverse vol.
          </p>
          <button onClick={handleCompareStrategies} disabled={loadingAction === 'compare'}
            style={{ width: '100%', padding: `${SPACING.DEFAULT} ${SPACING.LARGE}`, backgroundColor: loadingAction === 'compare' ? FINCEPT.MUTED : FINCEPT.CYAN, color: FINCEPT.DARK_BG, border: 'none', borderRadius: '2px', fontFamily: TYPOGRAPHY.MONO, fontSize: TYPOGRAPHY.SMALL, fontWeight: TYPOGRAPHY.BOLD, cursor: loadingAction === 'compare' ? 'not-allowed' : 'pointer', marginBottom: SPACING.LARGE, display: 'flex', alignItems: 'center', justifyContent: 'center', gap: SPACING.MEDIUM }}
          >
            <GitCompare size={14} />
            {loadingAction === 'compare' ? 'COMPARING...' : 'COMPARE ALL'}
          </button>
          {compareResults && <OptResultGrid data={compareResults} color={FINCEPT.CYAN} />}
        </div>

        {/* Risk Attribution */}
        <div>
          <div style={{ ...COMMON_STYLES.sectionHeader, borderBottom: `1px solid ${FINCEPT.ORANGE}`, paddingBottom: SPACING.MEDIUM }}>
            RISK ATTRIBUTION (skfolio)
          </div>
          <p style={{ color: FINCEPT.GRAY, fontSize: TYPOGRAPHY.SMALL, marginBottom: SPACING.LARGE }}>
            Risk contribution by asset. Run optimization first to populate weights.
          </p>
          <button onClick={handleRiskAttribution} disabled={loadingAction === 'riskattr'}
            style={{ width: '100%', padding: `${SPACING.DEFAULT} ${SPACING.LARGE}`, backgroundColor: loadingAction === 'riskattr' ? FINCEPT.MUTED : FINCEPT.ORANGE, color: FINCEPT.DARK_BG, border: 'none', borderRadius: '2px', fontFamily: TYPOGRAPHY.MONO, fontSize: TYPOGRAPHY.SMALL, fontWeight: TYPOGRAPHY.BOLD, cursor: loadingAction === 'riskattr' ? 'not-allowed' : 'pointer', marginBottom: SPACING.LARGE, display: 'flex', alignItems: 'center', justifyContent: 'center', gap: SPACING.MEDIUM }}
          >
            <Shield size={14} />
            {loadingAction === 'riskattr' ? 'ANALYZING...' : 'RISK ATTRIBUTION'}
          </button>
          {riskAttribution && <OptResultGrid data={riskAttribution} color={FINCEPT.ORANGE} />}
        </div>
      </div>

      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: SPACING.XLARGE }}>
        {/* Hyperparameter Tuning */}
        <div>
          <div style={{ ...COMMON_STYLES.sectionHeader, borderBottom: `1px solid ${FINCEPT.GREEN}`, paddingBottom: SPACING.MEDIUM, fontSize: TYPOGRAPHY.TINY }}>
            HYPERPARAMETER TUNING
          </div>
          <button onClick={handleHyperparameterTuning} disabled={loadingAction === 'hypertune'}
            style={{ width: '100%', padding: SPACING.DEFAULT, backgroundColor: loadingAction === 'hypertune' ? FINCEPT.MUTED : FINCEPT.GREEN, color: FINCEPT.DARK_BG, border: 'none', borderRadius: '2px', fontFamily: TYPOGRAPHY.MONO, fontSize: TYPOGRAPHY.TINY, fontWeight: TYPOGRAPHY.BOLD, cursor: loadingAction === 'hypertune' ? 'not-allowed' : 'pointer', marginBottom: SPACING.MEDIUM, marginTop: SPACING.MEDIUM }}
          >
            {loadingAction === 'hypertune' ? 'TUNING...' : 'TUNE'}
          </button>
          {hypertuneResults && <OptResultGrid data={hypertuneResults} color={FINCEPT.GREEN} />}
        </div>

        {/* Risk Metrics + Validate */}
        <div>
          <div style={{ ...COMMON_STYLES.sectionHeader, borderBottom: `1px solid ${FINCEPT.YELLOW}`, paddingBottom: SPACING.MEDIUM, fontSize: TYPOGRAPHY.TINY }}>
            RISK METRICS + VALIDATE
          </div>
          <button onClick={handleSkfolioRiskMetrics} disabled={loadingAction === 'skrm'}
            style={{ width: '100%', padding: SPACING.DEFAULT, backgroundColor: loadingAction === 'skrm' ? FINCEPT.MUTED : FINCEPT.YELLOW, color: FINCEPT.DARK_BG, border: 'none', borderRadius: '2px', fontFamily: TYPOGRAPHY.MONO, fontSize: TYPOGRAPHY.TINY, fontWeight: TYPOGRAPHY.BOLD, cursor: loadingAction === 'skrm' ? 'not-allowed' : 'pointer', marginBottom: SPACING.MEDIUM, marginTop: SPACING.MEDIUM }}
          >
            {loadingAction === 'skrm' ? 'RUNNING...' : 'COMPUTE'}
          </button>
          {skfolioRiskMetrics && <OptResultGrid data={skfolioRiskMetrics} color={FINCEPT.YELLOW} />}
          {validateResults && <OptResultGrid data={validateResults} color={FINCEPT.CYAN} />}
        </div>

        {/* Export Weights */}
        <div>
          <div style={{ ...COMMON_STYLES.sectionHeader, borderBottom: `1px solid ${FINCEPT.WHITE}`, paddingBottom: SPACING.MEDIUM, fontSize: TYPOGRAPHY.TINY }}>
            EXPORT WEIGHTS
          </div>
          <button
            onClick={async () => {
              try {
                const weightsStr = JSON.stringify(results?.weights || {});
                const assetNamesStr = JSON.stringify(assetSymbols.split(',').map(s => s.trim()));
                const res = await invoke<string>('skfolio_export_weights', { weights: weightsStr, assetNames: assetNamesStr, format: 'json' });
                const blob = new Blob([res], { type: 'application/json' });
                const url = URL.createObjectURL(blob);
                const a = document.createElement('a');
                a.href = url; a.download = `skfolio_weights_${Date.now()}.json`;
                document.body.appendChild(a); a.click(); document.body.removeChild(a);
                URL.revokeObjectURL(url);
              } catch (err: any) { setError(err?.message || String(err)); }
            }}
            style={{ width: '100%', padding: SPACING.DEFAULT, backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.ORANGE}`, color: FINCEPT.ORANGE, borderRadius: '2px', fontFamily: TYPOGRAPHY.MONO, fontSize: TYPOGRAPHY.TINY, fontWeight: TYPOGRAPHY.BOLD, cursor: 'pointer', marginTop: SPACING.MEDIUM, display: 'flex', alignItems: 'center', justifyContent: 'center', gap: SPACING.SMALL }}
          >
            <Download size={12} /> EXPORT JSON
          </button>
        </div>
      </div>
    </div>
  );
};
