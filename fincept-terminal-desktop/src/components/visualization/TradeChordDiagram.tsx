// File: src/components/visualization/TradeChordDiagram.tsx
// Professional Chord Diagram Visualization for Global Trade Flows

import React, { useEffect, useRef, useState } from 'react';
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
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const [hoveredPartner, setHoveredPartner] = useState<number | null>(null);
  const [tooltip, setTooltip] = useState<{ x: number; y: number; content: string } | null>(null);

  const formatCurrency = (value: number) => {
    if (value >= 1e12) return `$${(value / 1e12).toFixed(2)}T`;
    if (value >= 1e9) return `$${(value / 1e9).toFixed(2)}B`;
    if (value >= 1e6) return `$${(value / 1e6).toFixed(2)}M`;
    return `$${value.toFixed(2)}`;
  };

  useEffect(() => {
    if (!canvasRef.current || !data) return;

    const canvas = canvasRef.current;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    // Set canvas size
    const container = canvas.parentElement;
    if (!container) return;

    const width = container.clientWidth;
    const height = container.clientHeight;
    const dpr = window.devicePixelRatio || 1;

    canvas.width = width * dpr;
    canvas.height = height * dpr;
    canvas.style.width = `${width}px`;
    canvas.style.height = `${height}px`;
    ctx.scale(dpr, dpr);

    // Clear canvas
    ctx.fillStyle = BLOOMBERG.DARK_BG;
    ctx.fillRect(0, 0, width, height);

    // Calculate dimensions
    const centerX = width / 2;
    const centerY = height / 2;
    const radius = Math.min(width, height) * 0.35;

    // Prepare data
    type PartnerDisplay = {
      name: string;
      export: number;
      import: number;
      total: number;
      isSource: boolean;
    };

    const partners: PartnerDisplay[] = [
      { name: data.country_name, export: 0, import: 0, total: data.total_trade, isSource: true },
      ...data.partners.map(p => ({
        name: p.partner_name,
        export: p.export_value,
        import: p.import_value,
        total: p.total_trade,
        isSource: false
      }))
    ];

    const totalTrade = data.partners.reduce((sum, p) => sum + p.total_trade, 0);

    // Calculate angles for each partner
    const angles: { start: number; end: number; partner: typeof partners[0] }[] = [];
    let currentAngle = -Math.PI / 2; // Start at top

    partners.forEach((partner) => {
      const proportion = partner.total / (totalTrade + data.total_trade);
      const angleSpan = proportion * Math.PI * 2;
      angles.push({
        start: currentAngle,
        end: currentAngle + angleSpan,
        partner
      });
      currentAngle += angleSpan;
    });

    // Draw arcs (country segments)
    angles.forEach((angle, idx) => {
      const isHovered = idx === hoveredPartner;
      const isSource = angle.partner.isSource;

      ctx.beginPath();
      ctx.arc(centerX, centerY, radius, angle.start, angle.end);
      ctx.arc(centerX, centerY, radius + (isHovered ? 25 : 20), angle.end, angle.start, true);
      ctx.closePath();

      // Color based on type
      if (isSource) {
        ctx.fillStyle = isHovered ? BLOOMBERG.ORANGE : `${BLOOMBERG.ORANGE}CC`;
      } else {
        ctx.fillStyle = isHovered ? BLOOMBERG.CYAN : `${BLOOMBERG.CYAN}80`;
      }
      ctx.fill();

      // Border
      ctx.strokeStyle = BLOOMBERG.BORDER;
      ctx.lineWidth = 1;
      ctx.stroke();
    });

    // Draw chords (trade flows)
    data.partners.forEach((partner, idx) => {
      const sourceAngle = angles[0]; // Source country
      const targetAngle = angles[idx + 1]; // Partner country

      if (!sourceAngle || !targetAngle) return;

      const sourceMid = (sourceAngle.start + sourceAngle.end) / 2;
      const targetMid = (targetAngle.start + targetAngle.end) / 2;

      // Export chord (source to target)
      if (partner.export_value > 0) {
        drawChord(
          ctx,
          centerX,
          centerY,
          radius,
          sourceMid,
          targetMid,
          BLOOMBERG.GREEN,
          partner.export_value / totalTrade,
          idx === hoveredPartner
        );
      }

      // Import chord (target to source)
      if (partner.import_value > 0) {
        drawChord(
          ctx,
          centerX,
          centerY,
          radius,
          targetMid,
          sourceMid,
          BLOOMBERG.RED,
          partner.import_value / totalTrade,
          idx === hoveredPartner
        );
      }
    });

    // Draw labels
    ctx.fillStyle = BLOOMBERG.WHITE;
    ctx.font = '11px "Segoe UI", sans-serif';
    ctx.textBaseline = 'middle';

    angles.forEach((angle, idx) => {
      const midAngle = (angle.start + angle.end) / 2;
      const labelRadius = radius + 35;
      const x = centerX + Math.cos(midAngle) * labelRadius;
      const y = centerY + Math.sin(midAngle) * labelRadius;

      // Adjust text alignment based on position
      const isLeft = midAngle > Math.PI / 2 && midAngle < (3 * Math.PI) / 2;
      ctx.textAlign = isLeft ? 'right' : 'left';

      // Draw label
      const isHovered = idx === hoveredPartner;
      ctx.fillStyle = isHovered ? BLOOMBERG.ORANGE : angle.partner.isSource ? BLOOMBERG.YELLOW : BLOOMBERG.WHITE;
      ctx.font = isHovered ? 'bold 12px "Segoe UI"' : '11px "Segoe UI"';
      ctx.fillText(angle.partner.name, x, y);
    });

  }, [data, hoveredPartner]);

  const drawChord = (
    ctx: CanvasRenderingContext2D,
    centerX: number,
    centerY: number,
    radius: number,
    startAngle: number,
    endAngle: number,
    color: string,
    thickness: number,
    isHovered: boolean
  ) => {
    const startX = centerX + Math.cos(startAngle) * radius;
    const startY = centerY + Math.sin(startAngle) * radius;
    const endX = centerX + Math.cos(endAngle) * radius;
    const endY = centerY + Math.sin(endAngle) * radius;

    // Control points for bezier curve (towards center)
    const controlOffset = radius * 0.3;
    const control1X = centerX + Math.cos(startAngle) * controlOffset;
    const control1Y = centerY + Math.sin(startAngle) * controlOffset;
    const control2X = centerX + Math.cos(endAngle) * controlOffset;
    const control2Y = centerY + Math.sin(endAngle) * controlOffset;

    ctx.beginPath();
    ctx.moveTo(startX, startY);
    ctx.bezierCurveTo(control1X, control1Y, control2X, control2Y, endX, endY);

    // Style
    ctx.strokeStyle = isHovered ? color : `${color}60`;
    ctx.lineWidth = isHovered ? Math.max(3, thickness * 20) : Math.max(1, thickness * 15);
    ctx.stroke();
  };

  const handleMouseMove = (e: React.MouseEvent<HTMLCanvasElement>) => {
    const canvas = canvasRef.current;
    if (!canvas || !data) return;

    const rect = canvas.getBoundingClientRect();
    const x = e.clientX - rect.left;
    const y = e.clientY - rect.top;

    const centerX = rect.width / 2;
    const centerY = rect.height / 2;
    const radius = Math.min(rect.width, rect.height) * 0.35;

    // Check if mouse is over any arc
    const dx = x - centerX;
    const dy = y - centerY;
    const distance = Math.sqrt(dx * dx + dy * dy);
    const angle = Math.atan2(dy, dx);

    if (distance >= radius && distance <= radius + 25) {
      // Mouse is in arc region
      type PartnerDisplay = {
        name: string;
        export: number;
        import: number;
        total: number;
        isSource: boolean;
      };

      const partners: PartnerDisplay[] = [
        { name: data.country_name, export: 0, import: 0, total: data.total_trade, isSource: true },
        ...data.partners.map(p => ({
          name: p.partner_name,
          export: p.export_value,
          import: p.import_value,
          total: p.total_trade,
          isSource: false
        }))
      ];

      const totalTrade = data.partners.reduce((sum, p) => sum + p.total_trade, 0);
      let currentAngle = -Math.PI / 2;

      for (let i = 0; i < partners.length; i++) {
        const partner = partners[i];
        const partnerTotalTrade = partner.isSource ? data.total_trade : partner.total;
        const proportion = partnerTotalTrade / (totalTrade + data.total_trade);
        const angleSpan = proportion * Math.PI * 2;
        const endAngle = currentAngle + angleSpan;

        let normalizedAngle = angle;
        if (normalizedAngle < currentAngle) normalizedAngle += Math.PI * 2;

        if (normalizedAngle >= currentAngle && normalizedAngle < endAngle) {
          setHoveredPartner(i);

          // Show tooltip
          const tooltipContent = partner.isSource
            ? `${data.country_name}\nTotal Trade: ${formatCurrency(data.total_trade)}`
            : `${partner.name}\nExports: ${formatCurrency(partner.export)}\nImports: ${formatCurrency(partner.import)}\nTotal: ${formatCurrency(partner.total)}`;

          setTooltip({ x: e.clientX, y: e.clientY, content: tooltipContent });
          return;
        }

        currentAngle = endAngle;
      }
    }

    setHoveredPartner(null);
    setTooltip(null);
  };

  const handleMouseLeave = () => {
    setHoveredPartner(null);
    setTooltip(null);
  };

  return (
    <div style={{ position: 'relative', width: '100%', height: '100%' }}>
      <canvas
        ref={canvasRef}
        onMouseMove={handleMouseMove}
        onMouseLeave={handleMouseLeave}
        style={{
          width: '100%',
          height: '100%',
          cursor: hoveredPartner !== null ? 'pointer' : 'default'
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
            border: `1px solid ${BLOOMBERG.ORANGE}`,
            padding: '8px 12px',
            borderRadius: '4px',
            fontSize: '11px',
            color: BLOOMBERG.WHITE,
            pointerEvents: 'none',
            zIndex: 10000,
            whiteSpace: 'pre-line',
            boxShadow: `0 4px 12px ${BLOOMBERG.DARK_BG}80`,
            fontFamily: '"Segoe UI", sans-serif'
          }}
        >
          {tooltip.content}
        </div>
      )}

      {/* Right side table */}
      <div
        style={{
          position: 'absolute',
          right: '20px',
          top: '20px',
          backgroundColor: BLOOMBERG.PANEL_BG,
          border: `1px solid ${BLOOMBERG.BORDER}`,
          borderRadius: '4px',
          padding: '12px',
          maxWidth: '320px',
          maxHeight: 'calc(100% - 40px)',
          overflowY: 'auto'
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
              padding: '8px',
              marginBottom: '6px',
              backgroundColor: hoveredPartner === idx + 1 ? `${BLOOMBERG.ORANGE}20` : 'transparent',
              border: `1px solid ${hoveredPartner === idx + 1 ? BLOOMBERG.ORANGE : 'transparent'}`,
              borderRadius: '2px',
              cursor: 'pointer',
              transition: 'all 0.2s'
            }}
            onMouseEnter={() => setHoveredPartner(idx + 1)}
            onMouseLeave={() => setHoveredPartner(null)}
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
