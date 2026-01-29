/**
 * Surface Analytics - Settings Modal Component
 * Configuration for symbols, date ranges, data sources, and API credentials
 */

import React, { useState, useEffect, useCallback } from 'react';
import { X, Plus, Trash2, Key, Eye, EyeOff, AlertCircle, CheckCircle, Search, Loader2, Database, ChevronDown, ChevronUp, Clock } from 'lucide-react';
import { invoke } from '@tauri-apps/api/core';
import type { SurfaceAnalyticsConfig, OHLCVTimeframe } from '../types';
import { VOL_SURFACE_SYMBOLS, CORRELATION_ASSETS, FINCEPT_COLORS, TYPOGRAPHY } from '../constants';
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
    backgroundColor: FINCEPT_COLORS.BLACK,
    color: textColor,
    border: `1px solid ${FINCEPT_COLORS.BORDER}`,
    borderRadius: '2px',
    fontSize: '11px',
    fontFamily: '"IBM Plex Mono", monospace',
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
          backgroundColor: FINCEPT_COLORS.PANEL_BG,
          border: `1px solid ${FINCEPT_COLORS.BORDER}`,
        }}
        onClick={e => e.stopPropagation()}
      >
        {/* Header */}
        <div className="flex items-center justify-between mb-4">
          <h2 className="text-sm font-bold" style={{ color: accentColor }}>
            SURFACE ANALYTICS SETTINGS
          </h2>
          <button onClick={onClose} style={{ color: FINCEPT_COLORS.MUTED }}>
            <X size={16} />
          </button>
        </div>

        {/* Databento API Key Section */}
        <div
          className="mb-4 p-3"
          style={{
            backgroundColor: FINCEPT_COLORS.DARK_BG,
            border: `1px solid ${apiKeyStatus === 'valid' ? FINCEPT_COLORS.GREEN : apiKeyStatus === 'invalid' ? FINCEPT_COLORS.RED : FINCEPT_COLORS.BORDER}`,
            borderRadius: '2px',
          }}
        >
          <div className="flex items-center gap-2 mb-2">
            <Key size={14} style={{ color: accentColor }} />
            <label className="text-xs font-bold" style={{ color: accentColor, letterSpacing: '0.5px' }}>
              DATABENTO API KEY
            </label>
            {apiKeyStatus === 'valid' && (
              <span className="flex items-center gap-1 text-xs" style={{ color: FINCEPT_COLORS.GREEN }}>
                <CheckCircle size={12} /> CONFIGURED
              </span>
            )}
            {apiKeyStatus === 'invalid' && (
              <span className="flex items-center gap-1 text-xs" style={{ color: FINCEPT_COLORS.RED }}>
                <AlertCircle size={12} /> INVALID FORMAT
              </span>
            )}
          </div>

          {hasExistingKey ? (
            <div className="flex items-center justify-between">
              <span className="text-xs" style={{ color: FINCEPT_COLORS.MUTED }}>
                API key is configured (db-*****)
              </span>
              <button
                onClick={handleRemoveApiKey}
                className="flex items-center gap-1 px-2 py-1 text-xs"
                style={{
                  backgroundColor: FINCEPT_COLORS.RED + '20',
                  color: FINCEPT_COLORS.RED,
                  border: `1px solid ${FINCEPT_COLORS.RED}`,
                  borderRadius: '2px',
                }}
              >
                <Trash2 size={10} /> REMOVE
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
                    placeholder="Enter your Databento API key (db-*****)"
                    style={{
                      ...inputStyle,
                      paddingRight: '36px',
                    }}
                  />
                  <button
                    onClick={() => setShowApiKey(!showApiKey)}
                    className="absolute right-2 top-1/2 transform -translate-y-1/2"
                    style={{ color: FINCEPT_COLORS.MUTED }}
                  >
                    {showApiKey ? <EyeOff size={14} /> : <Eye size={14} />}
                  </button>
                </div>
                <button
                  onClick={handleSaveApiKey}
                  disabled={!apiKey.trim() || apiKeyStatus === 'saving'}
                  className="px-3 text-xs font-bold"
                  style={{
                    backgroundColor: apiKey.trim() ? accentColor : FINCEPT_COLORS.MUTED,
                    color: FINCEPT_COLORS.BLACK,
                    border: 'none',
                    borderRadius: '2px',
                    opacity: apiKey.trim() ? 1 : 0.5,
                  }}
                >
                  {apiKeyStatus === 'saving' ? 'SAVING...' : 'SAVE'}
                </button>
              </div>
              <div className="text-xs" style={{ color: FINCEPT_COLORS.MUTED }}>
                Get your API key from{' '}
                <a
                  href="https://databento.com"
                  target="_blank"
                  rel="noopener noreferrer"
                  style={{ color: FINCEPT_COLORS.CYAN, textDecoration: 'underline' }}
                >
                  databento.com
                </a>
                {' '}→ Dashboard → API Keys
              </div>
            </>
          )}
        </div>

        {/* Volatility Surface Symbol */}
        <div className="mb-4">
          <label className="block text-xs font-bold mb-2" style={{ color: FINCEPT_COLORS.MUTED }}>
            VOLATILITY SURFACE SYMBOL
          </label>

          {/* Current selected symbol */}
          <div
            className="flex items-center justify-between p-2 mb-2"
            style={{
              backgroundColor: FINCEPT_COLORS.BLACK,
              border: `1px solid ${FINCEPT_COLORS.GREEN}`,
              borderRadius: '2px',
            }}
          >
            <span className="text-sm font-bold" style={{ color: FINCEPT_COLORS.GREEN }}>
              {localConfig.selectedSymbol}
            </span>
            <CheckCircle size={14} style={{ color: FINCEPT_COLORS.GREEN }} />
          </div>

          {/* Symbol search input */}
          <div className="flex gap-2 mb-2">
            <div className="flex-1 relative">
              <input
                type="text"
                value={symbolSearchQuery}
                onChange={e => setSymbolSearchQuery(e.target.value.toUpperCase())}
                placeholder="Search any symbol (e.g., MSFT, GOOGL, META)"
                style={inputStyle}
                onKeyPress={e => e.key === 'Enter' && handleSymbolSearch()}
              />
            </div>
            <button
              onClick={handleSymbolSearch}
              disabled={!symbolSearchQuery.trim() || symbolSearching}
              className="px-3 flex items-center gap-1"
              style={{
                backgroundColor: symbolSearchQuery.trim() ? accentColor : FINCEPT_COLORS.MUTED,
                color: FINCEPT_COLORS.BLACK,
                border: 'none',
                borderRadius: '2px',
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
                  backgroundColor: localConfig.selectedSymbol === sym.value ? accentColor : FINCEPT_COLORS.BORDER,
                  color: localConfig.selectedSymbol === sym.value ? FINCEPT_COLORS.BLACK : textColor,
                  border: 'none',
                  borderRadius: '2px',
                }}
              >
                {sym.value}
              </button>
            ))}
          </div>
        </div>

        {/* Correlation Assets */}
        <div className="mb-4">
          <label className="block text-xs font-bold mb-2" style={{ color: FINCEPT_COLORS.MUTED }}>
            CORRELATION ASSETS ({localConfig.correlationAssets.length})
          </label>
          <div className="flex gap-2 mb-2">
            <input
              type="text"
              value={newAsset}
              onChange={e => setNewAsset(e.target.value)}
              placeholder="Add symbol (e.g., AAPL)"
              style={inputStyle}
              onKeyPress={e => e.key === 'Enter' && handleAddAsset()}
            />
            <button
              onClick={handleAddAsset}
              className="px-3"
              style={{
                backgroundColor: accentColor,
                color: FINCEPT_COLORS.BLACK,
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
                  backgroundColor: FINCEPT_COLORS.BORDER,
                  color: textColor,
                }}
              >
                {asset}
                <button
                  onClick={() => handleRemoveAsset(asset)}
                  style={{ color: FINCEPT_COLORS.RED }}
                >
                  <Trash2 size={10} />
                </button>
              </div>
            ))}
          </div>
          <button
            className="mt-2 text-xs underline"
            style={{ color: FINCEPT_COLORS.MUTED }}
            onClick={() =>
              setLocalConfig({
                ...localConfig,
                correlationAssets: [...CORRELATION_ASSETS],
              })
            }
          >
            Reset to defaults
          </button>
        </div>

        {/* Date Range */}
        <div className="mb-4">
          <label className="block text-xs font-bold mb-2" style={{ color: FINCEPT_COLORS.MUTED }}>
            HISTORICAL DATE RANGE
          </label>
          <div className="flex gap-2">
            <div className="flex-1">
              <label className="text-xs" style={{ color: FINCEPT_COLORS.MUTED }}>
                Start
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
              <label className="text-xs" style={{ color: FINCEPT_COLORS.MUTED }}>
                End
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
          <label className="block text-xs font-bold mb-2" style={{ color: FINCEPT_COLORS.MUTED }}>
            AUTO REFRESH INTERVAL
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
            <option value={0}>Manual Only</option>
            <option value={60000}>Every 1 minute</option>
            <option value={300000}>Every 5 minutes</option>
            <option value={900000}>Every 15 minutes</option>
            <option value={3600000}>Every 1 hour</option>
          </select>
        </div>

        {/* Timeframe Selector Toggle */}
        <div className="mb-4">
          <button
            onClick={() => setShowTimeframeSelector(!showTimeframeSelector)}
            className="flex items-center justify-between w-full p-2 text-xs font-bold"
            style={{
              backgroundColor: FINCEPT_COLORS.DARK_BG,
              border: `1px solid ${FINCEPT_COLORS.BORDER}`,
              borderRadius: '2px',
              color: accentColor,
            }}
          >
            <div className="flex items-center gap-2">
              <Clock size={14} />
              DATA TIMEFRAME
              <span className="px-1 py-0.5 text-[9px]" style={{
                backgroundColor: FINCEPT_COLORS.BORDER,
                borderRadius: '2px',
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
              backgroundColor: FINCEPT_COLORS.DARK_BG,
              border: `1px solid ${FINCEPT_COLORS.BORDER}`,
              borderRadius: '2px',
              color: accentColor,
            }}
          >
            <div className="flex items-center gap-2">
              <Database size={14} />
              BROWSE DATASETS
              <span className="px-1 py-0.5 text-[9px]" style={{
                backgroundColor: FINCEPT_COLORS.BORDER,
                borderRadius: '2px',
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
              backgroundColor: FINCEPT_COLORS.DARK_BG,
              border: `1px solid ${FINCEPT_COLORS.BORDER}`,
              borderRadius: '2px',
              color: accentColor,
            }}
          >
            <div className="flex items-center gap-2">
              <span>💰</span>
              COST ESTIMATOR
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
              backgroundColor: FINCEPT_COLORS.DARK_BG,
              border: `1px solid ${FINCEPT_COLORS.BORDER}`,
              borderRadius: '2px',
              color: accentColor,
            }}
          >
            <div className="flex items-center gap-2">
              <span>📋</span>
              REFERENCE DATA
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
              backgroundColor: FINCEPT_COLORS.DARK_BG,
              border: `1px solid ${FINCEPT_COLORS.BORDER}`,
              borderRadius: '2px',
              color: accentColor,
            }}
          >
            <div className="flex items-center gap-2">
              <span>📊</span>
              ORDER BOOK
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
              backgroundColor: FINCEPT_COLORS.DARK_BG,
              border: `1px solid ${activeStreams.length > 0 ? FINCEPT_COLORS.GREEN : FINCEPT_COLORS.BORDER}`,
              borderRadius: '2px',
              color: activeStreams.length > 0 ? FINCEPT_COLORS.GREEN : accentColor,
            }}
          >
            <div className="flex items-center gap-2">
              <span>📡</span>
              LIVE STREAMING
              {activeStreams.length > 0 && (
                <span
                  className="px-1 text-[9px]"
                  style={{
                    backgroundColor: FINCEPT_COLORS.GREEN,
                    color: FINCEPT_COLORS.BLACK,
                    borderRadius: '2px',
                  }}
                >
                  {activeStreams.length} ACTIVE
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
              backgroundColor: FINCEPT_COLORS.DARK_BG,
              border: `1px solid ${FINCEPT_COLORS.BORDER}`,
              borderRadius: '2px',
              color: accentColor,
            }}
          >
            <div className="flex items-center gap-2">
              <span>📈</span>
              FUTURES (CME GLOBEX)
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
              backgroundColor: FINCEPT_COLORS.DARK_BG,
              border: `1px solid ${FINCEPT_COLORS.BORDER}`,
              borderRadius: '2px',
              color: accentColor,
            }}
          >
            <div className="flex items-center gap-2">
              <span>📦</span>
              BATCH DOWNLOADS
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
              backgroundColor: FINCEPT_COLORS.DARK_BG,
              border: `1px solid ${FINCEPT_COLORS.BORDER}`,
              borderRadius: '2px',
              color: accentColor,
            }}
          >
            <div className="flex items-center gap-2">
              <span>📊</span>
              ADDITIONAL SCHEMAS
              <span className="px-1 py-0.5 text-[8px]" style={{ backgroundColor: FINCEPT_COLORS.BORDER, borderRadius: '2px', color: FINCEPT_COLORS.MUTED }}>
                TRADES, IMBALANCE, STATUS
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
              backgroundColor: FINCEPT_COLORS.BORDER,
              color: textColor,
              border: `1px solid ${FINCEPT_COLORS.MUTED}`,
            }}
          >
            CANCEL
          </button>
          <button
            onClick={handleSave}
            className="px-4 py-1 text-xs font-bold"
            style={{
              backgroundColor: accentColor,
              color: FINCEPT_COLORS.BLACK,
              border: `1px solid ${accentColor}`,
            }}
          >
            SAVE CHANGES
          </button>
        </div>
      </div>
    </div>
  );
};
