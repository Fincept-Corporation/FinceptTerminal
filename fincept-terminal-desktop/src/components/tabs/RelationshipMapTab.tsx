import React, { useState, useCallback, useMemo, useEffect } from 'react';
import { useTranslation } from 'react-i18next';
import ReactFlow, {
  Node,
  Edge,
  Background,
  Controls,
  MiniMap,
  useNodesState,
  useEdgesState,
  Handle,
  Position,
  MarkerType,
} from 'reactflow';
import 'reactflow/dist/style.css';
import { Search, Building2, Users, TrendingUp, Calendar, BarChart3, Globe, DollarSign, FileText, Award, Activity, Briefcase, PieChart, ExternalLink } from 'lucide-react';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { TabFooter } from '@/components/common/TabFooter';
import { relationshipMapService } from '@/services/geopolitics/relationshipMapService';
import { open } from '@tauri-apps/plugin-shell';

// ============================================================================
// CUSTOM NODE COMPONENTS
// ============================================================================

const CompanyNode = ({ data }: any) => {
  const { colors } = useTerminalTheme();

  const formatMarketCap = (val: number) => {
    if (!val) return 'N/A';
    if (val >= 1e12) return `$${(val/1e12).toFixed(2)}T`;
    if (val >= 1e9) return `$${(val/1e9).toFixed(2)}B`;
    if (val >= 1e6) return `$${(val/1e6).toFixed(2)}M`;
    return `$${val}`;
  };

  return (
    <div
      style={{
        background: 'linear-gradient(135deg, #1a1a1a 0%, #2d2d2d 100%)',
        border: `3px solid ${colors.accent}`,
        borderRadius: '12px',
        padding: '24px',
        minWidth: '320px',
        maxWidth: '400px',
        boxShadow: `0 0 40px ${colors.accent}80, 0 10px 30px rgba(0,0,0,0.7)`,
      }}
    >
      <Handle type="target" position={Position.Top} style={{ opacity: 0 }} />
      <div className="flex flex-col items-center text-center">
        <Building2 className="w-12 h-12 mb-3" style={{ color: colors.accent }} />
        <h2 className="text-2xl font-bold mb-1" style={{ color: colors.text }}>{data.name}</h2>
        <div className="text-xl mb-2 font-mono" style={{ color: colors.accent }}>{data.ticker}</div>
        <div className="text-sm mb-1" style={{ color: colors.textMuted }}>
          {data.sector} • {data.industry}
        </div>
        <div className="text-sm font-semibold mt-2" style={{ color: colors.success }}>
          Market Cap: {formatMarketCap(data.market_cap)}
        </div>
        <div className="text-xs mt-2 opacity-60">
          CIK: {data.cik} | {data.exchange}
        </div>
        {data.employees && (
          <div className="text-xs mt-1" style={{ color: colors.textMuted }}>
            {data.employees.toLocaleString()} employees
          </div>
        )}
      </div>
      <Handle type="source" position={Position.Bottom} style={{ opacity: 0 }} />
      <Handle type="source" position={Position.Left} style={{ opacity: 0 }} />
      <Handle type="source" position={Position.Right} style={{ opacity: 0 }} />
    </div>
  );
};

const DataNode = ({ data }: any) => {
  const { colors } = useTerminalTheme();
  const [expanded, setExpanded] = useState(false);

  const getIcon = () => {
    switch (data.type) {
      case 'events': return <Activity className="w-5 h-5" />;
      case 'filings': return <Calendar className="w-5 h-5" />;
      case 'financials': return <DollarSign className="w-5 h-5" />;
      case 'balance': return <BarChart3 className="w-5 h-5" />;
      case 'analysts': return <Award className="w-5 h-5" />;
      case 'ownership': return <PieChart className="w-5 h-5" />;
      case 'valuation': return <TrendingUp className="w-5 h-5" />;
      case 'info': return <Briefcase className="w-5 h-5" />;
      case 'executives': return <Users className="w-5 h-5" />;
      default: return <FileText className="w-5 h-5" />;
    }
  };

  const getColor = () => {
    switch (data.type) {
      case 'events': return '#ef4444';
      case 'filings': return '#3b82f6';
      case 'financials': return '#10b981';
      case 'balance': return colors.accent;
      case 'analysts': return '#8b5cf6';
      case 'ownership': return '#f59e0b';
      case 'valuation': return '#ec4899';
      case 'info': return '#14b8a6';
      case 'executives': return '#10b981';
      default: return colors.textMuted;
    }
  };

  const displayItems = expanded ? data.items : data.items?.slice(0, 4);
  const hasClickableItems = data.type === 'events' || data.type === 'filings';

  const handleItemClick = async (e: React.MouseEvent, item: any) => {
    e.stopPropagation();
    if (hasClickableItems && item.url) {
      await open(item.url);
    }
  };

  return (
    <div
      style={{
        background: '#1a1a1a',
        border: `2px solid ${getColor()}`,
        borderRadius: '8px',
        padding: '14px 18px',
        minWidth: '260px',
        maxWidth: '320px',
        boxShadow: `0 0 20px ${getColor()}50, 0 4px 12px rgba(0,0,0,0.5)`,
        cursor: 'pointer',
        transition: 'all 0.2s',
      }}
      onClick={() => data.items?.length > 4 && setExpanded(!expanded)}
      onMouseEnter={(e) => {
        e.currentTarget.style.transform = 'scale(1.02)';
        e.currentTarget.style.boxShadow = `0 0 30px ${getColor()}70, 0 6px 16px rgba(0,0,0,0.6)`;
      }}
      onMouseLeave={(e) => {
        e.currentTarget.style.transform = 'scale(1)';
        e.currentTarget.style.boxShadow = `0 0 20px ${getColor()}50, 0 4px 12px rgba(0,0,0,0.5)`;
      }}
    >
      <Handle type="target" position={Position.Top} style={{ background: getColor() }} />

      <div className="flex items-center gap-2 mb-3">
        <div style={{ color: getColor() }}>{getIcon()}</div>
        <h4 className="font-semibold text-sm" style={{ color: colors.text }}>{data.title}</h4>
        {data.items?.length > 4 && (
          <span className="text-xs ml-auto px-1.5 py-0.5 rounded" style={{
            backgroundColor: `${getColor()}20`,
            color: getColor()
          }}>
            {data.items.length}
          </span>
        )}
      </div>

      <div className="space-y-1.5 text-xs">
        {displayItems && displayItems.map((item: any, idx: number) => (
          <div
            key={idx}
            className="flex justify-between gap-3 hover:bg-white/5 rounded px-1 py-0.5 transition-colors"
            onClick={(e) => handleItemClick(e, item)}
            style={{ cursor: hasClickableItems && item.url ? 'pointer' : 'default' }}
          >
            <span className="truncate" style={{ color: colors.textMuted }}>{item.label}</span>
            <span className="font-medium text-right flex items-center gap-1" style={{ color: colors.text }}>
              {item.value}
              {hasClickableItems && item.url && <ExternalLink className="w-3 h-3 opacity-50" />}
            </span>
          </div>
        ))}
        {!expanded && data.items?.length > 4 && (
          <div className="text-center pt-1 text-xs font-semibold" style={{ color: getColor() }}>
            Click to see +{data.items.length - 4} more
          </div>
        )}
      </div>

      <Handle type="source" position={Position.Bottom} style={{ background: getColor(), opacity: 0 }} />
    </div>
  );
};

// ============================================================================
// MAIN COMPONENT
// ============================================================================

// Define nodeTypes outside component to avoid re-creation on every render
const nodeTypes = {
  company: CompanyNode,
  data: DataNode,
};

const RelationshipMapTab: React.FC = () => {
  const { t } = useTranslation('relationshipMap');
  const { colors, fontSize, fontFamily } = useTerminalTheme();
  const [searchQuery, setSearchQuery] = useState('');
  const [selectedSymbol, setSelectedSymbol] = useState('AAPL');
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const [nodes, setNodes, onNodesChange] = useNodesState([]);
  const [edges, setEdges, onEdgesChange] = useEdgesState([]);

  useEffect(() => {
    fetchRelationshipData(selectedSymbol);
  }, []);

  const fetchRelationshipData = async (ticker: string) => {
    setIsLoading(true);
    setError(null);
    try {
      const data = await relationshipMapService.getRelationshipMap(ticker);
      updateGraph(data);
    } catch (err) {
      console.error('[UI] Error:', err);
      setError(`Failed to load data for ${ticker}`);
    } finally {
      setIsLoading(false);
    }
  };

  const updateGraph = (data: any) => {
    const formatMoney = (val: number) => {
      if (!val || val === 0) return '$0';
      if (val >= 1e9) return `$${(val/1e9).toFixed(2)}B`;
      if (val >= 1e6) return `$${(val/1e6).toFixed(2)}M`;
      if (val >= 1e3) return `$${(val/1e3).toFixed(2)}K`;
      return `$${val.toFixed(2)}`;
    };

    const formatPercent = (val: number) => {
      if (!val && val !== 0) return 'N/A';
      return `${(val * 100).toFixed(2)}%`;
    };

    const formatNumber = (val: number) => {
      if (!val && val !== 0) return 'N/A';
      if (val >= 1e9) return `${(val/1e9).toFixed(2)}B`;
      if (val >= 1e6) return `${(val/1e6).toFixed(2)}M`;
      return val.toFixed(2);
    };

    const newNodes: Node[] = [
      // Center: Company
      {
        id: 'company',
        type: 'company',
        position: { x: 600, y: 350 },
        data: {
          name: data.company.name,
          ticker: data.company.ticker,
          industry: data.company.industry,
          sector: data.company.sector,
          cik: data.company.cik,
          exchange: data.company.exchange,
          employees: data.company.employees,
          market_cap: data.company.market_cap,
        },
      },
    ];

    const newEdges: Edge[] = [];

    // 1. Corporate Events (Top-Left) - 8-K Filings with Major Announcements
    const corporateEventItems = data.corporate_events && data.corporate_events.length > 0
      ? data.corporate_events.map((e: any) => {
          const date = new Date(e.filing_date).toLocaleDateString('en-US', { month: 'short', day: 'numeric', year: '2-digit' });
          // Get the first item code or use "Event"
          const itemCode = e.items && e.items.length > 0 ? e.items[0] : 'Event';
          return {
            label: date,
            value: itemCode,
            description: e.description || 'Material corporate event filed with SEC',
            url: e.filing_url,
            allItems: e.items || []
          };
        })
      : [{ label: 'No events available', value: 'Loading...' }];

    newNodes.push({
      id: 'events',
      type: 'data',
      position: { x: 50, y: 50 },
      data: {
        type: 'events',
        title: 'Corporate Events',
        items: corporateEventItems,
      },
    });
    newEdges.push({
      id: 'e-events',
      source: 'company',
      target: 'events',
      type: 'smoothstep',
      animated: true,
      style: { stroke: '#ef4444', strokeWidth: 2 },
      markerEnd: { type: MarkerType.ArrowClosed, color: '#ef4444' },
    });

    // 2. Recent SEC Filings (Top-Left-Center)
    if (data.filings?.length > 0) {
      newNodes.push({
        id: 'filings',
        type: 'data',
        position: { x: 200, y: 80 },
        data: {
          type: 'filings',
          title: 'Recent SEC Filings',
          items: data.filings.map((f: any) => ({
            label: `${f.form} (${f.description})`,
            value: new Date(f.filing_date).toLocaleDateString('en-US', { month: 'short', day: 'numeric', year: '2-digit' }),
          })),
        },
      });
      newEdges.push({
        id: 'e-filings',
        source: 'company',
        target: 'filings',
        type: 'smoothstep',
        animated: true,
        style: { stroke: '#3b82f6', strokeWidth: 2 },
        markerEnd: { type: MarkerType.ArrowClosed, color: '#3b82f6' },
      });
    }

    // 3. Financials (Top-Center)
    if (data.financials) {
      newNodes.push({
        id: 'financials',
        type: 'data',
        position: { x: 500, y: 50 },
        data: {
          type: 'financials',
          title: 'Financial Metrics',
          items: [
            { label: 'Revenue', value: formatMoney(data.financials.revenue) },
            { label: 'Gross Profit', value: formatMoney(data.financials.gross_profit) },
            { label: 'Operating CF', value: formatMoney(data.financials.operating_cashflow) },
            { label: 'Free CF', value: formatMoney(data.financials.free_cashflow) },
            { label: 'EBITDA Margin', value: formatPercent(data.financials.ebitda_margins) },
            { label: 'Profit Margin', value: formatPercent(data.financials.profit_margins) },
            { label: 'Revenue Growth', value: formatPercent(data.financials.revenue_growth) },
            { label: 'Earnings Growth', value: formatPercent(data.financials.earnings_growth) },
          ],
        },
      });
      newEdges.push({
        id: 'e-financials',
        source: 'company',
        target: 'financials',
        type: 'smoothstep',
        animated: true,
        style: { stroke: '#10b981', strokeWidth: 2 },
        markerEnd: { type: MarkerType.ArrowClosed, color: '#10b981' },
      });
    }

    // 4. Balance Sheet (Top-Right)
    if (data.balance_sheet) {
      newNodes.push({
        id: 'balance',
        type: 'data',
        position: { x: 850, y: 80 },
        data: {
          type: 'balance',
          title: 'Balance Sheet',
          items: [
            { label: 'Total Assets', value: formatMoney(data.balance_sheet.total_assets) },
            { label: 'Total Liabilities', value: formatMoney(data.balance_sheet.total_liabilities) },
            { label: 'Equity', value: formatMoney(data.balance_sheet.stockholders_equity) },
            { label: 'Cash', value: formatMoney(data.balance_sheet.total_cash) },
            { label: 'Debt', value: formatMoney(data.balance_sheet.total_debt) },
            { label: 'Shares Out', value: formatNumber(data.balance_sheet.shares_outstanding) },
          ],
        },
      });
      newEdges.push({
        id: 'e-balance',
        source: 'company',
        target: 'balance',
        type: 'smoothstep',
        animated: true,
        style: { stroke: colors.accent, strokeWidth: 2 },
        markerEnd: { type: MarkerType.ArrowClosed, color: colors.accent },
      });
    }

    // 5. Analysts (Right)
    if (data.analysts) {
      newNodes.push({
        id: 'analysts',
        type: 'data',
        position: { x: 1100, y: 350 },
        data: {
          type: 'analysts',
          title: 'Analyst Coverage',
          items: [
            { label: 'Recommendation', value: data.analysts.recommendation.toUpperCase() },
            { label: 'Rating Score', value: data.analysts.recommendation_mean ? data.analysts.recommendation_mean.toFixed(2) : 'N/A' },
            { label: 'Target Price', value: formatMoney(data.analysts.target_price) },
            { label: 'Target High', value: formatMoney(data.analysts.target_high) },
            { label: 'Target Low', value: formatMoney(data.analysts.target_low) },
            { label: 'Analysts', value: String(data.analysts.analyst_count) },
          ],
        },
      });
      newEdges.push({
        id: 'e-analysts',
        source: 'company',
        target: 'analysts',
        type: 'smoothstep',
        animated: true,
        style: { stroke: '#8b5cf6', strokeWidth: 2 },
        markerEnd: { type: MarkerType.ArrowClosed, color: '#8b5cf6' },
      });
    }

    // 6. Ownership (Bottom-Right)
    if (data.ownership) {
      newNodes.push({
        id: 'ownership',
        type: 'data',
        position: { x: 850, y: 620 },
        data: {
          type: 'ownership',
          title: 'Ownership Structure',
          items: [
            { label: 'Insider %', value: `${data.ownership.insider_percent.toFixed(2)}%` },
            { label: 'Institutional %', value: `${data.ownership.institutional_percent.toFixed(2)}%` },
            { label: 'Float Shares', value: formatNumber(data.ownership.float_shares) },
            { label: 'Total Shares', value: formatNumber(data.ownership.shares_outstanding) },
          ],
        },
      });
      newEdges.push({
        id: 'e-ownership',
        source: 'company',
        target: 'ownership',
        type: 'smoothstep',
        animated: true,
        style: { stroke: '#f59e0b', strokeWidth: 2 },
        markerEnd: { type: MarkerType.ArrowClosed, color: '#f59e0b' },
      });
    }

    // 7. Valuation (Bottom-Center)
    if (data.valuation) {
      newNodes.push({
        id: 'valuation',
        type: 'data',
        position: { x: 450, y: 650 },
        data: {
          type: 'valuation',
          title: 'Valuation Metrics',
          items: [
            { label: 'P/E Ratio', value: data.valuation.pe_ratio.toFixed(2) },
            { label: 'Forward P/E', value: data.valuation.forward_pe.toFixed(2) },
            { label: 'Price/Book', value: data.valuation.price_to_book.toFixed(2) },
            { label: 'Enterprise Value', value: formatMoney(data.valuation.enterprise_value) },
            { label: 'EV/Revenue', value: data.valuation.enterprise_to_revenue.toFixed(2) },
            { label: 'PEG Ratio', value: data.valuation.peg_ratio.toFixed(2) },
          ],
        },
      });
      newEdges.push({
        id: 'e-valuation',
        source: 'company',
        target: 'valuation',
        type: 'smoothstep',
        animated: true,
        style: { stroke: '#ec4899', strokeWidth: 2 },
        markerEnd: { type: MarkerType.ArrowClosed, color: '#ec4899' },
      });
    }

    // 8. Company Info (Bottom-Left)
    newNodes.push({
      id: 'info',
      type: 'data',
      position: { x: 100, y: 620 },
      data: {
        type: 'info',
        title: 'Company Information',
        items: [
          { label: 'Exchange', value: data.company.exchange },
          { label: 'Sector', value: data.company.sector },
          { label: 'Industry', value: data.company.industry },
          { label: 'Employees', value: data.company.employees ? data.company.employees.toLocaleString() : 'N/A' },
          { label: 'SIC Code', value: data.company.sic },
        ],
      },
    });
    newEdges.push({
      id: 'e-info',
      source: 'company',
      target: 'info',
      type: 'smoothstep',
      animated: true,
      style: { stroke: '#14b8a6', strokeWidth: 2 },
      markerEnd: { type: MarkerType.ArrowClosed, color: '#14b8a6' },
    });

    // 9. Executives (Left) - if available
    if (data.executives?.length > 0) {
      newNodes.push({
        id: 'executives',
        type: 'data',
        position: { x: 50, y: 350 },
        data: {
          type: 'executives',
          title: 'Executives',
          items: data.executives.map((exec: any) => ({
            label: exec.name,
            value: exec.role,
          })),
        },
      });
      newEdges.push({
        id: 'e-executives',
        source: 'company',
        target: 'executives',
        type: 'smoothstep',
        animated: true,
        style: { stroke: '#10b981', strokeWidth: 2 },
        markerEnd: { type: MarkerType.ArrowClosed, color: '#10b981' },
      });
    }

    console.log('[UI] Generated nodes:', newNodes.length);
    console.log('[UI] Generated edges:', newEdges.length);

    setNodes(newNodes);
    setEdges(newEdges);
  };

  const handleSearch = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!searchQuery.trim()) return;

    const ticker = searchQuery.toUpperCase().trim();
    setSelectedSymbol(ticker);
    await fetchRelationshipData(ticker);
  };

  return (
    <div
      className="h-full flex flex-col"
      style={{
        fontFamily,
        fontSize: `${fontSize}px`,
        backgroundColor: colors.background,
        color: colors.text,
      }}
    >
      {/* Header */}
      <div
        className="border-b px-4 py-3 flex items-center justify-between"
        style={{ borderColor: colors.textMuted }}
      >
        <div className="flex items-center gap-3">
          <Globe className="w-5 h-5" style={{ color: colors.accent }} />
          <h2 className="text-lg font-semibold">{t('title')}</h2>
          <span className="text-xs px-2 py-1 rounded font-mono" style={{
            backgroundColor: colors.panel,
            color: colors.accent,
            border: `1px solid ${colors.textMuted}`
          }}>
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
            style={{
              backgroundColor: colors.panel,
              borderColor: colors.textMuted,
              color: colors.text,
            }}
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
            <div className="text-center p-8 rounded border" style={{
              backgroundColor: colors.panel,
              borderColor: colors.alert
            }}>
              <p className="text-lg font-semibold mb-2" style={{ color: colors.alert }}>{t('error.title')}</p>
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
            fitView
            minZoom={0.3}
            maxZoom={2}
            defaultViewport={{ x: 0, y: 0, zoom: 0.75 }}
            style={{ background: '#0a0a0a' }}
          >
            <Background
              gap={20}
              size={1}
              style={{ background: '#0a0a0a' }}
              color="#1a1a1a"
            />
            <Controls
              style={{
                background: '#1a1a1a',
                border: `1px solid ${colors.textMuted}`,
                borderRadius: '4px',
              }}
            />
            <MiniMap
              style={{
                background: '#1a1a1a',
                border: `1px solid ${colors.textMuted}`,
              }}
              maskColor="#0a0a0a90"
              nodeColor={(node) => {
                if (node.id === 'company') return colors.accent;
                return colors.textMuted;
              }}
            />
          </ReactFlow>
        )}
      </div>

      <TabFooter tabName="Relationship Map" />
    </div>
  );
};

export default RelationshipMapTab;
