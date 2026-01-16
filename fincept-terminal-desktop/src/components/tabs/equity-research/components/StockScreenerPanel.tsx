// Stock Screener Panel Component
// Advanced equity screening UI with filter builder and preset screens

import React, { useState, useEffect } from 'react';
import { screenerApi } from '../../../../services/screener/screenerApi';
import type {
  ScreenCriteria,
  ScreenResult,
  FilterCondition,
  MetricType,
  FilterOperator,
  FilterValue,
  MetricInfo,
  StockData,
  SavedScreen,
} from '../../../../types/screener';
import {
  createSingleValue,
  createRangeValue,
  getMetricDisplayName,
  getOperatorDisplayName,
} from '../../../../types/screener';

type PresetScreen = 'value' | 'growth' | 'dividend' | 'momentum' | 'custom';

export const StockScreenerPanel: React.FC = () => {
  const [selectedPreset, setSelectedPreset] = useState<PresetScreen>('value');
  const [criteria, setCriteria] = useState<ScreenCriteria | null>(null);
  const [results, setResults] = useState<ScreenResult | null>(null);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [availableMetrics, setAvailableMetrics] = useState<MetricInfo[]>([]);
  const [savedScreens, setSavedScreens] = useState<SavedScreen[]>([]);
  const [showSaveDialog, setShowSaveDialog] = useState(false);
  const [newScreenName, setNewScreenName] = useState('');

  // Load available metrics on mount
  useEffect(() => {
    loadAvailableMetrics();
    loadSavedScreens();
  }, []);

  // Load preset when selected
  useEffect(() => {
    if (selectedPreset !== 'custom') {
      loadPresetScreen(selectedPreset);
    }
  }, [selectedPreset]);

  const loadAvailableMetrics = async () => {
    try {
      const metrics = await screenerApi.getAvailableMetrics();
      setAvailableMetrics(metrics);
    } catch (err) {
      console.error('Failed to load metrics:', err);
    }
  };

  const loadSavedScreens = async () => {
    try {
      const screens = await screenerApi.loadCustomScreens();
      setSavedScreens(screens);
    } catch (err) {
      console.error('Failed to load saved screens:', err);
    }
  };

  const loadPresetScreen = async (preset: 'value' | 'growth' | 'dividend' | 'momentum') => {
    try {
      let presetCriteria: ScreenCriteria;
      switch (preset) {
        case 'value':
          presetCriteria = await screenerApi.getValueScreen();
          break;
        case 'growth':
          presetCriteria = await screenerApi.getGrowthScreen();
          break;
        case 'dividend':
          presetCriteria = await screenerApi.getDividendScreen();
          break;
        case 'momentum':
          presetCriteria = await screenerApi.getMomentumScreen();
          break;
      }
      setCriteria(presetCriteria);
      setError(null);
    } catch (err) {
      setError(`Failed to load ${preset} screen`);
      console.error(err);
    }
  };

  const executeScreen = async () => {
    if (!criteria) {
      setError('No screening criteria defined');
      return;
    }

    setLoading(true);
    setError(null);

    try {
      const screenResults = await screenerApi.executeScreen(criteria);
      setResults(screenResults);

      // Show warning if using cached data
      if ((screenResults as any).error) {
        setError((screenResults as any).error);
      }
    } catch (err) {
      setError('Failed to execute screen');
      console.error(err);
    } finally {
      setLoading(false);
    }
  };

  const addCondition = () => {
    if (!criteria) {
      setCriteria({
        name: 'Custom Screen',
        description: 'User-defined screening criteria',
        conditions: [],
      });
    } else {
      setCriteria({
        ...criteria,
        conditions: [
          ...criteria.conditions,
          {
            metric: 'PeRatio',
            operator: 'LessThan',
            value: createSingleValue(20),
          },
        ],
      });
    }
  };

  const removeCondition = (index: number) => {
    if (!criteria) return;
    setCriteria({
      ...criteria,
      conditions: criteria.conditions.filter((_, i) => i !== index),
    });
  };

  const updateCondition = (index: number, field: keyof FilterCondition, value: any) => {
    if (!criteria) return;
    const newConditions = [...criteria.conditions];
    newConditions[index] = { ...newConditions[index], [field]: value };
    setCriteria({ ...criteria, conditions: newConditions });
  };

  const saveScreen = async () => {
    if (!criteria || !newScreenName.trim()) return;

    try {
      const saved = await screenerApi.saveCustomScreen(newScreenName, criteria);
      setSavedScreens([...savedScreens, saved]);
      setShowSaveDialog(false);
      setNewScreenName('');
    } catch (err) {
      console.error('Failed to save screen:', err);
    }
  };

  const loadSavedScreen = (screen: SavedScreen) => {
    setCriteria(screen.criteria);
    setSelectedPreset('custom');
  };

  const exportToCSV = () => {
    if (!results) return;

    const headers = ['Symbol', 'Name', 'Sector', 'Industry', ...Object.keys(results.matchingStocks[0]?.metrics || {})];
    const rows = results.matchingStocks.map((stock) => [
      stock.symbol,
      stock.name,
      stock.sector,
      stock.industry,
      ...Object.values(stock.metrics),
    ]);

    const csvContent = [
      headers.join(','),
      ...rows.map((row) => row.join(',')),
    ].join('\n');

    const blob = new Blob([csvContent], { type: 'text/csv;charset=utf-8;' });
    const link = document.createElement('a');
    const url = URL.createObjectURL(blob);
    link.setAttribute('href', url);
    link.setAttribute('download', `screen_results_${new Date().toISOString().split('T')[0]}.csv`);
    link.style.visibility = 'hidden';
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
  };

  return (
    <div className="space-y-6">
      {/* Header */}
      <div className="flex justify-between items-center">
        <div>
          <h2 className="text-2xl font-bold text-white">Stock Screener</h2>
          <p className="text-gray-400 text-sm mt-1">
            Filter stocks by fundamental metrics and criteria
          </p>
        </div>

        <div className="flex gap-2">
          {results && (
            <button
              onClick={exportToCSV}
              className="px-4 py-2 bg-gray-700 hover:bg-gray-600 text-white rounded-lg transition-colors font-medium flex items-center gap-2"
            >
              <svg className="w-4 h-4" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M12 10v6m0 0l-3-3m3 3l3-3m2 8H7a2 2 0 01-2-2V5a2 2 0 012-2h5.586a1 1 0 01.707.293l5.414 5.414a1 1 0 01.293.707V19a2 2 0 01-2 2z" />
              </svg>
              Export CSV
            </button>
          )}
          <button
            onClick={() => setShowSaveDialog(true)}
            disabled={!criteria || selectedPreset !== 'custom'}
            className="px-4 py-2 bg-gray-700 hover:bg-gray-600 disabled:bg-gray-800 disabled:cursor-not-allowed text-white rounded-lg transition-colors font-medium"
          >
            Save Screen
          </button>
          <button
            onClick={executeScreen}
            disabled={loading || !criteria || criteria.conditions.length === 0}
            className="px-4 py-2 bg-blue-600 hover:bg-blue-700 disabled:bg-gray-700 disabled:cursor-not-allowed text-white rounded-lg transition-colors font-medium"
          >
            {loading ? 'Screening...' : 'Run Screen'}
          </button>
        </div>
      </div>

      {/* Error Display */}
      {error && (
        <div className="bg-red-900/20 border border-red-700 rounded-lg p-4">
          <div className="flex items-center gap-3">
            <svg className="w-5 h-5 text-red-400" fill="none" viewBox="0 0 24 24" stroke="currentColor">
              <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M12 9v2m0 4h.01m-6.938 4h13.856c1.54 0 2.502-1.667 1.732-3L13.732 4c-.77-1.333-2.694-1.333-3.464 0L3.34 16c-.77 1.333.192 3 1.732 3z" />
            </svg>
            <p className="text-red-400">{error}</p>
          </div>
        </div>
      )}

      {/* Loading Overlay */}
      {loading && (
        <div className="bg-blue-900/20 border border-blue-700 rounded-lg p-6">
          <div className="flex items-center justify-center gap-4">
            <div className="animate-spin rounded-full h-8 w-8 border-b-2 border-blue-400"></div>
            <div>
              <p className="text-blue-400 font-semibold">Fetching real-time data from FMP...</p>
              <p className="text-blue-300 text-sm mt-1">This may take 30-60 seconds for 200+ stocks</p>
            </div>
          </div>
        </div>
      )}

      {/* Preset Screens */}
      <div className="bg-gray-900 rounded-lg p-4 border border-gray-700">
        <h3 className="text-lg font-semibold text-white mb-3">Screen Templates</h3>
        <div className="grid grid-cols-2 md:grid-cols-5 gap-3">
          {(['value', 'growth', 'dividend', 'momentum', 'custom'] as PresetScreen[]).map((preset) => (
            <button
              key={preset}
              onClick={() => setSelectedPreset(preset)}
              className={`p-3 rounded-lg border transition-colors ${
                selectedPreset === preset
                  ? 'bg-blue-600 border-blue-500 text-white'
                  : 'bg-gray-800 border-gray-700 text-gray-300 hover:bg-gray-700'
              }`}
            >
              <div className="font-semibold capitalize">{preset}</div>
              <div className="text-xs mt-1 opacity-80">
                {preset === 'value' && 'Low P/E, P/B'}
                {preset === 'growth' && 'High Growth'}
                {preset === 'dividend' && 'High Yield'}
                {preset === 'momentum' && 'Strong Trends'}
                {preset === 'custom' && 'Build Your Own'}
              </div>
            </button>
          ))}
        </div>
      </div>

      {/* Saved Screens */}
      {savedScreens.length > 0 && (
        <div className="bg-gray-900 rounded-lg p-4 border border-gray-700">
          <h3 className="text-lg font-semibold text-white mb-3">Saved Screens</h3>
          <div className="flex flex-wrap gap-2">
            {savedScreens.map((screen) => (
              <button
                key={screen.id}
                onClick={() => loadSavedScreen(screen)}
                className="px-3 py-2 text-sm bg-gray-800 hover:bg-gray-700 border border-gray-700 text-white rounded transition-colors"
              >
                {screen.name}
              </button>
            ))}
          </div>
        </div>
      )}

      {/* Filter Builder */}
      {criteria && (
        <div className="bg-gray-900 rounded-lg p-6 border border-gray-700">
          <div className="flex items-center justify-between mb-4">
            <div>
              <h3 className="text-lg font-semibold text-white">{criteria.name}</h3>
              <p className="text-sm text-gray-400">{criteria.description}</p>
            </div>
            <button
              onClick={addCondition}
              className="px-4 py-2 bg-blue-600 hover:bg-blue-700 text-white rounded-lg transition-colors font-medium flex items-center gap-2"
            >
              <svg className="w-4 h-4" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M12 4v16m8-8H4" />
              </svg>
              Add Filter
            </button>
          </div>

          {/* Filter Conditions */}
          <div className="space-y-3">
            {criteria.conditions.length === 0 ? (
              <div className="text-center py-8 text-gray-400">
                No filters defined. Click "Add Filter" to start building your screen.
              </div>
            ) : (
              criteria.conditions.map((condition, index) => (
                <FilterConditionRow
                  key={index}
                  condition={condition}
                  index={index}
                  availableMetrics={availableMetrics}
                  onUpdate={updateCondition}
                  onRemove={removeCondition}
                />
              ))
            )}
          </div>
        </div>
      )}

      {/* Results */}
      {results && (
        <div className="bg-gray-900 rounded-lg border border-gray-700">
          <div className="p-4 border-b border-gray-700">
            <div className="flex items-center justify-between">
              <div>
                <h3 className="text-lg font-semibold text-white">
                  Screening Results
                </h3>
                <p className="text-sm text-gray-400 mt-1">
                  Found {results.matchingStocks.length} of {results.totalStocksScreened} stocks
                </p>
              </div>
              <div className="text-sm text-gray-400">
                {((results.matchingStocks.length / results.totalStocksScreened) * 100).toFixed(1)}% match rate
              </div>
            </div>
          </div>

          <div className="overflow-x-auto">
            <ResultsTable stocks={results.matchingStocks} />
          </div>
        </div>
      )}

      {/* Save Dialog */}
      {showSaveDialog && (
        <div className="fixed inset-0 bg-black bg-opacity-50 flex items-center justify-center z-50">
          <div className="bg-gray-800 rounded-lg p-6 max-w-md w-full mx-4 border border-gray-700">
            <h3 className="text-xl font-semibold text-white mb-4">Save Custom Screen</h3>
            <input
              type="text"
              value={newScreenName}
              onChange={(e) => setNewScreenName(e.target.value)}
              placeholder="Enter screen name..."
              className="w-full px-4 py-2 bg-gray-900 border border-gray-700 rounded-lg text-white placeholder-gray-500 focus:outline-none focus:border-blue-500"
            />
            <div className="flex gap-3 mt-6">
              <button
                onClick={() => setShowSaveDialog(false)}
                className="flex-1 px-4 py-2 bg-gray-700 hover:bg-gray-600 text-white rounded-lg transition-colors"
              >
                Cancel
              </button>
              <button
                onClick={saveScreen}
                disabled={!newScreenName.trim()}
                className="flex-1 px-4 py-2 bg-blue-600 hover:bg-blue-700 disabled:bg-gray-700 disabled:cursor-not-allowed text-white rounded-lg transition-colors"
              >
                Save
              </button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
};

// Filter Condition Row Component
interface FilterConditionRowProps {
  condition: FilterCondition;
  index: number;
  availableMetrics: MetricInfo[];
  onUpdate: (index: number, field: keyof FilterCondition, value: any) => void;
  onRemove: (index: number) => void;
}

const FilterConditionRow: React.FC<FilterConditionRowProps> = ({
  condition,
  index,
  availableMetrics,
  onUpdate,
  onRemove,
}) => {
  const operators: FilterOperator[] = [
    'GreaterThan',
    'LessThan',
    'GreaterThanOrEqual',
    'LessThanOrEqual',
    'Equal',
    'Between',
  ];

  const getValue = (): number | { min: number; max: number } => {
    if ('Single' in condition.value) {
      return condition.value.Single;
    } else if ('Range' in condition.value) {
      return condition.value.Range;
    }
    return 0;
  };

  const handleValueChange = (newValue: number | { min: number; max: number }) => {
    if (condition.operator === 'Between') {
      onUpdate(index, 'value', createRangeValue((newValue as any).min, (newValue as any).max));
    } else {
      onUpdate(index, 'value', createSingleValue(newValue as number));
    }
  };

  const value = getValue();
  const isBetween = condition.operator === 'Between';

  return (
    <div className="flex items-center gap-3 p-3 bg-gray-800 rounded-lg border border-gray-700">
      {/* Metric Selector */}
      <select
        value={condition.metric}
        onChange={(e) => onUpdate(index, 'metric', e.target.value as MetricType)}
        className="flex-1 px-3 py-2 bg-gray-900 border border-gray-700 rounded text-white focus:outline-none focus:border-blue-500"
      >
        {availableMetrics.map((metric) => (
          <option key={metric.id} value={metric.type}>
            {metric.name} ({metric.category})
          </option>
        ))}
      </select>

      {/* Operator Selector */}
      <select
        value={condition.operator}
        onChange={(e) => {
          const newOperator = e.target.value as FilterOperator;
          onUpdate(index, 'operator', newOperator);
          // Reset value when switching to/from Between
          if (newOperator === 'Between') {
            onUpdate(index, 'value', createRangeValue(0, 100));
          } else if (condition.operator === 'Between') {
            onUpdate(index, 'value', createSingleValue(0));
          }
        }}
        className="w-32 px-3 py-2 bg-gray-900 border border-gray-700 rounded text-white focus:outline-none focus:border-blue-500"
      >
        {operators.map((op) => (
          <option key={op} value={op}>
            {getOperatorDisplayName(op)}
          </option>
        ))}
      </select>

      {/* Value Input(s) */}
      {isBetween ? (
        <div className="flex items-center gap-2">
          <input
            type="number"
            value={(value as { min: number; max: number }).min}
            onChange={(e) =>
              handleValueChange({
                min: parseFloat(e.target.value) || 0,
                max: (value as { min: number; max: number }).max,
              })
            }
            className="w-24 px-3 py-2 bg-gray-900 border border-gray-700 rounded text-white focus:outline-none focus:border-blue-500"
            placeholder="Min"
          />
          <span className="text-gray-400">to</span>
          <input
            type="number"
            value={(value as { min: number; max: number }).max}
            onChange={(e) =>
              handleValueChange({
                min: (value as { min: number; max: number }).min,
                max: parseFloat(e.target.value) || 0,
              })
            }
            className="w-24 px-3 py-2 bg-gray-900 border border-gray-700 rounded text-white focus:outline-none focus:border-blue-500"
            placeholder="Max"
          />
        </div>
      ) : (
        <input
          type="number"
          value={value as number}
          onChange={(e) => handleValueChange(parseFloat(e.target.value) || 0)}
          className="w-32 px-3 py-2 bg-gray-900 border border-gray-700 rounded text-white focus:outline-none focus:border-blue-500"
          placeholder="Value"
          step="0.01"
        />
      )}

      {/* Remove Button */}
      <button
        onClick={() => onRemove(index)}
        className="p-2 hover:bg-gray-700 rounded transition-colors"
        title="Remove filter"
      >
        <svg className="w-5 h-5 text-red-400" fill="none" viewBox="0 0 24 24" stroke="currentColor">
          <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M6 18L18 6M6 6l12 12" />
        </svg>
      </button>
    </div>
  );
};

// Results Table Component
interface ResultsTableProps {
  stocks: StockData[];
}

const ResultsTable: React.FC<ResultsTableProps> = ({ stocks }) => {
  const [sortField, setSortField] = useState<string>('symbol');
  const [sortDirection, setSortDirection] = useState<'asc' | 'desc'>('asc');

  if (stocks.length === 0) {
    return (
      <div className="p-8 text-center text-gray-400">
        No stocks match the screening criteria.
      </div>
    );
  }

  const allMetricKeys = Object.keys(stocks[0].metrics);

  const handleSort = (field: string) => {
    if (sortField === field) {
      setSortDirection(sortDirection === 'asc' ? 'desc' : 'asc');
    } else {
      setSortField(field);
      setSortDirection('asc');
    }
  };

  const sortedStocks = [...stocks].sort((a, b) => {
    let aVal: any;
    let bVal: any;

    if (sortField === 'symbol' || sortField === 'name' || sortField === 'sector' || sortField === 'industry') {
      aVal = a[sortField];
      bVal = b[sortField];
    } else {
      aVal = a.metrics[sortField] ?? 0;
      bVal = b.metrics[sortField] ?? 0;
    }

    if (typeof aVal === 'string') {
      return sortDirection === 'asc' ? aVal.localeCompare(bVal) : bVal.localeCompare(aVal);
    }

    return sortDirection === 'asc' ? aVal - bVal : bVal - aVal;
  });

  return (
    <table className="min-w-full divide-y divide-gray-700">
      <thead className="bg-gray-800">
        <tr>
          <SortableHeader field="symbol" label="Symbol" sortField={sortField} sortDirection={sortDirection} onSort={handleSort} />
          <SortableHeader field="name" label="Name" sortField={sortField} sortDirection={sortDirection} onSort={handleSort} />
          <SortableHeader field="sector" label="Sector" sortField={sortField} sortDirection={sortDirection} onSort={handleSort} />
          <SortableHeader field="industry" label="Industry" sortField={sortField} sortDirection={sortDirection} onSort={handleSort} />
          {allMetricKeys.slice(0, 6).map((key) => (
            <SortableHeader
              key={key}
              field={key}
              label={key.replace(/([A-Z])/g, ' $1').trim()}
              sortField={sortField}
              sortDirection={sortDirection}
              onSort={handleSort}
            />
          ))}
        </tr>
      </thead>
      <tbody className="divide-y divide-gray-800">
        {sortedStocks.map((stock) => (
          <tr key={stock.symbol} className="hover:bg-gray-800 transition-colors">
            <td className="px-4 py-3 whitespace-nowrap text-sm font-semibold text-blue-400">
              {stock.symbol}
            </td>
            <td className="px-4 py-3 whitespace-nowrap text-sm text-gray-300">
              {stock.name}
            </td>
            <td className="px-4 py-3 whitespace-nowrap text-sm text-gray-400">
              {stock.sector}
            </td>
            <td className="px-4 py-3 whitespace-nowrap text-sm text-gray-400">
              {stock.industry}
            </td>
            {allMetricKeys.slice(0, 6).map((key) => (
              <td key={key} className="px-4 py-3 whitespace-nowrap text-sm text-right text-gray-300">
                {stock.metrics[key]?.toFixed(2) || 'N/A'}
              </td>
            ))}
          </tr>
        ))}
      </tbody>
    </table>
  );
};

// Sortable Header Component
interface SortableHeaderProps {
  field: string;
  label: string;
  sortField: string;
  sortDirection: 'asc' | 'desc';
  onSort: (field: string) => void;
}

const SortableHeader: React.FC<SortableHeaderProps> = ({
  field,
  label,
  sortField,
  sortDirection,
  onSort,
}) => {
  const isActive = sortField === field;

  return (
    <th
      onClick={() => onSort(field)}
      className="px-4 py-3 text-left text-xs font-medium text-gray-400 uppercase tracking-wider cursor-pointer hover:text-gray-300 transition-colors"
    >
      <div className="flex items-center gap-1">
        {label}
        {isActive && (
          <svg
            className={`w-4 h-4 transition-transform ${sortDirection === 'desc' ? 'rotate-180' : ''}`}
            fill="none"
            viewBox="0 0 24 24"
            stroke="currentColor"
          >
            <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M5 15l7-7 7 7" />
          </svg>
        )}
      </div>
    </th>
  );
};
