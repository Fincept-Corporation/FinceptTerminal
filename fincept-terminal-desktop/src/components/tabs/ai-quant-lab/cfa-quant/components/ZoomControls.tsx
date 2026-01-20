/**
 * Zoom Controls Component
 * Provides zoom and pan controls for charts
 */

import React from 'react';
import { ChevronLeft, ChevronRight, ZoomIn, ZoomOut, RotateCcw } from 'lucide-react';
import { BB } from '../constants';
import type { ZoomControlsProps } from '../types';

export function ZoomControls({
  chartType,
  isZoomed,
  onZoomIn,
  onZoomOut,
  onResetZoom,
  onPanLeft,
  onPanRight,
  canPanLeft,
  canPanRight,
}: ZoomControlsProps) {
  return (
    <div className="flex items-center gap-1">
      {/* Pan Left */}
      <button
        onClick={() => onPanLeft(chartType)}
        disabled={!canPanLeft}
        className="p-1.5 transition-colors hover:opacity-80"
        style={{
          backgroundColor: BB.cardBg,
          border: `1px solid ${BB.borderDark}`,
          opacity: canPanLeft ? 1 : 0.4,
          cursor: canPanLeft ? 'pointer' : 'not-allowed'
        }}
        title="Pan Left"
      >
        <ChevronLeft size={14} style={{ color: canPanLeft ? BB.amber : BB.textSecondary }} />
      </button>
      {/* Zoom In */}
      <button
        onClick={() => onZoomIn(chartType)}
        className="p-1.5 transition-colors hover:opacity-80"
        style={{ backgroundColor: BB.cardBg, border: `1px solid ${BB.borderDark}` }}
        title="Zoom In"
      >
        <ZoomIn size={14} style={{ color: BB.textSecondary }} />
      </button>
      {/* Zoom Out */}
      <button
        onClick={() => onZoomOut(chartType)}
        disabled={!isZoomed}
        className="p-1.5 transition-colors hover:opacity-80"
        style={{
          backgroundColor: BB.cardBg,
          border: `1px solid ${BB.borderDark}`,
          opacity: isZoomed ? 1 : 0.4,
          cursor: isZoomed ? 'pointer' : 'not-allowed'
        }}
        title="Zoom Out"
      >
        <ZoomOut size={14} style={{ color: BB.textSecondary }} />
      </button>
      {/* Reset */}
      <button
        onClick={() => onResetZoom(chartType)}
        disabled={!isZoomed}
        className="p-1.5 transition-colors hover:opacity-80"
        style={{
          backgroundColor: BB.cardBg,
          border: `1px solid ${BB.borderDark}`,
          opacity: isZoomed ? 1 : 0.4,
          cursor: isZoomed ? 'pointer' : 'not-allowed'
        }}
        title="Reset Zoom"
      >
        <RotateCcw size={14} style={{ color: BB.textSecondary }} />
      </button>
      {/* Pan Right */}
      <button
        onClick={() => onPanRight(chartType)}
        disabled={!canPanRight}
        className="p-1.5 transition-colors hover:opacity-80"
        style={{
          backgroundColor: BB.cardBg,
          border: `1px solid ${BB.borderDark}`,
          opacity: canPanRight ? 1 : 0.4,
          cursor: canPanRight ? 'pointer' : 'not-allowed'
        }}
        title="Pan Right"
      >
        <ChevronRight size={14} style={{ color: canPanRight ? BB.amber : BB.textSecondary }} />
      </button>
    </div>
  );
}
