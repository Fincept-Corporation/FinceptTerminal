// useExport — Generic export hook for CSV and PNG image.
// Works with any DataPoint[] dataset — no tab-specific dependencies.

import { useState, useCallback, RefObject } from 'react';
import { save } from '@tauri-apps/plugin-dialog';
import { writeFile } from '@tauri-apps/plugin-fs';
import { FINCEPT_COLORS, CHART_WIDTH, CHART_HEIGHT } from './types';
import type { DataPoint, SaveNotification } from './types';

interface ExportMeta {
  /** Title shown in image header and CSV comment, e.g. "Federal Funds Rate" */
  title: string;
  /** Subtitle shown in image header, e.g. "USA • Source: FRED" */
  subtitle?: string;
  /** Used in filename generation, e.g. "fred_GDP_USA" */
  filenameSuffix: string;
}

interface UseExportReturn {
  saveNotification: SaveNotification | null;
  setSaveNotification: (notification: SaveNotification | null) => void;
  exportCSV: () => void;
  exportImage: () => Promise<void>;
}

export function useExport(
  data: DataPoint[] | null,
  meta: ExportMeta,
  chartContainerRef: RefObject<HTMLDivElement | null>,
): UseExportReturn {
  const [saveNotification, setSaveNotification] = useState<SaveNotification | null>(null);

  const exportCSV = useCallback(() => {
    if (!data || data.length === 0) return;

    const csv = [
      `# ${meta.title}`,
      meta.subtitle ? `# ${meta.subtitle}` : null,
      '',
      'Date,Value',
      ...data.map((d) => `${d.date},${d.value}`),
    ]
      .filter((line) => line !== null)
      .join('\n');

    const blob = new Blob([csv], { type: 'text/csv' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `fincept_terminal_${meta.filenameSuffix}.csv`;
    a.click();
    URL.revokeObjectURL(url);
  }, [data, meta]);

  const exportImage = useCallback(async () => {
    if (!data || data.length === 0 || !chartContainerRef.current) return;

    try {
      const defaultFileName = `fincept_terminal_${meta.filenameSuffix}_4K.png`;
      const filePath = await save({
        defaultPath: defaultFileName,
        filters: [
          { name: 'PNG Image', extensions: ['png'] },
          { name: 'All Files', extensions: ['*'] },
        ],
        title: 'Save Chart Image (Ultra HD) - Fincept Terminal',
      });

      if (!filePath) return;

      const svgElement = chartContainerRef.current.querySelector('svg');
      if (!svgElement) return;

      const clonedSvg = svgElement.cloneNode(true) as SVGElement;
      const width = CHART_WIDTH;
      const height = CHART_HEIGHT;
      const headerHeight = 80;
      const footerHeight = 40;
      const totalHeight = height + headerHeight + footerHeight;

      // Create wrapper SVG
      const wrapperSvg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
      wrapperSvg.setAttribute('width', String(width));
      wrapperSvg.setAttribute('height', String(totalHeight));
      wrapperSvg.setAttribute('xmlns', 'http://www.w3.org/2000/svg');

      // Background
      const bg = document.createElementNS('http://www.w3.org/2000/svg', 'rect');
      bg.setAttribute('width', String(width));
      bg.setAttribute('height', String(totalHeight));
      bg.setAttribute('fill', FINCEPT_COLORS.DARK_BG);
      wrapperSvg.appendChild(bg);

      // Title
      const title = document.createElementNS('http://www.w3.org/2000/svg', 'text');
      title.setAttribute('x', '24');
      title.setAttribute('y', '35');
      title.setAttribute('fill', FINCEPT_COLORS.WHITE);
      title.setAttribute('font-family', 'IBM Plex Mono, monospace');
      title.setAttribute('font-size', '22');
      title.setAttribute('font-weight', '700');
      title.textContent = meta.title;
      wrapperSvg.appendChild(title);

      // Subtitle
      if (meta.subtitle) {
        const subtitle = document.createElementNS('http://www.w3.org/2000/svg', 'text');
        subtitle.setAttribute('x', '24');
        subtitle.setAttribute('y', '60');
        subtitle.setAttribute('fill', FINCEPT_COLORS.GRAY);
        subtitle.setAttribute('font-family', 'IBM Plex Mono, monospace');
        subtitle.setAttribute('font-size', '14');
        subtitle.textContent = meta.subtitle;
        wrapperSvg.appendChild(subtitle);
      }

      // Chart
      const chartGroup = document.createElementNS('http://www.w3.org/2000/svg', 'g');
      chartGroup.setAttribute('transform', `translate(0, ${headerHeight})`);
      chartGroup.innerHTML = clonedSvg.innerHTML;
      wrapperSvg.appendChild(chartGroup);

      // Footer
      const footer = document.createElementNS('http://www.w3.org/2000/svg', 'text');
      footer.setAttribute('x', String(width - 24));
      footer.setAttribute('y', String(totalHeight - 12));
      footer.setAttribute('fill', FINCEPT_COLORS.MUTED);
      footer.setAttribute('font-family', 'IBM Plex Mono, monospace');
      footer.setAttribute('font-size', '12');
      footer.setAttribute('text-anchor', 'end');
      footer.textContent = `Fincept Terminal \u2022 ${new Date().toISOString().split('T')[0]}`;
      wrapperSvg.appendChild(footer);

      // Watermark
      const watermark = document.createElementNS('http://www.w3.org/2000/svg', 'text');
      watermark.setAttribute('x', String(width - 50));
      watermark.setAttribute('y', String(headerHeight + 50));
      watermark.setAttribute('fill', `${FINCEPT_COLORS.WHITE}50`);
      watermark.setAttribute('font-family', 'IBM Plex Mono, monospace');
      watermark.setAttribute('font-size', '22');
      watermark.setAttribute('font-weight', '700');
      watermark.setAttribute('text-anchor', 'end');
      watermark.textContent = 'FINCEPT TERMINAL';
      wrapperSvg.appendChild(watermark);

      // Convert to canvas
      const svgData = new XMLSerializer().serializeToString(wrapperSvg);
      const svgBlob = new Blob([svgData], { type: 'image/svg+xml;charset=utf-8' });
      const svgUrl = URL.createObjectURL(svgBlob);

      const canvas = document.createElement('canvas');
      const ctx = canvas.getContext('2d');
      const img = new window.Image();

      img.onload = async () => {
        const scale = 4; // Ultra HD
        canvas.width = width * scale;
        canvas.height = totalHeight * scale;

        if (ctx) {
          ctx.imageSmoothingEnabled = true;
          ctx.imageSmoothingQuality = 'high';
          ctx.scale(scale, scale);
          ctx.fillStyle = FINCEPT_COLORS.DARK_BG;
          ctx.fillRect(0, 0, width, totalHeight);
          ctx.drawImage(img, 0, 0);

          canvas.toBlob(async (blob) => {
            if (blob) {
              try {
                const arrayBuffer = await blob.arrayBuffer();
                const uint8Array = new Uint8Array(arrayBuffer);
                await writeFile(filePath, uint8Array);
                setSaveNotification({ show: true, path: filePath, type: 'image' });
                setTimeout(() => setSaveNotification(null), 5000);
              } catch (writeErr) {
                console.error('Failed to write image file:', writeErr);
              }
            }
          }, 'image/png');
        }

        URL.revokeObjectURL(svgUrl);
      };

      img.onerror = () => {
        console.error('Failed to load SVG for image export');
        URL.revokeObjectURL(svgUrl);
      };

      img.src = svgUrl;
    } catch (err) {
      console.error('Failed to export image:', err);
    }
  }, [data, meta, chartContainerRef]);

  return {
    saveNotification,
    setSaveNotification,
    exportCSV,
    exportImage,
  };
}

export default useExport;
