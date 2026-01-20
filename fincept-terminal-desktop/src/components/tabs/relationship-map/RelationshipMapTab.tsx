import React, { useState, useCallback, useMemo, useEffect, memo } from 'react';
import { useTranslation } from 'react-i18next';
import ReactFlow, {
  Node,
  Edge,
  Background,
  Controls,
  MiniMap,
  useNodesState,
  useEdgesState,
  MarkerType,
} from 'reactflow';
import 'reactflow/dist/style.css';
import { Search, Globe } from 'lucide-react';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { TabFooter } from '@/components/common/TabFooter';
import { relationshipMapService } from '@/services/geopolitics/relationshipMapService';

import { CompanyNode, DataNode } from './components';
import { NODE_POSITIONS, EDGE_CONFIG, FLOW_CONFIG, BACKGROUND_CONFIG, DEFAULT_TICKER } from './constants';
import { formatMoney, formatPercent, formatNumber, formatDate } from './utils';
import type { NodeColors, GraphData } from './types';

// Define nodeTypes outside component to prevent recreation
const nodeTypes = {
  company: CompanyNode,
  data: DataNode,
};

// Build graph nodes and edges from data
const buildGraph = (data: GraphData, colors: NodeColors) => {
  const newNodes: Node[] = [];
  const newEdges: Edge[] = [];

  // Center: Company node
  newNodes.push({
    id: 'company',
    type: 'company',
    position: NODE_POSITIONS.company,
    data: {
      ...data.company,
      colors,
    },
  });

  // Helper to add a data node with edge
  const addDataNode = (
    id: string,
    type: string,
    title: string,
    items: { label: string; value: string; url?: string }[],
    edgeColor: string
  ) => {
    const position = NODE_POSITIONS[id as keyof typeof NODE_POSITIONS];
    if (!position) return;

    newNodes.push({
      id,
      type: 'data',
      position,
      data: {
        type,
        title,
        items,
        colors,
      },
    });

    newEdges.push({
      id: `e-${id}`,
      source: 'company',
      target: id,
      type: EDGE_CONFIG.type,
      animated: EDGE_CONFIG.animated,
      style: { stroke: edgeColor, strokeWidth: EDGE_CONFIG.strokeWidth },
      markerEnd: { type: MarkerType.ArrowClosed, color: edgeColor },
    });
  };

  // 1. Corporate Events
  const eventItems =
    data.corporate_events && data.corporate_events.length > 0
      ? data.corporate_events.map((e) => ({
          label: formatDate(e.filing_date),
          value: e.items?.[0] || 'Event',
          url: e.filing_url,
        }))
      : [{ label: 'No events available', value: 'Loading...' }];

  addDataNode('events', 'events', 'Corporate Events', eventItems, '#ef4444');

  // 2. Recent SEC Filings
  if (data.filings && data.filings.length > 0) {
    addDataNode(
      'filings',
      'filings',
      'Recent SEC Filings',
      data.filings.map((f) => ({
        label: `${f.form} (${f.description || ''})`,
        value: formatDate(f.filing_date),
      })),
      '#3b82f6'
    );
  }

  // 3. Financials
  if (data.financials) {
    addDataNode(
      'financials',
      'financials',
      'Financial Metrics',
      [
        { label: 'Revenue', value: formatMoney(data.financials.revenue) },
        { label: 'Gross Profit', value: formatMoney(data.financials.gross_profit) },
        { label: 'Operating CF', value: formatMoney(data.financials.operating_cashflow) },
        { label: 'Free CF', value: formatMoney(data.financials.free_cashflow) },
        { label: 'EBITDA Margin', value: formatPercent(data.financials.ebitda_margins) },
        { label: 'Profit Margin', value: formatPercent(data.financials.profit_margins) },
        { label: 'Revenue Growth', value: formatPercent(data.financials.revenue_growth) },
        { label: 'Earnings Growth', value: formatPercent(data.financials.earnings_growth) },
      ],
      '#10b981'
    );
  }

  // 4. Balance Sheet
  if (data.balance_sheet) {
    addDataNode(
      'balance',
      'balance',
      'Balance Sheet',
      [
        { label: 'Total Assets', value: formatMoney(data.balance_sheet.total_assets) },
        { label: 'Total Liabilities', value: formatMoney(data.balance_sheet.total_liabilities) },
        { label: 'Equity', value: formatMoney(data.balance_sheet.stockholders_equity) },
        { label: 'Cash', value: formatMoney(data.balance_sheet.total_cash) },
        { label: 'Debt', value: formatMoney(data.balance_sheet.total_debt) },
        { label: 'Shares Out', value: formatNumber(data.balance_sheet.shares_outstanding) },
      ],
      colors.accent
    );
  }

  // 5. Analysts
  if (data.analysts) {
    addDataNode(
      'analysts',
      'analysts',
      'Analyst Coverage',
      [
        { label: 'Recommendation', value: data.analysts.recommendation.toUpperCase() },
        {
          label: 'Rating Score',
          value: data.analysts.recommendation_mean ? data.analysts.recommendation_mean.toFixed(2) : 'N/A',
        },
        { label: 'Target Price', value: formatMoney(data.analysts.target_price) },
        { label: 'Target High', value: formatMoney(data.analysts.target_high) },
        { label: 'Target Low', value: formatMoney(data.analysts.target_low) },
        { label: 'Analysts', value: String(data.analysts.analyst_count) },
      ],
      '#8b5cf6'
    );
  }

  // 6. Ownership
  if (data.ownership) {
    addDataNode(
      'ownership',
      'ownership',
      'Ownership Structure',
      [
        { label: 'Insider %', value: `${data.ownership.insider_percent.toFixed(2)}%` },
        { label: 'Institutional %', value: `${data.ownership.institutional_percent.toFixed(2)}%` },
        { label: 'Float Shares', value: formatNumber(data.ownership.float_shares) },
        { label: 'Total Shares', value: formatNumber(data.ownership.shares_outstanding) },
      ],
      '#f59e0b'
    );
  }

  // 7. Valuation
  if (data.valuation) {
    addDataNode(
      'valuation',
      'valuation',
      'Valuation Metrics',
      [
        { label: 'P/E Ratio', value: data.valuation.pe_ratio.toFixed(2) },
        { label: 'Forward P/E', value: data.valuation.forward_pe.toFixed(2) },
        { label: 'Price/Book', value: data.valuation.price_to_book.toFixed(2) },
        { label: 'Enterprise Value', value: formatMoney(data.valuation.enterprise_value) },
        { label: 'EV/Revenue', value: data.valuation.enterprise_to_revenue.toFixed(2) },
        { label: 'PEG Ratio', value: data.valuation.peg_ratio.toFixed(2) },
      ],
      '#ec4899'
    );
  }

  // 8. Company Info
  addDataNode(
    'info',
    'info',
    'Company Information',
    [
      { label: 'Exchange', value: data.company.exchange },
      { label: 'Sector', value: data.company.sector },
      { label: 'Industry', value: data.company.industry },
      { label: 'Employees', value: data.company.employees ? data.company.employees.toLocaleString() : 'N/A' },
      { label: 'CIK', value: data.company.cik },
    ],
    '#14b8a6'
  );

  // 9. Executives
  if (data.executives && data.executives.length > 0) {
    addDataNode(
      'executives',
      'executives',
      'Executives',
      data.executives.map((exec) => ({
        label: exec.name,
        value: exec.role,
      })),
      '#10b981'
    );
  }

  return { nodes: newNodes, edges: newEdges };
};

const RelationshipMapTab: React.FC = () => {
  const { t } = useTranslation('relationshipMap');
  const { colors, fontSize, fontFamily } = useTerminalTheme();
  const [searchQuery, setSearchQuery] = useState('');
  const [selectedSymbol, setSelectedSymbol] = useState(DEFAULT_TICKER);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const [nodes, setNodes, onNodesChange] = useNodesState([]);
  const [edges, setEdges, onEdgesChange] = useEdgesState([]);

  // Memoize node colors to pass to child components
  const nodeColors: NodeColors = useMemo(
    () => ({
      accent: colors.accent,
      text: colors.text,
      textMuted: colors.textMuted,
      success: colors.success,
      alert: colors.alert,
      panel: colors.panel,
    }),
    [colors.accent, colors.text, colors.textMuted, colors.success, colors.alert, colors.panel]
  );

  const fetchRelationshipData = useCallback(
    async (ticker: string) => {
      setIsLoading(true);
      setError(null);
      try {
        const data = await relationshipMapService.getRelationshipMap(ticker);
        const { nodes: newNodes, edges: newEdges } = buildGraph(data as unknown as GraphData, nodeColors);
        setNodes(newNodes);
        setEdges(newEdges);
      } catch (err) {
        console.error('[UI] Error:', err);
        setError(`Failed to load data for ${ticker}`);
      } finally {
        setIsLoading(false);
      }
    },
    [nodeColors, setNodes, setEdges]
  );

  useEffect(() => {
    fetchRelationshipData(selectedSymbol);
  }, []);

  const handleSearch = useCallback(
    async (e: React.FormEvent) => {
      e.preventDefault();
      if (!searchQuery.trim()) return;

      const ticker = searchQuery.toUpperCase().trim();
      setSelectedSymbol(ticker);
      await fetchRelationshipData(ticker);
    },
    [searchQuery, fetchRelationshipData]
  );

  // Memoize container style
  const containerStyle = useMemo(
    () => ({
      fontFamily,
      fontSize: `${fontSize}px`,
      backgroundColor: colors.background,
      color: colors.text,
    }),
    [fontFamily, fontSize, colors.background, colors.text]
  );

  // Memoize header border style
  const headerStyle = useMemo(
    () => ({ borderColor: colors.textMuted }),
    [colors.textMuted]
  );

  // Memoize ticker badge style
  const badgeStyle = useMemo(
    () => ({
      backgroundColor: colors.panel,
      color: colors.accent,
      border: `1px solid ${colors.textMuted}`,
    }),
    [colors.panel, colors.accent, colors.textMuted]
  );

  // Memoize input style
  const inputStyle = useMemo(
    () => ({
      backgroundColor: colors.panel,
      borderColor: colors.textMuted,
      color: colors.text,
    }),
    [colors.panel, colors.textMuted, colors.text]
  );

  // Memoize minimap node color function
  const minimapNodeColor = useCallback(
    (node: Node) => {
      if (node.id === 'company') return colors.accent;
      return colors.textMuted;
    },
    [colors.accent, colors.textMuted]
  );

  // Memoize controls style
  const controlsStyle = useMemo(
    () => ({
      background: '#1a1a1a',
      border: `1px solid ${colors.textMuted}`,
      borderRadius: '4px',
    }),
    [colors.textMuted]
  );

  // Memoize minimap style
  const minimapStyle = useMemo(
    () => ({
      background: '#1a1a1a',
      border: `1px solid ${colors.textMuted}`,
    }),
    [colors.textMuted]
  );

  return (
    <div className="h-full flex flex-col" style={containerStyle}>
      {/* Header */}
      <div className="border-b px-4 py-3 flex items-center justify-between" style={headerStyle}>
        <div className="flex items-center gap-3">
          <Globe className="w-5 h-5" style={{ color: colors.accent }} />
          <h2 className="text-lg font-semibold">{t('title')}</h2>
          <span className="text-xs px-2 py-1 rounded font-mono" style={badgeStyle}>
            {selectedSymbol}
          </span>
        </div>

        <form onSubmit={handleSearch} className="relative w-96">
          <Search
            className="absolute left-3 top-1/2 transform -translate-y-1/2 w-4 h-4"
            style={{ color: colors.textMuted }}
          />
          <input
            type="text"
            value={searchQuery}
            onChange={(e) => setSearchQuery(e.target.value)}
            placeholder={t('search.placeholder')}
            className="w-full pl-10 pr-4 py-2 rounded border focus:outline-none focus:ring-1"
            style={inputStyle}
          />
        </form>
      </div>

      {/* Graph */}
      <div className="flex-1" style={{ background: '#0a0a0a' }}>
        {isLoading ? (
          <div className="flex items-center justify-center h-full">
            <div className="text-center">
              <div
                className="animate-spin rounded-full h-16 w-16 border-b-2 mx-auto mb-4"
                style={{ borderColor: colors.accent }}
              />
              <p className="text-lg font-semibold" style={{ color: colors.text }}>
                {t('loading.title')} {selectedSymbol}...
              </p>
              <p className="text-sm mt-2" style={{ color: colors.textMuted }}>
                {t('loading.subtitle')}
              </p>
            </div>
          </div>
        ) : error ? (
          <div className="flex items-center justify-center h-full">
            <div
              className="text-center p-8 rounded border"
              style={{
                backgroundColor: colors.panel,
                borderColor: colors.alert,
              }}
            >
              <p className="text-lg font-semibold mb-2" style={{ color: colors.alert }}>
                {t('error.title')}
              </p>
              <p style={{ color: colors.textMuted }}>{error}</p>
            </div>
          </div>
        ) : (
          <ReactFlow
            nodes={nodes}
            edges={edges}
            onNodesChange={onNodesChange}
            onEdgesChange={onEdgesChange}
            nodeTypes={nodeTypes}
            fitView={FLOW_CONFIG.fitView}
            minZoom={FLOW_CONFIG.minZoom}
            maxZoom={FLOW_CONFIG.maxZoom}
            defaultViewport={FLOW_CONFIG.defaultViewport}
            style={BACKGROUND_CONFIG.style}
          >
            <Background
              gap={BACKGROUND_CONFIG.gap}
              size={BACKGROUND_CONFIG.size}
              style={BACKGROUND_CONFIG.style}
              color={BACKGROUND_CONFIG.color}
            />
            <Controls style={controlsStyle} />
            <MiniMap style={minimapStyle} maskColor="#0a0a0a90" nodeColor={minimapNodeColor} />
          </ReactFlow>
        )}
      </div>

      <TabFooter tabName="Relationship Map" />
    </div>
  );
};

export default memo(RelationshipMapTab);
