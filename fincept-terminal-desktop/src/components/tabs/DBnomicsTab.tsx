import React, { useState } from 'react';
import { RefreshCw, Download, Plus, Minus, Trash2 } from 'lucide-react';
import { useTerminalTheme } from '@/contexts/ThemeContext';

interface Provider {
  code: string;
  name: string;
}

interface Dataset {
  code: string;
  name: string;
}

interface Series {
  code: string;
  name: string;
  full_id: string;
}

interface Observation {
  period: string;
  value: number;
}

interface DataPoint {
  series_id: string;
  series_name: string;
  observations: Observation[];
  color: string;
}

type ChartType = 'line' | 'bar' | 'area' | 'scatter' | 'candlestick';

export default function DBnomicsTab() {
  const { colors, fontSize, fontFamily, fontWeight, fontStyle } = useTerminalTheme();
  const BASE_URL = 'https://api.db.nomics.world/v22';
  const COLORS = [colors.primary, colors.info, colors.secondary, colors.warning, colors.purple, colors.purple, colors.accent, colors.primary];

  const [providers, setProviders] = useState<Provider[]>([]);
  const [datasets, setDatasets] = useState<Dataset[]>([]);
  const [seriesList, setSeriesList] = useState<Series[]>([]);
  const [currentData, setCurrentData] = useState<DataPoint | null>(null);

  const [selectedProvider, setSelectedProvider] = useState<string | null>(null);
  const [selectedDataset, setSelectedDataset] = useState<string | null>(null);
  const [selectedSeries, setSelectedSeries] = useState<string | null>(null);

  const [status, setStatus] = useState('Loading providers...');
  const [loading, setLoading] = useState(false);

  const [slots, setSlots] = useState<DataPoint[][]>([[]]); // Array of slot arrays (each slot can have multiple series)
  const [slotChartTypes, setSlotChartTypes] = useState<ChartType[]>(['line']);
  const [activeView, setActiveView] = useState<'single' | 'comparison'>('single');
  const [singleViewSeries, setSingleViewSeries] = useState<DataPoint[]>([]);
  const [singleChartType, setSingleChartType] = useState<ChartType>('line');

  // Load providers and restore cached state on mount
  React.useEffect(() => {
    loadProviders();
    restoreState();
  }, []);

  // Save state to localStorage whenever important data changes
  React.useEffect(() => {
    saveState();
  }, [singleViewSeries, slots, slotChartTypes, singleChartType, activeView]);

  const saveState = () => {
    try {
      const state = {
        singleViewSeries,
        slots,
        slotChartTypes,
        singleChartType,
        activeView,
        timestamp: Date.now()
      };
      localStorage.setItem('dbnomics_state', JSON.stringify(state));
    } catch (error) {
      console.error('Failed to save DBnomics state:', error);
    }
  };

  const restoreState = () => {
    try {
      const saved = localStorage.getItem('dbnomics_state');
      if (saved) {
        const state = JSON.parse(saved);
        // Only restore if saved within last 24 hours
        if (state.timestamp && Date.now() - state.timestamp < 24 * 60 * 60 * 1000) {
          if (state.singleViewSeries) setSingleViewSeries(state.singleViewSeries);
          if (state.slots) setSlots(state.slots);
          if (state.slotChartTypes) setSlotChartTypes(state.slotChartTypes);
          if (state.singleChartType) setSingleChartType(state.singleChartType);
          if (state.activeView) setActiveView(state.activeView);
          setStatus('Restored previous session');
        }
      }
    } catch (error) {
      console.error('Failed to restore DBnomics state:', error);
    }
  };

  // API Functions
  const loadProviders = async () => {
    try {
      setLoading(true);
      setStatus('Loading providers...');
      const response = await fetch(`${BASE_URL}/providers`);
      if (response.ok) {
        const data = await response.json();
        const providerList: Provider[] = (data?.providers?.docs || []).map((p: any) => ({
          code: p.code || '',
          name: p.name || p.code || ''
        }));
        setProviders(providerList);
        setStatus(`Loaded ${providerList.length} providers`);
      } else {
        setStatus(`Error: ${response.status}`);
      }
    } catch (error) {
      setStatus(`Error: ${error}`);
    } finally {
      setLoading(false);
    }
  };

  const loadDatasets = async (providerCode: string) => {
    try {
      setLoading(true);
      setStatus(`Loading datasets...`);
      const response = await fetch(`${BASE_URL}/datasets/${providerCode}`);
      if (response.ok) {
        const data = await response.json();
        const datasetList: Dataset[] = (data?.datasets?.docs || []).map((d: any) => ({
          code: d.code || '',
          name: d.name || d.code || ''
        }));
        setDatasets(datasetList);
        setSeriesList([]);
        setStatus(`Loaded ${datasetList.length} datasets`);
      } else {
        setStatus(`Error: ${response.status}`);
      }
    } catch (error) {
      setStatus(`Error: ${error}`);
    } finally {
      setLoading(false);
    }
  };

  const loadSeries = async (providerCode: string, datasetCode: string) => {
    try {
      setLoading(true);
      setStatus(`Loading series...`);
      const response = await fetch(`${BASE_URL}/series/${providerCode}/${datasetCode}?limit=50&observations=false`);
      if (response.ok) {
        const data = await response.json();
        const seriesArray: Series[] = (data?.series?.docs || []).map((s: any) => ({
          code: s.series_code || '',
          name: s.series_name || s.series_code || '',
          full_id: `${providerCode}/${datasetCode}/${s.series_code}`
        }));
        setSeriesList(seriesArray);
        setStatus(`Loaded ${seriesArray.length} series`);
      } else {
        setStatus(`Error: ${response.status}`);
      }
    } catch (error) {
      setStatus(`Error: ${error}`);
    } finally {
      setLoading(false);
    }
  };

  const loadSeriesData = async (fullSeriesId: string, seriesName: string) => {
    try {
      setLoading(true);
      setStatus(`Loading data...`);
      const [providerCode, datasetCode, seriesCode] = fullSeriesId.split('/');
      const response = await fetch(`${BASE_URL}/series/${providerCode}/${datasetCode}/${seriesCode}?observations=1&format=json`);
      if (response.ok) {
        const data = await response.json();
        const seriesDocs = data?.series?.docs || [];
        if (seriesDocs.length > 0) {
          const firstSeries = seriesDocs[0];
          let observations: Observation[] = [];
          if (firstSeries.period && firstSeries.value) {
            for (let i = 0; i < Math.min(firstSeries.period.length, firstSeries.value.length); i++) {
              if (firstSeries.value[i] !== null) {
                observations.push({ period: firstSeries.period[i], value: firstSeries.value[i] });
              }
            }
          }
          if (observations.length > 0) {
            const colorIndex = (singleViewSeries.length + slots.flat().length) % COLORS.length;
            setCurrentData({ series_id: fullSeriesId, series_name: seriesName, observations, color: COLORS[colorIndex] });
            setStatus(`Loaded ${observations.length} points`);
          } else {
            setStatus('No data found');
          }
        } else {
          setStatus('No data found');
        }
      } else {
        setStatus(`Error: ${response.status}`);
      }
    } catch (error) {
      setStatus(`Error: ${error}`);
    } finally {
      setLoading(false);
    }
  };

  const handleProviderSelect = (providerCode: string) => {
    setSelectedProvider(providerCode);
    setSelectedDataset(null);
    setSelectedSeries(null);
    setDatasets([]);
    setSeriesList([]);
    loadDatasets(providerCode);
  };

  const handleDatasetSelect = (datasetCode: string) => {
    setSelectedDataset(datasetCode);
    setSelectedSeries(null);
    setSeriesList([]);
    if (selectedProvider) loadSeries(selectedProvider, datasetCode);
  };

  const handleSeriesSelect = (series: Series) => {
    setSelectedSeries(series.full_id);
    loadSeriesData(series.full_id, series.name);
  };

  // Slot Management
  const addSlot = () => {
    setSlots([...slots, []]);
    setSlotChartTypes([...slotChartTypes, 'line']);
    setStatus(`Added slot ${slots.length + 1}`);
  };

  const removeSlot = (index: number) => {
    if (slots.length > 1) {
      setSlots(slots.filter((_, i) => i !== index));
      setSlotChartTypes(slotChartTypes.filter((_, i) => i !== index));
      setStatus(`Removed slot ${index + 1}`);
    }
  };

  const addToSlot = (slotIndex: number) => {
    if (currentData && slotIndex >= 0 && slotIndex < slots.length) {
      const newSlots = [...slots];
      newSlots[slotIndex] = [...newSlots[slotIndex], currentData];
      setSlots(newSlots);
      setStatus(`Added to slot ${slotIndex + 1}`);
    }
  };

  const removeFromSlot = (slotIndex: number, seriesIndex: number) => {
    const newSlots = [...slots];
    newSlots[slotIndex] = newSlots[slotIndex].filter((_, i) => i !== seriesIndex);
    setSlots(newSlots);
    setStatus(`Removed from slot ${slotIndex + 1}`);
  };

  const addToSingleView = () => {
    if (currentData) {
      setSingleViewSeries([...singleViewSeries, currentData]);
      setActiveView('single'); // Switch to single view
      setStatus('Added to single view');
    }
  };

  const removeFromSingleView = (index: number) => {
    setSingleViewSeries(singleViewSeries.filter((_, i) => i !== index));
  };

  const clearAllSlots = () => {
    setSlots([[]]);
    setSlotChartTypes(['line']);
    setSingleViewSeries([]);
    localStorage.removeItem('dbnomics_state'); // Clear cache
    setStatus('Cleared all');
  };

  const exportData = () => {
    if (!currentData) return;
    const csv = ['Period,Value', ...currentData.observations.map(obs => `${obs.period},${obs.value}`)].join('\n');
    const blob = new Blob([csv], { type: 'text/csv' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `dbnomics_${Date.now()}.csv`;
    a.click();
    URL.revokeObjectURL(url);
    setStatus('Exported');
  };

  // Chart Rendering
  const renderChart = (seriesArray: DataPoint[], chartType: ChartType, compact = false) => {
    if (seriesArray.length === 0) return null;

    const w = compact ? '100%' : 800;
    const h: number = compact ? 240 : 400;
    const ml = 60;
    const mr = 20;
    const mt = 20;
    const mb = 50;

    // For compact view, calculate width dynamically
    const actualWidth: number = compact ? 380 : 800;
    const pw = actualWidth - ml - mr;
    const ph = h - mt - mb;

    // Aggregate all observations
    const allValues = seriesArray.flatMap(s => s.observations.map(o => o.value));
    const yMin = Math.min(...allValues) * 0.9;
    const yMax = Math.max(...allValues) * 1.1;
    const yRange = yMax - yMin;

    return (
      <svg width={w} height={h} viewBox={`0 0 ${actualWidth} ${h}`} preserveAspectRatio="xMidYMid meet" style={{ backgroundColor: colors.panel, width: '100%', height: 'auto', maxHeight: compact ? '240px' : '400px' }}>
        {/* Axes */}
        <line x1={ml} y1={mt} x2={ml} y2={h - mb} stroke={colors.textMuted} strokeWidth="1" />
        <line x1={ml} y1={h - mb} x2={actualWidth - mr} y2={h - mb} stroke={colors.textMuted} strokeWidth="1" />

        {/* Y-axis grid and labels */}
        {[0, 1, 2, 3, 4].map(i => {
          const val = yMin + (yRange * i) / 4;
          const y = h - mb - (ph * i) / 4;
          return (
            <g key={i}>
              <line x1={ml} y1={y} x2={actualWidth - mr} y2={y} stroke="#1a1a1a" strokeDasharray="2,2" />
              <text x={ml - 8} y={y + 3} textAnchor="end" fill={colors.textMuted} fontSize="9" fontFamily="monospace">
                {val.toFixed(1)}
              </text>
            </g>
          );
        })}

        {/* Render each series */}
        {seriesArray.map((series, sIdx) => {
          const sorted = [...series.observations].sort((a, b) => new Date(a.period).getTime() - new Date(b.period).getTime());

          return (
            <g key={sIdx}>
              {sorted.map((obs, idx) => {
                const x = ml + (pw * idx) / (sorted.length - 1 || 1);
                const y = h - mb - (ph * ((Number(obs.value) - yMin) / yRange));
                const barWidth = pw / sorted.length * 0.8;

                if (chartType === 'line' || chartType === 'area') {
                  return (
                    <g key={idx}>
                      {chartType === 'area' && idx === 0 && (
                        <polygon
                          points={(() => {
                            const points = sorted.map((o, i) => {
                              const xi = ml + (pw * i) / (sorted.length - 1 || 1);
                              const oValue = typeof o.value === 'number' ? o.value : Number(o.value);
                              const yi = h - mb - (ph * ((oValue - yMin) / yRange));
                              return `${xi},${yi}`;
                            }).join(' ');
                            return `${points} ${actualWidth - mr},${h - mb} ${ml},${h - mb}`;
                          })()}
                          fill={series.color}
                          opacity="0.2"
                        />
                      )}
                      <circle cx={x} cy={y} r="2" fill={series.color} />
                      {idx < sorted.length - 1 && (
                        <line
                          x1={x}
                          y1={y}
                          x2={ml + (pw * (idx + 1)) / (sorted.length - 1 || 1)}
                          y2={h - mb - (ph * ((Number(sorted[idx + 1].value) - yMin) / yRange))}
                          stroke={series.color}
                          strokeWidth="1.5"
                        />
                      )}
                    </g>
                  );
                } else if (chartType === 'bar') {
                  return (
                    <rect
                      key={idx}
                      x={x - barWidth / 2 + (sIdx * barWidth / seriesArray.length)}
                      y={y}
                      width={barWidth / seriesArray.length}
                      height={h - mb - y}
                      fill={series.color}
                      opacity="0.8"
                    />
                  );
                } else if (chartType === 'scatter') {
                  return <circle key={idx} cx={x} cy={y} r="3" fill={series.color} opacity="0.7" />;
                } else if (chartType === 'candlestick') {
                  const nextObs = sorted[idx + 1];
                  const open = obs.value;
                  const close = nextObs ? nextObs.value : obs.value;
                  const high = Math.max(open, close);
                  const low = Math.min(open, close);
                  const yHigh = h - mb - (ph * ((high - yMin) / yRange));
                  const yLow = h - mb - (ph * ((low - yMin) / yRange));
                  const yOpen = h - mb - (ph * ((open - yMin) / yRange));
                  const yClose = h - mb - (ph * ((close - yMin) / yRange));

                  return (
                    <g key={idx}>
                      <line x1={x} y1={yHigh} x2={x} y2={yLow} stroke={series.color} strokeWidth="1" />
                      <rect
                        x={x - 3}
                        y={Math.min(yOpen, yClose)}
                        width={6}
                        height={Math.abs(yClose - yOpen) || 1}
                        fill={close >= open ? colors.secondary : colors.alert}
                      />
                    </g>
                  );
                }
                return null;
              })}
            </g>
          );
        })}

        {/* X-axis labels */}
        {seriesArray[0] && seriesArray[0].observations.slice().sort((a, b) => new Date(a.period).getTime() - new Date(b.period).getTime()).map((obs, idx, arr) => {
          if (idx % Math.max(1, Math.floor(arr.length / 6)) !== 0 && idx !== arr.length - 1) return null;
          const x = ml + (pw * idx) / (arr.length - 1 || 1);
          return (
            <text key={idx} x={x} y={h - mb + 15} textAnchor="middle" fill={colors.textMuted} fontSize="8" fontFamily="monospace">
              {obs.period.substring(0, 7)}
            </text>
          );
        })}

        {/* Legend */}
        {seriesArray.map((series, idx) => (
          <g key={idx}>
            <rect x={ml + idx * 120} y={5} width={10} height={10} fill={series.color} />
            <text x={ml + idx * 120 + 15} y={13} fill={colors.text} fontSize="9" fontFamily="monospace">
              {series.series_name.substring(0, 12)}
            </text>
          </g>
        ))}
      </svg>
    );
  };

  return (
    <div style={{ width: '100%', height: '100%', backgroundColor: colors.background, color: colors.text, fontFamily: 'Consolas, monospace', fontSize: '11px', overflow: 'hidden', display: 'flex', flexDirection: 'column' }}>
      <style>{`
        /* Custom scrollbar styles */
        *::-webkit-scrollbar {
          width: 8px;
          height: 8px;
        }
        *::-webkit-scrollbar-track {
          background: #1a1a1a;
        }
        *::-webkit-scrollbar-thumb {
          background: colors.textMuted;
          border-radius: 4px;
        }
        *::-webkit-scrollbar-thumb:hover {
          background: #525252;
        }
      `}</style>
      {/* Header */}
      <div style={{ padding: '8px 12px', borderBottom: '1px solid colors.textMuted', display: 'flex', alignItems: 'center', gap: '12px', flexWrap: 'wrap' }}>
        <span style={{ color: colors.primary, fontWeight: 'bold' }}>DBNOMICS TERMINAL</span>
        <span style={{ color: colors.secondary, fontSize: '10px' }}>● {status}</span>
        <span style={{ color: colors.warning, fontSize: '10px' }}>Providers: {providers.length}</span>
        <div style={{ flex: 1 }}></div>
        <button onClick={loadProviders} disabled={loading} style={{ padding: '3px 10px', backgroundColor: '#2a2a2a', color: colors.text, border: '1px solid colors.textMuted', cursor: loading ? 'not-allowed' : 'pointer', fontSize: '9px' }}>
          LOAD
        </button>
        <button onClick={() => currentData && loadSeriesData(currentData.series_id, currentData.series_name)} disabled={!currentData} style={{ padding: '3px 10px', backgroundColor: '#2a2a2a', color: colors.text, border: '1px solid colors.textMuted', cursor: !currentData ? 'not-allowed' : 'pointer', fontSize: '9px' }}>
          <RefreshCw size={10} style={{ display: 'inline', marginRight: '4px' }} />REFRESH
        </button>
        <button onClick={exportData} disabled={!currentData} style={{ padding: '3px 10px', backgroundColor: '#2a2a2a', color: colors.text, border: '1px solid colors.textMuted', cursor: !currentData ? 'not-allowed' : 'pointer', fontSize: '9px' }}>
          <Download size={10} style={{ display: 'inline', marginRight: '4px' }} />EXPORT
        </button>
      </div>

      {/* Main Content */}
      <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>
        {/* Left Panel */}
        <div style={{ width: '320px', borderRight: '1px solid colors.textMuted', display: 'flex', flexDirection: 'column', backgroundColor: colors.panel }}>
          <div style={{ flex: 1, overflowY: 'auto', overflowX: 'hidden', padding: '8px' }}>
            <div style={{ color: colors.primary, fontSize: '10px', marginBottom: '4px' }}>1. PROVIDER</div>
            <div
              style={{
                width: '100%',
                height: '140px',
                backgroundColor: '#1a1a1a',
                border: '1px solid colors.textMuted',
                marginBottom: '12px',
                overflowY: 'auto',
                overflowX: 'hidden',
                scrollbarWidth: 'thin',
                scrollbarColor: 'colors.textMuted #1a1a1a'
              }}
            >
              {providers.map(p => (
                <div
                  key={p.code}
                  onClick={() => handleProviderSelect(p.code)}
                  style={{
                    padding: '6px 8px',
                    fontSize: '10px',
                    color: selectedProvider === p.code ? '#fff' : colors.text,
                    backgroundColor: selectedProvider === p.code ? colors.primary : 'transparent',
                    cursor: 'pointer',
                    fontFamily: 'Consolas, monospace',
                    lineHeight: '1.4',
                    wordWrap: 'break-word',
                    whiteSpace: 'normal',
                    borderBottom: '1px solid colors.panel'
                  }}
                  onMouseEnter={(e) => {
                    if (selectedProvider !== p.code) {
                      e.currentTarget.style.backgroundColor = '#2a2a2a';
                    }
                  }}
                  onMouseLeave={(e) => {
                    if (selectedProvider !== p.code) {
                      e.currentTarget.style.backgroundColor = 'transparent';
                    }
                  }}
                >
                  {p.code} - {p.name}
                </div>
              ))}
            </div>

            <div style={{ color: colors.primary, fontSize: '10px', marginBottom: '4px' }}>2. DATASET</div>
            <div
              style={{
                width: '100%',
                height: '140px',
                backgroundColor: '#1a1a1a',
                border: '1px solid colors.textMuted',
                marginBottom: '12px',
                overflowY: 'auto',
                overflowX: 'hidden',
                scrollbarWidth: 'thin',
                scrollbarColor: 'colors.textMuted #1a1a1a'
              }}
            >
              {datasets.map(d => (
                <div
                  key={d.code}
                  onClick={() => handleDatasetSelect(d.code)}
                  style={{
                    padding: '6px 8px',
                    fontSize: '10px',
                    color: selectedDataset === d.code ? '#fff' : colors.text,
                    backgroundColor: selectedDataset === d.code ? colors.primary : 'transparent',
                    cursor: 'pointer',
                    fontFamily: 'Consolas, monospace',
                    lineHeight: '1.4',
                    wordWrap: 'break-word',
                    whiteSpace: 'normal',
                    borderBottom: '1px solid colors.panel'
                  }}
                  onMouseEnter={(e) => {
                    if (selectedDataset !== d.code) {
                      e.currentTarget.style.backgroundColor = '#2a2a2a';
                    }
                  }}
                  onMouseLeave={(e) => {
                    if (selectedDataset !== d.code) {
                      e.currentTarget.style.backgroundColor = 'transparent';
                    }
                  }}
                >
                  {d.code} - {d.name}
                </div>
              ))}
            </div>

            <div style={{ color: colors.primary, fontSize: '10px', marginBottom: '4px' }}>3. SERIES</div>
            <div
              style={{
                width: '100%',
                height: '140px',
                backgroundColor: '#1a1a1a',
                border: '1px solid colors.textMuted',
                marginBottom: '12px',
                overflowY: 'auto',
                overflowX: 'hidden',
                scrollbarWidth: 'thin',
                scrollbarColor: 'colors.textMuted #1a1a1a'
              }}
            >
              {seriesList.map(s => (
                <div
                  key={s.full_id}
                  onClick={() => handleSeriesSelect(s)}
                  style={{
                    padding: '6px 8px',
                    fontSize: '10px',
                    color: selectedSeries === s.full_id ? '#fff' : colors.text,
                    backgroundColor: selectedSeries === s.full_id ? colors.primary : 'transparent',
                    cursor: 'pointer',
                    fontFamily: 'Consolas, monospace',
                    lineHeight: '1.4',
                    wordWrap: 'break-word',
                    whiteSpace: 'normal',
                    borderBottom: '1px solid colors.panel'
                  }}
                  onMouseEnter={(e) => {
                    if (selectedSeries !== s.full_id) {
                      e.currentTarget.style.backgroundColor = '#2a2a2a';
                    }
                  }}
                  onMouseLeave={(e) => {
                    if (selectedSeries !== s.full_id) {
                      e.currentTarget.style.backgroundColor = 'transparent';
                    }
                  }}
                >
                  {s.code} - {s.name}
                </div>
              ))}
            </div>

            <div style={{ height: '1px', backgroundColor: colors.textMuted, margin: '8px 0' }}></div>

            {/* Actions */}
            <div style={{ display: 'flex', gap: '4px', marginBottom: '8px' }}>
              <button onClick={addToSingleView} disabled={!currentData} style={{ flex: 1, padding: '4px', backgroundColor: '#1a1a1a', color: colors.text, border: '1px solid colors.textMuted', cursor: !currentData ? 'not-allowed' : 'pointer', fontSize: '9px' }}>
                ADD TO SINGLE
              </button>
              <button onClick={clearAllSlots} style={{ padding: '4px 8px', backgroundColor: '#1a1a1a', color: colors.alert, border: '1px solid colors.textMuted', cursor: 'pointer', fontSize: '9px' }}>
                <Trash2 size={10} />
              </button>
            </div>

            <div style={{ color: colors.primary, fontSize: '10px', marginBottom: '6px', display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
              <span>COMPARISON SLOTS</span>
              <button onClick={addSlot} style={{ padding: '2px 6px', backgroundColor: '#1a1a1a', color: colors.secondary, border: '1px solid colors.textMuted', cursor: 'pointer', fontSize: '9px' }}>
                <Plus size={10} />
              </button>
            </div>

            <div style={{ maxHeight: '300px', overflowY: 'auto' }}>
              {slots.map((slot, idx) => (
                <div key={idx} style={{ marginBottom: '6px', padding: '6px', backgroundColor: '#1a1a1a', border: '1px solid colors.textMuted', borderRadius: '2px' }}>
                  <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '4px' }}>
                    <span style={{ color: colors.warning, fontSize: '9px' }}>SLOT {idx + 1} ({slot.length})</span>
                    <div style={{ display: 'flex', gap: '2px' }}>
                      <button onClick={() => addToSlot(idx)} disabled={!currentData} style={{ padding: '2px 4px', backgroundColor: colors.panel, color: colors.secondary, border: '1px solid colors.textMuted', cursor: !currentData ? 'not-allowed' : 'pointer', fontSize: '8px' }}>
                        <Plus size={8} />
                      </button>
                      {slots.length > 1 && (
                        <button onClick={() => removeSlot(idx)} style={{ padding: '2px 4px', backgroundColor: colors.panel, color: colors.alert, border: '1px solid colors.textMuted', cursor: 'pointer', fontSize: '8px' }}>
                          <Minus size={8} />
                        </button>
                      )}
                    </div>
                  </div>
                  {slot.map((s, sIdx) => (
                    <div key={sIdx} style={{ fontSize: '8px', color: colors.textMuted, display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '2px' }}>
                      <span style={{ flex: 1, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }} title={s.series_name}>{s.series_name}</span>
                      <button onClick={() => removeFromSlot(idx, sIdx)} style={{ padding: '1px 3px', backgroundColor: 'transparent', color: colors.alert, border: 'none', cursor: 'pointer', fontSize: '8px' }}>
                        ×
                      </button>
                    </div>
                  ))}
                  {slot.length === 0 && <div style={{ fontSize: '8px', color: colors.textMuted, textAlign: 'center' }}>Empty</div>}
                </div>
              ))}
            </div>
          </div>
        </div>

        {/* Right Panel */}
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
          {/* View Toggle */}
          <div style={{ padding: '8px 12px', borderBottom: '1px solid colors.textMuted', display: 'flex', gap: '8px', alignItems: 'center' }}>
            <button onClick={() => setActiveView('single')} style={{ padding: '4px 12px', backgroundColor: activeView === 'single' ? colors.primary : '#2a2a2a', color: '#fff', border: '1px solid colors.textMuted', cursor: 'pointer', fontSize: '10px' }}>
              SINGLE
            </button>
            <button onClick={() => setActiveView('comparison')} style={{ padding: '4px 12px', backgroundColor: activeView === 'comparison' ? colors.primary : '#2a2a2a', color: '#fff', border: '1px solid colors.textMuted', cursor: 'pointer', fontSize: '10px' }}>
              COMPARE
            </button>

            {activeView === 'single' ? (
              <>
                <div style={{ color: colors.textMuted, fontSize: '9px', marginLeft: '8px' }}>Chart Type:</div>
                {(['line', 'bar', 'area', 'scatter', 'candlestick'] as ChartType[]).map(type => (
                  <button key={type} onClick={() => setSingleChartType(type)} style={{ padding: '3px 8px', backgroundColor: singleChartType === type ? colors.info : '#2a2a2a', color: '#fff', border: '1px solid colors.textMuted', cursor: 'pointer', fontSize: '9px', textTransform: 'uppercase' }}>
                    {type}
                  </button>
                ))}
              </>
            ) : null}
          </div>

          {/* Content Area */}
          <div style={{ flex: 1, overflowY: 'auto', padding: '12px' }}>
            {activeView === 'single' ? (
              <div>
                <div style={{ marginBottom: '12px' }}>
                  <div style={{ color: colors.primary, fontSize: '11px', marginBottom: '6px' }}>SINGLE VIEW ({singleViewSeries.length} series)</div>
                  {singleViewSeries.map((s, idx) => (
                    <div key={idx} style={{ display: 'inline-flex', alignItems: 'center', gap: '4px', padding: '3px 6px', backgroundColor: '#1a1a1a', border: '1px solid colors.textMuted', borderRadius: '2px', marginRight: '4px', marginBottom: '4px', fontSize: '9px' }}>
                      <div style={{ width: '8px', height: '8px', backgroundColor: s.color, borderRadius: '50%' }}></div>
                      <span>{s.series_name.substring(0, 20)}</span>
                      <button onClick={() => removeFromSingleView(idx)} style={{ padding: '0 4px', backgroundColor: 'transparent', color: colors.alert, border: 'none', cursor: 'pointer', fontSize: '10px' }}>×</button>
                    </div>
                  ))}
                </div>

                {singleViewSeries.length > 0 ? (
                  <>
                    {renderChart(singleViewSeries, singleChartType)}

                    {/* Data Table */}
                    <div style={{ marginTop: '24px' }}>
                      <div style={{ color: colors.primary, fontSize: '11px', marginBottom: '8px' }}>DATA TABLE</div>
                      <div style={{ overflowX: 'auto', overflowY: 'auto', maxHeight: '300px', border: '1px solid colors.textMuted', backgroundColor: colors.panel }}>
                        <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: '10px' }}>
                          <thead style={{ position: 'sticky', top: 0, backgroundColor: '#1a1a1a', zIndex: 1 }}>
                            <tr style={{ borderBottom: '2px solid colors.textMuted' }}>
                              <th style={{ padding: '8px 12px', textAlign: 'left', color: colors.primary, fontWeight: 'bold', whiteSpace: 'nowrap' }}>Period</th>
                              {singleViewSeries.map((series, idx) => (
                                <th key={idx} style={{ padding: '8px 12px', textAlign: 'right', fontWeight: 'bold', whiteSpace: 'nowrap' }}>
                                  <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'flex-end', gap: '6px' }}>
                                    <div style={{ width: '8px', height: '8px', backgroundColor: series.color, borderRadius: '50%' }}></div>
                                    <span style={{ color: series.color }}>{series.series_name.substring(0, 25)}</span>
                                  </div>
                                </th>
                              ))}
                            </tr>
                          </thead>
                          <tbody>
                            {(() => {
                              // Get all unique periods across all series
                              const allPeriods = Array.from(
                                new Set(singleViewSeries.flatMap(s => s.observations.map(o => o.period)))
                              ).sort((a, b) => new Date(b).getTime() - new Date(a).getTime());

                              return allPeriods.slice(0, 50).map((period, idx) => (
                                <tr key={idx} style={{ borderBottom: '1px solid #1a1a1a' }}>
                                  <td style={{ padding: '6px 12px', color: colors.text, whiteSpace: 'nowrap', fontWeight: 'bold' }}>{period}</td>
                                  {singleViewSeries.map((series, sIdx) => {
                                    const obs = series.observations.find(o => o.period === period);
                                    const value = obs ? (typeof obs.value === 'number' ? obs.value : Number(obs.value)) : null;
                                    return (
                                      <td key={sIdx} style={{ padding: '6px 12px', textAlign: 'right', color: value !== null ? '#a3a3a3' : colors.textMuted, whiteSpace: 'nowrap' }}>
                                        {value !== null && !isNaN(value) ? value.toFixed(4) : '—'}
                                      </td>
                                    );
                                  })}
                                </tr>
                              ));
                            })()}
                          </tbody>
                        </table>
                      </div>
                      <div style={{ color: colors.textMuted, fontSize: '9px', marginTop: '6px', textAlign: 'right' }}>
                        Showing latest 50 periods
                      </div>
                    </div>
                  </>
                ) : (
                  <div style={{ color: colors.textMuted, textAlign: 'center', paddingTop: '40px' }}>Add series to view</div>
                )}
              </div>
            ) : (
              <div>
                <div style={{ color: colors.primary, marginBottom: '12px', fontSize: '11px' }}>COMPARISON DASHBOARD</div>
                <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(380px, 1fr))', gap: '16px' }}>
                  {slots.map((slot, idx) => (
                    <div key={idx} style={{ border: '1px solid colors.textMuted', padding: '12px', backgroundColor: colors.panel, minHeight: '280px', display: 'flex', flexDirection: 'column' }}>
                      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '8px' }}>
                        <div style={{ color: colors.warning, fontSize: '10px' }}>SLOT {idx + 1}</div>
                        <select value={slotChartTypes[idx]} onChange={(e) => { const newTypes = [...slotChartTypes]; newTypes[idx] = e.target.value as ChartType; setSlotChartTypes(newTypes); }} style={{ padding: '2px 4px', backgroundColor: '#1a1a1a', color: colors.text, border: '1px solid colors.textMuted', fontSize: '8px', fontFamily: 'Consolas, monospace' }}>
                          <option value="line">LINE</option>
                          <option value="bar">BAR</option>
                          <option value="area">AREA</option>
                          <option value="scatter">SCATTER</option>
                          <option value="candlestick">CANDLE</option>
                        </select>
                      </div>
                      <div style={{ flex: 1, display: 'flex', alignItems: 'center', justifyContent: 'center', minHeight: '240px' }}>
                        {slot.length > 0 ? renderChart(slot, slotChartTypes[idx], true) : <div style={{ color: colors.textMuted, textAlign: 'center', fontSize: '10px' }}>Empty Slot</div>}
                      </div>
                    </div>
                  ))}
                </div>
              </div>
            )}
          </div>
        </div>
      </div>
    </div>
  );
}