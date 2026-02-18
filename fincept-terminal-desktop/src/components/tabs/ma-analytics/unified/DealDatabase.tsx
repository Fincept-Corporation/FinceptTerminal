/**
 * Deal Database - Track and Save M&A Deals
 *
 * Bloomberg-grade professional terminal UI with pipeline analytics,
 * deal value timeline visualization, and SEC filing scanner.
 */

import React, { useState, useEffect, useMemo } from 'react';
import { Database, Search, FileText, TrendingUp, RefreshCw, Scan, AlertCircle, ExternalLink, BarChart3, Clock } from 'lucide-react';
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

// SEC Filings table column definitions
const filingsColumns = [
  {
    key: 'company_name',
    label: 'Company',
    sortable: true,
    format: (val: any) => (
      <span style={{ color: FINCEPT.WHITE, fontWeight: TYPOGRAPHY.BOLD as any }}>{val}</span>
    ),
  },
  {
    key: 'filing_type',
    label: 'Type',
    width: '80px',
    sortable: true,
    format: (val: any) => (
      <span style={{
        padding: '2px 6px',
        borderRadius: '2px',
        backgroundColor: `${ACCENT}20`,
        color: ACCENT,
        fontSize: TYPOGRAPHY.TINY,
      }}>
        {val}
      </span>
    ),
  },
  {
    key: 'filing_date',
    label: 'Date',
    width: '90px',
    sortable: true,
    format: (val: any) => <span style={{ color: FINCEPT.GRAY }}>{val}</span>,
  },
  {
    key: 'confidence_score',
    label: 'Confidence',
    width: '90px',
    align: 'center' as const,
    sortable: true,
    format: (val: any) => {
      const color = val >= 0.9 ? FINCEPT.GREEN : val >= 0.75 ? '#FFC400' : FINCEPT.GRAY;
      return <span style={{ color, fontWeight: TYPOGRAPHY.BOLD as any }}>{(val * 100).toFixed(0)}%</span>;
    },
  },
  {
    key: 'deal_indicators',
    label: 'Indicators',
    format: (val: any) => (
      <span style={{
        color: FINCEPT.MUTED,
        overflow: 'hidden',
        textOverflow: 'ellipsis',
        whiteSpace: 'nowrap' as const,
        maxWidth: '180px',
        display: 'inline-block',
      }}>
        {val || '-'}
      </span>
    ),
  },
  {
    key: 'filing_url',
    label: 'Link',
    width: '50px',
    align: 'center' as const,
    sortable: false,
    format: (val: any) => (
      <a
        href={val}
        target="_blank"
        rel="noopener noreferrer"
        style={{ color: ACCENT, textDecoration: 'none' }}
        onClick={(e) => e.stopPropagation()}
      >
        <ExternalLink size={12} />
      </a>
    ),
  },
];

export const DealDatabase: React.FC = () => {
  const [deals, setDeals] = useState<MADeal[]>([]);
  const [selectedDeal, setSelectedDeal] = useState<MADeal | null>(null);
  const [recentFilings, setRecentFilings] = useState<SECFiling[]>([]);
  const [showFilings, setShowFilings] = useState(false);
  const [searchQuery, setSearchQuery] = useState('');
  const [loading, setLoading] = useState(false);
  const [scanning, setScanning] = useState(false);
  const [scanDays, setScanDays] = useState(30);
  const [statusMessage, setStatusMessage] = useState<{ type: 'success' | 'error' | 'info'; text: string } | null>(null);

  useEffect(() => {
    loadDeals();
  }, []);

  // Clear status message after 5 seconds
  useEffect(() => {
    if (statusMessage) {
      const timer = setTimeout(() => setStatusMessage(null), 5000);
      return () => clearTimeout(timer);
    }
  }, [statusMessage]);

  const loadDeals = async () => {
    setLoading(true);
    try {
      const result = await MAAnalyticsService.DealDatabase.getAllDeals();
      setDeals(result);
      setStatusMessage({ type: 'info', text: `Loaded ${result.length} deals from database` });
    } catch (error) {
      console.error('Failed to load deals:', error);
      setStatusMessage({ type: 'error', text: 'Failed to load deals from database' });
    } finally {
      setLoading(false);
    }
  };

  const scanFilings = async () => {
    setScanning(true);
    setStatusMessage({ type: 'info', text: `Scanning SEC filings from the last ${scanDays} days...` });
    try {
      const result = await MAAnalyticsService.DealDatabase.scanFilings(scanDays);
      const filingsFound = result.filings_found || 0;
      const dealsParsed = result.deals_parsed || 0;
      const dealsCreated = result.deals_created || 0;
      const totalDeals = dealsParsed + dealsCreated;

      // Store filings for display
      if (result.filings && result.filings.length > 0) {
        setRecentFilings(result.filings);
        setShowFilings(true);
      }

      setStatusMessage({
        type: 'success',
        text: `Found ${filingsFound} filings, ${totalDeals} deal${totalDeals !== 1 ? 's' : ''} added to database`,
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
    if (value >= 1e9) return `$${(value / 1e9).toFixed(2)}B`;
    if (value >= 1e6) return `$${(value / 1e6).toFixed(2)}M`;
    return `$${value.toLocaleString()}`;
  };

  const filteredDeals = deals.filter(deal =>
    deal.target_name.toLowerCase().includes(searchQuery.toLowerCase()) ||
    deal.acquirer_name.toLowerCase().includes(searchQuery.toLowerCase())
  );

  // --- Derived chart data ---

  const statusCounts = useMemo(() => {
    return deals.reduce((acc, d) => {
      const status = d.status || 'unknown';
      acc[status] = (acc[status] || 0) + 1;
      return acc;
    }, {} as Record<string, number>);
  }, [deals]);

  const pipelineData = useMemo(() => {
    return Object.entries(statusCounts).map(([status, count]) => ({
      name: status,
      value: count,
    }));
  }, [statusCounts]);

  const timelineData = useMemo(() => {
    return deals
      .filter(d => d.announced_date && d.deal_value)
      .map(d => ({
        date: new Date(d.announced_date).getTime(),
        value: d.deal_value,
        name: d.target_name,
        premium: d.premium_1day,
      }))
      .filter(d => !isNaN(d.date));
  }, [deals]);

  // Summary metrics
  const totalDealValue = useMemo(() => deals.reduce((sum, d) => sum + (d.deal_value || 0), 0), [deals]);
  const avgPremium = useMemo(() => {
    if (!deals.length) return 0;
    return deals.reduce((sum, d) => sum + (d.premium_1day || 0), 0) / deals.length;
  }, [deals]);
  const completedCount = useMemo(() => deals.filter(d => (d.status || '').toLowerCase() === 'completed').length, [deals]);

  // Find the selected deal index in filteredDeals for MADataTable highlight
  const selectedDealIndex = useMemo(() => {
    if (!selectedDeal) return undefined;
    return filteredDeals.findIndex(d => d.deal_id === selectedDeal.deal_id);
  }, [filteredDeals, selectedDeal]);

  // Deal list column definitions for left panel
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
      format: (val: any) => (
        <span style={{ color: ACCENT }}>{formatCurrency(val || 0)}</span>
      ),
    },
    {
      key: 'premium_1day',
      label: '%',
      width: '55px',
      align: 'right' as const,
      sortable: true,
      format: (val: any) => (
        <span style={{ color: val > 0 ? FINCEPT.GREEN : FINCEPT.RED }}>
          {val > 0 ? '+' : ''}{(val || 0).toFixed(1)}%
        </span>
      ),
    },
  ], []);

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%', overflow: 'hidden' }}>
      {/* TOP HEADER BAR */}
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
          <span style={{
            fontSize: TYPOGRAPHY.TINY,
            fontFamily: TYPOGRAPHY.MONO,
            color: FINCEPT.MUTED,
            marginLeft: SPACING.SMALL,
          }}>
            {deals.length} deal{deals.length !== 1 ? 's' : ''} tracked
          </span>
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
                   statusMessage.type === 'success' ? FINCEPT.GREEN :
                   ACCENT,
            display: 'flex',
            alignItems: 'center',
            gap: SPACING.SMALL,
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
              display: 'flex',
              alignItems: 'center',
              gap: SPACING.SMALL,
              textTransform: 'uppercase' as const,
            }}
          >
            <Scan size={11} className={scanning ? 'animate-spin' : ''} />
            {scanning ? 'SCANNING...' : 'SCAN SEC'}
          </button>
        </div>

        <MAExportButton data={deals} filename="deal_database" accentColor={ACCENT} />
      </div>

      {/* MAIN CONTENT: LEFT PANEL + RIGHT PANEL */}
      <div style={{ display: 'flex', flex: 1, overflow: 'hidden' }}>
        {/* LEFT - Deal List Panel (300px) */}
        <div style={{
          width: '300px',
          backgroundColor: FINCEPT.PANEL_BG,
          borderRight: `1px solid ${FINCEPT.BORDER}`,
          display: 'flex',
          flexDirection: 'column',
          overflow: 'hidden',
          flexShrink: 0,
        }}>
          {/* Search Bar */}
          <div style={{
            padding: SPACING.DEFAULT,
            borderBottom: `1px solid ${FINCEPT.BORDER}`,
            display: 'flex',
            gap: SPACING.SMALL,
            alignItems: 'center',
          }}>
            <div style={{ position: 'relative', flex: 1 }}>
              <Search
                size={11}
                style={{
                  position: 'absolute',
                  left: '8px',
                  top: '50%',
                  transform: 'translateY(-50%)',
                  color: FINCEPT.MUTED,
                }}
              />
              <input
                type="text"
                placeholder="Search deals..."
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
              title="Refresh deals"
              style={{
                padding: SPACING.SMALL,
                backgroundColor: 'transparent',
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '2px',
                color: FINCEPT.GRAY,
                cursor: loading ? 'not-allowed' : 'pointer',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                transition: 'all 0.15s',
              }}
              onMouseEnter={(e) => {
                e.currentTarget.style.borderColor = ACCENT;
                e.currentTarget.style.color = ACCENT;
              }}
              onMouseLeave={(e) => {
                e.currentTarget.style.borderColor = FINCEPT.BORDER;
                e.currentTarget.style.color = FINCEPT.GRAY;
              }}
            >
              <RefreshCw size={12} className={loading ? 'animate-spin' : ''} />
            </button>
          </div>

          {/* Deal List */}
          <div style={{ flex: 1, overflow: 'hidden', display: 'flex', flexDirection: 'column' }}>
            {loading ? (
              <div style={{ ...COMMON_STYLES.emptyState, padding: SPACING.XLARGE }}>
                <div
                  className="animate-spin"
                  style={{
                    width: '20px',
                    height: '20px',
                    border: `2px solid ${FINCEPT.BORDER}`,
                    borderTop: `2px solid ${ACCENT}`,
                    borderRadius: '50%',
                    marginBottom: SPACING.SMALL,
                  }}
                />
                <span style={{ fontFamily: TYPOGRAPHY.MONO, fontSize: TYPOGRAPHY.TINY }}>Loading deals...</span>
              </div>
            ) : filteredDeals.length === 0 ? (
              <div style={{ ...COMMON_STYLES.emptyState, padding: SPACING.XLARGE }}>
                <Database size={28} style={{ marginBottom: SPACING.SMALL, opacity: 0.3, color: ACCENT }} />
                <span style={{ fontSize: TYPOGRAPHY.SMALL, color: FINCEPT.GRAY, fontFamily: TYPOGRAPHY.MONO }}>
                  {searchQuery ? 'No matching deals' : 'No deals tracked yet'}
                </span>
                <div style={{
                  fontSize: TYPOGRAPHY.TINY,
                  fontFamily: TYPOGRAPHY.MONO,
                  color: FINCEPT.MUTED,
                  marginTop: SPACING.SMALL,
                  textAlign: 'center',
                  maxWidth: '220px',
                }}>
                  {searchQuery ? (
                    'Try a different search term'
                  ) : (
                    <>
                      Click <strong style={{ color: ACCENT }}>SCAN SEC</strong> above to discover M&A deals from SEC EDGAR
                    </>
                  )}
                </div>
              </div>
            ) : (
              <MADataTable
                columns={dealListColumns}
                data={filteredDeals as any}
                accentColor={ACCENT}
                compact
                maxHeight="100%"
                onRowClick={(row) => setSelectedDeal(row as MADeal)}
                selectedIndex={selectedDealIndex}
                emptyMessage="No deals found"
              />
            )}
          </div>
        </div>

        {/* RIGHT - Deal Details & Charts */}
        <div style={{
          flex: 1,
          backgroundColor: FINCEPT.DARK_BG,
          display: 'flex',
          flexDirection: 'column',
          overflow: 'hidden',
        }}>
          {selectedDeal ? (
            <div style={{ display: 'flex', flexDirection: 'column', height: '100%', overflow: 'hidden' }}>
              {/* Deal Header */}
              <div style={{
                padding: SPACING.LARGE,
                backgroundColor: FINCEPT.HEADER_BG,
                borderBottom: `1px solid ${ACCENT}40`,
                flexShrink: 0,
              }}>
                <div style={{
                  display: 'flex',
                  justifyContent: 'space-between',
                  alignItems: 'flex-start',
                }}>
                  <div>
                    <div style={{
                      fontSize: TYPOGRAPHY.HEADING,
                      fontFamily: TYPOGRAPHY.MONO,
                      fontWeight: TYPOGRAPHY.BOLD,
                      color: FINCEPT.WHITE,
                    }}>
                      {selectedDeal.target_name}
                    </div>
                    <div style={{
                      fontSize: TYPOGRAPHY.SMALL,
                      fontFamily: TYPOGRAPHY.MONO,
                      color: FINCEPT.GRAY,
                      marginTop: SPACING.TINY,
                      display: 'flex',
                      alignItems: 'center',
                      gap: SPACING.SMALL,
                    }}>
                      <TrendingUp size={11} style={{ color: ACCENT }} />
                      <span>Acquired by <span style={{ color: FINCEPT.WHITE }}>{selectedDeal.acquirer_name}</span></span>
                    </div>
                  </div>
                  <div style={{ textAlign: 'right' }}>
                    <div style={{
                      fontSize: TYPOGRAPHY.HEADING,
                      fontFamily: TYPOGRAPHY.MONO,
                      fontWeight: TYPOGRAPHY.BOLD,
                      color: ACCENT,
                    }}>
                      {formatCurrency(selectedDeal.deal_value)}
                    </div>
                    <div style={{
                      fontSize: TYPOGRAPHY.TINY,
                      fontFamily: TYPOGRAPHY.MONO,
                      color: FINCEPT.MUTED,
                      marginTop: SPACING.TINY,
                      letterSpacing: TYPOGRAPHY.WIDE,
                    }}>
                      DEAL VALUE
                    </div>
                  </div>
                </div>

                {/* Status Badges */}
                <div style={{ marginTop: SPACING.DEFAULT, display: 'flex', gap: SPACING.SMALL }}>
                  <span style={{
                    ...COMMON_STYLES.badgeInfo,
                    backgroundColor: selectedDeal.status === 'Completed' ? `${FINCEPT.GREEN}20` :
                                     selectedDeal.status === 'Pending' || selectedDeal.status === 'pending' ? `${'#FFC400'}20` :
                                     `${ACCENT}20`,
                    color: selectedDeal.status === 'Completed' ? FINCEPT.GREEN :
                           selectedDeal.status === 'Pending' || selectedDeal.status === 'pending' ? '#FFC400' :
                           ACCENT,
                    fontFamily: TYPOGRAPHY.MONO,
                    letterSpacing: TYPOGRAPHY.WIDE,
                  }}>
                    {selectedDeal.status.toUpperCase()}
                  </span>
                  <span style={{
                    ...COMMON_STYLES.badgeInfo,
                    backgroundColor: `${ACCENT}20`,
                    color: ACCENT,
                    fontFamily: TYPOGRAPHY.MONO,
                    letterSpacing: TYPOGRAPHY.WIDE,
                  }}>
                    {selectedDeal.industry.toUpperCase()}
                  </span>
                  <span style={{
                    ...COMMON_STYLES.badgeInfo,
                    fontFamily: TYPOGRAPHY.MONO,
                  }}>
                    {selectedDeal.announced_date}
                  </span>
                </div>
              </div>

              {/* Scrollable content */}
              <div style={{ flex: 1, overflow: 'auto', padding: SPACING.LARGE }}>
                {/* Key Metrics */}
                <MASectionHeader
                  title="Key Metrics"
                  icon={<BarChart3 size={13} />}
                  accentColor={ACCENT}
                />
                <div style={{
                  display: 'grid',
                  gridTemplateColumns: 'repeat(3, 1fr)',
                  gap: SPACING.DEFAULT,
                  marginBottom: SPACING.LARGE,
                }}>
                  <MAMetricCard
                    label="Premium (1-Day)"
                    value={`${selectedDeal.premium_1day > 0 ? '+' : ''}${selectedDeal.premium_1day.toFixed(1)}%`}
                    accentColor={selectedDeal.premium_1day > 0 ? FINCEPT.GREEN : FINCEPT.RED}
                    trend={selectedDeal.premium_1day > 0 ? 'up' : selectedDeal.premium_1day < 0 ? 'down' : 'neutral'}
                  />
                  <MAMetricCard
                    label="Cash Component"
                    value={`${selectedDeal.payment_cash_pct}%`}
                    accentColor={ACCENT}
                    subtitle={`Stock: ${selectedDeal.payment_stock_pct}%`}
                  />
                  <MAMetricCard
                    label="Announced"
                    value={selectedDeal.announced_date}
                    accentColor={FINCEPT.GRAY}
                    format="text"
                  />
                  {selectedDeal.ev_revenue != null && (
                    <MAMetricCard
                      label="EV / Revenue"
                      value={`${selectedDeal.ev_revenue.toFixed(2)}x`}
                      accentColor={FINCEPT.CYAN}
                    />
                  )}
                  {selectedDeal.ev_ebitda != null && (
                    <MAMetricCard
                      label="EV / EBITDA"
                      value={`${selectedDeal.ev_ebitda.toFixed(2)}x`}
                      accentColor={FINCEPT.CYAN}
                    />
                  )}
                  {selectedDeal.synergies != null && (
                    <MAMetricCard
                      label="Est. Synergies"
                      value={formatCurrency(selectedDeal.synergies)}
                      accentColor={FINCEPT.GREEN}
                    />
                  )}
                </div>

                {/* Payment Structure Detail */}
                <MASectionHeader
                  title="Payment Structure"
                  accentColor={ACCENT}
                />
                <div style={{
                  display: 'flex',
                  gap: SPACING.SMALL,
                  marginBottom: SPACING.LARGE,
                  height: '6px',
                  borderRadius: '2px',
                  overflow: 'hidden',
                  backgroundColor: FINCEPT.PANEL_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                }}>
                  {selectedDeal.payment_cash_pct > 0 && (
                    <div
                      style={{
                        width: `${selectedDeal.payment_cash_pct}%`,
                        backgroundColor: ACCENT,
                        borderRadius: '1px',
                      }}
                      title={`Cash: ${selectedDeal.payment_cash_pct}%`}
                    />
                  )}
                  {selectedDeal.payment_stock_pct > 0 && (
                    <div
                      style={{
                        width: `${selectedDeal.payment_stock_pct}%`,
                        backgroundColor: FINCEPT.CYAN,
                        borderRadius: '1px',
                      }}
                      title={`Stock: ${selectedDeal.payment_stock_pct}%`}
                    />
                  )}
                </div>
                <div style={{
                  display: 'flex',
                  gap: SPACING.LARGE,
                  marginBottom: SPACING.LARGE,
                  fontSize: TYPOGRAPHY.TINY,
                  fontFamily: TYPOGRAPHY.MONO,
                }}>
                  <span style={{ display: 'flex', alignItems: 'center', gap: SPACING.SMALL }}>
                    <span style={{ width: '8px', height: '8px', backgroundColor: ACCENT, borderRadius: '1px', display: 'inline-block' }} />
                    <span style={{ color: FINCEPT.GRAY }}>Cash {selectedDeal.payment_cash_pct}%</span>
                  </span>
                  <span style={{ display: 'flex', alignItems: 'center', gap: SPACING.SMALL }}>
                    <span style={{ width: '8px', height: '8px', backgroundColor: FINCEPT.CYAN, borderRadius: '1px', display: 'inline-block' }} />
                    <span style={{ color: FINCEPT.GRAY }}>Stock {selectedDeal.payment_stock_pct}%</span>
                  </span>
                </div>

                {/* Pipeline & Timeline Charts (if deals exist) */}
                {deals.length > 0 && (
                  <>
                    <MASectionHeader
                      title="Pipeline Analytics"
                      icon={<BarChart3 size={13} />}
                      accentColor={ACCENT}
                    />
                    <div style={{
                      display: 'grid',
                      gridTemplateColumns: '1fr 1fr',
                      gap: SPACING.DEFAULT,
                      marginBottom: SPACING.LARGE,
                    }}>
                      {/* Deal Pipeline Status Pie */}
                      {pipelineData.length > 0 && (
                        <MAChartPanel
                          title="Deal Pipeline Status"
                          icon={<BarChart3 size={12} />}
                          accentColor={ACCENT}
                          height={200}
                        >
                          <PieChart>
                            <Pie
                              data={pipelineData}
                              dataKey="value"
                              nameKey="name"
                              cx="50%"
                              cy="50%"
                              outerRadius={70}
                              label={({ name, value }) => `${name} (${value})`}
                              labelLine={{ stroke: FINCEPT.MUTED }}
                              style={{ fontSize: '9px', fontFamily: 'var(--ft-font-family, monospace)' }}
                            >
                              {pipelineData.map((entry, i) => (
                                <Cell key={i} fill={STATUS_COLORS[entry.name] || CHART_PALETTE[i % CHART_PALETTE.length]} />
                              ))}
                            </Pie>
                            <Legend
                              wrapperStyle={{ fontSize: '9px', fontFamily: 'var(--ft-font-family, monospace)' }}
                            />
                            <Tooltip contentStyle={CHART_STYLE.tooltip} />
                          </PieChart>
                        </MAChartPanel>
                      )}

                      {/* Deal Value Timeline Scatter */}
                      {timelineData.length > 0 && (
                        <MAChartPanel
                          title="Deal Value Timeline"
                          icon={<Clock size={12} />}
                          accentColor={ACCENT}
                          height={200}
                        >
                          <ScatterChart margin={{ top: 10, right: 10, bottom: 10, left: 10 }}>
                            <CartesianGrid stroke={CHART_STYLE.grid.stroke} strokeDasharray={CHART_STYLE.grid.strokeDasharray} />
                            <XAxis
                              type="number"
                              dataKey="date"
                              domain={['auto', 'auto']}
                              tickFormatter={(val) => new Date(val).toLocaleDateString()}
                              tick={CHART_STYLE.axis}
                              stroke={FINCEPT.BORDER}
                            />
                            <YAxis
                              dataKey="value"
                              tick={CHART_STYLE.axis}
                              stroke={FINCEPT.BORDER}
                              tickFormatter={(val) => {
                                if (val >= 1e9) return `$${(val / 1e9).toFixed(0)}B`;
                                if (val >= 1e6) return `$${(val / 1e6).toFixed(0)}M`;
                                return `$${val}`;
                              }}
                            />
                            <ZAxis dataKey="premium" range={[20, 200]} />
                            <Scatter data={timelineData} fill={ACCENT} />
                            <Tooltip
                              contentStyle={CHART_STYLE.tooltip}
                              formatter={(value: any, name: string) => {
                                if (name === 'date') return [new Date(value).toLocaleDateString(), 'Date'];
                                if (name === 'value') return [formatCurrency(value), 'Deal Value'];
                                if (name === 'premium') return [`${value > 0 ? '+' : ''}${value.toFixed(1)}%`, 'Premium'];
                                return [value, name];
                              }}
                              labelFormatter={() => ''}
                            />
                          </ScatterChart>
                        </MAChartPanel>
                      )}
                    </div>
                  </>
                )}
              </div>
            </div>
          ) : (
            /* No deal selected -- show charts, filings, or empty state */
            <div style={{ display: 'flex', flexDirection: 'column', height: '100%', overflow: 'hidden' }}>
              {deals.length > 0 ? (
                <div style={{ flex: 1, overflow: 'auto', padding: SPACING.LARGE }}>
                  {/* Summary Metrics */}
                  <MASectionHeader
                    title="Portfolio Summary"
                    subtitle={`${deals.length} deals`}
                    icon={<BarChart3 size={13} />}
                    accentColor={ACCENT}
                  />
                  <div style={{
                    display: 'grid',
                    gridTemplateColumns: 'repeat(4, 1fr)',
                    gap: SPACING.DEFAULT,
                    marginBottom: SPACING.LARGE,
                  }}>
                    <MAMetricCard
                      label="Total Deals"
                      value={String(deals.length)}
                      accentColor={ACCENT}
                    />
                    <MAMetricCard
                      label="Total Value"
                      value={formatCurrency(totalDealValue)}
                      accentColor={ACCENT}
                    />
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

                  {/* Charts Row */}
                  <div style={{
                    display: 'grid',
                    gridTemplateColumns: '1fr 1fr',
                    gap: SPACING.DEFAULT,
                    marginBottom: SPACING.LARGE,
                  }}>
                    {/* Pipeline Pie Chart */}
                    {pipelineData.length > 0 && (
                      <MAChartPanel
                        title="Deal Pipeline Status"
                        icon={<BarChart3 size={12} />}
                        accentColor={ACCENT}
                        height={220}
                      >
                        <PieChart>
                          <Pie
                            data={pipelineData}
                            dataKey="value"
                            nameKey="name"
                            cx="50%"
                            cy="50%"
                            outerRadius={70}
                            label={({ name, value }) => `${name} (${value})`}
                            labelLine={{ stroke: FINCEPT.MUTED }}
                            style={{ fontSize: '9px', fontFamily: 'var(--ft-font-family, monospace)' }}
                          >
                            {pipelineData.map((entry, i) => (
                              <Cell key={i} fill={STATUS_COLORS[entry.name] || CHART_PALETTE[i % CHART_PALETTE.length]} />
                            ))}
                          </Pie>
                          <Legend
                            wrapperStyle={{ fontSize: '9px', fontFamily: 'var(--ft-font-family, monospace)' }}
                          />
                          <Tooltip contentStyle={CHART_STYLE.tooltip} />
                        </PieChart>
                      </MAChartPanel>
                    )}

                    {/* Timeline Scatter Chart */}
                    {timelineData.length > 0 && (
                      <MAChartPanel
                        title="Deal Value Timeline"
                        icon={<Clock size={12} />}
                        accentColor={ACCENT}
                        height={220}
                      >
                        <ScatterChart margin={{ top: 10, right: 10, bottom: 10, left: 10 }}>
                          <CartesianGrid stroke={CHART_STYLE.grid.stroke} strokeDasharray={CHART_STYLE.grid.strokeDasharray} />
                          <XAxis
                            type="number"
                            dataKey="date"
                            domain={['auto', 'auto']}
                            tickFormatter={(val) => new Date(val).toLocaleDateString()}
                            tick={CHART_STYLE.axis}
                            stroke={FINCEPT.BORDER}
                          />
                          <YAxis
                            dataKey="value"
                            tick={CHART_STYLE.axis}
                            stroke={FINCEPT.BORDER}
                            tickFormatter={(val) => {
                              if (val >= 1e9) return `$${(val / 1e9).toFixed(0)}B`;
                              if (val >= 1e6) return `$${(val / 1e6).toFixed(0)}M`;
                              return `$${val}`;
                            }}
                          />
                          <ZAxis dataKey="premium" range={[20, 200]} />
                          <Scatter data={timelineData} fill={ACCENT} />
                          <Tooltip
                            contentStyle={CHART_STYLE.tooltip}
                            formatter={(value: any, name: string) => {
                              if (name === 'date') return [new Date(value).toLocaleDateString(), 'Date'];
                              if (name === 'value') return [formatCurrency(value), 'Deal Value'];
                              if (name === 'premium') return [`${value > 0 ? '+' : ''}${value.toFixed(1)}%`, 'Premium'];
                              return [value, name];
                            }}
                            labelFormatter={() => ''}
                          />
                        </ScatterChart>
                      </MAChartPanel>
                    )}
                  </div>

                  {/* Recent SEC Filings */}
                  {showFilings && recentFilings.length > 0 && (
                    <>
                      <MASectionHeader
                        title="Recent SEC Filings"
                        icon={<FileText size={13} />}
                        accentColor={ACCENT}
                        action={
                          <button
                            onClick={() => setShowFilings(false)}
                            style={{
                              ...COMMON_STYLES.buttonSecondary,
                              padding: '3px 8px',
                              fontSize: TYPOGRAPHY.TINY,
                            }}
                          >
                            HIDE
                          </button>
                        }
                      />
                      <MADataTable
                        columns={filingsColumns}
                        data={recentFilings as any}
                        accentColor={ACCENT}
                        compact
                        maxHeight="300px"
                        emptyMessage="No filings found"
                      />
                    </>
                  )}
                </div>
              ) : showFilings && recentFilings.length > 0 ? (
                /* No deals but filings scanned */
                <div style={{ flex: 1, overflow: 'auto', padding: SPACING.LARGE }}>
                  <MASectionHeader
                    title="Recent SEC Filings"
                    icon={<FileText size={13} />}
                    accentColor={ACCENT}
                    action={
                      <button
                        onClick={() => setShowFilings(false)}
                        style={{
                          ...COMMON_STYLES.buttonSecondary,
                          padding: '3px 8px',
                          fontSize: TYPOGRAPHY.TINY,
                        }}
                      >
                        HIDE
                      </button>
                    }
                  />
                  <MADataTable
                    columns={filingsColumns}
                    data={recentFilings as any}
                    accentColor={ACCENT}
                    compact
                    maxHeight="600px"
                    emptyMessage="No filings found"
                  />
                </div>
              ) : (
                /* Full empty state */
                <MAEmptyState
                  icon={<Database size={48} />}
                  title="Deal Database"
                  description="Scan SEC filings or select a deal to view details"
                  accentColor={ACCENT}
                  actionLabel="Scan SEC Filings"
                  onAction={scanFilings}
                />
              )}
            </div>
          )}
        </div>
      </div>
    </div>
  );
};
