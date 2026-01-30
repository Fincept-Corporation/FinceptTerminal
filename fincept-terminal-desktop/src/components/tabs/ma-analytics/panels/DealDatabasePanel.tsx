/**
 * Deal Database Panel - M&A Deal Tracker & SEC Filing Scanner
 *
 * Three-panel layout:
 * - LEFT: Deal list & search
 * - CENTER: Deal details & financials
 * - RIGHT: SEC filing scanner & actions
 */

import React, { useState, useEffect } from 'react';
import {
  Search,
  RefreshCw,
  Plus,
  FileText,
  TrendingUp,
  DollarSign,
  Calendar,
  Building2,
  Filter,
  Download,
  Eye,
  Database,
} from 'lucide-react';

import { FINCEPT, TYPOGRAPHY, SPACING, COMMON_STYLES } from '../../portfolio-tab/finceptStyles';
import { MAAnalyticsService, type MADeal } from '@/services/maAnalyticsService';

interface DealDatabasePanelProps {
  onSelectDeal: (deal: MADeal | null) => void;
}

export const DealDatabasePanel: React.FC<DealDatabasePanelProps> = ({ onSelectDeal }) => {
  const [deals, setDeals] = useState<MADeal[]>([]);
  const [selectedDeal, setSelectedDeal] = useState<MADeal | null>(null);
  const [searchQuery, setSearchQuery] = useState('');
  const [loading, setLoading] = useState(false);
  const [scanningFilings, setScanningFilings] = useState(false);
  const [recentFilings, setRecentFilings] = useState<any[]>([]);

  // Load deals on mount
  useEffect(() => {
    loadDeals();
  }, []);

  const loadDeals = async () => {
    setLoading(true);
    try {
      const result = await MAAnalyticsService.DealDatabase.getAllDeals();
      setDeals(result);
    } catch (error) {
      console.error('Failed to load deals:', error);
    } finally {
      setLoading(false);
    }
  };

  const handleSelectDeal = (deal: MADeal) => {
    setSelectedDeal(deal);
    onSelectDeal(deal);
  };

  const handleScanFilings = async () => {
    setScanningFilings(true);
    try {
      const result = await MAAnalyticsService.DealDatabase.scanFilings(30);
      setRecentFilings(JSON.parse(result as string) || []);
    } catch (error) {
      console.error('Failed to scan filings:', error);
    } finally {
      setScanningFilings(false);
    }
  };

  const handleSearch = async () => {
    if (!searchQuery.trim()) {
      loadDeals();
      return;
    }
    setLoading(true);
    try {
      const results = await MAAnalyticsService.DealDatabase.searchDeals(searchQuery);
      setDeals(results);
    } catch (error) {
      console.error('Search failed:', error);
    } finally {
      setLoading(false);
    }
  };

  const formatCurrency = (value: number) => {
    if (value >= 1_000_000_000) {
      return `$${(value / 1_000_000_000).toFixed(2)}B`;
    } else if (value >= 1_000_000) {
      return `$${(value / 1_000_000).toFixed(2)}M`;
    }
    return `$${value.toLocaleString()}`;
  };

  return (
    <div
      style={{
        display: 'flex',
        height: '100%',
        overflow: 'hidden',
      }}
    >
      {/* LEFT PANEL - Deal List */}
      <div
        style={{
          width: '320px',
          backgroundColor: FINCEPT.PANEL_BG,
          borderRight: `1px solid ${FINCEPT.BORDER}`,
          display: 'flex',
          flexDirection: 'column',
          overflow: 'hidden',
        }}
      >
        {/* Search Header */}
        <div
          style={{
            padding: SPACING.DEFAULT,
            backgroundColor: FINCEPT.HEADER_BG,
            borderBottom: `1px solid ${FINCEPT.BORDER}`,
          }}
        >
          <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.SMALL }}>DEAL SEARCH</div>
          <div style={{ display: 'flex', gap: SPACING.SMALL }}>
            <div style={{ position: 'relative', flex: 1 }}>
              <Search
                size={12}
                style={{
                  position: 'absolute',
                  left: '8px',
                  top: '50%',
                  transform: 'translateY(-50%)',
                  color: FINCEPT.GRAY,
                }}
              />
              <input
                type="text"
                placeholder="Search deals..."
                value={searchQuery}
                onChange={(e) => setSearchQuery(e.target.value)}
                onKeyPress={(e) => e.key === 'Enter' && handleSearch()}
                style={{
                  ...COMMON_STYLES.inputField,
                  paddingLeft: '28px',
                  fontSize: TYPOGRAPHY.SMALL,
                }}
              />
            </div>
            <button
              onClick={handleSearch}
              style={{
                ...COMMON_STYLES.buttonPrimary,
                padding: '8px',
              }}
              title="Search"
            >
              <Search size={12} />
            </button>
            <button
              onClick={loadDeals}
              style={{
                ...COMMON_STYLES.buttonSecondary,
                padding: '8px',
              }}
              title="Refresh"
            >
              <RefreshCw size={12} />
            </button>
          </div>
        </div>

        {/* Deal List */}
        <div
          style={{
            flex: 1,
            overflow: 'auto',
          }}
        >
          {loading ? (
            <div style={{ ...COMMON_STYLES.emptyState, padding: SPACING.XLARGE }}>
              <div
                className="animate-spin"
                style={{
                  width: '20px',
                  height: '20px',
                  border: `2px solid ${FINCEPT.BORDER}`,
                  borderTop: `2px solid ${FINCEPT.ORANGE}`,
                  borderRadius: '50%',
                  marginBottom: SPACING.SMALL,
                }}
              />
              <span>Loading deals...</span>
            </div>
          ) : deals.length === 0 ? (
            <div style={{ ...COMMON_STYLES.emptyState, padding: SPACING.XLARGE }}>
              <Database size={32} style={{ marginBottom: SPACING.SMALL, opacity: 0.3 }} />
              <span>No deals found</span>
              <button onClick={handleScanFilings} style={{ ...COMMON_STYLES.buttonPrimary, marginTop: SPACING.DEFAULT }}>
                <Plus size={12} style={{ marginRight: SPACING.TINY }} />
                SCAN FILINGS
              </button>
            </div>
          ) : (
            deals.map((deal) => {
              const isSelected = selectedDeal?.deal_id === deal.deal_id;
              return (
                <div
                  key={deal.deal_id}
                  onClick={() => handleSelectDeal(deal)}
                  style={{
                    padding: SPACING.DEFAULT,
                    backgroundColor: isSelected ? `${FINCEPT.ORANGE}15` : 'transparent',
                    borderLeft: isSelected ? `2px solid ${FINCEPT.ORANGE}` : '2px solid transparent',
                    borderBottom: `1px solid ${FINCEPT.BORDER}`,
                    cursor: 'pointer',
                    transition: 'all 0.2s',
                  }}
                  onMouseEnter={(e) => {
                    if (!isSelected) {
                      e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                    }
                  }}
                  onMouseLeave={(e) => {
                    if (!isSelected) {
                      e.currentTarget.style.backgroundColor = 'transparent';
                    }
                  }}
                >
                  <div style={{ fontSize: TYPOGRAPHY.SMALL, fontWeight: TYPOGRAPHY.BOLD, color: FINCEPT.WHITE }}>
                    {deal.target_name}
                  </div>
                  <div style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY, marginTop: SPACING.TINY }}>
                    {deal.acquirer_name}
                  </div>
                  <div
                    style={{
                      display: 'flex',
                      justifyContent: 'space-between',
                      marginTop: SPACING.SMALL,
                      fontSize: TYPOGRAPHY.TINY,
                    }}
                  >
                    <span style={{ color: FINCEPT.CYAN }}>{formatCurrency(deal.deal_value)}</span>
                    <span
                      style={{
                        color: deal.premium_1day > 0 ? FINCEPT.GREEN : FINCEPT.RED,
                      }}
                    >
                      {deal.premium_1day > 0 ? '+' : ''}
                      {deal.premium_1day.toFixed(1)}%
                    </span>
                  </div>
                  <div style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.MUTED, marginTop: SPACING.TINY }}>
                    {deal.announced_date}
                  </div>
                </div>
              );
            })
          )}
        </div>
      </div>

      {/* CENTER PANEL - Deal Details */}
      <div
        style={{
          flex: 1,
          backgroundColor: FINCEPT.DARK_BG,
          display: 'flex',
          flexDirection: 'column',
          overflow: 'hidden',
        }}
      >
        {selectedDeal ? (
          <>
            {/* Deal Header */}
            <div
              style={{
                padding: SPACING.LARGE,
                backgroundColor: FINCEPT.HEADER_BG,
                borderBottom: `2px solid ${FINCEPT.ORANGE}`,
              }}
            >
              <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start' }}>
                <div>
                  <div style={{ fontSize: TYPOGRAPHY.HEADING, fontWeight: TYPOGRAPHY.BOLD, color: FINCEPT.WHITE }}>
                    {selectedDeal.target_name}
                  </div>
                  <div
                    style={{
                      fontSize: TYPOGRAPHY.BODY,
                      color: FINCEPT.GRAY,
                      marginTop: SPACING.TINY,
                      display: 'flex',
                      alignItems: 'center',
                      gap: SPACING.SMALL,
                    }}
                  >
                    <Building2 size={12} />
                    <span>Acquired by {selectedDeal.acquirer_name}</span>
                  </div>
                </div>
                <div style={{ textAlign: 'right' }}>
                  <div style={{ fontSize: TYPOGRAPHY.HEADING, fontWeight: TYPOGRAPHY.BOLD, color: FINCEPT.CYAN }}>
                    {formatCurrency(selectedDeal.deal_value)}
                  </div>
                  <div style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY, marginTop: SPACING.TINY }}>
                    DEAL VALUE
                  </div>
                </div>
              </div>

              {/* Status Badge */}
              <div style={{ marginTop: SPACING.DEFAULT }}>
                <span
                  style={{
                    ...COMMON_STYLES.badgeInfo,
                    backgroundColor: selectedDeal.status === 'Completed' ? `${FINCEPT.GREEN}20` : `${FINCEPT.YELLOW}20`,
                    color: selectedDeal.status === 'Completed' ? FINCEPT.GREEN : FINCEPT.YELLOW,
                  }}
                >
                  {selectedDeal.status.toUpperCase()}
                </span>
                <span
                  style={{
                    ...COMMON_STYLES.badgeInfo,
                    marginLeft: SPACING.SMALL,
                  }}
                >
                  {selectedDeal.industry.toUpperCase()}
                </span>
              </div>
            </div>

            {/* Deal Metrics */}
            <div
              style={{
                padding: SPACING.LARGE,
                overflow: 'auto',
              }}
            >
              <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.DEFAULT }}>KEY METRICS</div>

              <div
                style={{
                  display: 'grid',
                  gridTemplateColumns: '1fr 1fr',
                  gap: SPACING.DEFAULT,
                }}
              >
                {/* Premium */}
                <div style={COMMON_STYLES.metricCard}>
                  <div style={COMMON_STYLES.dataLabel}>PREMIUM (1-DAY)</div>
                  <div
                    style={{
                      fontSize: TYPOGRAPHY.HEADING,
                      fontWeight: TYPOGRAPHY.BOLD,
                      color: selectedDeal.premium_1day > 0 ? FINCEPT.GREEN : FINCEPT.RED,
                      marginTop: SPACING.TINY,
                    }}
                  >
                    {selectedDeal.premium_1day > 0 ? '+' : ''}
                    {selectedDeal.premium_1day.toFixed(1)}%
                  </div>
                </div>

                {/* Payment Structure */}
                <div style={COMMON_STYLES.metricCard}>
                  <div style={COMMON_STYLES.dataLabel}>PAYMENT STRUCTURE</div>
                  <div style={{ marginTop: SPACING.SMALL, fontSize: TYPOGRAPHY.SMALL }}>
                    <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                      <span style={{ color: FINCEPT.GRAY }}>Cash:</span>
                      <span style={{ color: FINCEPT.WHITE }}>{selectedDeal.payment_cash_pct}%</span>
                    </div>
                    <div style={{ display: 'flex', justifyContent: 'space-between', marginTop: SPACING.TINY }}>
                      <span style={{ color: FINCEPT.GRAY }}>Stock:</span>
                      <span style={{ color: FINCEPT.WHITE }}>{selectedDeal.payment_stock_pct}%</span>
                    </div>
                  </div>
                </div>

                {/* EV/Revenue */}
                {selectedDeal.ev_revenue && (
                  <div style={COMMON_STYLES.metricCard}>
                    <div style={COMMON_STYLES.dataLabel}>EV / REVENUE</div>
                    <div
                      style={{
                        fontSize: TYPOGRAPHY.HEADING,
                        fontWeight: TYPOGRAPHY.BOLD,
                        color: FINCEPT.CYAN,
                        marginTop: SPACING.TINY,
                      }}
                    >
                      {selectedDeal.ev_revenue.toFixed(2)}x
                    </div>
                  </div>
                )}

                {/* EV/EBITDA */}
                {selectedDeal.ev_ebitda && (
                  <div style={COMMON_STYLES.metricCard}>
                    <div style={COMMON_STYLES.dataLabel}>EV / EBITDA</div>
                    <div
                      style={{
                        fontSize: TYPOGRAPHY.HEADING,
                        fontWeight: TYPOGRAPHY.BOLD,
                        color: FINCEPT.CYAN,
                        marginTop: SPACING.TINY,
                      }}
                    >
                      {selectedDeal.ev_ebitda.toFixed(2)}x
                    </div>
                  </div>
                )}

                {/* Synergies */}
                {selectedDeal.synergies && (
                  <div style={COMMON_STYLES.metricCard}>
                    <div style={COMMON_STYLES.dataLabel}>EST. SYNERGIES</div>
                    <div
                      style={{
                        fontSize: TYPOGRAPHY.HEADING,
                        fontWeight: TYPOGRAPHY.BOLD,
                        color: FINCEPT.GREEN,
                        marginTop: SPACING.TINY,
                      }}
                    >
                      {formatCurrency(selectedDeal.synergies)}
                    </div>
                  </div>
                )}

                {/* Announced Date */}
                <div style={COMMON_STYLES.metricCard}>
                  <div style={COMMON_STYLES.dataLabel}>ANNOUNCED</div>
                  <div
                    style={{
                      fontSize: TYPOGRAPHY.SMALL,
                      color: FINCEPT.WHITE,
                      marginTop: SPACING.TINY,
                    }}
                  >
                    {selectedDeal.announced_date}
                  </div>
                </div>
              </div>
            </div>
          </>
        ) : (
          <div style={COMMON_STYLES.emptyState}>
            <Eye size={32} style={{ marginBottom: SPACING.SMALL, opacity: 0.3 }} />
            <span>Select a deal to view details</span>
          </div>
        )}
      </div>

      {/* RIGHT PANEL - SEC Filing Scanner */}
      <div
        style={{
          width: '300px',
          backgroundColor: FINCEPT.PANEL_BG,
          borderLeft: `1px solid ${FINCEPT.BORDER}`,
          display: 'flex',
          flexDirection: 'column',
          overflow: 'hidden',
        }}
      >
        {/* Scanner Header */}
        <div
          style={{
            padding: SPACING.DEFAULT,
            backgroundColor: FINCEPT.HEADER_BG,
            borderBottom: `1px solid ${FINCEPT.BORDER}`,
          }}
        >
          <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.SMALL }}>SEC FILING SCANNER</div>
          <button
            onClick={handleScanFilings}
            disabled={scanningFilings}
            style={{
              ...COMMON_STYLES.buttonPrimary,
              width: '100%',
              opacity: scanningFilings ? 0.7 : 1,
            }}
          >
            {scanningFilings ? (
              <>
                <div
                  className="animate-spin"
                  style={{
                    width: '12px',
                    height: '12px',
                    border: `2px solid ${FINCEPT.DARK_BG}`,
                    borderTop: `2px solid transparent`,
                    borderRadius: '50%',
                    marginRight: SPACING.TINY,
                    display: 'inline-block',
                  }}
                />
                SCANNING...
              </>
            ) : (
              <>
                <RefreshCw size={12} style={{ marginRight: SPACING.TINY }} />
                SCAN RECENT FILINGS
              </>
            )}
          </button>
        </div>

        {/* Recent Filings List */}
        <div
          style={{
            flex: 1,
            overflow: 'auto',
            padding: SPACING.DEFAULT,
          }}
        >
          {recentFilings.length === 0 ? (
            <div style={COMMON_STYLES.emptyState}>
              <FileText size={24} style={{ marginBottom: SPACING.SMALL, opacity: 0.3 }} />
              <span style={{ fontSize: TYPOGRAPHY.TINY }}>No recent filings</span>
            </div>
          ) : (
            recentFilings.map((filing, index) => (
              <div
                key={index}
                style={{
                  ...COMMON_STYLES.metricCard,
                  marginBottom: SPACING.SMALL,
                }}
              >
                <div style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.ORANGE, fontWeight: TYPOGRAPHY.BOLD }}>
                  {filing.filing_type}
                </div>
                <div style={{ fontSize: TYPOGRAPHY.SMALL, color: FINCEPT.WHITE, marginTop: SPACING.TINY }}>
                  {filing.company_name}
                </div>
                <div style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY, marginTop: SPACING.TINY }}>
                  {filing.filing_date}
                </div>
                <div
                  style={{
                    marginTop: SPACING.SMALL,
                    display: 'flex',
                    justifyContent: 'space-between',
                    alignItems: 'center',
                  }}
                >
                  <span
                    style={{
                      fontSize: TYPOGRAPHY.TINY,
                      color: FINCEPT.CYAN,
                    }}
                  >
                    Confidence: {filing.confidence_score}%
                  </span>
                  <button
                    style={{
                      ...COMMON_STYLES.buttonSecondary,
                      fontSize: '7px',
                      padding: '4px 6px',
                    }}
                  >
                    VIEW
                  </button>
                </div>
              </div>
            ))
          )}
        </div>
      </div>
    </div>
  );
};
