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
    finceptTableData,
    finceptCeicCountry,
    setFinceptCeicCountry,
    finceptCeicIndicator,
    setFinceptCeicIndicator,
    finceptCeicIndicatorList,
    finceptCeicIndicatorsLoading,
    fetchFinceptCeicIndicators,
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

  const [finceptCeicSearch, setFinceptCeicSearch] = useState('');

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
          {dataSource === 'fincept' ? (
            /* ── Fincept guided config panel ── */
            <div style={{ display: 'flex', flexDirection: 'column', overflow: 'hidden', flex: 1 }}>

              {/* Step 1: Endpoint selector */}
              <div style={{ borderBottom: `1px solid ${FINCEPT.BORDER}`, padding: '10px 12px 6px' }}>
                <div style={{ fontSize: '9px', color: FINCEPT.ORANGE, fontWeight: 700, letterSpacing: '0.08em', marginBottom: '6px' }}>
                  ENDPOINT
                </div>
                {indicators.map(ind => (
                  <div
                    key={ind.id}
                    onClick={() => { setSelectedIndicator(ind.id); }}
                    style={{
                      padding: '5px 8px',
                      marginBottom: '2px',
                      cursor: 'pointer',
                      borderRadius: '2px',
                      backgroundColor: selectedIndicator === ind.id ? `${FINCEPT.ORANGE}20` : 'transparent',
                      border: selectedIndicator === ind.id ? `1px solid ${FINCEPT.ORANGE}` : '1px solid transparent',
                      fontSize: '10px',
                      color: selectedIndicator === ind.id ? FINCEPT.ORANGE : FINCEPT.WHITE,
                    }}
                  >
                    <div style={{ fontWeight: 600 }}>{ind.name}</div>
                    <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginTop: '1px' }}>{ind.category}</div>
                  </div>
                ))}
              </div>

              {/* Step 2: CEIC country + indicator drill-down (only for CEIC Series endpoints) */}
              {(selectedIndicator === 'ceic_series' || selectedIndicator === 'ceic_series_indicators') && (
                <div style={{ borderBottom: `1px solid ${FINCEPT.BORDER}`, padding: '10px 12px 8px' }}>
                  <div style={{ fontSize: '9px', color: FINCEPT.ORANGE, fontWeight: 700, letterSpacing: '0.08em', marginBottom: '6px' }}>
                    CEIC COUNTRY
                  </div>
                  <select
                    value={finceptCeicCountry}
                    onChange={e => setFinceptCeicCountry(e.target.value)}
                    style={{
                      width: '100%',
                      padding: '5px 8px',
                      backgroundColor: FINCEPT.DARK_BG,
                      color: FINCEPT.WHITE,
                      border: `1px solid ${FINCEPT.BORDER}`,
                      borderRadius: '2px',
                      fontSize: '11px',
                      marginBottom: '6px',
                      outline: 'none',
                    }}
                  >
                    {['china', 'european-union', 'france', 'germany', 'italy', 'japan', 'united-states'].map(c => (
                      <option key={c} value={c} style={{ backgroundColor: FINCEPT.DARK_BG }}>{c}</option>
                    ))}
                  </select>
                  <button
                    onClick={() => fetchFinceptCeicIndicators(finceptCeicCountry)}
                    disabled={finceptCeicIndicatorsLoading}
                    style={{
                      width: '100%',
                      padding: '5px',
                      backgroundColor: FINCEPT.ORANGE,
                      color: FINCEPT.DARK_BG,
                      border: 'none',
                      borderRadius: '2px',
                      fontSize: '10px',
                      fontWeight: 700,
                      cursor: 'pointer',
                      display: 'flex',
                      alignItems: 'center',
                      justifyContent: 'center',
                      gap: '4px',
                    }}
                  >
                    <RefreshCw size={10} className={finceptCeicIndicatorsLoading ? 'animate-spin' : ''} />
                    {finceptCeicIndicatorsLoading ? 'LOADING...' : 'LOAD INDICATORS'}
                  </button>
                </div>
              )}

              {/* Step 3: CEIC indicator picker (for ceic_series only) */}
              {selectedIndicator === 'ceic_series' && finceptCeicIndicatorList.length > 0 && (
                <div style={{ display: 'flex', flexDirection: 'column', flex: 1, overflow: 'hidden', padding: '10px 12px 0' }}>
                  <div style={{ fontSize: '9px', color: FINCEPT.ORANGE, fontWeight: 700, letterSpacing: '0.08em', marginBottom: '6px' }}>
                    INDICATOR ({finceptCeicIndicatorList.length})
                  </div>
                  <input
                    type="text"
                    placeholder="Search indicators..."
                    value={finceptCeicSearch}
                    onChange={e => setFinceptCeicSearch(e.target.value)}
                    style={{
                      padding: '5px 8px',
                      backgroundColor: FINCEPT.DARK_BG,
                      color: FINCEPT.WHITE,
                      border: `1px solid ${FINCEPT.BORDER}`,
                      borderRadius: '2px',
                      fontSize: '10px',
                      marginBottom: '6px',
                      outline: 'none',
                      width: '100%',
                    }}
                  />
                  <div style={{ overflowY: 'auto', flex: 1 }}>
                    {finceptCeicIndicatorList
                      .filter(i => i.indicator.includes(finceptCeicSearch.toLowerCase()))
                      .map(i => (
                        <div
                          key={i.series_id}
                          onClick={() => setFinceptCeicIndicator(i.indicator)}
                          style={{
                            padding: '5px 8px',
                            marginBottom: '2px',
                            cursor: 'pointer',
                            borderRadius: '2px',
                            backgroundColor: finceptCeicIndicator === i.indicator ? `${FINCEPT.ORANGE}20` : 'transparent',
                            border: finceptCeicIndicator === i.indicator ? `1px solid ${FINCEPT.ORANGE}` : '1px solid transparent',
                          }}
                        >
                          <div style={{ fontSize: '10px', color: finceptCeicIndicator === i.indicator ? FINCEPT.ORANGE : FINCEPT.WHITE, fontWeight: 500 }}>
                            {i.indicator.replace(/-/g, ' ')}
                          </div>
                          <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginTop: '1px' }}>
                            {i.data_points} data points
                          </div>
                        </div>
                      ))}
                  </div>
                </div>
              )}
            </div>
          ) : (
            /* ── Standard country + indicator selectors ── */
            <>
              <CountrySelector
                selectedCountry={selectedCountry}
                setSelectedCountry={setSelectedCountry}
                searchCountry={searchCountry}
                setSearchCountry={setSearchCountry}
              />
              <IndicatorSelector
                indicators={indicators}
                selectedIndicator={selectedIndicator}
                setSelectedIndicator={setSelectedIndicator}
                searchIndicator={searchIndicator}
                setSearchIndicator={setSearchIndicator}
                sourceName={currentSourceConfig.name}
                sourceColor={currentSourceConfig.color}
              />
            </>
          )}
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

          {/* Chart / Table area */}
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
            ) : dataSource === 'fincept' && finceptTableData ? (
              <>
                {/* Fincept Table Header */}
                <div style={{ marginBottom: '12px' }}>
                  <div style={{ fontSize: '14px', fontWeight: 700, color: FINCEPT.WHITE }}>{data?.metadata?.indicator_name || selectedIndicator}</div>
                  <div style={{ fontSize: '11px', color: FINCEPT.GRAY }}>
                    Source: <span style={{ color: FINCEPT.ORANGE }}>Fincept Macro API</span>
                    {finceptCeicIndicator && <>&nbsp;•&nbsp;<span style={{ color: FINCEPT.WHITE }}>{finceptCeicIndicator.replace(/-/g, ' ')}</span></>}
                    &nbsp;•&nbsp;{finceptTableData.rows.length} records
                  </div>
                </div>

                {/* Fincept Raw Table */}
                <div style={{ overflowX: 'auto', border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px' }}>
                  <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: '11px', fontFamily: '"IBM Plex Mono", monospace' }}>
                    <thead>
                      <tr style={{ backgroundColor: FINCEPT.HEADER_BG, borderBottom: `2px solid ${FINCEPT.ORANGE}` }}>
                        {finceptTableData.columns.map(col => (
                          <th
                            key={col}
                            style={{
                              padding: '8px 12px',
                              textAlign: 'left',
                              color: FINCEPT.ORANGE,
                              fontWeight: 700,
                              fontSize: '10px',
                              textTransform: 'uppercase',
                              whiteSpace: 'nowrap',
                              letterSpacing: '0.05em',
                            }}
                          >
                            {col.replace(/_/g, ' ')}
                          </th>
                        ))}
                      </tr>
                    </thead>
                    <tbody>
                      {finceptTableData.rows.map((row, i) => (
                        <tr
                          key={i}
                          style={{
                            borderBottom: `1px solid ${FINCEPT.BORDER}`,
                            backgroundColor: i % 2 === 0 ? FINCEPT.DARK_BG : FINCEPT.PANEL_BG,
                          }}
                        >
                          {finceptTableData.columns.map(col => {
                            const val = row[col];
                            const isNum = typeof val === 'number';
                            const isNeg = isNum && val < 0;
                            return (
                              <td
                                key={col}
                                style={{
                                  padding: '7px 12px',
                                  color: isNum ? (isNeg ? FINCEPT.RED : FINCEPT.GREEN) : FINCEPT.WHITE,
                                  whiteSpace: 'nowrap',
                                  maxWidth: '260px',
                                  overflow: 'hidden',
                                  textOverflow: 'ellipsis',
                                }}
                                title={String(val ?? '')}
                              >
                                {isNum ? (Number.isInteger(val) ? val : val.toFixed(2)) : String(val ?? '—')}
                              </td>
                            );
                          })}
                        </tr>
                      ))}
                    </tbody>
                  </table>
                </div>
              </>
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
                  Choose an endpoint from the left panel, then click FETCH DATA to load data from {currentSourceConfig.fullName}.
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
          {dataSource === 'fincept' && <span><span style={{ color: FINCEPT.GRAY }}>API:</span> <span style={{ color: FINCEPT.ORANGE }}>Fincept Macro</span></span>}
          {dataSource === 'fincept' && finceptCeicCountry && (selectedIndicator === 'ceic_series' || selectedIndicator === 'ceic_series_indicators') && <span><span style={{ color: FINCEPT.GRAY }}>CEIC COUNTRY:</span> {finceptCeicCountry}</span>}
          {dataSource === 'fincept' && finceptCeicIndicator && selectedIndicator === 'ceic_series' && <span><span style={{ color: FINCEPT.GRAY }}>SERIES:</span> {finceptCeicIndicator}</span>}
          {finceptTableData && <span><span style={{ color: FINCEPT.GRAY }}>ROWS:</span> {finceptTableData.rows.length}</span>}
          {data && data.data.length > 0 && <span><span style={{ color: FINCEPT.GRAY }}>POINTS:</span> {data.data.length}</span>}
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
