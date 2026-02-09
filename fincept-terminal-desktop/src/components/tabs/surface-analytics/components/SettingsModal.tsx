/**
 * Surface Analytics - Settings Modal Component
 * Configuration for symbols, date ranges, data sources, and API credentials
 */

import React, { useState, useEffect, useCallback } from 'react';
import { X, Plus, Trash2, Key, Eye, EyeOff, AlertCircle, CheckCircle, Search, Loader2, Database, ChevronDown, ChevronUp, Clock } from 'lucide-react';
import { invoke } from '@tauri-apps/api/core';
import { useTranslation } from 'react-i18next';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import type { SurfaceAnalyticsConfig, OHLCVTimeframe } from '../types';
import { VOL_SURFACE_SYMBOLS, CORRELATION_ASSETS } from '../constants';
import { sqliteService } from '@/services/core/sqliteService';
import { DatasetBrowser } from './DatasetBrowser';
import { CostEstimator } from './CostEstimator';
import { TimeframeSelector } from './TimeframeSelector';
import { ReferenceDataPanel } from './ReferenceDataPanel';
import { OrderBookPanel } from './OrderBookPanel';
import { LiveStreamPanel } from './LiveStreamPanel';
import { FuturesPanel } from './FuturesPanel';
import { BatchDownloadPanel } from './BatchDownloadPanel';
import { AdditionalSchemasPanel } from './AdditionalSchemasPanel';
import { useDatabentoApi } from '../hooks/useDatabentoApi';

interface SettingsModalProps {
  isOpen: boolean;
  onClose: () => void;
  config: SurfaceAnalyticsConfig;
  onSave: (config: SurfaceAnalyticsConfig) => void;
  onApiKeyChange?: () => void;
  accentColor: string;
  textColor: string;
}

export const SettingsModal: React.FC<SettingsModalProps> = ({
  isOpen,
  onClose,
  config,
  onSave,
  onApiKeyChange,
  accentColor,
  textColor,
}) => {
  const { t } = useTranslation('surfaceAnalytics');
  const { colors, fontSize, fontFamily } = useTerminalTheme();
  const [localConfig, setLocalConfig] = useState<SurfaceAnalyticsConfig>(config);
  const [newAsset, setNewAsset] = useState('');
  const [apiKey, setApiKey] = useState('');
  const [showApiKey, setShowApiKey] = useState(false);
  const [apiKeyStatus, setApiKeyStatus] = useState<'none' | 'valid' | 'invalid' | 'saving'>('none');
  const [hasExistingKey, setHasExistingKey] = useState(false);

  // Symbol search state
  const [symbolSearchQuery, setSymbolSearchQuery] = useState('');
  const [symbolSearching, setSymbolSearching] = useState(false);
  const [symbolSearchResult, setSymbolSearchResult] = useState<'none' | 'valid' | 'invalid'>('none');
  const [customSymbol, setCustomSymbol] = useState('');

  // Advanced sections toggle
  const [showDatasetBrowser, setShowDatasetBrowser] = useState(false);
  const [showCostEstimator, setShowCostEstimator] = useState(false);
  const [showTimeframeSelector, setShowTimeframeSelector] = useState(false);
  const [showReferenceData, setShowReferenceData] = useState(false);
  const [showOrderBook, setShowOrderBook] = useState(false);
  const [showLiveStream, setShowLiveStream] = useState(false);
  const [showFutures, setShowFutures] = useState(false);
  const [showBatchDownload, setShowBatchDownload] = useState(false);
  const [showAdditionalSchemas, setShowAdditionalSchemas] = useState(false);
  const [storedApiKey, setStoredApiKey] = useState<string | null>(null);

  // Live streaming hook
  const {
    startLiveStream,
    stopLiveStream,
    stopAllLiveStreams,
    activeStreams,
  } = useDatabentoApi();

  // Load stored API key for child components
  useEffect(() => {
    const loadStoredKey = async () => {
      try {
        const key = await sqliteService.getApiKey('DATABENTO_API_KEY');
        setStoredApiKey(key);
      } catch (err) {
        console.error('Failed to load stored API key:', err);
      }
    };
    if (isOpen) {
      loadStoredKey();
    }
  }, [isOpen, hasExistingKey]);

  // Load existing API key status on mount
  useEffect(() => {
    if (isOpen) {
      loadApiKeyStatus();
    }
  }, [isOpen]);

  const loadApiKeyStatus = async () => {
    try {
      const existingKey = await sqliteService.getApiKey('DATABENTO_API_KEY');
      if (existingKey && existingKey.startsWith('db-')) {
        setHasExistingKey(true);
        setApiKeyStatus('valid');
        setApiKey(''); // Don't show existing key for security
      } else {
        setHasExistingKey(false);
        setApiKeyStatus('none');
      }
    } catch (err) {
      console.error('Failed to load API key status:', err);
      setApiKeyStatus('none');
    }
  };

  const handleSaveApiKey = async () => {
    if (!apiKey.trim()) return;

    // Validate format
    if (!apiKey.startsWith('db-')) {
      setApiKeyStatus('invalid');
      return;
    }

    setApiKeyStatus('saving');
    try {
      await sqliteService.setApiKey('DATABENTO_API_KEY', apiKey.trim());
      setApiKeyStatus('valid');
      setHasExistingKey(true);
      setApiKey('');
      onApiKeyChange?.();
    } catch (err) {
      console.error('Failed to save API key:', err);
      setApiKeyStatus('invalid');
    }
  };

  const handleRemoveApiKey = async () => {
    try {
      await sqliteService.setApiKey('DATABENTO_API_KEY', '');
      setApiKeyStatus('none');
      setHasExistingKey(false);
      setApiKey('');
      onApiKeyChange?.();
    } catch (err) {
      console.error('Failed to remove API key:', err);
    }
  };

  // Symbol search handler
  const handleSymbolSearch = useCallback(async () => {
    if (!symbolSearchQuery.trim()) return;

    const symbol = symbolSearchQuery.toUpperCase().trim();
    setSymbolSearching(true);
    setSymbolSearchResult('none');

    try {
      // Get API key for search
      const storedApiKey = await sqliteService.getApiKey('DATABENTO_API_KEY');
      if (!storedApiKey) {
        // If no API key, just accept the symbol without validation
        setLocalConfig({ ...localConfig, selectedSymbol: symbol });
        setCustomSymbol(symbol);
        setSymbolSearchResult('valid');
        setSymbolSearchQuery('');
        return;
      }

      // Try to resolve the symbol via Databento
      const result = await invoke<string>('databento_resolve_symbols', {
        apiKey: storedApiKey,
        symbols: [symbol],
        dataset: null,
        stypeIn: null,
        stypeOut: null,
      });

      const parsed = JSON.parse(result);

      if (!parsed.error && parsed.mappings && parsed.mappings.length > 0) {
        setLocalConfig({ ...localConfig, selectedSymbol: symbol });
        setCustomSymbol(symbol);
        setSymbolSearchResult('valid');
        setSymbolSearchQuery('');
      } else {
        // Symbol not found, but still allow it (for demo mode)
        setLocalConfig({ ...localConfig, selectedSymbol: symbol });
        setCustomSymbol(symbol);
        setSymbolSearchResult('valid');
        setSymbolSearchQuery('');
      }
    } catch (err) {
      console.error('Symbol search failed:', err);
      // Allow symbol anyway for demo mode
      setLocalConfig({ ...localConfig, selectedSymbol: symbolSearchQuery.toUpperCase().trim() });
      setCustomSymbol(symbolSearchQuery.toUpperCase().trim());
      setSymbolSearchResult('valid');
      setSymbolSearchQuery('');
    } finally {
      setSymbolSearching(false);
    }
  }, [symbolSearchQuery, localConfig]);

  if (!isOpen) return null;

  const handleAddAsset = () => {
    if (newAsset && !localConfig.correlationAssets.includes(newAsset.toUpperCase())) {
      setLocalConfig({
        ...localConfig,
        correlationAssets: [...localConfig.correlationAssets, newAsset.toUpperCase()],
      });
      setNewAsset('');
    }
  };

  const handleRemoveAsset = (asset: string) => {
    setLocalConfig({
      ...localConfig,
      correlationAssets: localConfig.correlationAssets.filter(a => a !== asset),
    });
  };

  const handleSave = () => {
    onSave(localConfig);
    onClose();
  };

  const inputStyle: React.CSSProperties = {
    width: '100%',
    padding: '8px 10px',
    backgroundColor: colors.background,
    color: textColor,
    border: `1px solid ${colors.textMuted}`,
    borderRadius: 'var(--ft-border-radius)',
    fontSize: fontSize.body,
    fontFamily,
    outline: 'none',
  };

  return (
    <div
      className="fixed inset-0 flex items-center justify-center z-50"
      style={{ backgroundColor: 'rgba(0,0,0,0.8)' }}
      onClick={onClose}
    >
      <div
        className="w-full max-w-lg p-4 overflow-hidden"
        style={{
          backgroundColor: colors.panel,
          border: `1px solid ${colors.textMuted}`,
        }}
        onClick={e => e.stopPropagation()}
      >
        {/* Header */}
        <div className="flex items-center justify-between mb-4">
          <h2 className="text-sm font-bold" style={{ color: accentColor }}>
            {t('settings.title')}
          </h2>
          <button onClick={onClose} style={{ color: colors.textMuted }}>
            <X size={16} />
          </button>
        </div>

        {/* Databento API Key Section */}
        <div
          className="mb-4 p-3"
          style={{
            backgroundColor: colors.background,
            border: `1px solid ${apiKeyStatus === 'valid' ? colors.success : apiKeyStatus === 'invalid' ? colors.alert : colors.textMuted}`,
            borderRadius: 'var(--ft-border-radius)',
          }}
        >
          <div className="flex items-center gap-2 mb-2">
            <Key size={14} style={{ color: accentColor }} />
            <label className="text-xs font-bold" style={{ color: accentColor, letterSpacing: '0.5px' }}>
              {t('settings.databentoApiKey')}
            </label>
            {apiKeyStatus === 'valid' && (
              <span className="flex items-center gap-1 text-xs" style={{ color: colors.success }}>
                <CheckCircle size={12} /> {t('settings.configured')}
              </span>
            )}
            {apiKeyStatus === 'invalid' && (
              <span className="flex items-center gap-1 text-xs" style={{ color: colors.alert }}>
                <AlertCircle size={12} /> {t('settings.invalidFormat')}
              </span>
            )}
          </div>

          {hasExistingKey ? (
            <div className="flex items-center justify-between">
              <span className="text-xs" style={{ color: colors.textMuted }}>
                {t('settings.apiKeyConfigured')}
              </span>
              <button
                onClick={handleRemoveApiKey}
                className="flex items-center gap-1 px-2 py-1 text-xs"
                style={{
                  backgroundColor: colors.alert + '20',
                  color: colors.alert,
                  border: `1px solid ${colors.alert}`,
                  borderRadius: 'var(--ft-border-radius)',
                }}
              >
                <Trash2 size={10} /> {t('settings.remove')}
              </button>
            </div>
          ) : (
            <>
              <div className="flex gap-2 mb-2">
                <div className="flex-1 relative">
                  <input
                    type={showApiKey ? 'text' : 'password'}
                    value={apiKey}
                    onChange={e => {
                      setApiKey(e.target.value);
                      setApiKeyStatus('none');
                    }}
                    placeholder={t('settings.enterApiKey')}
                    style={{
                      ...inputStyle,
                      paddingRight: '36px',
                    }}
                  />
                  <button
                    onClick={() => setShowApiKey(!showApiKey)}
                    className="absolute right-2 top-1/2 transform -translate-y-1/2"
                    style={{ color: colors.textMuted }}
                  >
                    {showApiKey ? <EyeOff size={14} /> : <Eye size={14} />}
                  </button>
                </div>
                <button
                  onClick={handleSaveApiKey}
                  disabled={!apiKey.trim() || apiKeyStatus === 'saving'}
                  className="px-3 text-xs font-bold"
                  style={{
                    backgroundColor: apiKey.trim() ? accentColor : colors.textMuted,
                    color: colors.background,
                    border: 'none',
                    borderRadius: 'var(--ft-border-radius)',
                    opacity: apiKey.trim() ? 1 : 0.5,
                  }}
                >
                  {apiKeyStatus === 'saving' ? t('settings.saving') : t('settings.save')}
                </button>
              </div>
              <div className="text-xs" style={{ color: colors.textMuted }}>
                {t('settings.getApiKeyFrom')}{' '}
                <a
                  href="https://databento.com"
                  target="_blank"
                  rel="noopener noreferrer"
                  style={{ color: colors.info, textDecoration: 'underline' }}
                >
                  databento.com
                </a>
                {' '}→ {t('settings.dashboardApiKeys')}
              </div>
            </>
          )}
        </div>

        {/* Volatility Surface Symbol */}
        <div className="mb-4">
          <label className="block text-xs font-bold mb-2" style={{ color: colors.textMuted }}>
            {t('settings.volatilitySurfaceSymbol')}
          </label>

          {/* Current selected symbol */}
          <div
            className="flex items-center justify-between p-2 mb-2"
            style={{
              backgroundColor: colors.background,
              border: `1px solid ${colors.success}`,
              borderRadius: 'var(--ft-border-radius)',
            }}
          >
            <span className="text-sm font-bold" style={{ color: colors.success }}>
              {localConfig.selectedSymbol}
            </span>
            <CheckCircle size={14} style={{ color: colors.success }} />
          </div>

          {/* Symbol search input */}
          <div className="flex gap-2 mb-2">
            <div className="flex-1 relative">
              <input
                type="text"
                value={symbolSearchQuery}
                onChange={e => setSymbolSearchQuery(e.target.value.toUpperCase())}
                placeholder={t('settings.searchSymbol')}
                style={inputStyle}
                onKeyPress={e => e.key === 'Enter' && handleSymbolSearch()}
              />
            </div>
            <button
              onClick={handleSymbolSearch}
              disabled={!symbolSearchQuery.trim() || symbolSearching}
              className="px-3 flex items-center gap-1"
              style={{
                backgroundColor: symbolSearchQuery.trim() ? accentColor : colors.textMuted,
                color: colors.background,
                border: 'none',
                borderRadius: 'var(--ft-border-radius)',
                opacity: symbolSearchQuery.trim() ? 1 : 0.5,
              }}
            >
              {symbolSearching ? (
                <Loader2 size={14} className="animate-spin" />
              ) : (
                <Search size={14} />
              )}
            </button>
          </div>

          {/* Quick select popular symbols */}
          <div className="flex flex-wrap gap-1">
            {VOL_SURFACE_SYMBOLS.map(sym => (
              <button
                key={sym.value}
                onClick={() => setLocalConfig({ ...localConfig, selectedSymbol: sym.value })}
                className="px-2 py-0.5 text-xs"
                style={{
                  backgroundColor: localConfig.selectedSymbol === sym.value ? accentColor : colors.textMuted,
                  color: localConfig.selectedSymbol === sym.value ? colors.background : textColor,
                  border: 'none',
                  borderRadius: 'var(--ft-border-radius)',
                }}
              >
                {sym.value}
              </button>
            ))}
          </div>
        </div>

        {/* Correlation Assets */}
        <div className="mb-4">
          <label className="block text-xs font-bold mb-2" style={{ color: colors.textMuted }}>
            {t('settings.correlationAssets')} ({localConfig.correlationAssets.length})
          </label>
          <div className="flex gap-2 mb-2">
            <input
              type="text"
              value={newAsset}
              onChange={e => setNewAsset(e.target.value)}
              placeholder={t('settings.addSymbol')}
              style={inputStyle}
              onKeyPress={e => e.key === 'Enter' && handleAddAsset()}
            />
            <button
              onClick={handleAddAsset}
              className="px-3"
              style={{
                backgroundColor: accentColor,
                color: colors.background,
                border: 'none',
              }}
            >
              <Plus size={14} />
            </button>
          </div>
          <div className="flex flex-wrap gap-1 max-h-24 overflow-y-auto">
            {localConfig.correlationAssets.map(asset => (
              <div
                key={asset}
                className="flex items-center gap-1 px-2 py-0.5 text-xs"
                style={{
                  backgroundColor: colors.textMuted,
                  color: textColor,
                }}
              >
                {asset}
                <button
                  onClick={() => handleRemoveAsset(asset)}
                  style={{ color: colors.alert }}
                >
                  <Trash2 size={10} />
                </button>
              </div>
            ))}
          </div>
          <button
            className="mt-2 text-xs underline"
            style={{ color: colors.textMuted }}
            onClick={() =>
              setLocalConfig({
                ...localConfig,
                correlationAssets: [...CORRELATION_ASSETS],
              })
            }
          >
            {t('settings.resetToDefaults')}
          </button>
        </div>

        {/* Date Range */}
        <div className="mb-4">
          <label className="block text-xs font-bold mb-2" style={{ color: colors.textMuted }}>
            {t('settings.historicalDateRange')}
          </label>
          <div className="flex gap-2">
            <div className="flex-1">
              <label className="text-xs" style={{ color: colors.textMuted }}>
                {t('settings.start')}
              </label>
              <input
                type="date"
                value={localConfig.dateRange.start}
                onChange={e =>
                  setLocalConfig({
                    ...localConfig,
                    dateRange: { ...localConfig.dateRange, start: e.target.value },
                  })
                }
                style={inputStyle}
              />
            </div>
            <div className="flex-1">
              <label className="text-xs" style={{ color: colors.textMuted }}>
                {t('settings.end')}
              </label>
              <input
                type="date"
                value={localConfig.dateRange.end}
                onChange={e =>
                  setLocalConfig({
                    ...localConfig,
                    dateRange: { ...localConfig.dateRange, end: e.target.value },
                  })
                }
                style={inputStyle}
              />
            </div>
          </div>
        </div>

        {/* Auto Refresh */}
        <div className="mb-4">
          <label className="block text-xs font-bold mb-2" style={{ color: colors.textMuted }}>
            {t('settings.autoRefreshInterval')}
          </label>
          <select
            value={localConfig.refreshInterval}
            onChange={e =>
              setLocalConfig({
                ...localConfig,
                refreshInterval: parseInt(e.target.value),
              })
            }
            style={inputStyle}
          >
            <option value={0}>{t('settings.manualOnly')}</option>
            <option value={60000}>{t('settings.every1min')}</option>
            <option value={300000}>{t('settings.every5min')}</option>
            <option value={900000}>{t('settings.every15min')}</option>
            <option value={3600000}>{t('settings.every1hour')}</option>
          </select>
        </div>

        {/* Timeframe Selector Toggle */}
        <div className="mb-4">
          <button
            onClick={() => setShowTimeframeSelector(!showTimeframeSelector)}
            className="flex items-center justify-between w-full p-2 text-xs font-bold"
            style={{
              backgroundColor: colors.background,
              border: `1px solid ${colors.textMuted}`,
              borderRadius: 'var(--ft-border-radius)',
              color: accentColor,
            }}
          >
            <div className="flex items-center gap-2">
              <Clock size={14} />
              {t('timeframe.dataTimeframe')}
              <span className="px-1 py-0.5 text-[9px]" style={{
                backgroundColor: colors.textMuted,
                borderRadius: 'var(--ft-border-radius)',
                color: textColor,
              }}>
                {localConfig.timeframe?.toUpperCase().replace('OHLCV-', '') || '1D'}
              </span>
            </div>
            {showTimeframeSelector ? <ChevronUp size={14} /> : <ChevronDown size={14} />}
          </button>
          {showTimeframeSelector && (
            <div className="mt-2">
              <TimeframeSelector
                value={localConfig.timeframe || 'ohlcv-1d'}
                onChange={(tf: OHLCVTimeframe) => setLocalConfig({ ...localConfig, timeframe: tf })}
                accentColor={accentColor}
                textColor={textColor}
              />
            </div>
          )}
        </div>

        {/* Dataset Browser Toggle */}
        <div className="mb-4">
          <button
            onClick={() => setShowDatasetBrowser(!showDatasetBrowser)}
            className="flex items-center justify-between w-full p-2 text-xs font-bold"
            style={{
              backgroundColor: colors.background,
              border: `1px solid ${colors.textMuted}`,
              borderRadius: 'var(--ft-border-radius)',
              color: accentColor,
            }}
          >
            <div className="flex items-center gap-2">
              <Database size={14} />
              {t('settings.browseDatasets')}
              <span className="px-1 py-0.5 text-[9px]" style={{
                backgroundColor: colors.textMuted,
                borderRadius: 'var(--ft-border-radius)',
                color: textColor,
              }}>
                {localConfig.dataset || 'DBEQ.BASIC'}
              </span>
            </div>
            {showDatasetBrowser ? <ChevronUp size={14} /> : <ChevronDown size={14} />}
          </button>
          {showDatasetBrowser && (
            <div className="mt-2">
              <DatasetBrowser
                apiKey={storedApiKey}
                selectedDataset={localConfig.dataset || 'DBEQ.BASIC'}
                onSelectDataset={(ds: string) => setLocalConfig({ ...localConfig, dataset: ds })}
                accentColor={accentColor}
                textColor={textColor}
              />
            </div>
          )}
        </div>

        {/* Cost Estimator Toggle */}
        <div className="mb-4">
          <button
            onClick={() => setShowCostEstimator(!showCostEstimator)}
            className="flex items-center justify-between w-full p-2 text-xs font-bold"
            style={{
              backgroundColor: colors.background,
              border: `1px solid ${colors.textMuted}`,
              borderRadius: 'var(--ft-border-radius)',
              color: accentColor,
            }}
          >
            <div className="flex items-center gap-2">
              <span>$</span>
              {t('settings.costEstimator')}
            </div>
            {showCostEstimator ? <ChevronUp size={14} /> : <ChevronDown size={14} />}
          </button>
          {showCostEstimator && (
            <div className="mt-2">
              <CostEstimator
                apiKey={storedApiKey}
                dataset={localConfig.dataset || 'DBEQ.BASIC'}
                symbols={[localConfig.selectedSymbol, ...localConfig.correlationAssets.slice(0, 3)]}
                schema={localConfig.timeframe || 'ohlcv-1d'}
                startDate={localConfig.dateRange.start}
                endDate={localConfig.dateRange.end}
                accentColor={accentColor}
                textColor={textColor}
              />
            </div>
          )}
        </div>

        {/* Reference Data Toggle */}
        <div className="mb-4">
          <button
            onClick={() => setShowReferenceData(!showReferenceData)}
            className="flex items-center justify-between w-full p-2 text-xs font-bold"
            style={{
              backgroundColor: colors.background,
              border: `1px solid ${colors.textMuted}`,
              borderRadius: 'var(--ft-border-radius)',
              color: accentColor,
            }}
          >
            <div className="flex items-center gap-2">
              <span>#</span>
              {t('settings.referenceData')}
            </div>
            {showReferenceData ? <ChevronUp size={14} /> : <ChevronDown size={14} />}
          </button>
          {showReferenceData && (
            <div className="mt-2">
              <ReferenceDataPanel
                apiKey={storedApiKey}
                symbols={[localConfig.selectedSymbol, ...localConfig.correlationAssets.slice(0, 4)]}
                accentColor={accentColor}
                textColor={textColor}
              />
            </div>
          )}
        </div>

        {/* Order Book Toggle */}
        <div className="mb-4">
          <button
            onClick={() => setShowOrderBook(!showOrderBook)}
            className="flex items-center justify-between w-full p-2 text-xs font-bold"
            style={{
              backgroundColor: colors.background,
              border: `1px solid ${colors.textMuted}`,
              borderRadius: 'var(--ft-border-radius)',
              color: accentColor,
            }}
          >
            <div className="flex items-center gap-2">
              <span>=</span>
              {t('settings.orderBook')}
            </div>
            {showOrderBook ? <ChevronUp size={14} /> : <ChevronDown size={14} />}
          </button>
          {showOrderBook && (
            <div className="mt-2">
              <OrderBookPanel
                apiKey={storedApiKey}
                symbols={[localConfig.selectedSymbol, ...localConfig.correlationAssets.slice(0, 3)]}
                accentColor={accentColor}
                textColor={textColor}
              />
            </div>
          )}
        </div>

        {/* Live Stream Toggle */}
        <div className="mb-4">
          <button
            onClick={() => setShowLiveStream(!showLiveStream)}
            className="flex items-center justify-between w-full p-2 text-xs font-bold"
            style={{
              backgroundColor: colors.background,
              border: `1px solid ${activeStreams.length > 0 ? colors.success : colors.textMuted}`,
              borderRadius: 'var(--ft-border-radius)',
              color: activeStreams.length > 0 ? colors.success : accentColor,
            }}
          >
            <div className="flex items-center gap-2">
              <span>~</span>
              {t('settings.liveStreaming')}
              {activeStreams.length > 0 && (
                <span
                  className="px-1 text-[9px]"
                  style={{
                    backgroundColor: colors.success,
                    color: colors.background,
                    borderRadius: 'var(--ft-border-radius)',
                  }}
                >
                  {activeStreams.length} {t('settings.active')}
                </span>
              )}
            </div>
            {showLiveStream ? <ChevronUp size={14} /> : <ChevronDown size={14} />}
          </button>
          {showLiveStream && (
            <div className="mt-2">
              <LiveStreamPanel
                apiKey={storedApiKey}
                symbols={[localConfig.selectedSymbol, ...localConfig.correlationAssets.slice(0, 3)]}
                accentColor={accentColor}
                textColor={textColor}
                onStartStream={startLiveStream}
                onStopStream={stopLiveStream}
                onStopAllStreams={stopAllLiveStreams}
                activeStreams={activeStreams}
              />
            </div>
          )}
        </div>

        {/* Futures Toggle */}
        <div className="mb-4">
          <button
            onClick={() => setShowFutures(!showFutures)}
            className="flex items-center justify-between w-full p-2 text-xs font-bold"
            style={{
              backgroundColor: colors.background,
              border: `1px solid ${colors.textMuted}`,
              borderRadius: 'var(--ft-border-radius)',
              color: accentColor,
            }}
          >
            <div className="flex items-center gap-2">
              <span>^</span>
              {t('settings.futuresCme')}
            </div>
            {showFutures ? <ChevronUp size={14} /> : <ChevronDown size={14} />}
          </button>
          {showFutures && (
            <div className="mt-2">
              <FuturesPanel
                apiKey={storedApiKey}
                accentColor={accentColor}
                textColor={textColor}
              />
            </div>
          )}
        </div>

        {/* Batch Download Toggle */}
        <div className="mb-4">
          <button
            onClick={() => setShowBatchDownload(!showBatchDownload)}
            className="flex items-center justify-between w-full p-2 text-xs font-bold"
            style={{
              backgroundColor: colors.background,
              border: `1px solid ${colors.textMuted}`,
              borderRadius: 'var(--ft-border-radius)',
              color: accentColor,
            }}
          >
            <div className="flex items-center gap-2">
              <span>*</span>
              {t('settings.batchDownloads')}
            </div>
            {showBatchDownload ? <ChevronUp size={14} /> : <ChevronDown size={14} />}
          </button>
          {showBatchDownload && (
            <div className="mt-2">
              <BatchDownloadPanel
                apiKey={storedApiKey}
                accentColor={accentColor}
                textColor={textColor}
              />
            </div>
          )}
        </div>

        {/* Additional Schemas Toggle */}
        <div className="mb-4">
          <button
            onClick={() => setShowAdditionalSchemas(!showAdditionalSchemas)}
            className="flex items-center justify-between w-full p-2 text-xs font-bold"
            style={{
              backgroundColor: colors.background,
              border: `1px solid ${colors.textMuted}`,
              borderRadius: 'var(--ft-border-radius)',
              color: accentColor,
            }}
          >
            <div className="flex items-center gap-2">
              <span>+</span>
              {t('settings.additionalSchemas')}
              <span className="px-1 py-0.5 text-[8px]" style={{ backgroundColor: colors.textMuted, borderRadius: 'var(--ft-border-radius)', color: colors.textMuted }}>
                {t('settings.tradesImbalanceStatus')}
              </span>
            </div>
            {showAdditionalSchemas ? <ChevronUp size={14} /> : <ChevronDown size={14} />}
          </button>
          {showAdditionalSchemas && (
            <div className="mt-2">
              <AdditionalSchemasPanel
                apiKey={storedApiKey}
                symbols={[localConfig.selectedSymbol, ...localConfig.correlationAssets.slice(0, 3)]}
                accentColor={accentColor}
                textColor={textColor}
              />
            </div>
          )}
        </div>

        {/* Actions */}
        <div className="flex justify-end gap-2 mt-6">
          <button
            onClick={onClose}
            className="px-4 py-1 text-xs font-bold"
            style={{
              backgroundColor: colors.textMuted,
              color: textColor,
              border: `1px solid ${colors.textMuted}`,
            }}
          >
            {t('settings.cancel')}
          </button>
          <button
            onClick={handleSave}
            className="px-4 py-1 text-xs font-bold"
            style={{
              backgroundColor: accentColor,
              color: colors.background,
              border: `1px solid ${accentColor}`,
            }}
          >
            {t('settings.saveChanges')}
          </button>
        </div>
      </div>
    </div>
  );
};
