import React from 'react';
import { GitMerge } from 'lucide-react';
import { FINCEPT, TYPOGRAPHY, SPACING, COMMON_STYLES } from '../../portfolio-tab/finceptStyles';

export const MergerModelPanel: React.FC<{ selectedDeal: any }> = () => (
  <div style={COMMON_STYLES.emptyState}>
    <GitMerge size={48} style={{ opacity: 0.3, marginBottom: SPACING.DEFAULT }} />
    <div style={{ fontSize: TYPOGRAPHY.BODY, color: FINCEPT.GRAY }}>Merger Model - Under Construction</div>
    <div style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.MUTED, marginTop: SPACING.SMALL }}>
      Accretion/Dilution, Pro Forma, Sources & Uses
    </div>
  </div>
);
