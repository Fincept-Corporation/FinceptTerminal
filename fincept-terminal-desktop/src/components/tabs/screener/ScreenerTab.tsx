import React, { useState, useEffect, useMemo, useCallback, memo } from 'react';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { useTranslation } from 'react-i18next';
import { Search, Download, BarChart3, Loader2, AlertCircle, FolderTree } from 'lucide-react';
import { TabFooter } from '@/components/common/TabFooter';

import { useScreenerData } from './hooks';
import { SeriesBrowserModal, SeriesChart, SeriesDataTable } from './components';
import { POPULAR_SERIES } from './constants';

// Memoized quick-add button component
const QuickAddButton = memo(({
  seriesId,
  isSelected,
  onClick,
  colors,
  fontSize
}: {
  seriesId: string;
  isSelected: boolean;
  onClick: () => void;
  colors: any;
  fontSize: any;
}) => (
  <button
    onClick={onClick}
    style={{
      background: isSelected ? `${colors.success}22` : colors.background,
      border: `1px solid ${isSelected ? colors.success : colors.textMuted}44`,
      color: isSelected ? colors.success : colors.textMuted,
      padding: '4px 10px',
      fontSize: fontSize.tiny,
      cursor: 'pointer',
      borderRadius: '3px',
      fontWeight: isSelected ? 'bold' : 'normal',
      transition: 'all 0.15s'
    }}
  >
    {seriesId}
  </button>
));

QuickAddButton.displayName = 'QuickAddButton';

function ScreenerTab() {
  const { colors, fontSize, fontFamily, fontWeight, fontStyle } = useTerminalTheme();
  const { t } = useTranslation('screener');

  // Form state
  const [seriesIds, setSeriesIds] = useState('GDP');
  const [startDate, setStartDate] = useState('2000-01-01');
  const [endDate, setEndDate] = useState(() => new Date().toISOString().split('T')[0]);
  const [chartType, setChartType] = useState<'line' | 'area'>('line');
  const [normalizeData, setNormalizeData] = useState(false);
  const [showBrowser, setShowBrowser] = useState(false);

  // Data hook
  const {
    data,
    searchResults,
    categories,
    categorySeriesResults,
    loading,
    loadingMessage,
    searchLoading,
    categoryLoading,
    categorySeriesLoading,
    error,
    apiKeyConfigured,
    fetchData,
    searchSeries,
    loadCategories,
    loadCategorySeries,
    checkApiKey,
  } = useScreenerData();

  // Check API key on mount
  useEffect(() => {
    checkApiKey();
  }, [checkApiKey]);

  // Delayed auto-fetch after API key check
  useEffect(() => {
    if (apiKeyConfigured && data.length === 0 && !loading) {
      const timer = setTimeout(() => {
        fetchData(seriesIds, startDate, endDate);
      }, 500);
      return () => clearTimeout(timer);
    }
  }, [apiKeyConfigured]); // Only run when apiKeyConfigured changes

  // Memoize current series IDs set
  const currentSeriesIdsSet = useMemo(() => {
    return new Set(seriesIds.split(',').map(s => s.trim()).filter(Boolean));
  }, [seriesIds]);

  // Handlers
  const handleFetchData = useCallback(() => {
    fetchData(seriesIds, startDate, endDate);
  }, [fetchData, seriesIds, startDate, endDate]);

  const handleSeriesIdsChange = useCallback((e: React.ChangeEvent<HTMLInputElement>) => {
    setSeriesIds(e.target.value);
  }, []);

  const handleStartDateChange = useCallback((e: React.ChangeEvent<HTMLInputElement>) => {
    setStartDate(e.target.value);
  }, []);

  const handleEndDateChange = useCallback((e: React.ChangeEvent<HTMLInputElement>) => {
    setEndDate(e.target.value);
  }, []);

  const toggleChartType = useCallback(() => {
    setChartType(prev => prev === 'line' ? 'area' : 'line');
  }, []);

  const toggleNormalize = useCallback(() => {
    setNormalizeData(prev => !prev);
  }, []);

  const handleAddPopularSeries = useCallback((id: string) => {
    setSeriesIds(prev => {
      const currentIds = prev.split(',').map(s => s.trim()).filter(Boolean);
      if (currentIds.includes(id)) {
        return currentIds.filter(sid => sid !== id).join(',');
      }
      return prev ? `${prev},${id}` : id;
    });
  }, []);

  const handleAddSeriesFromBrowser = useCallback((id: string) => {
    if (!currentSeriesIdsSet.has(id)) {
      const newIds = seriesIds ? `${seriesIds},${id}` : id;
      setSeriesIds(newIds);
      // Auto-fetch after adding
      setTimeout(() => {
        fetchData(newIds, startDate, endDate);
      }, 100);
    }
  }, [currentSeriesIdsSet, seriesIds, startDate, endDate, fetchData]);

  const handleOpenBrowser = useCallback(() => {
    setShowBrowser(true);
    if (categories.length === 0) {
      loadCategories(0);
    }
  }, [categories.length, loadCategories]);

  const handleCloseBrowser = useCallback(() => {
    setShowBrowser(false);
  }, []);

  // Export to CSV
  const exportToCSV = useCallback(() => {
    if (data.length === 0) return;

    const allDates = new Set<string>();
    data.forEach(series => {
      series.observations.forEach(obs => allDates.add(obs.date));
    });
    const sortedDates = Array.from(allDates).sort();

    const headers = ['Date', ...data.map(s => s.series_id)].join(',');
    const rows = sortedDates.map(date => {
      const values = data.map(series => {
        const obs = series.observations.find(o => o.date === date);
        return obs?.value || '';
      });
      return [date, ...values].join(',');
    });

    const csv = [headers, ...rows].join('\n');
    const blob = new Blob([csv], { type: 'text/csv' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `fred_data_${new Date().toISOString().split('T')[0]}.csv`;
    a.click();
    URL.revokeObjectURL(url);
  }, [data]);

  // Memoize container style
  const containerStyle = useMemo(() => ({
    width: '100%',
    height: '100%',
    display: 'flex',
    flexDirection: 'column' as const,
    backgroundColor: colors.background,
    color: colors.text,
    fontFamily,
    fontWeight,
    fontStyle,
    overflow: 'hidden',
    fontSize: fontSize.body
  }), [colors.background, colors.text, fontFamily, fontWeight, fontStyle, fontSize.body]);

  // Memoize scrollbar style
  const scrollbarStyle = useMemo(() => `
    *::-webkit-scrollbar { width: 6px; height: 6px; }
    *::-webkit-scrollbar-track { background: ${colors.background}; }
    *::-webkit-scrollbar-thumb { background: ${colors.panel}; border-radius: 3px; }
    *::-webkit-scrollbar-thumb:hover { background: ${colors.textMuted}; }
  `, [colors.background, colors.panel, colors.textMuted]);

  // Total observations count
  const totalObservations = useMemo(() => {
    return data.reduce((acc, s) => acc + s.observation_count, 0);
  }, [data]);

  return (
    <div style={containerStyle}>
      <style>{scrollbarStyle}</style>

      {/* Series Browser Modal */}
      <SeriesBrowserModal
        isOpen={showBrowser}
        onClose={handleCloseBrowser}
        onAddSeries={handleAddSeriesFromBrowser}
        currentSeriesIds={currentSeriesIdsSet}
        searchResults={searchResults}
        searchLoading={searchLoading}
        categories={categories}
        categoryLoading={categoryLoading}
        categorySeriesResults={categorySeriesResults}
        categorySeriesLoading={categorySeriesLoading}
        onSearch={searchSeries}
        onLoadCategories={loadCategories}
        onLoadCategorySeries={loadCategorySeries}
        colors={colors}
        fontSize={fontSize}
      />

      {/* Header */}
      <div style={{
        borderBottom: `1px solid ${colors.primary}`,
        padding: '10px 16px',
        background: colors.panel,
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
        flexShrink: 0
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '10px' }}>
          <BarChart3 size={16} color={colors.primary} />
          <span style={{ color: colors.primary, fontSize: fontSize.heading, fontWeight: 'bold', letterSpacing: '0.5px' }}>
            {t('header.title', 'ECONOMIC DATA SCREENER')}
          </span>
        </div>
        <div style={{ display: 'flex', gap: '6px' }}>
          <button
            onClick={toggleChartType}
            style={{
              background: colors.background,
              border: `1px solid ${colors.primary}33`,
              color: colors.primary,
              padding: '6px 12px',
              fontSize: fontSize.small,
              cursor: 'pointer',
              borderRadius: '3px',
              fontWeight: 'bold',
              transition: 'all 0.15s'
            }}
          >
            {chartType === 'line' ? 'AREA' : 'LINE'}
          </button>
          <button
            onClick={toggleNormalize}
            disabled={data.length <= 1}
            style={{
              background: normalizeData ? colors.primary : colors.background,
              border: `1px solid ${colors.primary}33`,
              color: normalizeData ? colors.background : colors.primary,
              padding: '6px 12px',
              fontSize: fontSize.small,
              cursor: data.length > 1 ? 'pointer' : 'not-allowed',
              borderRadius: '3px',
              fontWeight: 'bold',
              opacity: data.length > 1 ? 1 : 0.5,
              transition: 'all 0.15s'
            }}
          >
            NORMALIZE
          </button>
          <button
            onClick={exportToCSV}
            disabled={data.length === 0}
            style={{
              background: data.length > 0 ? colors.background : colors.panel,
              border: `1px solid ${data.length > 0 ? colors.success : colors.textMuted}33`,
              color: data.length > 0 ? colors.success : colors.textMuted,
              padding: '6px 12px',
              fontSize: fontSize.small,
              cursor: data.length > 0 ? 'pointer' : 'not-allowed',
              borderRadius: '3px',
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
              fontWeight: 'bold',
              opacity: data.length > 0 ? 1 : 0.5,
              transition: 'opacity 0.15s'
            }}
          >
            <Download size={12} />
            CSV
          </button>
        </div>
      </div>

      {/* API Key Warning */}
      {!apiKeyConfigured && (
        <div style={{
          background: `${colors.warning}11`,
          border: `1px solid ${colors.warning}`,
          padding: '12px 16px',
          display: 'flex',
          alignItems: 'flex-start',
          gap: '12px',
          flexShrink: 0
        }}>
          <AlertCircle size={20} color={colors.warning} style={{ flexShrink: 0, marginTop: '2px' }} />
          <div style={{ flex: 1 }}>
            <p style={{ color: colors.warning, fontSize: fontSize.body, fontWeight: 'bold', marginBottom: '6px' }}>
              {t('apiKey.required', 'FRED API Key Required')}
            </p>
            <p style={{ color: colors.text, fontSize: fontSize.small, lineHeight: '1.5', marginBottom: '8px' }}>
              Configure your FRED API key in <strong>Settings → Credentials</strong> to access economic data.
            </p>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
              <p style={{ color: colors.textMuted, fontSize: fontSize.tiny }}>
                <strong style={{ color: colors.warning }}>1.</strong> Get free API key at{' '}
                <a
                  href="https://research.stlouisfed.org/useraccount/apikey"
                  target="_blank"
                  rel="noopener noreferrer"
                  style={{ color: colors.info, textDecoration: 'underline' }}
                >
                  research.stlouisfed.org
                </a>
              </p>
              <p style={{ color: colors.textMuted, fontSize: fontSize.tiny }}>
                <strong style={{ color: colors.warning }}>2.</strong> Add to <strong>Settings → Credentials → FRED_API_KEY</strong>
              </p>
              <p style={{ color: colors.textMuted, fontSize: fontSize.tiny }}>
                <strong style={{ color: colors.warning }}>3.</strong> Save and reload this tab
              </p>
            </div>
          </div>
        </div>
      )}

      {/* Query Panel */}
      <div style={{
        borderBottom: `1px solid ${colors.panel}`,
        padding: '12px 16px',
        background: colors.panel,
        flexShrink: 0
      }}>
        <div style={{ display: 'grid', gridTemplateColumns: '2fr 1fr 1fr auto', gap: '10px', marginBottom: '10px' }}>
          <div>
            <label style={{ color: colors.textMuted, fontSize: fontSize.tiny, display: 'block', marginBottom: '4px', fontWeight: 'bold' }}>
              {t('form.seriesIds', 'SERIES IDS (comma-separated)')}
            </label>
            <input
              type="text"
              value={seriesIds}
              onChange={handleSeriesIdsChange}
              placeholder="GDP,UNRATE,CPIAUCSL"
              style={{
                width: '100%',
                background: colors.background,
                border: `1px solid ${colors.textMuted}33`,
                color: colors.text,
                padding: '8px 10px',
                fontSize: fontSize.small,
                borderRadius: '3px',
                outline: 'none'
              }}
            />
          </div>
          <div>
            <label style={{ color: colors.textMuted, fontSize: fontSize.tiny, display: 'block', marginBottom: '4px', fontWeight: 'bold' }}>
              START DATE
            </label>
            <input
              type="date"
              value={startDate}
              onChange={handleStartDateChange}
              style={{
                width: '100%',
                background: colors.background,
                border: `1px solid ${colors.textMuted}33`,
                color: colors.text,
                padding: '8px 10px',
                fontSize: fontSize.small,
                borderRadius: '3px',
                outline: 'none'
              }}
            />
          </div>
          <div>
            <label style={{ color: colors.textMuted, fontSize: fontSize.tiny, display: 'block', marginBottom: '4px', fontWeight: 'bold' }}>
              END DATE
            </label>
            <input
              type="date"
              value={endDate}
              onChange={handleEndDateChange}
              style={{
                width: '100%',
                background: colors.background,
                border: `1px solid ${colors.textMuted}33`,
                color: colors.text,
                padding: '8px 10px',
                fontSize: fontSize.small,
                borderRadius: '3px',
                outline: 'none'
              }}
            />
          </div>
          <button
            onClick={handleFetchData}
            disabled={loading}
            style={{
              background: loading ? colors.panel : colors.primary,
              border: 'none',
              color: colors.background,
              padding: '0 18px',
              fontSize: fontSize.small,
              cursor: loading ? 'not-allowed' : 'pointer',
              borderRadius: '3px',
              fontWeight: 'bold',
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
              alignSelf: 'end',
              opacity: loading ? 0.6 : 1,
              transition: 'all 0.15s'
            }}
          >
            {loading ? <Loader2 size={12} className="animate-spin" /> : <Search size={12} />}
            {loading ? t('buttons.loading', 'LOADING...') : t('buttons.fetch', 'FETCH')}
          </button>
        </div>

        {/* Popular Series Quick Add & Browse Button */}
        <div style={{ display: 'flex', alignItems: 'flex-end', gap: '10px' }}>
          <div style={{ flex: 1 }}>
            <label style={{ color: colors.textMuted, fontSize: fontSize.tiny, marginBottom: '6px', display: 'block', fontWeight: 'bold' }}>
              {t('form.quickAdd', 'QUICK ADD')}
            </label>
            <div style={{ display: 'flex', gap: '4px', flexWrap: 'wrap' }}>
              {POPULAR_SERIES.map(series => (
                <QuickAddButton
                  key={series.id}
                  seriesId={series.id}
                  isSelected={currentSeriesIdsSet.has(series.id)}
                  onClick={() => handleAddPopularSeries(series.id)}
                  colors={colors}
                  fontSize={fontSize}
                />
              ))}
            </div>
          </div>
          <button
            onClick={handleOpenBrowser}
            disabled={!apiKeyConfigured}
            style={{
              background: apiKeyConfigured ? colors.primary : colors.panel,
              border: `1px solid ${apiKeyConfigured ? colors.primary : colors.textMuted}44`,
              color: apiKeyConfigured ? colors.background : colors.textMuted,
              padding: '8px 14px',
              fontSize: fontSize.small,
              cursor: apiKeyConfigured ? 'pointer' : 'not-allowed',
              borderRadius: '3px',
              fontWeight: 'bold',
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
              whiteSpace: 'nowrap',
              opacity: apiKeyConfigured ? 1 : 0.5,
              transition: 'opacity 0.15s'
            }}
          >
            <FolderTree size={12} />
            {t('buttons.browse', 'BROWSE SERIES')}
          </button>
        </div>
      </div>

      {/* Main Content */}
      <div style={{ flex: 1, display: 'flex', gap: '12px', padding: '12px 16px', overflow: 'hidden', minHeight: 0 }}>
        {/* Chart Section */}
        <div style={{ flex: 2, display: 'flex', flexDirection: 'column', minWidth: 0 }}>
          <div style={{
            background: colors.panel,
            border: `1px solid ${colors.primary}33`,
            borderRadius: '4px',
            padding: '14px',
            height: '100%',
            display: 'flex',
            flexDirection: 'column'
          }}>
            <h3 style={{ color: colors.primary, fontSize: fontSize.body, fontWeight: 'bold', marginBottom: '10px', letterSpacing: '0.5px' }}>
              {t('chart.title', 'TIME SERIES CHART')}
            </h3>
            <SeriesChart
              data={data}
              chartType={chartType}
              normalizeData={normalizeData}
              loading={loading}
              loadingMessage={loadingMessage}
              error={error}
              apiKeyConfigured={apiKeyConfigured}
              colors={colors}
              fontSize={fontSize}
            />
          </div>
        </div>

        {/* Data Tables */}
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', minWidth: 0, overflowY: 'auto', gap: '10px' }}>
          {data.map(series => (
            <SeriesDataTable
              key={series.series_id}
              series={series}
              colors={colors}
              fontSize={fontSize}
            />
          ))}
        </div>
      </div>

      {/* Footer */}
      <TabFooter
        tabName="ECONOMIC SCREENER"
        leftInfo={[
          { label: 'FRED Economic Data', color: colors.accent },
          { label: 'Federal Reserve Bank of St. Louis', color: colors.textMuted },
        ]}
        statusInfo={
          data.length > 0 && (
            <span style={{ color: colors.primary, fontWeight: 'bold' }}>
              {data.length} series | {totalObservations} observations
            </span>
          )
        }
        backgroundColor={colors.panel}
        borderColor={colors.primary}
      />
    </div>
  );
}

export default memo(ScreenerTab);
