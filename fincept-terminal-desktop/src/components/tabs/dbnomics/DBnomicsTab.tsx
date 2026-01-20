import React, { memo, useMemo, useCallback } from 'react';
import { RefreshCw, Download } from 'lucide-react';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { TabFooter } from '@/components/common/TabFooter';
import { useTranslation } from 'react-i18next';

import { useDBnomicsData } from './hooks';
import { SelectionPanel, DBnomicsChart, DataTable } from './components';
import { CHART_TYPES } from './constants';
import type { ChartType } from './types';

// Memoized series tag component
const SeriesTag = memo(({
  series,
  index,
  onRemove,
  colors,
}: {
  series: { series_name: string; color: string };
  index: number;
  onRemove: (index: number) => void;
  colors: any;
}) => (
  <div
    style={{
      display: 'inline-flex',
      alignItems: 'center',
      gap: '4px',
      padding: '3px 6px',
      backgroundColor: '#1a1a1a',
      border: `1px solid ${colors.textMuted}`,
      borderRadius: '2px',
      marginRight: '4px',
      marginBottom: '4px',
      fontSize: '9px',
    }}
  >
    <div style={{ width: '8px', height: '8px', backgroundColor: series.color, borderRadius: '50%' }} />
    <span>{series.series_name.substring(0, 20)}</span>
    <button
      onClick={() => onRemove(index)}
      style={{
        padding: '0 4px',
        backgroundColor: 'transparent',
        color: colors.alert,
        border: 'none',
        cursor: 'pointer',
        fontSize: '10px',
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
  colors,
}: {
  type: ChartType;
  isActive: boolean;
  onClick: () => void;
  colors: any;
}) => (
  <button
    onClick={onClick}
    style={{
      padding: '3px 8px',
      backgroundColor: isActive ? colors.info : '#2a2a2a',
      color: '#fff',
      border: `1px solid ${colors.textMuted}`,
      cursor: 'pointer',
      fontSize: '9px',
      textTransform: 'uppercase',
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
  colors,
}: {
  slot: any[];
  slotIndex: number;
  chartType: ChartType;
  onChartTypeChange: (type: ChartType) => void;
  colors: any;
}) => (
  <div
    style={{
      border: `1px solid ${colors.textMuted}`,
      padding: '12px',
      backgroundColor: colors.panel,
      minHeight: '280px',
      display: 'flex',
      flexDirection: 'column',
    }}
  >
    <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '8px' }}>
      <div style={{ color: colors.warning, fontSize: '10px' }}>SLOT {slotIndex + 1}</div>
      <select
        value={chartType}
        onChange={(e) => onChartTypeChange(e.target.value as ChartType)}
        style={{
          padding: '2px 4px',
          backgroundColor: '#1a1a1a',
          color: colors.text,
          border: `1px solid ${colors.textMuted}`,
          fontSize: '8px',
          fontFamily: 'Consolas, monospace',
        }}
      >
        <option value="line">LINE</option>
        <option value="bar">BAR</option>
        <option value="area">AREA</option>
        <option value="scatter">SCATTER</option>
        <option value="candlestick">CANDLE</option>
      </select>
    </div>
    <div style={{ flex: 1, display: 'flex', alignItems: 'center', justifyContent: 'center', minHeight: '240px' }}>
      {slot.length > 0 ? (
        <DBnomicsChart seriesArray={slot} chartType={chartType} compact colors={colors} />
      ) : (
        <div style={{ color: colors.textMuted, textAlign: 'center', fontSize: '10px' }}>Empty Slot</div>
      )}
    </div>
  </div>
));

ComparisonSlot.displayName = 'ComparisonSlot';

function DBnomicsTab() {
  const { t } = useTranslation('dbnomics');
  const { colors, fontSize, fontFamily, fontWeight, fontStyle } = useTerminalTheme();

  // Build chart colors from theme
  const chartColors = useMemo(
    () => [colors.primary, colors.info, colors.secondary, colors.warning, colors.purple, colors.accent],
    [colors]
  );

  // Use the data hook
  const {
    providers,
    datasets,
    seriesList,
    currentData,
    selectedProvider,
    selectedDataset,
    selectedSeries,
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

  // Memoized styles
  const containerStyle = useMemo(
    () => ({
      width: '100%',
      height: '100%',
      backgroundColor: colors.background,
      color: colors.text,
      fontFamily: 'Consolas, monospace',
      fontSize: '11px',
      overflow: 'hidden',
      display: 'flex',
      flexDirection: 'column' as const,
    }),
    [colors.background, colors.text]
  );

  const scrollbarStyle = useMemo(
    () => `
    *::-webkit-scrollbar { width: 8px; height: 8px; }
    *::-webkit-scrollbar-track { background: #1a1a1a; }
    *::-webkit-scrollbar-thumb { background: ${colors.textMuted}; border-radius: 4px; }
    *::-webkit-scrollbar-thumb:hover { background: #525252; }
  `,
    [colors.textMuted]
  );

  const headerStyle = useMemo(
    () => ({
      padding: '8px 12px',
      borderBottom: `1px solid ${colors.textMuted}`,
      display: 'flex',
      alignItems: 'center',
      gap: '12px',
      flexWrap: 'wrap' as const,
    }),
    [colors.textMuted]
  );

  return (
    <div style={containerStyle}>
      <style>{scrollbarStyle}</style>

      {/* Header */}
      <div style={headerStyle}>
        <span style={{ color: colors.primary, fontWeight: 'bold' }}>{t('title', 'DBNOMICS ECONOMIC DATABASE')}</span>
        <span style={{ color: colors.secondary, fontSize: '10px' }}>● {status}</span>
        <span style={{ color: colors.warning, fontSize: '10px' }}>Providers: {providers.length}</span>
        <div style={{ flex: 1 }} />
        <button
          onClick={loadProviders}
          disabled={loading}
          style={{
            padding: '3px 10px',
            backgroundColor: '#2a2a2a',
            color: colors.text,
            border: `1px solid ${colors.textMuted}`,
            cursor: loading ? 'not-allowed' : 'pointer',
            fontSize: '9px',
          }}
        >
          {t('toolbar.fetch', 'FETCH')}
        </button>
        <button
          onClick={refreshCurrentSeries}
          disabled={!currentData}
          style={{
            padding: '3px 10px',
            backgroundColor: '#2a2a2a',
            color: colors.text,
            border: `1px solid ${colors.textMuted}`,
            cursor: !currentData ? 'not-allowed' : 'pointer',
            fontSize: '9px',
          }}
        >
          <RefreshCw size={10} style={{ display: 'inline', marginRight: '4px' }} />
          {t('toolbar.refresh', 'REFRESH')}
        </button>
        <button
          onClick={exportData}
          disabled={!currentData}
          style={{
            padding: '3px 10px',
            backgroundColor: '#2a2a2a',
            color: colors.text,
            border: `1px solid ${colors.textMuted}`,
            cursor: !currentData ? 'not-allowed' : 'pointer',
            fontSize: '9px',
          }}
        >
          <Download size={10} style={{ display: 'inline', marginRight: '4px' }} />
          {t('toolbar.export', 'EXPORT')}
        </button>
      </div>

      {/* Main Content */}
      <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>
        {/* Left Panel - Selection */}
        <SelectionPanel
          providers={providers}
          datasets={datasets}
          seriesList={seriesList}
          selectedProvider={selectedProvider}
          selectedDataset={selectedDataset}
          selectedSeries={selectedSeries}
          currentData={currentData}
          slots={slots}
          onSelectProvider={selectProvider}
          onSelectDataset={selectDataset}
          onSelectSeries={selectSeries}
          onAddToSingleView={addToSingleView}
          onClearAll={clearAll}
          onAddSlot={addSlot}
          onRemoveSlot={removeSlot}
          onAddToSlot={addToSlot}
          onRemoveFromSlot={removeFromSlot}
          colors={colors}
        />

        {/* Right Panel - Charts */}
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
          {/* View Toggle */}
          <div
            style={{
              padding: '8px 12px',
              borderBottom: `1px solid ${colors.textMuted}`,
              display: 'flex',
              gap: '8px',
              alignItems: 'center',
            }}
          >
            <button
              onClick={() => handleViewChange('single')}
              style={{
                padding: '4px 12px',
                backgroundColor: activeView === 'single' ? colors.primary : '#2a2a2a',
                color: '#fff',
                border: `1px solid ${colors.textMuted}`,
                cursor: 'pointer',
                fontSize: '10px',
              }}
            >
              SINGLE
            </button>
            <button
              onClick={() => handleViewChange('comparison')}
              style={{
                padding: '4px 12px',
                backgroundColor: activeView === 'comparison' ? colors.primary : '#2a2a2a',
                color: '#fff',
                border: `1px solid ${colors.textMuted}`,
                cursor: 'pointer',
                fontSize: '10px',
              }}
            >
              COMPARE
            </button>

            {activeView === 'single' && (
              <>
                <div style={{ color: colors.textMuted, fontSize: '9px', marginLeft: '8px' }}>Chart Type:</div>
                {CHART_TYPES.map((type) => (
                  <ChartTypeButton
                    key={type}
                    type={type}
                    isActive={singleChartType === type}
                    onClick={() => handleChartTypeChange(type)}
                    colors={colors}
                  />
                ))}
              </>
            )}
          </div>

          {/* Content Area */}
          <div style={{ flex: 1, overflowY: 'auto', padding: '12px' }}>
            {activeView === 'single' ? (
              <div>
                <div style={{ marginBottom: '12px' }}>
                  <div style={{ color: colors.primary, fontSize: '11px', marginBottom: '6px' }}>
                    SINGLE VIEW ({singleViewSeries.length} series)
                  </div>
                  {singleViewSeries.map((s, idx) => (
                    <SeriesTag key={idx} series={s} index={idx} onRemove={removeFromSingleView} colors={colors} />
                  ))}
                </div>

                {singleViewSeries.length > 0 ? (
                  <>
                    <DBnomicsChart seriesArray={singleViewSeries} chartType={singleChartType} colors={colors} />
                    <DataTable seriesArray={singleViewSeries} colors={colors} />
                  </>
                ) : (
                  <div style={{ color: colors.textMuted, textAlign: 'center', paddingTop: '40px' }}>
                    Add series to view
                  </div>
                )}
              </div>
            ) : (
              <div>
                <div style={{ color: colors.primary, marginBottom: '12px', fontSize: '11px' }}>
                  COMPARISON DASHBOARD
                </div>
                <div
                  style={{
                    display: 'grid',
                    gridTemplateColumns: 'repeat(auto-fit, minmax(380px, 1fr))',
                    gap: '16px',
                  }}
                >
                  {slots.map((slot, idx) => (
                    <ComparisonSlot
                      key={idx}
                      slot={slot}
                      slotIndex={idx}
                      chartType={slotChartTypes[idx]}
                      onChartTypeChange={(type) => setSlotChartType(idx, type)}
                      colors={colors}
                    />
                  ))}
                </div>
              </div>
            )}
          </div>
        </div>
      </div>

      <TabFooter
        tabName="DBNOMICS ECONOMIC DATABASE"
        leftInfo={[
          { label: `Providers: ${providers.length}`, color: colors.textMuted },
          { label: `Datasets: ${datasets.length}`, color: colors.textMuted },
          { label: `Series: ${seriesList.length}`, color: colors.textMuted },
        ]}
        statusInfo={`${status} | Dataset: ${selectedDataset ? 'Selected' : 'None'}`}
        backgroundColor={colors.panel}
        borderColor={colors.textMuted}
      />
    </div>
  );
}

export default memo(DBnomicsTab);
