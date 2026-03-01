import React from 'react';
import { AlertTriangle, Target } from 'lucide-react';
import { FINCEPT, TYPOGRAPHY, SPACING, COMMON_STYLES } from '../../../finceptStyles';
import { OptResultGrid } from '../OptResultGrid';

interface StressTestTabProps {
  stressTestResults: any;
  scenarioResults: any;
  loadingAction: string | null;
  handleStressTest: () => void;
  handleScenarioAnalysis: () => void;
}

export const StressTestTab: React.FC<StressTestTabProps> = ({
  stressTestResults,
  scenarioResults,
  loadingAction,
  handleStressTest,
  handleScenarioAnalysis,
}) => {
  return (
    <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.XLARGE }}>
      {/* Stress Test */}
      <div>
        <div style={{ ...COMMON_STYLES.sectionHeader, borderBottom: `1px solid ${FINCEPT.RED}`, paddingBottom: SPACING.MEDIUM }}>
          MONTE CARLO STRESS TEST (skfolio)
        </div>
        <p style={{ color: FINCEPT.GRAY, fontSize: TYPOGRAPHY.SMALL, marginBottom: SPACING.LARGE }}>
          Run 10,000 simulations to stress-test your optimized portfolio weights.
        </p>
        <button
          onClick={handleStressTest}
          disabled={loadingAction === 'stress'}
          style={{
            width: '100%', padding: `${SPACING.DEFAULT} ${SPACING.LARGE}`,
            backgroundColor: loadingAction === 'stress' ? FINCEPT.MUTED : FINCEPT.RED,
            color: FINCEPT.WHITE, border: 'none', borderRadius: '2px',
            fontFamily: TYPOGRAPHY.MONO, fontSize: TYPOGRAPHY.SMALL, fontWeight: TYPOGRAPHY.BOLD,
            cursor: loadingAction === 'stress' ? 'not-allowed' : 'pointer', marginBottom: SPACING.LARGE,
            display: 'flex', alignItems: 'center', justifyContent: 'center', gap: SPACING.MEDIUM,
          }}
        >
          <AlertTriangle size={14} />
          {loadingAction === 'stress' ? 'RUNNING...' : 'RUN STRESS TEST'}
        </button>
        {stressTestResults && (
          <OptResultGrid data={stressTestResults} color={FINCEPT.RED} />
        )}
      </div>

      {/* Scenario Analysis */}
      <div>
        <div style={{ ...COMMON_STYLES.sectionHeader, borderBottom: `1px solid ${FINCEPT.YELLOW}`, paddingBottom: SPACING.MEDIUM }}>
          SCENARIO ANALYSIS (skfolio)
        </div>
        <p style={{ color: FINCEPT.GRAY, fontSize: TYPOGRAPHY.SMALL, marginBottom: SPACING.LARGE }}>
          Predefined scenarios: Market Crash (-30%), Bull Rally (+20%), Tech Selloff (-15%).
        </p>
        <button
          onClick={handleScenarioAnalysis}
          disabled={loadingAction === 'scenario'}
          style={{
            width: '100%', padding: `${SPACING.DEFAULT} ${SPACING.LARGE}`,
            backgroundColor: loadingAction === 'scenario' ? FINCEPT.MUTED : FINCEPT.YELLOW,
            color: FINCEPT.DARK_BG, border: 'none', borderRadius: '2px',
            fontFamily: TYPOGRAPHY.MONO, fontSize: TYPOGRAPHY.SMALL, fontWeight: TYPOGRAPHY.BOLD,
            cursor: loadingAction === 'scenario' ? 'not-allowed' : 'pointer', marginBottom: SPACING.LARGE,
            display: 'flex', alignItems: 'center', justifyContent: 'center', gap: SPACING.MEDIUM,
          }}
        >
          <Target size={14} />
          {loadingAction === 'scenario' ? 'RUNNING...' : 'RUN SCENARIOS'}
        </button>
        {scenarioResults && (
          <OptResultGrid data={scenarioResults} color={FINCEPT.YELLOW} />
        )}
      </div>
    </div>
  );
};
