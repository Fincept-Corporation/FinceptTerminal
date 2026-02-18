import React, { useState, useRef, useEffect } from 'react';
import { Download, Copy, Check } from 'lucide-react';
import { FINCEPT, TYPOGRAPHY } from '../../portfolio-tab/finceptStyles';

interface MAExportButtonProps {
  data: Record<string, any>[] | string;
  filename?: string;
  accentColor?: string;
}

export const MAExportButton: React.FC<MAExportButtonProps> = ({
  data,
  filename = 'ma_export',
  accentColor = FINCEPT.ORANGE,
}) => {
  const [open, setOpen] = useState(false);
  const [copied, setCopied] = useState(false);
  const ref = useRef<HTMLDivElement>(null);

  useEffect(() => {
    const handler = (e: MouseEvent) => {
      if (ref.current && !ref.current.contains(e.target as Node)) setOpen(false);
    };
    document.addEventListener('mousedown', handler);
    return () => document.removeEventListener('mousedown', handler);
  }, []);

  const toCsv = (): string => {
    if (typeof data === 'string') return data;
    if (!data.length) return '';
    const headers = Object.keys(data[0]);
    const rows = data.map(r => headers.map(h => {
      const val = r[h];
      return typeof val === 'string' && val.includes(',') ? `"${val}"` : String(val ?? '');
    }).join(','));
    return [headers.join(','), ...rows].join('\n');
  };

  const handleDownload = () => {
    const csv = toCsv();
    const blob = new Blob([csv], { type: 'text/csv' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `${filename}.csv`;
    a.click();
    URL.revokeObjectURL(url);
    setOpen(false);
  };

  const handleCopy = async () => {
    const csv = toCsv();
    await navigator.clipboard.writeText(csv);
    setCopied(true);
    setTimeout(() => { setCopied(false); setOpen(false); }, 1200);
  };

  return (
    <div ref={ref} style={{ position: 'relative' }}>
      <button
        onClick={() => setOpen(!open)}
        style={{
          padding: '4px 8px',
          backgroundColor: 'transparent',
          border: `1px solid ${FINCEPT.BORDER}`,
          color: FINCEPT.GRAY,
          fontSize: '9px',
          fontFamily: TYPOGRAPHY.MONO,
          cursor: 'pointer',
          display: 'flex',
          alignItems: 'center',
          gap: '4px',
          transition: 'all 0.15s',
        }}
        onMouseEnter={(e) => {
          e.currentTarget.style.borderColor = accentColor;
          e.currentTarget.style.color = accentColor;
        }}
        onMouseLeave={(e) => {
          e.currentTarget.style.borderColor = FINCEPT.BORDER;
          e.currentTarget.style.color = FINCEPT.GRAY;
        }}
      >
        <Download size={10} />
        EXPORT
      </button>

      {open && (
        <div style={{
          position: 'absolute',
          top: '100%',
          right: 0,
          marginTop: '4px',
          backgroundColor: FINCEPT.HEADER_BG,
          border: `1px solid ${FINCEPT.BORDER}`,
          borderRadius: '2px',
          zIndex: 100,
          minWidth: '120px',
          overflow: 'hidden',
        }}>
          <button
            onClick={handleDownload}
            style={{
              display: 'flex',
              alignItems: 'center',
              gap: '8px',
              width: '100%',
              padding: '8px 12px',
              backgroundColor: 'transparent',
              border: 'none',
              color: FINCEPT.WHITE,
              fontSize: '10px',
              fontFamily: TYPOGRAPHY.MONO,
              cursor: 'pointer',
              textAlign: 'left',
            }}
            onMouseEnter={(e) => e.currentTarget.style.backgroundColor = FINCEPT.HOVER}
            onMouseLeave={(e) => e.currentTarget.style.backgroundColor = 'transparent'}
          >
            <Download size={12} /> CSV Download
          </button>
          <button
            onClick={handleCopy}
            style={{
              display: 'flex',
              alignItems: 'center',
              gap: '8px',
              width: '100%',
              padding: '8px 12px',
              backgroundColor: 'transparent',
              border: 'none',
              borderTop: `1px solid ${FINCEPT.BORDER}`,
              color: copied ? FINCEPT.GREEN : FINCEPT.WHITE,
              fontSize: '10px',
              fontFamily: TYPOGRAPHY.MONO,
              cursor: 'pointer',
              textAlign: 'left',
            }}
            onMouseEnter={(e) => e.currentTarget.style.backgroundColor = FINCEPT.HOVER}
            onMouseLeave={(e) => e.currentTarget.style.backgroundColor = 'transparent'}
          >
            {copied ? <Check size={12} /> : <Copy size={12} />}
            {copied ? 'Copied!' : 'Copy to Clipboard'}
          </button>
        </div>
      )}
    </div>
  );
};
