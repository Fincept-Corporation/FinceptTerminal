// File: src/components/visualization/TradeChordDiagram.tsx
// Professional Interactive Chord Diagram with Zoom, Pan, and Smart Label Positioning

import React, { useEffect, useRef, useState } from 'react';
import * as d3 from 'd3';
import type { ChordData } from '@/services/tradeService';

// Bloomberg Professional Color Palette
const BLOOMBERG = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  BLUE: '#0088FF',
  PURPLE: '#9D4EDD',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A'
};

interface TradeChordDiagramProps {
  data: ChordData;
}

export const TradeChordDiagram: React.FC<TradeChordDiagramProps> = ({ data }) => {
  const svgRef = useRef<SVGSVGElement>(null);
  const containerRef = useRef<HTMLDivElement>(null);
  const [hoveredPartner, setHoveredPartner] = useState<number | null>(null);
  const [selectedPartner, setSelectedPartner] = useState<number | null>(null);
  const [tooltip, setTooltip] = useState<{ x: number; y: number; content: string } | null>(null);

  const formatCurrency = (value: number) => {
    if (value >= 1e12) return `$${(value / 1e12).toFixed(2)}T`;
    if (value >= 1e9) return `$${(value / 1e9).toFixed(2)}B`;
    if (value >= 1e6) return `$${(value / 1e6).toFixed(2)}M`;
    if (value >= 1e3) return `$${(value / 1e3).toFixed(2)}K`;
    return `$${value.toFixed(2)}`;
  };

  useEffect(() => {
    if (!svgRef.current || !containerRef.current || !data) return;

    const container = containerRef.current;
    const svg = d3.select(svgRef.current);

    // Clear previous content
    svg.selectAll('*').remove();

    // Get dimensions
    const width = container.clientWidth;
    const height = container.clientHeight;
    const centerX = width / 2;
    const centerY = height / 2;
    const baseRadius = Math.min(width, height) * 0.3;

    // Set up SVG
    svg.attr('width', width).attr('height', height);

    // Create zoom behavior
    const zoom = d3.zoom<SVGSVGElement, unknown>()
      .scaleExtent([0.5, 5])
      .on('zoom', (event) => {
        g.attr('transform', event.transform.toString());
      });

    svg.call(zoom as any);

    // Main group for zoom/pan
    const g = svg.append('g');

    // Prepare data
    type PartnerData = {
      name: string;
      exportValue: number;
      importValue: number;
      totalTrade: number;
      isSource: boolean;
      index: number;
    };

    const partners: PartnerData[] = [
      {
        name: data.country_name,
        exportValue: data.total_export,
        importValue: data.total_import,
        totalTrade: data.total_trade,
        isSource: true,
        index: 0
      },
      ...data.partners.map((p, idx) => ({
        name: p.partner_name,
        exportValue: p.export_value,
        importValue: p.import_value,
        totalTrade: p.total_trade,
        isSource: false,
        index: idx + 1
      }))
    ];

    const totalTrade = d3.sum(partners, p => p.totalTrade);

    // Calculate angles
    let currentAngle = -Math.PI / 2;
    const arcs = partners.map(partner => {
      const proportion = partner.totalTrade / totalTrade;
      const angleSpan = proportion * Math.PI * 2;
      const arc = {
        partner,
        startAngle: currentAngle,
        endAngle: currentAngle + angleSpan,
        midAngle: currentAngle + angleSpan / 2
      };
      currentAngle += angleSpan;
      return arc;
    });

    // Create arc generator
    const arcGenerator = d3.arc<any>()
      .innerRadius(baseRadius)
      .outerRadius((d: any) => {
        const isHovered = d.partner.index === hoveredPartner;
        const isSelected = d.partner.index === selectedPartner;
        return baseRadius + (isSelected ? 30 : isHovered ? 25 : 20);
      })
      .startAngle((d: any) => d.startAngle)
      .endAngle((d: any) => d.endAngle);

    // Draw arcs
    const arcGroup = g.append('g').attr('class', 'arcs');

    arcGroup.selectAll('path')
      .data(arcs)
      .join('path')
      .attr('d', arcGenerator as any)
      .attr('transform', `translate(${centerX},${centerY})`)
      .attr('fill', (d: any) => {
        if (d.partner.isSource) return BLOOMBERG.ORANGE;
        return BLOOMBERG.CYAN;
      })
      .attr('fill-opacity', (d: any) => {
        const isHovered = d.partner.index === hoveredPartner;
        const isSelected = d.partner.index === selectedPartner;
        if (isSelected) return 1;
        if (isHovered) return 0.9;
        return 0.7;
      })
      .attr('stroke', BLOOMBERG.BORDER)
      .attr('stroke-width', 1.5)
      .style('cursor', 'pointer')
      .on('mouseenter', function(event, d: any) {
        setHoveredPartner(d.partner.index);
        const tooltipContent = d.partner.isSource
          ? `${d.partner.name}\nTotal Trade: ${formatCurrency(d.partner.totalTrade)}\nExports: ${formatCurrency(d.partner.exportValue)}\nImports: ${formatCurrency(d.partner.importValue)}`
          : `${d.partner.name}\nTotal: ${formatCurrency(d.partner.totalTrade)}\nExports to ${data.country_name}: ${formatCurrency(d.partner.exportValue)}\nImports from ${data.country_name}: ${formatCurrency(d.partner.importValue)}`;
        setTooltip({ x: event.pageX, y: event.pageY, content: tooltipContent });
      })
      .on('mouseleave', function() {
        setHoveredPartner(null);
        setTooltip(null);
      })
      .on('click', function(event, d: any) {
        event.stopPropagation();
        setSelectedPartner(d.partner.index === selectedPartner ? null : d.partner.index);
      });

    // Draw chords (trade flows)
    const chordGroup = g.append('g').attr('class', 'chords');

    data.partners.forEach((partner, idx) => {
      const sourceArc = arcs[0];
      const targetArc = arcs[idx + 1];

      if (!sourceArc || !targetArc) return;

      // Export chord (green)
      if (partner.export_value > 0) {
        const path = createChordPath(
          centerX, centerY, baseRadius,
          sourceArc.midAngle, targetArc.midAngle
        );

        const thickness = Math.max(1, (partner.export_value / totalTrade) * 25);
        const isHighlighted = hoveredPartner === idx + 1 || selectedPartner === idx + 1;

        chordGroup.append('path')
          .attr('d', path)
          .attr('stroke', BLOOMBERG.GREEN)
          .attr('stroke-width', isHighlighted ? thickness * 1.5 : thickness)
          .attr('stroke-opacity', isHighlighted ? 0.9 : 0.4)
          .attr('fill', 'none')
          .attr('stroke-linecap', 'round')
          .style('pointer-events', 'none');
      }

      // Import chord (red)
      if (partner.import_value > 0) {
        const path = createChordPath(
          centerX, centerY, baseRadius,
          targetArc.midAngle, sourceArc.midAngle
        );

        const thickness = Math.max(1, (partner.import_value / totalTrade) * 25);
        const isHighlighted = hoveredPartner === idx + 1 || selectedPartner === idx + 1;

        chordGroup.append('path')
          .attr('d', path)
          .attr('stroke', BLOOMBERG.RED)
          .attr('stroke-width', isHighlighted ? thickness * 1.5 : thickness)
          .attr('stroke-opacity', isHighlighted ? 0.9 : 0.4)
          .attr('fill', 'none')
          .attr('stroke-linecap', 'round')
          .style('pointer-events', 'none');
      }
    });

    // Smart label positioning to avoid collisions
    const labelGroup = g.append('g').attr('class', 'labels');
    const labelRadius = baseRadius + 45;
    const minLabelDistance = 18; // Minimum pixels between labels

    // Calculate initial label positions
    const labelPositions = arcs.map(arc => ({
      arc,
      x: centerX + Math.cos(arc.midAngle) * labelRadius,
      y: centerY + Math.sin(arc.midAngle) * labelRadius,
      angle: arc.midAngle
    }));

    // Adjust positions to avoid collisions (simple repulsion)
    for (let iteration = 0; iteration < 50; iteration++) {
      let adjusted = false;
      for (let i = 0; i < labelPositions.length; i++) {
        for (let j = i + 1; j < labelPositions.length; j++) {
          const pos1 = labelPositions[i];
          const pos2 = labelPositions[j];
          const dx = pos2.x - pos1.x;
          const dy = pos2.y - pos1.y;
          const distance = Math.sqrt(dx * dx + dy * dy);

          if (distance < minLabelDistance && distance > 0) {
            adjusted = true;
            const angle = Math.atan2(dy, dx);
            const targetDistance = minLabelDistance;
            const force = (targetDistance - distance) / 2;

            pos1.x -= Math.cos(angle) * force;
            pos1.y -= Math.sin(angle) * force;
            pos2.x += Math.cos(angle) * force;
            pos2.y += Math.sin(angle) * force;
          }
        }
      }
      if (!adjusted) break;
    }

    // Draw labels with leader lines
    labelPositions.forEach((pos, idx) => {
      const arcMidX = centerX + Math.cos(pos.arc.midAngle) * (baseRadius + 25);
      const arcMidY = centerY + Math.sin(pos.arc.midAngle) * (baseRadius + 25);

      // Leader line
      labelGroup.append('line')
        .attr('x1', arcMidX)
        .attr('y1', arcMidY)
        .attr('x2', pos.x)
        .attr('y2', pos.y)
        .attr('stroke', BLOOMBERG.MUTED)
        .attr('stroke-width', 0.5)
        .attr('stroke-dasharray', '2,2')
        .attr('opacity', 0.5);

      // Text background for better readability
      const text = pos.arc.partner.name;
      const textAlign = pos.angle > Math.PI / 2 && pos.angle < (3 * Math.PI) / 2 ? 'end' : 'start';

      labelGroup.append('text')
        .attr('x', pos.x)
        .attr('y', pos.y)
        .attr('text-anchor', textAlign)
        .attr('dominant-baseline', 'middle')
        .attr('font-size', '11px')
        .attr('font-weight', pos.arc.partner.isSource ? '700' : '400')
        .attr('font-family', '"Segoe UI", sans-serif')
        .attr('fill', pos.arc.partner.isSource ? BLOOMBERG.YELLOW : BLOOMBERG.WHITE)
        .attr('stroke', BLOOMBERG.DARK_BG)
        .attr('stroke-width', 3)
        .attr('paint-order', 'stroke')
        .text(text.length > 15 ? text.substring(0, 15) + '...' : text)
        .style('cursor', 'pointer')
        .on('click', function() {
          setSelectedPartner(pos.arc.partner.index === selectedPartner ? null : pos.arc.partner.index);
        });
    });

    // Add zoom controls
    const controlsGroup = svg.append('g').attr('class', 'zoom-controls');

    // Zoom in button
    controlsGroup.append('rect')
      .attr('x', 20)
      .attr('y', 20)
      .attr('width', 32)
      .attr('height', 32)
      .attr('rx', 4)
      .attr('fill', BLOOMBERG.PANEL_BG)
      .attr('stroke', BLOOMBERG.BORDER)
      .style('cursor', 'pointer')
      .on('click', function() {
        svg.transition().duration(300).call(zoom.scaleBy as any, 1.3);
      });

    controlsGroup.append('text')
      .attr('x', 36)
      .attr('y', 40)
      .attr('text-anchor', 'middle')
      .attr('fill', BLOOMBERG.WHITE)
      .attr('font-size', '20px')
      .attr('font-weight', 'bold')
      .text('+')
      .style('pointer-events', 'none');

    // Zoom out button
    controlsGroup.append('rect')
      .attr('x', 20)
      .attr('y', 60)
      .attr('width', 32)
      .attr('height', 32)
      .attr('rx', 4)
      .attr('fill', BLOOMBERG.PANEL_BG)
      .attr('stroke', BLOOMBERG.BORDER)
      .style('cursor', 'pointer')
      .on('click', function() {
        svg.transition().duration(300).call(zoom.scaleBy as any, 0.7);
      });

    controlsGroup.append('text')
      .attr('x', 36)
      .attr('y', 80)
      .attr('text-anchor', 'middle')
      .attr('fill', BLOOMBERG.WHITE)
      .attr('font-size', '20px')
      .attr('font-weight', 'bold')
      .text('−')
      .style('pointer-events', 'none');

    // Reset button
    controlsGroup.append('rect')
      .attr('x', 20)
      .attr('y', 100)
      .attr('width', 32)
      .attr('height', 32)
      .attr('rx', 4)
      .attr('fill', BLOOMBERG.PANEL_BG)
      .attr('stroke', BLOOMBERG.BORDER)
      .style('cursor', 'pointer')
      .on('click', function() {
        svg.transition().duration(500).call(
          zoom.transform as any,
          d3.zoomIdentity
        );
        setSelectedPartner(null);
      });

    controlsGroup.append('text')
      .attr('x', 36)
      .attr('y', 120)
      .attr('text-anchor', 'middle')
      .attr('fill', BLOOMBERG.WHITE)
      .attr('font-size', '16px')
      .text('⟲')
      .style('pointer-events', 'none');

    // Click on background to deselect
    svg.on('click', function() {
      setSelectedPartner(null);
    });

  }, [data, hoveredPartner, selectedPartner]);

  // Helper function to create smooth chord paths
  const createChordPath = (
    centerX: number,
    centerY: number,
    radius: number,
    startAngle: number,
    endAngle: number
  ): string => {
    const startX = centerX + Math.cos(startAngle) * radius;
    const startY = centerY + Math.sin(startAngle) * radius;
    const endX = centerX + Math.cos(endAngle) * radius;
    const endY = centerY + Math.sin(endAngle) * radius;

    // Control points for smooth bezier curve
    const controlOffset = radius * 0.25;
    const control1X = centerX + Math.cos(startAngle) * controlOffset;
    const control1Y = centerY + Math.sin(startAngle) * controlOffset;
    const control2X = centerX + Math.cos(endAngle) * controlOffset;
    const control2Y = centerY + Math.sin(endAngle) * controlOffset;

    return `M ${startX} ${startY} C ${control1X} ${control1Y}, ${control2X} ${control2Y}, ${endX} ${endY}`;
  };

  return (
    <div ref={containerRef} style={{ position: 'relative', width: '100%', height: '100%' }}>
      <svg
        ref={svgRef}
        style={{
          width: '100%',
          height: '100%',
          backgroundColor: BLOOMBERG.DARK_BG,
          cursor: 'grab'
        }}
      />

      {/* Tooltip */}
      {tooltip && (
        <div
          style={{
            position: 'fixed',
            left: tooltip.x + 10,
            top: tooltip.y + 10,
            backgroundColor: BLOOMBERG.PANEL_BG,
            border: `2px solid ${BLOOMBERG.ORANGE}`,
            padding: '10px 14px',
            borderRadius: '6px',
            fontSize: '12px',
            fontWeight: '500',
            color: BLOOMBERG.WHITE,
            pointerEvents: 'none',
            zIndex: 10000,
            whiteSpace: 'pre-line',
            boxShadow: `0 8px 24px ${BLOOMBERG.DARK_BG}CC`,
            fontFamily: '"Segoe UI", sans-serif',
            backdropFilter: 'blur(10px)'
          }}
        >
          {tooltip.content}
        </div>
      )}

      {/* Instructions */}
      <div
        style={{
          position: 'absolute',
          bottom: '20px',
          left: '50%',
          transform: 'translateX(-50%)',
          backgroundColor: `${BLOOMBERG.PANEL_BG}E6`,
          border: `1px solid ${BLOOMBERG.BORDER}`,
          borderRadius: '20px',
          padding: '8px 16px',
          fontSize: '10px',
          color: BLOOMBERG.GRAY,
          display: 'flex',
          gap: '16px',
          backdropFilter: 'blur(10px)'
        }}
      >
        <span>🖱️ Drag to Pan</span>
        <span>🔍 Scroll to Zoom</span>
        <span>👆 Click to Select</span>
        <span>⟲ Reset View</span>
      </div>

      {/* Right side table */}
      <div
        style={{
          position: 'absolute',
          right: '20px',
          top: '20px',
          backgroundColor: `${BLOOMBERG.PANEL_BG}F2`,
          border: `1px solid ${BLOOMBERG.BORDER}`,
          borderRadius: '8px',
          padding: '12px',
          maxWidth: '320px',
          maxHeight: 'calc(100% - 40px)',
          overflowY: 'auto',
          backdropFilter: 'blur(10px)'
        }}
      >
        <div style={{
          fontSize: '11px',
          fontWeight: 600,
          color: BLOOMBERG.ORANGE,
          marginBottom: '12px',
          paddingBottom: '8px',
          borderBottom: `1px solid ${BLOOMBERG.BORDER}`
        }}>
          TRADING PARTNERS
        </div>

        {data.partners.map((partner, idx) => (
          <div
            key={partner.partner_id}
            style={{
              padding: '10px',
              marginBottom: '6px',
              backgroundColor: selectedPartner === idx + 1
                ? `${BLOOMBERG.ORANGE}30`
                : hoveredPartner === idx + 1
                  ? `${BLOOMBERG.CYAN}20`
                  : 'transparent',
              border: `1px solid ${
                selectedPartner === idx + 1
                  ? BLOOMBERG.ORANGE
                  : hoveredPartner === idx + 1
                    ? BLOOMBERG.CYAN
                    : 'transparent'
              }`,
              borderRadius: '4px',
              cursor: 'pointer',
              transition: 'all 0.2s ease'
            }}
            onMouseEnter={() => setHoveredPartner(idx + 1)}
            onMouseLeave={() => setHoveredPartner(null)}
            onClick={() => setSelectedPartner(idx + 1 === selectedPartner ? null : idx + 1)}
          >
            <div style={{
              fontSize: '11px',
              fontWeight: 600,
              color: BLOOMBERG.WHITE,
              marginBottom: '4px',
              display: 'flex',
              justifyContent: 'space-between',
              alignItems: 'center'
            }}>
              <span>{idx + 1}. {partner.partner_name}</span>
              <span style={{ fontSize: '10px', color: BLOOMBERG.YELLOW }}>
                {formatCurrency(partner.total_trade)}
              </span>
            </div>
            <div style={{ fontSize: '9px', color: BLOOMBERG.GRAY, display: 'flex', gap: '12px' }}>
              <span style={{ color: BLOOMBERG.GREEN }}>
                ↑ {formatCurrency(partner.export_value)}
              </span>
              <span style={{ color: BLOOMBERG.RED }}>
                ↓ {formatCurrency(partner.import_value)}
              </span>
            </div>
          </div>
        ))}
      </div>
    </div>
  );
};
