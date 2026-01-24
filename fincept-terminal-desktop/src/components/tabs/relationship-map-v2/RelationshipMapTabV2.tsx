// Relationship Map V2 - Comprehensive Corporate Intelligence Visualization
// Multi-layer graph with valuation signals, ownership, peers, events

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

// Node components
import CompanyNode from './components/nodes/CompanyNode';
import PeerNode from './components/nodes/PeerNode';
import InstitutionalHolderNode from './components/nodes/InstitutionalHolderNode';
import FundFamilyNode from './components/nodes/FundFamilyNode';
import InsiderNode from './components/nodes/InsiderNode';
import EventNode from './components/nodes/EventNode';
import MetricsClusterNode from './components/nodes/MetricsClusterNode';
import SupplyChainNode from './components/nodes/SupplyChainNode';

// Edge components
import OwnershipEdge from './components/edges/OwnershipEdge';

// Panels & Controls
import HeaderBar from './components/controls/HeaderBar';
import FilterPanel from './components/panels/FilterPanel';
import DetailPanel from './components/panels/DetailPanel';
import LegendPanel from './components/controls/LegendPanel';

// ============================================================================
// NODE & EDGE TYPE REGISTRATIONS
// ============================================================================

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
};

// ============================================================================
// GRAPH BUILDER
// ============================================================================

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
  if (filters.showPeers) {
    for (const peer of data.peers) {
      // Apply peer filter
      const peerVal = data.peerValuations.get(peer.ticker);
      if (filters.peerFilter === 'undervalued' && peerVal?.action !== 'BUY') continue;
      if (filters.peerFilter === 'overvalued' && peerVal?.action !== 'SELL') continue;

      const peerId = `peer-${peer.ticker}`;
      nodes.push({
        id: peerId,
        type: 'peer',
        position: { x: 0, y: 0 },
        data: {
          id: peerId,
          category: 'peer',
          label: peer.ticker,
          peer,
          peerValuation: peerVal,
        },
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
  if (filters.showInstitutional) {
    for (const holder of data.institutionalHolders) {
      if (holder.percentage < filters.minOwnership) continue;
      const holderId = `inst-${holder.name.replace(/\s+/g, '-').slice(0, 20)}`;
      nodes.push({
        id: holderId,
        type: 'institutional',
        position: { x: 0, y: 0 },
        data: {
          id: holderId,
          category: 'institutional',
          label: holder.name,
          holder,
        },
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
  if (filters.showFundFamilies) {
    for (const family of data.fundFamilies) {
      if (family.total_percentage < filters.minOwnership) continue;
      const familyId = `fund-${family.name.replace(/\s+/g, '-').slice(0, 20)}`;
      nodes.push({
        id: familyId,
        type: 'fund_family',
        position: { x: 0, y: 0 },
        data: {
          id: familyId,
          category: 'fund_family',
          label: family.name,
          fundFamily: family,
        },
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
  if (filters.showInsiders) {
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
  if (filters.showEvents) {
    for (let i = 0; i < data.corporateEvents.length; i++) {
      const event = data.corporateEvents[i];
      const eventId = `event-${i}`;
      nodes.push({
        id: eventId,
        type: 'event',
        position: { x: 0, y: 0 },
        data: {
          id: eventId,
          category: 'event',
          label: event.description || event.form,
          event,
        },
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

  // 7. Metrics Clusters
  if (filters.showMetrics) {
    // Financials cluster
    if (data.company.revenue > 0) {
      const finId = 'metrics-financials';
      nodes.push({
        id: finId,
        type: 'metrics',
        position: { x: 0, y: 0 },
        data: {
          id: finId,
          category: 'metrics',
          label: 'Financials',
          metricsType: 'financials',
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
      edges.push({
        id: `edge-${companyId}-${finId}`,
        source: companyId,
        target: finId,
        type: 'internal',
        data: { category: 'internal' },
      });
    }

    // Valuation cluster
    if (data.company.pe_ratio > 0 || data.company.price_to_book > 0) {
      const valId = 'metrics-valuation';
      nodes.push({
        id: valId,
        type: 'metrics',
        position: { x: 0, y: 0 },
        data: {
          id: valId,
          category: 'metrics',
          label: 'Valuation',
          metricsType: 'valuation',
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
      edges.push({
        id: `edge-${companyId}-${valId}`,
        source: companyId,
        target: valId,
        type: 'internal',
        data: { category: 'internal' },
      });
    }

    // Balance Sheet cluster
    if (data.company.total_cash > 0 || data.company.total_debt > 0) {
      const bsId = 'metrics-balance';
      nodes.push({
        id: bsId,
        type: 'metrics',
        position: { x: 0, y: 0 },
        data: {
          id: bsId,
          category: 'metrics',
          label: 'Balance Sheet',
          metricsType: 'balance_sheet',
          metrics: {
            total_cash: data.company.total_cash,
            total_debt: data.company.total_debt,
            shares_outstanding: data.company.shares_outstanding,
            float_shares: data.company.float_shares,
          },
        },
      });
      edges.push({
        id: `edge-${companyId}-${bsId}`,
        source: companyId,
        target: bsId,
        type: 'internal',
        data: { category: 'internal' },
      });
    }
  }

  return { nodes, edges };
}

// ============================================================================
// MAIN COMPONENT
// ============================================================================

const RelationshipMapTabV2: React.FC = () => {
  const { data, loading, error, phase, fetchData } = useRelationshipData();
  const [layoutMode, setLayoutMode] = useState<LayoutMode>('layered');
  const [filters, setFilters] = useState<FilterState>(DEFAULT_FILTERS);
  const [showFilters, setShowFilters] = useState(false);
  const [selectedNode, setSelectedNode] = useState<GraphNodeData | null>(null);

  // Build graph from data
  const { nodes: rawNodes, edges } = useMemo(() => {
    if (!data) return { nodes: [], edges: [] };
    return buildGraph(data, filters);
  }, [data, filters]);

  // Apply layout
  const nodes = useMemo(() => {
    if (rawNodes.length === 0) return [];

    const effectiveMode = layoutMode === 'custom'
      ? selectOptimalLayout(rawNodes)
      : layoutMode;

    return applyLayout(rawNodes, effectiveMode, {
      mode: effectiveMode,
      centerX: 600,
      centerY: 500,
      width: 2000,
      height: 1600,
      padding: 100,
    });
  }, [rawNodes, layoutMode]);

  // Node counts for filter panel
  const nodeCounts: Record<string, number> = useMemo(() => {
    if (!data) return { peer: 0, institutional: 0, fund_family: 0, insider: 0, event: 0, metrics: 0 };
    return {
      peer: data.peers.length,
      institutional: data.institutionalHolders.length,
      fund_family: data.fundFamilies.length,
      insider: data.insiderHolders.length,
      event: data.corporateEvents.length,
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
    <div style={{ width: '100%', height: '100%', position: 'relative', background: 'var(--ft-color-background)' }}>
      {/* Header Bar */}
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

      {/* Filter Panel */}
      {showFilters && (
        <FilterPanel
          filters={filters}
          onChange={setFilters}
          nodeCounts={nodeCounts}
        />
      )}

      {/* Main Graph */}
      <div style={{ position: 'absolute', top: '48px', left: 0, right: 0, bottom: 0 }}>
        {nodes.length > 0 ? (
          <ReactFlow
            nodes={nodes}
            edges={edges}
            nodeTypes={nodeTypes}
            edgeTypes={edgeTypes}
            onNodeClick={handleNodeClick}
            onPaneClick={handlePaneClick}
            fitView
            fitViewOptions={{ padding: 0.2, maxZoom: 1.5 }}
            minZoom={0.1}
            maxZoom={3}
            defaultEdgeOptions={{ animated: false }}
            proOptions={{ hideAttribution: true }}
          >
            <Background color="rgba(120,120,120,0.1)" gap={40} />
            <Controls
              position="bottom-right"
              style={{ background: 'rgba(10,10,10,0.8)', border: '1px solid rgba(120,120,120,0.3)', borderRadius: '4px' }}
            />
            <MiniMap
              nodeColor={(node: any) => {
                const category = node?.data?.category;
                if (category === 'company') return '#FFA500';
                if (category === 'peer') return '#6496FA';
                if (category === 'institutional') return '#00C800';
                if (category === 'fund_family') return '#C864FF';
                if (category === 'insider') return '#00FFFF';
                if (category === 'event') return '#FF0000';
                return '#787878';
              }}
              style={{ background: 'rgba(10,10,10,0.8)', border: '1px solid rgba(120,120,120,0.3)' }}
              position="bottom-left"
              pannable
              zoomable
            />
          </ReactFlow>
        ) : !loading ? (
          <div style={{
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            justifyContent: 'center',
            height: '100%',
            color: 'var(--ft-color-text-muted)',
            gap: '12px',
          }}>
            <div style={{ fontSize: '40px', opacity: 0.3 }}>&#x1F310;</div>
            <div style={{ fontSize: '14px', fontWeight: 'bold', color: 'var(--ft-color-text)' }}>
              Corporate Intelligence Map
            </div>
            <div style={{ fontSize: '11px', maxWidth: '400px', textAlign: 'center', lineHeight: 1.6 }}>
              Enter a ticker symbol to visualize the complete corporate relationship network:
              ownership structure, peer valuations, insider activity, and corporate events.
            </div>
            <div style={{ fontSize: '10px', color: 'var(--ft-color-primary)', marginTop: '8px' }}>
              Try: AAPL, MSFT, TSLA, NVDA, GOOGL
            </div>
          </div>
        ) : null}

        {/* Error Display */}
        {error && (
          <div style={{
            position: 'absolute',
            top: '50%',
            left: '50%',
            transform: 'translate(-50%, -50%)',
            background: 'rgba(255,0,0,0.1)',
            border: '1px solid var(--ft-color-alert)',
            borderRadius: '6px',
            padding: '16px 24px',
            color: 'var(--ft-color-alert)',
            fontSize: '12px',
            textAlign: 'center',
          }}>
            <div style={{ fontWeight: 'bold', marginBottom: '4px' }}>Error Loading Data</div>
            <div style={{ fontSize: '10px', color: 'var(--ft-color-text-muted)' }}>{error}</div>
          </div>
        )}
      </div>

      {/* Detail Panel */}
      <DetailPanel
        nodeData={selectedNode}
        onClose={() => setSelectedNode(null)}
      />

      {/* Legend */}
      <LegendPanel visible={nodes.length > 0 && !selectedNode} />
    </div>
  );
};

export default RelationshipMapTabV2;
