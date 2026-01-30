import React from 'react';
import { FINCEPT, TYPOGRAPHY, SPACING, COMMON_STYLES } from '../../portfolio-tab/finceptStyles';
import { Activity } from 'lucide-react';

export const SynergiesPanel: React.FC<{ selectedDeal?: any }> = () => (
  <div style={COMMON_STYLES.emptyState}>
    <Activity size={48} style={{ opacity: 0.3, marginBottom: SPACING.DEFAULT }} />
    <div style={{ fontSize: TYPOGRAPHY.BODY, color: FINCEPT.GRAY }}>SynergiesPanel - Under Construction</div>
  </div>
);
