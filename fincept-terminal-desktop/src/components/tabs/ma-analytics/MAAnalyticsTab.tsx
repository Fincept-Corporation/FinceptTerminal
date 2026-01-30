/**
 * M&A Analytics Tab - Bloomberg FA Replacement
 *
 * Complete M&A and Financial Advisory System
 * Three-panel terminal layout following Fincept design system
 */

import React, { useState } from 'react';
import {
  Building2,
  TrendingUp,
  Calculator,
  BarChart3,
  Rocket,
  FileText,
  GitMerge,
  Zap,
  Activity,
  Target,
  Database,
} from 'lucide-react';

// Import Fincept styles
import { FINCEPT, TYPOGRAPHY, SPACING, BORDERS, EFFECTS, COMMON_STYLES } from '../portfolio-tab/finceptStyles';

// Import tab components
import { DealDatabasePanel } from './panels/DealDatabasePanel';
import { ValuationPanel } from './panels/ValuationPanel';
import { MergerModelPanel } from './panels/MergerModelPanel';
import { LBOModelPanel } from './panels/LBOModelPanel';
import { StartupValuationPanel } from './panels/StartupValuationPanel';
import { DealStructurePanel } from './panels/DealStructurePanel';
import { SynergiesPanel } from './panels/SynergiesPanel';
import { FairnessOpinionPanel } from './panels/FairnessOpinionPanel';
import { IndustryMetricsPanel } from './panels/IndustryMetricsPanel';
import { AdvancedAnalyticsPanel } from './panels/AdvancedAnalyticsPanel';

type AnalysisModule =
  | 'deal_database'
  | 'valuation'
  | 'merger_model'
  | 'lbo'
  | 'startup'
  | 'deal_structure'
  | 'synergies'
  | 'fairness'
  | 'industry_metrics'
  | 'advanced';

const MODULES = [
  { id: 'deal_database' as const, label: 'DEAL DATABASE', icon: Database },
  { id: 'valuation' as const, label: 'VALUATION', icon: TrendingUp },
  { id: 'merger_model' as const, label: 'MERGER MODEL', icon: GitMerge },
  { id: 'lbo' as const, label: 'LBO MODEL', icon: Building2 },
  { id: 'startup' as const, label: 'STARTUP', icon: Rocket },
  { id: 'deal_structure' as const, label: 'DEAL STRUCTURE', icon: FileText },
  { id: 'synergies' as const, label: 'SYNERGIES', icon: Zap },
  { id: 'fairness' as const, label: 'FAIRNESS OPINION', icon: Target },
  { id: 'industry_metrics' as const, label: 'INDUSTRY METRICS', icon: BarChart3 },
  { id: 'advanced' as const, label: 'ADVANCED', icon: Activity },
];

const MAAnalyticsTab: React.FC = () => {
  const [activeModule, setActiveModule] = useState<AnalysisModule>('deal_database');
  const [selectedDeal, setSelectedDeal] = useState<any>(null);

  const renderActivePanel = () => {
    switch (activeModule) {
      case 'deal_database':
        return <DealDatabasePanel onSelectDeal={setSelectedDeal} />;
      case 'valuation':
        return <ValuationPanel selectedDeal={selectedDeal} />;
      case 'merger_model':
        return <MergerModelPanel selectedDeal={selectedDeal} />;
      case 'lbo':
        return <LBOModelPanel selectedDeal={selectedDeal} />;
      case 'startup':
        return <StartupValuationPanel />;
      case 'deal_structure':
        return <DealStructurePanel selectedDeal={selectedDeal} />;
      case 'synergies':
        return <SynergiesPanel selectedDeal={selectedDeal} />;
      case 'fairness':
        return <FairnessOpinionPanel selectedDeal={selectedDeal} />;
      case 'industry_metrics':
        return <IndustryMetricsPanel />;
      case 'advanced':
        return <AdvancedAnalyticsPanel selectedDeal={selectedDeal} />;
      default:
        return null;
    }
  };

  return (
    <div
      style={{
        ...COMMON_STYLES.container,
        fontFamily: TYPOGRAPHY.MONO,
      }}
    >
      {/* Top Navigation Bar */}
      <div
        style={{
          ...COMMON_STYLES.header,
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
          flexWrap: 'wrap',
          gap: SPACING.MEDIUM,
        }}
      >
        {/* Left Section - Title */}
        <div style={{ display: 'flex', alignItems: 'center', gap: SPACING.MEDIUM }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: SPACING.SMALL }}>
            <Building2 size={16} color={FINCEPT.ORANGE} />
            <span
              style={{
                color: FINCEPT.ORANGE,
                fontWeight: TYPOGRAPHY.BOLD,
                fontSize: TYPOGRAPHY.SUBHEADING,
                letterSpacing: TYPOGRAPHY.WIDE,
                textTransform: 'uppercase',
              }}
            >
              M&A ANALYTICS
            </span>
          </div>

          <div style={COMMON_STYLES.verticalDivider} />

          {/* Module Selector Tabs */}
          <div style={{ display: 'flex', gap: SPACING.TINY }}>
            {MODULES.map((module) => {
              const Icon = module.icon;
              const isActive = activeModule === module.id;
              return (
                <button
                  key={module.id}
                  onClick={() => setActiveModule(module.id)}
                  style={{
                    ...COMMON_STYLES.tabButton(isActive),
                    display: 'flex',
                    alignItems: 'center',
                    gap: SPACING.SMALL,
                  }}
                >
                  <Icon size={12} />
                  {module.label}
                </button>
              );
            })}
          </div>
        </div>

        {/* Right Section - Actions */}
        <div style={{ display: 'flex', alignItems: 'center', gap: SPACING.SMALL }}>
          <span
            style={{
              fontSize: TYPOGRAPHY.TINY,
              color: FINCEPT.GRAY,
              letterSpacing: TYPOGRAPHY.WIDE,
            }}
          >
            BLOOMBERG FA REPLACEMENT
          </span>
        </div>
      </div>

      {/* Main Content Area */}
      <div
        style={{
          flex: 1,
          overflow: 'hidden',
          display: 'flex',
          flexDirection: 'column',
        }}
      >
        {renderActivePanel()}
      </div>

      {/* Status Bar */}
      <div
        style={{
          backgroundColor: FINCEPT.HEADER_BG,
          borderTop: `1px solid ${FINCEPT.BORDER}`,
          padding: `${SPACING.SMALL} ${SPACING.LARGE}`,
          fontSize: TYPOGRAPHY.TINY,
          color: FINCEPT.GRAY,
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
        }}
      >
        <div style={{ display: 'flex', gap: SPACING.LARGE }}>
          <span>
            MODULE: <span style={{ color: FINCEPT.ORANGE }}>{MODULES.find((m) => m.id === activeModule)?.label}</span>
          </span>
          {selectedDeal && (
            <span>
              SELECTED DEAL:{' '}
              <span style={{ color: FINCEPT.CYAN }}>
                {selectedDeal.acquirer_name} + {selectedDeal.target_name}
              </span>
            </span>
          )}
        </div>
        <div>
          <span style={{ color: FINCEPT.MUTED }}>TERMINAL v3.2.1</span>
        </div>
      </div>
    </div>
  );
};

export default MAAnalyticsTab;
