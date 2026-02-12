// DataTable Component - Display economic data in table format

import React from 'react';
import { ChevronUp, ChevronDown } from 'lucide-react';
import { FINCEPT } from '../constants';
import type { DataPoint } from '../types';

interface DataTableProps {
  data: DataPoint[];
  sourceColor: string;
  formatValue: (val: number) => string;
}

export function DataTable({ data, sourceColor, formatValue }: DataTableProps) {
  return (
    <div style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', overflow: 'hidden' }}>
      <div style={{ padding: '10px 12px', backgroundColor: FINCEPT.HEADER_BG, fontSize: '9px', fontWeight: 700, color: FINCEPT.WHITE, letterSpacing: '0.5px' }}>
        DATA TABLE
      </div>
      <div style={{ maxHeight: '200px', overflowY: 'auto' }}>
        <table style={{ width: '100%', borderCollapse: 'collapse' }}>
          <thead>
            <tr style={{ backgroundColor: FINCEPT.DARK_BG }}>
              <th style={{ padding: '8px 12px', textAlign: 'left', fontSize: '9px', fontWeight: 700, color: FINCEPT.ORANGE, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>DATE</th>
              <th style={{ padding: '8px 12px', textAlign: 'right', fontSize: '9px', fontWeight: 700, color: FINCEPT.ORANGE, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>VALUE</th>
              <th style={{ padding: '8px 12px', textAlign: 'right', fontSize: '9px', fontWeight: 700, color: FINCEPT.ORANGE, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>CHANGE</th>
            </tr>
          </thead>
          <tbody>
            {[...data].reverse().map((d, i, arr) => {
              const prev = arr[i + 1];
              const change = prev ? ((d.value - prev.value) / Math.abs(prev.value)) * 100 : 0;
              return (
                <tr key={d.date} style={{ borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
                  <td style={{ padding: '8px 12px', fontSize: '11px', color: FINCEPT.WHITE }}>{d.date}</td>
                  <td style={{ padding: '8px 12px', textAlign: 'right', fontSize: '11px', color: sourceColor, fontWeight: 600 }}>{formatValue(d.value)}</td>
                  <td style={{ padding: '8px 12px', textAlign: 'right', fontSize: '10px', color: change >= 0 ? FINCEPT.GREEN : FINCEPT.RED }}>
                    {prev && (
                      <span style={{ display: 'flex', alignItems: 'center', justifyContent: 'flex-end', gap: '4px' }}>
                        {change >= 0 ? <ChevronUp size={12} /> : <ChevronDown size={12} />}
                        {Math.abs(change).toFixed(2)}%
                      </span>
                    )}
                  </td>
                </tr>
              );
            })}
          </tbody>
        </table>
      </div>
    </div>
  );
}

export default DataTable;
