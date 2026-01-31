/**
 * M&A Analytics Tab - Streamlined & Unified
 *
 * Clean, professional M&A toolkit with 3 focused sections:
 * 1. Valuation Toolkit - DCF, LBO, Trading Comps
 * 2. Merger Analysis - Accretion/Dilution, Synergies, Deal Structure
 * 3. Deal Database - Track and save deals
 */

import React, { useState } from 'react';
import { Calculator, GitMerge, Database, TrendingUp, Building2, Zap } from 'lucide-react';
import { FINCEPT, TYPOGRAPHY, SPACING, COMMON_STYLES } from '../portfolio-tab/finceptStyles';

// Import new unified panels
import { ValuationToolkit } from './unified/ValuationToolkit';
import { MergerAnalysis } from './unified/MergerAnalysis';
import { DealDatabase } from './unified/DealDatabase';

type MainTab = 'valuation' | 'merger' | 'database';

const MAAnalyticsTabNew: React.FC = () => {
  const [activeTab, setActiveTab] = useState<MainTab>('valuation');

  const TABS = [
    {
      id: 'valuation' as const,
      label: 'VALUATION TOOLKIT',
      icon: Calculator,
      description: 'DCF, LBO, Trading Comps'
    },
    {
      id: 'merger' as const,
      label: 'MERGER ANALYSIS',
      icon: GitMerge,
      description: 'Accretion/Dilution, Synergies'
    },
    {
      id: 'database' as const,
      label: 'DEAL DATABASE',
      icon: Database,
      description: 'Track & Save Deals'
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
      default:
        return null;
    }
  };

  return (
    <div style={{
      display: 'flex',
      flexDirection: 'column',
      height: '100%',
      backgroundColor: FINCEPT.DARK_BG,
      fontFamily: TYPOGRAPHY.MONO,
    }}>
      {/* Header */}
      <div style={{
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `2px solid ${FINCEPT.ORANGE}`,
        padding: SPACING.DEFAULT,
      }}>
        <div style={{
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
          marginBottom: SPACING.DEFAULT,
        }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: SPACING.SMALL }}>
            <Building2 size={20} color={FINCEPT.ORANGE} />
            <span style={{
              color: FINCEPT.ORANGE,
              fontWeight: TYPOGRAPHY.BOLD,
              fontSize: TYPOGRAPHY.HEADING,
              letterSpacing: TYPOGRAPHY.WIDE,
            }}>
              M&A ANALYTICS
            </span>
          </div>
          <div style={{
            fontSize: TYPOGRAPHY.TINY,
            color: FINCEPT.GRAY,
            letterSpacing: TYPOGRAPHY.WIDE,
          }}>
            PROFESSIONAL TOOLKIT
          </div>
        </div>

        {/* Tab Navigation */}
        <div style={{ display: 'flex', gap: SPACING.SMALL }}>
          {TABS.map((tab) => {
            const Icon = tab.icon;
            const isActive = activeTab === tab.id;
            return (
              <button
                key={tab.id}
                onClick={() => setActiveTab(tab.id)}
                style={{
                  flex: 1,
                  padding: SPACING.DEFAULT,
                  backgroundColor: isActive ? FINCEPT.PANEL_BG : 'transparent',
                  border: `1px solid ${isActive ? FINCEPT.ORANGE : FINCEPT.BORDER}`,
                  borderRadius: '4px',
                  color: isActive ? FINCEPT.ORANGE : FINCEPT.GRAY,
                  cursor: 'pointer',
                  transition: 'all 0.2s',
                  display: 'flex',
                  flexDirection: 'column',
                  alignItems: 'center',
                  gap: SPACING.TINY,
                }}
                onMouseEnter={(e) => {
                  if (!isActive) {
                    e.currentTarget.style.borderColor = FINCEPT.GRAY;
                    e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                  }
                }}
                onMouseLeave={(e) => {
                  if (!isActive) {
                    e.currentTarget.style.borderColor = FINCEPT.BORDER;
                    e.currentTarget.style.backgroundColor = 'transparent';
                  }
                }}
              >
                <div style={{ display: 'flex', alignItems: 'center', gap: SPACING.SMALL }}>
                  <Icon size={14} />
                  <span style={{
                    fontSize: TYPOGRAPHY.SMALL,
                    fontWeight: TYPOGRAPHY.BOLD,
                    letterSpacing: TYPOGRAPHY.WIDE,
                  }}>
                    {tab.label}
                  </span>
                </div>
                <span style={{
                  fontSize: TYPOGRAPHY.TINY,
                  color: FINCEPT.MUTED,
                }}>
                  {tab.description}
                </span>
              </button>
            );
          })}
        </div>
      </div>

      {/* Main Content */}
      <div style={{
        flex: 1,
        overflow: 'hidden',
      }}>
        {renderContent()}
      </div>

      {/* Status Bar */}
      <div style={{
        backgroundColor: FINCEPT.HEADER_BG,
        borderTop: `1px solid ${FINCEPT.BORDER}`,
        padding: `${SPACING.SMALL} ${SPACING.DEFAULT}`,
        fontSize: TYPOGRAPHY.TINY,
        color: FINCEPT.GRAY,
        display: 'flex',
        justifyContent: 'space-between',
      }}>
        <span>
          ACTIVE: <span style={{ color: FINCEPT.ORANGE }}>
            {TABS.find(t => t.id === activeTab)?.label}
          </span>
        </span>
        <span style={{ color: FINCEPT.MUTED }}>FINCEPT TERMINAL v3.2.1</span>
      </div>
    </div>
  );
};

export default MAAnalyticsTabNew;
