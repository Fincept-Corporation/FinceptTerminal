// Layout Algorithms for Relationship Map V2
// Supports: Layered, Force-directed, Hierarchical, Circular

import type { Node } from 'reactflow';
import type { GraphNodeData, LayoutMode, LayoutConfig, NodeCategory } from '../types';
import { NODE_SIZES, LAYOUT_CONFIG } from '../constants';

type NodeSizeKey = keyof typeof NODE_SIZES;

function getNodeSize(category: string): { width: number; height: number } {
  return NODE_SIZES[category as NodeSizeKey] || { width: 200, height: 100 };
}

interface PositionedNode {
  id: string;
  x: number;
  y: number;
}

/**
 * Select optimal layout mode based on graph composition.
 */
export function selectOptimalLayout(nodes: Node<GraphNodeData>[]): LayoutMode {
  const count = nodes.length;
  const peerCount = nodes.filter(n => n.data.category === 'peer').length;
  const institutionalCount = nodes.filter(n => n.data.category === 'institutional').length;

  if (count <= 15) return 'layered';
  if (peerCount > institutionalCount && peerCount >= 5) return 'circular';
  if (count > 40) return 'force';
  return 'layered';
}

/**
 * Apply layout to nodes based on mode.
 */
export function applyLayout(
  nodes: Node<GraphNodeData>[],
  mode: LayoutMode,
  config: LayoutConfig
): Node<GraphNodeData>[] {
  let positions: PositionedNode[];

  switch (mode) {
    case 'layered':
      positions = calculateLayeredLayout(nodes, config);
      break;
    case 'force':
      positions = calculateForceLayout(nodes, config);
      break;
    case 'circular':
      positions = calculateCircularLayout(nodes, config);
      break;
    case 'hierarchical':
      positions = calculateHierarchicalLayout(nodes, config);
      break;
    case 'radial':
      positions = calculateRadialLayout(nodes, config);
      break;
    default:
      positions = calculateLayeredLayout(nodes, config);
  }

  // Apply positions to nodes
  return nodes.map(node => {
    const pos = positions.find(p => p.id === node.id);
    if (pos) {
      return { ...node, position: { x: pos.x, y: pos.y } };
    }
    return node;
  });
}

/**
 * Layered Layout - Concentric rings around center company node.
 * Ring order: Company → Metrics → Insiders → Institutional → Fund Families → Peers → Events
 */
function calculateLayeredLayout(
  nodes: Node<GraphNodeData>[],
  config: LayoutConfig
): PositionedNode[] {
  const positions: PositionedNode[] = [];
  const cx = config.centerX;
  const cy = config.centerY;

  // Group nodes by category
  const groups = groupByCategory(nodes);

  // Company node at center
  for (const node of groups.company) {
    positions.push({ id: node.id, x: cx - NODE_SIZES.company.width / 2, y: cy - NODE_SIZES.company.height / 2 });
  }

  // Metrics nodes in inner ring
  placeInRing(groups.metrics, LAYOUT_CONFIG.RINGS.METRICS, cx, cy, positions, -Math.PI / 4);

  // Insider nodes
  placeInRing(groups.insider, LAYOUT_CONFIG.RINGS.INSIDERS, cx, cy, positions, 0);

  // Institutional holders
  placeInRing(groups.institutional, LAYOUT_CONFIG.RINGS.INSTITUTIONAL, cx, cy, positions, Math.PI / 6);

  // Fund families
  placeInRing(groups.fund_family, LAYOUT_CONFIG.RINGS.FUND_FAMILIES, cx, cy, positions, Math.PI / 3);

  // Peer companies
  placeInRing(groups.peer, LAYOUT_CONFIG.RINGS.PEERS, cx, cy, positions, 0);

  // Supply chain nodes (customers and suppliers)
  placeInRing(groups.supply_chain, LAYOUT_CONFIG.RINGS.SUPPLY_CHAIN, cx, cy, positions, -Math.PI / 3);

  // Segment nodes
  placeInRing(groups.segment, LAYOUT_CONFIG.RINGS.SEGMENTS, cx, cy, positions, Math.PI * 0.75);

  // Events
  placeInRing(groups.event, LAYOUT_CONFIG.RINGS.EVENTS, cx, cy, positions, Math.PI / 4);

  // Debt holders
  placeInRing(groups.debt_holder, LAYOUT_CONFIG.RINGS.DEBT_HOLDERS, cx, cy, positions, Math.PI * 0.6);

  return positions;
}

/**
 * Force-directed Layout - Simple spring-based simulation.
 */
function calculateForceLayout(
  nodes: Node<GraphNodeData>[],
  config: LayoutConfig
): PositionedNode[] {
  const positions: PositionedNode[] = [];
  const cx = config.centerX;
  const cy = config.centerY;

  // Initialize positions in a spiral
  const nodePositions: { id: string; x: number; y: number; vx: number; vy: number; fixed: boolean }[] = [];

  nodes.forEach((node, i) => {
    const isCompany = node.data.category === 'company';
    const angle = (i / nodes.length) * Math.PI * 2;
    const radius = isCompany ? 0 : 300 + Math.random() * 400;

    nodePositions.push({
      id: node.id,
      x: cx + Math.cos(angle) * radius,
      y: cy + Math.sin(angle) * radius,
      vx: 0,
      vy: 0,
      fixed: isCompany,
    });
  });

  // Run simplified force simulation (100 iterations)
  const iterations = 100;
  const chargeStrength = LAYOUT_CONFIG.FORCE.CHARGE_STRENGTH;
  const centerGravity = LAYOUT_CONFIG.FORCE.CENTER_GRAVITY;

  for (let iter = 0; iter < iterations; iter++) {
    const alpha = 1 - iter / iterations;

    for (let i = 0; i < nodePositions.length; i++) {
      if (nodePositions[i].fixed) continue;

      let fx = 0, fy = 0;

      // Repulsion from all other nodes
      for (let j = 0; j < nodePositions.length; j++) {
        if (i === j) continue;
        const dx = nodePositions[i].x - nodePositions[j].x;
        const dy = nodePositions[i].y - nodePositions[j].y;
        const dist = Math.max(Math.sqrt(dx * dx + dy * dy), 50);
        const force = chargeStrength / (dist * dist);
        fx += (dx / dist) * force * alpha;
        fy += (dy / dist) * force * alpha;
      }

      // Center gravity
      fx += (cx - nodePositions[i].x) * centerGravity * alpha;
      fy += (cy - nodePositions[i].y) * centerGravity * alpha;

      // Apply forces
      nodePositions[i].vx = (nodePositions[i].vx + fx) * 0.6;
      nodePositions[i].vy = (nodePositions[i].vy + fy) * 0.6;
      nodePositions[i].x += nodePositions[i].vx;
      nodePositions[i].y += nodePositions[i].vy;
    }
  }

  for (const np of nodePositions) {
    const node = nodes.find(n => n.id === np.id);
    const size = node ? getNodeSize(node.data.category) : { width: 200, height: 100 };
    positions.push({ id: np.id, x: np.x - size.width / 2, y: np.y - size.height / 2 });
  }

  return positions;
}

/**
 * Circular Layout - Nodes in concentric circles, peers in outer ring.
 */
function calculateCircularLayout(
  nodes: Node<GraphNodeData>[],
  config: LayoutConfig
): PositionedNode[] {
  const positions: PositionedNode[] = [];
  const cx = config.centerX;
  const cy = config.centerY;

  const groups = groupByCategory(nodes);

  // Company at center
  for (const node of groups.company) {
    positions.push({ id: node.id, x: cx - NODE_SIZES.company.width / 2, y: cy - NODE_SIZES.company.height / 2 });
  }

  // Inner circle: insiders + metrics
  const innerNodes = [...groups.insider, ...groups.metrics];
  placeInRing(innerNodes, 350, cx, cy, positions, 0);

  // Middle circle: institutional + fund families
  const middleNodes = [...groups.institutional, ...groups.fund_family];
  placeInRing(middleNodes, 600, cx, cy, positions, Math.PI / 8);

  // Outer circle: peers (evenly spaced)
  placeInRing(groups.peer, 850, cx, cy, positions, 0);

  // Events scattered outside
  placeInRing(groups.event, 1050, cx, cy, positions, Math.PI / 6);

  // Supply chain
  placeInRing(groups.supply_chain, 1100, cx, cy, positions, Math.PI / 3);

  return positions;
}

/**
 * Hierarchical Layout - Top-down tree structure.
 */
function calculateHierarchicalLayout(
  nodes: Node<GraphNodeData>[],
  config: LayoutConfig
): PositionedNode[] {
  const positions: PositionedNode[] = [];
  const cx = config.centerX;
  const startY = config.centerY - 400;

  const groups = groupByCategory(nodes);
  const levels: Node<GraphNodeData>[][] = [
    groups.company,
    [...groups.insider, ...groups.metrics],
    [...groups.institutional, ...groups.fund_family],
    groups.peer,
    [...groups.event, ...groups.supply_chain],
  ];

  let levelY = startY;
  for (const level of levels) {
    if (level.length === 0) continue;
    const totalWidth = level.length * 300;
    const startX = cx - totalWidth / 2;

    level.forEach((node, i) => {
      const size = getNodeSize(node.data.category);
      positions.push({
        id: node.id,
        x: startX + i * 300 + (300 - size.width) / 2,
        y: levelY,
      });
    });

    levelY += 250;
  }

  return positions;
}

/**
 * Radial Layout - Nodes radiate outward from center in sectors.
 */
function calculateRadialLayout(
  nodes: Node<GraphNodeData>[],
  config: LayoutConfig
): PositionedNode[] {
  const positions: PositionedNode[] = [];
  const cx = config.centerX;
  const cy = config.centerY;

  const groups = groupByCategory(nodes);
  const categories: NodeCategory[] = ['insider', 'institutional', 'fund_family', 'peer', 'event', 'metrics', 'supply_chain', 'segment', 'debt_holder'];

  // Company at center
  for (const node of groups.company) {
    positions.push({ id: node.id, x: cx - NODE_SIZES.company.width / 2, y: cy - NODE_SIZES.company.height / 2 });
  }

  // Each category gets a sector of the circle
  const categoryNodes = categories.filter(c => groups[c].length > 0);
  const sectorAngle = (Math.PI * 2) / Math.max(categoryNodes.length, 1);

  categoryNodes.forEach((cat, catIndex) => {
    const baseAngle = catIndex * sectorAngle - Math.PI / 2;
    const catNodes = groups[cat];

    catNodes.forEach((node, nodeIndex) => {
      const radius = 400 + nodeIndex * 180;
      const angleSpread = sectorAngle * 0.6;
      const nodeAngle = baseAngle + (nodeIndex / Math.max(catNodes.length - 1, 1) - 0.5) * angleSpread;
      const size = getNodeSize(node.data.category);

      positions.push({
        id: node.id,
        x: cx + Math.cos(nodeAngle) * radius - size.width / 2,
        y: cy + Math.sin(nodeAngle) * radius - size.height / 2,
      });
    });
  });

  return positions;
}

// ============================================================================
// HELPERS
// ============================================================================

function groupByCategory(nodes: Node<GraphNodeData>[]): Record<NodeCategory, Node<GraphNodeData>[]> {
  const groups: Record<NodeCategory, Node<GraphNodeData>[]> = {
    company: [],
    peer: [],
    institutional: [],
    fund_family: [],
    insider: [],
    event: [],
    supply_chain: [],
    segment: [],
    debt_holder: [],
    metrics: [],
  };

  for (const node of nodes) {
    const cat = node.data.category;
    if (groups[cat]) {
      groups[cat].push(node);
    }
  }

  return groups;
}

function placeInRing(
  nodes: Node<GraphNodeData>[],
  radius: number,
  cx: number,
  cy: number,
  positions: PositionedNode[],
  startAngle: number
): void {
  if (nodes.length === 0) return;
  const angleStep = (Math.PI * 2) / nodes.length;

  nodes.forEach((node, i) => {
    const angle = startAngle + i * angleStep;
    const size = getNodeSize(node.data.category);
    positions.push({
      id: node.id,
      x: cx + Math.cos(angle) * radius - size.width / 2,
      y: cy + Math.sin(angle) * radius - size.height / 2,
    });
  });
}
