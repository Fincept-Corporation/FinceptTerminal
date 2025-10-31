import React, { useEffect, useRef } from 'react';

export interface ChartDataPoint {
  timestamp: number;
  value: number;
  label?: string;
}

interface SimpleLineChartProps {
  data: ChartDataPoint[];
  width?: number;
  height?: number;
  color?: string;
  backgroundColor?: string;
  gridColor?: string;
  textColor?: string;
  lineWidth?: number;
  showGrid?: boolean;
  showLabels?: boolean;
}

const SimpleLineChart: React.FC<SimpleLineChartProps> = ({
  data,
  width = 400,
  height = 200,
  color = '#00ff88',
  backgroundColor = '#0a0a0a',
  gridColor = '#222222',
  textColor = '#888888',
  lineWidth = 2,
  showGrid = true,
  showLabels = true,
}) => {
  const canvasRef = useRef<HTMLCanvasElement>(null);

  useEffect(() => {
    if (!canvasRef.current || data.length === 0) return;

    const canvas = canvasRef.current;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    // Set canvas size
    canvas.width = width;
    canvas.height = height;

    // Clear canvas
    ctx.fillStyle = backgroundColor;
    ctx.fillRect(0, 0, width, height);

    // Calculate data range
    const values = data.map(d => d.value);
    const maxValue = Math.max(...values);
    const minValue = Math.min(...values);
    const valueRange = maxValue - minValue || 1;

    // Padding
    const padding = { top: 20, right: 40, bottom: 30, left: 10 };
    const chartWidth = width - padding.left - padding.right;
    const chartHeight = height - padding.top - padding.bottom;

    // Draw grid
    if (showGrid) {
      ctx.strokeStyle = gridColor;
      ctx.lineWidth = 1;

      // Horizontal grid lines
      const gridLines = 5;
      for (let i = 0; i <= gridLines; i++) {
        const y = padding.top + (chartHeight / gridLines) * i;
        ctx.beginPath();
        ctx.moveTo(padding.left, y);
        ctx.lineTo(width - padding.right, y);
        ctx.stroke();
      }

      // Vertical grid lines
      const verticalLines = 4;
      for (let i = 0; i <= verticalLines; i++) {
        const x = padding.left + (chartWidth / verticalLines) * i;
        ctx.beginPath();
        ctx.moveTo(x, padding.top);
        ctx.lineTo(x, height - padding.bottom);
        ctx.stroke();
      }
    }

    // Draw price labels
    if (showLabels) {
      ctx.fillStyle = textColor;
      ctx.font = '9px Consolas, monospace';
      ctx.textAlign = 'left';

      const labelCount = 5;
      for (let i = 0; i <= labelCount; i++) {
        const value = minValue + (valueRange / labelCount) * (labelCount - i);
        const y = padding.top + (chartHeight / labelCount) * i;
        ctx.fillText(value.toFixed(2), width - padding.right + 5, y + 3);
      }

      // Draw date labels
      ctx.textAlign = 'center';
      const dateLabels = [0, Math.floor(data.length / 2), data.length - 1];
      dateLabels.forEach(idx => {
        if (idx < data.length) {
          const x = padding.left + (chartWidth / (data.length - 1)) * idx;
          const date = new Date(data[idx].timestamp * 1000);
          const dateStr = `${date.getMonth() + 1}/${date.getDate()}`;
          ctx.fillText(dateStr, x, height - padding.bottom + 15);
        }
      });
    }

    // Draw line
    ctx.strokeStyle = color;
    ctx.lineWidth = lineWidth;
    ctx.beginPath();

    data.forEach((point, index) => {
      const x = padding.left + (chartWidth / (data.length - 1)) * index;
      const normalizedValue = (point.value - minValue) / valueRange;
      const y = height - padding.bottom - (normalizedValue * chartHeight);

      if (index === 0) {
        ctx.moveTo(x, y);
      } else {
        ctx.lineTo(x, y);
      }
    });

    ctx.stroke();

    // Draw glow effect
    ctx.shadowColor = color;
    ctx.shadowBlur = 8;
    ctx.stroke();
    ctx.shadowBlur = 0;

    // Draw points
    data.forEach((point, index) => {
      const x = padding.left + (chartWidth / (data.length - 1)) * index;
      const normalizedValue = (point.value - minValue) / valueRange;
      const y = height - padding.bottom - (normalizedValue * chartHeight);

      // Only draw points at start, end, and every 10th point for clarity
      if (index === 0 || index === data.length - 1 || index % 10 === 0) {
        ctx.fillStyle = color;
        ctx.beginPath();
        ctx.arc(x, y, 2, 0, Math.PI * 2);
        ctx.fill();
      }
    });

    // Draw current price indicator
    if (data.length > 0) {
      const lastPoint = data[data.length - 1];
      const lastX = padding.left + chartWidth;
      const normalizedValue = (lastPoint.value - minValue) / valueRange;
      const lastY = height - padding.bottom - (normalizedValue * chartHeight);

      // Draw horizontal line
      ctx.strokeStyle = color;
      ctx.lineWidth = 1;
      ctx.setLineDash([5, 5]);
      ctx.beginPath();
      ctx.moveTo(padding.left, lastY);
      ctx.lineTo(width - padding.right, lastY);
      ctx.stroke();
      ctx.setLineDash([]);

      // Draw price box
      ctx.fillStyle = color;
      const priceText = lastPoint.value.toFixed(2);
      const textWidth = ctx.measureText(priceText).width;
      ctx.fillRect(width - padding.right + 2, lastY - 8, textWidth + 6, 16);
      ctx.fillStyle = backgroundColor;
      ctx.font = 'bold 10px Consolas, monospace';
      ctx.textAlign = 'left';
      ctx.fillText(priceText, width - padding.right + 5, lastY + 3);
    }

  }, [data, width, height, color, backgroundColor, gridColor, textColor, lineWidth, showGrid, showLabels]);

  if (data.length === 0) {
    return (
      <div
        style={{
          width: `${width}px`,
          height: `${height}px`,
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          backgroundColor,
          color: textColor,
          fontSize: '11px',
          fontFamily: 'Consolas, monospace',
        }}
      >
        No chart data available
      </div>
    );
  }

  return <canvas ref={canvasRef} style={{ display: 'block' }} />;
};

export default SimpleLineChart;
