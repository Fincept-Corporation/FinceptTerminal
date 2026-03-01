import React, { useState, useEffect, useCallback, useRef } from 'react';
import { MarketplaceApiService } from '@/services/marketplace/marketplaceApi';
import { withTimeout } from '@/services/core/apiUtils';
import { showError } from '@/utils/notifications';
import { FINCEPT, FONT_FAMILY, API_TIMEOUT_MS } from '../types';
import { ShoppingCart, Download } from 'lucide-react';

export const MyPurchasesScreen: React.FC<{ session: any }> = ({ session }) => {
  const [purchases, setPurchases] = useState<any[]>([]);
  const [isLoading, setIsLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const mountedRef = useRef(true);

  useEffect(() => {
    mountedRef.current = true;
    return () => { mountedRef.current = false; };
  }, []);

  useEffect(() => {
    const fetchPurchases = async () => {
      if (!session?.api_key) return;
      setIsLoading(true);
      setError(null);
      try {
        const response = await withTimeout(
          MarketplaceApiService.getUserPurchases(session.api_key, 1, 50),
          API_TIMEOUT_MS,
          'Fetch purchases timeout'
        );
        if (!mountedRef.current) return;
        if (response.success && response.data) {
          setPurchases(response.data.purchases || []);
        }
      } catch (err) {
        if (!mountedRef.current) return;
        console.error('Failed to fetch purchases:', err);
        setError(err instanceof Error ? err.message : 'Failed to fetch purchases');
      } finally {
        if (mountedRef.current) setIsLoading(false);
      }
    };
    fetchPurchases();
  }, [session?.api_key]);

  const handleDownload = useCallback(async (datasetId: number) => {
    if (!session?.api_key) return;
    try {
      const response = await withTimeout(
        MarketplaceApiService.downloadDataset(session.api_key, datasetId),
        API_TIMEOUT_MS,
        'Download timeout'
      );
      if (response.success && response.data?.download_url) {
        window.open(response.data.download_url, '_blank');
      } else {
        showError('Download failed');
      }
    } catch (err) {
      showError('Download failed', [
        { label: 'ERROR', value: err instanceof Error ? err.message : 'Unknown error' }
      ]);
    }
  }, [session?.api_key]);

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%', overflow: 'hidden' }}>
      <div style={{
        padding: '12px',
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`
      }}>
        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <ShoppingCart size={12} color={FINCEPT.ORANGE} />
            <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
              MY PURCHASES ({purchases.length})
            </span>
          </div>
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
            Loading purchases...
          </div>
        ) : purchases.length === 0 ? (
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
            <ShoppingCart size={24} style={{ marginBottom: '8px', opacity: 0.5 }} />
            <span>No purchases yet</span>
          </div>
        ) : (
          <div style={{ maxWidth: '1000px', margin: '0 auto', display: 'flex', flexDirection: 'column', gap: '12px' }}>
            {purchases.map((purchase) => (
              <div key={purchase.id} style={{
                backgroundColor: FINCEPT.PANEL_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderLeft: `4px solid ${FINCEPT.GREEN}`,
                borderRadius: '2px',
                padding: '16px',
                display: 'flex',
                justifyContent: 'space-between',
                alignItems: 'center'
              }}>
                <div style={{ flex: 1 }}>
                  <div style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.WHITE, marginBottom: '6px' }}>
                    {purchase.dataset_title}
                  </div>
                  <div style={{ fontSize: '9px', color: FINCEPT.GRAY, display: 'flex', gap: '16px' }}>
                    <span>PURCHASED: {new Date(purchase.purchased_at).toLocaleDateString()}</span>
                    <span>PAID: <span style={{ color: FINCEPT.GREEN }}>{purchase.price_paid} CREDITS</span></span>
                  </div>
                </div>
                <button
                  onClick={() => handleDownload(purchase.dataset_id)}
                  style={{
                    padding: '8px 16px',
                    backgroundColor: FINCEPT.ORANGE,
                    color: FINCEPT.DARK_BG,
                    border: 'none',
                    borderRadius: '2px',
                    fontSize: '9px',
                    fontWeight: 700,
                    cursor: 'pointer',
                    fontFamily: FONT_FAMILY,
                    letterSpacing: '0.5px',
                    display: 'flex',
                    alignItems: 'center',
                    gap: '6px'
                  }}
                >
                  <Download size={10} />
                  DOWNLOAD
                </button>
              </div>
            ))}
          </div>
        )}
      </div>
    </div>
  );
};
