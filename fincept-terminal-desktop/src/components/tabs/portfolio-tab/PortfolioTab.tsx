/**
 * Portfolio Management Tab - Fincept Terminal Style
 * Three-panel terminal layout with professional portfolio tracking and analytics
 */

import React, { useState, useEffect } from 'react';
import {
  Briefcase, TrendingUp, TrendingDown, DollarSign, BarChart3,
  RefreshCw, Plus, Download, Trash2, PieChart, Activity,
  Clock, AlertCircle, Target, RotateCcw, ArrowUpRight, ArrowDownRight, Info,
  Layers
} from 'lucide-react';
import { createPortfolioTabTour } from '../tours/portfolioTabTour';
import PositionsView from './portfolio/PositionsView';
import HistoryView from './portfolio/HistoryView';
import AnalyticsView from './portfolio/AnalyticsView';
import SectorsView from './portfolio/SectorsView';
import PerformanceView from './portfolio/PerformanceView';
import RiskMetricsView from './portfolio/RiskMetricsView';
import ReportsView from './portfolio/ReportsView';
import AlertsView from './portfolio/AlertsView';
import ActiveManagementView from './portfolio/ActiveManagementView';
import PortfolioOptimizationView from './portfolio/PortfolioOptimizationView';
import { CustomIndexView, CreateIndexModal } from './custom-index';
import CreatePortfolioModal from './modals/CreatePortfolioModal';
import AddAssetModal from './modals/AddAssetModal';
import SellAssetModal from './modals/SellAssetModal';
import ConfirmDeleteModal from './modals/ConfirmDeleteModal';
import { usePortfolioOperations } from './hooks/usePortfolioOperations';
import { formatCurrency, formatPercent } from './portfolio/utils';
import { TabFooter } from '@/components/common/TabFooter';
import { FINCEPT, TYPOGRAPHY, SPACING, BORDERS, EFFECTS, COMMON_STYLES, LAYOUT } from './finceptStyles';

type SubTab = 'positions' | 'history' | 'analytics' | 'sectors' | 'performance' | 'risk' | 'reports' | 'alerts' | 'active-mgmt' | 'optimization' | 'indices';

const PortfolioTab: React.FC = () => {
  const [currentTime, setCurrentTime] = useState(new Date());
  const [activeSubTab, setActiveSubTab] = useState<SubTab>('positions');

  // Modal states
  const [showCreatePortfolio, setShowCreatePortfolio] = useState(false);
  const [showAddAsset, setShowAddAsset] = useState(false);
  const [showSellAsset, setShowSellAsset] = useState(false);
  const [showDeleteConfirm, setShowDeleteConfirm] = useState(false);
  const [portfolioToDelete, setPortfolioToDelete] = useState<string | null>(null);
  const [showCreateIndex, setShowCreateIndex] = useState(false);
  const [indexRefreshKey, setIndexRefreshKey] = useState(0);

  // Form states
  const [newPortfolioName, setNewPortfolioName] = useState('');
  const [newPortfolioOwner, setNewPortfolioOwner] = useState('');
  const [newPortfolioCurrency, setNewPortfolioCurrency] = useState('USD');

  const [addAssetSymbol, setAddAssetSymbol] = useState('');
  const [addAssetQuantity, setAddAssetQuantity] = useState('');
  const [addAssetPrice, setAddAssetPrice] = useState('');

  const [sellAssetSymbol, setSellAssetSymbol] = useState('');
  const [sellAssetQuantity, setSellAssetQuantity] = useState('');
  const [sellAssetPrice, setSellAssetPrice] = useState('');

  // Use custom hook for portfolio operations
  const {
    portfolios,
    selectedPortfolio,
    setSelectedPortfolio,
    portfolioSummary,
    transactions,
    loading,
    refreshing,
    createPortfolio,
    addAsset,
    sellAsset,
    deletePortfolio,
    refreshPortfolioData,
    exportToCSV
  } = usePortfolioOperations();

  // Update time
  useEffect(() => {
    const timer = setInterval(() => {
      setCurrentTime(new Date());
    }, 1000);
    return () => clearInterval(timer);
  }, []);

  // Handle create portfolio
  const handleCreatePortfolio = async () => {
    try {
      await createPortfolio(newPortfolioName, newPortfolioOwner, newPortfolioCurrency);
      setShowCreatePortfolio(false);
      setNewPortfolioName('');
      setNewPortfolioOwner('');
      setNewPortfolioCurrency('USD');
    } catch (error) {
      console.error('[PortfolioTab] Error creating portfolio:', error);
      alert(error instanceof Error ? error.message : 'Failed to create portfolio');
    }
  };

  // Handle add asset
  const handleAddAsset = async () => {
    try {
      await addAsset(addAssetSymbol, addAssetQuantity, addAssetPrice);
      setShowAddAsset(false);
      setAddAssetSymbol('');
      setAddAssetQuantity('');
      setAddAssetPrice('');
    } catch (error) {
      console.error('[PortfolioTab] Error adding asset:', error);
      alert(error instanceof Error ? error.message : 'Failed to add asset');
    }
  };

  // Handle sell asset
  const handleSellAsset = async () => {
    try {
      await sellAsset(sellAssetSymbol, sellAssetQuantity, sellAssetPrice);
      setShowSellAsset(false);
      setSellAssetSymbol('');
      setSellAssetQuantity('');
      setSellAssetPrice('');
    } catch (error) {
      console.error('[PortfolioTab] Error selling asset:', error);
      alert(error instanceof Error ? error.message : 'Failed to sell asset');
    }
  };

  // Handle delete portfolio - show confirmation modal
  const handleDeletePortfolio = (portfolioId: string) => {
    setPortfolioToDelete(portfolioId);
    setShowDeleteConfirm(true);
  };

  // Confirm delete portfolio
  const confirmDeletePortfolio = async () => {
    if (!portfolioToDelete) return;

    try {
      await deletePortfolio(portfolioToDelete);
      setShowDeleteConfirm(false);
      setPortfolioToDelete(null);
    } catch (error) {
      console.error('[PortfolioTab] Error deleting portfolio:', error);
      setShowDeleteConfirm(false);
      setPortfolioToDelete(null);
    }
  };

  const currency = selectedPortfolio?.currency || 'USD';

  // Sub-tab configurations
  const subTabs = [
    { id: 'positions' as SubTab, label: 'POSITIONS', icon: Briefcase },
    { id: 'history' as SubTab, label: 'HISTORY', icon: Clock },
    { id: 'indices' as SubTab, label: 'INDICES', icon: Layers },
    { id: 'analytics' as SubTab, label: 'ANALYTICS', icon: BarChart3 },
    { id: 'sectors' as SubTab, label: 'SECTORS', icon: PieChart },
    { id: 'performance' as SubTab, label: 'PERFORMANCE', icon: TrendingUp },
    { id: 'risk' as SubTab, label: 'RISK', icon: AlertCircle },
    { id: 'optimization' as SubTab, label: 'OPTIMIZATION', icon: Target },
    { id: 'active-mgmt' as SubTab, label: 'ACTIVE MGMT', icon: Activity },
    { id: 'reports' as SubTab, label: 'REPORTS', icon: BarChart3 },
    { id: 'alerts' as SubTab, label: 'ALERTS', icon: AlertCircle }
  ];

  return (
    <div style={{
      ...COMMON_STYLES.container,
    }}>
      {/* Top Navigation Bar */}
      <div style={{
        ...COMMON_STYLES.header,
        height: 'auto',
        padding: '6px 16px',
        gap: '8px',
        flexWrap: 'nowrap',
      }}>
        {/* Title */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '6px', flexShrink: 0 }}>
          <Briefcase size={14} color={FINCEPT.ORANGE} style={{ filter: EFFECTS.ICON_GLOW_ORANGE }} />
          <span style={{
            fontSize: TYPOGRAPHY.BODY,
            fontWeight: TYPOGRAPHY.BOLD,
            color: FINCEPT.ORANGE,
            letterSpacing: TYPOGRAPHY.WIDE,
            textShadow: EFFECTS.ORANGE_GLOW,
            textTransform: 'uppercase' as const,
          }}>
            PORTFOLIO
          </span>
        </div>

        <div style={{ ...COMMON_STYLES.verticalDivider, height: '16px' }} />

        {/* Sub-tab Navigation */}
        <div id="portfolio-subtabs" style={{ display: 'flex', gap: '2px', flex: 1, overflowX: 'auto', overflowY: 'hidden' }}>
          {subTabs.map(tab => {
            const Icon = tab.icon;
            const isActive = activeSubTab === tab.id;
            return (
              <button
                key={tab.id}
                onClick={() => setActiveSubTab(tab.id)}
                disabled={!selectedPortfolio}
                style={{
                  ...COMMON_STYLES.tabButton(isActive),
                  display: 'flex',
                  alignItems: 'center',
                  gap: '3px',
                  padding: '4px 8px',
                  fontFamily: TYPOGRAPHY.MONO,
                  cursor: selectedPortfolio ? 'pointer' : 'not-allowed',
                  opacity: selectedPortfolio ? 1 : 0.5,
                  whiteSpace: 'nowrap',
                  flexShrink: 0,
                }}
                onMouseEnter={(e) => {
                  if (!isActive && selectedPortfolio) {
                    e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                    e.currentTarget.style.color = FINCEPT.WHITE;
                  }
                }}
                onMouseLeave={(e) => {
                  if (!isActive) {
                    e.currentTarget.style.backgroundColor = 'transparent';
                    e.currentTarget.style.color = selectedPortfolio ? FINCEPT.GRAY : FINCEPT.MUTED;
                  }
                }}
              >
                <Icon size={9} />
                {tab.label}
              </button>
            );
          })}
        </div>

        <div style={{ ...COMMON_STYLES.verticalDivider, height: '16px' }} />

        {/* Actions */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '4px', flexShrink: 0 }}>
          {selectedPortfolio && (
            <>
              <button
                id="portfolio-buy"
                onClick={() => setShowAddAsset(true)}
                style={{
                  padding: '3px 8px',
                  backgroundColor: 'transparent',
                  border: `1px solid ${FINCEPT.GREEN}`,
                  borderRadius: '2px',
                  color: FINCEPT.GREEN,
                  fontSize: TYPOGRAPHY.TINY,
                  fontWeight: TYPOGRAPHY.BOLD,
                  fontFamily: TYPOGRAPHY.MONO,
                  letterSpacing: TYPOGRAPHY.WIDE,
                  cursor: 'pointer',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '3px',
                  transition: EFFECTS.TRANSITION_FAST,
                }}
              >
                <ArrowUpRight size={9} />
                BUY
              </button>

              <button
                id="portfolio-sell"
                onClick={() => setShowSellAsset(true)}
                style={{
                  padding: '3px 8px',
                  backgroundColor: 'transparent',
                  border: `1px solid ${FINCEPT.RED}`,
                  borderRadius: '2px',
                  color: FINCEPT.RED,
                  fontSize: TYPOGRAPHY.TINY,
                  fontWeight: TYPOGRAPHY.BOLD,
                  fontFamily: TYPOGRAPHY.MONO,
                  letterSpacing: TYPOGRAPHY.WIDE,
                  cursor: 'pointer',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '3px',
                  transition: EFFECTS.TRANSITION_FAST,
                }}
              >
                <ArrowDownRight size={9} />
                SELL
              </button>

              <button
                id="portfolio-refresh"
                onClick={refreshPortfolioData}
                disabled={refreshing}
                style={{
                  padding: '3px 6px',
                  backgroundColor: 'transparent',
                  border: BORDERS.STANDARD,
                  borderRadius: '2px',
                  color: refreshing ? FINCEPT.MUTED : FINCEPT.CYAN,
                  fontSize: TYPOGRAPHY.TINY,
                  cursor: refreshing ? 'wait' : 'pointer',
                  display: 'flex',
                  alignItems: 'center',
                  transition: EFFECTS.TRANSITION_FAST,
                }}
              >
                <RefreshCw size={9} className={refreshing ? 'animate-spin' : ''} />
              </button>

              <button
                id="portfolio-export"
                onClick={exportToCSV}
                style={{
                  padding: '3px 6px',
                  backgroundColor: 'transparent',
                  border: BORDERS.STANDARD,
                  borderRadius: '2px',
                  color: FINCEPT.CYAN,
                  fontSize: TYPOGRAPHY.TINY,
                  cursor: 'pointer',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '3px',
                  transition: EFFECTS.TRANSITION_FAST,
                }}
              >
                <Download size={9} />
              </button>
            </>
          )}

          <button
            onClick={() => {
              const tour = createPortfolioTabTour((tab: string) => setActiveSubTab(tab as any));
              tour.drive();
            }}
            style={{
              padding: '3px 6px',
              backgroundColor: 'transparent',
              border: `1px solid ${FINCEPT.BLUE}`,
              borderRadius: '2px',
              color: FINCEPT.BLUE,
              fontSize: TYPOGRAPHY.TINY,
              fontWeight: TYPOGRAPHY.BOLD,
              fontFamily: TYPOGRAPHY.MONO,
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '2px',
              transition: EFFECTS.TRANSITION_FAST,
            }}
            title="Start portfolio tour"
          >
            <Info size={9} />
            HELP
          </button>
        </div>
      </div>

      {/* Three-Panel Layout */}
      <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>
        {/* Left Panel - Portfolio List */}
        <div style={{
          width: '280px',
          flexShrink: 0,
          backgroundColor: FINCEPT.PANEL_BG,
          borderRight: `1px solid ${FINCEPT.BORDER}`,
          display: 'flex',
          flexDirection: 'column',
          overflow: 'hidden',
        }}>
          {/* Left Panel Header */}
          <div style={{
            padding: SPACING.DEFAULT,
            backgroundColor: FINCEPT.HEADER_BG,
            borderBottom: `1px solid ${FINCEPT.BORDER}`,
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'space-between',
          }}>
            <span style={{ ...COMMON_STYLES.dataLabel }}>PORTFOLIOS</span>
            <button
              id="portfolio-create"
              onClick={() => setShowCreatePortfolio(true)}
              style={{
                padding: '2px 8px',
                backgroundColor: 'transparent',
                border: `1px solid ${FINCEPT.ORANGE}`,
                borderRadius: '2px',
                color: FINCEPT.ORANGE,
                fontSize: TYPOGRAPHY.TINY,
                fontWeight: TYPOGRAPHY.BOLD,
                fontFamily: TYPOGRAPHY.MONO,
                letterSpacing: TYPOGRAPHY.WIDE,
                cursor: 'pointer',
                display: 'flex',
                alignItems: 'center',
                gap: '3px',
                transition: EFFECTS.TRANSITION_FAST,
              }}
            >
              <Plus size={9} />
              NEW
            </button>
          </div>

          {/* Portfolio List */}
          <div style={{ flex: 1, overflow: 'auto' }}>
            {portfolios.length === 0 ? (
              <div style={{
                ...COMMON_STYLES.emptyState,
                padding: SPACING.XLARGE,
                height: 'auto',
              }}>
                <Briefcase size={24} style={{ marginBottom: '8px', opacity: 0.5 }} />
                <span>No portfolios yet</span>
                <span style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY, marginTop: '4px' }}>
                  Click NEW to create one
                </span>
              </div>
            ) : (
              portfolios.map(portfolio => {
                const isSelected = selectedPortfolio?.id === portfolio.id;
                return (
                  <div
                    key={portfolio.id}
                    onClick={() => setSelectedPortfolio(portfolio)}
                    style={{
                      padding: '10px 12px',
                      backgroundColor: isSelected ? `${FINCEPT.ORANGE}15` : 'transparent',
                      borderLeft: isSelected ? `2px solid ${FINCEPT.ORANGE}` : '2px solid transparent',
                      cursor: 'pointer',
                      transition: EFFECTS.TRANSITION_STANDARD,
                      borderBottom: `1px solid ${FINCEPT.BORDER}`,
                    }}
                    onMouseEnter={(e) => {
                      if (!isSelected) e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                    }}
                    onMouseLeave={(e) => {
                      if (!isSelected) e.currentTarget.style.backgroundColor = 'transparent';
                    }}
                  >
                    <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '4px' }}>
                      <span style={{
                        color: isSelected ? FINCEPT.ORANGE : FINCEPT.WHITE,
                        fontSize: TYPOGRAPHY.SMALL,
                        fontWeight: TYPOGRAPHY.BOLD,
                        fontFamily: TYPOGRAPHY.MONO,
                      }}>
                        {portfolio.name}
                      </span>
                      <button
                        onClick={(e) => {
                          e.stopPropagation();
                          handleDeletePortfolio(portfolio.id);
                        }}
                        style={{
                          background: 'none',
                          border: 'none',
                          color: FINCEPT.MUTED,
                          cursor: 'pointer',
                          padding: '2px',
                          display: 'flex',
                          transition: EFFECTS.TRANSITION_FAST,
                        }}
                        onMouseEnter={(e) => { e.currentTarget.style.color = FINCEPT.RED; }}
                        onMouseLeave={(e) => { e.currentTarget.style.color = FINCEPT.MUTED; }}
                      >
                        <Trash2 size={10} />
                      </button>
                    </div>
                    <div style={{ display: 'flex', gap: '8px' }}>
                      <span style={{ ...COMMON_STYLES.dataLabel, fontSize: '8px' }}>
                        {portfolio.currency}
                      </span>
                      {portfolio.owner && (
                        <span style={{ ...COMMON_STYLES.dataLabel, fontSize: '8px' }}>
                          {portfolio.owner}
                        </span>
                      )}
                    </div>
                  </div>
                );
              })
            )}
          </div>
        </div>

        {/* Center Content */}
        <div style={{ flex: 1, overflow: 'hidden', display: 'flex', flexDirection: 'column' }}>
          {!selectedPortfolio ? (
            <div style={{
              ...COMMON_STYLES.emptyState,
              gap: SPACING.LARGE,
            }}>
              <Briefcase size={48} color={FINCEPT.MUTED} style={{ opacity: 0.2 }} />
              <div style={{
                color: FINCEPT.GRAY,
                fontSize: TYPOGRAPHY.SUBHEADING,
                fontWeight: TYPOGRAPHY.BOLD,
                letterSpacing: '1px',
                fontFamily: TYPOGRAPHY.MONO,
                textTransform: 'uppercase' as const,
              }}>
                NO PORTFOLIO SELECTED
              </div>
              <div style={{
                color: FINCEPT.MUTED,
                fontSize: TYPOGRAPHY.SMALL,
                fontFamily: TYPOGRAPHY.MONO,
              }}>
                {portfolios.length === 0 ? 'Create a new portfolio to get started' : 'Select a portfolio from the left panel'}
              </div>
            </div>
          ) : !portfolioSummary ? (
            <div style={{
              ...COMMON_STYLES.emptyState,
            }}>
              <div style={{
                color: FINCEPT.GRAY,
                fontSize: TYPOGRAPHY.BODY,
                fontWeight: TYPOGRAPHY.SEMIBOLD,
                letterSpacing: '1px',
                fontFamily: TYPOGRAPHY.MONO,
              }}>
                LOADING PORTFOLIO DATA...
              </div>
            </div>
          ) : (
            <div style={{ flex: 1, overflow: 'hidden' }}>
              {activeSubTab === 'positions' && <div id="positions-view" style={{ height: '100%' }}><PositionsView portfolioSummary={portfolioSummary} /></div>}
              {activeSubTab === 'history' && <div id="history-view" style={{ height: '100%' }}><HistoryView transactions={transactions} currency={currency} /></div>}
              {activeSubTab === 'analytics' && <div id="analytics-view" style={{ height: '100%' }}><AnalyticsView portfolioSummary={portfolioSummary} /></div>}
              {activeSubTab === 'sectors' && <div id="sectors-view" style={{ height: '100%' }}><SectorsView portfolioSummary={portfolioSummary} /></div>}
              {activeSubTab === 'performance' && <div id="performance-view" style={{ height: '100%' }}><PerformanceView portfolioSummary={portfolioSummary} /></div>}
              {activeSubTab === 'risk' && <div id="risk-view" style={{ height: '100%' }}><RiskMetricsView portfolioSummary={portfolioSummary} /></div>}
              {activeSubTab === 'optimization' && <div id="optimization-view" style={{ height: '100%' }}><PortfolioOptimizationView portfolioSummary={portfolioSummary} /></div>}
              {activeSubTab === 'reports' && <div id="reports-view" style={{ height: '100%' }}><ReportsView portfolioSummary={portfolioSummary} transactions={transactions} /></div>}
              {activeSubTab === 'alerts' && <div id="alerts-view" style={{ height: '100%' }}><AlertsView portfolioSummary={portfolioSummary} /></div>}
              {activeSubTab === 'active-mgmt' && (
                <div id="active-mgmt-view" style={{ height: '100%' }}>
                  <ActiveManagementView
                    portfolioId={selectedPortfolio.id}
                    portfolioData={{
                      returns: portfolioSummary.holdings.length > 0
                        ? portfolioSummary.holdings.map(h => (h.day_change_percent || 0) / 100)
                        : [],
                      weights: portfolioSummary.holdings.map(h => h.weight / 100)
                    }}
                  />
                </div>
              )}
              {activeSubTab === 'indices' && (
                <div id="indices-view" style={{ height: '100%', padding: SPACING.DEFAULT }}>
                  <CustomIndexView
                    key={indexRefreshKey}
                    portfolioId={selectedPortfolio?.id}
                    onCreateIndex={() => setShowCreateIndex(true)}
                  />
                </div>
              )}
            </div>
          )}
        </div>

        {/* Right Panel - Quick Summary */}
        <div style={{
          width: '300px',
          flexShrink: 0,
          backgroundColor: FINCEPT.PANEL_BG,
          borderLeft: `1px solid ${FINCEPT.BORDER}`,
          display: 'flex',
          flexDirection: 'column',
          overflow: 'hidden',
        }}>
          {/* Right Panel Header */}
          <div style={{
            padding: SPACING.DEFAULT,
            backgroundColor: FINCEPT.HEADER_BG,
            borderBottom: `1px solid ${FINCEPT.BORDER}`,
          }}>
            <span style={{ ...COMMON_STYLES.dataLabel }}>PORTFOLIO SUMMARY</span>
          </div>

          {/* Summary Content */}
          <div style={{ flex: 1, overflow: 'auto', padding: SPACING.DEFAULT }}>
            {!portfolioSummary ? (
              <div style={{
                ...COMMON_STYLES.emptyState,
                height: 'auto',
                padding: SPACING.XLARGE,
              }}>
                <DollarSign size={24} style={{ marginBottom: '8px', opacity: 0.3 }} />
                <span>Select a portfolio</span>
              </div>
            ) : (
              <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.MEDIUM }}>
                {/* Total Value Card */}
                <div style={{
                  ...COMMON_STYLES.metricCard,
                  borderColor: FINCEPT.ORANGE,
                  padding: SPACING.DEFAULT,
                }}>
                  <div style={{ ...COMMON_STYLES.dataLabel }}>TOTAL VALUE</div>
                  <div style={{
                    color: FINCEPT.ORANGE,
                    fontSize: TYPOGRAPHY.HEADING,
                    fontWeight: TYPOGRAPHY.BOLD,
                    fontFamily: TYPOGRAPHY.MONO,
                  }}>
                    {formatCurrency(portfolioSummary.total_market_value, currency)}
                  </div>
                </div>

                {/* Total P&L */}
                <div style={{
                  ...COMMON_STYLES.metricCard,
                  borderColor: portfolioSummary.total_unrealized_pnl >= 0 ? FINCEPT.GREEN : FINCEPT.RED,
                  padding: SPACING.DEFAULT,
                }}>
                  <div style={{ ...COMMON_STYLES.dataLabel }}>UNREALIZED P&L</div>
                  <div style={{
                    display: 'flex',
                    alignItems: 'center',
                    gap: '4px',
                  }}>
                    {portfolioSummary.total_unrealized_pnl >= 0
                      ? <ArrowUpRight size={14} color={FINCEPT.GREEN} />
                      : <ArrowDownRight size={14} color={FINCEPT.RED} />
                    }
                    <span style={{
                      color: portfolioSummary.total_unrealized_pnl >= 0 ? FINCEPT.GREEN : FINCEPT.RED,
                      fontSize: TYPOGRAPHY.SUBHEADING,
                      fontWeight: TYPOGRAPHY.BOLD,
                      fontFamily: TYPOGRAPHY.MONO,
                    }}>
                      {portfolioSummary.total_unrealized_pnl >= 0 ? '+' : ''}{formatCurrency(portfolioSummary.total_unrealized_pnl, currency)}
                    </span>
                  </div>
                  <div style={{
                    color: portfolioSummary.total_unrealized_pnl_percent >= 0 ? FINCEPT.GREEN : FINCEPT.RED,
                    fontSize: TYPOGRAPHY.SMALL,
                    fontFamily: TYPOGRAPHY.MONO,
                  }}>
                    {formatPercent(portfolioSummary.total_unrealized_pnl_percent)}
                  </div>
                </div>

                {/* Cost Basis */}
                <div style={{ ...COMMON_STYLES.metricCard, padding: SPACING.DEFAULT }}>
                  <div style={{ ...COMMON_STYLES.dataLabel }}>COST BASIS</div>
                  <div style={{
                    color: FINCEPT.CYAN,
                    fontSize: TYPOGRAPHY.SUBHEADING,
                    fontWeight: TYPOGRAPHY.BOLD,
                    fontFamily: TYPOGRAPHY.MONO,
                  }}>
                    {formatCurrency(portfolioSummary.total_cost_basis, currency)}
                  </div>
                </div>

                {/* Day Change */}
                <div style={{ ...COMMON_STYLES.metricCard, padding: SPACING.DEFAULT }}>
                  <div style={{ ...COMMON_STYLES.dataLabel }}>DAY CHANGE</div>
                  <div style={{
                    color: portfolioSummary.total_day_change >= 0 ? FINCEPT.GREEN : FINCEPT.RED,
                    fontSize: TYPOGRAPHY.BODY,
                    fontWeight: TYPOGRAPHY.BOLD,
                    fontFamily: TYPOGRAPHY.MONO,
                  }}>
                    {portfolioSummary.total_day_change >= 0 ? '+' : ''}{formatCurrency(portfolioSummary.total_day_change, currency)}
                    <span style={{ marginLeft: '6px', fontSize: TYPOGRAPHY.SMALL }}>
                      ({formatPercent(portfolioSummary.total_day_change_percent)})
                    </span>
                  </div>
                </div>

                {/* Divider */}
                <div style={COMMON_STYLES.divider} />

                {/* Quick Stats */}
                <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.MEDIUM }}>
                  <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                    <span style={{ ...COMMON_STYLES.dataLabel }}>POSITIONS</span>
                    <span style={{ ...COMMON_STYLES.dataValue }}>{portfolioSummary.total_positions}</span>
                  </div>
                  <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                    <span style={{ ...COMMON_STYLES.dataLabel }}>CURRENCY</span>
                    <span style={{ ...COMMON_STYLES.dataValue }}>{currency}</span>
                  </div>
                </div>

                {/* Divider */}
                <div style={COMMON_STYLES.divider} />

                {/* Top Holdings */}
                <div>
                  <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.MEDIUM }}>TOP HOLDINGS</div>
                  {portfolioSummary.holdings
                    .slice()
                    .sort((a, b) => b.weight - a.weight)
                    .slice(0, 5)
                    .map(holding => (
                      <div key={holding.id} style={{
                        display: 'flex',
                        justifyContent: 'space-between',
                        alignItems: 'center',
                        padding: '4px 0',
                        borderBottom: `1px solid ${FINCEPT.BORDER}`,
                      }}>
                        <div>
                          <div style={{ color: FINCEPT.CYAN, fontSize: TYPOGRAPHY.SMALL, fontWeight: TYPOGRAPHY.BOLD, fontFamily: TYPOGRAPHY.MONO }}>
                            {holding.symbol}
                          </div>
                          <div style={{ color: FINCEPT.GRAY, fontSize: TYPOGRAPHY.TINY, fontFamily: TYPOGRAPHY.MONO }}>
                            {holding.weight.toFixed(1)}%
                          </div>
                        </div>
                        <div style={{ textAlign: 'right' }}>
                          <div style={{
                            color: holding.unrealized_pnl >= 0 ? FINCEPT.GREEN : FINCEPT.RED,
                            fontSize: TYPOGRAPHY.SMALL,
                            fontWeight: TYPOGRAPHY.BOLD,
                            fontFamily: TYPOGRAPHY.MONO,
                          }}>
                            {holding.unrealized_pnl >= 0 ? '+' : ''}{formatCurrency(holding.unrealized_pnl, currency)}
                          </div>
                          <div style={{
                            color: holding.unrealized_pnl_percent >= 0 ? FINCEPT.GREEN : FINCEPT.RED,
                            fontSize: TYPOGRAPHY.TINY,
                            fontFamily: TYPOGRAPHY.MONO,
                          }}>
                            {formatPercent(holding.unrealized_pnl_percent)}
                          </div>
                        </div>
                      </div>
                    ))
                  }
                </div>
              </div>
            )}
          </div>
        </div>
      </div>

      {/* Status Bar (Bottom) */}
      <TabFooter
        tabName="PORTFOLIO MANAGER"
        backgroundColor={FINCEPT.HEADER_BG}
        borderColor={FINCEPT.ORANGE}
        leftInfo={
          portfolioSummary
            ? [
                {
                  label: 'Positions',
                  value: portfolioSummary.total_positions.toString(),
                  color: FINCEPT.WHITE
                },
                {
                  label: 'Total Value',
                  value: formatCurrency(portfolioSummary.total_market_value, currency),
                  color: FINCEPT.WHITE
                },
                {
                  label: 'Total P&L',
                  value: `${portfolioSummary.total_unrealized_pnl >= 0 ? '+' : ''}${formatCurrency(portfolioSummary.total_unrealized_pnl, currency)}`,
                  color: portfolioSummary.total_unrealized_pnl >= 0 ? FINCEPT.GREEN : FINCEPT.RED
                }
              ]
            : []
        }
        statusInfo={
          <span style={{ color: FINCEPT.GRAY, fontFamily: TYPOGRAPHY.MONO }}>
            {selectedPortfolio ? `${selectedPortfolio.name} | ` : ''}
            Last Updated: {currentTime.toLocaleTimeString('en-US', { hour12: false })}
          </span>
        }
      />

      {/* Fincept Terminal CSS */}
      <style>{`
        @import url('https://fonts.googleapis.com/css2?family=IBM+Plex+Mono:wght@400;500;600;700&display=swap');

        ${COMMON_STYLES.scrollbarCSS}

        @keyframes pulse {
          0%, 100% { opacity: 1; }
          50% { opacity: 0.5; }
        }

        @keyframes priceFlash {
          0% { background-color: ${FINCEPT.YELLOW}40; }
          100% { background-color: transparent; }
        }
      `}</style>

      {/* Modals */}
      <CreatePortfolioModal
        show={showCreatePortfolio}
        formState={{
          name: newPortfolioName,
          owner: newPortfolioOwner,
          currency: newPortfolioCurrency
        }}
        onNameChange={setNewPortfolioName}
        onOwnerChange={setNewPortfolioOwner}
        onCurrencyChange={setNewPortfolioCurrency}
        onClose={() => setShowCreatePortfolio(false)}
        onCreate={handleCreatePortfolio}
      />

      <AddAssetModal
        show={showAddAsset}
        formState={{
          symbol: addAssetSymbol,
          quantity: addAssetQuantity,
          price: addAssetPrice
        }}
        onSymbolChange={setAddAssetSymbol}
        onQuantityChange={setAddAssetQuantity}
        onPriceChange={setAddAssetPrice}
        onClose={() => setShowAddAsset(false)}
        onAdd={handleAddAsset}
      />

      <SellAssetModal
        show={showSellAsset}
        formState={{
          symbol: sellAssetSymbol,
          quantity: sellAssetQuantity,
          price: sellAssetPrice
        }}
        onSymbolChange={setSellAssetSymbol}
        onQuantityChange={setSellAssetQuantity}
        onPriceChange={setSellAssetPrice}
        onClose={() => setShowSellAsset(false)}
        onSell={handleSellAsset}
      />

      <ConfirmDeleteModal
        show={showDeleteConfirm}
        portfolioName={portfolios.find(p => p.id === portfolioToDelete)?.name || ''}
        onClose={() => {
          setShowDeleteConfirm(false);
          setPortfolioToDelete(null);
        }}
        onConfirm={confirmDeletePortfolio}
      />

      <CreateIndexModal
        isOpen={showCreateIndex}
        onClose={() => setShowCreateIndex(false)}
        onCreated={() => {
          setShowCreateIndex(false);
          setIndexRefreshKey(prev => prev + 1);
        }}
        portfolioSummary={portfolioSummary}
      />
    </div>
  );
};

export default PortfolioTab;
