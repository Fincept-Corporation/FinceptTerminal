/**
 * StakingPanel - Kraken staking management interface
 * Fincept Terminal Style
 */

import React, { useState, useCallback, useEffect } from 'react';
import { DollarSign, TrendingUp, Clock, AlertCircle, CheckCircle, Loader } from 'lucide-react';
import { useBrokerContext } from '../../../../../contexts/BrokerContext';

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

interface StakingAsset {
  asset: string;
  balance: number;
  stakedBalance: number;
  availableBalance: number;
  apy: number;
  lockPeriod: string;
  method: string;
}

interface StakingMethod {
  method: string;
  apy: number;
  lockPeriod: string;
  minAmount: number;
}

export function StakingPanel() {
  const { activeBroker, activeAdapter } = useBrokerContext();
  const [stakingAssets, setStakingAssets] = useState<StakingAsset[]>([]);
  const [selectedAsset, setSelectedAsset] = useState<string>('');
  const [stakingMethods, setStakingMethods] = useState<StakingMethod[]>([]);
  const [selectedMethod, setSelectedMethod] = useState<string>('');
  const [amount, setAmount] = useState<string>('');
  const [isLoading, setIsLoading] = useState(true);
  const [isStaking, setIsStaking] = useState(false);
  const [isUnstaking, setIsUnstaking] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [success, setSuccess] = useState<string | null>(null);

  useEffect(() => {
    if (activeBroker === 'kraken') {
      fetchStakingAssets();
    }
  }, [activeBroker]);

  const fetchStakingAssets = useCallback(async () => {
    if (!activeAdapter) {
      setIsLoading(false);
      return;
    }

    setIsLoading(true);
    setError(null);
    try {
      const balance = await activeAdapter.fetchBalance();
      const balances = balance as any;

      const stakableAssets: StakingAsset[] = [
        { asset: 'ETH', balance: balances['ETH']?.total || 0, stakedBalance: 0, availableBalance: balances['ETH']?.free || 0, apy: 4.5, lockPeriod: 'Flexible', method: 'ETH2.S' },
        { asset: 'DOT', balance: balances['DOT']?.total || 0, stakedBalance: 0, availableBalance: balances['DOT']?.free || 0, apy: 12.0, lockPeriod: '28 days', method: 'DOT.S' },
        { asset: 'SOL', balance: balances['SOL']?.total || 0, stakedBalance: 0, availableBalance: balances['SOL']?.free || 0, apy: 6.5, lockPeriod: '2 days', method: 'SOL.S' },
        { asset: 'ADA', balance: balances['ADA']?.total || 0, stakedBalance: 0, availableBalance: balances['ADA']?.free || 0, apy: 5.0, lockPeriod: 'Flexible', method: 'ADA.S' },
        { asset: 'MATIC', balance: balances['MATIC']?.total || 0, stakedBalance: 0, availableBalance: balances['MATIC']?.free || 0, apy: 8.0, lockPeriod: '3 days', method: 'MATIC.S' },
      ];

      setStakingAssets(stakableAssets.filter(a => a.balance > 0 || a.stakedBalance > 0));
    } catch (err) {
      setError('Failed to fetch staking assets');
      console.error('Staking fetch error:', err);
    } finally {
      setIsLoading(false);
    }
  }, [activeAdapter]);

  const handleAssetSelect = (asset: string) => {
    setSelectedAsset(asset);
    setAmount('');
    setError(null);
    setSuccess(null);

    const assetData = stakingAssets.find(a => a.asset === asset);
    if (assetData) {
      setStakingMethods([{ method: assetData.method, apy: assetData.apy, lockPeriod: assetData.lockPeriod, minAmount: 0.01 }]);
      setSelectedMethod(assetData.method);
    }
  };

  const handleStake = async () => {
    if (!selectedAsset || !selectedMethod || !amount || parseFloat(amount) <= 0) {
      setError('Please fill all fields');
      return;
    }

    const assetData = stakingAssets.find(a => a.asset === selectedAsset);
    if (!assetData || parseFloat(amount) > assetData.availableBalance) {
      setError('Insufficient balance');
      return;
    }

    setIsStaking(true);
    setError(null);
    setSuccess(null);

    try {
      if (typeof (activeAdapter as any).stakeAsset === 'function') {
        await (activeAdapter as any).stakeAsset(selectedAsset, parseFloat(amount), selectedMethod);
        setSuccess(`Successfully staked ${amount} ${selectedAsset}`);
        setAmount('');
        setTimeout(() => fetchStakingAssets(), 2000);
      } else {
        setError('Staking not supported by current adapter');
      }
    } catch (err: any) {
      setError(err?.message || 'Failed to stake asset');
      console.error('Staking error:', err);
    } finally {
      setIsStaking(false);
    }
  };

  const handleUnstake = async () => {
    if (!selectedAsset || !amount || parseFloat(amount) <= 0) {
      setError('Please enter valid amount');
      return;
    }

    const assetData = stakingAssets.find(a => a.asset === selectedAsset);
    if (!assetData || parseFloat(amount) > assetData.stakedBalance) {
      setError('Insufficient staked balance');
      return;
    }

    setIsUnstaking(true);
    setError(null);
    setSuccess(null);

    try {
      if (typeof (activeAdapter as any).unstakeAsset === 'function') {
        await (activeAdapter as any).unstakeAsset(selectedAsset, parseFloat(amount));
        setSuccess(`Successfully unstaked ${amount} ${selectedAsset}`);
        setAmount('');
        setTimeout(() => fetchStakingAssets(), 2000);
      } else {
        setError('Unstaking not supported by current adapter');
      }
    } catch (err: any) {
      setError(err?.message || 'Failed to unstake asset');
      console.error('Unstaking error:', err);
    } finally {
      setIsUnstaking(false);
    }
  };

  const setPercentage = (percentage: number) => {
    if (!selectedAsset) return;
    const assetData = stakingAssets.find(a => a.asset === selectedAsset);
    if (assetData) {
      const calcAmount = (assetData.availableBalance * percentage) / 100;
      setAmount(calcAmount.toFixed(8));
    }
  };

  if (activeBroker !== 'kraken') {
    return null;
  }

  if (isLoading) {
    return (
      <div style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '24px' }}>
        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', padding: '32px 0' }}>
          <Loader style={{ width: 24, height: 24, color: FINCEPT.BLUE, animation: 'spin 1s linear infinite' }} />
        </div>
      </div>
    );
  }

  return (
    <div style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', padding: '16px', height: '100%', overflow: 'auto' }}>
      {/* Header */}
      <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '16px', paddingBottom: '12px', borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <DollarSign style={{ width: 18, height: 18, color: FINCEPT.GREEN }} />
          <span style={{ fontSize: '13px', fontWeight: 600, color: FINCEPT.WHITE, letterSpacing: '0.5px' }}>KRAKEN STAKING</span>
        </div>
        <button onClick={fetchStakingAssets} style={{ padding: '6px 12px', fontSize: '10px', color: FINCEPT.CYAN, backgroundColor: 'transparent', border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', cursor: 'pointer', transition: 'all 0.2s' }}
          onMouseEnter={(e) => { e.currentTarget.style.borderColor = FINCEPT.CYAN; e.currentTarget.style.backgroundColor = `${FINCEPT.CYAN}15`; }}
          onMouseLeave={(e) => { e.currentTarget.style.borderColor = FINCEPT.BORDER; e.currentTarget.style.backgroundColor = 'transparent'; }}>
          REFRESH
        </button>
      </div>

      {stakingAssets.length === 0 ? (
        <div style={{ textAlign: 'center', padding: '40px 0', color: FINCEPT.MUTED }}>
          <AlertCircle style={{ width: 48, height: 48, margin: '0 auto 12px', color: FINCEPT.MUTED }} />
          <div style={{ fontSize: '11px', marginBottom: '6px' }}>NO STAKABLE ASSETS FOUND</div>
          <div style={{ fontSize: '10px' }}>Add funds to your Kraken account to start staking</div>
        </div>
      ) : (
        <>
          {/* Asset Selection Grid */}
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '10px', marginBottom: '16px' }}>
            {stakingAssets.map((asset) => (
              <div key={asset.asset} onClick={() => handleAssetSelect(asset.asset)}
                style={{ padding: '12px', borderRadius: '2px', border: `2px solid ${selectedAsset === asset.asset ? FINCEPT.CYAN : FINCEPT.BORDER}`, backgroundColor: selectedAsset === asset.asset ? `${FINCEPT.CYAN}10` : FINCEPT.HEADER_BG, cursor: 'pointer', transition: 'all 0.2s' }}
                onMouseEnter={(e) => { if (selectedAsset !== asset.asset) e.currentTarget.style.borderColor = FINCEPT.MUTED; }}
                onMouseLeave={(e) => { if (selectedAsset !== asset.asset) e.currentTarget.style.borderColor = FINCEPT.BORDER; }}>
                <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '8px' }}>
                  <span style={{ color: FINCEPT.WHITE, fontWeight: 700, fontSize: '12px', fontFamily: 'monospace' }}>{asset.asset}</span>
                  <div style={{ display: 'flex', alignItems: 'center', gap: '4px', color: FINCEPT.GREEN, fontSize: '10px' }}>
                    <TrendingUp style={{ width: 12, height: 12 }} />
                    {asset.apy.toFixed(2)}% APY
                  </div>
                </div>
                <div style={{ fontSize: '10px', color: FINCEPT.GRAY }}>
                  <div>BALANCE: {asset.balance.toFixed(8)}</div>
                  {asset.stakedBalance > 0 && <div style={{ color: FINCEPT.GREEN }}>STAKED: {asset.stakedBalance.toFixed(8)}</div>}
                </div>
                <div style={{ display: 'flex', alignItems: 'center', gap: '4px', fontSize: '10px', color: FINCEPT.MUTED, marginTop: '6px' }}>
                  <Clock style={{ width: 10, height: 10 }} />
                  {asset.lockPeriod}
                </div>
              </div>
            ))}
          </div>

          {/* Staking Form */}
          {selectedAsset && (
            <div style={{ borderTop: `1px solid ${FINCEPT.BORDER}`, paddingTop: '16px' }}>
              <div style={{ marginBottom: '12px' }}>
                <label style={{ display: 'block', fontSize: '10px', color: FINCEPT.GRAY, marginBottom: '6px', letterSpacing: '0.5px' }}>AMOUNT TO STAKE</label>
                <input type="text" inputMode="decimal" value={amount} onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setAmount(v); }} placeholder="0.00"
                  style={{ width: '100%', padding: '10px 12px', backgroundColor: FINCEPT.HEADER_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', color: FINCEPT.WHITE, fontSize: '12px', fontFamily: 'monospace' }} />
                <div style={{ display: 'flex', gap: '6px', marginTop: '8px' }}>
                  {[25, 50, 75, 100].map((pct) => (
                    <button key={pct} onClick={() => setPercentage(pct)}
                      style={{ flex: 1, padding: '6px', fontSize: '10px', backgroundColor: FINCEPT.HEADER_BG, color: FINCEPT.GRAY, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', cursor: 'pointer', transition: 'all 0.2s' }}
                      onMouseEnter={(e) => { e.currentTarget.style.backgroundColor = FINCEPT.HOVER; e.currentTarget.style.color = FINCEPT.WHITE; }}
                      onMouseLeave={(e) => { e.currentTarget.style.backgroundColor = FINCEPT.HEADER_BG; e.currentTarget.style.color = FINCEPT.GRAY; }}>
                      {pct}%
                    </button>
                  ))}
                </div>
              </div>

              {selectedAsset && amount && parseFloat(amount) > 0 && (
                <div style={{ padding: '10px', backgroundColor: `${FINCEPT.CYAN}08`, border: `1px solid ${FINCEPT.CYAN}25`, borderRadius: '2px', marginBottom: '12px' }}>
                  <div style={{ fontSize: '10px' }}>
                    <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '4px' }}>
                      <span style={{ color: FINCEPT.GRAY }}>ESTIMATED YEARLY EARNINGS:</span>
                      <span style={{ color: FINCEPT.GREEN, fontWeight: 700, fontFamily: 'monospace' }}>
                        {(parseFloat(amount) * (stakingAssets.find((a) => a.asset === selectedAsset)?.apy || 0) / 100).toFixed(8)} {selectedAsset}
                      </span>
                    </div>
                    <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                      <span style={{ color: FINCEPT.GRAY }}>LOCK PERIOD:</span>
                      <span style={{ color: FINCEPT.WHITE, fontWeight: 600 }}>{stakingAssets.find((a) => a.asset === selectedAsset)?.lockPeriod}</span>
                    </div>
                  </div>
                </div>
              )}

              {error && (
                <div style={{ padding: '10px', backgroundColor: `${FINCEPT.RED}10`, border: `1px solid ${FINCEPT.RED}30`, borderRadius: '2px', display: 'flex', alignItems: 'flex-start', gap: '8px', marginBottom: '12px' }}>
                  <AlertCircle style={{ width: 14, height: 14, color: FINCEPT.RED, flexShrink: 0, marginTop: '2px' }} />
                  <span style={{ fontSize: '10px', color: FINCEPT.RED }}>{error}</span>
                </div>
              )}

              {success && (
                <div style={{ padding: '10px', backgroundColor: `${FINCEPT.GREEN}10`, border: `1px solid ${FINCEPT.GREEN}30`, borderRadius: '2px', display: 'flex', alignItems: 'flex-start', gap: '8px', marginBottom: '12px' }}>
                  <CheckCircle style={{ width: 14, height: 14, color: FINCEPT.GREEN, flexShrink: 0, marginTop: '2px' }} />
                  <span style={{ fontSize: '10px', color: FINCEPT.GREEN }}>{success}</span>
                </div>
              )}

              <div style={{ display: 'flex', gap: '10px', marginBottom: '12px' }}>
                <button onClick={handleStake} disabled={isStaking || !amount || parseFloat(amount) <= 0}
                  style={{ flex: 1, padding: '10px 14px', backgroundColor: isStaking || !amount || parseFloat(amount) <= 0 ? FINCEPT.MUTED : FINCEPT.GREEN, color: FINCEPT.WHITE, fontSize: '11px', fontWeight: 700, border: 'none', borderRadius: '2px', cursor: isStaking || !amount || parseFloat(amount) <= 0 ? 'not-allowed' : 'pointer', display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '6px', opacity: isStaking || !amount || parseFloat(amount) <= 0 ? 0.5 : 1, transition: 'all 0.2s', letterSpacing: '0.5px' }}>
                  {isStaking ? <><Loader style={{ width: 14, height: 14, animation: 'spin 1s linear infinite' }} />STAKING...</> : <><TrendingUp style={{ width: 14, height: 14 }} />STAKE</>}
                </button>
                <button onClick={handleUnstake} disabled={isUnstaking || !amount || parseFloat(amount) <= 0 || (stakingAssets.find((a) => a.asset === selectedAsset)?.stakedBalance || 0) === 0}
                  style={{ flex: 1, padding: '10px 14px', backgroundColor: isUnstaking || !amount || parseFloat(amount) <= 0 ? FINCEPT.MUTED : FINCEPT.ORANGE, color: FINCEPT.WHITE, fontSize: '11px', fontWeight: 700, border: 'none', borderRadius: '2px', cursor: isUnstaking || !amount || parseFloat(amount) <= 0 ? 'not-allowed' : 'pointer', display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '6px', opacity: isUnstaking || !amount || parseFloat(amount) <= 0 ? 0.5 : 1, transition: 'all 0.2s', letterSpacing: '0.5px' }}>
                  {isUnstaking ? <><Loader style={{ width: 14, height: 14, animation: 'spin 1s linear infinite' }} />UNSTAKING...</> : 'UNSTAKE'}
                </button>
              </div>

              <div style={{ padding: '10px', backgroundColor: FINCEPT.HEADER_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px' }}>
                <div style={{ fontSize: '10px', color: FINCEPT.GRAY, lineHeight: '1.5' }}>
                  <strong style={{ color: FINCEPT.WHITE }}>NOTE:</strong> Staking rewards are automatically paid out. Unstaking may take up to the lock period to complete. Check Kraken's terms for each asset's specific staking conditions.
                </div>
              </div>
            </div>
          )}
        </>
      )}
    </div>
  );
}
