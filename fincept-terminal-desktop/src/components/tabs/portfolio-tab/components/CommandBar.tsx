import React, { useState, useMemo } from 'react';
import {
  Briefcase, ChevronDown, Search, ArrowUpRight, ArrowDownRight,
  RefreshCw, Download, Upload, Timer, Plus, Trash2, Zap, Bot,
  BarChart2, TrendingUp, Settings2, Activity, FileText, Layers,
  Shield, Calendar, Globe,
} from 'lucide-react';
import { FINCEPT, COMMON_STYLES } from '../finceptStyles';
import { valColor } from './helpers';
import { formatCurrency, formatPercent } from '../portfolio/utils';
import { useTranslation } from 'react-i18next';
import { REFRESH_OPTIONS } from '../hooks/usePortfolioOperations';
import type { Portfolio, PortfolioSummary } from '../../../../services/portfolio/portfolioService';
import type { DetailView } from '../types';

interface CommandBarProps {
  portfolios: Portfolio[];
  selectedPortfolio: Portfolio | null;
  onSelectPortfolio: (p: Portfolio) => void;
  portfolioSummary: PortfolioSummary | null;
  onCreatePortfolio: () => void;
  onDeletePortfolio: (id: string) => void;
  onBuy: () => void;
  onSell: () => void;
  onRefresh: () => void;
  onExport: () => void;
  onExportJSON: () => void;
  onImport: () => void;
  refreshing: boolean;
  currency: string;
  refreshIntervalMs: number;
  onSetRefreshInterval: (ms: number) => void;
  showFFN?: boolean;
  onToggleFFN?: () => void;
  onAnalyzeWithAI?: () => void;
  aiAnalyzing?: boolean;
  detailView?: DetailView | null;
  onSetDetailView?: (view: DetailView | null) => void;
}

const DETAIL_VIEW_BUTTONS: { view: DetailView; label: string; icon: React.ElementType; color: string }[] = [
  { view: 'analytics-sectors', label: 'SECTORS', icon: BarChart2, color: FINCEPT.CYAN },
  { view: 'perf-risk', label: 'PERF/RISK', icon: TrendingUp, color: '#22C55E' },
  { view: 'optimization', label: 'OPTIMIZE', icon: Settings2, color: '#F59E0B' },
  { view: 'quantstats', label: 'QUANTSTATS', icon: Activity, color: '#8B5CF6' },
  { view: 'reports-pme', label: 'REPORTS', icon: FileText, color: '#06B6D4' },
  { view: 'indices', label: 'INDICES', icon: Layers, color: '#EC4899' },
  { view: 'risk-mgmt', label: 'RISK', icon: Shield, color: '#EF4444' },
  { view: 'planning', label: 'PLANNING', icon: Calendar, color: '#10B981' },
  { view: 'economics', label: 'ECONOMICS', icon: Globe, color: '#6366F1' },
];

const CommandBar: React.FC<CommandBarProps> = ({
  portfolios, selectedPortfolio, onSelectPortfolio, portfolioSummary,
  onCreatePortfolio, onDeletePortfolio, onBuy, onSell, onRefresh, onExport, onExportJSON, onImport, refreshing, currency,
  refreshIntervalMs, onSetRefreshInterval, showFFN, onToggleFFN, onAnalyzeWithAI, aiAnalyzing,
  detailView, onSetDetailView,
}) => {
  const { t } = useTranslation('portfolio');
  const [showDropdown, setShowDropdown] = useState(false);
  const [showIntervalDropdown, setShowIntervalDropdown] = useState(false);
  const [searchTerm, setSearchTerm] = useState('');

  const currentIntervalLabel = REFRESH_OPTIONS.find(o => o.ms === refreshIntervalMs)?.label
    ?? `${Math.round(refreshIntervalMs / 1000)}s`;

  const s = portfolioSummary;
  const pnl = s?.total_unrealized_pnl ?? 0;
  const dayChg = s?.total_day_change ?? 0;
  const dayChgPct = s?.total_day_change_percent ?? 0;
  const gainers = s?.holdings.filter(h => h.unrealized_pnl > 0).length ?? 0;
  const losers = s?.holdings.filter(h => h.unrealized_pnl < 0).length ?? 0;

  const filteredPortfolios = useMemo(() => {
    if (!searchTerm) return portfolios;
    return portfolios.filter(p => p.name.toLowerCase().includes(searchTerm.toLowerCase()));
  }, [portfolios, searchTerm]);

  return (
    <div style={{
      height: '40px', flexShrink: 0,
      background: 'linear-gradient(180deg, #141414 0%, #0A0A0A 100%)',
      borderBottom: `2px solid ${FINCEPT.ORANGE}`,
      display: 'flex', alignItems: 'center', padding: '0 10px', gap: '8px',
      position: 'relative',
    }}>
      {/* Portfolio Selector */}
      <div
        id="portfolio-selector"
        onClick={() => setShowDropdown(!showDropdown)}
        style={{
          display: 'flex', alignItems: 'center', gap: '6px',
          padding: '4px 10px', backgroundColor: '#0A0A0A',
          border: `1px solid ${FINCEPT.ORANGE}`, cursor: 'pointer', flexShrink: 0,
        }}
      >
        <Briefcase size={10} color={FINCEPT.ORANGE} />
        <span style={{ fontSize: '12px', fontWeight: 800, color: FINCEPT.ORANGE, letterSpacing: '1px' }}>
          {selectedPortfolio?.name?.toUpperCase() || t('commandBar.selectPortfolio', 'SELECT PORTFOLIO')}
        </span>
        <ChevronDown size={11} color={FINCEPT.ORANGE} />
      </div>

      {/* Dropdown */}
      {showDropdown && (
        <div style={{
          position: 'absolute', top: '38px', left: '10px', zIndex: 100,
          backgroundColor: '#0A0A0A', border: `1px solid ${FINCEPT.ORANGE}`,
          minWidth: '250px', maxHeight: '300px', overflow: 'auto',
          boxShadow: `0 4px 12px rgba(0,0,0,0.8)`,
        }}>
          <div style={{ padding: '6px', borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '4px', padding: '4px 6px', backgroundColor: '#050505', border: `1px solid ${FINCEPT.BORDER}` }}>
              <Search size={9} color={FINCEPT.GRAY} />
              <input
                placeholder={t('commandBar.searchPortfolios', 'Search portfolios...')}
                value={searchTerm}
                onChange={(e) => setSearchTerm(e.target.value)}
                autoFocus
                style={{ flex: 1, background: 'none', border: 'none', outline: 'none', color: FINCEPT.WHITE, fontSize: '10px' }}
              />
            </div>
          </div>
          {filteredPortfolios.map(p => (
            <div
              key={p.id}
              onClick={() => { onSelectPortfolio(p); setShowDropdown(false); setSearchTerm(''); }}
              style={{
                padding: '8px 10px', cursor: 'pointer',
                backgroundColor: selectedPortfolio?.id === p.id ? `${FINCEPT.ORANGE}12` : 'transparent',
                borderLeft: selectedPortfolio?.id === p.id ? `3px solid ${FINCEPT.ORANGE}` : '3px solid transparent',
                borderBottom: `1px solid ${FINCEPT.BORDER}20`,
                display: 'flex', justifyContent: 'space-between', alignItems: 'center',
              }}
              onMouseEnter={(e) => { e.currentTarget.style.backgroundColor = FINCEPT.HOVER; }}
              onMouseLeave={(e) => { e.currentTarget.style.backgroundColor = selectedPortfolio?.id === p.id ? `${FINCEPT.ORANGE}12` : 'transparent'; }}
            >
              <div>
                <div style={{ fontSize: '10px', fontWeight: 700, color: selectedPortfolio?.id === p.id ? FINCEPT.ORANGE : FINCEPT.WHITE }}>{p.name}</div>
                <div style={{ fontSize: '8px', color: FINCEPT.CYAN }}>{p.currency}</div>
              </div>
              <button
                onClick={(e) => { e.stopPropagation(); onDeletePortfolio(p.id); setShowDropdown(false); }}
                style={{ background: 'none', border: 'none', color: FINCEPT.MUTED, cursor: 'pointer', padding: '2px', display: 'flex' }}
                onMouseEnter={(e) => { e.currentTarget.style.color = FINCEPT.RED; }}
                onMouseLeave={(e) => { e.currentTarget.style.color = FINCEPT.MUTED; }}
              >
                <Trash2 size={9} />
              </button>
            </div>
          ))}
          <div
            id="portfolio-create"
            onClick={() => { onCreatePortfolio(); setShowDropdown(false); }}
            style={{ padding: '8px 10px', cursor: 'pointer', display: 'flex', alignItems: 'center', gap: '4px', color: FINCEPT.ORANGE, fontSize: '9px', fontWeight: 700, borderTop: `1px solid ${FINCEPT.BORDER}` }}
            onMouseEnter={(e) => { e.currentTarget.style.backgroundColor = FINCEPT.HOVER; }}
            onMouseLeave={(e) => { e.currentTarget.style.backgroundColor = 'transparent'; }}
          >
            <Plus size={9} /> {t('commandBar.newPortfolio', 'NEW PORTFOLIO')}
          </div>
        </div>
      )}

      {/* Close dropdown overlay */}
      {showDropdown && (
        <div onClick={() => setShowDropdown(false)} style={{ position: 'fixed', inset: 0, zIndex: 99 }} />
      )}

      {/* Scrolling Stats */}
      <div style={{ flex: 1, display: 'flex', alignItems: 'center', gap: '16px', overflow: 'hidden', whiteSpace: 'nowrap' }}>
        {s && [
          { l: t('commandBar.nav', 'NAV'), v: formatCurrency(s.total_market_value, currency), c: FINCEPT.YELLOW },
          { l: t('commandBar.pnl', 'P&L'), v: `${pnl >= 0 ? '+' : ''}${formatCurrency(pnl, currency)}`, c: valColor(pnl) },
          { l: t('commandBar.day', 'DAY'), v: `${dayChg >= 0 ? '+' : ''}${formatCurrency(dayChg, currency)} (${formatPercent(dayChgPct)})`, c: valColor(dayChg) },
          { l: t('commandBar.pos', 'POS'), v: `${s.total_positions}`, c: FINCEPT.WHITE },
          { l: t('commandBar.wl', 'W/L'), v: `${gainers}/${losers}`, c: FINCEPT.WHITE },
        ].map(stat => (
          <div key={stat.l} style={{ display: 'flex', alignItems: 'baseline', gap: '4px', flexShrink: 0 }}>
            <span style={{ fontSize: '9px', color: FINCEPT.GRAY, fontWeight: 700 }}>{stat.l}</span>
            <span style={{ fontSize: '12px', fontWeight: 700, color: stat.c }}>{stat.v}</span>
          </div>
        ))}
      </div>

      {/* Import button — always visible */}
      <button id="portfolio-import" onClick={onImport} style={{
        padding: '4px 10px', backgroundColor: `${FINCEPT.CYAN}15`, border: `1px solid ${FINCEPT.CYAN}60`,
        color: FINCEPT.CYAN, fontSize: '10px', fontWeight: 800, cursor: 'pointer', flexShrink: 0,
        display: 'flex', alignItems: 'center', gap: '4px',
      }}>
        <Upload size={10} /> IMPORT
      </button>

      {/* Actions */}
      {selectedPortfolio && (
        <div style={{ display: 'flex', gap: '4px', flexShrink: 0 }}>
          <button id="portfolio-buy" onClick={onBuy} style={{
            padding: '4px 10px', backgroundColor: `${FINCEPT.GREEN}15`, border: `1px solid ${FINCEPT.GREEN}60`,
            color: FINCEPT.GREEN, fontSize: '10px', fontWeight: 800, cursor: 'pointer',
            display: 'flex', alignItems: 'center', gap: '4px',
          }}>
            <ArrowUpRight size={10} /> {t('actions.buy', 'BUY')}
          </button>
          <button id="portfolio-sell" onClick={onSell} style={{
            padding: '4px 10px', backgroundColor: `${FINCEPT.RED}15`, border: `1px solid ${FINCEPT.RED}60`,
            color: FINCEPT.RED, fontSize: '10px', fontWeight: 800, cursor: 'pointer',
            display: 'flex', alignItems: 'center', gap: '4px',
          }}>
            <ArrowDownRight size={10} /> {t('actions.sell', 'SELL')}
          </button>
          <button id="portfolio-refresh" onClick={onRefresh} disabled={refreshing} style={{
            padding: '4px 6px', backgroundColor: 'transparent', border: `1px solid ${FINCEPT.BORDER}`,
            color: FINCEPT.CYAN, cursor: refreshing ? 'wait' : 'pointer', display: 'flex', alignItems: 'center',
          }}>
            <RefreshCw size={9} className={refreshing ? 'animate-spin' : ''} />
          </button>
          {/* Refresh Interval Selector */}
          <div style={{ position: 'relative' }}>
            <button
              onClick={() => setShowIntervalDropdown(!showIntervalDropdown)}
              style={{
                padding: '4px 8px', backgroundColor: 'transparent', border: `1px solid ${FINCEPT.BORDER}`,
                color: FINCEPT.ORANGE, cursor: 'pointer', display: 'flex', alignItems: 'center', gap: '3px',
                fontSize: '9px', fontWeight: 700,
              }}
            >
              <Timer size={9} />
              {currentIntervalLabel}
            </button>
            {showIntervalDropdown && (
              <>
                <div onClick={() => setShowIntervalDropdown(false)} style={{ position: 'fixed', inset: 0, zIndex: 99 }} />
                <div style={{
                  position: 'absolute', top: '28px', right: 0, zIndex: 100,
                  backgroundColor: '#0A0A0A', border: `1px solid ${FINCEPT.ORANGE}`,
                  minWidth: '100px', boxShadow: '0 4px 12px rgba(0,0,0,0.8)',
                }}>
                  <div style={{ padding: '4px 8px', fontSize: '8px', color: FINCEPT.GRAY, fontWeight: 700, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
                    REFRESH INTERVAL
                  </div>
                  {REFRESH_OPTIONS.map(opt => (
                    <div
                      key={opt.ms}
                      onClick={() => { onSetRefreshInterval(opt.ms); setShowIntervalDropdown(false); }}
                      style={{
                        padding: '6px 10px', cursor: 'pointer', fontSize: '10px', fontWeight: 600,
                        color: refreshIntervalMs === opt.ms ? FINCEPT.ORANGE : FINCEPT.WHITE,
                        backgroundColor: refreshIntervalMs === opt.ms ? `${FINCEPT.ORANGE}12` : 'transparent',
                        borderLeft: refreshIntervalMs === opt.ms ? `2px solid ${FINCEPT.ORANGE}` : '2px solid transparent',
                      }}
                      onMouseEnter={(e) => { e.currentTarget.style.backgroundColor = FINCEPT.HOVER; }}
                      onMouseLeave={(e) => { e.currentTarget.style.backgroundColor = refreshIntervalMs === opt.ms ? `${FINCEPT.ORANGE}12` : 'transparent'; }}
                    >
                      {opt.label}
                    </div>
                  ))}
                </div>
              </>
            )}
          </div>
          <button id="portfolio-export" onClick={onExport} title="EXPORT CSV" style={{
            padding: '4px 6px', backgroundColor: 'transparent', border: `1px solid ${FINCEPT.BORDER}`,
            color: FINCEPT.GRAY, cursor: 'pointer', display: 'flex', alignItems: 'center',
          }}>
            <Download size={9} />
          </button>
          <button id="portfolio-export-json" onClick={onExportJSON} title="EXPORT JSON" style={{
            padding: '4px 6px', backgroundColor: 'transparent', border: `1px solid ${FINCEPT.BORDER}`,
            color: FINCEPT.CYAN, cursor: 'pointer', display: 'flex', alignItems: 'center',
          }}>
            <Upload size={9} />
          </button>
          {/* FFN Analytics Toggle */}
          {onToggleFFN && (
            <button onClick={onToggleFFN} style={{
              padding: '5px 10px',
              backgroundColor: showFFN ? FINCEPT.ORANGE : 'transparent',
              border: `1px solid ${FINCEPT.ORANGE}`,
              color: showFFN ? '#000' : FINCEPT.ORANGE,
              cursor: 'pointer',
              display: 'flex', alignItems: 'center', gap: '4px',
              fontSize: '9px', fontWeight: 700, letterSpacing: '0.5px',
            }}>
              <Zap size={10} />
              FFN
            </button>
          )}
          {/* Detail View Navigation Buttons */}
          {onSetDetailView && DETAIL_VIEW_BUTTONS.map(({ view, label, icon: Icon, color }) => {
            const isActive = detailView === view && !showFFN;
            return (
              <button
                key={view}
                onClick={() => {
                  if (isActive) {
                    onSetDetailView(null);
                  } else {
                    onSetDetailView(view);
                    if (showFFN && onToggleFFN) onToggleFFN();
                  }
                }}
                title={label}
                style={{
                  padding: '5px 8px',
                  backgroundColor: isActive ? `${color}25` : 'transparent',
                  border: `1px solid ${isActive ? color : color + '60'}`,
                  color: color,
                  cursor: 'pointer',
                  display: 'flex', alignItems: 'center', gap: '3px',
                  fontSize: '8px', fontWeight: 700, letterSpacing: '0.5px',
                  flexShrink: 0,
                }}
                onMouseEnter={(e) => { e.currentTarget.style.backgroundColor = `${color}20`; }}
                onMouseLeave={(e) => { e.currentTarget.style.backgroundColor = isActive ? `${color}25` : 'transparent'; }}
              >
                <Icon size={9} />
                {label}
              </button>
            );
          })}
          {/* AI Analysis */}
          {onAnalyzeWithAI && (
            <button
              onClick={onAnalyzeWithAI}
              disabled={aiAnalyzing}
              title="Analyze portfolio with AI agent"
              style={{
                padding: '5px 10px',
                backgroundColor: aiAnalyzing ? '#9D4EDD22' : 'transparent',
                border: '1px solid #9D4EDD',
                color: '#9D4EDD',
                cursor: aiAnalyzing ? 'wait' : 'pointer',
                display: 'flex', alignItems: 'center', gap: '4px',
                fontSize: '9px', fontWeight: 700, letterSpacing: '0.5px',
                opacity: aiAnalyzing ? 0.7 : 1,
              }}
            >
              <Bot size={10} className={aiAnalyzing ? 'animate-pulse' : ''} />
              {aiAnalyzing ? 'ANALYZING...' : 'AI'}
            </button>
          )}
        </div>
      )}
    </div>
  );
};

export default CommandBar;
