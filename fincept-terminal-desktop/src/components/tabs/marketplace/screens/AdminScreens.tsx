import React, { useState, useEffect, useCallback, useRef } from 'react';
import { Dataset } from '@/services/marketplace/marketplaceApi';
import { MarketplaceApiService } from '@/services/marketplace/marketplaceApi';
import { withTimeout } from '@/services/core/apiUtils';
import { sanitizeInput } from '@/services/core/validators';
import { showConfirm, showWarning, showError, showSuccess } from '@/utils/notifications';
import { FINCEPT, FONT_FAMILY, API_TIMEOUT_MS } from '../types';
import { StatCard } from '../SharedComponents';
import { BarChart3, Shield, Clock, TrendingUp, Check, X } from 'lucide-react';

// Admin Stats Screen
export const AdminStatsScreen: React.FC<{ session: any }> = ({ session }) => {
  const [stats, setStats] = useState<any>(null);
  const [isLoading, setIsLoading] = useState(true);
  const mountedRef = useRef(true);

  useEffect(() => {
    mountedRef.current = true;
    return () => { mountedRef.current = false; };
  }, []);

  useEffect(() => {
    const fetchStats = async () => {
      if (!session?.api_key) return;
      setIsLoading(true);
      try {
        const response = await withTimeout(
          MarketplaceApiService.getMarketplaceStats(session.api_key),
          API_TIMEOUT_MS,
          'Fetch stats timeout'
        );
        if (!mountedRef.current) return;
        if (response.success && response.data) {
          setStats(response.data);
        }
      } catch (err) {
        if (!mountedRef.current) return;
        console.error('Failed to fetch stats:', err);
      } finally {
        if (mountedRef.current) setIsLoading(false);
      }
    };
    fetchStats();
  }, [session?.api_key]);

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%', overflow: 'hidden' }}>
      <div style={{
        padding: '12px',
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <Shield size={12} color={FINCEPT.ORANGE} />
          <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
            MARKETPLACE STATISTICS
          </span>
        </div>
      </div>

      <div className="marketplace-scroll" style={{ flex: 1, overflowY: 'auto', padding: '16px' }}>
        {isLoading ? (
          <div style={{
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            height: '200px',
            color: FINCEPT.MUTED,
            fontSize: '10px'
          }}>
            Loading statistics...
          </div>
        ) : !stats ? (
          <div style={{
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            justifyContent: 'center',
            height: '200px',
            color: FINCEPT.MUTED,
            fontSize: '10px',
            textAlign: 'center'
          }}>
            <BarChart3 size={24} style={{ marginBottom: '8px', opacity: 0.5 }} />
            <span>No statistics available</span>
          </div>
        ) : (
          <div style={{ maxWidth: '1200px', margin: '0 auto' }}>
            <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '12px', marginBottom: '16px' }}>
              <StatCard label="TOTAL DATASETS" value={stats.total_datasets || 0} color={FINCEPT.BLUE} />
              <StatCard label="TOTAL PURCHASES" value={stats.total_purchases || 0} color={FINCEPT.GREEN} />
              <StatCard label="TOTAL REVENUE" value={stats.total_revenue || 0} color={FINCEPT.ORANGE} />
            </div>
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px', marginBottom: '24px' }}>
              <StatCard label="TOTAL UPLOADS" value={stats.total_uploads || 0} color={FINCEPT.CYAN} />
              <StatCard label="ACTIVE USERS" value={stats.active_users || 0} color={FINCEPT.PURPLE} />
            </div>

            {stats.popular_categories && Object.keys(stats.popular_categories).length > 0 && (
              <div style={{
                backgroundColor: FINCEPT.PANEL_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '2px',
                padding: '16px'
              }}>
                <div style={{
                  fontSize: '10px',
                  fontWeight: 700,
                  color: FINCEPT.ORANGE,
                  marginBottom: '16px',
                  letterSpacing: '0.5px'
                }}>
                  POPULAR CATEGORIES
                </div>
                {Object.entries(stats.popular_categories).map(([category, count]: [string, any]) => (
                  <div key={category} style={{
                    borderBottom: `1px solid ${FINCEPT.BORDER}`,
                    padding: '12px 0',
                    display: 'flex',
                    justifyContent: 'space-between'
                  }}>
                    <div style={{ fontSize: '10px', color: FINCEPT.WHITE }}>
                      {category.toUpperCase()}
                    </div>
                    <div style={{ fontSize: '10px', fontWeight: 700, color: FINCEPT.CYAN }}>
                      {count} datasets
                    </div>
                  </div>
                ))}
              </div>
            )}
          </div>
        )}
      </div>
    </div>
  );
};

// Admin Pending Screen
export const AdminPendingScreen: React.FC<{ session: any; onRefresh: () => void }> = ({ session, onRefresh }) => {
  const [pendingDatasets, setPendingDatasets] = useState<Dataset[]>([]);
  const [isLoading, setIsLoading] = useState(true);
  const [rejectingId, setRejectingId] = useState<number | null>(null);
  const [rejectionReason, setRejectionReason] = useState('');
  const mountedRef = useRef(true);

  useEffect(() => {
    mountedRef.current = true;
    return () => { mountedRef.current = false; };
  }, []);

  const loadPending = useCallback(async () => {
    if (!session?.api_key) return;
    setIsLoading(true);
    try {
      const response = await withTimeout(
        MarketplaceApiService.getPendingDatasets(session.api_key),
        API_TIMEOUT_MS,
        'Fetch pending datasets timeout'
      );
      if (!mountedRef.current) return;
      if (response.success && response.data) {
        setPendingDatasets(response.data.datasets || []);
      }
    } catch (err) {
      if (!mountedRef.current) return;
      console.error('Failed to fetch pending datasets:', err);
    } finally {
      if (mountedRef.current) setIsLoading(false);
    }
  }, [session?.api_key]);

  useEffect(() => {
    loadPending();
  }, [loadPending]);

  const handleApprove = useCallback(async (datasetId: number, title: string) => {
    const confirmed = await showConfirm('', {
      title: `Approve dataset "${title}"?`,
      type: 'info'
    });
    if (!confirmed) return;
    setIsLoading(true);
    try {
      const response = await withTimeout(
        MarketplaceApiService.approveDataset(session.api_key, datasetId),
        API_TIMEOUT_MS,
        'Approve dataset timeout'
      );
      if (!mountedRef.current) return;
      if (response.success) {
        showSuccess('Dataset approved');
        loadPending();
        onRefresh();
      } else {
        showError('Approval failed');
      }
    } catch (err) {
      if (mountedRef.current) {
        showError('Approval failed', [
          { label: 'ERROR', value: err instanceof Error ? err.message : 'Unknown error' }
        ]);
      }
    } finally {
      if (mountedRef.current) setIsLoading(false);
    }
  }, [session?.api_key, loadPending, onRefresh]);

  const handleReject = useCallback(async (datasetId: number) => {
    const sanitizedReason = sanitizeInput(rejectionReason);
    if (!sanitizedReason.trim()) {
      showWarning('Please provide a rejection reason');
      return;
    }
    setIsLoading(true);
    try {
      const response = await withTimeout(
        MarketplaceApiService.rejectDataset(session.api_key, datasetId, sanitizedReason),
        API_TIMEOUT_MS,
        'Reject dataset timeout'
      );
      if (!mountedRef.current) return;
      if (response.success) {
        showSuccess('Dataset rejected');
        setRejectingId(null);
        setRejectionReason('');
        loadPending();
        onRefresh();
      } else {
        showError('Rejection failed');
      }
    } catch (err) {
      if (mountedRef.current) {
        showError('Rejection failed', [
          { label: 'ERROR', value: err instanceof Error ? err.message : 'Unknown error' }
        ]);
      }
    } finally {
      if (mountedRef.current) setIsLoading(false);
    }
  }, [session?.api_key, rejectionReason, loadPending, onRefresh]);

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%', overflow: 'hidden' }}>
      <div style={{
        padding: '12px',
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <Clock size={12} color={FINCEPT.ORANGE} />
          <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
            PENDING APPROVALS ({pendingDatasets.length})
          </span>
        </div>
      </div>

      <div className="marketplace-scroll" style={{ flex: 1, overflowY: 'auto', padding: '16px' }}>
        {isLoading ? (
          <div style={{
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            height: '200px',
            color: FINCEPT.MUTED,
            fontSize: '10px'
          }}>
            Loading pending datasets...
          </div>
        ) : pendingDatasets.length === 0 ? (
          <div style={{
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            justifyContent: 'center',
            height: '200px',
            color: FINCEPT.MUTED,
            fontSize: '10px',
            textAlign: 'center'
          }}>
            <Clock size={24} style={{ marginBottom: '8px', opacity: 0.5 }} />
            <span>No pending datasets</span>
          </div>
        ) : (
          <div style={{ maxWidth: '1000px', margin: '0 auto', display: 'flex', flexDirection: 'column', gap: '12px' }}>
            {pendingDatasets.map((dataset) => (
              <div key={dataset.id} style={{
                backgroundColor: FINCEPT.PANEL_BG,
                border: `2px solid ${FINCEPT.YELLOW}`,
                borderRadius: '2px',
                padding: '16px'
              }}>
                <div style={{ marginBottom: '12px' }}>
                  <div style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.WHITE, marginBottom: '6px' }}>
                    {dataset.title}
                  </div>
                  <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '8px' }}>
                    by {dataset.uploader_username} • {dataset.category} • {dataset.price_tier} • {(dataset.file_size / 1024 / 1024).toFixed(2)} MB
                  </div>
                  <div style={{ fontSize: '10px', color: FINCEPT.WHITE, lineHeight: '1.5', marginBottom: '12px' }}>
                    {dataset.description}
                  </div>
                  {dataset.tags && dataset.tags.length > 0 && (
                    <div style={{ display: 'flex', gap: '6px', flexWrap: 'wrap' }}>
                      {dataset.tags.map((tag, idx) => (
                        <span key={idx} style={{
                          padding: '2px 6px',
                          backgroundColor: `${FINCEPT.CYAN}20`,
                          color: FINCEPT.CYAN,
                          fontSize: '8px',
                          fontWeight: 700,
                          borderRadius: '2px'
                        }}>
                          #{tag}
                        </span>
                      ))}
                    </div>
                  )}
                </div>

                {rejectingId === dataset.id ? (
                  <div style={{
                    backgroundColor: `${FINCEPT.RED}10`,
                    border: `1px solid ${FINCEPT.RED}`,
                    borderRadius: '2px',
                    padding: '12px',
                    marginTop: '12px'
                  }}>
                    <div style={{
                      fontSize: '9px',
                      fontWeight: 700,
                      color: FINCEPT.RED,
                      marginBottom: '8px',
                      letterSpacing: '0.5px'
                    }}>
                      REJECTION REASON
                    </div>
                    <textarea
                      value={rejectionReason}
                      onChange={(e) => setRejectionReason(e.target.value)}
                      placeholder="Enter reason for rejection..."
                      style={{
                        width: '100%',
                        padding: '8px',
                        backgroundColor: FINCEPT.DARK_BG,
                        color: FINCEPT.WHITE,
                        border: `1px solid ${FINCEPT.BORDER}`,
                        borderRadius: '2px',
                        fontSize: '10px',
                        fontFamily: FONT_FAMILY,
                        minHeight: '60px',
                        marginBottom: '8px',
                        outline: 'none'
                      }}
                    />
                    <div style={{ display: 'flex', gap: '8px' }}>
                      <button
                        onClick={() => handleReject(dataset.id)}
                        disabled={!rejectionReason.trim()}
                        style={{
                          padding: '6px 12px',
                          backgroundColor: FINCEPT.RED,
                          color: FINCEPT.WHITE,
                          border: 'none',
                          borderRadius: '2px',
                          fontSize: '9px',
                          fontWeight: 700,
                          cursor: rejectionReason.trim() ? 'pointer' : 'not-allowed',
                          fontFamily: FONT_FAMILY,
                          letterSpacing: '0.5px',
                          opacity: rejectionReason.trim() ? 1 : 0.5
                        }}
                      >
                        CONFIRM REJECT
                      </button>
                      <button
                        onClick={() => { setRejectingId(null); setRejectionReason(''); }}
                        style={{
                          padding: '6px 12px',
                          backgroundColor: 'transparent',
                          color: FINCEPT.GRAY,
                          border: `1px solid ${FINCEPT.BORDER}`,
                          borderRadius: '2px',
                          fontSize: '9px',
                          fontWeight: 700,
                          cursor: 'pointer',
                          fontFamily: FONT_FAMILY,
                          letterSpacing: '0.5px'
                        }}
                      >
                        CANCEL
                      </button>
                    </div>
                  </div>
                ) : (
                  <div style={{ display: 'flex', gap: '8px' }}>
                    <button
                      onClick={() => handleApprove(dataset.id, dataset.title)}
                      style={{
                        padding: '8px 16px',
                        backgroundColor: 'transparent',
                        border: `1px solid ${FINCEPT.GREEN}`,
                        color: FINCEPT.GREEN,
                        borderRadius: '2px',
                        fontSize: '9px',
                        fontWeight: 700,
                        cursor: 'pointer',
                        fontFamily: FONT_FAMILY,
                        letterSpacing: '0.5px',
                        display: 'flex',
                        alignItems: 'center',
                        gap: '4px'
                      }}
                    >
                      <Check size={10} />
                      APPROVE
                    </button>
                    <button
                      onClick={() => setRejectingId(dataset.id)}
                      style={{
                        padding: '8px 16px',
                        backgroundColor: 'transparent',
                        border: `1px solid ${FINCEPT.RED}`,
                        color: FINCEPT.RED,
                        borderRadius: '2px',
                        fontSize: '9px',
                        fontWeight: 700,
                        cursor: 'pointer',
                        fontFamily: FONT_FAMILY,
                        letterSpacing: '0.5px',
                        display: 'flex',
                        alignItems: 'center',
                        gap: '4px'
                      }}
                    >
                      <X size={10} />
                      REJECT
                    </button>
                  </div>
                )}
              </div>
            ))}
          </div>
        )}
      </div>
    </div>
  );
};

// Admin Revenue Screen
export const AdminRevenueScreen: React.FC<{ session: any }> = ({ session }) => {
  const [revenue, setRevenue] = useState<any>(null);
  const [isLoading, setIsLoading] = useState(true);
  const mountedRef = useRef(true);

  useEffect(() => {
    mountedRef.current = true;
    return () => { mountedRef.current = false; };
  }, []);

  useEffect(() => {
    const fetchRevenue = async () => {
      if (!session?.api_key) return;
      setIsLoading(true);
      try {
        const response = await withTimeout(
          MarketplaceApiService.getAdminRevenueAnalytics(session.api_key),
          API_TIMEOUT_MS,
          'Fetch revenue timeout'
        );
        if (!mountedRef.current) return;
        if (response.success && response.data) {
          setRevenue(response.data);
        }
      } catch (err) {
        if (!mountedRef.current) return;
        console.error('Failed to fetch revenue:', err);
      } finally {
        if (mountedRef.current) setIsLoading(false);
      }
    };
    fetchRevenue();
  }, [session?.api_key]);

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%', overflow: 'hidden' }}>
      <div style={{
        padding: '12px',
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <TrendingUp size={12} color={FINCEPT.ORANGE} />
          <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
            REVENUE ANALYTICS
          </span>
        </div>
      </div>

      <div className="marketplace-scroll" style={{ flex: 1, overflowY: 'auto', padding: '16px' }}>
        {isLoading ? (
          <div style={{
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            height: '200px',
            color: FINCEPT.MUTED,
            fontSize: '10px'
          }}>
            Loading revenue data...
          </div>
        ) : !revenue ? (
          <div style={{
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            justifyContent: 'center',
            height: '200px',
            color: FINCEPT.MUTED,
            fontSize: '10px',
            textAlign: 'center'
          }}>
            <TrendingUp size={24} style={{ marginBottom: '8px', opacity: 0.5 }} />
            <span>No revenue data available</span>
          </div>
        ) : (
          <div style={{ maxWidth: '1200px', margin: '0 auto' }}>
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px', marginBottom: '24px' }}>
              <div style={{
                backgroundColor: FINCEPT.PANEL_BG,
                border: `2px solid ${FINCEPT.GREEN}`,
                borderRadius: '2px',
                padding: '24px',
                textAlign: 'center'
              }}>
                <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '8px', letterSpacing: '0.5px' }}>
                  TOTAL REVENUE
                </div>
                <div style={{ fontSize: '36px', fontWeight: 700, color: FINCEPT.GREEN }}>
                  {revenue.total_revenue || 0}
                </div>
                <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginTop: '4px', letterSpacing: '0.5px' }}>
                  CREDITS
                </div>
              </div>
              <div style={{
                backgroundColor: FINCEPT.PANEL_BG,
                border: `2px solid ${FINCEPT.BLUE}`,
                borderRadius: '2px',
                padding: '24px',
                textAlign: 'center'
              }}>
                <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '8px', letterSpacing: '0.5px' }}>
                  TOTAL TRANSACTIONS
                </div>
                <div style={{ fontSize: '36px', fontWeight: 700, color: FINCEPT.BLUE }}>
                  {revenue.total_transactions || 0}
                </div>
              </div>
            </div>

            {revenue.revenue_by_tier && Object.keys(revenue.revenue_by_tier).length > 0 && (
              <div style={{
                backgroundColor: FINCEPT.PANEL_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '2px',
                padding: '16px',
                marginBottom: '24px'
              }}>
                <div style={{
                  fontSize: '10px',
                  fontWeight: 700,
                  color: FINCEPT.ORANGE,
                  marginBottom: '16px',
                  letterSpacing: '0.5px'
                }}>
                  REVENUE BY TIER
                </div>
                {Object.entries(revenue.revenue_by_tier).map(([tier, amount]: [string, any]) => (
                  <div key={tier} style={{
                    borderBottom: `1px solid ${FINCEPT.BORDER}`,
                    padding: '12px 0',
                    display: 'flex',
                    justifyContent: 'space-between'
                  }}>
                    <div style={{ fontSize: '10px', color: FINCEPT.WHITE }}>
                      {tier.toUpperCase()}
                    </div>
                    <div style={{ fontSize: '10px', fontWeight: 700, color: FINCEPT.GREEN }}>
                      {amount} credits
                    </div>
                  </div>
                ))}
              </div>
            )}

            {revenue.top_datasets && revenue.top_datasets.length > 0 && (
              <div style={{
                backgroundColor: FINCEPT.PANEL_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '2px',
                padding: '16px'
              }}>
                <div style={{
                  fontSize: '10px',
                  fontWeight: 700,
                  color: FINCEPT.ORANGE,
                  marginBottom: '16px',
                  letterSpacing: '0.5px'
                }}>
                  TOP PERFORMING DATASETS
                </div>
                {revenue.top_datasets.map((dataset: any, idx: number) => (
                  <div key={dataset.id} style={{
                    borderBottom: `1px solid ${FINCEPT.BORDER}`,
                    padding: '12px 0',
                    display: 'flex',
                    justifyContent: 'space-between',
                    alignItems: 'center'
                  }}>
                    <div style={{ flex: 1 }}>
                      <div style={{ fontSize: '10px', fontWeight: 700, color: FINCEPT.WHITE, marginBottom: '4px' }}>
                        #{idx + 1} {dataset.title}
                      </div>
                      <div style={{ fontSize: '9px', color: FINCEPT.GRAY }}>
                        {dataset.downloads} downloads
                      </div>
                    </div>
                    <div style={{ fontSize: '14px', fontWeight: 700, color: FINCEPT.GREEN, marginLeft: '16px' }}>
                      {dataset.revenue} credits
                    </div>
                  </div>
                ))}
              </div>
            )}
          </div>
        )}
      </div>
    </div>
  );
};
