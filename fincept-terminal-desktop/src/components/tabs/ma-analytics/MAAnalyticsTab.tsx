/**
 * M&A Analytics Tab - Bloomberg-Grade Corporate Finance Terminal
 *
 * Three-panel layout with collapsible sidebars:
 * - Left: Module navigation (CORE / SPECIALIZED / ANALYTICS)
 * - Center: Active module content
 * - Right: Deal context panel
 * - Top: Nav bar with branding + status
 * - Bottom: Status bar with clock
 */

import React, { useReducer, useCallback, useMemo } from 'react';
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
  PanelLeftClose,
  PanelLeftOpen,
  PanelRightClose,
  PanelRightOpen,
  Info,
  Layers,
  TrendingUp,
} from 'lucide-react';
import { FINCEPT, TYPOGRAPHY } from '../portfolio-tab/finceptStyles';
import { MA_MODULES, MA_COLORS } from './constants';
import { MAStatusBar } from './components';

// Import unified panels
import { ValuationToolkit } from './unified/ValuationToolkit';
import { MergerAnalysis } from './unified/MergerAnalysis';
import { DealDatabase } from './unified/DealDatabase';
import { StartupValuation } from './unified/StartupValuation';
import { FairnessOpinion } from './unified/FairnessOpinion';
import { IndustryMetrics } from './unified/IndustryMetrics';
import { AdvancedAnalytics } from './unified/AdvancedAnalytics';
import { DealComparison } from './unified/DealComparison';

type MainTab = typeof MA_MODULES[number]['id'];

const ICONS: Record<string, React.ElementType> = {
  valuation: Calculator,
  merger: GitMerge,
  deals: Database,
  startup: Rocket,
  fairness: Scale,
  industry: BarChart3,
  advanced: Activity,
  comparison: GitCompare,
};

const MODULE_DESCRIPTIONS: Record<string, string[]> = {
  valuation: ['DCF Analysis', 'LBO Modeling (Returns, Full Model, Debt Schedule, Sensitivity)', 'Trading Comparables', 'Precedent Transactions'],
  merger: ['Accretion / Dilution Analysis', 'Revenue & Cost Synergies', 'Deal Structure (Payment, Earnout, Exchange, Collar, CVR)', 'Pro Forma Financials'],
  deals: ['Deal Database & Search', 'SEC Filing Scanner (EDGAR)', 'Auto-Parse Deal Indicators', 'Confidence Scoring'],
  startup: ['Berkus Method', 'Scorecard Method', 'VC Method', 'First Chicago Method', 'Risk Factor Summation', 'Comprehensive (All Methods)'],
  fairness: ['Fairness Analysis & Conclusion', 'Premium Analysis (Multi-Period)', 'Process Quality Assessment'],
  industry: ['Technology (SaaS, Marketplace, Semi)', 'Healthcare (Pharma, Biotech, Devices)', 'Financial Services (Banking, Insurance, AM)'],
  advanced: ['Monte Carlo Simulation (1K-100K runs)', 'Regression Analysis (OLS, Multiple)'],
  comparison: ['Side-by-Side Deal Comparison', 'Deal Rankings', 'Premium Benchmarking', 'Payment Structure Analysis', 'Industry Analysis'],
};

// State management
interface State {
  activeTab: MainTab;
  leftPanelOpen: boolean;
  rightPanelOpen: boolean;
}

type Action =
  | { type: 'SET_TAB'; payload: MainTab }
  | { type: 'TOGGLE_LEFT' }
  | { type: 'TOGGLE_RIGHT' };

function reducer(state: State, action: Action): State {
  switch (action.type) {
    case 'SET_TAB': return { ...state, activeTab: action.payload };
    case 'TOGGLE_LEFT': return { ...state, leftPanelOpen: !state.leftPanelOpen };
    case 'TOGGLE_RIGHT': return { ...state, rightPanelOpen: !state.rightPanelOpen };
    default: return state;
  }
}

const initialState: State = {
  activeTab: 'valuation',
  leftPanelOpen: true,
  rightPanelOpen: true,
};

const MAAnalyticsTabNew: React.FC = () => {
  const [state, dispatch] = useReducer(reducer, initialState);
  const { activeTab, leftPanelOpen, rightPanelOpen } = state;

  // Workspace persistence
  const getWorkspaceState = useCallback(
    () => ({ activeTab }),
    [activeTab]
  );

  const setWorkspaceState = useCallback((ws: Record<string, unknown>) => {
    if (typeof ws.activeTab === 'string') dispatch({ type: 'SET_TAB', payload: ws.activeTab as MainTab });
  }, []);

  useWorkspaceTabState('ma-analytics', getWorkspaceState, setWorkspaceState);

  const activeModule = useMemo(() => MA_MODULES.find(m => m.id === activeTab)!, [activeTab]);
  const activeColor = activeModule.color;
  const categories = ['CORE', 'SPECIALIZED', 'ANALYTICS'] as const;

  const renderContent = () => {
    switch (activeTab) {
      case 'valuation': return <ValuationToolkit />;
      case 'merger': return <MergerAnalysis />;
      case 'deals': return <DealDatabase />;
      case 'startup': return <StartupValuation />;
      case 'fairness': return <FairnessOpinion />;
      case 'industry': return <IndustryMetrics />;
      case 'advanced': return <AdvancedAnalytics />;
      case 'comparison': return <DealComparison />;
      default: return null;
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
      {/* ═══ TOP NAV BAR ═══ */}
      <div style={{
        height: '44px',
        display: 'flex',
        alignItems: 'center',
        gap: '12px',
        padding: '0 12px',
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        flexShrink: 0,
      }}>
        {/* Branding */}
        <div style={{
          display: 'flex',
          alignItems: 'center',
          gap: '8px',
          padding: '4px 12px',
          backgroundColor: `${FINCEPT.ORANGE}15`,
          border: `1px solid ${FINCEPT.ORANGE}40`,
          borderRadius: '2px',
        }}>
          <Building2 size={16} color={FINCEPT.ORANGE} />
          <span style={{
            fontSize: '12px',
            fontWeight: TYPOGRAPHY.BOLD,
            color: FINCEPT.ORANGE,
            letterSpacing: '1px',
          }}>
            M&A ANALYTICS
          </span>
        </div>

        {/* Divider */}
        <div style={{ width: '1px', height: '20px', backgroundColor: FINCEPT.BORDER }} />

        {/* Module quick selector */}
        <div style={{ display: 'flex', gap: '2px' }}>
          {MA_MODULES.map((mod) => {
            const Icon = ICONS[mod.id];
            const isActive = activeTab === mod.id;
            return (
              <button
                key={mod.id}
                onClick={() => dispatch({ type: 'SET_TAB', payload: mod.id })}
                title={mod.label}
                style={{
                  display: 'flex',
                  alignItems: 'center',
                  gap: '4px',
                  padding: '4px 8px',
                  backgroundColor: isActive ? `${mod.color}20` : 'transparent',
                  border: isActive ? `1px solid ${mod.color}50` : '1px solid transparent',
                  borderRadius: '2px',
                  color: isActive ? mod.color : FINCEPT.MUTED,
                  fontSize: '9px',
                  fontFamily: TYPOGRAPHY.MONO,
                  fontWeight: isActive ? TYPOGRAPHY.BOLD : TYPOGRAPHY.REGULAR,
                  letterSpacing: '0.5px',
                  cursor: 'pointer',
                  transition: 'all 0.15s',
                  whiteSpace: 'nowrap' as const,
                }}
                onMouseEnter={(e) => {
                  if (!isActive) {
                    e.currentTarget.style.backgroundColor = `${mod.color}10`;
                    e.currentTarget.style.color = mod.color;
                  }
                }}
                onMouseLeave={(e) => {
                  if (!isActive) {
                    e.currentTarget.style.backgroundColor = 'transparent';
                    e.currentTarget.style.color = FINCEPT.MUTED;
                  }
                }}
              >
                <Icon size={12} />
                {mod.shortLabel}
              </button>
            );
          })}
        </div>

        <div style={{ flex: 1 }} />

        {/* Status badge */}
        <div style={{
          display: 'flex',
          alignItems: 'center',
          gap: '4px',
          padding: '3px 8px',
          backgroundColor: `${FINCEPT.GREEN}15`,
          border: `1px solid ${FINCEPT.GREEN}40`,
          borderRadius: '2px',
          fontSize: '9px',
          fontFamily: TYPOGRAPHY.MONO,
          color: FINCEPT.GREEN,
        }}>
          <div style={{
            width: '5px', height: '5px', borderRadius: '50%',
            backgroundColor: FINCEPT.GREEN,
          }} />
          READY
        </div>

        {/* Subtitle */}
        <span style={{ fontSize: '9px', color: FINCEPT.MUTED, letterSpacing: '1px' }}>
          CORPORATE FINANCE TOOLKIT
        </span>
      </div>

      {/* ═══ MAIN BODY (3-panel) ═══ */}
      <div style={{ display: 'flex', flex: 1, minHeight: 0 }}>
        {/* ─── LEFT SIDEBAR ─── */}
        <div style={{
          width: leftPanelOpen ? '200px' : '36px',
          backgroundColor: FINCEPT.PANEL_BG,
          borderRight: `1px solid ${FINCEPT.BORDER}`,
          display: 'flex',
          flexDirection: 'column',
          transition: 'width 0.2s',
          flexShrink: 0,
          overflow: 'hidden',
        }}>
          {/* Sidebar header */}
          <div style={{
            display: 'flex',
            alignItems: 'center',
            justifyContent: leftPanelOpen ? 'space-between' : 'center',
            padding: leftPanelOpen ? '10px 12px' : '10px 8px',
            borderBottom: `1px solid ${FINCEPT.BORDER}`,
          }}>
            {leftPanelOpen && (
              <span style={{
                fontSize: '9px',
                fontWeight: TYPOGRAPHY.BOLD,
                color: FINCEPT.ORANGE,
                letterSpacing: TYPOGRAPHY.WIDE,
              }}>
                MODULES
              </span>
            )}
            <button
              onClick={() => dispatch({ type: 'TOGGLE_LEFT' })}
              style={{
                background: 'none', border: 'none', cursor: 'pointer',
                color: FINCEPT.GRAY, padding: '2px', display: 'flex',
              }}
            >
              {leftPanelOpen ? <PanelLeftClose size={14} /> : <PanelLeftOpen size={14} />}
            </button>
          </div>

          {/* Module list */}
          <div style={{ flex: 1, overflowY: 'auto', padding: leftPanelOpen ? '8px 0' : '8px 0' }}>
            {categories.map((cat) => (
              <React.Fragment key={cat}>
                {/* Category label */}
                {leftPanelOpen && (
                  <div style={{
                    padding: '8px 12px 4px',
                    fontSize: '8px',
                    fontWeight: TYPOGRAPHY.BOLD,
                    color: FINCEPT.MUTED,
                    letterSpacing: '1px',
                  }}>
                    {cat}
                  </div>
                )}
                {/* Module items */}
                {MA_MODULES.filter(m => m.category === cat).map((mod) => {
                  const Icon = ICONS[mod.id];
                  const isActive = activeTab === mod.id;
                  return (
                    <button
                      key={mod.id}
                      onClick={() => dispatch({ type: 'SET_TAB', payload: mod.id })}
                      title={mod.label}
                      style={{
                        display: 'flex',
                        alignItems: 'center',
                        gap: '8px',
                        width: '100%',
                        padding: leftPanelOpen ? '8px 12px' : '8px',
                        justifyContent: leftPanelOpen ? 'flex-start' : 'center',
                        backgroundColor: isActive ? `${mod.color}15` : 'transparent',
                        borderLeft: isActive ? `2px solid ${mod.color}` : '2px solid transparent',
                        border: 'none',
                        borderLeftStyle: 'solid' as const,
                        borderLeftWidth: '2px',
                        borderLeftColor: isActive ? mod.color : 'transparent',
                        color: isActive ? mod.color : FINCEPT.GRAY,
                        fontSize: '10px',
                        fontFamily: TYPOGRAPHY.MONO,
                        fontWeight: isActive ? TYPOGRAPHY.BOLD : TYPOGRAPHY.REGULAR,
                        cursor: 'pointer',
                        transition: 'all 0.15s',
                        textAlign: 'left' as const,
                        whiteSpace: 'nowrap' as const,
                      }}
                      onMouseEnter={(e) => {
                        if (!isActive) {
                          e.currentTarget.style.backgroundColor = `${mod.color}10`;
                          e.currentTarget.style.color = mod.color;
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
                      {leftPanelOpen && <span>{mod.label}</span>}
                    </button>
                  );
                })}
              </React.Fragment>
            ))}
          </div>
        </div>

        {/* ─── CENTER CONTENT ─── */}
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', minWidth: 0, overflow: 'hidden' }}>
          {/* Module header bar */}
          <div style={{
            display: 'flex',
            alignItems: 'center',
            gap: '10px',
            padding: '8px 16px',
            backgroundColor: FINCEPT.HEADER_BG,
            borderBottom: `1px solid ${FINCEPT.BORDER}`,
            flexShrink: 0,
          }}>
            {React.createElement(ICONS[activeTab], { size: 16, color: activeColor })}
            <span style={{
              fontSize: '12px',
              fontWeight: TYPOGRAPHY.BOLD,
              color: activeColor,
              letterSpacing: TYPOGRAPHY.WIDE,
            }}>
              {activeModule.label.toUpperCase()}
            </span>
            <div style={{
              width: '1px', height: '14px', backgroundColor: FINCEPT.BORDER,
            }} />
            <span style={{
              fontSize: '9px', color: FINCEPT.MUTED,
              letterSpacing: '0.5px',
            }}>
              {activeModule.category.toUpperCase()}
            </span>
          </div>

          {/* Content area */}
          <div style={{ flex: 1, overflow: 'hidden' }}>
            {renderContent()}
          </div>
        </div>

        {/* ─── RIGHT SIDEBAR ─── */}
        <div style={{
          width: rightPanelOpen ? '220px' : '36px',
          backgroundColor: FINCEPT.PANEL_BG,
          borderLeft: `1px solid ${FINCEPT.BORDER}`,
          display: 'flex',
          flexDirection: 'column',
          transition: 'width 0.2s',
          flexShrink: 0,
          overflow: 'hidden',
        }}>
          {/* Right sidebar header */}
          <div style={{
            display: 'flex',
            alignItems: 'center',
            justifyContent: rightPanelOpen ? 'space-between' : 'center',
            padding: rightPanelOpen ? '10px 12px' : '10px 8px',
            borderBottom: `1px solid ${FINCEPT.BORDER}`,
          }}>
            {rightPanelOpen && (
              <span style={{
                fontSize: '9px',
                fontWeight: TYPOGRAPHY.BOLD,
                color: FINCEPT.CYAN,
                letterSpacing: TYPOGRAPHY.WIDE,
              }}>
                MODULE INFO
              </span>
            )}
            <button
              onClick={() => dispatch({ type: 'TOGGLE_RIGHT' })}
              style={{
                background: 'none', border: 'none', cursor: 'pointer',
                color: FINCEPT.GRAY, padding: '2px', display: 'flex',
              }}
            >
              {rightPanelOpen ? <PanelRightClose size={14} /> : <PanelRightOpen size={14} />}
            </button>
          </div>

          {rightPanelOpen && (
            <div style={{ flex: 1, overflowY: 'auto', padding: '12px' }}>
              {/* Active module info card */}
              <div style={{
                padding: '12px',
                backgroundColor: `${activeColor}08`,
                border: `1px solid ${activeColor}30`,
                borderLeft: `3px solid ${activeColor}`,
                borderRadius: '2px',
                marginBottom: '16px',
              }}>
                <div style={{
                  display: 'flex', alignItems: 'center', gap: '6px',
                  marginBottom: '8px',
                }}>
                  {React.createElement(ICONS[activeTab], { size: 14, color: activeColor })}
                  <span style={{
                    fontSize: '10px',
                    fontWeight: TYPOGRAPHY.BOLD,
                    color: activeColor,
                  }}>
                    {activeModule.label.toUpperCase()}
                  </span>
                </div>
                <div style={{
                  fontSize: '9px', color: FINCEPT.GRAY,
                  lineHeight: 1.5,
                }}>
                  {activeModule.category} module
                </div>
              </div>

              {/* Capabilities list */}
              <div style={{ marginBottom: '16px' }}>
                <div style={{
                  fontSize: '9px',
                  fontWeight: TYPOGRAPHY.BOLD,
                  color: FINCEPT.ORANGE,
                  letterSpacing: TYPOGRAPHY.WIDE,
                  marginBottom: '8px',
                  display: 'flex', alignItems: 'center', gap: '6px',
                }}>
                  <Layers size={10} />
                  CAPABILITIES
                </div>
                {(MODULE_DESCRIPTIONS[activeTab] || []).map((cap, i) => (
                  <div key={i} style={{
                    display: 'flex',
                    alignItems: 'flex-start',
                    gap: '6px',
                    padding: '4px 0',
                    fontSize: '9px',
                    fontFamily: TYPOGRAPHY.MONO,
                    color: FINCEPT.WHITE,
                    lineHeight: 1.4,
                  }}>
                    <span style={{ color: activeColor, flexShrink: 0, marginTop: '1px' }}>{'>'}</span>
                    {cap}
                  </div>
                ))}
              </div>

              {/* Quick stats */}
              <div style={{
                padding: '10px',
                backgroundColor: FINCEPT.HEADER_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '2px',
              }}>
                <div style={{
                  fontSize: '9px',
                  fontWeight: TYPOGRAPHY.BOLD,
                  color: FINCEPT.CYAN,
                  letterSpacing: TYPOGRAPHY.WIDE,
                  marginBottom: '8px',
                  display: 'flex', alignItems: 'center', gap: '6px',
                }}>
                  <TrendingUp size={10} />
                  QUICK STATS
                </div>
                <div style={{ display: 'flex', flexDirection: 'column', gap: '6px' }}>
                  <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '9px', fontFamily: TYPOGRAPHY.MONO }}>
                    <span style={{ color: FINCEPT.MUTED }}>Total Modules</span>
                    <span style={{ color: FINCEPT.WHITE, fontWeight: TYPOGRAPHY.BOLD }}>8</span>
                  </div>
                  <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '9px', fontFamily: TYPOGRAPHY.MONO }}>
                    <span style={{ color: FINCEPT.MUTED }}>Valuation Methods</span>
                    <span style={{ color: FINCEPT.WHITE, fontWeight: TYPOGRAPHY.BOLD }}>15+</span>
                  </div>
                  <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '9px', fontFamily: TYPOGRAPHY.MONO }}>
                    <span style={{ color: FINCEPT.MUTED }}>Rust Commands</span>
                    <span style={{ color: FINCEPT.WHITE, fontWeight: TYPOGRAPHY.BOLD }}>60+</span>
                  </div>
                  <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '9px', fontFamily: TYPOGRAPHY.MONO }}>
                    <span style={{ color: FINCEPT.MUTED }}>Python Scripts</span>
                    <span style={{ color: FINCEPT.WHITE, fontWeight: TYPOGRAPHY.BOLD }}>30+</span>
                  </div>
                </div>
              </div>

              {/* Keyboard hint */}
              <div style={{
                marginTop: '16px',
                padding: '10px',
                backgroundColor: FINCEPT.HEADER_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '2px',
              }}>
                <div style={{
                  fontSize: '9px',
                  fontWeight: TYPOGRAPHY.BOLD,
                  color: FINCEPT.MUTED,
                  letterSpacing: TYPOGRAPHY.WIDE,
                  marginBottom: '8px',
                  display: 'flex', alignItems: 'center', gap: '6px',
                }}>
                  <Info size={10} />
                  TIPS
                </div>
                <div style={{ fontSize: '8px', color: FINCEPT.MUTED, lineHeight: 1.5 }}>
                  Click sidebar modules to switch views. Collapse panels for more workspace.
                  Each module provides professional-grade analytics with export capabilities.
                </div>
              </div>
            </div>
          )}
        </div>
      </div>

      {/* ═══ STATUS BAR ═══ */}
      <MAStatusBar
        activeModule={activeModule.label.toUpperCase()}
        moduleColor={activeColor}
        statusItems={[
          { label: 'MODULE', value: activeModule.category.toUpperCase() },
          { label: 'ENGINE', value: 'PYTHON + RUST', color: FINCEPT.GREEN },
        ]}
      />
    </div>
  );
};

export default MAAnalyticsTabNew;
