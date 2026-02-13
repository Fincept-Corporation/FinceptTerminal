// CryptoOrderBook.tsx - Professional Order Book Display
// Direct Kraken WebSocket connection with incremental updates
// Extended with: Tick History Buffer, Volume-at-Price, Depth Imbalance, Trade Signals
// Performance: rAF-throttled rendering, DOM recycling, canvas size caching
import React, { useCallback, useRef, useEffect, memo, useState } from 'react';
import { FINCEPT } from '../constants';

const NUM_LEVELS = 12;
const MAX_TICK_HISTORY = 3600;
const IMBALANCE_BUY_THRESHOLD = 0.3;
const IMBALANCE_SELL_THRESHOLD = -0.3;
const RISE_BUY_THRESHOLD = 0.15;
const RISE_SELL_THRESHOLD = -0.15;
const TICK_CAPTURE_INTERVAL = 1000;
const CANVAS_VISIBLE_POINTS = 300;

// Pre-allocated reusable arrays to avoid GC pressure on every frame
const _askSortBuf: [number, number][] = [];
const _bidSortBuf: [number, number][] = [];
const _cumBuf: number[] = [];

/** Sort order book map into a reusable buffer via insertion sort. Returns item count. */
function sortInto(book: Record<string, number>, buf: [number, number][], asc: boolean): number {
  const keys = Object.keys(book);
  const n = keys.length;
  while (buf.length < n) buf.push([0, 0]);
  for (let i = 0; i < n; i++) {
    buf[i][0] = Number(keys[i]);
    buf[i][1] = book[keys[i]];
  }
  // Insertion sort — optimal for small n (~25), nearly sorted data
  for (let i = 1; i < n; i++) {
    const kp = buf[i][0], kq = buf[i][1];
    let j = i - 1;
    if (asc) {
      while (j >= 0 && buf[j][0] > kp) { buf[j + 1][0] = buf[j][0]; buf[j + 1][1] = buf[j][1]; j--; }
    } else {
      while (j >= 0 && buf[j][0] < kp) { buf[j + 1][0] = buf[j][0]; buf[j + 1][1] = buf[j][1]; j--; }
    }
    buf[j + 1][0] = kp; buf[j + 1][1] = kq;
  }
  return n;
}

type OrderBookViewType = 'orderbook' | 'volume' | 'imbalance' | 'signals';

interface TickSnapshot {
  timestamp: number;
  bestBid: number;
  bestAsk: number;
  bidQty1: number;
  bidQty2: number;
  bidQty3: number;
  askQty1: number;
  askQty2: number;
  askQty3: number;
  totalBidQty: number;
  totalAskQty: number;
  imbalance: number;
  riseRatio30: number;
  riseRatio60: number;
  riseRatio300: number;
}

interface Signal {
  timestamp: number;
  direction: 'BUY' | 'SELL' | 'NEUTRAL';
  imbalance: number;
  riseRatio: number;
  price: number;
}

interface CryptoOrderBookProps {
  currentPrice: number;
  selectedSymbol: string;
  activeBroker: string | null;
}

// Pre-computed format caches to avoid toLocaleString overhead
const priceFormatCache = new Map<string, string>();
const formatPrice = (price: number): string => {
  const key = price.toFixed(price >= 1000 ? 2 : price >= 1 ? 2 : 6);
  let cached = priceFormatCache.get(key);
  if (!cached) {
    if (price >= 1000) cached = price.toLocaleString(undefined, { minimumFractionDigits: 2, maximumFractionDigits: 2 });
    else if (price >= 1) cached = price.toFixed(2);
    else cached = price.toFixed(6);
    if (priceFormatCache.size > 500) priceFormatCache.clear();
    priceFormatCache.set(key, cached);
  }
  return cached;
};

const formatQty = (qty: number): string => {
  if (qty >= 1000) return qty.toLocaleString(undefined, { maximumFractionDigits: 2 });
  if (qty >= 1) return qty.toFixed(4);
  return qty.toFixed(6);
};

export const CryptoOrderBook = memo(function CryptoOrderBook({
  currentPrice,
  selectedSymbol,
  activeBroker,
}: CryptoOrderBookProps) {
  const [activeView, setActiveView] = useState<OrderBookViewType>('orderbook');
  const [status, setStatus] = useState<'connecting' | 'connected' | 'disconnected'>('disconnected');
  const activeViewRef = useRef<OrderBookViewType>('orderbook');

  const wsRef = useRef<WebSocket | null>(null);
  const asksRef = useRef<Record<string, number>>({});
  const bidsRef = useRef<Record<string, number>>({});
  const tickHistoryRef = useRef<TickSnapshot[]>([]);
  const signalHistoryRef = useRef<Signal[]>([]);
  const cumulativePnlRef = useRef(0);
  const lastSignalPriceRef = useRef(0);
  const activeSignalRef = useRef<'BUY' | 'SELL' | null>(null);
  const lastTickCaptureRef = useRef(0);
  const rafIdRef = useRef(0);
  const dirtyRef = useRef(false);

  // Canvas refs for order book (Binance-style canvas rendering)
  const asksCanvasRef = useRef<HTMLCanvasElement>(null);
  const bidsCanvasRef = useRef<HTMLCanvasElement>(null);
  const obCanvasSizeRef = useRef({ aw: 0, ah: 0, bw: 0, bh: 0 }); // cached canvas sizes
  const spreadValueRef = useRef<HTMLSpanElement>(null);
  const spreadPercentRef = useRef<HTMLSpanElement>(null);
  const priceRef = useRef<HTMLDivElement>(null);
  const levelCountRef = useRef<HTMLSpanElement>(null);
  const updateTimeRef = useRef<HTMLSpanElement>(null);
  const askTotalRef = useRef<HTMLSpanElement>(null);
  const bidTotalRef = useRef<HTMLSpanElement>(null);
  const tickCountRef = useRef<HTMLSpanElement>(null);

  // Canvas refs + cached sizes
  const imbalanceCanvasRef = useRef<HTMLCanvasElement>(null);
  const riseCanvasRef = useRef<HTMLCanvasElement>(null);
  const canvasSizeRef = useRef({ w: 0, h: 0 });
  const imbalanceValueRef = useRef<HTMLSpanElement>(null);
  const riseValueRef = useRef<HTMLSpanElement>(null);
  const bidTotalImbalanceRef = useRef<HTMLSpanElement>(null);
  const askTotalImbalanceRef = useRef<HTMLSpanElement>(null);

  // Volume + signals refs
  const volumeContainerRef = useRef<HTMLDivElement>(null);
  const signalIndicatorRef = useRef<HTMLDivElement>(null);
  const signalHistoryContainerRef = useRef<HTMLDivElement>(null);
  const lastSignalRenderCountRef = useRef(0);

  // Keep activeViewRef in sync without re-renders
  useEffect(() => { activeViewRef.current = activeView; }, [activeView]);

  // Capture tick snapshot using pre-sorted buffers (called from renderFrame, throttled to 1/sec)
  const captureTickSnapshot = useCallback((askCount: number, bidCount: number) => {
    const now = Date.now();
    if (now - lastTickCaptureRef.current < TICK_CAPTURE_INTERVAL) return;
    if (askCount === 0 || bidCount === 0) return;
    lastTickCaptureRef.current = now;

    // Use the already-sorted global buffers (filled by renderFrame)
    let totalBidQty = 0;
    for (let i = 0; i < bidCount; i++) totalBidQty += _bidSortBuf[i][1];
    let totalAskQty = 0;
    for (let i = 0; i < askCount; i++) totalAskQty += _askSortBuf[i][1];

    // Weighted depth imbalance (SGX: 50/30/20 top 3)
    const wBid = 50 * (_bidSortBuf[0]?.[1] || 0) + 30 * (_bidSortBuf[1]?.[1] || 0) + 20 * (_bidSortBuf[2]?.[1] || 0);
    const wAsk = 50 * (_askSortBuf[0]?.[1] || 0) + 30 * (_askSortBuf[1]?.[1] || 0) + 20 * (_askSortBuf[2]?.[1] || 0);
    const imbalance = (wAsk + wBid) > 0 ? (wBid - wAsk) / (wBid + wAsk) : 0;

    const history = tickHistoryRef.current;
    const bestAsk = _askSortBuf[0][0];
    const bestBid = _bidSortBuf[0][0];

    const calcRise = (lookbackMs: number) => {
      const cutoff = now - lookbackMs;
      for (let i = history.length - 1; i >= 0; i--) {
        if (history[i].timestamp <= cutoff && history[i].bestAsk > 0) {
          return ((bestAsk - history[i].bestAsk) / history[i].bestAsk) * 100;
        }
      }
      return 0;
    };

    const snapshot: TickSnapshot = {
      timestamp: now,
      bestBid,
      bestAsk,
      bidQty1: _bidSortBuf[0]?.[1] || 0,
      bidQty2: _bidSortBuf[1]?.[1] || 0,
      bidQty3: _bidSortBuf[2]?.[1] || 0,
      askQty1: _askSortBuf[0]?.[1] || 0,
      askQty2: _askSortBuf[1]?.[1] || 0,
      askQty3: _askSortBuf[2]?.[1] || 0,
      totalBidQty,
      totalAskQty,
      imbalance,
      riseRatio30: calcRise(30000),
      riseRatio60: calcRise(60000),
      riseRatio300: calcRise(300000),
    };

    history.push(snapshot);
    if (history.length > MAX_TICK_HISTORY) history.splice(0, history.length - MAX_TICK_HISTORY);

    // Signal generation
    const riseRatio = snapshot.riseRatio60;
    let direction: 'BUY' | 'SELL' | 'NEUTRAL' = 'NEUTRAL';
    if (imbalance > IMBALANCE_BUY_THRESHOLD && riseRatio > RISE_BUY_THRESHOLD) direction = 'BUY';
    else if (imbalance < IMBALANCE_SELL_THRESHOLD && riseRatio < RISE_SELL_THRESHOLD) direction = 'SELL';

    const midPrice = (bestBid + bestAsk) / 2;
    if (direction !== 'NEUTRAL') {
      const prev = activeSignalRef.current;
      if (prev && prev !== direction && lastSignalPriceRef.current > 0) {
        cumulativePnlRef.current += prev === 'BUY'
          ? midPrice - lastSignalPriceRef.current
          : lastSignalPriceRef.current - midPrice;
      }
      if (prev !== direction) {
        activeSignalRef.current = direction;
        lastSignalPriceRef.current = midPrice;
        signalHistoryRef.current.push({ timestamp: now, direction, imbalance, riseRatio, price: midPrice });
        if (signalHistoryRef.current.length > 200) signalHistoryRef.current.splice(0, signalHistoryRef.current.length - 200);
      }
    }
  }, []);

  // Canvas-based order book rendering (Binance-style: no DOM nodes, pure canvas paint)
  const paintOrderBookCanvas = useCallback((
    canvas: HTMLCanvasElement | null,
    buf: [number, number][],
    count: number,
    maxTotal: number,
    side: 'ask' | 'bid',
    reverse: boolean,
    sizeCache: { w: number; h: number }
  ) => {
    if (!canvas) return;
    const ctx = canvas.getContext('2d', { alpha: false });
    if (!ctx) return;

    // Resize canvas buffer to match CSS size (only when changed)
    const rect = canvas.getBoundingClientRect();
    const dpr = window.devicePixelRatio || 1;
    const w = Math.round(rect.width * dpr);
    const h = Math.round(rect.height * dpr);
    if (sizeCache.w !== w || sizeCache.h !== h) {
      sizeCache.w = w;
      sizeCache.h = h;
      canvas.width = w;
      canvas.height = h;
    }

    const levels = Math.min(count, NUM_LEVELS);
    const cssW = rect.width;
    const cssH = rect.height;

    // Clear
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    ctx.fillStyle = '#0A0A0A';
    ctx.fillRect(0, 0, cssW, cssH);

    if (levels === 0) {
      ctx.font = '10px "IBM Plex Mono", monospace';
      ctx.fillStyle = '#666';
      ctx.textAlign = 'center';
      ctx.textBaseline = 'middle';
      ctx.fillText(`Waiting for ${side} data...`, cssW / 2, cssH / 2);
      return;
    }

    // Compute cumulative
    _cumBuf.length = levels;
    let cum = 0;
    for (let i = 0; i < levels; i++) { cum += buf[i][1]; _cumBuf[i] = cum; }

    const ROW_H = Math.min(22, cssH / levels);
    const PAD_X = 8;
    const priceColor = side === 'ask' ? '#EF4444' : '#22C55E';
    const depthColor = side === 'ask' ? 'rgba(239,68,68,0.12)' : 'rgba(34,197,94,0.12)';

    // Column positions: Price (left), Size (center-right), Total (right)
    const col1 = PAD_X;
    const col2 = cssW * 0.55;
    const col3 = cssW - PAD_X;

    ctx.textBaseline = 'middle';
    ctx.font = '600 11px "IBM Plex Mono", monospace';

    // Asks reversed: render highest at top (row 0 = buf[levels-1])
    // Bids normal: render highest at top (row 0 = buf[0])
    // With flex-end on asks, we want: bottom row = best ask (lowest price = buf[0])
    // So row 0 at visual top = buf[levels-1] (highest ask), going down to buf[0]
    const startY = reverse ? (cssH - levels * ROW_H) : 0; // asks: push to bottom

    for (let rowIdx = 0; rowIdx < levels; rowIdx++) {
      const dataIdx = reverse ? (levels - 1 - rowIdx) : rowIdx;
      const price = buf[dataIdx][0];
      const qty = buf[dataIdx][1];
      const cumVal = _cumBuf[dataIdx];
      const y = startY + rowIdx * ROW_H;

      // Depth bar
      const barW = (cumVal / maxTotal) * cssW;
      ctx.fillStyle = depthColor;
      if (side === 'ask') {
        ctx.fillRect(cssW - barW, y + 1, barW, ROW_H - 2);
      } else {
        ctx.fillRect(0, y + 1, barW, ROW_H - 2);
      }

      const textY = y + ROW_H / 2;

      // Price
      ctx.fillStyle = priceColor;
      ctx.textAlign = 'left';
      ctx.fillText(formatPrice(price), col1, textY);

      // Size
      ctx.fillStyle = '#E5E5E5';
      ctx.textAlign = 'right';
      ctx.fillText(formatQty(qty), col2, textY);

      // Total
      ctx.fillStyle = '#888';
      ctx.font = '10px "IBM Plex Mono", monospace';
      ctx.fillText(formatQty(cumVal), col3, textY);
      ctx.font = '600 11px "IBM Plex Mono", monospace';
    }
  }, []);

  // Render canvas charts (only when imbalance tab is active)
  const renderCanvasCharts = useCallback(() => {
    const history = tickHistoryRef.current;
    if (history.length === 0) return;
    const latest = history[history.length - 1];

    // Update stat text nodes
    if (imbalanceValueRef.current) {
      imbalanceValueRef.current.textContent = latest.imbalance.toFixed(4);
      imbalanceValueRef.current.style.color = latest.imbalance > 0.3 ? FINCEPT.GREEN : latest.imbalance < -0.3 ? FINCEPT.RED : FINCEPT.WHITE;
    }
    if (riseValueRef.current) {
      riseValueRef.current.textContent = `${latest.riseRatio60.toFixed(4)}%`;
      riseValueRef.current.style.color = latest.riseRatio60 > 0 ? FINCEPT.GREEN : latest.riseRatio60 < 0 ? FINCEPT.RED : FINCEPT.WHITE;
    }
    if (bidTotalImbalanceRef.current) bidTotalImbalanceRef.current.textContent = formatQty(latest.totalBidQty);
    if (askTotalImbalanceRef.current) askTotalImbalanceRef.current.textContent = formatQty(latest.totalAskQty);

    // Imbalance canvas
    const canvas = imbalanceCanvasRef.current;
    if (canvas) {
      const ctx = canvas.getContext('2d');
      if (ctx) {
        // Only resize if dimensions actually changed
        const rect = canvas.getBoundingClientRect();
        const targetW = Math.round(rect.width * 2);
        const targetH = Math.round(rect.height * 2);
        if (canvas.width !== targetW || canvas.height !== targetH) {
          canvas.width = targetW;
          canvas.height = targetH;
        }
        const dw = rect.width;
        const dh = rect.height;
        ctx.setTransform(2, 0, 0, 2, 0, 0);
        ctx.clearRect(0, 0, dw, dh);

        ctx.fillStyle = '#0A0A0A';
        ctx.fillRect(0, 0, dw, dh);

        const midY = dh / 2;
        ctx.strokeStyle = '#1A1A1A';
        ctx.lineWidth = 0.5;
        ctx.beginPath(); ctx.moveTo(0, midY); ctx.lineTo(dw, midY); ctx.stroke();

        // Zones
        const buyZoneY = midY * 0.7;
        const sellZoneY = midY * 1.3;
        ctx.fillStyle = 'rgba(0,214,111,0.05)';
        ctx.fillRect(0, 0, dw, buyZoneY);
        ctx.fillStyle = 'rgba(255,59,59,0.05)';
        ctx.fillRect(0, sellZoneY, dw, dh - sellZoneY);

        ctx.setLineDash([3, 3]);
        ctx.strokeStyle = 'rgba(0,214,111,0.3)';
        ctx.beginPath(); ctx.moveTo(0, buyZoneY); ctx.lineTo(dw, buyZoneY); ctx.stroke();
        ctx.strokeStyle = 'rgba(255,59,59,0.3)';
        ctx.beginPath(); ctx.moveTo(0, sellZoneY); ctx.lineTo(dw, sellZoneY); ctx.stroke();
        ctx.setLineDash([]);

        const visible = history.length <= CANVAS_VISIBLE_POINTS ? history : history.slice(-CANVAS_VISIBLE_POINTS);
        if (visible.length > 1) {
          const step = dw / (visible.length - 1);
          ctx.beginPath();
          ctx.lineWidth = 1.5;
          for (let i = 0; i < visible.length; i++) {
            const y = midY - (visible[i].imbalance * midY);
            if (i === 0) ctx.moveTo(0, y); else ctx.lineTo(i * step, y);
          }
          const latestImb = visible[visible.length - 1].imbalance;
          ctx.strokeStyle = latestImb > 0.3 ? FINCEPT.GREEN : latestImb < -0.3 ? FINCEPT.RED : FINCEPT.CYAN;
          ctx.stroke();
        }

        ctx.fillStyle = FINCEPT.GRAY;
        ctx.font = '9px monospace';
        ctx.fillText('+1.0', 2, 10);
        ctx.fillText(' 0.0', 2, midY + 3);
        ctx.fillText('-1.0', 2, dh - 3);
      }
    }

    // Rise ratio canvas
    const riseCanvas = riseCanvasRef.current;
    if (riseCanvas) {
      const ctx = riseCanvas.getContext('2d');
      if (ctx) {
        const rect = riseCanvas.getBoundingClientRect();
        const targetW = Math.round(rect.width * 2);
        const targetH = Math.round(rect.height * 2);
        if (riseCanvas.width !== targetW || riseCanvas.height !== targetH) {
          riseCanvas.width = targetW;
          riseCanvas.height = targetH;
        }
        const dw = rect.width;
        const dh = rect.height;
        ctx.setTransform(2, 0, 0, 2, 0, 0);
        ctx.clearRect(0, 0, dw, dh);

        ctx.fillStyle = '#0A0A0A';
        ctx.fillRect(0, 0, dw, dh);

        const midY = dh / 2;
        ctx.strokeStyle = '#1A1A1A';
        ctx.lineWidth = 0.5;
        ctx.beginPath(); ctx.moveTo(0, midY); ctx.lineTo(dw, midY); ctx.stroke();

        const visible = history.length <= CANVAS_VISIBLE_POINTS ? history : history.slice(-CANVAS_VISIBLE_POINTS);
        if (visible.length > 1) {
          let maxRise = 0.5;
          for (let i = 0; i < visible.length; i++) {
            const abs60 = Math.abs(visible[i].riseRatio60);
            if (abs60 > maxRise) maxRise = abs60;
          }
          const step = dw / (visible.length - 1);

          // 30s
          ctx.beginPath(); ctx.lineWidth = 0.8; ctx.strokeStyle = 'rgba(157,78,221,0.4)';
          for (let i = 0; i < visible.length; i++) {
            const y = midY - (visible[i].riseRatio30 / maxRise) * midY;
            if (i === 0) ctx.moveTo(0, y); else ctx.lineTo(i * step, y);
          }
          ctx.stroke();

          // 60s
          ctx.beginPath(); ctx.lineWidth = 1.5;
          const lr = visible[visible.length - 1].riseRatio60;
          ctx.strokeStyle = lr > 0 ? FINCEPT.GREEN : lr < 0 ? FINCEPT.RED : FINCEPT.CYAN;
          for (let i = 0; i < visible.length; i++) {
            const y = midY - (visible[i].riseRatio60 / maxRise) * midY;
            if (i === 0) ctx.moveTo(0, y); else ctx.lineTo(i * step, y);
          }
          ctx.stroke();

          // 300s
          ctx.beginPath(); ctx.lineWidth = 0.8; ctx.strokeStyle = 'rgba(255,136,0,0.4)';
          for (let i = 0; i < visible.length; i++) {
            const y = midY - (visible[i].riseRatio300 / maxRise) * midY;
            if (i === 0) ctx.moveTo(0, y); else ctx.lineTo(i * step, y);
          }
          ctx.stroke();
        }

        ctx.fillStyle = FINCEPT.GRAY;
        ctx.font = '9px monospace';
        ctx.fillText('RISE %', 2, 10);
      }
    }
  }, []);

  // Volume view (only rebuild when data changes significantly)
  const renderVolumeView = useCallback(() => {
    const container = volumeContainerRef.current;
    if (!container) return;

    const asks = asksRef.current;
    const bids = bidsRef.current;
    const levels: { price: number; bidQty: number; askQty: number; total: number }[] = [];

    Object.entries(bids).forEach(([p, q]) => levels.push({ price: parseFloat(p), bidQty: q, askQty: 0, total: q }));
    Object.entries(asks).forEach(([p, q]) => {
      const price = parseFloat(p);
      const existing = levels.find(l => l.price === price);
      if (existing) { existing.askQty = q; existing.total += q; }
      else levels.push({ price, bidQty: 0, askQty: q, total: q });
    });

    levels.sort((a, b) => b.price - a.price);

    if (levels.length === 0) {
      container.innerHTML = `<div style="display:flex;align-items:center;justify-content:center;height:100%;color:${FINCEPT.GRAY};font-size:11px;">Waiting for order book data...</div>`;
      return;
    }

    const maxTotal = Math.max(...levels.map(l => l.total));
    const poc = levels.reduce((max, l) => l.total > max.total ? l : max, levels[0]);

    container.innerHTML = `
      <div style="padding:8px 10px;display:flex;justify-content:space-between;border-bottom:1px solid ${FINCEPT.BORDER};font-size:9px;font-family:monospace;">
        <span style="color:${FINCEPT.ORANGE};font-weight:700;">VOLUME BY PRICE (LIVE)</span>
        <span style="color:${FINCEPT.GRAY};">POC: <span style="color:${FINCEPT.CYAN};font-weight:700;">${formatPrice(poc.price)}</span></span>
      </div>
      <div style="padding:6px 10px;display:flex;gap:16px;border-bottom:1px solid ${FINCEPT.BORDER};font-size:10px;font-family:monospace;background:#0A0A0A;">
        <div><span style="color:${FINCEPT.GRAY};font-size:8px;">LEVELS</span><br/><span style="color:${FINCEPT.CYAN};font-weight:700;">${levels.length}</span></div>
        <div><span style="color:${FINCEPT.GRAY};font-size:8px;">MAX VOL</span><br/><span style="color:${FINCEPT.PURPLE};font-weight:700;">${formatQty(maxTotal)}</span></div>
        <div><span style="color:${FINCEPT.GRAY};font-size:8px;">POC VOL</span><br/><span style="color:${FINCEPT.ORANGE};font-weight:700;">${formatQty(poc.total)}</span></div>
      </div>
      <div style="padding:4px 10px;display:grid;grid-template-columns:80px 1fr 60px;font-size:8px;color:${FINCEPT.GRAY};text-transform:uppercase;letter-spacing:0.5px;border-bottom:1px solid ${FINCEPT.BORDER};">
        <span>Price</span><span style="text-align:center;">Volume</span><span style="text-align:right;">Size</span>
      </div>
      ${levels.slice(0, 25).map(l => {
        const bidPct = maxTotal > 0 ? (l.bidQty / maxTotal) * 100 : 0;
        const askPct = maxTotal > 0 ? (l.askQty / maxTotal) * 100 : 0;
        const isPOC = l.price === poc.price;
        const priceColor = l.bidQty > 0 && l.askQty === 0 ? FINCEPT.GREEN : l.askQty > 0 && l.bidQty === 0 ? FINCEPT.RED : FINCEPT.WHITE;
        return `<div style="display:grid;grid-template-columns:80px 1fr 60px;padding:2px 10px;font-size:10px;font-family:monospace;align-items:center;height:20px;${isPOC ? `background:rgba(0,229,255,0.08);border-left:2px solid ${FINCEPT.CYAN};` : ''}">
            <span style="color:${priceColor};font-weight:${isPOC ? 700 : 500};">${formatPrice(l.price)}</span>
            <div style="display:flex;height:12px;gap:1px;">
              <div style="width:${bidPct}%;background:${FINCEPT.GREEN};border-radius:1px;opacity:0.7;"></div>
              <div style="width:${askPct}%;background:${FINCEPT.RED};border-radius:1px;opacity:0.7;"></div>
            </div>
            <span style="text-align:right;color:${FINCEPT.GRAY};font-size:9px;">${formatQty(l.total)}</span>
          </div>`;
      }).join('')}
      <div style="display:flex;justify-content:center;gap:16px;padding:8px;font-size:9px;font-family:monospace;border-top:1px solid ${FINCEPT.BORDER};">
        <span style="display:flex;align-items:center;gap:4px;"><span style="width:8px;height:8px;background:${FINCEPT.GREEN};display:inline-block;"></span><span style="color:${FINCEPT.WHITE};">BID</span></span>
        <span style="display:flex;align-items:center;gap:4px;"><span style="width:8px;height:8px;background:${FINCEPT.RED};display:inline-block;"></span><span style="color:${FINCEPT.WHITE};">ASK</span></span>
        <span style="display:flex;align-items:center;gap:4px;"><span style="width:8px;height:8px;background:${FINCEPT.CYAN};display:inline-block;"></span><span style="color:${FINCEPT.WHITE};">POC</span></span>
      </div>`;
  }, []);

  // Signals view (only rebuild signal list when count changes)
  const renderSignalsView = useCallback(() => {
    const history = tickHistoryRef.current;
    const signals = signalHistoryRef.current;
    if (history.length === 0) return;

    const latest = history[history.length - 1];
    const imb = latest.imbalance;
    const rise = latest.riseRatio60;

    let currentSignal: 'BUY' | 'SELL' | 'NEUTRAL' = 'NEUTRAL';
    if (imb > IMBALANCE_BUY_THRESHOLD && rise > RISE_BUY_THRESHOLD) currentSignal = 'BUY';
    else if (imb < IMBALANCE_SELL_THRESHOLD && rise < RISE_SELL_THRESHOLD) currentSignal = 'SELL';

    const signalColor = currentSignal === 'BUY' ? FINCEPT.GREEN : currentSignal === 'SELL' ? FINCEPT.RED : FINCEPT.GRAY;
    const pnl = cumulativePnlRef.current;

    if (signalIndicatorRef.current) {
      signalIndicatorRef.current.innerHTML = `
        <div style="text-align:center;padding:16px;">
          <div style="font-size:9px;color:${FINCEPT.GRAY};letter-spacing:1px;margin-bottom:6px;">CURRENT SIGNAL</div>
          <div style="font-size:28px;font-weight:700;color:${signalColor};font-family:monospace;text-shadow:0 0 20px ${signalColor}40;">${currentSignal}</div>
          <div style="margin-top:8px;display:flex;justify-content:center;gap:20px;font-size:10px;font-family:monospace;">
            <div><span style="color:${FINCEPT.GRAY};">IMB: </span><span style="color:${imb > 0 ? FINCEPT.GREEN : imb < 0 ? FINCEPT.RED : FINCEPT.WHITE};font-weight:600;">${imb.toFixed(4)}</span></div>
            <div><span style="color:${FINCEPT.GRAY};">RISE: </span><span style="color:${rise > 0 ? FINCEPT.GREEN : rise < 0 ? FINCEPT.RED : FINCEPT.WHITE};font-weight:600;">${rise.toFixed(4)}%</span></div>
          </div>
        </div>
        <div style="display:flex;justify-content:space-around;padding:8px 12px;border-top:1px solid ${FINCEPT.BORDER};border-bottom:1px solid ${FINCEPT.BORDER};background:#0A0A0A;">
          <div style="text-align:center;">
            <div style="font-size:8px;color:${FINCEPT.GRAY};letter-spacing:0.5px;">P&L (PAPER)</div>
            <div style="font-size:14px;font-weight:700;color:${pnl >= 0 ? FINCEPT.GREEN : FINCEPT.RED};font-family:monospace;">${pnl >= 0 ? '+' : ''}${formatPrice(pnl)}</div>
          </div>
          <div style="text-align:center;">
            <div style="font-size:8px;color:${FINCEPT.GRAY};letter-spacing:0.5px;">SIGNALS</div>
            <div style="font-size:14px;font-weight:700;color:${FINCEPT.CYAN};font-family:monospace;">${signals.length}</div>
          </div>
          <div style="text-align:center;">
            <div style="font-size:8px;color:${FINCEPT.GRAY};letter-spacing:0.5px;">TICKS</div>
            <div style="font-size:14px;font-weight:700;color:${FINCEPT.WHITE};font-family:monospace;">${history.length}</div>
          </div>
        </div>`;
    }

    // Only rebuild signal list when new signals appear
    if (signalHistoryContainerRef.current && signals.length !== lastSignalRenderCountRef.current) {
      lastSignalRenderCountRef.current = signals.length;
      const recent = signals.slice(-30).reverse();
      if (recent.length === 0) {
        signalHistoryContainerRef.current.innerHTML = `<div style="display:flex;align-items:center;justify-content:center;height:100%;color:${FINCEPT.GRAY};font-size:10px;font-family:monospace;">Waiting for signals...</div>`;
      } else {
        signalHistoryContainerRef.current.innerHTML = `
          <div style="display:grid;grid-template-columns:65px 50px 70px 70px 1fr;padding:4px 8px;font-size:8px;color:${FINCEPT.GRAY};text-transform:uppercase;letter-spacing:0.3px;border-bottom:1px solid ${FINCEPT.BORDER};">
            <span>Time</span><span>Signal</span><span style="text-align:right;">Imbalance</span><span style="text-align:right;">Rise %</span><span style="text-align:right;">Price</span>
          </div>
          ${recent.map(s => {
            const time = new Date(s.timestamp).toLocaleTimeString();
            const sc = s.direction === 'BUY' ? FINCEPT.GREEN : FINCEPT.RED;
            return `<div style="display:grid;grid-template-columns:65px 50px 70px 70px 1fr;padding:3px 8px;font-size:10px;font-family:monospace;border-bottom:1px solid #1A1A1A;">
                <span style="color:${FINCEPT.GRAY};">${time}</span>
                <span style="color:${sc};font-weight:700;">${s.direction}</span>
                <span style="text-align:right;color:${s.imbalance > 0 ? FINCEPT.GREEN : FINCEPT.RED};">${s.imbalance.toFixed(3)}</span>
                <span style="text-align:right;color:${s.riseRatio > 0 ? FINCEPT.GREEN : FINCEPT.RED};">${s.riseRatio.toFixed(3)}</span>
                <span style="text-align:right;color:${FINCEPT.WHITE};">${formatPrice(s.price)}</span>
              </div>`;
          }).join('')}`;
      }
    }
  }, []);

  // Cached time string (avoid toLocaleTimeString on every frame)
  const lastTimeStrRef = useRef('');
  const lastTimeSecRef = useRef(0);

  // Main render loop - rAF throttled, only renders active view
  const renderFrame = useCallback(() => {
    if (!dirtyRef.current) return;
    dirtyRef.current = false;

    // Sort both sides once into shared buffers (reused by tick capture + render)
    const askCount = sortInto(asksRef.current, _askSortBuf, true);   // ascending
    const bidCount = sortInto(bidsRef.current, _bidSortBuf, false);  // descending

    // Capture tick data (throttled internally to 1/sec)
    captureTickSnapshot(askCount, bidCount);

    const view = activeViewRef.current;

    if (view === 'orderbook') {
      const levelsAsk = Math.min(askCount, NUM_LEVELS);
      const levelsBid = Math.min(bidCount, NUM_LEVELS);

      let askTotal = 0;
      for (let i = 0; i < levelsAsk; i++) askTotal += _askSortBuf[i][1];
      let bidTotal = 0;
      for (let i = 0; i < levelsBid; i++) bidTotal += _bidSortBuf[i][1];
      const maxTotal = Math.max(askTotal, bidTotal) || 1;

      const askSizeCache = { w: obCanvasSizeRef.current.aw, h: obCanvasSizeRef.current.ah };
      const bidSizeCache = { w: obCanvasSizeRef.current.bw, h: obCanvasSizeRef.current.bh };
      paintOrderBookCanvas(asksCanvasRef.current, _askSortBuf, askCount, maxTotal, 'ask', true, askSizeCache);
      paintOrderBookCanvas(bidsCanvasRef.current, _bidSortBuf, bidCount, maxTotal, 'bid', false, bidSizeCache);
      obCanvasSizeRef.current.aw = askSizeCache.w;
      obCanvasSizeRef.current.ah = askSizeCache.h;
      obCanvasSizeRef.current.bw = bidSizeCache.w;
      obCanvasSizeRef.current.bh = bidSizeCache.h;

      if (askCount > 0 && bidCount > 0) {
        const bestAsk = _askSortBuf[0][0];
        const bestBid = _bidSortBuf[0][0];
        const spread = bestAsk - bestBid;
        const spreadStr = formatPrice(spread);
        if (spreadValueRef.current && spreadValueRef.current.textContent !== spreadStr) spreadValueRef.current.textContent = spreadStr;
        const spreadPctStr = `(${((spread / bestAsk) * 100).toFixed(3)}%)`;
        if (spreadPercentRef.current && spreadPercentRef.current.textContent !== spreadPctStr) spreadPercentRef.current.textContent = spreadPctStr;
      }

      const askTotalStr = formatQty(askTotal);
      if (askTotalRef.current && askTotalRef.current.textContent !== askTotalStr) askTotalRef.current.textContent = askTotalStr;
      const bidTotalStr = formatQty(bidTotal);
      if (bidTotalRef.current && bidTotalRef.current.textContent !== bidTotalStr) bidTotalRef.current.textContent = bidTotalStr;

      const totalLvl = `${askCount + bidCount}`;
      if (levelCountRef.current && levelCountRef.current.textContent !== totalLvl) levelCountRef.current.textContent = totalLvl;

      // Time string — update once per second max
      const nowSec = (Date.now() / 1000) | 0;
      if (nowSec !== lastTimeSecRef.current) {
        lastTimeSecRef.current = nowSec;
        lastTimeStrRef.current = new Date().toLocaleTimeString();
      }
      if (updateTimeRef.current && updateTimeRef.current.textContent !== lastTimeStrRef.current) updateTimeRef.current.textContent = lastTimeStrRef.current;
    } else if (view === 'imbalance') {
      renderCanvasCharts();
    } else if (view === 'volume') {
      renderVolumeView();
    } else if (view === 'signals') {
      renderSignalsView();
    }

    // Update tick count in header
    const tickStr = `${tickHistoryRef.current.length}`;
    if (tickCountRef.current && tickCountRef.current.textContent !== tickStr) tickCountRef.current.textContent = tickStr;
  }, [captureTickSnapshot, paintOrderBookCanvas, renderCanvasCharts, renderVolumeView, renderSignalsView]);

  // Schedule render via rAF (coalesces multiple WS messages into one frame)
  const scheduleRender = useCallback(() => {
    if (!dirtyRef.current) {
      dirtyRef.current = true;
      rafIdRef.current = requestAnimationFrame(renderFrame);
    }
  }, [renderFrame]);

  const connect = useCallback(() => {
    if (wsRef.current) { wsRef.current.close(); wsRef.current = null; }

    asksRef.current = {};
    bidsRef.current = {};
    tickHistoryRef.current = [];
    signalHistoryRef.current = [];
    cumulativePnlRef.current = 0;
    lastSignalPriceRef.current = 0;
    activeSignalRef.current = null;
    lastTickCaptureRef.current = 0;
    lastSignalRenderCountRef.current = 0;

    if (activeBroker !== 'kraken' || !selectedSymbol) {
      setStatus('disconnected');
      return;
    }

    setStatus('connecting');
    const ws = new WebSocket('wss://ws.kraken.com/v2');
    wsRef.current = ws;

    ws.onopen = () => {
      setStatus('connected');
      ws.send(JSON.stringify({ method: 'subscribe', params: { channel: 'book', symbol: [selectedSymbol], depth: 25 } }));
    };

    ws.onmessage = (e) => {
      try {
        const msg = JSON.parse(e.data);
        if (msg.channel !== 'book') return;

        if (msg.type === 'snapshot' && msg.data?.[0]) {
          asksRef.current = {};
          bidsRef.current = {};
          const d = msg.data[0];
          if (d.asks) for (const a of d.asks) {
            const p = a.price ?? a[0];
            const q = a.qty ?? a[1];
            if (p != null && q != null) asksRef.current[String(p)] = Number(q);
          }
          if (d.bids) for (const b of d.bids) {
            const p = b.price ?? b[0];
            const q = b.qty ?? b[1];
            if (p != null && q != null) bidsRef.current[String(p)] = Number(q);
          }
        } else if (msg.type === 'update' && msg.data?.[0]) {
          const d = msg.data[0];
          if (d.asks) for (const a of d.asks) {
            const p = a.price ?? a[0];
            const q = a.qty ?? a[1];
            if (p == null) continue;
            if (Number(q) === 0) delete asksRef.current[String(p)];
            else asksRef.current[String(p)] = Number(q);
          }
          if (d.bids) for (const b of d.bids) {
            const p = b.price ?? b[0];
            const q = b.qty ?? b[1];
            if (p == null) continue;
            if (Number(q) === 0) delete bidsRef.current[String(p)];
            else bidsRef.current[String(p)] = Number(q);
          }
        }

        scheduleRender();
      } catch (err) {
        console.error('[CryptoOrderBook] WS parse error:', err);
      }
    };

    ws.onclose = () => setStatus('disconnected');
    ws.onerror = (err) => {
      console.error('[CryptoOrderBook] WS error:', err);
      setStatus('disconnected');
    };
  }, [activeBroker, selectedSymbol, scheduleRender]);

  useEffect(() => {
    connect();
    return () => {
      if (wsRef.current) { wsRef.current.close(); wsRef.current = null; }
      if (rafIdRef.current) cancelAnimationFrame(rafIdRef.current);
    };
  }, [connect]);

  // When switching tabs, force a render of the new view (skip if no data yet)
  const viewSwitchCount = useRef(0);
  useEffect(() => {
    viewSwitchCount.current++;
    // Skip the initial mount render — no data yet, will render when WS data arrives
    if (viewSwitchCount.current <= 1 && Object.keys(asksRef.current).length === 0) return;
    dirtyRef.current = true;
    rafIdRef.current = requestAnimationFrame(renderFrame);
  }, [activeView, renderFrame]);

  useEffect(() => {
    if (priceRef.current) priceRef.current.textContent = `$${formatPrice(currentPrice)}`;
  }, [currentPrice]);

  return (
    <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden', backgroundColor: FINCEPT.PANEL_BG }}>
      <style>{`
        .ob-tab-btn { padding:6px 8px; background:transparent; border:none; color:${FINCEPT.GRAY}; cursor:pointer; font-size:9px; font-weight:600; letter-spacing:0.5px; transition:color 0.15s; border-bottom:2px solid transparent; }
        .ob-tab-btn:hover { color:${FINCEPT.WHITE}; }
        .ob-tab-btn.active { color:${FINCEPT.ORANGE}; border-bottom-color:${FINCEPT.ORANGE}; }
        .ob-scroll::-webkit-scrollbar { width:4px; }
        .ob-scroll::-webkit-scrollbar-track { background:transparent; }
        .ob-scroll::-webkit-scrollbar-thumb { background:${FINCEPT.BORDER}; border-radius:2px; }
        .ob-scroll::-webkit-scrollbar-thumb:hover { background:${FINCEPT.GRAY}; }
      `}</style>

      {/* Tab Header */}
      <div style={{ display:'flex', alignItems:'center', justifyContent:'space-between', borderBottom:`1px solid ${FINCEPT.BORDER}`, backgroundColor:FINCEPT.HEADER_BG }}>
        <div style={{ display:'flex' }}>
          {(['orderbook','volume','imbalance','signals'] as OrderBookViewType[]).map(v => (
            <button key={v} className={`ob-tab-btn ${activeView === v ? 'active' : ''}`} onClick={() => setActiveView(v)}>
              {{ orderbook:'BOOK', volume:'VOLUME', imbalance:'IMBALANCE', signals:'SIGNALS' }[v]}
            </button>
          ))}
        </div>
        <div style={{ padding:'0 8px', fontSize:'8px', color:FINCEPT.GRAY, fontFamily:'monospace' }}>
          {activeView === 'orderbook' && <span><span ref={levelCountRef}>0</span> lvl</span>}
          {(activeView === 'imbalance' || activeView === 'signals') && <span><span ref={tickCountRef}>0</span> ticks</span>}
        </div>
      </div>

      {/* ORDER BOOK */}
      <div className="ob-scroll" style={{ flex:1, display:activeView === 'orderbook' ? 'flex' : 'none', flexDirection:'column', overflowY:'auto', overflowX:'hidden' }}>
        <div style={{ padding:'6px 12px', display:'flex', alignItems:'center', justifyContent:'space-between', borderBottom:`1px solid ${FINCEPT.BORDER}`, backgroundColor:'rgba(0,0,0,0.2)' }}>
          <div style={{ display:'flex', alignItems:'center', gap:'6px' }}>
            <div style={{ width:'6px', height:'6px', borderRadius:'50%', backgroundColor: status === 'connected' ? FINCEPT.GREEN : status === 'connecting' ? FINCEPT.YELLOW : FINCEPT.RED, boxShadow: status === 'connected' ? `0 0 6px ${FINCEPT.GREEN}` : 'none' }} />
            <span style={{ fontSize:'10px', color:FINCEPT.GRAY }}>{status === 'connected' ? 'LIVE' : status === 'connecting' ? 'CONNECTING' : 'OFFLINE'}</span>
          </div>
          {activeBroker !== 'kraken' && <span style={{ fontSize:'9px', color:FINCEPT.YELLOW }}>Select Kraken</span>}
          <span style={{ fontSize:'9px', color:FINCEPT.GRAY }}><span ref={updateTimeRef}>--:--:--</span></span>
        </div>
        <div style={{ display:'flex', justifyContent:'space-between', padding:'4px 8px', borderBottom:`1px solid ${FINCEPT.BORDER}` }}>
          <div style={{ display:'flex', gap:'12px', fontSize:'9px', color:FINCEPT.GRAY, textTransform:'uppercase', letterSpacing:'0.5px' }}>
            <span>Price</span>
          </div>
          <div style={{ display:'flex', gap:'20px', fontSize:'9px', color:FINCEPT.GRAY, textTransform:'uppercase', letterSpacing:'0.5px' }}>
            <span>Size</span>
            <span>Total</span>
          </div>
        </div>
        <div style={{ flex:1, display:'flex', flexDirection:'column', overflow:'hidden', borderBottom:`1px solid ${FINCEPT.BORDER}` }}>
          <div style={{ padding:'4px 8px', display:'flex', justifyContent:'space-between', alignItems:'center', backgroundColor:'rgba(239,68,68,0.05)' }}>
            <span style={{ fontSize:'9px', fontWeight:700, color:FINCEPT.RED }}>ASKS</span>
            <span style={{ fontSize:'9px', color:FINCEPT.GRAY }}>Total: <span ref={askTotalRef} style={{ color:FINCEPT.RED }}>0</span></span>
          </div>
          <canvas ref={asksCanvasRef} style={{ flex:1, width:'100%', display:'block', minHeight:`${NUM_LEVELS * 22}px` }} />
        </div>
        <div style={{ padding:'8px 12px', backgroundColor:FINCEPT.HEADER_BG, display:'flex', alignItems:'center', justifyContent:'space-between', borderBottom:`1px solid ${FINCEPT.BORDER}` }}>
          <div ref={priceRef} style={{ fontSize:'16px', fontWeight:700, color:FINCEPT.YELLOW, fontFamily:'"IBM Plex Mono",monospace' }}>$0.00</div>
          <div style={{ fontSize:'10px', display:'flex', alignItems:'center', gap:'4px' }}>
            <span style={{ color:FINCEPT.GRAY }}>Spread:</span>
            <span ref={spreadValueRef} style={{ color:FINCEPT.WHITE, fontWeight:600 }}>--</span>
            <span ref={spreadPercentRef} style={{ color:FINCEPT.YELLOW }}></span>
          </div>
        </div>
        <div style={{ flex:1, display:'flex', flexDirection:'column', overflow:'hidden' }}>
          <div style={{ padding:'4px 8px', display:'flex', justifyContent:'space-between', alignItems:'center', backgroundColor:'rgba(34,197,94,0.05)' }}>
            <span style={{ fontSize:'9px', fontWeight:700, color:FINCEPT.GREEN }}>BIDS</span>
            <span style={{ fontSize:'9px', color:FINCEPT.GRAY }}>Total: <span ref={bidTotalRef} style={{ color:FINCEPT.GREEN }}>0</span></span>
          </div>
          <canvas ref={bidsCanvasRef} style={{ flex:1, width:'100%', display:'block', minHeight:`${NUM_LEVELS * 22}px` }} />
        </div>
      </div>

      {/* VOLUME */}
      <div ref={volumeContainerRef} style={{ flex:1, overflow:'auto', fontFamily:'"IBM Plex Mono",monospace', display:activeView === 'volume' ? 'block' : 'none' }} />

      {/* IMBALANCE */}
      <div style={{ flex:1, display:activeView === 'imbalance' ? 'flex' : 'none', flexDirection:'column', overflow:'hidden' }}>
        <div style={{ display:'flex', gap:'1px', backgroundColor:FINCEPT.BORDER }}>
          <div style={{ flex:1, padding:'8px 10px', backgroundColor:'#0A0A0A', textAlign:'center' }}>
            <div style={{ fontSize:'8px', color:FINCEPT.GRAY, letterSpacing:'0.5px' }}>IMBALANCE</div>
            <div style={{ fontSize:'16px', fontWeight:700, fontFamily:'monospace' }}><span ref={imbalanceValueRef} style={{ color:FINCEPT.WHITE }}>0.0000</span></div>
          </div>
          <div style={{ flex:1, padding:'8px 10px', backgroundColor:'#0A0A0A', textAlign:'center' }}>
            <div style={{ fontSize:'8px', color:FINCEPT.GRAY, letterSpacing:'0.5px' }}>RISE (60s)</div>
            <div style={{ fontSize:'16px', fontWeight:700, fontFamily:'monospace' }}><span ref={riseValueRef} style={{ color:FINCEPT.WHITE }}>0.0000%</span></div>
          </div>
        </div>
        <div style={{ display:'flex', gap:'1px', backgroundColor:FINCEPT.BORDER }}>
          <div style={{ flex:1, padding:'4px 10px', backgroundColor:'#0A0A0A', textAlign:'center' }}>
            <span style={{ fontSize:'8px', color:FINCEPT.GRAY }}>BID VOL: </span>
            <span ref={bidTotalImbalanceRef} style={{ fontSize:'10px', color:FINCEPT.GREEN, fontWeight:600, fontFamily:'monospace' }}>0</span>
          </div>
          <div style={{ flex:1, padding:'4px 10px', backgroundColor:'#0A0A0A', textAlign:'center' }}>
            <span style={{ fontSize:'8px', color:FINCEPT.GRAY }}>ASK VOL: </span>
            <span ref={askTotalImbalanceRef} style={{ fontSize:'10px', color:FINCEPT.RED, fontWeight:600, fontFamily:'monospace' }}>0</span>
          </div>
        </div>
        <div style={{ padding:'6px 10px 2px', borderBottom:`1px solid ${FINCEPT.BORDER}` }}>
          <div style={{ display:'flex', justifyContent:'space-between', alignItems:'center', marginBottom:'4px' }}>
            <span style={{ fontSize:'9px', color:FINCEPT.ORANGE, fontWeight:700, letterSpacing:'0.5px' }}>DEPTH IMBALANCE</span>
            <span style={{ fontSize:'8px', color:FINCEPT.GRAY }}>(Bid-Ask)/(Bid+Ask)</span>
          </div>
          <canvas ref={imbalanceCanvasRef} style={{ width:'100%', height:'120px', display:'block' }} />
        </div>
        <div style={{ flex:1, padding:'6px 10px 2px' }}>
          <div style={{ display:'flex', justifyContent:'space-between', alignItems:'center', marginBottom:'4px' }}>
            <span style={{ fontSize:'9px', color:FINCEPT.ORANGE, fontWeight:700, letterSpacing:'0.5px' }}>PRICE MOMENTUM</span>
            <div style={{ display:'flex', gap:'8px', fontSize:'8px' }}>
              <span style={{ color:'rgba(157,78,221,0.7)' }}>30s</span>
              <span style={{ color:FINCEPT.CYAN }}>60s</span>
              <span style={{ color:'rgba(255,136,0,0.7)' }}>5m</span>
            </div>
          </div>
          <canvas ref={riseCanvasRef} style={{ width:'100%', height:'120px', display:'block' }} />
        </div>
      </div>

      {/* SIGNALS */}
      <div style={{ flex:1, display:activeView === 'signals' ? 'flex' : 'none', flexDirection:'column', overflow:'hidden' }}>
        <div ref={signalIndicatorRef} style={{ flexShrink:0 }} />
        <div style={{ padding:'6px 10px 4px' }}>
          <span style={{ fontSize:'9px', color:FINCEPT.ORANGE, fontWeight:700, letterSpacing:'0.5px' }}>SIGNAL HISTORY</span>
        </div>
        <div ref={signalHistoryContainerRef} style={{ flex:1, overflow:'auto', fontFamily:'"IBM Plex Mono",monospace' }} />
        <div style={{ padding:'6px 10px', borderTop:`1px solid ${FINCEPT.BORDER}`, fontSize:'8px', color:FINCEPT.GRAY, fontFamily:'monospace', textAlign:'center' }}>
          BUY: imb&gt;{IMBALANCE_BUY_THRESHOLD} &amp; rise&gt;{RISE_BUY_THRESHOLD}% | SELL: imb&lt;{IMBALANCE_SELL_THRESHOLD} &amp; rise&lt;{RISE_SELL_THRESHOLD}%
        </div>
      </div>
    </div>
  );
});

export default CryptoOrderBook;
