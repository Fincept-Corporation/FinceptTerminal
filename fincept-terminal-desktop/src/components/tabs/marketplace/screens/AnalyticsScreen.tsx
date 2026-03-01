import React, { useState, useEffect, useRef } from 'react';
import { MarketplaceApiService } from '@/services/marketplace/marketplaceApi';
import { withTimeout } from '@/services/core/apiUtils';
import { FINCEPT, API_TIMEOUT_MS } from '../types';
import { BarChart3 } from 'lucide-react';

export const AnalyticsScreen: React.FC<{ session: any }> = ({ session }) => {
  const [analytics, setAnalytics] = useState<any>(null);
  const [isLoading, setIsLoading] = useState(true);
  const mountedRef = useRef(true);

  useEffect(() => {
    mountedRef.current = true;
    return () => { mountedRef.current = false; };
  }, []);

  useEffect(() => {
    const fetchAnalytics = async () => {
      if (!session?.api_key) return;
      setIsLoading(true);
      try {
        const response = await withTimeout(
          MarketplaceApiService.getDatasetAnalytics(session.api_key),
          API_TIMEOUT_MS,
          'Fetch analytics timeout'
        );
        if (!mountedRef.current) return;
        if (response.success && response.data) {
          setAnalytics(response.data);
        }
      } catch (err) {
        if (!mountedRef.current) return;
        console.error('Failed to fetch analytics:', err);
      } finally {
        if (mountedRef.current) setIsLoading(false);
      }
    };
    fetchAnalytics();
  }, [session?.api_key]);

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%', overflow: 'hidden' }}>
      <div style={{
        padding: '12px',
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <BarChart3 size={12} color={FINCEPT.ORANGE} />
          <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
            DATASET ANALYTICS
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
            Loading analytics...
          </div>
        ) : !analytics ? (
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
            <span>No analytics data available</span>
          </div>
        ) : (
          <div style={{ maxWidth: '1200px', margin: '0 auto' }}>
            {/* Summary Stats */}
            <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '12px', marginBottom: '24px' }}>
              <div style={{
                backgroundColor: FINCEPT.PANEL_BG,
                border: `2px solid ${FINCEPT.BLUE}`,
                borderRadius: '2px',
                padding: '16px',
                textAlign: 'center'
              }}>
                <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '6px', letterSpacing: '0.5px' }}>
                  TOTAL DATASETS
                </div>
                <div style={{ fontSize: '24px', fontWeight: 700, color: FINCEPT.BLUE }}>
                  {analytics.total_datasets || 0}
                </div>
              </div>
              <div style={{
                backgroundColor: FINCEPT.PANEL_BG,
                border: `2px solid ${FINCEPT.GREEN}`,
                borderRadius: '2px',
                padding: '16px',
                textAlign: 'center'
              }}>
                <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '6px', letterSpacing: '0.5px' }}>
                  TOTAL DOWNLOADS
                </div>
                <div style={{ fontSize: '24px', fontWeight: 700, color: FINCEPT.GREEN }}>
                  {analytics.total_downloads || 0}
                </div>
              </div>
              <div style={{
                backgroundColor: FINCEPT.PANEL_BG,
                border: `2px solid ${FINCEPT.ORANGE}`,
                borderRadius: '2px',
                padding: '16px',
                textAlign: 'center'
              }}>
                <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '6px', letterSpacing: '0.5px' }}>
                  TOTAL REVENUE
                </div>
                <div style={{ fontSize: '24px', fontWeight: 700, color: FINCEPT.ORANGE }}>
                  {analytics.total_revenue || 0}
                </div>
              </div>
            </div>

            {/* Dataset Performance */}
            {analytics.datasets && analytics.datasets.length > 0 && (
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
                  DATASET PERFORMANCE
                </div>
                {analytics.datasets.map((dataset: any) => (
                  <div key={dataset.id} style={{
                    borderBottom: `1px solid ${FINCEPT.BORDER}`,
                    padding: '12px 0'
                  }}>
                    <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                      <div style={{ fontSize: '10px', color: FINCEPT.WHITE, flex: 1 }}>
                        {dataset.title}
                      </div>
                      <div style={{ display: 'flex', gap: '24px' }}>
                        <div style={{ textAlign: 'center' }}>
                          <div style={{ fontSize: '12px', fontWeight: 700, color: FINCEPT.CYAN }}>
                            {dataset.downloads}
                          </div>
                          <div style={{ fontSize: '8px', color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
                            DOWNLOADS
                          </div>
                        </div>
                        <div style={{ textAlign: 'center' }}>
                          <div style={{ fontSize: '12px', fontWeight: 700, color: FINCEPT.GREEN }}>
                            {dataset.revenue}
                          </div>
                          <div style={{ fontSize: '8px', color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
                            REVENUE
                          </div>
                        </div>
                        <div style={{ textAlign: 'center' }}>
                          <div style={{ fontSize: '12px', fontWeight: 700, color: FINCEPT.YELLOW }}>
                            ★ {dataset.rating.toFixed(1)}
                          </div>
                          <div style={{ fontSize: '8px', color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
                            RATING
                          </div>
                        </div>
                      </div>
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
