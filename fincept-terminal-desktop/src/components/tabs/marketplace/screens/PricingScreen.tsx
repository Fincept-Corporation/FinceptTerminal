import React, { useState, useEffect, useRef } from 'react';
import { MarketplaceApiService } from '@/services/marketplace/marketplaceApi';
import { withTimeout } from '@/services/core/apiUtils';
import { FINCEPT, API_TIMEOUT_MS } from '../types';
import { DollarSign, Check } from 'lucide-react';

export const PricingScreen: React.FC<{ session: any }> = ({ session }) => {
  const [pricingTiers, setPricingTiers] = useState<any[]>([]);
  const [isLoading, setIsLoading] = useState(true);
  const mountedRef = useRef(true);

  useEffect(() => {
    mountedRef.current = true;
    return () => { mountedRef.current = false; };
  }, []);

  useEffect(() => {
    const fetchPricing = async () => {
      if (!session?.api_key) return;
      setIsLoading(true);
      try {
        const response = await withTimeout(
          MarketplaceApiService.getPricingTiers(session.api_key),
          API_TIMEOUT_MS,
          'Fetch pricing timeout'
        );
        if (!mountedRef.current) return;
        if (response.success && response.data) {
          setPricingTiers(response.data.tiers || []);
        }
      } catch (err) {
        if (!mountedRef.current) return;
        console.error('Failed to fetch pricing tiers:', err);
      } finally {
        if (mountedRef.current) setIsLoading(false);
      }
    };
    fetchPricing();
  }, [session?.api_key]);

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%', overflow: 'hidden' }}>
      <div style={{
        padding: '12px',
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <DollarSign size={12} color={FINCEPT.ORANGE} />
          <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
            PRICING TIERS
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
            Loading pricing...
          </div>
        ) : pricingTiers.length === 0 ? (
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
            <DollarSign size={24} style={{ marginBottom: '8px', opacity: 0.5 }} />
            <span>No pricing information available</span>
          </div>
        ) : (
          <div style={{
            maxWidth: '1200px',
            margin: '0 auto',
            display: 'grid',
            gridTemplateColumns: 'repeat(auto-fit, minmax(280px, 1fr))',
            gap: '16px'
          }}>
            {pricingTiers.map((tier, idx) => (
              <div key={idx} style={{
                backgroundColor: FINCEPT.PANEL_BG,
                border: `2px solid ${FINCEPT.ORANGE}`,
                borderRadius: '2px',
                padding: '20px',
                textAlign: 'center'
              }}>
                <div style={{
                  fontSize: '12px',
                  fontWeight: 700,
                  color: FINCEPT.ORANGE,
                  marginBottom: '12px',
                  letterSpacing: '0.5px'
                }}>
                  {tier.tier.toUpperCase()}
                </div>
                <div style={{
                  fontSize: '32px',
                  fontWeight: 700,
                  color: FINCEPT.GREEN,
                  marginBottom: '4px'
                }}>
                  {tier.price_credits}
                </div>
                <div style={{
                  fontSize: '9px',
                  color: FINCEPT.GRAY,
                  marginBottom: '16px',
                  letterSpacing: '0.5px'
                }}>
                  CREDITS
                </div>
                <div style={{
                  fontSize: '10px',
                  color: FINCEPT.WHITE,
                  lineHeight: '1.6',
                  marginBottom: '20px'
                }}>
                  {tier.description}
                </div>
                {tier.features && tier.features.length > 0 && (
                  <div style={{
                    textAlign: 'left',
                    borderTop: `1px solid ${FINCEPT.BORDER}`,
                    paddingTop: '16px'
                  }}>
                    <div style={{
                      fontSize: '9px',
                      fontWeight: 700,
                      color: FINCEPT.CYAN,
                      marginBottom: '12px',
                      letterSpacing: '0.5px'
                    }}>
                      FEATURES
                    </div>
                    {tier.features.map((feature: string, fidx: number) => (
                      <div key={fidx} style={{
                        fontSize: '9px',
                        color: FINCEPT.WHITE,
                        marginBottom: '6px',
                        display: 'flex',
                        alignItems: 'center',
                        gap: '8px'
                      }}>
                        <Check size={10} color={FINCEPT.GREEN} />
                        <span>{feature}</span>
                      </div>
                    ))}
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
