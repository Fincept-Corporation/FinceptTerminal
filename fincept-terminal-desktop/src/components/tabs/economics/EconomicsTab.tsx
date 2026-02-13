// EconomicsTab - Main component for economic data visualization
// Refactored to use modular components, hooks, and constants

import React, { useRef, useState } from 'react';
import { revealItemInDir } from '@tauri-apps/plugin-opener';
import {
  RefreshCw,
  Download,
  Globe,
  AlertCircle,
  Database,
  Image,
  CheckCircle,
  FolderOpen,
  Key,
} from 'lucide-react';

// Local imports
import { FINCEPT, DATA_SOURCES, getIndicatorsForSource } from './constants';
import type { DataSource } from './types';
import { useEconomicsData, useExport } from './hooks';
import {
  EconomicsChart,
  StatsBar,
  DataTable,
  ApiKeyPanel,
  CountrySelector,
  IndicatorSelector,
  DateRangePicker,
} from './components';

// Utility function for formatting values
const formatValue = (val: number): string => {
  if (Math.abs(val) >= 1e12) return `${(val / 1e12).toFixed(2)}T`;
  if (Math.abs(val) >= 1e9) return `${(val / 1e9).toFixed(2)}B`;
  if (Math.abs(val) >= 1e6) return `${(val / 1e6).toFixed(2)}M`;
  if (Math.abs(val) >= 1e3) return `${(val / 1e3).toFixed(2)}K`;
  if (Math.abs(val) < 0.01 && val !== 0) return val.toExponential(2);
  return val.toFixed(2);
};

export default function EconomicsTab() {
  const chartContainerRef = useRef<HTMLDivElement>(null);
  const [searchCountry, setSearchCountry] = useState('');
  const [searchIndicator, setSearchIndicator] = useState('');

  // Use custom hooks
  const {
    dataSource,
    setDataSource,
    selectedCountry,
    setSelectedCountry,
    selectedIndicator,
    setSelectedIndicator,
    data,
    loading,
    error,
    fetchData,
    stats,
    wtoApiKey,
    setWtoApiKey,
    eiaApiKey,
    setEiaApiKey,
    blsApiKey,
    setBlsApiKey,
    beaApiKey,
    setBeaApiKey,
    showApiKeyInput,
    setShowApiKeyInput,
    apiKeySaving,
    saveApiKey,
    // Date range
    dateRangePreset,
    setDateRangePreset,
    startDate,
    endDate,
    setStartDate,
    setEndDate,
  } = useEconomicsData();

  const currentSourceConfig = DATA_SOURCES.find(s => s.id === dataSource)!;
  const indicators = getIndicatorsForSource(dataSource);

  const {
    saveNotification,
    setSaveNotification,
    exportCSV,
    exportImage,
  } = useExport(data, currentSourceConfig, chartContainerRef);

  // Get API key portal URL based on source
  const getApiKeyPortalUrl = (source: DataSource): string => {
    switch (source) {
      case 'wto': return 'https://apiportal.wto.org/';
      case 'eia': return 'https://www.eia.gov/opendata/register.php';
      case 'bls': return 'https://data.bls.gov/registrationEngine/';
      case 'bea': return 'https://apps.bea.gov/API/signup/';
      default: return '';
    }
  };

  // Get current API key value
  const getCurrentApiKey = (): string => {
    switch (dataSource) {
      case 'wto': return wtoApiKey;
      case 'eia': return eiaApiKey;
      case 'bls': return blsApiKey;
      case 'bea': return beaApiKey;
      default: return '';
    }
  };

  // Set current API key
  const setCurrentApiKey = (value: string) => {
    switch (dataSource) {
      case 'wto': setWtoApiKey(value); break;
      case 'eia': setEiaApiKey(value); break;
      case 'bls': setBlsApiKey(value); break;
      case 'bea': setBeaApiKey(value); break;
    }
  };

  // Save current API key
  const handleSaveApiKey = () => {
    const keyName = currentSourceConfig.apiKeyName;
    if (keyName) {
      saveApiKey(keyName, getCurrentApiKey());
    }
  };

  return (
    <div
      style={{
        width: '100%',
        height: '100%',
        backgroundColor: FINCEPT.DARK_BG,
        color: FINCEPT.WHITE,
        fontFamily: '"IBM Plex Mono", monospace',
        display: 'flex',
        flexDirection: 'column',
        overflow: 'hidden',
      }}
    >
      {/* Header */}
      <div
        style={{
          padding: '10px 16px',
          backgroundColor: FINCEPT.HEADER_BG,
          borderBottom: `2px solid ${FINCEPT.ORANGE}`,
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
        }}
      >
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          <Globe size={18} color={FINCEPT.ORANGE} />
          <span style={{ fontSize: '14px', fontWeight: 700, color: FINCEPT.WHITE }}>ECONOMIC DATA</span>

          {/* Data source dropdown selector */}
          <select
            value={dataSource}
            onChange={(e) => setDataSource(e.target.value as DataSource)}
            style={{
              padding: '6px 12px',
              backgroundColor: FINCEPT.DARK_BG,
              color: currentSourceConfig.color,
              border: `1px solid ${currentSourceConfig.color}`,
              borderRadius: '2px',
              fontSize: '11px',
              fontWeight: 600,
              cursor: 'pointer',
              outline: 'none',
              minWidth: '220px',
            }}
          >
            {DATA_SOURCES.map((source) => (
              <option key={source.id} value={source.id} style={{ backgroundColor: FINCEPT.DARK_BG, color: FINCEPT.WHITE }}>
                {source.fullName} {source.requiresApiKey ? '(API Key)' : ''}
              </option>
            ))}
          </select>

          {/* API Key indicator for sources that require it */}
          {currentSourceConfig.requiresApiKey && (
            <button
              onClick={() => setShowApiKeyInput(!showApiKeyInput)}
              title={getCurrentApiKey() ? 'API Key configured - Click to edit' : `Configure ${currentSourceConfig.name} API Key`}
              style={{
                padding: '4px 8px',
                backgroundColor: getCurrentApiKey() ? `${FINCEPT.GREEN}20` : `${FINCEPT.RED}20`,
                color: getCurrentApiKey() ? FINCEPT.GREEN : FINCEPT.RED,
                border: `1px solid ${getCurrentApiKey() ? FINCEPT.GREEN : FINCEPT.RED}`,
                borderRadius: '2px',
                fontSize: '9px',
                fontWeight: 700,
                cursor: 'pointer',
                display: 'flex',
                alignItems: 'center',
                gap: '4px',
              }}
            >
              <Key size={12} />
              {getCurrentApiKey() ? 'API KEY SET' : 'SET API KEY'}
            </button>
          )}

          {/* Date Range Picker */}
          <DateRangePicker
            dateRangePreset={dateRangePreset}
            setDateRangePreset={setDateRangePreset}
            startDate={startDate}
            endDate={endDate}
            setStartDate={setStartDate}
            setEndDate={setEndDate}
            sourceColor={currentSourceConfig.color}
          />
        </div>

        <div style={{ display: 'flex', gap: '8px' }}>
          <button
            onClick={fetchData}
            disabled={loading}
            style={{
              padding: '6px 12px',
              backgroundColor: FINCEPT.ORANGE,
              color: FINCEPT.DARK_BG,
              border: 'none',
              borderRadius: '2px',
              fontSize: '10px',
              fontWeight: 700,
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
            }}
          >
            <RefreshCw size={12} className={loading ? 'animate-spin' : ''} />
            FETCH DATA
          </button>
          <button
            onClick={exportCSV}
            disabled={!data || data.data.length === 0}
            style={{
              padding: '6px 12px',
              backgroundColor: 'transparent',
              color: FINCEPT.WHITE,
              border: `1px solid ${FINCEPT.BORDER}`,
              borderRadius: '2px',
              fontSize: '10px',
              fontWeight: 700,
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
              opacity: data && data.data.length > 0 ? 1 : 0.5,
            }}
          >
            <Download size={12} />
            CSV
          </button>
          <button
            onClick={exportImage}
            disabled={!data || data.data.length === 0}
            style={{
              padding: '6px 12px',
              backgroundColor: 'transparent',
              color: FINCEPT.WHITE,
              border: `1px solid ${FINCEPT.BORDER}`,
              borderRadius: '2px',
              fontSize: '10px',
              fontWeight: 700,
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
              opacity: data && data.data.length > 0 ? 1 : 0.5,
            }}
          >
            <Image size={12} />
            IMAGE
          </button>
        </div>
      </div>

      {/* Save Success Notification */}
      {saveNotification?.show && (
        <div
          style={{
            padding: '12px 16px',
            backgroundColor: `${FINCEPT.GREEN}15`,
            borderBottom: `1px solid ${FINCEPT.GREEN}`,
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'space-between',
            gap: '12px',
          }}
        >
          <div style={{ display: 'flex', alignItems: 'center', gap: '10px' }}>
            <CheckCircle size={18} color={FINCEPT.GREEN} />
            <div>
              <div style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.WHITE }}>
                {saveNotification.type === 'image' ? 'Image' : 'CSV'} saved successfully!
              </div>
              <div style={{ fontSize: '10px', color: FINCEPT.GRAY, marginTop: '2px' }}>
                {saveNotification.path}
              </div>
            </div>
          </div>
          <div style={{ display: 'flex', gap: '8px' }}>
            <button
              onClick={async () => {
                try {
                  await revealItemInDir(saveNotification.path);
                } catch (err) {
                  console.error('Failed to open path:', err);
                }
              }}
              style={{
                padding: '6px 12px',
                backgroundColor: FINCEPT.GREEN,
                color: FINCEPT.DARK_BG,
                border: 'none',
                borderRadius: '2px',
                fontSize: '9px',
                fontWeight: 700,
                cursor: 'pointer',
                display: 'flex',
                alignItems: 'center',
                gap: '6px',
              }}
            >
              <FolderOpen size={12} />
              OPEN FILE
            </button>
            <button
              onClick={() => setSaveNotification(null)}
              style={{
                padding: '6px 12px',
                backgroundColor: 'transparent',
                color: FINCEPT.GRAY,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '2px',
                fontSize: '9px',
                fontWeight: 700,
                cursor: 'pointer',
              }}
            >
              DISMISS
            </button>
          </div>
        </div>
      )}

      {/* API Key Configuration Panel */}
      {showApiKeyInput && currentSourceConfig.requiresApiKey && (
        <ApiKeyPanel
          apiKey={getCurrentApiKey()}
          setApiKey={setCurrentApiKey}
          onSave={handleSaveApiKey}
          onClose={() => setShowApiKeyInput(false)}
          saving={apiKeySaving}
          sourceName={currentSourceConfig.name}
          sourceColor={currentSourceConfig.color}
          portalUrl={getApiKeyPortalUrl(dataSource)}
        />
      )}

      {/* Main content */}
      <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>
        {/* Left Panel - Selectors */}
        <div
          style={{
            width: '280px',
            backgroundColor: FINCEPT.PANEL_BG,
            borderRight: `1px solid ${FINCEPT.BORDER}`,
            display: 'flex',
            flexDirection: 'column',
            overflow: 'hidden',
          }}
        >
          {/* Country selector */}
          <CountrySelector
            selectedCountry={selectedCountry}
            setSelectedCountry={setSelectedCountry}
            searchCountry={searchCountry}
            setSearchCountry={setSearchCountry}
          />

          {/* Indicator selector */}
          <IndicatorSelector
            indicators={indicators}
            selectedIndicator={selectedIndicator}
            setSelectedIndicator={setSelectedIndicator}
            searchIndicator={searchIndicator}
            setSearchIndicator={setSearchIndicator}
            sourceName={currentSourceConfig.name}
            sourceColor={currentSourceConfig.color}
          />
        </div>

        {/* Right Panel - Chart and Data */}
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
          {/* Stats bar */}
          {stats && (
            <StatsBar
              stats={stats}
              sourceColor={currentSourceConfig.color}
              formatValue={formatValue}
            />
          )}

          {/* Chart area */}
          <div style={{ flex: 1, padding: '16px', overflowY: 'auto' }}>
            {loading ? (
              <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', height: '100%' }}>
                <RefreshCw size={32} className="animate-spin" style={{ color: currentSourceConfig.color, marginBottom: '16px' }} />
                <div style={{ fontSize: '12px', color: FINCEPT.WHITE }}>Loading data from {currentSourceConfig.fullName}...</div>
              </div>
            ) : error ? (
              <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', height: '100%' }}>
                <AlertCircle size={32} style={{ color: FINCEPT.RED, marginBottom: '16px' }} />
                <div style={{ fontSize: '12px', color: FINCEPT.RED, marginBottom: '8px' }}>Error loading data</div>
                <div style={{ fontSize: '10px', color: FINCEPT.GRAY, textAlign: 'center', maxWidth: '400px' }}>{error}</div>
                <button
                  onClick={fetchData}
                  style={{
                    marginTop: '16px',
                    padding: '8px 16px',
                    backgroundColor: FINCEPT.ORANGE,
                    color: FINCEPT.DARK_BG,
                    border: 'none',
                    borderRadius: '2px',
                    fontSize: '10px',
                    fontWeight: 700,
                    cursor: 'pointer',
                  }}
                >
                  TRY AGAIN
                </button>
              </div>
            ) : data && data.data.length > 0 ? (
              <>
                {/* Title */}
                <div style={{ marginBottom: '16px' }}>
                  <div style={{ fontSize: '14px', fontWeight: 700, color: FINCEPT.WHITE }}>{data.metadata?.indicator_name || data.indicator}</div>
                  <div style={{ fontSize: '11px', color: FINCEPT.GRAY }}>
                    {data.metadata?.country_name || data.country} • Source: <span style={{ color: currentSourceConfig.color }}>{currentSourceConfig.fullName}</span>
                  </div>
                </div>

                {/* Chart */}
                <div ref={chartContainerRef} style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', padding: '16px', marginBottom: '16px' }}>
                  <EconomicsChart
                    data={data.data}
                    sourceColor={currentSourceConfig.color}
                    formatValue={formatValue}
                  />
                </div>

                {/* Data table */}
                <DataTable
                  data={data.data}
                  sourceColor={currentSourceConfig.color}
                  formatValue={formatValue}
                />
              </>
            ) : (
              <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', height: '100%' }}>
                <Database size={48} style={{ color: FINCEPT.MUTED, marginBottom: '16px', opacity: 0.5 }} />
                <div style={{ fontSize: '12px', color: FINCEPT.WHITE, marginBottom: '8px' }}>Select data and click FETCH DATA</div>
                <div style={{ fontSize: '10px', color: FINCEPT.GRAY, textAlign: 'center', maxWidth: '300px' }}>
                  Choose a country and indicator from the left panel, then click the FETCH DATA button to load economic data from {currentSourceConfig.fullName}.
                </div>
              </div>
            )}
          </div>
        </div>
      </div>

      {/* Status bar */}
      <div
        style={{
          padding: '6px 16px',
          backgroundColor: FINCEPT.HEADER_BG,
          borderTop: `1px solid ${FINCEPT.BORDER}`,
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center',
          fontSize: '9px',
        }}
      >
        <div style={{ display: 'flex', gap: '16px', color: FINCEPT.WHITE, flexWrap: 'wrap' }}>
          <span title={currentSourceConfig.fullName}><span style={{ color: FINCEPT.GRAY }}>SOURCE:</span> <span style={{ color: currentSourceConfig.color }}>{currentSourceConfig.name}</span></span>
          <span><span style={{ color: FINCEPT.GRAY }}>COUNTRY:</span> {selectedCountry}</span>
          <span><span style={{ color: FINCEPT.GRAY }}>INDICATOR:</span> {selectedIndicator}</span>
          {data && <span><span style={{ color: FINCEPT.GRAY }}>POINTS:</span> {data.data.length}</span>}
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
          <span style={{ width: '6px', height: '6px', borderRadius: '50%', backgroundColor: loading ? FINCEPT.YELLOW : error ? FINCEPT.RED : FINCEPT.GREEN }} />
          <span style={{ color: loading ? FINCEPT.YELLOW : error ? FINCEPT.RED : FINCEPT.GREEN }}>
            {loading ? 'Loading...' : error ? 'Error' : 'Ready'}
          </span>
        </div>
      </div>
    </div>
  );
}
