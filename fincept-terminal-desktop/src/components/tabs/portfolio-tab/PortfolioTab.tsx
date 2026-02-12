/**
 * Portfolio Management Tab - Fincept Terminal Style
 * Three-panel terminal layout with professional portfolio tracking and analytics
 */

import React, { useState, useEffect, useCallback } from 'react';
import { useWorkspaceTabState } from '@/hooks/useWorkspaceTabState';
import { useTranslation } from 'react-i18next';
import { useTerminalTheme } from '@/contexts/ThemeContext';
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
import QuantStatsView from './portfolio/QuantStatsView';
import ActiveManagementView from './portfolio/ActiveManagementView';
import PortfolioOptimizationView from './portfolio/PortfolioOptimizationView';
import PyPMEView from './portfolio/PyPMEView';
import SkfolioView from './portfolio/SkfolioView';
import { CustomIndexView, CreateIndexModal } from './custom-index';
import CreatePortfolioModal from './modals/CreatePortfolioModal';
import AddAssetModal from './modals/AddAssetModal';
import SellAssetModal from './modals/SellAssetModal';
import ConfirmDeleteModal from './modals/ConfirmDeleteModal';
import { usePortfolioOperations } from './hooks/usePortfolioOperations';
import { formatCurrency, formatPercent } from './portfolio/utils';
import { TabFooter } from '@/components/common/TabFooter';
import { showError } from '@/utils/notifications';

type SubTab = 'positions' | 'history' | 'analytics' | 'sectors' | 'performance' | 'risk' | 'reports' | 'alerts' | 'active-mgmt' | 'optimization' | 'indices' | 'quantstats' | 'pme' | 'skfolio';

const PortfolioTab: React.FC = () => {
  const { t } = useTranslation('portfolio');
  const { colors, fontSize, fontFamily } = useTerminalTheme();
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
    exportToCSV,
    updateTransaction,
    deleteTransaction
  } = usePortfolioOperations();

  // Workspace tab state
  const getWorkspaceState = useCallback(() => ({
    activeSubTab,
  }), [activeSubTab]);

  const setWorkspaceState = useCallback((state: any) => {
    if (typeof state.activeSubTab === "string") setActiveSubTab(state.activeSubTab);
  }, []);

  useWorkspaceTabState("portfolio", getWorkspaceState, setWorkspaceState);

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
      showError('Failed to create portfolio', [
        { label: 'ERROR', value: error instanceof Error ? error.message : 'Unknown error' }
      ]);
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
      showError('Failed to add asset', [
        { label: 'ERROR', value: error instanceof Error ? error.message : 'Unknown error' }
      ]);
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
      showError('Failed to sell asset', [
        { label: 'ERROR', value: error instanceof Error ? error.message : 'Unknown error' }
      ]);
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
    { id: 'positions' as SubTab, label: t('views.positions'), icon: Briefcase },
    { id: 'history' as SubTab, label: t('views.history'), icon: Clock },
    { id: 'indices' as SubTab, label: t('views.indices', 'INDICES'), icon: Layers },
    { id: 'analytics' as SubTab, label: t('views.analytics'), icon: BarChart3 },
    { id: 'sectors' as SubTab, label: t('views.sectors'), icon: PieChart },
    { id: 'performance' as SubTab, label: t('views.performance'), icon: TrendingUp },
    { id: 'risk' as SubTab, label: t('views.risk', 'RISK'), icon: AlertCircle },
    { id: 'optimization' as SubTab, label: t('views.optimization', 'OPTIMIZATION'), icon: Target },
    { id: 'active-mgmt' as SubTab, label: t('views.activeMgmt'), icon: Activity },
    { id: 'quantstats' as SubTab, label: t('views.quantstats', 'QUANTSTATS'), icon: Activity },
    { id: 'pme' as SubTab, label: 'PME', icon: TrendingUp },
    { id: 'skfolio' as SubTab, label: 'SKFOLIO', icon: Layers },
    { id: 'reports' as SubTab, label: t('views.reports'), icon: BarChart3 },
    { id: 'alerts' as SubTab, label: t('views.alerts'), icon: AlertCircle }
  ];

  return (
    <div style={{
      height: '100%',
      backgroundColor: colors.background,
      color: colors.text,
      fontFamily,
      overflow: 'hidden',
      display: 'flex',
      flexDirection: 'column',
    }}>
      {/* Top Navigation Bar */}
      <div style={{
        backgroundColor: 'var(--ft-color-header, #1A1A1A)',
        borderBottom: `2px solid ${colors.primary}`,
        padding: '6px 16px',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        flexShrink: 0,
        boxShadow: `0 2px 8px ${colors.primary}20`,
        gap: '8px',
        flexWrap: 'nowrap',
      }}>
        {/* Title */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '6px', flexShrink: 0 }}>
          <Briefcase size={14} color={colors.primary} style={{ filter: `drop-shadow(0 0 4px ${colors.primary})` }} />
          <span style={{
            fontSize: fontSize.body,
            fontWeight: 700,
            color: colors.primary,
            letterSpacing: '0.5px',
            textShadow: `0 0 10px ${colors.primary}40`,
            textTransform: 'uppercase' as const,
          }}>
            {t('views.positions', 'PORTFOLIO')}
          </span>
        </div>

        <div style={{ width: '1px', backgroundColor: 'var(--ft-color-border, #2A2A2A)', alignSelf: 'stretch', height: '16px' }} />

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
                  padding: '6px 12px',
                  backgroundColor: isActive ? colors.primary : 'transparent',
                  color: isActive ? colors.background : colors.textMuted,
                  border: 'none',
                  borderRadius: '2px',
                  fontSize: fontSize.tiny,
                  fontWeight: 700,
                  letterSpacing: '0.5px',
                  cursor: selectedPortfolio ? 'pointer' : 'not-allowed',
                  transition: 'all 0.15s ease',
                  textTransform: 'uppercase' as const,
                  display: 'flex',
                  alignItems: 'center',
                  gap: '3px',
                  fontFamily,
                  opacity: selectedPortfolio ? 1 : 0.5,
                  whiteSpace: 'nowrap',
                  flexShrink: 0,
                }}
                onMouseEnter={(e) => {
                  if (!isActive && selectedPortfolio) {
                    e.currentTarget.style.backgroundColor = 'var(--ft-color-hover, #1F1F1F)';
                    e.currentTarget.style.color = colors.text;
                  }
                }}
                onMouseLeave={(e) => {
                  if (!isActive) {
                    e.currentTarget.style.backgroundColor = 'transparent';
                    e.currentTarget.style.color = selectedPortfolio ? colors.textMuted : '#4A4A4A';
                  }
                }}
              >
                <Icon size={9} />
                {tab.label}
              </button>
            );
          })}
        </div>

        <div style={{ width: '1px', backgroundColor: 'var(--ft-color-border, #2A2A2A)', alignSelf: 'stretch', height: '16px' }} />

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
                  border: `1px solid ${colors.success}`,
                  borderRadius: '2px',
                  color: colors.success,
                  fontSize: fontSize.tiny,
                  fontWeight: 700,
                  fontFamily,
                  letterSpacing: '0.5px',
                  cursor: 'pointer',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '3px',
                  transition: 'all 0.15s ease',
                }}
              >
                <ArrowUpRight size={9} />
                {t('actions.buy', 'BUY')}
              </button>

              <button
                id="portfolio-sell"
                onClick={() => setShowSellAsset(true)}
                style={{
                  padding: '3px 8px',
                  backgroundColor: 'transparent',
                  border: `1px solid ${colors.alert}`,
                  borderRadius: '2px',
                  color: colors.alert,
                  fontSize: fontSize.tiny,
                  fontWeight: 700,
                  fontFamily,
                  letterSpacing: '0.5px',
                  cursor: 'pointer',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '3px',
                  transition: 'all 0.15s ease',
                }}
              >
                <ArrowDownRight size={9} />
                {t('actions.sell', 'SELL')}
              </button>

              <button
                id="portfolio-refresh"
                onClick={refreshPortfolioData}
                disabled={refreshing}
                style={{
                  padding: '3px 6px',
                  backgroundColor: 'transparent',
                  border: '1px solid var(--ft-color-border, #2A2A2A)',
                  borderRadius: '2px',
                  color: refreshing ? '#4A4A4A' : 'var(--ft-color-accent, #00E5FF)',
                  fontSize: fontSize.tiny,
                  cursor: refreshing ? 'wait' : 'pointer',
                  display: 'flex',
                  alignItems: 'center',
                  transition: 'all 0.15s ease',
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
                  border: '1px solid var(--ft-color-border, #2A2A2A)',
                  borderRadius: '2px',
                  color: 'var(--ft-color-accent, #00E5FF)',
                  fontSize: fontSize.tiny,
                  cursor: 'pointer',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '3px',
                  transition: 'all 0.15s ease',
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
              border: `1px solid var(--ft-color-info, #0088FF)`,
              borderRadius: '2px',
              color: 'var(--ft-color-info, #0088FF)',
              fontSize: fontSize.tiny,
              fontWeight: 700,
              fontFamily,
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '2px',
              transition: 'all 0.15s ease',
            }}
            title={t('views.help')}
          >
            <Info size={9} />
            {t('views.help')}
          </button>
        </div>
      </div>

      {/* Three-Panel Layout */}
      <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>
        {/* Left Panel - Portfolio List */}
        <div style={{
          width: '280px',
          flexShrink: 0,
          backgroundColor: colors.panel,
          borderRight: `1px solid var(--ft-color-border, #2A2A2A)`,
          display: 'flex',
          flexDirection: 'column',
          overflow: 'hidden',
        }}>
          {/* Left Panel Header */}
          <div style={{
            padding: '12px',
            backgroundColor: 'var(--ft-color-header, #1A1A1A)',
            borderBottom: `1px solid var(--ft-color-border, #2A2A2A)`,
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'space-between',
          }}>
            <span style={{ color: colors.textMuted, fontSize: fontSize.tiny, fontWeight: 700, letterSpacing: '0.5px', textTransform: 'uppercase' }}>{t('sidebar.portfolios')}</span>
            <button
              id="portfolio-create"
              onClick={() => setShowCreatePortfolio(true)}
              style={{
                padding: '2px 8px',
                backgroundColor: 'transparent',
                border: `1px solid ${colors.primary}`,
                borderRadius: '2px',
                color: colors.primary,
                fontSize: fontSize.tiny,
                fontWeight: 700,
                fontFamily,
                letterSpacing: '0.5px',
                cursor: 'pointer',
                display: 'flex',
                alignItems: 'center',
                gap: '3px',
                transition: 'all 0.15s ease',
              }}
            >
              <Plus size={9} />
              {t('actions.new', 'NEW')}
            </button>
          </div>

          {/* Portfolio List */}
          <div style={{ flex: 1, overflow: 'auto' }}>
            {portfolios.length === 0 ? (
              <div style={{
                display: 'flex',
                flexDirection: 'column',
                alignItems: 'center',
                justifyContent: 'center',
                padding: '24px',
                height: 'auto',
                color: '#4A4A4A',
                fontSize: fontSize.small,
                textAlign: 'center',
              }}>
                <Briefcase size={24} style={{ marginBottom: '8px', opacity: 0.5 }} />
                <span>{t('sidebar.noPortfolios')}</span>
                <span style={{ fontSize: fontSize.tiny, color: colors.textMuted, marginTop: '4px' }}>
                  {t('sidebar.clickCreate')}
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
                      backgroundColor: isSelected ? `${colors.primary}15` : 'transparent',
                      borderLeft: isSelected ? `2px solid ${colors.primary}` : '2px solid transparent',
                      cursor: 'pointer',
                      transition: 'all 0.2s ease',
                      borderBottom: `1px solid var(--ft-color-border, #2A2A2A)`,
                    }}
                    onMouseEnter={(e) => {
                      if (!isSelected) e.currentTarget.style.backgroundColor = 'var(--ft-color-hover, #1F1F1F)';
                    }}
                    onMouseLeave={(e) => {
                      if (!isSelected) e.currentTarget.style.backgroundColor = 'transparent';
                    }}
                  >
                    <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '4px' }}>
                      <span style={{
                        color: isSelected ? colors.primary : colors.text,
                        fontSize: fontSize.small,
                        fontWeight: 700,
                        fontFamily,
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
                          color: '#4A4A4A',
                          cursor: 'pointer',
                          padding: '2px',
                          display: 'flex',
                          transition: 'all 0.15s ease',
                        }}
                        onMouseEnter={(e) => { e.currentTarget.style.color = colors.alert; }}
                        onMouseLeave={(e) => { e.currentTarget.style.color = '#4A4A4A'; }}
                      >
                        <Trash2 size={10} />
                      </button>
                    </div>
                    <div style={{ display: 'flex', gap: '8px' }}>
                      <span style={{ color: colors.textMuted, fontSize: '8px', fontWeight: 700, letterSpacing: '0.5px', textTransform: 'uppercase' }}>
                        {portfolio.currency}
                      </span>
                      {portfolio.owner && (
                        <span style={{ color: colors.textMuted, fontSize: '8px', fontWeight: 700, letterSpacing: '0.5px', textTransform: 'uppercase' }}>
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
              display: 'flex',
              flexDirection: 'column',
              alignItems: 'center',
              justifyContent: 'center',
              height: '100%',
              color: '#4A4A4A',
              fontSize: fontSize.small,
              textAlign: 'center',
              gap: '16px',
            }}>
              <Briefcase size={48} color="#4A4A4A" style={{ opacity: 0.2 }} />
              <div style={{
                color: colors.textMuted,
                fontSize: fontSize.subheading,
                fontWeight: 700,
                letterSpacing: '1px',
                fontFamily,
                textTransform: 'uppercase' as const,
              }}>
                {t('extracted.noPortfolioSelected')}
              </div>
              <div style={{
                color: '#4A4A4A',
                fontSize: fontSize.small,
                fontFamily,
              }}>
                {portfolios.length === 0 ? t('content.selectOrCreate') : t('content.selectOrCreate')}
              </div>
            </div>
          ) : !portfolioSummary ? (
            <div style={{
              display: 'flex',
              flexDirection: 'column',
              alignItems: 'center',
              justifyContent: 'center',
              height: '100%',
              color: '#4A4A4A',
              fontSize: fontSize.small,
              textAlign: 'center',
            }}>
              <div style={{
                color: colors.textMuted,
                fontSize: fontSize.body,
                fontWeight: 600,
                letterSpacing: '1px',
                fontFamily,
              }}>
                {t('extracted.loadingPortfolioData')}
              </div>
            </div>
          ) : (
            <div style={{ flex: 1, overflow: 'hidden' }}>
              {activeSubTab === 'positions' && <div id="positions-view" style={{ height: '100%' }}><PositionsView portfolioSummary={portfolioSummary} /></div>}
              {activeSubTab === 'history' && <div id="history-view" style={{ height: '100%' }}><HistoryView transactions={transactions} currency={currency} onUpdateTransaction={updateTransaction} onDeleteTransaction={deleteTransaction} /></div>}
              {activeSubTab === 'analytics' && <div id="analytics-view" style={{ height: '100%' }}><AnalyticsView portfolioSummary={portfolioSummary} /></div>}
              {activeSubTab === 'sectors' && <div id="sectors-view" style={{ height: '100%' }}><SectorsView portfolioSummary={portfolioSummary} /></div>}
              {activeSubTab === 'performance' && <div id="performance-view" style={{ height: '100%' }}><PerformanceView portfolioSummary={portfolioSummary} /></div>}
              {activeSubTab === 'risk' && <div id="risk-view" style={{ height: '100%' }}><RiskMetricsView portfolioSummary={portfolioSummary} /></div>}
              {activeSubTab === 'optimization' && <div id="optimization-view" style={{ height: '100%' }}><PortfolioOptimizationView portfolioSummary={portfolioSummary} /></div>}
              {activeSubTab === 'quantstats' && <div id="quantstats-view" style={{ height: '100%' }}><QuantStatsView portfolioSummary={portfolioSummary} /></div>}
              {activeSubTab === 'pme' && <div id="pme-view" style={{ height: '100%' }}><PyPMEView portfolioSummary={portfolioSummary} /></div>}
              {activeSubTab === 'skfolio' && <div id="skfolio-view" style={{ height: '100%' }}><SkfolioView portfolioSummary={portfolioSummary} /></div>}
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
                <div id="indices-view" style={{ height: '100%', padding: '12px' }}>
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
          backgroundColor: colors.panel,
          borderLeft: `1px solid var(--ft-color-border, #2A2A2A)`,
          display: 'flex',
          flexDirection: 'column',
          overflow: 'hidden',
        }}>
          {/* Right Panel Header */}
          <div style={{
            padding: '12px',
            backgroundColor: 'var(--ft-color-header, #1A1A1A)',
            borderBottom: `1px solid var(--ft-color-border, #2A2A2A)`,
          }}>
            <span style={{ color: colors.textMuted, fontSize: fontSize.tiny, fontWeight: 700, letterSpacing: '0.5px', textTransform: 'uppercase' }}>{t('summary.portfolioSummary', 'PORTFOLIO SUMMARY')}</span>
          </div>

          {/* Summary Content */}
          <div style={{ flex: 1, overflow: 'auto', padding: '12px' }}>
            {!portfolioSummary ? (
              <div style={{
                display: 'flex',
                flexDirection: 'column',
                alignItems: 'center',
                justifyContent: 'center',
                height: 'auto',
                padding: '24px',
                color: '#4A4A4A',
                fontSize: fontSize.small,
                textAlign: 'center',
              }}>
                <DollarSign size={24} style={{ marginBottom: '8px', opacity: 0.3 }} />
                <span>{t('content.selectPortfolio', 'Select a portfolio')}</span>
              </div>
            ) : (
              <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
                {/* Total Value Card */}
                <div style={{
                  backgroundColor: colors.panel,
                  border: `1px solid ${colors.primary}`,
                  borderRadius: '2px',
                  padding: '12px',
                  display: 'flex',
                  flexDirection: 'column',
                  gap: '4px',
                }}>
                  <div style={{ color: colors.textMuted, fontSize: fontSize.tiny, fontWeight: 700, letterSpacing: '0.5px', textTransform: 'uppercase' }}>{t('summary.totalValue')}</div>
                  <div style={{
                    color: colors.primary,
                    fontSize: fontSize.heading,
                    fontWeight: 700,
                    fontFamily,
                  }}>
                    {formatCurrency(portfolioSummary.total_market_value, currency)}
                  </div>
                </div>

                {/* Total P&L */}
                <div style={{
                  backgroundColor: colors.panel,
                  border: `1px solid ${portfolioSummary.total_unrealized_pnl >= 0 ? colors.success : colors.alert}`,
                  borderRadius: '2px',
                  padding: '12px',
                  display: 'flex',
                  flexDirection: 'column',
                  gap: '4px',
                }}>
                  <div style={{ color: colors.textMuted, fontSize: fontSize.tiny, fontWeight: 700, letterSpacing: '0.5px', textTransform: 'uppercase' }}>{t('summary.unrealizedPnl', 'UNREALIZED P&L')}</div>
                  <div style={{
                    display: 'flex',
                    alignItems: 'center',
                    gap: '4px',
                  }}>
                    {portfolioSummary.total_unrealized_pnl >= 0
                      ? <ArrowUpRight size={14} color={colors.success} />
                      : <ArrowDownRight size={14} color={colors.alert} />
                    }
                    <span style={{
                      color: portfolioSummary.total_unrealized_pnl >= 0 ? colors.success : colors.alert,
                      fontSize: fontSize.subheading,
                      fontWeight: 700,
                      fontFamily,
                    }}>
                      {portfolioSummary.total_unrealized_pnl >= 0 ? '+' : ''}{formatCurrency(portfolioSummary.total_unrealized_pnl, currency)}
                    </span>
                  </div>
                  <div style={{
                    color: portfolioSummary.total_unrealized_pnl_percent >= 0 ? colors.success : colors.alert,
                    fontSize: fontSize.small,
                    fontFamily,
                  }}>
                    {formatPercent(portfolioSummary.total_unrealized_pnl_percent)}
                  </div>
                </div>

                {/* Cost Basis */}
                <div style={{ backgroundColor: colors.panel, border: '1px solid var(--ft-color-border, #2A2A2A)', borderRadius: '2px', padding: '12px', display: 'flex', flexDirection: 'column', gap: '4px' }}>
                  <div style={{ color: colors.textMuted, fontSize: fontSize.tiny, fontWeight: 700, letterSpacing: '0.5px', textTransform: 'uppercase' }}>{t('summary.costBasis', 'COST BASIS')}</div>
                  <div style={{
                    color: 'var(--ft-color-accent, #00E5FF)',
                    fontSize: fontSize.subheading,
                    fontWeight: 700,
                    fontFamily,
                  }}>
                    {formatCurrency(portfolioSummary.total_cost_basis, currency)}
                  </div>
                </div>

                {/* Day Change */}
                <div style={{ backgroundColor: colors.panel, border: '1px solid var(--ft-color-border, #2A2A2A)', borderRadius: '2px', padding: '12px', display: 'flex', flexDirection: 'column', gap: '4px' }}>
                  <div style={{ color: colors.textMuted, fontSize: fontSize.tiny, fontWeight: 700, letterSpacing: '0.5px', textTransform: 'uppercase' }}>{t('summary.dayChange', 'DAY CHANGE')}</div>
                  <div style={{
                    color: portfolioSummary.total_day_change >= 0 ? colors.success : colors.alert,
                    fontSize: fontSize.body,
                    fontWeight: 700,
                    fontFamily,
                  }}>
                    {portfolioSummary.total_day_change >= 0 ? '+' : ''}{formatCurrency(portfolioSummary.total_day_change, currency)}
                    <span style={{ marginLeft: '6px', fontSize: fontSize.small }}>
                      ({formatPercent(portfolioSummary.total_day_change_percent)})
                    </span>
                  </div>
                </div>

                {/* Divider */}
                <div style={{ height: '1px', backgroundColor: 'var(--ft-color-border, #2A2A2A)', margin: '8px 0' }} />

                {/* Quick Stats */}
                <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
                  <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                    <span style={{ color: colors.textMuted, fontSize: fontSize.tiny, fontWeight: 700, letterSpacing: '0.5px', textTransform: 'uppercase' }}>{t('summary.positions')}</span>
                    <span style={{ color: 'var(--ft-color-accent, #00E5FF)', fontSize: fontSize.small, fontFamily }}>{portfolioSummary.total_positions}</span>
                  </div>
                  <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                    <span style={{ color: colors.textMuted, fontSize: fontSize.tiny, fontWeight: 700, letterSpacing: '0.5px', textTransform: 'uppercase' }}>{t('sidebar.currency')}</span>
                    <span style={{ color: 'var(--ft-color-accent, #00E5FF)', fontSize: fontSize.small, fontFamily }}>{currency}</span>
                  </div>
                </div>

                {/* Divider */}
                <div style={{ height: '1px', backgroundColor: 'var(--ft-color-border, #2A2A2A)', margin: '8px 0' }} />

                {/* Top Holdings */}
                <div>
                  <div style={{ color: colors.textMuted, fontSize: fontSize.tiny, fontWeight: 700, letterSpacing: '0.5px', textTransform: 'uppercase', marginBottom: '8px' }}>{t('summary.topHoldings', 'TOP HOLDINGS')}</div>
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
                        borderBottom: `1px solid var(--ft-color-border, #2A2A2A)`,
                      }}>
                        <div>
                          <div style={{ color: 'var(--ft-color-accent, #00E5FF)', fontSize: fontSize.small, fontWeight: 700, fontFamily }}>
                            {holding.symbol}
                          </div>
                          <div style={{ color: colors.textMuted, fontSize: fontSize.tiny, fontFamily }}>
                            {holding.weight.toFixed(1)}%
                          </div>
                        </div>
                        <div style={{ textAlign: 'right' }}>
                          <div style={{
                            color: holding.unrealized_pnl >= 0 ? colors.success : colors.alert,
                            fontSize: fontSize.small,
                            fontWeight: 700,
                            fontFamily,
                          }}>
                            {holding.unrealized_pnl >= 0 ? '+' : ''}{formatCurrency(holding.unrealized_pnl, currency)}
                          </div>
                          <div style={{
                            color: holding.unrealized_pnl_percent >= 0 ? colors.success : colors.alert,
                            fontSize: fontSize.tiny,
                            fontFamily,
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
        tabName={t('title', 'PORTFOLIO MANAGER')}
        backgroundColor="var(--ft-color-header, #1A1A1A)"
        borderColor={colors.primary}
        leftInfo={
          portfolioSummary
            ? [
                {
                  label: t('summary.positions'),
                  value: portfolioSummary.total_positions.toString(),
                  color: colors.text
                },
                {
                  label: t('summary.totalValue'),
                  value: formatCurrency(portfolioSummary.total_market_value, currency),
                  color: colors.text
                },
                {
                  label: t('summary.totalPnl'),
                  value: `${portfolioSummary.total_unrealized_pnl >= 0 ? '+' : ''}${formatCurrency(portfolioSummary.total_unrealized_pnl, currency)}`,
                  color: portfolioSummary.total_unrealized_pnl >= 0 ? colors.success : colors.alert
                }
              ]
            : []
        }
        statusInfo={
          <span style={{ color: colors.textMuted, fontFamily }}>
            {selectedPortfolio ? `${selectedPortfolio.name} | ` : ''}
            {t('status.lastUpdated')}: {currentTime.toLocaleTimeString('en-US', { hour12: false })}
          </span>
        }
      />

      {/* Fincept Terminal CSS */}
      <style>{`
        @import url('https://fonts.googleapis.com/css2?family=IBM+Plex+Mono:wght@400;500;600;700&display=swap');

        *::-webkit-scrollbar {
          width: 6px;
          height: 6px;
        }
        *::-webkit-scrollbar-track {
          background: var(--ft-color-background, #000000);
        }
        *::-webkit-scrollbar-thumb {
          background: var(--ft-color-border, #2A2A2A);
          border-radius: 3px;
        }
        *::-webkit-scrollbar-thumb:hover {
          background: #4A4A4A;
        }

        @keyframes pulse {
          0%, 100% { opacity: 1; }
          50% { opacity: 0.5; }
        }

        @keyframes priceFlash {
          0% { background-color: var(--ft-color-warning, #FFD700)40; }
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
