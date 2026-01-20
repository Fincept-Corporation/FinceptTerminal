import React, { useState, useEffect } from 'react';
import {
  TrendingUp,
  TrendingDown,
  Minus,
  Database,
  RefreshCw,
  Settings2,
} from 'lucide-react';
import {
  LineChart,
  Line,
  AreaChart,
  Area,
  BarChart,
  Bar,
  PieChart,
  Pie,
  Cell,
  XAxis,
  YAxis,
  Tooltip,
  ResponsiveContainer,
} from 'recharts';

// ============ Types ============
export interface LiveDataConfig {
  source: 'portfolio' | 'watchlist' | 'market' | 'custom';
  ticker?: string;
  columns?: string[];
  refreshInterval?: number;
}

export interface KPICardConfig {
  title: string;
  value: string | number;
  change?: number;
  changeType?: 'percent' | 'absolute';
  prefix?: string;
  suffix?: string;
  color?: string;
}

export interface SparklineConfig {
  data: number[];
  color?: string;
  height?: number;
  showArea?: boolean;
}

export interface DynamicChartConfig {
  type: 'line' | 'bar' | 'area' | 'pie' | 'candlestick';
  data: any[];
  xKey?: string;
  yKey?: string;
  colors?: string[];
  height?: number;
}

// ============ Sample Data ============
const SAMPLE_PORTFOLIO_DATA = [
  { symbol: 'AAPL', name: 'Apple Inc.', shares: 100, price: 178.50, change: 2.35, value: 17850 },
  { symbol: 'GOOGL', name: 'Alphabet Inc.', shares: 50, price: 141.20, change: -1.20, value: 7060 },
  { symbol: 'MSFT', name: 'Microsoft Corp.', shares: 75, price: 378.90, change: 4.50, value: 28417.50 },
  { symbol: 'AMZN', name: 'Amazon.com Inc.', shares: 30, price: 178.25, change: 1.85, value: 5347.50 },
  { symbol: 'NVDA', name: 'NVIDIA Corp.', shares: 40, price: 875.30, change: 15.20, value: 35012 },
];

const SAMPLE_CHART_DATA = [
  { date: 'Jan', value: 4000, volume: 2400 },
  { date: 'Feb', value: 3000, volume: 1398 },
  { date: 'Mar', value: 2000, volume: 9800 },
  { date: 'Apr', value: 2780, volume: 3908 },
  { date: 'May', value: 1890, volume: 4800 },
  { date: 'Jun', value: 2390, volume: 3800 },
  { date: 'Jul', value: 3490, volume: 4300 },
];

const COLORS = ['#FFA500', '#4A90D9', '#10B981', '#EF4444', '#8B5CF6', '#F59E0B'];

// ============ Live Data Table ============
interface LiveDataTableProps {
  config?: LiveDataConfig;
  isPreview?: boolean;
}

export const LiveDataTable: React.FC<LiveDataTableProps> = ({ config, isPreview }) => {
  const [data, setData] = useState(SAMPLE_PORTFOLIO_DATA);
  const [isLoading, setIsLoading] = useState(false);

  const columns = config?.columns || ['symbol', 'name', 'shares', 'price', 'change', 'value'];

  const formatValue = (key: string, val: any) => {
    if (key === 'price' || key === 'value') return `$${val.toLocaleString()}`;
    if (key === 'change') return (
      <span className={val >= 0 ? 'text-green-500' : 'text-red-500'}>
        {val >= 0 ? '+' : ''}{val.toFixed(2)}%
      </span>
    );
    return val;
  };

  return (
    <div className="border border-gray-300 rounded overflow-hidden bg-white">
      <div className="flex items-center justify-between px-3 py-2 bg-gray-50 border-b">
        <div className="flex items-center gap-2">
          <Database size={14} className="text-[#FFA500]" />
          <span className="text-xs font-semibold text-gray-700">
            {config?.source === 'portfolio' ? 'Portfolio Holdings' :
             config?.source === 'watchlist' ? 'Watchlist' : 'Market Data'}
          </span>
        </div>
        {!isPreview && (
          <button className="p-1 hover:bg-gray-200 rounded">
            <RefreshCw size={12} className="text-gray-500" />
          </button>
        )}
      </div>
      <table className="w-full text-xs">
        <thead className="bg-gray-100">
          <tr>
            {columns.map(col => (
              <th key={col} className="px-3 py-2 text-left font-semibold text-gray-600 capitalize">
                {col}
              </th>
            ))}
          </tr>
        </thead>
        <tbody>
          {data.map((row, idx) => (
            <tr key={idx} className="border-t border-gray-100 hover:bg-gray-50">
              {columns.map(col => (
                <td key={col} className="px-3 py-2 text-gray-800">
                  {formatValue(col, (row as any)[col])}
                </td>
              ))}
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  );
};

// ============ KPI Card ============
interface KPICardProps {
  config: KPICardConfig;
}

export const KPICard: React.FC<KPICardProps> = ({ config }) => {
  const { title, value, change, changeType = 'percent', prefix = '', suffix = '', color = '#FFA500' } = config;

  const isPositive = (change || 0) >= 0;
  const TrendIcon = change === 0 ? Minus : isPositive ? TrendingUp : TrendingDown;

  return (
    <div className="bg-white border border-gray-200 rounded-lg p-4 shadow-sm">
      <p className="text-xs text-gray-500 mb-1">{title}</p>
      <div className="flex items-end justify-between">
        <p className="text-2xl font-bold" style={{ color }}>
          {prefix}{typeof value === 'number' ? value.toLocaleString() : value}{suffix}
        </p>
        {change !== undefined && (
          <div className={`flex items-center gap-1 text-xs ${isPositive ? 'text-green-500' : 'text-red-500'}`}>
            <TrendIcon size={14} />
            <span>{isPositive ? '+' : ''}{change}{changeType === 'percent' ? '%' : ''}</span>
          </div>
        )}
      </div>
    </div>
  );
};

// ============ KPI Cards Row ============
interface KPICardsRowProps {
  cards?: KPICardConfig[];
}

export const KPICardsRow: React.FC<KPICardsRowProps> = ({ cards }) => {
  const defaultCards: KPICardConfig[] = cards || [
    { title: 'Total Value', value: 93687, change: 5.2, prefix: '$' },
    { title: 'Daily P&L', value: 1250, change: 1.35, prefix: '$', color: '#10B981' },
    { title: 'Total Return', value: '18.5%', change: 3.2 },
    { title: 'Positions', value: 12, change: 0 },
  ];

  return (
    <div className="grid grid-cols-4 gap-3">
      {defaultCards.map((card, idx) => (
        <KPICard key={idx} config={card} />
      ))}
    </div>
  );
};

// ============ Sparkline ============
interface SparklineProps {
  config?: SparklineConfig;
  inline?: boolean;
}

export const Sparkline: React.FC<SparklineProps> = ({ config, inline }) => {
  const data = (config?.data || [45, 52, 38, 45, 19, 23, 25, 30, 35, 40, 45, 50]).map((v, i) => ({ v }));
  const color = config?.color || '#FFA500';
  const height = config?.height || (inline ? 20 : 40);

  return (
    <div style={{ width: inline ? 80 : '100%', height }}>
      <ResponsiveContainer width="100%" height="100%">
        <AreaChart data={data} margin={{ top: 0, right: 0, left: 0, bottom: 0 }}>
          <defs>
            <linearGradient id={`sparkGrad-${color}`} x1="0" y1="0" x2="0" y2="1">
              <stop offset="5%" stopColor={color} stopOpacity={0.3} />
              <stop offset="95%" stopColor={color} stopOpacity={0} />
            </linearGradient>
          </defs>
          <Area
            type="monotone"
            dataKey="v"
            stroke={color}
            strokeWidth={1.5}
            fill={config?.showArea !== false ? `url(#sparkGrad-${color})` : 'none'}
          />
        </AreaChart>
      </ResponsiveContainer>
    </div>
  );
};

// ============ Dynamic Chart ============
interface DynamicChartProps {
  config?: DynamicChartConfig;
}

export const DynamicChart: React.FC<DynamicChartProps> = ({ config }) => {
  const type = config?.type || 'line';
  const data = config?.data || SAMPLE_CHART_DATA;
  const xKey = config?.xKey || 'date';
  const yKey = config?.yKey || 'value';
  const colors = config?.colors || COLORS;
  const height = config?.height || 200;

  const renderChart = () => {
    switch (type) {
      case 'bar':
        return (
          <BarChart data={data}>
            <XAxis dataKey={xKey} tick={{ fontSize: 10 }} />
            <YAxis tick={{ fontSize: 10 }} />
            <Tooltip />
            <Bar dataKey={yKey} fill={colors[0]} radius={[4, 4, 0, 0]} />
          </BarChart>
        );
      case 'area':
        return (
          <AreaChart data={data}>
            <defs>
              <linearGradient id="areaGrad" x1="0" y1="0" x2="0" y2="1">
                <stop offset="5%" stopColor={colors[0]} stopOpacity={0.3} />
                <stop offset="95%" stopColor={colors[0]} stopOpacity={0} />
              </linearGradient>
            </defs>
            <XAxis dataKey={xKey} tick={{ fontSize: 10 }} />
            <YAxis tick={{ fontSize: 10 }} />
            <Tooltip />
            <Area type="monotone" dataKey={yKey} stroke={colors[0]} fill="url(#areaGrad)" />
          </AreaChart>
        );
      case 'pie':
        return (
          <PieChart>
            <Pie
              data={data}
              dataKey={yKey}
              nameKey={xKey}
              cx="50%"
              cy="50%"
              outerRadius={60}
              label={({ name, percent }) => `${name} ${(percent * 100).toFixed(0)}%`}
              labelLine={false}
            >
              {data.map((_, idx) => (
                <Cell key={idx} fill={colors[idx % colors.length]} />
              ))}
            </Pie>
            <Tooltip />
          </PieChart>
        );
      case 'line':
      default:
        return (
          <LineChart data={data}>
            <XAxis dataKey={xKey} tick={{ fontSize: 10 }} />
            <YAxis tick={{ fontSize: 10 }} />
            <Tooltip />
            <Line type="monotone" dataKey={yKey} stroke={colors[0]} strokeWidth={2} dot={false} />
          </LineChart>
        );
    }
  };

  return (
    <div className="bg-white border border-gray-200 rounded p-4">
      <ResponsiveContainer width="100%" height={height}>
        {renderChart()}
      </ResponsiveContainer>
    </div>
  );
};

// ============ Financial Metrics Widget ============
interface FinancialMetricsProps {
  ticker?: string;
}

export const FinancialMetricsWidget: React.FC<FinancialMetricsProps> = ({ ticker = 'AAPL' }) => {
  const metrics = [
    { label: 'P/E Ratio', value: '28.5x' },
    { label: 'EPS', value: '$6.14' },
    { label: 'Market Cap', value: '$2.8T' },
    { label: 'Dividend Yield', value: '0.51%' },
    { label: 'Beta', value: '1.28' },
    { label: '52W High', value: '$199.62' },
    { label: '52W Low', value: '$124.17' },
    { label: 'Avg Volume', value: '58.2M' },
  ];

  return (
    <div className="bg-white border border-gray-200 rounded overflow-hidden">
      <div className="px-3 py-2 bg-gray-50 border-b flex items-center justify-between">
        <span className="text-xs font-semibold text-gray-700">{ticker} - Key Metrics</span>
        <span className="text-[10px] text-gray-500">Real-time</span>
      </div>
      <div className="grid grid-cols-4 gap-px bg-gray-200">
        {metrics.map((m, idx) => (
          <div key={idx} className="bg-white p-3 text-center">
            <p className="text-[10px] text-gray-500 mb-1">{m.label}</p>
            <p className="text-sm font-semibold text-gray-800">{m.value}</p>
          </div>
        ))}
      </div>
    </div>
  );
};

// ============ Chart Data Editor ============
interface ChartDataEditorProps {
  data: any[];
  onChange: (data: any[]) => void;
  columns: string[];
}

export const ChartDataEditor: React.FC<ChartDataEditorProps> = ({ data, onChange, columns }) => {
  const [csvInput, setCsvInput] = useState('');

  const handleCsvImport = () => {
    try {
      const lines = csvInput.trim().split('\n');
      const headers = lines[0].split(',').map(h => h.trim());
      const rows = lines.slice(1).map(line => {
        const values = line.split(',').map(v => v.trim());
        const row: any = {};
        headers.forEach((h, i) => {
          row[h] = isNaN(Number(values[i])) ? values[i] : Number(values[i]);
        });
        return row;
      });
      onChange(rows);
      setCsvInput('');
    } catch (e) {
      console.error('CSV parse error:', e);
    }
  };

  const addRow = () => {
    const newRow: any = {};
    columns.forEach(col => { newRow[col] = ''; });
    onChange([...data, newRow]);
  };

  const updateCell = (rowIdx: number, col: string, value: string) => {
    const newData = [...data];
    newData[rowIdx][col] = isNaN(Number(value)) ? value : Number(value);
    onChange(newData);
  };

  const deleteRow = (idx: number) => {
    onChange(data.filter((_, i) => i !== idx));
  };

  return (
    <div className="space-y-3">
      <div className="text-xs font-semibold text-gray-600">Chart Data</div>

      {/* Table Editor */}
      <div className="border border-gray-300 rounded overflow-hidden">
        <table className="w-full text-xs">
          <thead className="bg-gray-100">
            <tr>
              {columns.map(col => (
                <th key={col} className="px-2 py-1 text-left">{col}</th>
              ))}
              <th className="w-8"></th>
            </tr>
          </thead>
          <tbody>
            {data.map((row, rowIdx) => (
              <tr key={rowIdx} className="border-t">
                {columns.map(col => (
                  <td key={col} className="px-1 py-1">
                    <input
                      type="text"
                      value={row[col] || ''}
                      onChange={(e) => updateCell(rowIdx, col, e.target.value)}
                      className="w-full px-1 py-0.5 text-xs border border-gray-200 rounded"
                    />
                  </td>
                ))}
                <td className="px-1">
                  <button
                    onClick={() => deleteRow(rowIdx)}
                    className="text-red-500 hover:text-red-700 text-xs"
                  >
                    ×
                  </button>
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>

      <button
        onClick={addRow}
        className="text-xs text-[#FFA500] hover:underline"
      >
        + Add Row
      </button>

      {/* CSV Import */}
      <div className="pt-2 border-t">
        <label className="text-[10px] text-gray-500 block mb-1">Import CSV</label>
        <textarea
          value={csvInput}
          onChange={(e) => setCsvInput(e.target.value)}
          placeholder="date,value&#10;Jan,100&#10;Feb,150"
          className="w-full h-16 text-xs p-2 border border-gray-300 rounded resize-none"
        />
        <button
          onClick={handleCsvImport}
          className="mt-1 text-xs px-2 py-1 bg-[#FFA500] text-black rounded"
        >
          Import
        </button>
      </div>
    </div>
  );
};

export default {
  LiveDataTable,
  KPICard,
  KPICardsRow,
  Sparkline,
  DynamicChart,
  FinancialMetricsWidget,
  ChartDataEditor,
};
