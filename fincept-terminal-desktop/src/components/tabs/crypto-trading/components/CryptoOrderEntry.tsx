// CryptoOrderEntry.tsx - Order Entry Panel for Crypto Trading
import React from 'react';
import { FINCEPT } from '../constants';
import { EnhancedOrderForm } from '../../trading/core/OrderForm';
import { HyperLiquidVaultManager } from '../../trading/exchange-specific/hyperliquid';

interface CryptoOrderEntryProps {
  selectedSymbol: string;
  currentPrice: number;
  balance: number;
  tradingMode: 'paper' | 'live';
  activeBroker: string | null;
  isLoading: boolean;
  isConnecting: boolean;
  paperAdapter: any;
  realAdapter: any;
  onTradingModeChange: (mode: 'paper' | 'live') => void;
}

export function CryptoOrderEntry({
  selectedSymbol,
  currentPrice,
  balance,
  tradingMode,
  activeBroker,
  isLoading,
  isConnecting,
  paperAdapter,
  realAdapter,
  onTradingModeChange,
}: CryptoOrderEntryProps) {
  return (
    <div style={{
      height: '45%',
      borderBottom: `1px solid ${FINCEPT.BORDER}`,
      display: 'flex',
      flexDirection: 'column',
      overflow: 'hidden'
    }}>
      <div style={{
        padding: '8px 12px',
        backgroundColor: tradingMode === 'live' ? FINCEPT.RED : FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        fontSize: '10px',
        fontWeight: 700,
        color: tradingMode === 'live' ? FINCEPT.WHITE : FINCEPT.ORANGE,
        letterSpacing: '0.5px',
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center'
      }}>
        <span>ORDER ENTRY</span>
        <span style={{
          padding: '2px 8px',
          backgroundColor: tradingMode === 'live' ? FINCEPT.WHITE : FINCEPT.GREEN,
          color: tradingMode === 'live' ? FINCEPT.RED : FINCEPT.DARK_BG,
          fontSize: '9px',
          fontWeight: 700,
          borderRadius: '2px'
        }}>
          {tradingMode === 'live' ? '🔴 LIVE' : '⚪ PAPER'}
        </span>
      </div>
      <div style={{ flex: 1, overflow: 'auto', padding: '12px' }}>
        {/* Loading State */}
        {isLoading ? (
          <div style={{
            padding: '20px',
            textAlign: 'center',
            border: `1px solid ${FINCEPT.CYAN}`,
            backgroundColor: `${FINCEPT.CYAN}10`
          }}>
            <div style={{ color: FINCEPT.CYAN, fontSize: '11px', fontWeight: 700, marginBottom: '8px' }}>
              ⏳ LOADING...
            </div>
            <div style={{ color: FINCEPT.GRAY, fontSize: '10px' }}>
              Loading broker configurations...
            </div>
          </div>
        ) : tradingMode === 'paper' ? (
          /* PAPER TRADING MODE */
          !paperAdapter ? (
            <div style={{
              padding: '20px',
              textAlign: 'center',
              border: `1px solid ${FINCEPT.RED}`,
              backgroundColor: `${FINCEPT.RED}10`
            }}>
              <div style={{ color: FINCEPT.RED, fontSize: '12px', fontWeight: 700, marginBottom: '12px' }}>
                {isConnecting ? '⏳ CONNECTING...' : '❌ ADAPTER NOT READY'}
              </div>
              {!isConnecting && (
                <>
                  <div style={{ color: FINCEPT.WHITE, fontSize: '10px', marginBottom: '12px', lineHeight: '1.5' }}>
                    Paper trading adapter failed to initialize.
                  </div>
                  <button
                    onClick={() => window.location.reload()}
                    style={{
                      padding: '6px 12px',
                      backgroundColor: FINCEPT.ORANGE,
                      border: 'none',
                      color: FINCEPT.DARK_BG,
                      cursor: 'pointer',
                      fontSize: '10px',
                      fontWeight: 700,
                      borderRadius: '2px'
                    }}
                  >
                    RELOAD APPLICATION
                  </button>
                </>
              )}
            </div>
          ) : !paperAdapter.isConnected() ? (
            <div style={{
              padding: '20px',
              textAlign: 'center',
              border: `1px solid ${FINCEPT.YELLOW}`,
              backgroundColor: `${FINCEPT.YELLOW}10`
            }}>
              <div style={{ color: FINCEPT.YELLOW, fontSize: '11px', fontWeight: 700, marginBottom: '8px' }}>
                ⏳ CONNECTING...
              </div>
              <div style={{ color: FINCEPT.GRAY, fontSize: '10px' }}>
                Establishing connection to {activeBroker}...
              </div>
            </div>
          ) : (
            <>
              {/* Paper Trading Banner */}
              <div style={{
                padding: '8px',
                marginBottom: '12px',
                backgroundColor: `${FINCEPT.GREEN}15`,
                border: `1px solid ${FINCEPT.GREEN}`,
                borderRadius: '4px',
                textAlign: 'center'
              }}>
                <div style={{ color: FINCEPT.GREEN, fontSize: '10px', fontWeight: 700 }}>
                  ⚪ PAPER TRADING MODE
                </div>
                <div style={{ color: FINCEPT.GRAY, fontSize: '9px', marginTop: '2px' }}>
                  Simulated orders • No real funds at risk
                </div>
              </div>

              <EnhancedOrderForm
                symbol={selectedSymbol}
                currentPrice={currentPrice}
                balance={balance}
                onOrderPlaced={() => {
                  console.log('Paper order placed successfully');
                }}
              />

              {/* HyperLiquid Vault Manager */}
              <HyperLiquidVaultManager />
            </>
          )
        ) : (
          /* LIVE TRADING MODE */
          !realAdapter ? (
            <div style={{
              padding: '20px',
              textAlign: 'center',
              border: `1px solid ${FINCEPT.RED}`,
              backgroundColor: `${FINCEPT.RED}10`
            }}>
              <div style={{ color: FINCEPT.RED, fontSize: '12px', fontWeight: 700, marginBottom: '12px' }}>
                ❌ LIVE ADAPTER NOT CONFIGURED
              </div>
              <div style={{ color: FINCEPT.WHITE, fontSize: '10px', marginBottom: '12px' }}>
                Please configure API credentials for {activeBroker} to enable live trading.
              </div>
              <button
                onClick={() => onTradingModeChange('paper')}
                style={{
                  padding: '6px 12px',
                  backgroundColor: FINCEPT.GREEN,
                  border: 'none',
                  color: FINCEPT.DARK_BG,
                  cursor: 'pointer',
                  fontSize: '10px',
                  fontWeight: 700,
                  borderRadius: '2px'
                }}
              >
                SWITCH TO PAPER MODE
              </button>
            </div>
          ) : !realAdapter.isConnected() ? (
            <div style={{
              padding: '20px',
              textAlign: 'center',
              border: `1px solid ${FINCEPT.YELLOW}`,
              backgroundColor: `${FINCEPT.YELLOW}10`
            }}>
              <div style={{ color: FINCEPT.YELLOW, fontSize: '11px', fontWeight: 700, marginBottom: '8px' }}>
                ⏳ CONNECTING TO EXCHANGE...
              </div>
              <div style={{ color: FINCEPT.GRAY, fontSize: '10px' }}>
                Authenticating with {activeBroker}...
              </div>
            </div>
          ) : (
            <>
              {/* LIVE TRADING WARNING BANNER */}
              <div style={{
                padding: '10px',
                marginBottom: '12px',
                backgroundColor: `${FINCEPT.RED}20`,
                border: `2px solid ${FINCEPT.RED}`,
                borderRadius: '4px',
                textAlign: 'center'
              }}>
                <div style={{ color: FINCEPT.RED, fontSize: '12px', fontWeight: 700 }}>
                  🔴 LIVE TRADING MODE
                </div>
                <div style={{ color: FINCEPT.WHITE, fontSize: '10px', marginTop: '4px', fontWeight: 600 }}>
                  ⚠️ REAL FUNDS AT RISK ⚠️
                </div>
                <div style={{ color: FINCEPT.GRAY, fontSize: '9px', marginTop: '4px' }}>
                  Orders will be executed on {activeBroker?.toUpperCase()}
                </div>
              </div>

              <EnhancedOrderForm
                symbol={selectedSymbol}
                currentPrice={currentPrice}
                balance={balance}
                onOrderPlaced={() => {
                  console.log('LIVE order placed successfully');
                }}
              />

              {/* HyperLiquid Vault Manager */}
              <HyperLiquidVaultManager />
            </>
          )
        )}
      </div>
    </div>
  );
}

export default CryptoOrderEntry;
