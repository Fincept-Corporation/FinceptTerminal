/**
 * StakingPanel - Kraken staking management interface
 */

import React, { useState, useCallback, useEffect } from 'react';
import { DollarSign, TrendingUp, Clock, AlertCircle, CheckCircle, Loader } from 'lucide-react';
import { useBrokerContext } from '../../../../../contexts/BrokerContext';

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

  // Fetch staking assets on mount
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
      // Fetch balance
      const balance = await activeAdapter.fetchBalance();
      const balances = balance as any; // Type assertion for dynamic currency access

      // Mock staking data (in production, fetch from Kraken API)
      const stakableAssets: StakingAsset[] = [
        {
          asset: 'ETH',
          balance: balances['ETH']?.total || 0,
          stakedBalance: 0,
          availableBalance: balances['ETH']?.free || 0,
          apy: 4.5,
          lockPeriod: 'Flexible',
          method: 'ETH2.S',
        },
        {
          asset: 'DOT',
          balance: balances['DOT']?.total || 0,
          stakedBalance: 0,
          availableBalance: balances['DOT']?.free || 0,
          apy: 12.0,
          lockPeriod: '28 days',
          method: 'DOT.S',
        },
        {
          asset: 'SOL',
          balance: balances['SOL']?.total || 0,
          stakedBalance: 0,
          availableBalance: balances['SOL']?.free || 0,
          apy: 6.5,
          lockPeriod: '2 days',
          method: 'SOL.S',
        },
        {
          asset: 'ADA',
          balance: balances['ADA']?.total || 0,
          stakedBalance: 0,
          availableBalance: balances['ADA']?.free || 0,
          apy: 5.0,
          lockPeriod: 'Flexible',
          method: 'ADA.S',
        },
        {
          asset: 'MATIC',
          balance: balances['MATIC']?.total || 0,
          stakedBalance: 0,
          availableBalance: balances['MATIC']?.free || 0,
          apy: 8.0,
          lockPeriod: '3 days',
          method: 'MATIC.S',
        },
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

    // Set available staking methods for selected asset
    const assetData = stakingAssets.find(a => a.asset === asset);
    if (assetData) {
      setStakingMethods([
        {
          method: assetData.method,
          apy: assetData.apy,
          lockPeriod: assetData.lockPeriod,
          minAmount: 0.01,
        },
      ]);
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
      // Call Kraken adapter's stakeAsset method
      if (typeof (activeAdapter as any).stakeAsset === 'function') {
        await (activeAdapter as any).stakeAsset(selectedAsset, parseFloat(amount), selectedMethod);
        setSuccess(`Successfully staked ${amount} ${selectedAsset}`);
        setAmount('');

        // Refresh staking assets after 2 seconds
        setTimeout(() => {
          fetchStakingAssets();
        }, 2000);
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
      // Call Kraken adapter's unstakeAsset method
      if (typeof (activeAdapter as any).unstakeAsset === 'function') {
        await (activeAdapter as any).unstakeAsset(selectedAsset, parseFloat(amount));
        setSuccess(`Successfully unstaked ${amount} ${selectedAsset}`);
        setAmount('');

        // Refresh staking assets after 2 seconds
        setTimeout(() => {
          fetchStakingAssets();
        }, 2000);
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
      <div className="bg-gray-900 border border-gray-800 rounded p-6">
        <div className="flex items-center justify-center py-8">
          <Loader className="w-6 h-6 text-blue-500 animate-spin" />
        </div>
      </div>
    );
  }

  return (
    <div className="bg-gray-900 border border-gray-800 rounded p-6">
      {/* Header */}
      <div className="flex items-center justify-between mb-6">
        <div className="flex items-center gap-2">
          <DollarSign className="w-5 h-5 text-green-500" />
          <span className="text-lg font-semibold text-white">Kraken Staking</span>
        </div>
        <button
          onClick={fetchStakingAssets}
          className="px-3 py-1 text-xs text-blue-400 hover:text-blue-300 border border-blue-500/30 hover:border-blue-400/50 rounded transition"
        >
          Refresh
        </button>
      </div>

      {/* Staking Assets List */}
      {stakingAssets.length === 0 ? (
        <div className="text-center py-8 text-gray-500">
          <AlertCircle className="w-12 h-12 mx-auto mb-3 text-gray-600" />
          <div className="text-sm">No stakable assets found</div>
          <div className="text-xs mt-1">Add funds to your Kraken account to start staking</div>
        </div>
      ) : (
        <>
          {/* Asset Selection Grid */}
          <div className="grid grid-cols-2 gap-3 mb-6">
            {stakingAssets.map((asset) => (
              <button
                key={asset.asset}
                onClick={() => handleAssetSelect(asset.asset)}
                className={`p-4 rounded border-2 transition ${
                  selectedAsset === asset.asset
                    ? 'border-blue-500 bg-blue-500/10'
                    : 'border-gray-700 bg-gray-800/50 hover:border-gray-600'
                }`}
              >
                <div className="flex items-center justify-between mb-2">
                  <span className="text-white font-semibold">{asset.asset}</span>
                  <div className="flex items-center gap-1 text-green-500 text-xs">
                    <TrendingUp className="w-3 h-3" />
                    {asset.apy.toFixed(2)}% APY
                  </div>
                </div>
                <div className="text-xs text-gray-400">
                  <div>Balance: {asset.balance.toFixed(8)}</div>
                  {asset.stakedBalance > 0 && (
                    <div className="text-green-500">Staked: {asset.stakedBalance.toFixed(8)}</div>
                  )}
                </div>
                <div className="flex items-center gap-1 text-xs text-gray-500 mt-2">
                  <Clock className="w-3 h-3" />
                  {asset.lockPeriod}
                </div>
              </button>
            ))}
          </div>

          {/* Staking Form */}
          {selectedAsset && (
            <div className="border-t border-gray-800 pt-6 space-y-4">
              {/* Amount Input */}
              <div>
                <label className="block text-xs text-gray-400 mb-2">Amount to Stake</label>
                <input
                  type="number"
                  value={amount}
                  onChange={(e) => setAmount(e.target.value)}
                  placeholder="0.00"
                  className="w-full px-4 py-3 bg-gray-800 border border-gray-700 rounded text-white focus:outline-none focus:border-blue-500"
                  step="0.00000001"
                  min="0"
                />
                {/* Quick Percentage Buttons */}
                <div className="flex gap-2 mt-2">
                  {[25, 50, 75, 100].map((pct) => (
                    <button
                      key={pct}
                      onClick={() => setPercentage(pct)}
                      className="flex-1 px-2 py-1 text-xs bg-gray-800 hover:bg-gray-700 border border-gray-700 rounded transition"
                    >
                      {pct}%
                    </button>
                  ))}
                </div>
              </div>

              {/* Info Display */}
              {selectedAsset && amount && parseFloat(amount) > 0 && (
                <div className="p-3 bg-blue-500/10 border border-blue-500/20 rounded">
                  <div className="text-xs space-y-1">
                    <div className="flex justify-between">
                      <span className="text-gray-400">Estimated yearly earnings:</span>
                      <span className="text-green-500 font-semibold">
                        {(
                          parseFloat(amount) *
                          (stakingAssets.find((a) => a.asset === selectedAsset)?.apy || 0) /
                          100
                        ).toFixed(8)}{' '}
                        {selectedAsset}
                      </span>
                    </div>
                    <div className="flex justify-between">
                      <span className="text-gray-400">Lock period:</span>
                      <span className="text-white">
                        {stakingAssets.find((a) => a.asset === selectedAsset)?.lockPeriod}
                      </span>
                    </div>
                  </div>
                </div>
              )}

              {/* Error/Success Messages */}
              {error && (
                <div className="p-3 bg-red-500/10 border border-red-500/20 rounded flex items-start gap-2">
                  <AlertCircle className="w-4 h-4 text-red-500 flex-shrink-0 mt-0.5" />
                  <span className="text-xs text-red-400">{error}</span>
                </div>
              )}

              {success && (
                <div className="p-3 bg-green-500/10 border border-green-500/20 rounded flex items-start gap-2">
                  <CheckCircle className="w-4 h-4 text-green-500 flex-shrink-0 mt-0.5" />
                  <span className="text-xs text-green-400">{success}</span>
                </div>
              )}

              {/* Action Buttons */}
              <div className="flex gap-3">
                <button
                  onClick={handleStake}
                  disabled={isStaking || !amount || parseFloat(amount) <= 0}
                  className="flex-1 px-4 py-3 bg-green-600 hover:bg-green-700 disabled:bg-gray-700 disabled:cursor-not-allowed text-white font-semibold rounded transition flex items-center justify-center gap-2"
                >
                  {isStaking ? (
                    <>
                      <Loader className="w-4 h-4 animate-spin" />
                      Staking...
                    </>
                  ) : (
                    <>
                      <TrendingUp className="w-4 h-4" />
                      Stake
                    </>
                  )}
                </button>

                <button
                  onClick={handleUnstake}
                  disabled={
                    isUnstaking ||
                    !amount ||
                    parseFloat(amount) <= 0 ||
                    (stakingAssets.find((a) => a.asset === selectedAsset)?.stakedBalance || 0) === 0
                  }
                  className="flex-1 px-4 py-3 bg-orange-600 hover:bg-orange-700 disabled:bg-gray-700 disabled:cursor-not-allowed text-white font-semibold rounded transition flex items-center justify-center gap-2"
                >
                  {isUnstaking ? (
                    <>
                      <Loader className="w-4 h-4 animate-spin" />
                      Unstaking...
                    </>
                  ) : (
                    'Unstake'
                  )}
                </button>
              </div>

              {/* Info Note */}
              <div className="p-3 bg-gray-800/50 border border-gray-700 rounded">
                <div className="text-xs text-gray-400">
                  <strong className="text-gray-300">Note:</strong> Staking rewards are
                  automatically paid out. Unstaking may take up to the lock period to complete.
                  Check Kraken's terms for each asset's specific staking conditions.
                </div>
              </div>
            </div>
          )}
        </>
      )}
    </div>
  );
}
