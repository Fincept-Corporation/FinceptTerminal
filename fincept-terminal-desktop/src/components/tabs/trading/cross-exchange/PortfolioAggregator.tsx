/**
 * PortfolioAggregator - Unified view of balances across all exchanges
 * Fincept Terminal Style
 */

import React from 'react';
import { Wallet, TrendingUp, RefreshCw, Loader, PieChart } from 'lucide-react';
import { useCrossExchangePortfolio } from '../hooks/useCrossExchange';

// Fincept Professional Color Palette
const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  BLUE: '#0088FF',
  PURPLE: '#9D4EDD',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A'
};

export function PortfolioAggregator() {
  const { aggregatedBalances, totalPortfolioValue, isLoading, error, refresh } =
    useCrossExchangePortfolio();

  const formatCurrency = (value: number): string => {
    return value.toLocaleString('en-US', {
      minimumFractionDigits: 2,
      maximumFractionDigits: 2,
    });
  };

  const formatCrypto = (value: number, decimals: number = 4): string => {
    return value.toLocaleString('en-US', {
      minimumFractionDigits: decimals,
      maximumFractionDigits: decimals,
    });
  };

  if (isLoading) {
    return (
      <div style={{
        backgroundColor: FINCEPT.PANEL_BG,
        border: `1px solid ${FINCEPT.BORDER}`,
        borderRadius: '4px',
        padding: '24px'
      }}>
        <div style={{
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          padding: '32px 0'
        }}>
          <Loader style={{ width: 24, height: 24, color: FINCEPT.BLUE, animation: 'spin 1s linear infinite' }} />
        </div>
      </div>
    );
  }

  if (error) {
    return (
      <div style={{
        backgroundColor: FINCEPT.PANEL_BG,
        border: `1px solid ${FINCEPT.BORDER}`,
        borderRadius: '4px',
        padding: '24px'
      }}>
        <div style={{
          textAlign: 'center',
          padding: '32px 0',
          color: FINCEPT.RED,
          fontSize: '12px'
        }}>
          {error}
        </div>
      </div>
    );
  }

  return (
    <div style={{
      backgroundColor: FINCEPT.PANEL_BG,
      border: `1px solid ${FINCEPT.BORDER}`,
      borderRadius: '4px',
      padding: '16px',
      height: '100%',
      overflow: 'auto'
    }}>
      {/* Header */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        marginBottom: '16px',
        paddingBottom: '12px',
        borderBottom: `1px solid ${FINCEPT.BORDER}`
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <PieChart style={{ width: 18, height: 18, color: FINCEPT.CYAN }} />
          <span style={{ fontSize: '13px', fontWeight: 600, color: FINCEPT.WHITE, letterSpacing: '0.5px' }}>
            CROSS-EXCHANGE PORTFOLIO
          </span>
        </div>
        <button
          onClick={refresh}
          style={{
            padding: '6px 12px',
            fontSize: '10px',
            color: FINCEPT.CYAN,
            backgroundColor: 'transparent',
            border: `1px solid ${FINCEPT.BORDER}`,
            borderRadius: '2px',
            cursor: 'pointer',
            display: 'flex',
            alignItems: 'center',
            gap: '6px',
            transition: 'all 0.2s'
          }}
          onMouseEnter={(e) => {
            e.currentTarget.style.borderColor = FINCEPT.CYAN;
            e.currentTarget.style.backgroundColor = `${FINCEPT.CYAN}15`;
          }}
          onMouseLeave={(e) => {
            e.currentTarget.style.borderColor = FINCEPT.BORDER;
            e.currentTarget.style.backgroundColor = 'transparent';
          }}
        >
          <RefreshCw style={{ width: 12, height: 12 }} />
          REFRESH
        </button>
      </div>

      {/* Total Portfolio Value */}
      <div style={{
        marginBottom: '20px',
        padding: '16px',
        backgroundColor: `${FINCEPT.CYAN}08`,
        border: `1px solid ${FINCEPT.CYAN}25`,
        borderRadius: '2px'
      }}>
        <div style={{ fontSize: '10px', color: FINCEPT.GRAY, marginBottom: '6px', letterSpacing: '0.5px' }}>
          TOTAL PORTFOLIO VALUE
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          <div style={{ fontSize: '24px', fontWeight: 700, color: FINCEPT.WHITE, fontFamily: 'monospace' }}>
            ${formatCurrency(totalPortfolioValue)}
          </div>
          <div style={{ display: 'flex', alignItems: 'center', gap: '4px', color: FINCEPT.GREEN, fontSize: '11px' }}>
            <TrendingUp style={{ width: 14, height: 14 }} />
            <span>+2.45%</span>
          </div>
        </div>
        <div style={{ fontSize: '10px', color: FINCEPT.MUTED, marginTop: '6px' }}>
          ACROSS {aggregatedBalances.length} ASSETS
        </div>
      </div>

      {/* Aggregated Balances */}
      <div>
        <div style={{
          fontSize: '11px',
          fontWeight: 600,
          color: FINCEPT.GRAY,
          marginBottom: '12px',
          letterSpacing: '0.5px'
        }}>
          ASSET BREAKDOWN
        </div>

        {aggregatedBalances.length === 0 ? (
          <div style={{
            textAlign: 'center',
            padding: '40px 0',
            color: FINCEPT.MUTED
          }}>
            <Wallet style={{ width: 48, height: 48, margin: '0 auto 12px', color: FINCEPT.MUTED }} />
            <div style={{ fontSize: '11px' }}>NO BALANCES FOUND</div>
          </div>
        ) : (
          <div style={{ display: 'flex', flexDirection: 'column', gap: '10px' }}>
            {aggregatedBalances
              .sort((a, b) => b.totalUsdValue - a.totalUsdValue)
              .map((balance) => (
                <div
                  key={balance.currency}
                  style={{
                    padding: '12px',
                    backgroundColor: FINCEPT.HEADER_BG,
                    border: `1px solid ${FINCEPT.BORDER}`,
                    borderRadius: '2px',
                    transition: 'all 0.2s'
                  }}
                  onMouseEnter={(e) => {
                    e.currentTarget.style.borderColor = FINCEPT.CYAN;
                    e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                  }}
                  onMouseLeave={(e) => {
                    e.currentTarget.style.borderColor = FINCEPT.BORDER;
                    e.currentTarget.style.backgroundColor = FINCEPT.HEADER_BG;
                  }}
                >
                  {/* Currency Header */}
                  <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '10px' }}>
                    <div style={{ display: 'flex', alignItems: 'center', gap: '10px' }}>
                      <div style={{
                        width: 32,
                        height: 32,
                        background: `linear-gradient(135deg, ${FINCEPT.CYAN}, ${FINCEPT.PURPLE})`,
                        borderRadius: '50%',
                        display: 'flex',
                        alignItems: 'center',
                        justifyContent: 'center',
                        color: FINCEPT.WHITE,
                        fontWeight: 700,
                        fontSize: '11px',
                        fontFamily: 'monospace'
                      }}>
                        {balance.currency.substring(0, 2)}
                      </div>
                      <div>
                        <div style={{ color: FINCEPT.WHITE, fontWeight: 600, fontSize: '12px', fontFamily: 'monospace' }}>
                          {balance.currency}
                        </div>
                        <div style={{ fontSize: '10px', color: FINCEPT.GRAY, fontFamily: 'monospace' }}>
                          {formatCrypto(balance.totalAmount)} {balance.currency}
                        </div>
                      </div>
                    </div>
                    <div style={{ textAlign: 'right' }}>
                      <div style={{ color: FINCEPT.WHITE, fontWeight: 600, fontSize: '12px', fontFamily: 'monospace' }}>
                        ${formatCurrency(balance.totalUsdValue)}
                      </div>
                      <div style={{ fontSize: '10px', color: FINCEPT.GRAY }}>
                        {((balance.totalUsdValue / totalPortfolioValue) * 100).toFixed(2)}%
                      </div>
                    </div>
                  </div>

                  {/* Exchange Breakdown */}
                  <div style={{ display: 'flex', flexDirection: 'column', gap: '6px', paddingLeft: '42px' }}>
                    {balance.exchanges.map((exch, idx) => (
                      <div
                        key={idx}
                        style={{
                          display: 'flex',
                          alignItems: 'center',
                          justifyContent: 'space-between',
                          fontSize: '10px',
                          padding: '6px 8px',
                          backgroundColor: `${FINCEPT.DARK_BG}80`,
                          borderRadius: '2px'
                        }}
                      >
                        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                          <span style={{
                            padding: '2px 8px',
                            backgroundColor: FINCEPT.HEADER_BG,
                            borderRadius: '2px',
                            color: FINCEPT.GRAY,
                            textTransform: 'uppercase',
                            fontSize: '9px',
                            letterSpacing: '0.5px'
                          }}>
                            {exch.exchange}
                          </span>
                          <span style={{ color: FINCEPT.GRAY, fontFamily: 'monospace' }}>
                            {formatCrypto(exch.amount)} {balance.currency}
                          </span>
                        </div>
                        <span style={{ color: FINCEPT.WHITE, fontFamily: 'monospace' }}>
                          ${formatCurrency(exch.usdValue)}
                        </span>
                      </div>
                    ))}
                  </div>

                  {/* Progress Bar */}
                  <div style={{
                    marginTop: '10px',
                    height: '4px',
                    backgroundColor: FINCEPT.HEADER_BG,
                    borderRadius: '2px',
                    overflow: 'hidden'
                  }}>
                    <div
                      style={{
                        height: '100%',
                        background: `linear-gradient(90deg, ${FINCEPT.CYAN}, ${FINCEPT.PURPLE})`,
                        width: `${(balance.totalUsdValue / totalPortfolioValue) * 100}%`,
                        transition: 'width 0.3s'
                      }}
                    />
                  </div>
                </div>
              ))}
          </div>
        )}
      </div>

      {/* Info Note */}
      <div style={{
        marginTop: '16px',
        padding: '10px',
        backgroundColor: `${FINCEPT.CYAN}08`,
        border: `1px solid ${FINCEPT.CYAN}25`,
        borderRadius: '2px'
      }}>
        <div style={{ fontSize: '10px', color: FINCEPT.CYAN, lineHeight: '1.5' }}>
          <strong>NOTE:</strong> This view aggregates balances from all connected exchanges. Values
          are updated every 30 seconds. Exchange-specific details are shown for each asset.
        </div>
      </div>
    </div>
  );
}
