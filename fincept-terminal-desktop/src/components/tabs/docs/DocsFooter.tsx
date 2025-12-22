import React from 'react';
import { DocSubsection } from './types';
import { TabFooter } from '@/components/common/TabFooter';
import { COLORS } from './constants';

interface DocsFooterProps {
  activeSubsection: DocSubsection | undefined;
}

export function DocsFooter({ activeSubsection }: DocsFooterProps) {
  return (
    <TabFooter
      tabName="DOCUMENTATION"
      leftInfo={[
        { label: 'FinScript Language Reference', color: '#06b6d4' },
        { label: 'API Documentation', color: '#3b82f6' },
      ]}
      statusInfo={activeSubsection ? activeSubsection.title : 'Browse Documentation'}
      backgroundColor={COLORS.PANEL_BG}
      borderColor={COLORS.ORANGE}
    />
  );
}
