/**
 * M&A Analytics Tab - Comprehensive Corporate Finance Toolkit
 *
 * Professional M&A analytics with 8 focused sections:
 * 1. Valuation Toolkit - DCF, LBO, Trading Comps
 * 2. Merger Analysis - Accretion/Dilution, Synergies, Deal Structure
 * 3. Deal Database - Track and save deals
 * 4. Startup Valuation - Berkus, Scorecard, VC Method, First Chicago, Risk Factor
 * 5. Fairness Opinion - Valuation framework, premium analysis, process quality
 * 6. Industry Metrics - Technology, Healthcare, Financial Services
 * 7. Advanced Analytics - Monte Carlo, Regression
 * 8. Deal Comparison - Compare, rank, benchmark deals
 */

import React, { useState, useCallback } from 'react';
import { useWorkspaceTabState } from '@/hooks/useWorkspaceTabState';
import {
  Calculator,
  GitMerge,
  Database,
  Building2,
  Rocket,
  Scale,
  BarChart3,
  Activity,
  GitCompare,
} from 'lucide-react';
import { FINCEPT, TYPOGRAPHY, SPACING, COMMON_STYLES } from '../portfolio-tab/finceptStyles';

// Import unified panels
import { ValuationToolkit } from './unified/ValuationToolkit';
import { MergerAnalysis } from './unified/MergerAnalysis';
import { DealDatabase } from './unified/DealDatabase';
import { StartupValuation } from './unified/StartupValuation';
import { FairnessOpinion } from './unified/FairnessOpinion';
import { IndustryMetrics } from './unified/IndustryMetrics';
import { AdvancedAnalytics } from './unified/AdvancedAnalytics';
import { DealComparison } from './unified/DealComparison';

type MainTab =
  | 'valuation'
  | 'merger'
  | 'database'
  | 'startup'
  | 'fairness'
  | 'industry'
  | 'advanced'
  | 'comparison';

interface TabConfig {
  id: MainTab;
  label: string;
  shortLabel: string;
  icon: React.ElementType;
  description: string;
  category: 'core' | 'specialized' | 'analytics';
}

const MAAnalyticsTabNew: React.FC = () => {
  const [activeTab, setActiveTab] = useState<MainTab>('valuation');

  // Workspace tab state
  const getWorkspaceState = useCallback(
    () => ({
      activeTab,
    }),
    [activeTab]
  );

  const setWorkspaceState = useCallback((state: Record<string, unknown>) => {
    if (typeof state.activeTab === 'string') setActiveTab(state.activeTab as MainTab);
  }, []);

  useWorkspaceTabState('ma-analytics', getWorkspaceState, setWorkspaceState);

  const TABS: TabConfig[] = [
    // Core M&A Tools
    {
      id: 'valuation',
      label: 'Valuation Toolkit',
      shortLabel: 'Valuation',
      icon: Calculator,
      description: 'DCF, LBO, Trading Comps',
      category: 'core',
    },
    {
      id: 'merger',
      label: 'Merger Analysis',
      shortLabel: 'Mergers',
      icon: GitMerge,
      description: 'Accretion/Dilution, Synergies',
      category: 'core',
    },
    {
      id: 'database',
      label: 'Deal Database',
      shortLabel: 'Deals',
      icon: Database,
      description: 'Track & Save Deals',
      category: 'core',
    },
    // Specialized Tools
    {
      id: 'startup',
      label: 'Startup Valuation',
      shortLabel: 'Startup',
      icon: Rocket,
      description: 'Berkus, Scorecard, VC Method',
      category: 'specialized',
    },
    {
      id: 'fairness',
      label: 'Fairness Opinion',
      shortLabel: 'Fairness',
      icon: Scale,
      description: 'Board-level analysis',
      category: 'specialized',
    },
    {
      id: 'industry',
      label: 'Industry Metrics',
      shortLabel: 'Industry',
      icon: BarChart3,
      description: 'Sector-specific metrics',
      category: 'specialized',
    },
    // Analytics
    {
      id: 'advanced',
      label: 'Advanced Analytics',
      shortLabel: 'Advanced',
      icon: Activity,
      description: 'Monte Carlo, Regression',
      category: 'analytics',
    },
    {
      id: 'comparison',
      label: 'Deal Comparison',
      shortLabel: 'Compare',
      icon: GitCompare,
      description: 'Compare & Benchmark',
      category: 'analytics',
    },
  ];

  const renderContent = () => {
    switch (activeTab) {
      case 'valuation':
        return <ValuationToolkit />;
      case 'merger':
        return <MergerAnalysis />;
      case 'database':
        return <DealDatabase />;
      case 'startup':
        return <StartupValuation />;
      case 'fairness':
        return <FairnessOpinion />;
      case 'industry':
        return <IndustryMetrics />;
      case 'advanced':
        return <AdvancedAnalytics />;
      case 'comparison':
        return <DealComparison />;
      default:
        return null;
    }
  };

  const renderCategoryTabs = (category: 'core' | 'specialized' | 'analytics') => {
    const categoryTabs = TABS.filter((t) => t.category === category);
    return categoryTabs.map((tab) => {
      const Icon = tab.icon;
      const isActive = activeTab === tab.id;
      return (
        <button
          key={tab.id}
          onClick={() => setActiveTab(tab.id)}
          title={`${tab.label}: ${tab.description}`}
          style={{
            display: 'flex',
            alignItems: 'center',
            gap: '6px',
            padding: '8px 12px',
            backgroundColor: isActive ? FINCEPT.PANEL_BG : 'transparent',
            border: `1px solid ${isActive ? FINCEPT.ORANGE : 'transparent'}`,
            borderRadius: '4px',
            color: isActive ? FINCEPT.ORANGE : FINCEPT.GRAY,
            cursor: 'pointer',
            transition: 'all 0.15s',
            fontSize: '11px',
            fontWeight: isActive ? 600 : 400,
            letterSpacing: '0.5px',
            whiteSpace: 'nowrap',
          }}
          onMouseEnter={(e) => {
            if (!isActive) {
              e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
              e.currentTarget.style.color = '#fff';
            }
          }}
          onMouseLeave={(e) => {
            if (!isActive) {
              e.currentTarget.style.backgroundColor = 'transparent';
              e.currentTarget.style.color = FINCEPT.GRAY;
            }
          }}
        >
          <Icon size={14} />
          <span>{tab.shortLabel}</span>
        </button>
      );
    });
  };

  return (
    <div
      style={{
        display: 'flex',
        flexDirection: 'column',
        height: '100%',
        backgroundColor: FINCEPT.DARK_BG,
        fontFamily: TYPOGRAPHY.MONO,
      }}
    >
      {/* Header */}
      <div
        style={{
          backgroundColor: FINCEPT.HEADER_BG,
          borderBottom: `2px solid ${FINCEPT.ORANGE}`,
          padding: '12px 16px',
        }}
      >
        {/* Title Row */}
        <div
          style={{
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'space-between',
            marginBottom: '12px',
          }}
        >
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <Building2 size={20} color={FINCEPT.ORANGE} />
            <span
              style={{
                color: FINCEPT.ORANGE,
                fontWeight: 700,
                fontSize: '14px',
                letterSpacing: '1px',
              }}
            >
              M&A ANALYTICS
            </span>
          </div>
          <div
            style={{
              fontSize: '10px',
              color: FINCEPT.GRAY,
              letterSpacing: '1px',
            }}
          >
            CORPORATE FINANCE TOOLKIT
          </div>
        </div>

        {/* Tab Navigation - Grouped by Category */}
        <div
          style={{
            display: 'flex',
            alignItems: 'center',
            gap: '16px',
            flexWrap: 'wrap',
          }}
        >
          {/* Core Tools */}
          <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
            <span
              style={{
                fontSize: '9px',
                color: FINCEPT.MUTED,
                letterSpacing: '1px',
                marginRight: '4px',
              }}
            >
              CORE
            </span>
            {renderCategoryTabs('core')}
          </div>

          <div
            style={{
              width: '1px',
              height: '24px',
              backgroundColor: FINCEPT.BORDER,
            }}
          />

          {/* Specialized Tools */}
          <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
            <span
              style={{
                fontSize: '9px',
                color: FINCEPT.MUTED,
                letterSpacing: '1px',
                marginRight: '4px',
              }}
            >
              SPECIALIZED
            </span>
            {renderCategoryTabs('specialized')}
          </div>

          <div
            style={{
              width: '1px',
              height: '24px',
              backgroundColor: FINCEPT.BORDER,
            }}
          />

          {/* Analytics */}
          <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
            <span
              style={{
                fontSize: '9px',
                color: FINCEPT.MUTED,
                letterSpacing: '1px',
                marginRight: '4px',
              }}
            >
              ANALYTICS
            </span>
            {renderCategoryTabs('analytics')}
          </div>
        </div>
      </div>

      {/* Main Content */}
      <div
        style={{
          flex: 1,
          overflow: 'hidden',
        }}
      >
        {renderContent()}
      </div>

      {/* Status Bar */}
      <div
        style={{
          backgroundColor: FINCEPT.HEADER_BG,
          borderTop: `1px solid ${FINCEPT.BORDER}`,
          padding: '6px 16px',
          fontSize: '10px',
          color: FINCEPT.GRAY,
          display: 'flex',
          justifyContent: 'space-between',
        }}
      >
        <span>
          ACTIVE:{' '}
          <span style={{ color: FINCEPT.ORANGE }}>
            {TABS.find((t) => t.id === activeTab)?.label.toUpperCase()}
          </span>
        </span>
        <span style={{ color: FINCEPT.MUTED }}>FINCEPT TERMINAL v3.3</span>
      </div>
    </div>
  );
};

export default MAAnalyticsTabNew;
