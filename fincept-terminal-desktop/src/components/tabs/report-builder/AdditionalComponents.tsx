import React, { useState } from 'react';
import {
  Quote,
  List,
  ListOrdered,
  FileText,
  Stamp,
  QrCode,
  PenTool,
  AlertTriangle,
  ChevronRight,
  Hash,
} from 'lucide-react';

// ============ Quote/Callout Box ============
interface QuoteBoxProps {
  content: string;
  author?: string;
  type?: 'quote' | 'info' | 'warning' | 'success' | 'error';
}

export const QuoteBox: React.FC<QuoteBoxProps> = ({ content, author, type = 'quote' }) => {
  const styles = {
    quote: { bg: '#f8f9fa', border: '#6c757d', icon: Quote },
    info: { bg: '#e7f3ff', border: '#0066cc', icon: FileText },
    warning: { bg: '#fff8e1', border: '#ffa500', icon: AlertTriangle },
    success: { bg: '#e8f5e9', border: '#4caf50', icon: FileText },
    error: { bg: '#ffebee', border: '#f44336', icon: AlertTriangle },
  };

  const style = styles[type];
  const Icon = style.icon;

  return (
    <div
      className="p-4 rounded-r-lg my-4"
      style={{ backgroundColor: style.bg, borderLeft: `4px solid ${style.border}` }}
    >
      <div className="flex gap-3">
        <Icon size={20} style={{ color: style.border }} className="flex-shrink-0 mt-0.5" />
        <div className="flex-1">
          <p className="text-sm text-gray-700 italic">{content}</p>
          {author && <p className="text-xs text-gray-500 mt-2">— {author}</p>}
        </div>
      </div>
    </div>
  );
};

// ============ Bullet/Numbered Lists ============
interface ListComponentProps {
  items: string[];
  ordered?: boolean;
  nested?: boolean;
}

export const ListComponent: React.FC<ListComponentProps> = ({ items, ordered, nested }) => {
  const Tag = ordered ? 'ol' : 'ul';

  return (
    <Tag className={`my-3 ${ordered ? 'list-decimal' : 'list-disc'} ${nested ? 'ml-6' : 'ml-4'}`}>
      {items.map((item, idx) => (
        <li key={idx} className="text-sm text-gray-800 py-1 pl-1">
          {item}
        </li>
      ))}
    </Tag>
  );
};

// ============ Footnotes/References ============
interface FootnoteProps {
  id: number;
  content: string;
  inline?: boolean;
}

export const Footnote: React.FC<FootnoteProps> = ({ id, content, inline }) => {
  if (inline) {
    return (
      <sup className="text-[#FFA500] cursor-pointer hover:underline text-xs font-semibold">
        [{id}]
      </sup>
    );
  }

  return (
    <div className="flex gap-2 py-1 text-xs text-gray-600 border-t border-gray-200">
      <span className="text-[#FFA500] font-semibold">[{id}]</span>
      <span>{content}</span>
    </div>
  );
};

interface FootnotesListProps {
  footnotes: { id: number; content: string }[];
}

export const FootnotesList: React.FC<FootnotesListProps> = ({ footnotes }) => (
  <div className="mt-8 pt-4 border-t-2 border-gray-300">
    <h4 className="text-xs font-bold text-gray-600 mb-2">REFERENCES</h4>
    {footnotes.map(fn => (
      <Footnote key={fn.id} id={fn.id} content={fn.content} />
    ))}
  </div>
);

// ============ Table of Contents ============
interface TOCItem {
  id: string;
  title: string;
  level: number;
  page?: number;
}

interface TableOfContentsProps {
  items: TOCItem[];
  showPageNumbers?: boolean;
}

export const TableOfContents: React.FC<TableOfContentsProps> = ({ items, showPageNumbers = true }) => (
  <div className="bg-gray-50 p-4 rounded border border-gray-200 my-4">
    <h3 className="text-sm font-bold text-gray-800 mb-3 pb-2 border-b">Table of Contents</h3>
    <nav>
      {items.map((item, idx) => (
        <div
          key={item.id}
          className="flex items-center justify-between py-1.5 hover:bg-gray-100 rounded px-2 cursor-pointer"
          style={{ paddingLeft: `${(item.level - 1) * 16 + 8}px` }}
        >
          <div className="flex items-center gap-2">
            {item.level === 1 && <ChevronRight size={12} className="text-[#FFA500]" />}
            <span className={`text-sm ${item.level === 1 ? 'font-semibold' : ''} text-gray-700`}>
              {item.title}
            </span>
          </div>
          {showPageNumbers && item.page && (
            <span className="text-xs text-gray-400">{item.page}</span>
          )}
        </div>
      ))}
    </nav>
  </div>
);

// ============ Watermark ============
interface WatermarkProps {
  text: string;
  opacity?: number;
  rotation?: number;
  color?: string;
}

export const Watermark: React.FC<WatermarkProps> = ({
  text,
  opacity = 0.1,
  rotation = -45,
  color = '#000000'
}) => (
  <div
    className="absolute inset-0 flex items-center justify-center pointer-events-none overflow-hidden"
    style={{ zIndex: 0 }}
  >
    <span
      className="text-6xl font-bold whitespace-nowrap select-none"
      style={{
        opacity,
        transform: `rotate(${rotation}deg)`,
        color,
      }}
    >
      {text}
    </span>
  </div>
);

// ============ QR Code (Simple SVG-based) ============
interface QRCodeProps {
  value: string;
  size?: number;
  label?: string;
}

export const QRCodeComponent: React.FC<QRCodeProps> = ({ value, size = 100, label }) => {
  // Simple placeholder - in production, use a QR library
  const generatePattern = (str: string) => {
    const pattern: boolean[][] = [];
    const gridSize = 21;
    for (let i = 0; i < gridSize; i++) {
      pattern[i] = [];
      for (let j = 0; j < gridSize; j++) {
        // Positioning patterns (corners)
        const isCorner = (i < 7 && j < 7) || (i < 7 && j >= gridSize - 7) || (i >= gridSize - 7 && j < 7);
        if (isCorner) {
          const inOuter = i === 0 || i === 6 || j === 0 || j === 6 ||
                         (i >= gridSize - 7 && (i === gridSize - 7 || i === gridSize - 1)) ||
                         (j >= gridSize - 7 && (j === gridSize - 7 || j === gridSize - 1));
          const inInner = (i >= 2 && i <= 4 && j >= 2 && j <= 4) ||
                         (i >= 2 && i <= 4 && j >= gridSize - 5 && j <= gridSize - 3) ||
                         (i >= gridSize - 5 && i <= gridSize - 3 && j >= 2 && j <= 4);
          pattern[i][j] = inOuter || inInner;
        } else {
          // Data pattern based on string hash
          const hash = (str.charCodeAt((i * gridSize + j) % str.length) + i + j) % 3;
          pattern[i][j] = hash === 0;
        }
      }
    }
    return pattern;
  };

  const pattern = generatePattern(value);
  const cellSize = size / 21;

  return (
    <div className="inline-flex flex-col items-center gap-2">
      <svg width={size} height={size} className="border border-gray-300 rounded">
        <rect width={size} height={size} fill="white" />
        {pattern.map((row, i) =>
          row.map((cell, j) =>
            cell && (
              <rect
                key={`${i}-${j}`}
                x={j * cellSize}
                y={i * cellSize}
                width={cellSize}
                height={cellSize}
                fill="black"
              />
            )
          )
        )}
      </svg>
      {label && <span className="text-xs text-gray-500">{label}</span>}
    </div>
  );
};

// ============ Signature Block ============
interface SignatureBlockProps {
  name: string;
  title?: string;
  date?: string;
  showLine?: boolean;
}

export const SignatureBlock: React.FC<SignatureBlockProps> = ({ name, title, date, showLine = true }) => (
  <div className="mt-8 inline-block min-w-[200px]">
    {showLine && <div className="border-b border-gray-400 h-8 mb-2" />}
    <p className="text-sm font-semibold text-gray-800">{name}</p>
    {title && <p className="text-xs text-gray-600">{title}</p>}
    {date && <p className="text-xs text-gray-500 mt-1">Date: {date}</p>}
  </div>
);

// ============ Disclaimer Section ============
interface DisclaimerProps {
  title?: string;
  content: string;
  type?: 'standard' | 'legal' | 'confidential';
}

export const DisclaimerSection: React.FC<DisclaimerProps> = ({
  title,
  content,
  type = 'standard'
}) => {
  const styles = {
    standard: { bg: '#f5f5f5', border: '#ddd' },
    legal: { bg: '#fff8e1', border: '#ffa500' },
    confidential: { bg: '#ffebee', border: '#f44336' },
  };

  const defaultTitles = {
    standard: 'Disclaimer',
    legal: 'Legal Notice',
    confidential: 'Confidential',
  };

  const style = styles[type];

  return (
    <div
      className="mt-8 p-4 rounded text-xs"
      style={{ backgroundColor: style.bg, border: `1px solid ${style.border}` }}
    >
      <h4 className="font-bold text-gray-700 mb-2 uppercase text-[10px]">
        {title || defaultTitles[type]}
      </h4>
      <p className="text-gray-600 leading-relaxed">{content}</p>
    </div>
  );
};

// ============ Section Header ============
interface SectionHeaderProps {
  number?: string;
  title: string;
  subtitle?: string;
}

export const SectionHeader: React.FC<SectionHeaderProps> = ({ number, title, subtitle }) => (
  <div className="mb-4 pb-2 border-b-2 border-[#FFA500]">
    <div className="flex items-baseline gap-3">
      {number && (
        <span className="text-2xl font-bold text-[#FFA500]">{number}</span>
      )}
      <h2 className="text-xl font-bold text-gray-800">{title}</h2>
    </div>
    {subtitle && <p className="text-sm text-gray-500 mt-1">{subtitle}</p>}
  </div>
);

// ============ Page Number ============
interface PageNumberProps {
  current: number;
  total?: number;
  format?: 'numeric' | 'roman';
  position?: 'left' | 'center' | 'right';
}

export const PageNumber: React.FC<PageNumberProps> = ({
  current,
  total,
  format = 'numeric',
  position = 'center'
}) => {
  const toRoman = (num: number): string => {
    const romanNumerals: [number, string][] = [
      [1000, 'M'], [900, 'CM'], [500, 'D'], [400, 'CD'],
      [100, 'C'], [90, 'XC'], [50, 'L'], [40, 'XL'],
      [10, 'X'], [9, 'IX'], [5, 'V'], [4, 'IV'], [1, 'I']
    ];
    let result = '';
    for (const [value, symbol] of romanNumerals) {
      while (num >= value) {
        result += symbol;
        num -= value;
      }
    }
    return result;
  };

  const formatNumber = (n: number) => format === 'roman' ? toRoman(n) : n.toString();

  return (
    <div className={`text-xs text-gray-500 text-${position}`}>
      {total ? `${formatNumber(current)} / ${formatNumber(total)}` : formatNumber(current)}
    </div>
  );
};

export default {
  QuoteBox,
  ListComponent,
  Footnote,
  FootnotesList,
  TableOfContents,
  Watermark,
  QRCodeComponent,
  SignatureBlock,
  DisclaimerSection,
  SectionHeader,
  PageNumber,
};
