// usePxWebData Hook - Browse and fetch Statistics Finland (PxWeb) data

import { useState, useCallback } from 'react';
import { invoke } from '@tauri-apps/api/core';
import type {
  PxWebView,
  PxWebNode,
  PxWebVariable,
  PxWebStatData,
  PxWebResponse,
} from '../types';

interface UsePxWebDataReturn {
  // View
  activeView: PxWebView;
  setActiveView: (view: PxWebView) => void;

  // Browse
  nodes: PxWebNode[];
  pathStack: string[];
  currentPath: string;
  fetchNodes: (path?: string) => Promise<void>;
  navigateInto: (node: PxWebNode) => void;
  navigateBack: () => void;

  // Metadata
  variables: PxWebVariable[];
  tableLabel: string;
  fetchMetadata: (tablePath: string) => Promise<void>;
  selectedTable: string | null;

  // Data
  statData: PxWebStatData | null;
  fetchData: (tablePath: string, query?: any[]) => Promise<void>;

  // Export
  exportCSV: () => void;

  // Shared state
  loading: boolean;
  error: string | null;
}

export function usePxWebData(): UsePxWebDataReturn {
  const [activeView, setActiveView] = useState<PxWebView>('browse');
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  // Browse
  const [nodes, setNodes] = useState<PxWebNode[]>([]);
  const [pathStack, setPathStack] = useState<string[]>(['']);
  const [currentPath, setCurrentPath] = useState('');

  // Metadata
  const [variables, setVariables] = useState<PxWebVariable[]>([]);
  const [tableLabel, setTableLabel] = useState('');
  const [selectedTable, setSelectedTable] = useState<string | null>(null);

  // Data
  const [statData, setStatData] = useState<PxWebStatData | null>(null);

  const fetchNodes = useCallback(async (path: string = '') => {
    setLoading(true);
    setError(null);
    try {
      const result = await invoke('execute_pxweb_command', {
        command: 'nodes',
        args: path ? [path] : ([] as string[]),
      }) as string;

      const parsed: PxWebResponse = JSON.parse(result);
      if (!parsed.success) {
        setError(parsed.error || 'Failed to fetch nodes');
        setNodes([]);
      } else {
        const data = parsed.data;
        if (Array.isArray(data)) {
          setNodes(data.map((n: any) => ({
            dbid: n.dbid || '',
            id: n.id || '',
            type: n.type || 'l',
            text: n.text || '',
            description: n.description || '',
            updated: n.updated || '',
          })));
        } else {
          setNodes([]);
        }
        setCurrentPath(path);
      }
    } catch (err) {
      const msg = err instanceof Error ? err.message : String(err);
      setError(`Failed to fetch nodes: ${msg}`);
      setNodes([]);
    } finally {
      setLoading(false);
    }
  }, []);

  const navigateInto = useCallback((node: PxWebNode) => {
    if (node.type === 't') {
      // It's a table — fetch metadata
      const tablePath = currentPath ? `${currentPath}/${node.id}` : node.id;
      setSelectedTable(tablePath);
      setActiveView('metadata');
      fetchMetadataInternal(tablePath);
    } else {
      // It's a folder — navigate deeper
      const newPath = currentPath ? `${currentPath}/${node.id}` : node.id;
      setPathStack(prev => [...prev, newPath]);
      fetchNodes(newPath);
    }
   
  }, [currentPath, fetchNodes]);

  const navigateBack = useCallback(() => {
    if (pathStack.length <= 1) return;
    const newStack = pathStack.slice(0, -1);
    setPathStack(newStack);
    const parentPath = newStack[newStack.length - 1] || '';
    fetchNodes(parentPath);
  }, [pathStack, fetchNodes]);

  const fetchMetadataInternal = async (tablePath: string) => {
    setLoading(true);
    setError(null);
    try {
      const result = await invoke('execute_pxweb_command', {
        command: 'metadata',
        args: [tablePath],
      }) as string;

      const parsed: PxWebResponse = JSON.parse(result);
      if (!parsed.success) {
        setError(parsed.error || 'Failed to fetch metadata');
        setVariables([]);
      } else {
        const data = parsed.data;
        // PxWeb metadata returns { title, variables: [...] }
        setTableLabel(data?.title || tablePath);
        const vars = data?.variables || [];
        setVariables(vars.map((v: any) => ({
          code: v.code || '',
          text: v.text || '',
          values: v.values || [],
          valueTexts: v.valueTexts || [],
          elimination: v.elimination || false,
          time: v.time || false,
        })));
      }
    } catch (err) {
      const msg = err instanceof Error ? err.message : String(err);
      setError(`Failed to fetch metadata: ${msg}`);
      setVariables([]);
    } finally {
      setLoading(false);
    }
  };

  const fetchMetadata = useCallback(async (tablePath: string) => {
    setSelectedTable(tablePath);
    await fetchMetadataInternal(tablePath);
   
  }, []);

  const fetchData = useCallback(async (tablePath: string, query: any[] = []) => {
    setLoading(true);
    setError(null);
    try {
      const args: string[] = [tablePath];
      if (query.length > 0) {
        args.push(JSON.stringify(query));
      }

      const result = await invoke('execute_pxweb_command', {
        command: 'data',
        args,
      }) as string;

      const parsed: PxWebResponse = JSON.parse(result);
      if (!parsed.success) {
        setError(parsed.error || 'Failed to fetch data');
        setStatData(null);
      } else {
        setStatData(parsed.data as PxWebStatData);
        setActiveView('data');
      }
    } catch (err) {
      const msg = err instanceof Error ? err.message : String(err);
      setError(`Failed to fetch data: ${msg}`);
      setStatData(null);
    } finally {
      setLoading(false);
    }
  }, []);

  const exportCSV = useCallback(() => {
    if (activeView === 'browse' && nodes.length > 0) {
      const csv = [
        '# Statistics Finland - Database Nodes',
        '',
        'Type,ID,Name,Updated',
        ...nodes.map(n =>
          `${n.type === 't' ? 'Table' : 'Folder'},${n.id},"${(n.text || '').replace(/"/g, '""')}",${n.updated || ''}`
        ),
      ].join('\n');
      downloadCSV(csv, 'fincept_statfin_nodes.csv');
    } else if (activeView === 'metadata' && variables.length > 0) {
      const csv = [
        `# Statistics Finland - Table Metadata: ${selectedTable}`,
        '',
        'Variable,Values Count,Time Dim,Eliminable',
        ...variables.map(v =>
          `"${v.text}",${v.values.length},${v.time || false},${v.elimination || false}`
        ),
      ].join('\n');
      downloadCSV(csv, `fincept_statfin_meta_${selectedTable?.replace(/\//g, '_') || 'table'}.csv`);
    } else if (activeView === 'data' && statData?.value) {
      // Flatten json-stat2 into tabular CSV
      const dims = statData.id || [];
      const header = dims.map(d => statData.dimension?.[d]?.label || d).join(',') + ',Value';
      const dimCats = dims.map(d => {
        const cat = statData.dimension?.[d]?.category;
        const labels = cat?.label || {};
        const indexEntries = Object.entries(cat?.index || {}).sort((a, b) => a[1] - b[1]);
        return indexEntries.map(([k]) => labels[k] || k);
      });

      // Build rows via cartesian product
      const rows: string[] = [];
      const sizes = statData.size || dims.map((_, i) => dimCats[i].length);
      const totalValues = statData.value?.length || 0;

      for (let i = 0; i < totalValues; i++) {
        let remainder = i;
        const parts: string[] = [];
        for (let d = dims.length - 1; d >= 0; d--) {
          const dimSize = sizes[d];
          const idx = remainder % dimSize;
          remainder = Math.floor(remainder / dimSize);
          parts.unshift(`"${(dimCats[d]?.[idx] || '').replace(/"/g, '""')}"`);
        }
        parts.push(String(statData.value?.[i] ?? ''));
        rows.push(parts.join(','));
      }

      const csv = [
        `# Statistics Finland - Data: ${statData.label || selectedTable}`,
        '',
        header,
        ...rows,
      ].join('\n');
      downloadCSV(csv, `fincept_statfin_data_${selectedTable?.replace(/\//g, '_') || 'table'}.csv`);
    }
  }, [activeView, nodes, variables, statData, selectedTable]);

  return {
    activeView,
    setActiveView,
    nodes,
    pathStack,
    currentPath,
    fetchNodes,
    navigateInto,
    navigateBack,
    variables,
    tableLabel,
    fetchMetadata,
    selectedTable,
    statData,
    fetchData,
    exportCSV,
    loading,
    error,
  };
}

function downloadCSV(csv: string, filename: string) {
  const blob = new Blob([csv], { type: 'text/csv' });
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = filename;
  a.click();
  URL.revokeObjectURL(url);
}

export default usePxWebData;
