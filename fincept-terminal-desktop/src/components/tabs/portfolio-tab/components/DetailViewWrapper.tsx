/**
 * DetailViewWrapper — Full-screen container for analytics sub-panels.
 * Replaces the main portfolio layout when a detail view is active.
 * Provides a header bar with back button + title, and renders the selected panel at full size.
 */
import React from 'react';
import { ArrowLeft, Briefcase } from 'lucide-react';
import { FINCEPT } from '../finceptStyles';
import { AnalyticsSectorsPanel, PerformanceRiskPanel, ReportsPanel, RiskManagementPanel, PlanningPanel, EconomicsPanel } from '../views';
import QuantStatsView from '../portfolio/QuantStatsView';
import PortfolioOptimizationView from '../portfolio/PortfolioOptimizationView';
import { CustomIndexView } from '../custom-index';
import type { DetailView } from '../types';
import type { PortfolioSummary, Portfolio, Transaction } from '../../../../services/portfolio/portfolioService';

interface DetailViewWrapperProps {
  view: DetailView;
  onBack: () => void;
  portfolioSummary: PortfolioSummary;
  transactions: Transaction[];
  selectedPortfolio: Portfolio;
  currency: string;
  indexRefreshKey: number;
  onCreateIndex: () => void;
}

const VIEW_LABELS: Record<DetailView, string> = {
  'analytics-sectors': 'ANALYTICS & SECTORS',
  'perf-risk': 'PERFORMANCE & RISK',
  'optimization': 'PORTFOLIO OPTIMIZATION',
  'quantstats': 'QUANTSTATS ANALYSIS',
  'reports-pme': 'REPORTS & PME',
  'indices': 'CUSTOM INDICES',
  'risk-mgmt': 'RISK MANAGEMENT',
  'planning': 'PLANNING & BEHAVIORAL',
  'economics': 'ECONOMICS & MARKETS',
};

const DetailViewWrapper: React.FC<DetailViewWrapperProps> = ({
  view, onBack, portfolioSummary, transactions, selectedPortfolio, currency,
  indexRefreshKey, onCreateIndex,
}) => {
  return (
    <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
      {/* Header bar */}
      <div style={{
        height: '36px', flexShrink: 0,
        background: 'linear-gradient(180deg, #141414 0%, #0A0A0A 100%)',
        borderBottom: `1px solid ${FINCEPT.ORANGE}60`,
        display: 'flex', alignItems: 'center', padding: '0 10px', gap: '10px',
      }}>
        <button
          onClick={onBack}
          style={{
            display: 'flex', alignItems: 'center', gap: '4px',
            padding: '4px 10px', backgroundColor: `${FINCEPT.ORANGE}12`,
            border: `1px solid ${FINCEPT.ORANGE}50`, color: FINCEPT.ORANGE,
            fontSize: '9px', fontWeight: 700, cursor: 'pointer',
            letterSpacing: '0.5px',
          }}
          onMouseEnter={(e) => { e.currentTarget.style.backgroundColor = `${FINCEPT.ORANGE}25`; }}
          onMouseLeave={(e) => { e.currentTarget.style.backgroundColor = `${FINCEPT.ORANGE}12`; }}
        >
          <ArrowLeft size={11} />
          BACK
        </button>

        <div style={{
          width: '1px', height: '16px', backgroundColor: `${FINCEPT.ORANGE}40`,
        }} />

        <span style={{
          fontSize: '11px', fontWeight: 800, color: FINCEPT.ORANGE,
          letterSpacing: '1px',
        }}>
          {VIEW_LABELS[view]}
        </span>

        <div style={{ flex: 1 }} />

        <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
          <Briefcase size={9} color={FINCEPT.GRAY} />
          <span style={{ fontSize: '9px', color: FINCEPT.GRAY, fontWeight: 600 }}>
            {selectedPortfolio.name.toUpperCase()}
          </span>
          <span style={{ fontSize: '8px', color: FINCEPT.MUTED, marginLeft: '2px' }}>
            ({currency})
          </span>
        </div>
      </div>

      {/* Content — full height remaining */}
      <div style={{ flex: 1, overflow: 'visible' }}>
        {view === 'analytics-sectors' && (
          <AnalyticsSectorsPanel portfolioSummary={portfolioSummary} currency={currency} />
        )}
        {view === 'perf-risk' && (
          <PerformanceRiskPanel portfolioSummary={portfolioSummary} currency={currency} />
        )}
        {view === 'optimization' && (
          <PortfolioOptimizationView portfolioSummary={portfolioSummary} />
        )}
        {view === 'quantstats' && (
          <QuantStatsView portfolioSummary={portfolioSummary} />
        )}
        {view === 'reports-pme' && (
          <ReportsPanel portfolioSummary={portfolioSummary} transactions={transactions} currency={currency} />
        )}
        {view === 'indices' && (
          <div style={{ height: '100%', padding: '10px' }}>
            <CustomIndexView key={indexRefreshKey} portfolioId={selectedPortfolio.id} onCreateIndex={onCreateIndex} />
          </div>
        )}
        {view === 'risk-mgmt' && (
          <RiskManagementPanel portfolioSummary={portfolioSummary} currency={currency} />
        )}
        {view === 'planning' && (
          <PlanningPanel portfolioSummary={portfolioSummary} currency={currency} />
        )}
        {view === 'economics' && (
          <EconomicsPanel portfolioSummary={portfolioSummary} currency={currency} />
        )}
      </div>
    </div>
  );
};

export default DetailViewWrapper;
