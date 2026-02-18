import React from 'react';
import { useCurrentTime } from '@/contexts/TimezoneContext';
import { APP_VERSION } from '@/constants/version';
import { FINCEPT, COMMON_STYLES } from '../finceptStyles';
import { valColor } from './helpers';
import { formatCurrency } from '../portfolio/utils';
import { useTranslation } from 'react-i18next';
import type { Portfolio, PortfolioSummary } from '../../../../services/portfolio/portfolioService';

interface StatusBarProps {
  selectedPortfolio: Portfolio | null;
  portfolioSummary: PortfolioSummary | null;
  currency: string;
  holdingsCount: number;
}

const StatusBar: React.FC<StatusBarProps> = ({ selectedPortfolio, portfolioSummary, currency, holdingsCount }) => {
  const { t } = useTranslation('portfolio');
  const { formattedTime, timezone } = useCurrentTime();
  const pnl = portfolioSummary?.total_unrealized_pnl ?? 0;
  const dayChg = portfolioSummary?.total_day_change ?? 0;

  return (
    <div style={{
      height: '24px', flexShrink: 0, backgroundColor: FINCEPT.HEADER_BG,
      borderTop: `1px solid ${FINCEPT.BORDER}`, padding: '0 12px',
      display: 'flex', alignItems: 'center', justifyContent: 'space-between',
      fontSize: '9px',
    }}>
      <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
        <span style={{ color: FINCEPT.ORANGE, fontWeight: 800 }}>{t('statusBar.fincept', 'FINCEPT')}</span>
        <span style={{ color: FINCEPT.BORDER }}>|</span>
        <span style={{ color: FINCEPT.GRAY }}>{t('statusBar.portfolioTerminal', 'PORTFOLIO TERMINAL')} v{APP_VERSION}</span>
        {selectedPortfolio && (
          <>
            <span style={{ color: FINCEPT.BORDER }}>|</span>
            <span style={{ color: FINCEPT.CYAN }}>{selectedPortfolio.name?.toUpperCase()}</span>
          </>
        )}
        <span style={{ color: FINCEPT.BORDER }}>|</span>
        <div style={{ display: 'flex', alignItems: 'center', gap: '3px' }}>
          <div style={{ width: '5px', height: '5px', borderRadius: '50%', backgroundColor: FINCEPT.GREEN, boxShadow: `0 0 4px ${FINCEPT.GREEN}`, animation: 'pulse 2s infinite' }} />
          <span style={{ color: FINCEPT.GREEN }}>{t('statusBar.live', 'LIVE')}</span>
        </div>
        {portfolioSummary && (
          <>
            <span style={{ color: FINCEPT.BORDER }}>|</span>
            <span style={{ color: FINCEPT.GRAY }}>{t('statusBar.pos', 'POS')}: <span style={{ color: FINCEPT.WHITE }}>{portfolioSummary.total_positions}</span></span>
            <span style={{ color: FINCEPT.GRAY }}>{t('statusBar.ins', 'INS')}: <span style={{ color: FINCEPT.WHITE }}>{holdingsCount}</span></span>
          </>
        )}
      </div>
      <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
        {portfolioSummary && (
          <>
            <span style={{ color: FINCEPT.GRAY }}>{t('statusBar.nav', 'NAV')}:</span>
            <span style={{ color: FINCEPT.YELLOW, fontWeight: 600 }}>{formatCurrency(portfolioSummary.total_market_value, currency)}</span>
            <span style={{ color: FINCEPT.BORDER }}>|</span>
            <span style={{ color: FINCEPT.GRAY }}>{t('statusBar.pnl', 'P&L')}:</span>
            <span style={{ color: valColor(pnl), fontWeight: 600 }}>{pnl >= 0 ? '+' : ''}{formatCurrency(pnl, currency)}</span>
            <span style={{ color: FINCEPT.BORDER }}>|</span>
            <span style={{ color: FINCEPT.GRAY }}>{t('statusBar.day', 'DAY')}:</span>
            <span style={{ color: valColor(dayChg), fontWeight: 600 }}>{dayChg >= 0 ? '+' : ''}{formatCurrency(dayChg, currency)}</span>
            <span style={{ color: FINCEPT.BORDER }}>|</span>
          </>
        )}
        <span style={{ color: FINCEPT.CYAN, fontWeight: 600 }}>{formattedTime}</span>
        <span style={{ color: FINCEPT.ORANGE, fontWeight: 700 }}>{timezone.shortLabel}</span>
      </div>
    </div>
  );
};

export default StatusBar;
