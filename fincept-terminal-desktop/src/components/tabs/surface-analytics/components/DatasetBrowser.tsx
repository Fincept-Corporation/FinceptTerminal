/**
 * Surface Analytics - Dataset Browser Component
 * Browse and explore available Databento datasets
 */

import React, { useState, useEffect, useCallback } from 'react';
import { Database, ChevronRight, RefreshCw, AlertCircle, Calendar, FileText, DollarSign } from 'lucide-react';
import { invoke } from '@tauri-apps/api/core';
import { useTranslation } from 'react-i18next';
import { useTerminalTheme } from '@/contexts/ThemeContext';

interface DatasetInfo {
  id: string;
  name: string;
  start_date?: string;
  end_date?: string;
  schemas?: string[];
}

interface DatasetBrowserProps {
  apiKey: string | null;
  onSelectDataset?: (dataset: string) => void;
  selectedDataset?: string;
  accentColor: string;
  textColor: string;
}

// Dataset categories for organization
const DATASET_CATEGORIES: Record<string, string[]> = {
  'US Equities': ['XNAS.ITCH', 'XNYS.PILLAR', 'XNAS.BASIC', 'DBEQ.BASIC', 'ARCX.PILLAR'],
  'US Options': ['OPRA.PILLAR'],
  'Futures': ['GLBX.MDP3', 'IFEU.IMPACT', 'IFUS.IMPACT'],
  'Other': [],
};

// Dataset descriptions
const DATASET_DESCRIPTIONS: Record<string, string> = {
  'OPRA.PILLAR': 'US equity options (all exchanges)',
  'XNAS.ITCH': 'NASDAQ full depth order book',
  'XNYS.PILLAR': 'NYSE full depth order book',
  'GLBX.MDP3': 'CME Globex futures & options',
  'DBEQ.BASIC': 'Sample equity data (free tier)',
  'XNAS.BASIC': 'NASDAQ basic trades',
  'ARCX.PILLAR': 'NYSE Arca equities',
  'IFEU.IMPACT': 'ICE Futures Europe',
  'IFUS.IMPACT': 'ICE Futures US',
};

export const DatasetBrowser: React.FC<DatasetBrowserProps> = ({
  apiKey,
  onSelectDataset,
  selectedDataset,
  accentColor,
  textColor,
}) => {
  const { t } = useTranslation('surfaceAnalytics');
  const { colors } = useTerminalTheme();
  const [datasets, setDatasets] = useState<DatasetInfo[]>([]);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [expandedDataset, setExpandedDataset] = useState<string | null>(null);
  const [datasetDetails, setDatasetDetails] = useState<Record<string, DatasetInfo>>({});
  const [loadingDetails, setLoadingDetails] = useState<string | null>(null);

  // Fetch available datasets
  const fetchDatasets = useCallback(async () => {
    if (!apiKey) {
      setError('API key required to browse datasets');
      return;
    }

    setLoading(true);
    setError(null);

    try {
      const result = await invoke<string>('databento_list_datasets', {
        apiKey: apiKey,
      });
      const parsed = JSON.parse(result);

      if (parsed.error) {
        setError(parsed.message);
      } else {
        setDatasets(parsed.datasets || []);
      }
    } catch (err) {
      setError(`Failed to fetch datasets: ${err}`);
    } finally {
      setLoading(false);
    }
  }, [apiKey]);

  // Fetch dataset details when expanded
  const fetchDatasetDetails = useCallback(async (dataset: string) => {
    if (!apiKey || datasetDetails[dataset]) return;

    setLoadingDetails(dataset);

    try {
      const result = await invoke<string>('databento_get_dataset_info', {
        apiKey: apiKey,
        dataset: dataset,
      });
      const parsed = JSON.parse(result);

      if (!parsed.error) {
        setDatasetDetails(prev => ({
          ...prev,
          [dataset]: parsed,
        }));
      }
    } catch (err) {
      console.error(`Failed to fetch details for ${dataset}:`, err);
    } finally {
      setLoadingDetails(null);
    }
  }, [apiKey, datasetDetails]);

  // Load datasets on mount
  useEffect(() => {
    if (apiKey) {
      fetchDatasets();
    }
  }, [apiKey, fetchDatasets]);

  // Handle dataset expansion
  const handleToggleExpand = (dataset: string) => {
    if (expandedDataset === dataset) {
      setExpandedDataset(null);
    } else {
      setExpandedDataset(dataset);
      fetchDatasetDetails(dataset);
    }
  };

  // Categorize datasets
  const categorizedDatasets = () => {
    const result: Record<string, DatasetInfo[]> = {};
    const usedIds = new Set<string>();

    // Categorize known datasets
    for (const [category, knownIds] of Object.entries(DATASET_CATEGORIES)) {
      result[category] = datasets.filter(d => {
        const isInCategory = knownIds.some(known => d.id.includes(known) || d.name.includes(known));
        if (isInCategory) {
          usedIds.add(d.id);
        }
        return isInCategory;
      });
    }

    // Add uncategorized to "Other"
    result['Other'] = datasets.filter(d => !usedIds.has(d.id));

    return result;
  };

  if (!apiKey) {
    return (
      <div
        className="p-4 text-center"
        style={{
          backgroundColor: colors.background,
          border: `1px solid ${colors.textMuted}`,
          borderRadius: 'var(--ft-border-radius)',
        }}
      >
        <Database size={24} style={{ color: colors.textMuted, margin: '0 auto 8px' }} />
        <div className="text-xs" style={{ color: colors.textMuted }}>
          Configure Databento API key to browse datasets
        </div>
      </div>
    );
  }

  return (
    <div
      style={{
        backgroundColor: colors.background,
        border: `1px solid ${colors.textMuted}`,
        borderRadius: 'var(--ft-border-radius)',
        overflow: 'hidden',
      }}
    >
      {/* Header */}
      <div
        className="flex items-center justify-between p-2"
        style={{
          backgroundColor: colors.panel,
          borderBottom: `1px solid ${colors.textMuted}`,
        }}
      >
        <div className="flex items-center gap-2">
          <Database size={14} style={{ color: accentColor }} />
          <span className="text-xs font-bold" style={{ color: accentColor }}>
            AVAILABLE DATASETS
          </span>
          <span className="text-xs" style={{ color: colors.textMuted }}>
            ({datasets.length})
          </span>
        </div>
        <button
          onClick={fetchDatasets}
          disabled={loading}
          style={{ color: colors.textMuted }}
        >
          <RefreshCw size={12} className={loading ? 'animate-spin' : ''} />
        </button>
      </div>

      {/* Content */}
      <div className="max-h-64 overflow-y-auto p-2">
        {error ? (
          <div className="flex items-center gap-2 p-2 text-xs" style={{ color: colors.alert }}>
            <AlertCircle size={14} />
            {error}
          </div>
        ) : loading ? (
          <div className="flex items-center justify-center p-4">
            <RefreshCw size={16} className="animate-spin" style={{ color: accentColor }} />
          </div>
        ) : datasets.length === 0 ? (
          <div className="text-xs text-center p-4" style={{ color: colors.textMuted }}>
            No datasets available
          </div>
        ) : (
          Object.entries(categorizedDatasets()).map(([category, categoryDatasets]) => {
            if (categoryDatasets.length === 0) return null;

            return (
              <div key={category} className="mb-3">
                <div
                  className="text-xs font-bold mb-1 px-1"
                  style={{ color: colors.textMuted, letterSpacing: '0.5px' }}
                >
                  {category.toUpperCase()}
                </div>

                {categoryDatasets.map(dataset => {
                  const isExpanded = expandedDataset === dataset.id;
                  const isSelected = selectedDataset === dataset.id;
                  const details = datasetDetails[dataset.id];
                  const description = DATASET_DESCRIPTIONS[dataset.id] || dataset.name;

                  return (
                    <div
                      key={dataset.id}
                      style={{
                        backgroundColor: isSelected ? `${accentColor}20` : colors.background,
                        border: `1px solid ${isSelected ? accentColor : colors.textMuted}`,
                        borderRadius: 'var(--ft-border-radius)',
                        marginBottom: '4px',
                      }}
                    >
                      {/* Dataset Row */}
                      <div
                        className="flex items-center justify-between p-2 cursor-pointer"
                        onClick={() => handleToggleExpand(dataset.id)}
                      >
                        <div className="flex items-center gap-2">
                          <ChevronRight
                            size={12}
                            style={{
                              color: colors.textMuted,
                              transform: isExpanded ? 'rotate(90deg)' : 'none',
                              transition: 'transform 0.2s',
                            }}
                          />
                          <span className="text-xs font-bold" style={{ color: textColor }}>
                            {dataset.id}
                          </span>
                        </div>
                        <button
                          onClick={(e) => {
                            e.stopPropagation();
                            onSelectDataset?.(dataset.id);
                          }}
                          className="px-2 py-0.5 text-xs"
                          style={{
                            backgroundColor: isSelected ? accentColor : 'transparent',
                            color: isSelected ? colors.background : accentColor,
                            border: `1px solid ${accentColor}`,
                            borderRadius: 'var(--ft-border-radius)',
                          }}
                        >
                          {isSelected ? 'SELECTED' : 'SELECT'}
                        </button>
                      </div>

                      {/* Dataset Details (expanded) */}
                      {isExpanded && (
                        <div
                          className="px-2 pb-2"
                          style={{ borderTop: `1px solid ${colors.textMuted}` }}
                        >
                          <div className="text-xs mt-2 mb-2" style={{ color: colors.textMuted }}>
                            {description}
                          </div>

                          {loadingDetails === dataset.id ? (
                            <div className="flex items-center gap-2 text-xs" style={{ color: colors.textMuted }}>
                              <RefreshCw size={10} className="animate-spin" />
                              Loading details...
                            </div>
                          ) : details ? (
                            <div className="grid grid-cols-2 gap-2 text-xs">
                              {details.start_date && (
                                <div className="flex items-center gap-1" style={{ color: colors.info }}>
                                  <Calendar size={10} />
                                  <span>From: {details.start_date}</span>
                                </div>
                              )}
                              {details.end_date && (
                                <div className="flex items-center gap-1" style={{ color: colors.info }}>
                                  <Calendar size={10} />
                                  <span>To: {details.end_date}</span>
                                </div>
                              )}
                              {details.schemas && details.schemas.length > 0 && (
                                <div className="col-span-2">
                                  <div className="flex items-center gap-1 mb-1" style={{ color: colors.success }}>
                                    <FileText size={10} />
                                    <span>Schemas:</span>
                                  </div>
                                  <div className="flex flex-wrap gap-1">
                                    {details.schemas.slice(0, 8).map(schema => (
                                      <span
                                        key={schema}
                                        className="px-1 py-0.5"
                                        style={{
                                          backgroundColor: colors.textMuted,
                                          color: textColor,
                                          fontSize: '9px',
                                          borderRadius: 'var(--ft-border-radius)',
                                        }}
                                      >
                                        {schema}
                                      </span>
                                    ))}
                                    {details.schemas.length > 8 && (
                                      <span className="text-xs" style={{ color: colors.textMuted }}>
                                        +{details.schemas.length - 8} more
                                      </span>
                                    )}
                                  </div>
                                </div>
                              )}
                            </div>
                          ) : null}
                        </div>
                      )}
                    </div>
                  );
                })}
              </div>
            );
          })
        )}
      </div>
    </div>
  );
};
