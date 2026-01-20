/**
 * CFA Quant Chart Zoom Hook
 * Handles zoom and pan functionality for charts
 */

import { useState, useCallback } from 'react';
import type { ZoomState, QuantAnalyticsResult } from '../types';

interface UseChartZoomProps {
  priceDataLength: number;
  analysisResult: QuantAnalyticsResult<Record<string, unknown>> | null;
  selectedAnalysis: string;
}

interface UseChartZoomReturn {
  dataChartZoom: ZoomState | null;
  setDataChartZoom: (zoom: ZoomState | null) => void;
  resultsChartZoom: ZoomState | null;
  setResultsChartZoom: (zoom: ZoomState | null) => void;
  refAreaLeft: number | null;
  refAreaRight: number | null;
  isSelecting: boolean;
  activeChart: 'data' | 'results' | null;
  handleZoomIn: (chartType: 'data' | 'results') => void;
  handleZoomOut: (chartType: 'data' | 'results') => void;
  handleResetZoom: (chartType: 'data' | 'results') => void;
  handlePanLeft: (chartType: 'data' | 'results') => void;
  handlePanRight: (chartType: 'data' | 'results') => void;
  canPanLeft: (chartType: 'data' | 'results') => boolean;
  canPanRight: (chartType: 'data' | 'results') => boolean;
  handleMouseDown: (e: any, chartType: 'data' | 'results') => void;
  handleMouseMove: (e: any) => void;
  handleMouseUp: () => void;
  handleWheel: (e: React.WheelEvent, chartType: 'data' | 'results') => void;
  getResultChartDataLength: () => number;
}

export function useChartZoom({
  priceDataLength,
  analysisResult,
  selectedAnalysis,
}: UseChartZoomProps): UseChartZoomReturn {
  const [dataChartZoom, setDataChartZoom] = useState<ZoomState | null>(null);
  const [resultsChartZoom, setResultsChartZoom] = useState<ZoomState | null>(null);
  const [refAreaLeft, setRefAreaLeft] = useState<number | null>(null);
  const [refAreaRight, setRefAreaRight] = useState<number | null>(null);
  const [isSelecting, setIsSelecting] = useState(false);
  const [activeChart, setActiveChart] = useState<'data' | 'results' | null>(null);

  const getResultChartDataLength = useCallback(() => {
    if (!analysisResult?.result) return priceDataLength;
    if (selectedAnalysis === 'forecasting') {
      const forecasts = (analysisResult.result as Record<string, unknown>).value as number[];
      return priceDataLength + (forecasts?.length || 0);
    }
    if (selectedAnalysis === 'trend_analysis') {
      return priceDataLength + 5;
    }
    if (selectedAnalysis === 'arima_model') {
      const result = analysisResult.result as Record<string, unknown>;
      const value = result.value as Record<string, unknown>;
      const forecasts = value?.forecasts as number[];
      return priceDataLength + (forecasts?.length || 5);
    }
    return priceDataLength;
  }, [analysisResult, priceDataLength, selectedAnalysis]);

  const handleZoomIn = useCallback((chartType: 'data' | 'results') => {
    const currentZoom = chartType === 'data' ? dataChartZoom : resultsChartZoom;
    const setZoom = chartType === 'data' ? setDataChartZoom : setResultsChartZoom;
    const dataLength = chartType === 'results' ? getResultChartDataLength() : priceDataLength;

    if (dataLength <= 5) return;

    const currentStart = currentZoom?.startIndex ?? 0;
    const currentEnd = currentZoom?.endIndex ?? dataLength - 1;
    const range = currentEnd - currentStart;
    const center = Math.floor((currentStart + currentEnd) / 2);
    const newRange = Math.max(5, Math.floor(range * 0.7));
    const newStart = Math.max(0, center - Math.floor(newRange / 2));
    const newEnd = Math.min(dataLength - 1, newStart + newRange);

    setZoom({ startIndex: newStart, endIndex: newEnd });
  }, [priceDataLength, dataChartZoom, resultsChartZoom, getResultChartDataLength]);

  const handleZoomOut = useCallback((chartType: 'data' | 'results') => {
    const currentZoom = chartType === 'data' ? dataChartZoom : resultsChartZoom;
    const setZoom = chartType === 'data' ? setDataChartZoom : setResultsChartZoom;
    const dataLength = chartType === 'results' ? getResultChartDataLength() : priceDataLength;

    if (!currentZoom) return;

    const currentStart = currentZoom.startIndex;
    const currentEnd = currentZoom.endIndex;
    const range = currentEnd - currentStart;
    const center = Math.floor((currentStart + currentEnd) / 2);
    const newRange = Math.min(dataLength - 1, Math.floor(range * 1.5));
    const newStart = Math.max(0, center - Math.floor(newRange / 2));
    const newEnd = Math.min(dataLength - 1, newStart + newRange);

    if (newStart === 0 && newEnd === dataLength - 1) {
      setZoom(null);
    } else {
      setZoom({ startIndex: newStart, endIndex: newEnd });
    }
  }, [priceDataLength, dataChartZoom, resultsChartZoom, getResultChartDataLength]);

  const handleResetZoom = useCallback((chartType: 'data' | 'results') => {
    const setZoom = chartType === 'data' ? setDataChartZoom : setResultsChartZoom;
    setZoom(null);
  }, []);

  const handlePanLeft = useCallback((chartType: 'data' | 'results') => {
    const currentZoom = chartType === 'data' ? dataChartZoom : resultsChartZoom;
    const setZoom = chartType === 'data' ? setDataChartZoom : setResultsChartZoom;

    if (!currentZoom) return;

    const range = currentZoom.endIndex - currentZoom.startIndex;
    const step = Math.max(1, Math.floor(range * 0.2));
    const newStart = Math.max(0, currentZoom.startIndex - step);
    const newEnd = newStart + range;

    setZoom({ startIndex: newStart, endIndex: newEnd });
  }, [dataChartZoom, resultsChartZoom]);

  const handlePanRight = useCallback((chartType: 'data' | 'results') => {
    const currentZoom = chartType === 'data' ? dataChartZoom : resultsChartZoom;
    const setZoom = chartType === 'data' ? setDataChartZoom : setResultsChartZoom;
    const dataLength = chartType === 'results' ? getResultChartDataLength() : priceDataLength;

    if (!currentZoom) return;

    const range = currentZoom.endIndex - currentZoom.startIndex;
    const step = Math.max(1, Math.floor(range * 0.2));
    const newEnd = Math.min(dataLength - 1, currentZoom.endIndex + step);
    const newStart = newEnd - range;

    setZoom({ startIndex: Math.max(0, newStart), endIndex: newEnd });
  }, [dataChartZoom, resultsChartZoom, priceDataLength, getResultChartDataLength]);

  const canPanLeft = useCallback((chartType: 'data' | 'results') => {
    const currentZoom = chartType === 'data' ? dataChartZoom : resultsChartZoom;
    return currentZoom !== null && currentZoom.startIndex > 0;
  }, [dataChartZoom, resultsChartZoom]);

  const canPanRight = useCallback((chartType: 'data' | 'results') => {
    const currentZoom = chartType === 'data' ? dataChartZoom : resultsChartZoom;
    const dataLength = chartType === 'results' ? getResultChartDataLength() : priceDataLength;
    return currentZoom !== null && currentZoom.endIndex < dataLength - 1;
  }, [dataChartZoom, resultsChartZoom, priceDataLength, getResultChartDataLength]);

  const handleMouseDown = useCallback((e: any, chartType: 'data' | 'results') => {
    const activeLabel = typeof e.activeLabel === 'string' ? parseInt(e.activeLabel, 10) : e.activeLabel;
    if (activeLabel !== undefined && !isNaN(activeLabel)) {
      setRefAreaLeft(activeLabel);
      setIsSelecting(true);
      setActiveChart(chartType);
    }
  }, []);

  const handleMouseMove = useCallback((e: any) => {
    const activeLabel = typeof e.activeLabel === 'string' ? parseInt(e.activeLabel, 10) : e.activeLabel;
    if (isSelecting && activeLabel !== undefined && !isNaN(activeLabel)) {
      setRefAreaRight(activeLabel);
    }
  }, [isSelecting]);

  const handleMouseUp = useCallback(() => {
    if (refAreaLeft !== null && refAreaRight !== null && activeChart) {
      const left = Math.min(refAreaLeft, refAreaRight);
      const right = Math.max(refAreaLeft, refAreaRight);

      if (right - left >= 2) {
        const setZoom = activeChart === 'data' ? setDataChartZoom : setResultsChartZoom;
        setZoom({ startIndex: left, endIndex: right });
      }
    }
    setRefAreaLeft(null);
    setRefAreaRight(null);
    setIsSelecting(false);
    setActiveChart(null);
  }, [refAreaLeft, refAreaRight, activeChart]);

  const handleWheel = useCallback((e: React.WheelEvent, chartType: 'data' | 'results') => {
    e.preventDefault();

    if (e.shiftKey) {
      if (e.deltaY > 0) {
        handlePanRight(chartType);
      } else if (e.deltaY < 0) {
        handlePanLeft(chartType);
      }
      return;
    }

    if (e.ctrlKey || e.metaKey) {
      if (e.deltaY < 0) {
        handleZoomIn(chartType);
      } else if (e.deltaY > 0) {
        handleZoomOut(chartType);
      }
      return;
    }

    if (e.deltaY > 0) {
      handlePanRight(chartType);
    } else if (e.deltaY < 0) {
      handlePanLeft(chartType);
    }
  }, [handlePanLeft, handlePanRight, handleZoomIn, handleZoomOut]);

  return {
    dataChartZoom,
    setDataChartZoom,
    resultsChartZoom,
    setResultsChartZoom,
    refAreaLeft,
    refAreaRight,
    isSelecting,
    activeChart,
    handleZoomIn,
    handleZoomOut,
    handleResetZoom,
    handlePanLeft,
    handlePanRight,
    canPanLeft,
    canPanRight,
    handleMouseDown,
    handleMouseMove,
    handleMouseUp,
    handleWheel,
    getResultChartDataLength,
  };
}
