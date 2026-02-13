// useExport Hook - Thin wrapper around the shared export hook
// Adapts IndicatorData shape to the generic DataPoint[] + ExportMeta API

import { useMemo, RefObject } from 'react';
import { useExport as useSharedExport } from '@/components/common/charts';
import type { IndicatorData, DataSourceConfig } from '../types';

interface UseExportReturn {
  saveNotification: { show: boolean; path: string; type: 'image' | 'csv' } | null;
  setSaveNotification: (notification: { show: boolean; path: string; type: 'image' | 'csv' } | null) => void;
  exportCSV: () => void;
  exportImage: () => Promise<void>;
}

export function useExport(
  data: IndicatorData | null,
  currentSourceConfig: DataSourceConfig,
  chartContainerRef: RefObject<HTMLDivElement | null>
): UseExportReturn {
  const points = useMemo(() => data?.data ?? null, [data]);

  const meta = useMemo(() => ({
    title: data?.metadata?.indicator_name || data?.indicator || '',
    subtitle: `${data?.metadata?.country_name || data?.country || ''} \u2022 Source: ${data?.metadata?.source || currentSourceConfig.name}`,
    filenameSuffix: `${currentSourceConfig.id}_${data?.indicator || 'data'}_${data?.country || 'all'}`,
  }), [data, currentSourceConfig]);

  return useSharedExport(points, meta, chartContainerRef);
}

export default useExport;
