import React, { useEffect, useRef } from 'react';

export interface CandlestickDataPoint {
  timestamp: number;
  open: number;
  high: number;
  low: number;
  close: number;
  volume?: number;
}

interface CandlestickChartProps {
  data: CandlestickDataPoint[];
  width?: number;
  height?: number;
  bullishColor?: string;
  bearishColor?: string;
  backgroundColor?: string;
  gridColor?: string;
  textColor?: string;
  showGrid?: boolean;
  showVolume?: boolean;
  showLabels?: boolean;
}

const CandlestickChart: React.FC<CandlestickChartProps> = ({
  data,
  width = 400,
  height = 250,
  bullishColor = '#00ff88',
  bearishColor = '#ff4444',
  backgroundColor = '#0a0a0a',
  gridColor = '#222222',
  textColor = '#888888',
  showGrid = true,
  showVolume = true,
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
    const highs = data.map(d => d.high);
    const lows = data.map(d => d.low);
    const maxPrice = Math.max(...highs);
    const minPrice = Math.min(...lows);
    const priceRange = maxPrice - minPrice || 1;

    // Padding
    const padding = { top: 20, right: 50, bottom: showVolume ? 70 : 30, left: 10 };
    const chartHeight = showVolume ? height * 0.7 : height - padding.top - padding.bottom;
    const chartWidth = width - padding.left - padding.right;
    const volumeHeight = showVolume ? height * 0.2 : 0;

    // Calculate candle width
    const candleWidth = Math.max(2, Math.floor(chartWidth / data.length) - 2);
    const candleSpacing = Math.floor(chartWidth / data.length);

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
      const verticalLines = Math.min(5, data.length);
      for (let i = 0; i <= verticalLines; i++) {
        const x = padding.left + (chartWidth / verticalLines) * i;
        ctx.beginPath();
        ctx.moveTo(x, padding.top);
        ctx.lineTo(x, padding.top + chartHeight);
        ctx.stroke();
      }
    }

    // Draw price labels on right side
    if (showLabels) {
      ctx.fillStyle = textColor;
      ctx.font = '9px Consolas, monospace';
      ctx.textAlign = 'left';

      const labelCount = 5;
      for (let i = 0; i <= labelCount; i++) {
        const price = minPrice + (priceRange / labelCount) * (labelCount - i);
        const y = padding.top + (chartHeight / labelCount) * i;
        ctx.fillText(price.toFixed(2), width - padding.right + 5, y + 3);
      }
    }

    // Find max volume for scaling
    const maxVolume = showVolume && data.some(d => d.volume)
      ? Math.max(...data.map(d => d.volume || 0))
      : 1;

    // Draw candlesticks
    data.forEach((candle, index) => {
      const x = padding.left + index * candleSpacing + candleSpacing / 2;

      // Determine if bullish or bearish
      const isBullish = candle.close >= candle.open;
      const color = isBullish ? bullishColor : bearishColor;

      // Calculate y positions
      const highY = padding.top + ((maxPrice - candle.high) / priceRange) * chartHeight;
      const lowY = padding.top + ((maxPrice - candle.low) / priceRange) * chartHeight;
      const openY = padding.top + ((maxPrice - candle.open) / priceRange) * chartHeight;
      const closeY = padding.top + ((maxPrice - candle.close) / priceRange) * chartHeight;

      // Draw wick (high-low line)
      ctx.strokeStyle = color;
      ctx.lineWidth = 1;
      ctx.beginPath();
      ctx.moveTo(x, highY);
      ctx.lineTo(x, lowY);
      ctx.stroke();

      // Draw body (open-close rectangle)
      const bodyTop = Math.min(openY, closeY);
      const bodyHeight = Math.abs(closeY - openY) || 1; // Ensure at least 1px for doji

      ctx.fillStyle = color;
      ctx.fillRect(x - candleWidth / 2, bodyTop, candleWidth, bodyHeight);

      // Draw volume bars if enabled
      if (showVolume && candle.volume) {
        const volumeY = padding.top + chartHeight + 10;
        const volumeBarHeight = (candle.volume / maxVolume) * volumeHeight;

        ctx.fillStyle = isBullish
          ? `${bullishColor}66` // Semi-transparent
          : `${bearishColor}66`;
        ctx.fillRect(
          x - candleWidth / 2,
          volumeY + volumeHeight - volumeBarHeight,
          candleWidth,
          volumeBarHeight
        );
      }
    });

    // Draw date labels
    if (showLabels) {
      ctx.fillStyle = textColor;
      ctx.font = '9px Consolas, monospace';
      ctx.textAlign = 'center';

      const labelIndices = [0, Math.floor(data.length / 2), data.length - 1];
      labelIndices.forEach(idx => {
        if (idx < data.length) {
          const x = padding.left + idx * candleSpacing + candleSpacing / 2;
          const date = new Date(data[idx].timestamp * 1000);
          const dateStr = `${date.getMonth() + 1}/${date.getDate()}`;
          const y = showVolume
            ? padding.top + chartHeight + volumeHeight + 20
            : height - 10;
          ctx.fillText(dateStr, x, y);
        }
      });

      // Volume label
      if (showVolume) {
        ctx.textAlign = 'left';
        ctx.fillText('VOL', padding.left, padding.top + chartHeight + 25);
      }
    }

    // Draw current price indicator (last candle)
    if (data.length > 0) {
      const lastCandle = data[data.length - 1];
      const lastPrice = lastCandle.close;
      const lastY = padding.top + ((maxPrice - lastPrice) / priceRange) * chartHeight;
      const isBullish = lastCandle.close >= lastCandle.open;
      const indicatorColor = isBullish ? bullishColor : bearishColor;

      // Draw horizontal dashed line
      ctx.strokeStyle = indicatorColor;
      ctx.lineWidth = 1;
      ctx.setLineDash([5, 5]);
      ctx.beginPath();
      ctx.moveTo(padding.left, lastY);
      ctx.lineTo(width - padding.right, lastY);
      ctx.stroke();
      ctx.setLineDash([]);

      // Draw price box
      ctx.fillStyle = indicatorColor;
      const priceText = lastPrice.toFixed(2);
      const textWidth = ctx.measureText(priceText).width;
      ctx.fillRect(width - padding.right + 2, lastY - 8, textWidth + 6, 16);
      ctx.fillStyle = backgroundColor;
      ctx.font = 'bold 10px Consolas, monospace';
      ctx.textAlign = 'left';
      ctx.fillText(priceText, width - padding.right + 5, lastY + 3);

      // Draw percentage change
      const firstCandle = data[0];
      const priceChange = lastPrice - firstCandle.open;
      const percentChange = (priceChange / firstCandle.open) * 100;
      const changeText = `${percentChange >= 0 ? '+' : ''}${percentChange.toFixed(2)}%`;

      ctx.fillStyle = indicatorColor;
      ctx.font = 'bold 11px Consolas, monospace';
      ctx.textAlign = 'left';
      ctx.fillText(changeText, padding.left + 5, padding.top + 15);
    }

  }, [data, width, height, bullishColor, bearishColor, backgroundColor, gridColor, textColor, showGrid, showVolume, showLabels]);

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

export default CandlestickChart;
