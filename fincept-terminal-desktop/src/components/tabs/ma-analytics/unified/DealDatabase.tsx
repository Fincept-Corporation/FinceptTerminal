/**
 * Deal Database - Track and Save M&A Deals
 *
 * Simple deal tracking and management with SEC filing scanner
 */

import React, { useState, useEffect } from 'react';
import { Database, Search, FileText, TrendingUp, RefreshCw, Scan, AlertCircle } from 'lucide-react';
import { FINCEPT, TYPOGRAPHY, SPACING, COMMON_STYLES } from '../../portfolio-tab/finceptStyles';
import { MAAnalyticsService, type MADeal } from '@/services/maAnalyticsService';

export const DealDatabase: React.FC = () => {
  const [deals, setDeals] = useState<MADeal[]>([]);
  const [selectedDeal, setSelectedDeal] = useState<MADeal | null>(null);
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
      const count = Array.isArray(result) ? result.length : 0;
      setStatusMessage({ type: 'success', text: `Found ${count} M&A filings. Refreshing deals...` });
      // Reload deals after scanning
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

  return (
    <div style={{ display: 'flex', height: '100%', overflow: 'hidden' }}>
      {/* LEFT - Deal List */}
      <div style={{
        width: '350px',
        backgroundColor: FINCEPT.PANEL_BG,
        borderRight: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex',
        flexDirection: 'column',
        overflow: 'hidden',
      }}>
        {/* Search Header */}
        <div style={{
          padding: SPACING.DEFAULT,
          backgroundColor: FINCEPT.HEADER_BG,
          borderBottom: `1px solid ${FINCEPT.BORDER}`,
        }}>
          <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.SMALL }}>
            DEAL TRACKER
          </div>

          {/* Status Message */}
          {statusMessage && (
            <div style={{
              padding: SPACING.SMALL,
              marginBottom: SPACING.SMALL,
              borderRadius: '4px',
              fontSize: TYPOGRAPHY.TINY,
              backgroundColor: statusMessage.type === 'error' ? `${FINCEPT.RED}20` :
                             statusMessage.type === 'success' ? `${FINCEPT.GREEN}20` :
                             `${FINCEPT.CYAN}20`,
              color: statusMessage.type === 'error' ? FINCEPT.RED :
                     statusMessage.type === 'success' ? FINCEPT.GREEN :
                     FINCEPT.CYAN,
              display: 'flex',
              alignItems: 'center',
              gap: SPACING.SMALL,
            }}>
              <AlertCircle size={12} />
              {statusMessage.text}
            </div>
          )}

          {/* Search Input */}
          <div style={{ display: 'flex', gap: SPACING.SMALL, marginBottom: SPACING.SMALL }}>
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
                style={{
                  ...COMMON_STYLES.inputField,
                  paddingLeft: '28px',
                  fontSize: TYPOGRAPHY.SMALL,
                }}
              />
            </div>
            <button
              onClick={loadDeals}
              disabled={loading}
              title="Refresh deals"
              style={{
                padding: SPACING.SMALL,
                backgroundColor: FINCEPT.PANEL_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '4px',
                color: FINCEPT.GRAY,
                cursor: loading ? 'not-allowed' : 'pointer',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
              }}
            >
              <RefreshCw size={14} className={loading ? 'animate-spin' : ''} />
            </button>
          </div>

          {/* Scan SEC Filings */}
          <div style={{
            display: 'flex',
            gap: SPACING.SMALL,
            alignItems: 'center',
            marginBottom: SPACING.SMALL,
          }}>
            <select
              value={scanDays}
              onChange={(e) => setScanDays(Number(e.target.value))}
              style={{
                ...COMMON_STYLES.inputField,
                width: '80px',
                fontSize: TYPOGRAPHY.TINY,
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
                flex: 1,
                padding: SPACING.SMALL,
                backgroundColor: scanning ? FINCEPT.PANEL_BG : FINCEPT.ORANGE,
                border: 'none',
                borderRadius: '4px',
                color: scanning ? FINCEPT.GRAY : FINCEPT.DARK_BG,
                cursor: scanning ? 'not-allowed' : 'pointer',
                fontSize: TYPOGRAPHY.TINY,
                fontWeight: TYPOGRAPHY.BOLD,
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                gap: SPACING.TINY,
              }}
            >
              <Scan size={12} className={scanning ? 'animate-spin' : ''} />
              {scanning ? 'SCANNING...' : 'SCAN SEC FILINGS'}
            </button>
          </div>

          <div style={{
            fontSize: TYPOGRAPHY.TINY,
            color: FINCEPT.MUTED,
          }}>
            {filteredDeals.length} deal{filteredDeals.length !== 1 ? 's' : ''} tracked
          </div>
        </div>

        {/* Deal List */}
        <div style={{ flex: 1, overflow: 'auto' }}>
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
          ) : filteredDeals.length === 0 ? (
            <div style={{ ...COMMON_STYLES.emptyState, padding: SPACING.XLARGE }}>
              <Database size={32} style={{ marginBottom: SPACING.SMALL, opacity: 0.3 }} />
              <span style={{ fontSize: TYPOGRAPHY.SMALL, color: FINCEPT.GRAY }}>
                {searchQuery ? 'No matching deals' : 'No deals tracked yet'}
              </span>
              <div style={{
                fontSize: TYPOGRAPHY.TINY,
                color: FINCEPT.MUTED,
                marginTop: SPACING.SMALL,
                textAlign: 'center',
                maxWidth: '220px',
              }}>
                {searchQuery ? (
                  'Try a different search term'
                ) : (
                  <>
                    Click <strong style={{ color: FINCEPT.ORANGE }}>SCAN SEC FILINGS</strong> above to automatically discover M&A deals from SEC EDGAR
                  </>
                )}
              </div>
            </div>
          ) : (
            filteredDeals.map((deal) => {
              const isSelected = selectedDeal?.deal_id === deal.deal_id;
              return (
                <div
                  key={deal.deal_id}
                  onClick={() => setSelectedDeal(deal)}
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
                  <div style={{
                    fontSize: TYPOGRAPHY.SMALL,
                    fontWeight: TYPOGRAPHY.BOLD,
                    color: FINCEPT.WHITE,
                  }}>
                    {deal.target_name}
                  </div>
                  <div style={{
                    fontSize: TYPOGRAPHY.TINY,
                    color: FINCEPT.GRAY,
                    marginTop: SPACING.TINY,
                  }}>
                    {deal.acquirer_name}
                  </div>
                  <div style={{
                    display: 'flex',
                    justifyContent: 'space-between',
                    marginTop: SPACING.SMALL,
                    fontSize: TYPOGRAPHY.TINY,
                  }}>
                    <span style={{ color: FINCEPT.CYAN }}>
                      {formatCurrency(deal.deal_value)}
                    </span>
                    <span style={{
                      color: deal.premium_1day > 0 ? FINCEPT.GREEN : FINCEPT.RED,
                    }}>
                      {deal.premium_1day > 0 ? '+' : ''}
                      {deal.premium_1day.toFixed(1)}%
                    </span>
                  </div>
                  <div style={{
                    fontSize: TYPOGRAPHY.TINY,
                    color: FINCEPT.MUTED,
                    marginTop: SPACING.TINY,
                  }}>
                    {deal.announced_date}
                  </div>
                </div>
              );
            })
          )}
        </div>
      </div>

      {/* RIGHT - Deal Details */}
      <div style={{
        flex: 1,
        backgroundColor: FINCEPT.DARK_BG,
        display: 'flex',
        flexDirection: 'column',
        overflow: 'hidden',
      }}>
        {selectedDeal ? (
          <>
            {/* Deal Header */}
            <div style={{
              padding: SPACING.LARGE,
              backgroundColor: FINCEPT.HEADER_BG,
              borderBottom: `2px solid ${FINCEPT.ORANGE}`,
            }}>
              <div style={{
                display: 'flex',
                justifyContent: 'space-between',
                alignItems: 'flex-start',
              }}>
                <div>
                  <div style={{
                    fontSize: TYPOGRAPHY.HEADING,
                    fontWeight: TYPOGRAPHY.BOLD,
                    color: FINCEPT.WHITE,
                  }}>
                    {selectedDeal.target_name}
                  </div>
                  <div style={{
                    fontSize: TYPOGRAPHY.BODY,
                    color: FINCEPT.GRAY,
                    marginTop: SPACING.TINY,
                    display: 'flex',
                    alignItems: 'center',
                    gap: SPACING.SMALL,
                  }}>
                    <TrendingUp size={12} />
                    <span>Acquired by {selectedDeal.acquirer_name}</span>
                  </div>
                </div>
                <div style={{ textAlign: 'right' }}>
                  <div style={{
                    fontSize: TYPOGRAPHY.HEADING,
                    fontWeight: TYPOGRAPHY.BOLD,
                    color: FINCEPT.CYAN,
                  }}>
                    {formatCurrency(selectedDeal.deal_value)}
                  </div>
                  <div style={{
                    fontSize: TYPOGRAPHY.TINY,
                    color: FINCEPT.GRAY,
                    marginTop: SPACING.TINY,
                  }}>
                    DEAL VALUE
                  </div>
                </div>
              </div>

              {/* Status Badges */}
              <div style={{ marginTop: SPACING.DEFAULT }}>
                <span style={{
                  ...COMMON_STYLES.badgeInfo,
                  backgroundColor: selectedDeal.status === 'Completed' ? `${FINCEPT.GREEN}20` : `${FINCEPT.YELLOW}20`,
                  color: selectedDeal.status === 'Completed' ? FINCEPT.GREEN : FINCEPT.YELLOW,
                }}>
                  {selectedDeal.status.toUpperCase()}
                </span>
                <span style={{
                  ...COMMON_STYLES.badgeInfo,
                  marginLeft: SPACING.SMALL,
                }}>
                  {selectedDeal.industry.toUpperCase()}
                </span>
              </div>
            </div>

            {/* Deal Metrics */}
            <div style={{
              padding: SPACING.LARGE,
              overflow: 'auto',
            }}>
              <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.DEFAULT }}>
                KEY METRICS
              </div>

              <div style={{
                display: 'grid',
                gridTemplateColumns: '1fr 1fr',
                gap: SPACING.DEFAULT,
              }}>
                {/* Premium */}
                <div style={COMMON_STYLES.metricCard}>
                  <div style={COMMON_STYLES.dataLabel}>PREMIUM (1-DAY)</div>
                  <div style={{
                    fontSize: TYPOGRAPHY.HEADING,
                    fontWeight: TYPOGRAPHY.BOLD,
                    color: selectedDeal.premium_1day > 0 ? FINCEPT.GREEN : FINCEPT.RED,
                    marginTop: SPACING.TINY,
                  }}>
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
                    <div style={{
                      display: 'flex',
                      justifyContent: 'space-between',
                      marginTop: SPACING.TINY,
                    }}>
                      <span style={{ color: FINCEPT.GRAY }}>Stock:</span>
                      <span style={{ color: FINCEPT.WHITE }}>{selectedDeal.payment_stock_pct}%</span>
                    </div>
                  </div>
                </div>

                {/* EV/Revenue */}
                {selectedDeal.ev_revenue && (
                  <div style={COMMON_STYLES.metricCard}>
                    <div style={COMMON_STYLES.dataLabel}>EV / REVENUE</div>
                    <div style={{
                      fontSize: TYPOGRAPHY.HEADING,
                      fontWeight: TYPOGRAPHY.BOLD,
                      color: FINCEPT.CYAN,
                      marginTop: SPACING.TINY,
                    }}>
                      {selectedDeal.ev_revenue.toFixed(2)}x
                    </div>
                  </div>
                )}

                {/* EV/EBITDA */}
                {selectedDeal.ev_ebitda && (
                  <div style={COMMON_STYLES.metricCard}>
                    <div style={COMMON_STYLES.dataLabel}>EV / EBITDA</div>
                    <div style={{
                      fontSize: TYPOGRAPHY.HEADING,
                      fontWeight: TYPOGRAPHY.BOLD,
                      color: FINCEPT.CYAN,
                      marginTop: SPACING.TINY,
                    }}>
                      {selectedDeal.ev_ebitda.toFixed(2)}x
                    </div>
                  </div>
                )}

                {/* Announced Date */}
                <div style={COMMON_STYLES.metricCard}>
                  <div style={COMMON_STYLES.dataLabel}>ANNOUNCED</div>
                  <div style={{
                    fontSize: TYPOGRAPHY.SMALL,
                    color: FINCEPT.WHITE,
                    marginTop: SPACING.TINY,
                  }}>
                    {selectedDeal.announced_date}
                  </div>
                </div>

                {/* Synergies */}
                {selectedDeal.synergies && (
                  <div style={COMMON_STYLES.metricCard}>
                    <div style={COMMON_STYLES.dataLabel}>EST. SYNERGIES</div>
                    <div style={{
                      fontSize: TYPOGRAPHY.HEADING,
                      fontWeight: TYPOGRAPHY.BOLD,
                      color: FINCEPT.GREEN,
                      marginTop: SPACING.TINY,
                    }}>
                      {formatCurrency(selectedDeal.synergies)}
                    </div>
                  </div>
                )}
              </div>
            </div>
          </>
        ) : (
          <div style={COMMON_STYLES.emptyState}>
            <FileText size={48} style={{ opacity: 0.3, marginBottom: SPACING.DEFAULT }} />
            <div style={{ fontSize: TYPOGRAPHY.BODY, color: FINCEPT.GRAY }}>
              Select a deal to view details
            </div>
            <div style={{
              fontSize: TYPOGRAPHY.TINY,
              color: FINCEPT.MUTED,
              marginTop: SPACING.SMALL,
              maxWidth: '300px',
              textAlign: 'center',
            }}>
              Track M&A deals, save valuations, and manage your deal pipeline
            </div>
          </div>
        )}
      </div>
    </div>
  );
};
