/**
 * Deal Database - Track and Save M&A Deals
 *
 * Bloomberg-grade professional terminal UI with pipeline analytics,
 * deal value timeline visualization, and SEC filing scanner.
 */

import React, { useState, useEffect, useMemo } from 'react';
import { Database, Search, FileText, TrendingUp, RefreshCw, Scan, AlertCircle, ExternalLink, BarChart3, Clock, ChevronRight, Activity } from 'lucide-react';
import { FINCEPT, TYPOGRAPHY, SPACING, COMMON_STYLES } from '../../portfolio-tab/finceptStyles';
import { MA_COLORS, CHART_STYLE, CHART_PALETTE } from '../constants';
import { MAMetricCard, MAChartPanel, MASectionHeader, MAEmptyState, MADataTable, MAExportButton } from '../components';
import { MAAnalyticsService, type MADeal } from '@/services/maAnalyticsService';
import {
  PieChart, Pie, Cell, Legend, Tooltip,
  ScatterChart, Scatter, XAxis, YAxis, CartesianGrid, ZAxis,
} from 'recharts';

interface SECFiling {
  accession_number: string;
  filing_type: string;
  filing_date: string;
  company_name: string;
  cik: string;
  filing_url: string;
  confidence_score: number;
  deal_indicators?: string;
}

type PanelView = 'deal' | 'filing' | 'summary' | 'empty';
type LeftTab = 'deals' | 'filings';

const ACCENT = MA_COLORS.dealDb;

const STATUS_COLORS: Record<string, string> = {
  'Completed': FINCEPT.GREEN,
  'completed': FINCEPT.GREEN,
  'pending': '#FFC400',
  'Pending': '#FFC400',
  'announced': FINCEPT.CYAN,
  'Announced': FINCEPT.CYAN,
  'withdrawn': FINCEPT.RED,
  'Withdrawn': FINCEPT.RED,
  'unknown': FINCEPT.GRAY,
};

const CONFIDENCE_COLOR = (score: number) =>
  score >= 0.9 ? FINCEPT.GREEN : score >= 0.75 ? '#FFC400' : score >= 0.5 ? ACCENT : FINCEPT.GRAY;

const FORM_TYPE_LABELS: Record<string, string> = {
  'DEFM14A': 'Definitive Merger Proxy',
  'PREM14A': 'Preliminary Merger Proxy',
  'S-4': 'Registration (Merger)',
  'SC TO-T': 'Tender Offer (3rd Party)',
  'SC TO-I': 'Tender Offer (Issuer)',
  'SC 13E-3': 'Going Private',
  '8-K': 'Material Event',
  '425': 'Merger Communication',
};

export const DealDatabase: React.FC = () => {
  const [deals, setDeals] = useState<MADeal[]>([]);
  const [selectedDeal, setSelectedDeal] = useState<MADeal | null>(null);
  const [recentFilings, setRecentFilings] = useState<SECFiling[]>([]);
  const [selectedFiling, setSelectedFiling] = useState<SECFiling | null>(null);
  const [leftTab, setLeftTab] = useState<LeftTab>('filings');
  const [searchQuery, setSearchQuery] = useState('');
  const [loading, setLoading] = useState(false);
  const [scanning, setScanning] = useState(false);
  const [scanDays, setScanDays] = useState(30);
  const [hideSelfTenders, setHideSelfTenders] = useState(true);
  const [statusMessage, setStatusMessage] = useState<{ type: 'success' | 'error' | 'info'; text: string } | null>(null);

  useEffect(() => {
    loadDeals();
  }, []);

  // Clear status message after 6 seconds
  useEffect(() => {
    if (statusMessage) {
      const timer = setTimeout(() => setStatusMessage(null), 6000);
      return () => clearTimeout(timer);
    }
  }, [statusMessage]);

  const loadDeals = async () => {
    setLoading(true);
    try {
      const result = await MAAnalyticsService.DealDatabase.getAllDeals();
      setDeals(result);
      if (result.length > 0) {
        setStatusMessage({ type: 'info', text: `Loaded ${result.length} deal${result.length !== 1 ? 's' : ''} from database` });
        setLeftTab('deals');
      }
    } catch (error) {
      console.error('Failed to load deals:', error);
      setStatusMessage({ type: 'error', text: 'Failed to load deals from database' });
    } finally {
      setLoading(false);
    }
  };

  const scanFilings = async () => {
    setScanning(true);
    setStatusMessage({ type: 'info', text: `Scanning SEC EDGAR for last ${scanDays} days...` });
    try {
      const result = await MAAnalyticsService.DealDatabase.scanFilings(scanDays);
      const filingsFound = result.filings_found || 0;
      const dealsParsed = result.deals_parsed || 0;
      const dealsCreated = result.deals_created || 0;
      const totalDeals = dealsParsed + dealsCreated;

      // Store filings for display — always show them after a scan
      const filings: SECFiling[] = result.filings || [];
      if (filings.length > 0) {
        setRecentFilings(filings);
        setLeftTab('filings');
        // Auto-select first high-confidence filing
        const topFiling = [...filings].sort((a, b) => b.confidence_score - a.confidence_score)[0];
        setSelectedFiling(topFiling);
        setSelectedDeal(null);
      }

      setStatusMessage({
        type: 'success',
        text: `Found ${filingsFound} filing${filingsFound !== 1 ? 's' : ''}${totalDeals > 0 ? `, ${totalDeals} deal${totalDeals !== 1 ? 's' : ''} added` : ''}`,
      });

      // Reload deals from database
      await loadDeals();
    } catch (error) {
      console.error('Failed to scan filings:', error);
      setStatusMessage({ type: 'error', text: `Scan failed: ${error instanceof Error ? error.message : 'Unknown error'}` });
    } finally {
      setScanning(false);
    }
  };

  const formatCurrency = (value: number) => {
    if (!value || isNaN(value)) return '—';
    if (value >= 1e9) return `$${(value / 1e9).toFixed(2)}B`;
    if (value >= 1e6) return `$${(value / 1e6).toFixed(2)}M`;
    return `$${value.toLocaleString()}`;
  };

  const filteredDeals = useMemo(() =>
    deals.filter(deal => {
      // Optionally hide self-tenders (acquirer === target, e.g. fund share repurchases)
      if (hideSelfTenders && deal.acquirer_name && deal.target_name &&
          deal.acquirer_name.trim().toLowerCase() === deal.target_name.trim().toLowerCase()) {
        return false;
      }
      if (!searchQuery) return true;
      const q = searchQuery.toLowerCase();
      return deal.target_name?.toLowerCase().includes(q) ||
             deal.acquirer_name?.toLowerCase().includes(q) ||
             deal.deal_type?.toLowerCase().includes(q) ||
             deal.industry?.toLowerCase().includes(q);
    }), [deals, searchQuery, hideSelfTenders]);

  const filteredFilings = useMemo(() =>
    recentFilings.filter(f =>
      f.company_name?.toLowerCase().includes(searchQuery.toLowerCase()) ||
      f.filing_type?.toLowerCase().includes(searchQuery.toLowerCase())
    ), [recentFilings, searchQuery]);

  // --- Derived chart data ---
  const statusCounts = useMemo(() => {
    return deals.reduce((acc, d) => {
      const status = d.status || 'unknown';
      acc[status] = (acc[status] || 0) + 1;
      return acc;
    }, {} as Record<string, number>);
  }, [deals]);

  const pipelineData = useMemo(() =>
    Object.entries(statusCounts).map(([status, count]) => ({ name: status, value: count })),
    [statusCounts]);

  const timelineData = useMemo(() =>
    deals
      .filter(d => d.announced_date && d.deal_value)
      .map(d => ({
        date: new Date(d.announced_date).getTime(),
        value: d.deal_value,
        name: d.target_name,
        premium: d.premium_1day,
      }))
      .filter(d => !isNaN(d.date)),
    [deals]);

  // Filing type distribution for chart
  const filingTypeData = useMemo(() => {
    const counts: Record<string, number> = {};
    recentFilings.forEach(f => {
      counts[f.filing_type] = (counts[f.filing_type] || 0) + 1;
    });
    return Object.entries(counts)
      .sort((a, b) => b[1] - a[1])
      .map(([name, value]) => ({ name, value }));
  }, [recentFilings]);

  // Confidence distribution
  const highConfidenceFilings = useMemo(() =>
    recentFilings.filter(f => f.confidence_score >= 0.85), [recentFilings]);

  // Summary metrics
  const totalDealValue = useMemo(() => deals.reduce((sum, d) => sum + (d.deal_value || 0), 0), [deals]);
  const avgPremium = useMemo(() => {
    if (!deals.length) return 0;
    return deals.reduce((sum, d) => sum + (d.premium_1day || 0), 0) / deals.length;
  }, [deals]);
  const completedCount = useMemo(() =>
    deals.filter(d => (d.status || '').toLowerCase() === 'completed').length, [deals]);

  const selectedDealIndex = useMemo(() => {
    if (!selectedDeal) return undefined;
    return filteredDeals.findIndex(d => d.deal_id === selectedDeal.deal_id);
  }, [filteredDeals, selectedDeal]);

  const selectedFilingIndex = useMemo(() => {
    if (!selectedFiling) return undefined;
    return filteredFilings.findIndex(f => f.accession_number === selectedFiling.accession_number);
  }, [filteredFilings, selectedFiling]);

  // Determine right panel view
  const panelView: PanelView = selectedDeal
    ? 'deal'
    : selectedFiling
      ? 'filing'
      : (deals.length > 0 || recentFilings.length > 0)
        ? 'summary'
        : 'empty';

  // Deal list columns
  const dealListColumns = useMemo(() => [
    {
      key: 'target_name',
      label: 'Target',
      sortable: true,
      format: (val: any) => (
        <span style={{ color: FINCEPT.WHITE, fontWeight: TYPOGRAPHY.BOLD as any }}>{val}</span>
      ),
    },
    {
      key: 'deal_value',
      label: 'Value',
      width: '80px',
      align: 'right' as const,
      sortable: true,
      format: (val: any) => val
        ? <span style={{ color: ACCENT }}>{formatCurrency(val)}</span>
        : <span style={{ color: FINCEPT.MUTED }}>–</span>,
    },
    {
      key: 'premium_1day',
      label: '%',
      width: '55px',
      align: 'right' as const,
      sortable: true,
      format: (val: any) => {
        if (!val || val === 0) return <span style={{ color: FINCEPT.MUTED }}>–</span>;
        return (
          <span style={{ color: val > 0 ? FINCEPT.GREEN : FINCEPT.RED }}>
            {val > 0 ? '+' : ''}{val.toFixed(1)}%
          </span>
        );
      },
    },
  ], []);

  // Filing list columns (compact)
  const filingListColumns = useMemo(() => [
    {
      key: 'company_name',
      label: 'Company',
      sortable: true,
      format: (val: any) => (
        <span style={{ color: FINCEPT.WHITE, fontWeight: TYPOGRAPHY.BOLD as any,
          overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' as const,
          display: 'block', maxWidth: '140px' }}>
          {val}
        </span>
      ),
    },
    {
      key: 'filing_type',
      label: 'Type',
      width: '72px',
      sortable: true,
      format: (val: any) => (
        <span style={{
          padding: '2px 5px',
          borderRadius: '2px',
          backgroundColor: `${ACCENT}20`,
          color: ACCENT,
          fontSize: TYPOGRAPHY.TINY,
          whiteSpace: 'nowrap' as const,
        }}>
          {val}
        </span>
      ),
    },
    {
      key: 'confidence_score',
      label: 'Conf',
      width: '42px',
      align: 'right' as const,
      sortable: true,
      format: (val: any) => (
        <span style={{ color: CONFIDENCE_COLOR(val), fontWeight: TYPOGRAPHY.BOLD as any }}>
          {((val || 0) * 100).toFixed(0)}%
        </span>
      ),
    },
  ], []);

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%', overflow: 'hidden' }}>
      {/* ── TOP HEADER BAR ── */}
      <div style={{
        ...COMMON_STYLES.header,
        borderBottom: `2px solid ${ACCENT}`,
        padding: `${SPACING.SMALL} ${SPACING.LARGE}`,
        gap: SPACING.DEFAULT,
        flexShrink: 0,
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: SPACING.MEDIUM }}>
          <Database size={14} style={{ color: ACCENT }} />
          <span style={{
            fontSize: TYPOGRAPHY.BODY,
            fontFamily: TYPOGRAPHY.MONO,
            fontWeight: TYPOGRAPHY.BOLD,
            color: ACCENT,
            letterSpacing: TYPOGRAPHY.WIDE,
            textTransform: 'uppercase' as const,
          }}>
            DEAL DATABASE
          </span>
          {/* Counters */}
          <div style={{ display: 'flex', gap: SPACING.SMALL }}>
            {deals.length > 0 && (
              <span style={{
                fontSize: TYPOGRAPHY.TINY, fontFamily: TYPOGRAPHY.MONO,
                color: FINCEPT.GREEN, padding: '1px 6px',
                backgroundColor: `${FINCEPT.GREEN}15`, borderRadius: '2px',
              }}>
                {deals.length} deal{deals.length !== 1 ? 's' : ''}
              </span>
            )}
            {recentFilings.length > 0 && (
              <span style={{
                fontSize: TYPOGRAPHY.TINY, fontFamily: TYPOGRAPHY.MONO,
                color: ACCENT, padding: '1px 6px',
                backgroundColor: `${ACCENT}15`, borderRadius: '2px',
              }}>
                {recentFilings.length} filing{recentFilings.length !== 1 ? 's' : ''}
                {highConfidenceFilings.length > 0 && (
                  <span style={{ color: FINCEPT.GREEN, marginLeft: '4px' }}>
                    ({highConfidenceFilings.length} high conf.)
                  </span>
                )}
              </span>
            )}
          </div>
        </div>

        <div style={{ flex: 1 }} />

        {/* Status Message */}
        {statusMessage && (
          <div style={{
            padding: `${SPACING.TINY} ${SPACING.MEDIUM}`,
            borderRadius: '2px',
            fontSize: TYPOGRAPHY.TINY,
            fontFamily: TYPOGRAPHY.MONO,
            backgroundColor: statusMessage.type === 'error' ? `${FINCEPT.RED}20` :
                             statusMessage.type === 'success' ? `${FINCEPT.GREEN}20` :
                             `${ACCENT}20`,
            color: statusMessage.type === 'error' ? FINCEPT.RED :
                   statusMessage.type === 'success' ? FINCEPT.GREEN : ACCENT,
            display: 'flex', alignItems: 'center', gap: SPACING.SMALL,
            maxWidth: '360px',
          }}>
            <AlertCircle size={10} />
            {statusMessage.text}
          </div>
        )}

        {/* Scan Controls */}
        <div style={{ display: 'flex', alignItems: 'center', gap: SPACING.SMALL }}>
          <select
            value={scanDays}
            onChange={(e) => setScanDays(Number(e.target.value))}
            style={{
              ...COMMON_STYLES.inputField,
              width: '80px',
              fontSize: TYPOGRAPHY.TINY,
              padding: '4px 6px',
            }}
          >
            <option value={7}>7 days</option>
            <option value={14}>14 days</option>
            <option value={30}>30 days</option>
            <option value={60}>60 days</option>
            <option value={90}>90 days</option>
          </select>
          <button
            onClick={scanFilings}
            disabled={scanning}
            style={{
              padding: `${SPACING.SMALL} ${SPACING.DEFAULT}`,
              backgroundColor: scanning ? FINCEPT.PANEL_BG : ACCENT,
              border: scanning ? `1px solid ${FINCEPT.BORDER}` : 'none',
              borderRadius: '2px',
              color: scanning ? FINCEPT.GRAY : FINCEPT.DARK_BG,
              cursor: scanning ? 'not-allowed' : 'pointer',
              fontSize: TYPOGRAPHY.TINY,
              fontFamily: TYPOGRAPHY.MONO,
              fontWeight: TYPOGRAPHY.BOLD,
              letterSpacing: TYPOGRAPHY.WIDE,
              display: 'flex', alignItems: 'center', gap: SPACING.SMALL,
              textTransform: 'uppercase' as const,
            }}
          >
            <Scan size={11} className={scanning ? 'animate-spin' : ''} />
            {scanning ? 'SCANNING...' : 'SCAN SEC'}
          </button>
        </div>

        <MAExportButton data={[...deals, ...recentFilings]} filename="deal_database" accentColor={ACCENT} />
      </div>

      {/* ── MAIN CONTENT ── */}
      <div style={{ display: 'flex', flex: 1, overflow: 'hidden' }}>
        {/* ── LEFT PANEL (320px) ── */}
        <div style={{
          width: '320px',
          backgroundColor: FINCEPT.PANEL_BG,
          borderRight: `1px solid ${FINCEPT.BORDER}`,
          display: 'flex',
          flexDirection: 'column',
          overflow: 'hidden',
          flexShrink: 0,
        }}>
          {/* Tab switcher */}
          <div style={{
            display: 'flex',
            borderBottom: `1px solid ${FINCEPT.BORDER}`,
            flexShrink: 0,
          }}>
            {(['filings', 'deals'] as LeftTab[]).map(tab => (
              <button
                key={tab}
                onClick={() => setLeftTab(tab)}
                style={{
                  flex: 1,
                  padding: `${SPACING.SMALL} ${SPACING.DEFAULT}`,
                  backgroundColor: 'transparent',
                  border: 'none',
                  borderBottom: leftTab === tab ? `2px solid ${ACCENT}` : '2px solid transparent',
                  color: leftTab === tab ? ACCENT : FINCEPT.MUTED,
                  fontSize: TYPOGRAPHY.TINY,
                  fontFamily: TYPOGRAPHY.MONO,
                  fontWeight: leftTab === tab ? TYPOGRAPHY.BOLD : TYPOGRAPHY.REGULAR,
                  letterSpacing: TYPOGRAPHY.WIDE,
                  cursor: 'pointer',
                  textTransform: 'uppercase' as const,
                  transition: 'all 0.15s',
                  display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '4px',
                }}
              >
                {tab === 'filings' ? <FileText size={10} /> : <Database size={10} />}
                {tab === 'filings'
                  ? `Filings${recentFilings.length > 0 ? ` (${recentFilings.length})` : ''}`
                  : `Deals${deals.length > 0 ? ` (${deals.length})` : ''}`}
              </button>
            ))}
          </div>

          {/* Search */}
          <div style={{
            padding: SPACING.DEFAULT,
            borderBottom: `1px solid ${FINCEPT.BORDER}`,
            display: 'flex', flexDirection: 'column', gap: SPACING.SMALL,
          }}>
            <div style={{ display: 'flex', gap: SPACING.SMALL, alignItems: 'center' }}>
            <div style={{ position: 'relative', flex: 1 }}>
              <Search size={11} style={{
                position: 'absolute', left: '8px', top: '50%',
                transform: 'translateY(-50%)', color: FINCEPT.MUTED,
              }} />
              <input
                type="text"
                placeholder={leftTab === 'filings' ? 'Search filings...' : 'Search deals...'}
                value={searchQuery}
                onChange={(e) => setSearchQuery(e.target.value)}
                style={{
                  ...COMMON_STYLES.inputField,
                  paddingLeft: '26px',
                  fontSize: TYPOGRAPHY.TINY,
                  padding: '6px 10px 6px 26px',
                }}
              />
            </div>
            <button
              onClick={loadDeals}
              disabled={loading}
              title="Refresh"
              style={{
                padding: SPACING.SMALL,
                backgroundColor: 'transparent',
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '2px',
                color: FINCEPT.GRAY,
                cursor: loading ? 'not-allowed' : 'pointer',
                display: 'flex', alignItems: 'center', justifyContent: 'center',
                transition: 'all 0.15s',
              }}
              onMouseEnter={(e) => { e.currentTarget.style.borderColor = ACCENT; e.currentTarget.style.color = ACCENT; }}
              onMouseLeave={(e) => { e.currentTarget.style.borderColor = FINCEPT.BORDER; e.currentTarget.style.color = FINCEPT.GRAY; }}
            >
              <RefreshCw size={12} className={loading ? 'animate-spin' : ''} />
            </button>
            </div>
            {leftTab === 'deals' && (
              <label style={{
                display: 'flex', alignItems: 'center', gap: SPACING.SMALL,
                fontSize: TYPOGRAPHY.TINY, fontFamily: TYPOGRAPHY.MONO, color: FINCEPT.GRAY,
                cursor: 'pointer', userSelect: 'none' as const,
              }}>
                <input
                  type="checkbox"
                  checked={hideSelfTenders}
                  onChange={(e) => setHideSelfTenders(e.target.checked)}
                  style={{ accentColor: ACCENT, cursor: 'pointer' }}
                />
                Hide self-tenders / fund filings
              </label>
            )}
          </div>

          {/* List body */}
          <div style={{ flex: 1, overflow: 'hidden', display: 'flex', flexDirection: 'column' }}>
            {loading ? (
              <div style={{ ...COMMON_STYLES.emptyState, padding: SPACING.XLARGE }}>
                <div className="animate-spin" style={{
                  width: '20px', height: '20px',
                  border: `2px solid ${FINCEPT.BORDER}`,
                  borderTop: `2px solid ${ACCENT}`,
                  borderRadius: '50%', marginBottom: SPACING.SMALL,
                }} />
                <span style={{ fontFamily: TYPOGRAPHY.MONO, fontSize: TYPOGRAPHY.TINY }}>Loading...</span>
              </div>
            ) : leftTab === 'filings' ? (
              filteredFilings.length === 0 ? (
                <div style={{ ...COMMON_STYLES.emptyState, padding: SPACING.XLARGE }}>
                  <FileText size={28} style={{ marginBottom: SPACING.SMALL, opacity: 0.3, color: ACCENT }} />
                  <span style={{ fontSize: TYPOGRAPHY.SMALL, color: FINCEPT.GRAY, fontFamily: TYPOGRAPHY.MONO }}>
                    {searchQuery ? 'No matching filings' : 'No filings scanned yet'}
                  </span>
                  {!searchQuery && (
                    <div style={{
                      fontSize: TYPOGRAPHY.TINY, fontFamily: TYPOGRAPHY.MONO,
                      color: FINCEPT.MUTED, marginTop: SPACING.SMALL,
                      textAlign: 'center', maxWidth: '220px',
                    }}>
                      Click <strong style={{ color: ACCENT }}>SCAN SEC</strong> to fetch real M&A filings from SEC EDGAR
                    </div>
                  )}
                </div>
              ) : (
                <MADataTable
                  columns={filingListColumns}
                  data={filteredFilings as any}
                  accentColor={ACCENT}
                  compact
                  maxHeight="100%"
                  onRowClick={(row: any) => {
                    setSelectedFiling(row as SECFiling);
                    setSelectedDeal(null);
                  }}
                  selectedIndex={selectedFilingIndex}
                  emptyMessage="No filings found"
                />
              )
            ) : (
              filteredDeals.length === 0 ? (
                <div style={{ ...COMMON_STYLES.emptyState, padding: SPACING.XLARGE }}>
                  <Database size={28} style={{ marginBottom: SPACING.SMALL, opacity: 0.3, color: ACCENT }} />
                  <span style={{ fontSize: TYPOGRAPHY.SMALL, color: FINCEPT.GRAY, fontFamily: TYPOGRAPHY.MONO }}>
                    {searchQuery ? 'No matching deals' : 'No deals in database'}
                  </span>
                  {!searchQuery && (
                    <div style={{
                      fontSize: TYPOGRAPHY.TINY, fontFamily: TYPOGRAPHY.MONO,
                      color: FINCEPT.MUTED, marginTop: SPACING.SMALL,
                      textAlign: 'center', maxWidth: '220px',
                    }}>
                      Deals are parsed from high-confidence SEC filings
                    </div>
                  )}
                </div>
              ) : (
                <MADataTable
                  columns={dealListColumns}
                  data={filteredDeals as any}
                  accentColor={ACCENT}
                  compact
                  maxHeight="100%"
                  onRowClick={(row: any) => {
                    setSelectedDeal(row as MADeal);
                    setSelectedFiling(null);
                  }}
                  selectedIndex={selectedDealIndex}
                  emptyMessage="No deals found"
                />
              )
            )}
          </div>
        </div>

        {/* ── RIGHT PANEL ── */}
        <div style={{
          flex: 1,
          backgroundColor: FINCEPT.DARK_BG,
          display: 'flex',
          flexDirection: 'column',
          overflow: 'hidden',
        }}>
          {/* ── DEAL DETAIL VIEW ── */}
          {panelView === 'deal' && selectedDeal && (
            <div style={{ display: 'flex', flexDirection: 'column', height: '100%', overflow: 'hidden' }}>
              {/* Deal Header */}
              <div style={{
                padding: SPACING.LARGE,
                backgroundColor: FINCEPT.HEADER_BG,
                borderBottom: `1px solid ${ACCENT}40`,
                flexShrink: 0,
              }}>
                <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start' }}>
                  <div>
                    <div style={{
                      fontSize: TYPOGRAPHY.HEADING, fontFamily: TYPOGRAPHY.MONO,
                      fontWeight: TYPOGRAPHY.BOLD, color: FINCEPT.WHITE,
                    }}>
                      {selectedDeal.target_name}
                    </div>
                    <div style={{
                      fontSize: TYPOGRAPHY.SMALL, fontFamily: TYPOGRAPHY.MONO,
                      color: FINCEPT.GRAY, marginTop: SPACING.TINY,
                      display: 'flex', alignItems: 'center', gap: SPACING.SMALL,
                    }}>
                      <TrendingUp size={11} style={{ color: ACCENT }} />
                      <span>Acquired by <span style={{ color: FINCEPT.WHITE }}>{selectedDeal.acquirer_name}</span></span>
                    </div>
                  </div>
                  <div style={{ textAlign: 'right' }}>
                    <div style={{
                      fontSize: TYPOGRAPHY.HEADING, fontFamily: TYPOGRAPHY.MONO,
                      fontWeight: TYPOGRAPHY.BOLD, color: ACCENT,
                    }}>
                      {formatCurrency(selectedDeal.deal_value)}
                    </div>
                    <div style={{
                      fontSize: TYPOGRAPHY.TINY, fontFamily: TYPOGRAPHY.MONO,
                      color: FINCEPT.MUTED, marginTop: SPACING.TINY,
                      letterSpacing: TYPOGRAPHY.WIDE,
                    }}>
                      DEAL VALUE
                    </div>
                  </div>
                </div>
                <div style={{ marginTop: SPACING.DEFAULT, display: 'flex', gap: SPACING.SMALL, flexWrap: 'wrap' as const }}>
                  <span style={{
                    ...COMMON_STYLES.badgeInfo,
                    backgroundColor: `${STATUS_COLORS[selectedDeal.status] || ACCENT}20`,
                    color: STATUS_COLORS[selectedDeal.status] || ACCENT,
                    fontFamily: TYPOGRAPHY.MONO, letterSpacing: TYPOGRAPHY.WIDE,
                  }}>
                    {selectedDeal.status.toUpperCase()}
                  </span>
                  {selectedDeal.deal_type && (
                    <span style={{
                      ...COMMON_STYLES.badgeInfo,
                      backgroundColor: `${FINCEPT.CYAN}20`, color: FINCEPT.CYAN,
                      fontFamily: TYPOGRAPHY.MONO, letterSpacing: TYPOGRAPHY.WIDE,
                    }}>
                      {selectedDeal.deal_type.toUpperCase()}
                    </span>
                  )}
                  <span style={{
                    ...COMMON_STYLES.badgeInfo,
                    backgroundColor: `${ACCENT}20`, color: ACCENT,
                    fontFamily: TYPOGRAPHY.MONO, letterSpacing: TYPOGRAPHY.WIDE,
                  }}>
                    {selectedDeal.industry.toUpperCase()}
                  </span>
                  {selectedDeal.hostile_flag === 1 && (
                    <span style={{ ...COMMON_STYLES.badgeInfo, backgroundColor: `${FINCEPT.RED}20`, color: FINCEPT.RED, fontFamily: TYPOGRAPHY.MONO }}>
                      HOSTILE
                    </span>
                  )}
                  {selectedDeal.tender_offer_flag === 1 && (
                    <span style={{ ...COMMON_STYLES.badgeInfo, backgroundColor: `${FINCEPT.CYAN}15`, color: FINCEPT.CYAN, fontFamily: TYPOGRAPHY.MONO }}>
                      TENDER OFFER
                    </span>
                  )}
                  {selectedDeal.cross_border_flag === 1 && (
                    <span style={{ ...COMMON_STYLES.badgeInfo, backgroundColor: `${FINCEPT.GREEN}15`, color: FINCEPT.GREEN, fontFamily: TYPOGRAPHY.MONO }}>
                      CROSS-BORDER
                    </span>
                  )}
                  <span style={{ ...COMMON_STYLES.badgeInfo, fontFamily: TYPOGRAPHY.MONO }}>
                    {selectedDeal.announced_date}
                  </span>
                </div>
              </div>

              <div style={{ flex: 1, overflow: 'auto', padding: SPACING.LARGE }}>
                <MASectionHeader title="Key Metrics" icon={<BarChart3 size={13} />} accentColor={ACCENT} />
                <div style={{
                  display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)',
                  gap: SPACING.DEFAULT, marginBottom: SPACING.LARGE,
                }}>
                  <MAMetricCard
                    label="Premium (1-Day)"
                    value={selectedDeal.premium_1day
                      ? `${selectedDeal.premium_1day > 0 ? '+' : ''}${selectedDeal.premium_1day.toFixed(1)}%`
                      : 'N/A'}
                    accentColor={selectedDeal.premium_1day > 0 ? FINCEPT.GREEN : selectedDeal.premium_1day < 0 ? FINCEPT.RED : FINCEPT.GRAY}
                    trend={selectedDeal.premium_1day > 0 ? 'up' : selectedDeal.premium_1day < 0 ? 'down' : 'neutral'}
                  />
                  <MAMetricCard
                    label="Cash Component"
                    value={`${selectedDeal.payment_cash_pct}%`}
                    accentColor={ACCENT}
                    subtitle={`Stock: ${selectedDeal.payment_stock_pct}% · ${selectedDeal.payment_method || 'Cash'}`}
                  />
                  <MAMetricCard
                    label="Announced"
                    value={selectedDeal.announced_date}
                    accentColor={FINCEPT.GRAY}
                    format="text"
                  />
                  {selectedDeal.offer_price_per_share != null && (
                    <MAMetricCard label="Offer Price / Share" value={`$${selectedDeal.offer_price_per_share.toFixed(2)}`} accentColor={FINCEPT.GREEN} />
                  )}
                  {selectedDeal.expected_close_date && (
                    <MAMetricCard label="Expected Close" value={selectedDeal.expected_close_date} accentColor={FINCEPT.GRAY} format="text" />
                  )}
                  {selectedDeal.breakup_fee != null && (
                    <MAMetricCard label="Breakup Fee" value={formatCurrency(selectedDeal.breakup_fee)} accentColor={FINCEPT.RED} />
                  )}
                  {selectedDeal.ev_revenue != null && (
                    <MAMetricCard label="EV / Revenue" value={`${selectedDeal.ev_revenue.toFixed(2)}x`} accentColor={FINCEPT.CYAN} />
                  )}
                  {selectedDeal.ev_ebitda != null && (
                    <MAMetricCard label="EV / EBITDA" value={`${selectedDeal.ev_ebitda.toFixed(2)}x`} accentColor={FINCEPT.CYAN} />
                  )}
                  {selectedDeal.synergies != null && (
                    <MAMetricCard label="Est. Synergies" value={formatCurrency(selectedDeal.synergies)} accentColor={FINCEPT.GREEN} />
                  )}
                </div>

                <MASectionHeader title="Payment Structure" accentColor={ACCENT} />
                <div style={{
                  height: '6px', borderRadius: '2px', overflow: 'hidden',
                  backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`,
                  display: 'flex', marginBottom: SPACING.SMALL,
                }}>
                  {selectedDeal.payment_cash_pct > 0 && (
                    <div style={{ width: `${selectedDeal.payment_cash_pct}%`, backgroundColor: ACCENT }} title={`Cash: ${selectedDeal.payment_cash_pct}%`} />
                  )}
                  {selectedDeal.payment_stock_pct > 0 && (
                    <div style={{ width: `${selectedDeal.payment_stock_pct}%`, backgroundColor: FINCEPT.CYAN }} title={`Stock: ${selectedDeal.payment_stock_pct}%`} />
                  )}
                  {selectedDeal.payment_cash_pct === 0 && selectedDeal.payment_stock_pct === 0 && (
                    <div style={{ width: '100%', backgroundColor: FINCEPT.BORDER }} title="Payment structure unknown" />
                  )}
                </div>
                <div style={{
                  display: 'flex', gap: SPACING.LARGE, marginBottom: SPACING.LARGE,
                  fontSize: TYPOGRAPHY.TINY, fontFamily: TYPOGRAPHY.MONO,
                }}>
                  <span style={{ display: 'flex', alignItems: 'center', gap: SPACING.SMALL }}>
                    <span style={{ width: '8px', height: '8px', backgroundColor: ACCENT, borderRadius: '1px', display: 'inline-block' }} />
                    <span style={{ color: FINCEPT.GRAY }}>Cash {selectedDeal.payment_cash_pct}%</span>
                  </span>
                  <span style={{ display: 'flex', alignItems: 'center', gap: SPACING.SMALL }}>
                    <span style={{ width: '8px', height: '8px', backgroundColor: FINCEPT.CYAN, borderRadius: '1px', display: 'inline-block' }} />
                    <span style={{ color: FINCEPT.GRAY }}>Stock {selectedDeal.payment_stock_pct}%</span>
                  </span>
                  {selectedDeal.payment_method && (
                    <span style={{ color: FINCEPT.MUTED, marginLeft: 'auto' }}>
                      Method: <span style={{ color: FINCEPT.WHITE }}>{selectedDeal.payment_method}</span>
                    </span>
                  )}
                </div>

                {deals.length > 1 && (
                  <>
                    <MASectionHeader title="Pipeline Analytics" icon={<Activity size={13} />} accentColor={ACCENT} />
                    <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.DEFAULT, marginBottom: SPACING.LARGE }}>
                      {pipelineData.length > 0 && (
                        <MAChartPanel title="Deal Pipeline Status" icon={<BarChart3 size={12} />} accentColor={ACCENT} height={200}>
                          <PieChart>
                            <Pie data={pipelineData} dataKey="value" nameKey="name" cx="50%" cy="50%" outerRadius={70}
                              label={({ name, value }) => `${name} (${value})`}
                              labelLine={{ stroke: FINCEPT.MUTED }}
                              style={{ fontSize: '9px', fontFamily: 'var(--ft-font-family, monospace)' }}>
                              {pipelineData.map((entry, i) => (
                                <Cell key={i} fill={STATUS_COLORS[entry.name] || CHART_PALETTE[i % CHART_PALETTE.length]} />
                              ))}
                            </Pie>
                            <Legend wrapperStyle={{ fontSize: '9px', fontFamily: 'var(--ft-font-family, monospace)' }} />
                            <Tooltip contentStyle={CHART_STYLE.tooltip} />
                          </PieChart>
                        </MAChartPanel>
                      )}
                      {timelineData.length > 0 && (
                        <MAChartPanel title="Deal Value Timeline" icon={<Clock size={12} />} accentColor={ACCENT} height={200}>
                          <ScatterChart margin={{ top: 10, right: 10, bottom: 10, left: 10 }}>
                            <CartesianGrid stroke={CHART_STYLE.grid.stroke} strokeDasharray={CHART_STYLE.grid.strokeDasharray} />
                            <XAxis type="number" dataKey="date" domain={['auto', 'auto']}
                              tickFormatter={(val) => new Date(val).toLocaleDateString()}
                              tick={CHART_STYLE.axis} stroke={FINCEPT.BORDER} />
                            <YAxis dataKey="value" tick={CHART_STYLE.axis} stroke={FINCEPT.BORDER}
                              tickFormatter={(val) => val >= 1e9 ? `$${(val / 1e9).toFixed(0)}B` : `$${(val / 1e6).toFixed(0)}M`} />
                            <ZAxis dataKey="premium" range={[20, 200]} />
                            <Scatter data={timelineData} fill={ACCENT} />
                            <Tooltip contentStyle={CHART_STYLE.tooltip}
                              formatter={(value: any, name: string) => {
                                if (name === 'date') return [new Date(value).toLocaleDateString(), 'Date'];
                                if (name === 'value') return [formatCurrency(value), 'Deal Value'];
                                if (name === 'premium') return [`${value > 0 ? '+' : ''}${(value).toFixed(1)}%`, 'Premium'];
                                return [value, name];
                              }}
                              labelFormatter={() => ''} />
                          </ScatterChart>
                        </MAChartPanel>
                      )}
                    </div>
                  </>
                )}
              </div>
            </div>
          )}

          {/* ── FILING DETAIL VIEW ── */}
          {panelView === 'filing' && selectedFiling && (
            <div style={{ display: 'flex', flexDirection: 'column', height: '100%', overflow: 'hidden' }}>
              {/* Filing Header */}
              <div style={{
                padding: SPACING.LARGE,
                backgroundColor: FINCEPT.HEADER_BG,
                borderBottom: `1px solid ${ACCENT}40`,
                flexShrink: 0,
              }}>
                <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start' }}>
                  <div style={{ flex: 1, minWidth: 0 }}>
                    <div style={{
                      fontSize: TYPOGRAPHY.HEADING, fontFamily: TYPOGRAPHY.MONO,
                      fontWeight: TYPOGRAPHY.BOLD, color: FINCEPT.WHITE,
                      overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' as const,
                    }}>
                      {selectedFiling.company_name}
                    </div>
                    <div style={{
                      fontSize: TYPOGRAPHY.SMALL, fontFamily: TYPOGRAPHY.MONO,
                      color: FINCEPT.GRAY, marginTop: SPACING.TINY,
                      display: 'flex', alignItems: 'center', gap: SPACING.SMALL,
                    }}>
                      <FileText size={11} style={{ color: ACCENT }} />
                      <span>{FORM_TYPE_LABELS[selectedFiling.filing_type] || selectedFiling.filing_type}</span>
                    </div>
                  </div>
                  <div style={{ textAlign: 'right', flexShrink: 0, marginLeft: SPACING.DEFAULT }}>
                    <div style={{
                      fontSize: '22px', fontFamily: TYPOGRAPHY.MONO,
                      fontWeight: TYPOGRAPHY.BOLD,
                      color: CONFIDENCE_COLOR(selectedFiling.confidence_score),
                    }}>
                      {(selectedFiling.confidence_score * 100).toFixed(0)}%
                    </div>
                    <div style={{
                      fontSize: TYPOGRAPHY.TINY, fontFamily: TYPOGRAPHY.MONO,
                      color: FINCEPT.MUTED, marginTop: SPACING.TINY,
                      letterSpacing: TYPOGRAPHY.WIDE,
                    }}>
                      M&A CONFIDENCE
                    </div>
                  </div>
                </div>

                <div style={{ marginTop: SPACING.DEFAULT, display: 'flex', gap: SPACING.SMALL, flexWrap: 'wrap' as const, alignItems: 'center' }}>
                  <span style={{
                    padding: '3px 8px', borderRadius: '2px',
                    backgroundColor: `${ACCENT}20`, color: ACCENT,
                    fontSize: TYPOGRAPHY.TINY, fontFamily: TYPOGRAPHY.MONO,
                    letterSpacing: TYPOGRAPHY.WIDE,
                  }}>
                    {selectedFiling.filing_type}
                  </span>
                  <span style={{ ...COMMON_STYLES.badgeInfo, fontFamily: TYPOGRAPHY.MONO }}>
                    {selectedFiling.filing_date}
                  </span>
                  {selectedFiling.cik && (
                    <span style={{ ...COMMON_STYLES.badgeInfo, fontFamily: TYPOGRAPHY.MONO, color: FINCEPT.MUTED }}>
                      CIK: {selectedFiling.cik}
                    </span>
                  )}
                  <a
                    href={selectedFiling.filing_url}
                    target="_blank"
                    rel="noopener noreferrer"
                    style={{
                      display: 'flex', alignItems: 'center', gap: '4px',
                      padding: '3px 8px', borderRadius: '2px',
                      backgroundColor: `${ACCENT}15`, color: ACCENT,
                      fontSize: TYPOGRAPHY.TINY, fontFamily: TYPOGRAPHY.MONO,
                      textDecoration: 'none', letterSpacing: TYPOGRAPHY.WIDE,
                      border: `1px solid ${ACCENT}30`,
                    }}
                    onClick={(e) => e.stopPropagation()}
                  >
                    <ExternalLink size={10} />
                    VIEW ON EDGAR
                  </a>
                </div>
              </div>

              <div style={{ flex: 1, overflow: 'auto', padding: SPACING.LARGE }}>
                {/* Filing Metrics */}
                <MASectionHeader title="Filing Details" icon={<FileText size={13} />} accentColor={ACCENT} />
                <div style={{
                  display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)',
                  gap: SPACING.DEFAULT, marginBottom: SPACING.LARGE,
                }}>
                  <MAMetricCard
                    label="Form Type"
                    value={selectedFiling.filing_type}
                    accentColor={ACCENT}
                    format="text"
                  />
                  <MAMetricCard
                    label="Filing Date"
                    value={selectedFiling.filing_date}
                    accentColor={FINCEPT.GRAY}
                    format="text"
                  />
                  <MAMetricCard
                    label="M&A Confidence"
                    value={`${(selectedFiling.confidence_score * 100).toFixed(0)}%`}
                    accentColor={CONFIDENCE_COLOR(selectedFiling.confidence_score)}
                    trend={selectedFiling.confidence_score >= 0.75 ? 'up' : 'neutral'}
                  />
                </div>

                {/* Confidence bar */}
                <MASectionHeader title="Signal Strength" accentColor={ACCENT} />
                <div style={{
                  marginBottom: SPACING.LARGE,
                  padding: SPACING.DEFAULT,
                  backgroundColor: FINCEPT.PANEL_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '2px',
                }}>
                  <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: SPACING.SMALL, fontSize: TYPOGRAPHY.TINY, fontFamily: TYPOGRAPHY.MONO, color: FINCEPT.GRAY }}>
                    <span>0%</span>
                    <span style={{ color: CONFIDENCE_COLOR(selectedFiling.confidence_score) }}>
                      {(selectedFiling.confidence_score * 100).toFixed(0)}% — {
                        selectedFiling.confidence_score >= 0.9 ? 'Very High' :
                        selectedFiling.confidence_score >= 0.75 ? 'High' :
                        selectedFiling.confidence_score >= 0.5 ? 'Medium' : 'Low'
                      }
                    </span>
                    <span>100%</span>
                  </div>
                  <div style={{ height: '6px', backgroundColor: FINCEPT.BORDER, borderRadius: '2px', overflow: 'hidden' }}>
                    <div style={{
                      width: `${selectedFiling.confidence_score * 100}%`,
                      height: '100%',
                      backgroundColor: CONFIDENCE_COLOR(selectedFiling.confidence_score),
                      borderRadius: '2px',
                      transition: 'width 0.3s ease',
                    }} />
                  </div>
                </div>

                {/* Deal Indicators */}
                {selectedFiling.deal_indicators && (
                  <>
                    <MASectionHeader title="M&A Indicators Detected" icon={<Activity size={13} />} accentColor={ACCENT} />
                    <div style={{
                      marginBottom: SPACING.LARGE,
                      display: 'flex', flexWrap: 'wrap' as const, gap: SPACING.SMALL,
                    }}>
                      {selectedFiling.deal_indicators.split(',').map((indicator, i) => (
                        <span key={i} style={{
                          padding: '3px 8px', borderRadius: '2px',
                          backgroundColor: `${FINCEPT.GREEN}15`,
                          border: `1px solid ${FINCEPT.GREEN}30`,
                          color: FINCEPT.GREEN,
                          fontSize: TYPOGRAPHY.TINY, fontFamily: TYPOGRAPHY.MONO,
                        }}>
                          {indicator.trim()}
                        </span>
                      ))}
                    </div>
                  </>
                )}

                {/* Form type explanation */}
                <MASectionHeader title="Form Type Reference" accentColor={ACCENT} />
                <div style={{
                  padding: SPACING.DEFAULT,
                  backgroundColor: FINCEPT.PANEL_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '2px',
                  marginBottom: SPACING.LARGE,
                }}>
                  {Object.entries(FORM_TYPE_LABELS).map(([form, desc]) => (
                    <div key={form} style={{
                      display: 'flex', gap: SPACING.DEFAULT,
                      padding: `${SPACING.TINY} 0`,
                      borderBottom: `1px solid ${FINCEPT.BORDER}20`,
                      alignItems: 'center',
                    }}>
                      <span style={{
                        width: '80px', flexShrink: 0,
                        fontSize: TYPOGRAPHY.TINY, fontFamily: TYPOGRAPHY.MONO,
                        color: form === selectedFiling.filing_type ? ACCENT : FINCEPT.MUTED,
                        fontWeight: form === selectedFiling.filing_type ? TYPOGRAPHY.BOLD : TYPOGRAPHY.REGULAR,
                      }}>
                        {form}
                      </span>
                      {form === selectedFiling.filing_type && (
                        <ChevronRight size={10} style={{ color: ACCENT, flexShrink: 0 }} />
                      )}
                      <span style={{
                        fontSize: TYPOGRAPHY.TINY, fontFamily: TYPOGRAPHY.MONO,
                        color: form === selectedFiling.filing_type ? FINCEPT.WHITE : FINCEPT.MUTED,
                      }}>
                        {desc}
                      </span>
                    </div>
                  ))}
                </div>

                {/* Accession number */}
                <div style={{
                  padding: SPACING.DEFAULT,
                  backgroundColor: FINCEPT.PANEL_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '2px',
                  fontSize: TYPOGRAPHY.TINY, fontFamily: TYPOGRAPHY.MONO,
                  color: FINCEPT.MUTED,
                }}>
                  <span style={{ color: FINCEPT.GRAY }}>Accession Number: </span>
                  <span style={{ color: ACCENT }}>{selectedFiling.accession_number}</span>
                </div>
              </div>
            </div>
          )}

          {/* ── SUMMARY VIEW (deals or filings exist, nothing selected) ── */}
          {panelView === 'summary' && (
            <div style={{ flex: 1, overflow: 'auto', padding: SPACING.LARGE }}>
              {/* Filings Summary */}
              {recentFilings.length > 0 && (
                <>
                  <MASectionHeader
                    title="SEC Filings Summary"
                    subtitle={`${recentFilings.length} filings from last ${scanDays} days`}
                    icon={<FileText size={13} />}
                    accentColor={ACCENT}
                  />
                  <div style={{
                    display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)',
                    gap: SPACING.DEFAULT, marginBottom: SPACING.LARGE,
                  }}>
                    <MAMetricCard label="Total Filings" value={String(recentFilings.length)} accentColor={ACCENT} />
                    <MAMetricCard
                      label="High Confidence"
                      value={String(highConfidenceFilings.length)}
                      accentColor={FINCEPT.GREEN}
                      subtitle={`${recentFilings.length > 0 ? ((highConfidenceFilings.length / recentFilings.length) * 100).toFixed(0) : 0}% of total`}
                    />
                    <MAMetricCard
                      label="Form Types"
                      value={String(new Set(recentFilings.map(f => f.filing_type)).size)}
                      accentColor={FINCEPT.CYAN}
                    />
                    <MAMetricCard
                      label="Avg Confidence"
                      value={`${recentFilings.length > 0 ? ((recentFilings.reduce((s, f) => s + f.confidence_score, 0) / recentFilings.length) * 100).toFixed(0) : 0}%`}
                      accentColor={ACCENT}
                    />
                  </div>

                  {/* Form type distribution chart */}
                  {filingTypeData.length > 0 && (
                    <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.DEFAULT, marginBottom: SPACING.LARGE }}>
                      <MAChartPanel title="Filing Type Distribution" icon={<FileText size={12} />} accentColor={ACCENT} height={200}>
                        <PieChart>
                          <Pie data={filingTypeData} dataKey="value" nameKey="name" cx="50%" cy="50%" outerRadius={65}
                            label={({ name, value }) => `${name} (${value})`}
                            labelLine={{ stroke: FINCEPT.MUTED }}
                            style={{ fontSize: '9px', fontFamily: 'var(--ft-font-family, monospace)' }}>
                            {filingTypeData.map((_, i) => (
                              <Cell key={i} fill={CHART_PALETTE[i % CHART_PALETTE.length]} />
                            ))}
                          </Pie>
                          <Legend wrapperStyle={{ fontSize: '9px', fontFamily: 'var(--ft-font-family, monospace)' }} />
                          <Tooltip contentStyle={CHART_STYLE.tooltip} />
                        </PieChart>
                      </MAChartPanel>

                      {/* High confidence filings quick list */}
                      <div style={{
                        backgroundColor: FINCEPT.PANEL_BG,
                        border: `1px solid ${FINCEPT.BORDER}`,
                        borderRadius: '2px',
                        padding: SPACING.DEFAULT,
                        height: '200px',
                        overflow: 'auto',
                      }}>
                        <div style={{
                          fontSize: TYPOGRAPHY.TINY, fontFamily: TYPOGRAPHY.MONO,
                          color: FINCEPT.GREEN, letterSpacing: TYPOGRAPHY.WIDE,
                          marginBottom: SPACING.SMALL, fontWeight: TYPOGRAPHY.BOLD,
                        }}>
                          HIGH CONFIDENCE FILINGS
                        </div>
                        {highConfidenceFilings.slice(0, 8).map((f, i) => (
                          <div
                            key={i}
                            onClick={() => { setSelectedFiling(f); setLeftTab('filings'); }}
                            style={{
                              display: 'flex', justifyContent: 'space-between', alignItems: 'center',
                              padding: `${SPACING.TINY} 0`,
                              borderBottom: `1px solid ${FINCEPT.BORDER}20`,
                              cursor: 'pointer',
                            }}
                            onMouseEnter={(e) => { e.currentTarget.style.backgroundColor = `${ACCENT}10`; }}
                            onMouseLeave={(e) => { e.currentTarget.style.backgroundColor = 'transparent'; }}
                          >
                            <div style={{ flex: 1, minWidth: 0 }}>
                              <div style={{
                                fontSize: TYPOGRAPHY.TINY, fontFamily: TYPOGRAPHY.MONO,
                                color: FINCEPT.WHITE,
                                overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' as const,
                              }}>
                                {f.company_name}
                              </div>
                              <div style={{
                                fontSize: '9px', fontFamily: TYPOGRAPHY.MONO,
                                color: FINCEPT.MUTED,
                              }}>
                                {f.filing_type} · {f.filing_date}
                              </div>
                            </div>
                            <span style={{
                              fontSize: TYPOGRAPHY.TINY, fontFamily: TYPOGRAPHY.MONO,
                              color: CONFIDENCE_COLOR(f.confidence_score),
                              fontWeight: TYPOGRAPHY.BOLD, marginLeft: SPACING.SMALL,
                              flexShrink: 0,
                            }}>
                              {(f.confidence_score * 100).toFixed(0)}%
                            </span>
                          </div>
                        ))}
                      </div>
                    </div>
                  )}
                </>
              )}

              {/* Deals Summary (if deals exist) */}
              {deals.length > 0 && (
                <>
                  <MASectionHeader
                    title="Deal Portfolio Summary"
                    subtitle={`${deals.length} deals`}
                    icon={<Database size={13} />}
                    accentColor={ACCENT}
                  />
                  <div style={{
                    display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)',
                    gap: SPACING.DEFAULT, marginBottom: SPACING.LARGE,
                  }}>
                    <MAMetricCard label="Total Deals" value={String(deals.length)} accentColor={ACCENT} />
                    <MAMetricCard label="Total Value" value={formatCurrency(totalDealValue)} accentColor={ACCENT} />
                    <MAMetricCard
                      label="Avg Premium"
                      value={`${avgPremium > 0 ? '+' : ''}${avgPremium.toFixed(1)}%`}
                      accentColor={avgPremium > 0 ? FINCEPT.GREEN : FINCEPT.RED}
                      trend={avgPremium > 0 ? 'up' : avgPremium < 0 ? 'down' : 'neutral'}
                    />
                    <MAMetricCard
                      label="Completed"
                      value={`${completedCount} / ${deals.length}`}
                      accentColor={FINCEPT.GREEN}
                      subtitle={`${deals.length > 0 ? ((completedCount / deals.length) * 100).toFixed(0) : 0}% completion`}
                    />
                  </div>
                  {pipelineData.length > 0 && timelineData.length > 0 && (
                    <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.DEFAULT, marginBottom: SPACING.LARGE }}>
                      <MAChartPanel title="Deal Pipeline Status" icon={<BarChart3 size={12} />} accentColor={ACCENT} height={220}>
                        <PieChart>
                          <Pie data={pipelineData} dataKey="value" nameKey="name" cx="50%" cy="50%" outerRadius={70}
                            label={({ name, value }) => `${name} (${value})`}
                            labelLine={{ stroke: FINCEPT.MUTED }}
                            style={{ fontSize: '9px', fontFamily: 'var(--ft-font-family, monospace)' }}>
                            {pipelineData.map((entry, i) => (
                              <Cell key={i} fill={STATUS_COLORS[entry.name] || CHART_PALETTE[i % CHART_PALETTE.length]} />
                            ))}
                          </Pie>
                          <Legend wrapperStyle={{ fontSize: '9px', fontFamily: 'var(--ft-font-family, monospace)' }} />
                          <Tooltip contentStyle={CHART_STYLE.tooltip} />
                        </PieChart>
                      </MAChartPanel>
                      <MAChartPanel title="Deal Value Timeline" icon={<Clock size={12} />} accentColor={ACCENT} height={220}>
                        <ScatterChart margin={{ top: 10, right: 10, bottom: 10, left: 10 }}>
                          <CartesianGrid stroke={CHART_STYLE.grid.stroke} strokeDasharray={CHART_STYLE.grid.strokeDasharray} />
                          <XAxis type="number" dataKey="date" domain={['auto', 'auto']}
                            tickFormatter={(val) => new Date(val).toLocaleDateString()}
                            tick={CHART_STYLE.axis} stroke={FINCEPT.BORDER} />
                          <YAxis dataKey="value" tick={CHART_STYLE.axis} stroke={FINCEPT.BORDER}
                            tickFormatter={(val) => val >= 1e9 ? `$${(val / 1e9).toFixed(0)}B` : `$${(val / 1e6).toFixed(0)}M`} />
                          <ZAxis dataKey="premium" range={[20, 200]} />
                          <Scatter data={timelineData} fill={ACCENT} />
                          <Tooltip contentStyle={CHART_STYLE.tooltip}
                            formatter={(value: any, name: string) => {
                              if (name === 'date') return [new Date(value).toLocaleDateString(), 'Date'];
                              if (name === 'value') return [formatCurrency(value), 'Deal Value'];
                              if (name === 'premium') return [`${value > 0 ? '+' : ''}${(value).toFixed(1)}%`, 'Premium'];
                              return [value, name];
                            }}
                            labelFormatter={() => ''} />
                        </ScatterChart>
                      </MAChartPanel>
                    </div>
                  )}
                </>
              )}

              {/* Prompt to select a filing */}
              {recentFilings.length > 0 && !deals.length && (
                <div style={{
                  padding: SPACING.LARGE,
                  backgroundColor: FINCEPT.PANEL_BG,
                  border: `1px solid ${ACCENT}30`,
                  borderRadius: '2px',
                  textAlign: 'center',
                  fontSize: TYPOGRAPHY.SMALL,
                  fontFamily: TYPOGRAPHY.MONO,
                  color: FINCEPT.GRAY,
                }}>
                  <FileText size={20} style={{ color: ACCENT, marginBottom: SPACING.SMALL, display: 'block', margin: '0 auto 8px' }} />
                  Select a filing from the list to view details
                </div>
              )}
            </div>
          )}

          {/* ── EMPTY STATE ── */}
          {panelView === 'empty' && (
            <MAEmptyState
              icon={<Database size={48} />}
              title="Deal Database"
              description="Scan SEC EDGAR to discover real M&A filings — no demo data, live signals only"
              accentColor={ACCENT}
              actionLabel="Scan SEC Filings"
              onAction={scanFilings}
            />
          )}
        </div>
      </div>
    </div>
  );
};
