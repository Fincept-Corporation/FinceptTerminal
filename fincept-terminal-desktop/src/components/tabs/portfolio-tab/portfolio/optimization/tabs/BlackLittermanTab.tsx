import React from 'react';
import { Brain, Target, Layers, Shield } from 'lucide-react';
import { FINCEPT, TYPOGRAPHY, SPACING, COMMON_STYLES } from '../../../finceptStyles';
import { OptResultGrid } from '../OptResultGrid';

interface BlackLittermanTabProps {
  blViews: string;
  setBlViews: (s: string) => void;
  blConfidences: string;
  setBlConfidences: (s: string) => void;
  blResults: any;
  hrpResults: any;
  constraintsResults: any;
  viewsOptResults: any;
  sectorMapper: string;
  setSectorMapper: (s: string) => void;
  sectorLower: string;
  setSectorLower: (s: string) => void;
  sectorUpper: string;
  setSectorUpper: (s: string) => void;
  loadingAction: string | null;
  handleBlackLitterman: () => void;
  handleHRP: () => void;
  handleCustomConstraints: () => void;
  handleViewsOptimization: () => void;
}

export const BlackLittermanTab: React.FC<BlackLittermanTabProps> = ({
  blViews,
  setBlViews,
  blConfidences,
  setBlConfidences,
  blResults,
  hrpResults,
  constraintsResults,
  viewsOptResults,
  sectorMapper,
  setSectorMapper,
  sectorLower,
  setSectorLower,
  sectorUpper,
  setSectorUpper,
  loadingAction,
  handleBlackLitterman,
  handleHRP,
  handleCustomConstraints,
  handleViewsOptimization,
}) => {
  return (
    <div>
      {/* Views Input */}
      <div style={{ marginBottom: SPACING.XLARGE }}>
        <div style={{ ...COMMON_STYLES.sectionHeader, borderBottom: `1px solid ${FINCEPT.CYAN}`, paddingBottom: SPACING.MEDIUM }}>
          BLACK-LITTERMAN VIEWS (PyPortfolioOpt)
        </div>
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.LARGE, marginBottom: SPACING.LARGE }}>
          <div>
            <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: SPACING.SMALL }}>VIEWS (JSON: symbol → expected return)</label>
            <input type="text" value={blViews} onChange={(e) => setBlViews(e.target.value)}
              style={{ ...COMMON_STYLES.inputField }} placeholder='{"AAPL": 0.10, "MSFT": 0.05}' />
          </div>
          <div>
            <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: SPACING.SMALL }}>CONFIDENCES (JSON array)</label>
            <input type="text" value={blConfidences} onChange={(e) => setBlConfidences(e.target.value)}
              style={{ ...COMMON_STYLES.inputField }} placeholder='[0.8, 0.6]' />
          </div>
        </div>
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.LARGE }}>
          <button onClick={handleBlackLitterman} disabled={loadingAction === 'bl'}
            style={{ padding: `${SPACING.DEFAULT} ${SPACING.LARGE}`, backgroundColor: loadingAction === 'bl' ? FINCEPT.MUTED : FINCEPT.CYAN, color: FINCEPT.DARK_BG, border: 'none', borderRadius: '2px', fontFamily: TYPOGRAPHY.MONO, fontSize: TYPOGRAPHY.SMALL, fontWeight: TYPOGRAPHY.BOLD, cursor: loadingAction === 'bl' ? 'not-allowed' : 'pointer', display: 'flex', alignItems: 'center', justifyContent: 'center', gap: SPACING.MEDIUM }}
          >
            <Brain size={14} /> {loadingAction === 'bl' ? 'RUNNING...' : 'BLACK-LITTERMAN'}
          </button>
          <button onClick={handleViewsOptimization} disabled={loadingAction === 'viewsopt'}
            style={{ padding: `${SPACING.DEFAULT} ${SPACING.LARGE}`, backgroundColor: loadingAction === 'viewsopt' ? FINCEPT.MUTED : FINCEPT.ORANGE, color: FINCEPT.DARK_BG, border: 'none', borderRadius: '2px', fontFamily: TYPOGRAPHY.MONO, fontSize: TYPOGRAPHY.SMALL, fontWeight: TYPOGRAPHY.BOLD, cursor: loadingAction === 'viewsopt' ? 'not-allowed' : 'pointer', display: 'flex', alignItems: 'center', justifyContent: 'center', gap: SPACING.MEDIUM }}
          >
            <Target size={14} /> {loadingAction === 'viewsopt' ? 'RUNNING...' : 'VIEWS OPTIMIZATION'}
          </button>
        </div>
      </div>

      {/* BL Results */}
      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.XLARGE, marginBottom: SPACING.XLARGE }}>
        {blResults && (
          <div>
            <div style={{ color: FINCEPT.CYAN, fontSize: TYPOGRAPHY.BODY, fontWeight: TYPOGRAPHY.BOLD, marginBottom: SPACING.MEDIUM }}>BLACK-LITTERMAN RESULTS</div>
            <OptResultGrid data={blResults} color={FINCEPT.CYAN} />
          </div>
        )}
        {viewsOptResults && (
          <div>
            <div style={{ color: FINCEPT.ORANGE, fontSize: TYPOGRAPHY.BODY, fontWeight: TYPOGRAPHY.BOLD, marginBottom: SPACING.MEDIUM }}>VIEWS OPTIMIZATION RESULTS</div>
            <OptResultGrid data={viewsOptResults} color={FINCEPT.ORANGE} />
          </div>
        )}
      </div>

      {/* HRP */}
      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.XLARGE, marginBottom: SPACING.XLARGE }}>
        <div>
          <div style={{ ...COMMON_STYLES.sectionHeader, borderBottom: `1px solid ${FINCEPT.GREEN}`, paddingBottom: SPACING.MEDIUM }}>
            HIERARCHICAL RISK PARITY (PyPortfolioOpt)
          </div>
          <button onClick={handleHRP} disabled={loadingAction === 'hrp'}
            style={{ width: '100%', padding: `${SPACING.DEFAULT} ${SPACING.LARGE}`, backgroundColor: loadingAction === 'hrp' ? FINCEPT.MUTED : FINCEPT.GREEN, color: FINCEPT.DARK_BG, border: 'none', borderRadius: '2px', fontFamily: TYPOGRAPHY.MONO, fontSize: TYPOGRAPHY.SMALL, fontWeight: TYPOGRAPHY.BOLD, cursor: loadingAction === 'hrp' ? 'not-allowed' : 'pointer', marginTop: SPACING.MEDIUM, display: 'flex', alignItems: 'center', justifyContent: 'center', gap: SPACING.MEDIUM }}
          >
            <Layers size={14} /> {loadingAction === 'hrp' ? 'RUNNING...' : 'RUN HRP'}
          </button>
          {hrpResults && <div style={{ marginTop: SPACING.MEDIUM }}><OptResultGrid data={hrpResults} color={FINCEPT.GREEN} /></div>}
        </div>

        {/* Custom Constraints */}
        <div>
          <div style={{ ...COMMON_STYLES.sectionHeader, borderBottom: `1px solid ${FINCEPT.YELLOW}`, paddingBottom: SPACING.MEDIUM }}>
            SECTOR CONSTRAINTS (PyPortfolioOpt)
          </div>
          <div style={{ display: 'grid', gap: SPACING.MEDIUM, marginTop: SPACING.MEDIUM }}>
            <div>
              <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: '2px' }}>SECTOR MAP (JSON)</label>
              <input type="text" value={sectorMapper} onChange={(e) => setSectorMapper(e.target.value)}
                style={{ ...COMMON_STYLES.inputField, fontSize: TYPOGRAPHY.TINY }} />
            </div>
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.SMALL }}>
              <div>
                <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: '2px' }}>LOWER BOUNDS</label>
                <input type="text" value={sectorLower} onChange={(e) => setSectorLower(e.target.value)}
                  style={{ ...COMMON_STYLES.inputField, fontSize: TYPOGRAPHY.TINY }} />
              </div>
              <div>
                <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: '2px' }}>UPPER BOUNDS</label>
                <input type="text" value={sectorUpper} onChange={(e) => setSectorUpper(e.target.value)}
                  style={{ ...COMMON_STYLES.inputField, fontSize: TYPOGRAPHY.TINY }} />
              </div>
            </div>
            <button onClick={handleCustomConstraints} disabled={loadingAction === 'constraints'}
              style={{ width: '100%', padding: SPACING.DEFAULT, backgroundColor: loadingAction === 'constraints' ? FINCEPT.MUTED : FINCEPT.YELLOW, color: FINCEPT.DARK_BG, border: 'none', borderRadius: '2px', fontFamily: TYPOGRAPHY.MONO, fontSize: TYPOGRAPHY.SMALL, fontWeight: TYPOGRAPHY.BOLD, cursor: loadingAction === 'constraints' ? 'not-allowed' : 'pointer', display: 'flex', alignItems: 'center', justifyContent: 'center', gap: SPACING.MEDIUM }}
            >
              <Shield size={14} /> {loadingAction === 'constraints' ? 'RUNNING...' : 'OPTIMIZE WITH CONSTRAINTS'}
            </button>
          </div>
          {constraintsResults && <div style={{ marginTop: SPACING.MEDIUM }}><OptResultGrid data={constraintsResults} color={FINCEPT.YELLOW} /></div>}
        </div>
      </div>
    </div>
  );
};
