// Relationship Map - Comprehensive Corporate Intelligence Visualization
// FINCEPT Design System compliant

import React, { useState, useMemo, useCallback } from 'react';
import ReactFlow, {
  Background,
  Controls,
  MiniMap,
  type Node,
  type Edge,
  type NodeTypes,
  type EdgeTypes,
} from 'reactflow';
import 'reactflow/dist/style.css';

import type {
  GraphNodeData,
  GraphEdgeData,
  LayoutMode,
  FilterState,
  RelationshipMapDataV2,
} from './types';
import { DEFAULT_FILTERS } from './types';
import { useRelationshipData } from './hooks/useRelationshipData';
import { applyLayout, selectOptimalLayout } from './utils/layoutAlgorithms';
import { FINCEPT, NODE_COLORS } from './constants';

import CompanyNode from './components/nodes/CompanyNode';
import PeerNode from './components/nodes/PeerNode';
import InstitutionalHolderNode from './components/nodes/InstitutionalHolderNode';
import FundFamilyNode from './components/nodes/FundFamilyNode';
import InsiderNode from './components/nodes/InsiderNode';
import EventNode from './components/nodes/EventNode';
import MetricsClusterNode from './components/nodes/MetricsClusterNode';
import SupplyChainNode from './components/nodes/SupplyChainNode';
import OwnershipEdge from './components/edges/OwnershipEdge';
import HeaderBar from './components/controls/HeaderBar';
import FilterPanel from './components/panels/FilterPanel';
import DetailPanel from './components/panels/DetailPanel';
import LegendPanel from './components/controls/LegendPanel';

const nodeTypes: NodeTypes = {
  company: CompanyNode,
  peer: PeerNode,
  institutional: InstitutionalHolderNode,
  fund_family: FundFamilyNode,
  insider: InsiderNode,
  event: EventNode,
  metrics: MetricsClusterNode,
  supply_chain: SupplyChainNode,
};

const edgeTypes: EdgeTypes = {
  ownership: OwnershipEdge,
  peer: OwnershipEdge,
  event: OwnershipEdge,
  internal: OwnershipEdge,
  supply_chain: OwnershipEdge,
  segment: OwnershipEdge,
  debt: OwnershipEdge,
};

function buildGraph(
  data: RelationshipMapDataV2,
  filters: FilterState
): { nodes: Node<GraphNodeData>[]; edges: Edge<GraphEdgeData>[] } {
  const nodes: Node<GraphNodeData>[] = [];
  const edges: Edge<GraphEdgeData>[] = [];
  const companyId = `company-${data.company.ticker}`;

  // 1. Company Node (always shown)
  nodes.push({
    id: companyId,
    type: 'company',
    position: { x: 0, y: 0 },
    data: {
      id: companyId,
      category: 'company',
      label: data.company.ticker,
      company: data.company,
      valuation: data.companyValuation,
      executives: data.executives,
    },
  });

  // 2. Peer Nodes
  if (filters.showPeers && data.peers.length > 0) {
    for (const peer of data.peers) {
      const peerVal = data.peerValuations.get(peer.ticker);
      if (filters.peerFilter === 'undervalued' && peerVal?.action !== 'BUY') continue;
      if (filters.peerFilter === 'overvalued' && peerVal?.action !== 'SELL') continue;

      const peerId = `peer-${peer.ticker}`;
      nodes.push({
        id: peerId,
        type: 'peer',
        position: { x: 0, y: 0 },
        data: { id: peerId, category: 'peer', label: peer.ticker, peer, peerValuation: peerVal },
      });
      edges.push({
        id: `edge-${companyId}-${peerId}`,
        source: companyId,
        target: peerId,
        type: 'peer',
        data: { category: 'peer', label: 'peer' },
      });
    }
  }

  // 3. Institutional Holders
  if (filters.showInstitutional && data.institutionalHolders.length > 0) {
    for (const holder of data.institutionalHolders) {
      if (holder.percentage < filters.minOwnership) continue;
      const holderId = `inst-${holder.name.replace(/\s+/g, '-').slice(0, 20)}`;
      nodes.push({
        id: holderId,
        type: 'institutional',
        position: { x: 0, y: 0 },
        data: { id: holderId, category: 'institutional', label: holder.name, holder },
      });
      edges.push({
        id: `edge-${holderId}-${companyId}`,
        source: holderId,
        target: companyId,
        type: 'ownership',
        data: { category: 'ownership', percentage: holder.percentage, shares: holder.shares, value: holder.value },
      });
    }
  }

  // 4. Fund Families
  if (filters.showFundFamilies && data.fundFamilies.length > 0) {
    for (const family of data.fundFamilies) {
      if (family.total_percentage < filters.minOwnership) continue;
      const familyId = `fund-${family.name.replace(/\s+/g, '-').slice(0, 20)}`;
      nodes.push({
        id: familyId,
        type: 'fund_family',
        position: { x: 0, y: 0 },
        data: { id: familyId, category: 'fund_family', label: family.name, fundFamily: family },
      });
      edges.push({
        id: `edge-${familyId}-${companyId}`,
        source: familyId,
        target: companyId,
        type: 'ownership',
        data: { category: 'ownership', percentage: family.total_percentage, value: family.total_value },
      });
    }
  }

  // 5. Insider Holders
  if (filters.showInsiders && data.insiderHolders.length > 0) {
    for (const insider of data.insiderHolders) {
      const insiderId = `insider-${insider.name.replace(/\s+/g, '-').slice(0, 20)}`;
      nodes.push({
        id: insiderId,
        type: 'insider',
        position: { x: 0, y: 0 },
        data: {
          id: insiderId,
          category: 'insider',
          label: insider.name,
          insider,
          transactions: data.insiderTransactions.filter(t =>
            t.name.toLowerCase().includes(insider.name.split(' ')[0].toLowerCase())
          ),
        },
      });
      edges.push({
        id: `edge-${insiderId}-${companyId}`,
        source: insiderId,
        target: companyId,
        type: 'internal',
        data: { category: 'internal' },
      });
    }
  }

  // 6. Corporate Events
  if (filters.showEvents && data.corporateEvents.length > 0) {
    for (let i = 0; i < data.corporateEvents.length; i++) {
      const event = data.corporateEvents[i];
      const eventId = `event-${i}`;
      nodes.push({
        id: eventId,
        type: 'event',
        position: { x: 0, y: 0 },
        data: { id: eventId, category: 'event', label: event.description || event.form, event },
      });
      edges.push({
        id: `edge-${companyId}-${eventId}`,
        source: companyId,
        target: eventId,
        type: 'event',
        data: { category: 'event' },
      });
    }
  }

  // 7. Metrics Clusters - always show if we have ANY company data
  if (filters.showMetrics) {
    const hasFinancials = data.company.revenue || data.company.gross_profit || data.company.ebitda || data.company.free_cashflow;
    if (hasFinancials) {
      const finId = 'metrics-financials';
      nodes.push({
        id: finId, type: 'metrics', position: { x: 0, y: 0 },
        data: {
          id: finId, category: 'metrics', label: 'Financials', metricsType: 'financials',
          metrics: {
            revenue: data.company.revenue,
            gross_profit: data.company.gross_profit,
            ebitda: data.company.ebitda,
            free_cashflow: data.company.free_cashflow,
            profit_margins: data.company.profit_margins,
            operating_margins: data.company.operating_margins,
          },
        },
      });
      edges.push({ id: `edge-${companyId}-${finId}`, source: companyId, target: finId, type: 'internal', data: { category: 'internal' } });
    }

    const hasValuation = data.company.pe_ratio || data.company.price_to_book || data.company.peg_ratio || data.company.enterprise_value;
    if (hasValuation) {
      const valId = 'metrics-valuation';
      nodes.push({
        id: valId, type: 'metrics', position: { x: 0, y: 0 },
        data: {
          id: valId, category: 'metrics', label: 'Valuation', metricsType: 'valuation',
          metrics: {
            pe_ratio: data.company.pe_ratio,
            forward_pe: data.company.forward_pe,
            price_to_book: data.company.price_to_book,
            peg_ratio: data.company.peg_ratio,
            enterprise_value: data.company.enterprise_value,
            ev_to_revenue: data.company.enterprise_to_revenue,
          },
        },
      });
      edges.push({ id: `edge-${companyId}-${valId}`, source: companyId, target: valId, type: 'internal', data: { category: 'internal' } });
    }

    const hasBalance = data.company.total_cash || data.company.total_debt || data.company.shares_outstanding;
    if (hasBalance) {
      const bsId = 'metrics-balance';
      nodes.push({
        id: bsId, type: 'metrics', position: { x: 0, y: 0 },
        data: {
          id: bsId, category: 'metrics', label: 'Balance Sheet', metricsType: 'balance_sheet',
          metrics: {
            total_cash: data.company.total_cash,
            total_debt: data.company.total_debt,
            shares_outstanding: data.company.shares_outstanding,
            float_shares: data.company.float_shares,
          },
        },
      });
      edges.push({ id: `edge-${companyId}-${bsId}`, source: companyId, target: bsId, type: 'internal', data: { category: 'internal' } });
    }
  }

  // 8. Supply Chain Nodes
  if (filters.showSupplyChain && data.supplyChain && data.supplyChain.length > 0) {
    for (let i = 0; i < data.supplyChain.length; i++) {
      const entity = data.supplyChain[i];
      const scId = `sc-${entity.name.replace(/\s+/g, '-').slice(0, 20)}-${i}`;
      nodes.push({
        id: scId,
        type: 'supply_chain',
        position: { x: 0, y: 0 },
        data: { id: scId, category: 'supply_chain', label: entity.name, supplyChain: entity },
      });
      // Direction: customer buys FROM company, supplier sells TO company
      if (entity.relationship === 'customer') {
        edges.push({
          id: `edge-${companyId}-${scId}`,
          source: companyId,
          target: scId,
          type: 'supply_chain',
          data: { category: 'supply_chain', relationship: 'customer' },
        });
      } else {
        edges.push({
          id: `edge-${scId}-${companyId}`,
          source: scId,
          target: companyId,
          type: 'supply_chain',
          data: { category: 'supply_chain', relationship: 'supplier' },
        });
      }
    }
  }

  // 9. Segment Nodes
  if (filters.showSegments && data.segments && data.segments.length > 0) {
    for (let i = 0; i < data.segments.length; i++) {
      const segment = data.segments[i];
      const segId = `seg-${segment.name.replace(/\s+/g, '-').slice(0, 20)}-${i}`;
      nodes.push({
        id: segId,
        type: 'metrics',  // Reuse metrics node for segments
        position: { x: 0, y: 0 },
        data: {
          id: segId, category: 'segment', label: segment.name, segment,
          metrics: { value: segment.value, type: segment.type },
        },
      });
      edges.push({
        id: `edge-${companyId}-${segId}`,
        source: companyId,
        target: segId,
        type: 'internal',
        data: { category: 'segment' },
      });
    }
  }

  // 10. Debt Holder Nodes
  if (filters.showDebtHolders && data.debtHolders && data.debtHolders.length > 0) {
    for (let i = 0; i < data.debtHolders.length; i++) {
      const dh = data.debtHolders[i];
      const dhId = `debt-${dh.name.replace(/\s+/g, '-').slice(0, 20)}-${i}`;
      nodes.push({
        id: dhId,
        type: 'event',  // Reuse event node styling for debt holders
        position: { x: 0, y: 0 },
        data: { id: dhId, category: 'debt_holder', label: dh.name, debtHolder: dh },
      });
      edges.push({
        id: `edge-${dhId}-${companyId}`,
        source: dhId,
        target: companyId,
        type: 'internal',
        data: { category: 'debt' },
      });
    }
  }

  return { nodes, edges };
}

const RelationshipMapTab: React.FC = () => {
  const { data, loading, error, phase, fetchData } = useRelationshipData();
  const [layoutMode, setLayoutMode] = useState<LayoutMode>('layered');
  const [filters, setFilters] = useState<FilterState>(DEFAULT_FILTERS);
  const [showFilters, setShowFilters] = useState(false);
  const [selectedNode, setSelectedNode] = useState<GraphNodeData | null>(null);

  const { nodes: rawNodes, edges } = useMemo(() => {
    if (!data) return { nodes: [], edges: [] };
    return buildGraph(data, filters);
  }, [data, filters]);

  const nodes = useMemo(() => {
    if (rawNodes.length === 0) return [];
    const effectiveMode = layoutMode === 'custom' ? selectOptimalLayout(rawNodes) : layoutMode;
    return applyLayout(rawNodes, effectiveMode, {
      mode: effectiveMode, centerX: 600, centerY: 500, width: 2000, height: 1600, padding: 100,
    });
  }, [rawNodes, layoutMode]);

  const nodeCounts: Record<string, number> = useMemo(() => {
    if (!data) return { peer: 0, institutional: 0, fund_family: 0, insider: 0, event: 0, supply_chain: 0, segment: 0, debt_holder: 0, metrics: 0 };
    return {
      peer: data.peers.length,
      institutional: data.institutionalHolders.length,
      fund_family: data.fundFamilies.length,
      insider: data.insiderHolders.length,
      event: data.corporateEvents.length,
      supply_chain: data.supplyChain?.length || 0,
      segment: data.segments?.length || 0,
      debt_holder: data.debtHolders?.length || 0,
      metrics: 3,
    };
  }, [data]);

  const handleSearch = useCallback((ticker: string) => {
    setSelectedNode(null);
    fetchData(ticker);
  }, [fetchData]);

  const handleNodeClick = useCallback((_: React.MouseEvent, node: Node<GraphNodeData>) => {
    setSelectedNode(node.data);
  }, []);

  const handlePaneClick = useCallback(() => {
    setSelectedNode(null);
  }, []);

  return (
    <div style={{ width: '100%', height: '100%', position: 'relative', backgroundColor: FINCEPT.DARK_BG, fontFamily: '"IBM Plex Mono", "Consolas", monospace' }}>
      <HeaderBar
        onSearch={handleSearch}
        layoutMode={layoutMode}
        onLayoutChange={setLayoutMode}
        onToggleFilters={() => setShowFilters(!showFilters)}
        showFilters={showFilters}
        dataQuality={data?.dataQuality}
        loading={loading}
        progress={phase.progress}
        message={phase.message}
      />

      {showFilters && (
        <FilterPanel filters={filters} onChange={setFilters} nodeCounts={nodeCounts} />
      )}

      <div style={{ position: 'absolute', top: '48px', left: 0, right: 0, bottom: '28px' }}>
        {nodes.length > 0 ? (
          <ReactFlow
            nodes={nodes}
            edges={edges}
            nodeTypes={nodeTypes}
            edgeTypes={edgeTypes}
            onNodeClick={handleNodeClick}
            onPaneClick={handlePaneClick}
            onlyRenderVisibleElements={true}
            fitView
            fitViewOptions={{ padding: 0.2, maxZoom: 1.5 }}
            minZoom={0.05}
            maxZoom={3}
            defaultEdgeOptions={{ animated: false }}
            proOptions={{ hideAttribution: true }}
          >
            <Background color={FINCEPT.BORDER} gap={40} />
            <Controls
              position="bottom-right"
              style={{ background: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px' }}
            />
            <MiniMap
              nodeColor={(node: any) => {
                const cat = node?.data?.category;
                return NODE_COLORS[cat as keyof typeof NODE_COLORS]?.border || FINCEPT.GRAY;
              }}
              style={{ background: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}` }}
              position="bottom-left"
              pannable
              zoomable
            />
          </ReactFlow>
        ) : !loading ? (
          <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', height: '100%', color: FINCEPT.MUTED, fontSize: '10px', textAlign: 'center' }}>
            <div style={{ fontSize: '32px', opacity: 0.3, marginBottom: '12px' }}>&#x1F4C8;</div>
            <div style={{ fontSize: '12px', fontWeight: 700, color: FINCEPT.WHITE, letterSpacing: '0.5px' }}>
              CORPORATE INTELLIGENCE MAP
            </div>
            <div style={{ fontSize: '10px', maxWidth: '360px', marginTop: '8px', lineHeight: 1.6, color: FINCEPT.GRAY }}>
              Enter a ticker symbol to visualize corporate relationships: ownership structure, peer valuations, insider activity, and events.
            </div>
            <div style={{ fontSize: '9px', color: FINCEPT.ORANGE, marginTop: '12px', fontWeight: 700 }}>
              TRY: AAPL, MSFT, TSLA, NVDA, GOOGL
            </div>
          </div>
        ) : null}

        {error && (
          <div style={{ position: 'absolute', top: '50%', left: '50%', transform: 'translate(-50%, -50%)', background: `${FINCEPT.RED}15`, border: `1px solid ${FINCEPT.RED}`, borderRadius: '2px', padding: '16px 24px', textAlign: 'center' }}>
            <div style={{ fontWeight: 700, marginBottom: '4px', color: FINCEPT.RED, fontSize: '10px' }}>ERROR LOADING DATA</div>
            <div style={{ fontSize: '9px', color: FINCEPT.GRAY }}>{error}</div>
          </div>
        )}
      </div>

      <DetailPanel nodeData={selectedNode} onClose={() => setSelectedNode(null)} />
      <LegendPanel visible={nodes.length > 0 && !selectedNode} />

      {/* Status Bar */}
      <div style={{ position: 'absolute', bottom: 0, left: 0, right: 0, height: '28px', backgroundColor: FINCEPT.HEADER_BG, borderTop: `1px solid ${FINCEPT.BORDER}`, padding: '4px 16px', display: 'flex', alignItems: 'center', justifyContent: 'space-between', fontSize: '9px', color: FINCEPT.GRAY }}>
        <span>{data ? `${nodes.length} NODES | ${edges.length} EDGES` : 'READY'}</span>
        <span>{data ? `QUALITY: ${data.dataQuality.overall}%` : ''}</span>
        <span style={{ color: FINCEPT.ORANGE }}>FINCEPT TERMINAL</span>
      </div>
    </div>
  );
};

export default RelationshipMapTab;
