import React, { memo, useMemo, useCallback } from 'react';
import { RefreshCw, Download, Database, TrendingUp, BarChart3, Activity } from 'lucide-react';
import { useTranslation } from 'react-i18next';

import { useDBnomicsData } from './hooks';
import { SelectionPanel, DBnomicsChart, DataTable } from './components';
import { CHART_TYPES } from './constants';
import type { ChartType } from './types';

// Fincept Design System Colors
const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  BLUE: '#0088FF',
  PURPLE: '#9D4EDD',
};

const FONT_FAMILY = '"IBM Plex Mono", "Consolas", monospace';

// Memoized series tag component
const SeriesTag = memo(({
  series,
  index,
  onRemove,
}: {
  series: { series_name: string; color: string };
  index: number;
  onRemove: (index: number) => void;
}) => (
  <div
    style={{
      display: 'inline-flex',
      alignItems: 'center',
      gap: '4px',
      padding: '3px 6px',
      backgroundColor: FINCEPT.HEADER_BG,
      border: `1px solid ${FINCEPT.BORDER}`,
      borderRadius: '2px',
      marginRight: '4px',
      marginBottom: '4px',
      fontSize: '9px',
      fontFamily: FONT_FAMILY,
    }}
  >
    <div style={{ width: '8px', height: '8px', backgroundColor: series.color, borderRadius: '50%' }} />
    <span style={{ color: FINCEPT.WHITE }}>{series.series_name.substring(0, 20)}</span>
    <button
      onClick={() => onRemove(index)}
      style={{
        padding: '0 4px',
        backgroundColor: 'transparent',
        color: FINCEPT.RED,
        border: 'none',
        cursor: 'pointer',
        fontSize: '10px',
        fontFamily: FONT_FAMILY,
      }}
    >
      ×
    </button>
  </div>
));

SeriesTag.displayName = 'SeriesTag';

// Memoized chart type button
const ChartTypeButton = memo(({
  type,
  isActive,
  onClick,
}: {
  type: ChartType;
  isActive: boolean;
  onClick: () => void;
}) => (
  <button
    onClick={onClick}
    style={{
      padding: '6px 12px',
      backgroundColor: isActive ? FINCEPT.ORANGE : 'transparent',
      color: isActive ? FINCEPT.DARK_BG : FINCEPT.GRAY,
      border: 'none',
      cursor: 'pointer',
      fontSize: '9px',
      fontWeight: 700,
      letterSpacing: '0.5px',
      fontFamily: FONT_FAMILY,
      transition: 'all 0.2s',
      textTransform: 'uppercase',
      borderRadius: '2px',
    }}
  >
    {type}
  </button>
));

ChartTypeButton.displayName = 'ChartTypeButton';

// Memoized comparison slot component
const ComparisonSlot = memo(({
  slot,
  slotIndex,
  chartType,
  onChartTypeChange,
}: {
  slot: any[];
  slotIndex: number;
  chartType: ChartType;
  onChartTypeChange: (type: ChartType) => void;
}) => (
  <div
    style={{
      backgroundColor: FINCEPT.PANEL_BG,
      border: `1px solid ${FINCEPT.BORDER}`,
      borderRadius: '2px',
      minHeight: '280px',
      display: 'flex',
      flexDirection: 'column',
    }}
  >
    <div style={{
      padding: '12px',
      backgroundColor: FINCEPT.HEADER_BG,
      borderBottom: `1px solid ${FINCEPT.BORDER}`,
      display: 'flex',
      justifyContent: 'space-between',
      alignItems: 'center',
    }}>
      <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px', fontFamily: FONT_FAMILY }}>
        SLOT {slotIndex + 1}
      </span>
      <select
        value={chartType}
        onChange={(e) => onChartTypeChange(e.target.value as ChartType)}
        style={{
          padding: '4px 8px',
          backgroundColor: FINCEPT.DARK_BG,
          color: FINCEPT.WHITE,
          border: `1px solid ${FINCEPT.BORDER}`,
          borderRadius: '2px',
          fontSize: '9px',
          fontFamily: FONT_FAMILY,
        }}
      >
        <option value="line">LINE</option>
        <option value="bar">BAR</option>
        <option value="area">AREA</option>
        <option value="scatter">SCATTER</option>
        <option value="candlestick">CANDLE</option>
      </select>
    </div>
    <div style={{ flex: 1, display: 'flex', alignItems: 'center', justifyContent: 'center', minHeight: '240px', padding: '12px' }}>
      {slot.length > 0 ? (
        <DBnomicsChart seriesArray={slot} chartType={chartType} compact />
      ) : (
        <div style={{
          display: 'flex',
          flexDirection: 'column',
          alignItems: 'center',
          justifyContent: 'center',
          color: FINCEPT.MUTED,
          fontSize: '10px',
          textAlign: 'center',
          fontFamily: FONT_FAMILY,
        }}>
          <BarChart3 size={24} style={{ marginBottom: '8px', opacity: 0.5 }} />
          <span>EMPTY SLOT</span>
        </div>
      )}
    </div>
  </div>
));

ComparisonSlot.displayName = 'ComparisonSlot';

function DBnomicsTab() {
  const { t } = useTranslation('dbnomics');

  // Build chart colors from FINCEPT palette
  const chartColors = useMemo(
    () => [FINCEPT.ORANGE, FINCEPT.CYAN, FINCEPT.GREEN, FINCEPT.YELLOW, FINCEPT.PURPLE, FINCEPT.BLUE],
    []
  );

  // Use the data hook
  const {
    providers,
    datasets,
    seriesList,
    currentData,
    globalSearchResults,
    selectedProvider,
    selectedDataset,
    selectedSeries,
    providerSearch,
    datasetSearch,
    seriesSearch,
    globalSearch,
    datasetsPagination,
    seriesPagination,
    searchPagination,
    slots,
    slotChartTypes,
    singleViewSeries,
    singleChartType,
    activeView,
    loading,
    status,
    loadProviders,
    selectProvider,
    selectDataset,
    selectSeries,
    refreshCurrentSeries,
    setProviderSearch,
    setDatasetSearch,
    setSeriesSearch,
    setGlobalSearch,
    selectSearchResult,
    loadMoreDatasets,
    loadMoreSeries,
    loadMoreSearchResults,
    addSlot,
    removeSlot,
    addToSlot,
    removeFromSlot,
    setSlotChartType,
    addToSingleView,
    removeFromSingleView,
    setSingleChartType,
    setActiveView,
    clearAll,
    exportData,
  } = useDBnomicsData(chartColors);

  // Memoized handlers
  const handleViewChange = useCallback((view: 'single' | 'comparison') => {
    setActiveView(view);
  }, [setActiveView]);

  const handleChartTypeChange = useCallback((type: ChartType) => {
    setSingleChartType(type);
  }, [setSingleChartType]);

  return (
    <div style={{
      width: '100%',
      height: '100%',
      backgroundColor: FINCEPT.DARK_BG,
      color: FINCEPT.WHITE,
      fontFamily: FONT_FAMILY,
      fontSize: '11px',
      overflow: 'hidden',
      display: 'flex',
      flexDirection: 'column',
    }}>
      <style>{`
        *::-webkit-scrollbar { width: 6px; height: 6px; }
        *::-webkit-scrollbar-track { background: ${FINCEPT.DARK_BG}; }
        *::-webkit-scrollbar-thumb { background: ${FINCEPT.BORDER}; border-radius: 3px; }
        *::-webkit-scrollbar-thumb:hover { background: ${FINCEPT.MUTED}; }
      `}</style>

      {/* Top Nav Bar */}
      <div style={{
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `2px solid ${FINCEPT.ORANGE}`,
        padding: '8px 16px',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        boxShadow: `0 2px 8px ${FINCEPT.ORANGE}20`,
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          <Database size={14} style={{ color: FINCEPT.ORANGE }} />
          <span style={{ color: FINCEPT.ORANGE, fontWeight: 700, fontSize: '11px', letterSpacing: '0.5px' }}>
            {t('title', 'DBNOMICS ECONOMIC DATABASE')}
          </span>
          <span style={{
            padding: '2px 6px',
            backgroundColor: `${FINCEPT.GREEN}20`,
            color: FINCEPT.GREEN,
            fontSize: '8px',
            fontWeight: 700,
            borderRadius: '2px',
          }}>
            {status}
          </span>
          <span style={{ color: FINCEPT.GRAY, fontSize: '9px', fontWeight: 700, letterSpacing: '0.5px' }}>
            PROVIDERS: <span style={{ color: FINCEPT.CYAN }}>{providers.length}</span>
          </span>
        </div>

        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <button
            onClick={loadProviders}
            disabled={loading}
            style={{
              padding: '6px 10px',
              backgroundColor: 'transparent',
              border: `1px solid ${FINCEPT.BORDER}`,
              color: loading ? FINCEPT.MUTED : FINCEPT.GRAY,
              fontSize: '9px',
              fontWeight: 700,
              borderRadius: '2px',
              cursor: loading ? 'not-allowed' : 'pointer',
              fontFamily: FONT_FAMILY,
              opacity: loading ? 0.5 : 1,
              transition: 'all 0.2s',
            }}
          >
            {t('toolbar.fetch', 'FETCH')}
          </button>
          <button
            onClick={refreshCurrentSeries}
            disabled={!currentData}
            style={{
              padding: '6px 10px',
              backgroundColor: 'transparent',
              border: `1px solid ${FINCEPT.BORDER}`,
              color: !currentData ? FINCEPT.MUTED : FINCEPT.GRAY,
              fontSize: '9px',
              fontWeight: 700,
              borderRadius: '2px',
              cursor: !currentData ? 'not-allowed' : 'pointer',
              fontFamily: FONT_FAMILY,
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
              opacity: !currentData ? 0.5 : 1,
              transition: 'all 0.2s',
            }}
          >
            <RefreshCw size={10} />
            {t('toolbar.refresh', 'REFRESH')}
          </button>
          <button
            onClick={exportData}
            disabled={!currentData}
            style={{
              padding: '6px 10px',
              backgroundColor: !currentData ? 'transparent' : FINCEPT.ORANGE,
              border: !currentData ? `1px solid ${FINCEPT.BORDER}` : 'none',
              color: !currentData ? FINCEPT.MUTED : FINCEPT.DARK_BG,
              fontSize: '9px',
              fontWeight: 700,
              borderRadius: '2px',
              cursor: !currentData ? 'not-allowed' : 'pointer',
              fontFamily: FONT_FAMILY,
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
              opacity: !currentData ? 0.5 : 1,
              transition: 'all 0.2s',
            }}
          >
            <Download size={10} />
            {t('toolbar.export', 'EXPORT')}
          </button>
        </div>
      </div>

      {/* Main Content - Three Panel Layout */}
      <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>
        {/* Left Panel - Selection (280px) */}
        <SelectionPanel
          providers={providers}
          datasets={datasets}
          seriesList={seriesList}
          selectedProvider={selectedProvider}
          selectedDataset={selectedDataset}
          selectedSeries={selectedSeries}
          currentData={currentData}
          slots={slots}
          loading={loading}
          providerSearch={providerSearch}
          datasetSearch={datasetSearch}
          seriesSearch={seriesSearch}
          globalSearch={globalSearch}
          globalSearchResults={globalSearchResults}
          datasetsPagination={datasetsPagination}
          seriesPagination={seriesPagination}
          searchPagination={searchPagination}
          onSelectProvider={selectProvider}
          onSelectDataset={selectDataset}
          onSelectSeries={selectSeries}
          onAddToSingleView={addToSingleView}
          onClearAll={clearAll}
          onAddSlot={addSlot}
          onRemoveSlot={removeSlot}
          onAddToSlot={addToSlot}
          onRemoveFromSlot={removeFromSlot}
          onProviderSearchChange={setProviderSearch}
          onDatasetSearchChange={setDatasetSearch}
          onSeriesSearchChange={setSeriesSearch}
          onGlobalSearchChange={setGlobalSearch}
          onSelectSearchResult={selectSearchResult}
          onLoadMoreDatasets={loadMoreDatasets}
          onLoadMoreSeries={loadMoreSeries}
          onLoadMoreSearchResults={loadMoreSearchResults}
        />

        {/* Center Content (flex: 1) */}
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
          {/* View Toggle - Section Header */}
          <div style={{
            padding: '12px',
            backgroundColor: FINCEPT.HEADER_BG,
            borderBottom: `1px solid ${FINCEPT.BORDER}`,
            display: 'flex',
            gap: '8px',
            alignItems: 'center',
          }}>
            <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
              VIEW
            </span>
            <button
              onClick={() => handleViewChange('single')}
              style={{
                padding: '6px 12px',
                backgroundColor: activeView === 'single' ? FINCEPT.ORANGE : 'transparent',
                color: activeView === 'single' ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                border: 'none',
                fontSize: '9px',
                fontWeight: 700,
                letterSpacing: '0.5px',
                cursor: 'pointer',
                fontFamily: FONT_FAMILY,
                borderRadius: '2px',
                transition: 'all 0.2s',
              }}
            >
              SINGLE
            </button>
            <button
              onClick={() => handleViewChange('comparison')}
              style={{
                padding: '6px 12px',
                backgroundColor: activeView === 'comparison' ? FINCEPT.ORANGE : 'transparent',
                color: activeView === 'comparison' ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                border: 'none',
                fontSize: '9px',
                fontWeight: 700,
                letterSpacing: '0.5px',
                cursor: 'pointer',
                fontFamily: FONT_FAMILY,
                borderRadius: '2px',
                transition: 'all 0.2s',
              }}
            >
              COMPARE
            </button>

            {activeView === 'single' && (
              <>
                <div style={{ width: '1px', height: '16px', backgroundColor: FINCEPT.BORDER, margin: '0 4px' }} />
                <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
                  CHART TYPE
                </span>
                {CHART_TYPES.map((type) => (
                  <ChartTypeButton
                    key={type}
                    type={type}
                    isActive={singleChartType === type}
                    onClick={() => handleChartTypeChange(type)}
                  />
                ))}
              </>
            )}
          </div>

          {/* Content Area */}
          <div style={{ flex: 1, overflowY: 'auto', padding: '12px' }}>
            {activeView === 'single' ? (
              <div>
                {/* Series Tags Header */}
                <div style={{
                  marginBottom: '12px',
                  padding: '12px',
                  backgroundColor: FINCEPT.PANEL_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '2px',
                }}>
                  <div style={{
                    fontSize: '9px',
                    fontWeight: 700,
                    color: FINCEPT.GRAY,
                    letterSpacing: '0.5px',
                    marginBottom: '8px',
                  }}>
                    SINGLE VIEW — <span style={{ color: FINCEPT.CYAN }}>{singleViewSeries.length}</span> SERIES
                  </div>
                  <div>
                    {singleViewSeries.map((s, idx) => (
                      <SeriesTag key={idx} series={s} index={idx} onRemove={removeFromSingleView} />
                    ))}
                    {singleViewSeries.length === 0 && (
                      <span style={{ color: FINCEPT.MUTED, fontSize: '9px' }}>No series added</span>
                    )}
                  </div>
                </div>

                {singleViewSeries.length > 0 ? (
                  <>
                    <div style={{
                      backgroundColor: FINCEPT.PANEL_BG,
                      border: `1px solid ${FINCEPT.BORDER}`,
                      borderRadius: '2px',
                      padding: '12px',
                      marginBottom: '12px',
                    }}>
                      <DBnomicsChart seriesArray={singleViewSeries} chartType={singleChartType} />
                    </div>
                    <DataTable seriesArray={singleViewSeries} />
                  </>
                ) : (
                  <div style={{
                    display: 'flex',
                    flexDirection: 'column',
                    alignItems: 'center',
                    justifyContent: 'center',
                    height: '300px',
                    color: FINCEPT.MUTED,
                    fontSize: '10px',
                    textAlign: 'center',
                    fontFamily: FONT_FAMILY,
                  }}>
                    <TrendingUp size={24} style={{ marginBottom: '8px', opacity: 0.5 }} />
                    <span>SELECT A SERIES AND ADD TO VIEW</span>
                  </div>
                )}
              </div>
            ) : (
              <div>
                <div style={{
                  padding: '12px',
                  backgroundColor: FINCEPT.PANEL_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '2px',
                  marginBottom: '12px',
                }}>
                  <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
                    COMPARISON DASHBOARD
                  </span>
                </div>
                <div
                  style={{
                    display: 'grid',
                    gridTemplateColumns: 'repeat(auto-fit, minmax(380px, 1fr))',
                    gap: '12px',
                  }}
                >
                  {slots.map((slot, idx) => (
                    <ComparisonSlot
                      key={idx}
                      slot={slot}
                      slotIndex={idx}
                      chartType={slotChartTypes[idx]}
                      onChartTypeChange={(type) => setSlotChartType(idx, type)}
                    />
                  ))}
                </div>
              </div>
            )}
          </div>
        </div>
      </div>

      {/* Status Bar (Bottom) */}
      <div style={{
        backgroundColor: FINCEPT.HEADER_BG,
        borderTop: `1px solid ${FINCEPT.BORDER}`,
        padding: '4px 16px',
        fontSize: '9px',
        color: FINCEPT.GRAY,
        fontFamily: FONT_FAMILY,
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
          <span style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
            <Activity size={10} style={{ color: FINCEPT.ORANGE }} />
            DBNOMICS
          </span>
          <span>PROVIDERS: <span style={{ color: FINCEPT.CYAN }}>{providers.length}</span></span>
          <span>DATASETS: <span style={{ color: FINCEPT.CYAN }}>{datasets.length}</span></span>
          <span>SERIES: <span style={{ color: FINCEPT.CYAN }}>{seriesList.length}</span></span>
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          <span>{status}</span>
          <span>DATASET: <span style={{ color: selectedDataset ? FINCEPT.GREEN : FINCEPT.MUTED }}>{selectedDataset ? 'SELECTED' : 'NONE'}</span></span>
        </div>
      </div>
    </div>
  );
}

export default memo(DBnomicsTab);
