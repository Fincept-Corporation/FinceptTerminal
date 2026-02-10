// ResultsDisplayNode.tsx - Display results from previous nodes
import React, { useState } from 'react';
import { Position } from 'reactflow';
import { Eye, Copy, Check, Maximize2, Minimize2 } from 'lucide-react';
import {
  FINCEPT,
  FONT_FAMILY,
  BORDER_RADIUS,
  SPACING,
  BaseNode,
  NodeHeader,
  IconButton,
  EmptyState,
} from './shared';

interface ResultsDisplayNodeProps {
  data: {
    label: string;
    inputData?: any;
    color: string;
  };
  selected: boolean;
}

const ResultsDisplayNode: React.FC<ResultsDisplayNodeProps> = ({ data, selected }) => {
  const [expanded, setExpanded] = useState(true);
  const [copied, setCopied] = useState(false);
  const [width, setWidth] = useState(400);
  const [height, setHeight] = useState(300);

  const handleCopy = () => {
    if (data.inputData) {
      navigator.clipboard.writeText(JSON.stringify(data.inputData, null, 2));
      setCopied(true);
      setTimeout(() => setCopied(false), 2000);
    }
  };

  const formatValue = (value: any): string => {
    if (value === null || value === undefined) return 'null';
    if (typeof value === 'object') {
      return JSON.stringify(value, null, 2);
    }
    return String(value);
  };

  const renderMarkdown = (text: string) => {
    const lines = text.split('\n');
    const elements: React.ReactElement[] = [];

    lines.forEach((line, i) => {
      let style: React.CSSProperties = {
        color: FINCEPT.WHITE,
        fontSize: '10px',
        lineHeight: '1.6',
        marginBottom: '4px',
        fontFamily: FONT_FAMILY
      };

      // Markdown separators
      if (line.trim() === '---' || line.trim() === '***' || /^-{3,}$/.test(line.trim())) {
        elements.push(
          <div key={i} style={{
            borderTop: `1px solid ${FINCEPT.BORDER}`,
            margin: '8px 0',
            opacity: 0.5
          }} />
        );
        return;
      }

      // Headers
      if (line.startsWith('###')) {
        style = {
          color: FINCEPT.GREEN,
          fontWeight: 700,
          fontSize: '10px',
          marginTop: '8px',
          marginBottom: '4px',
          textTransform: 'uppercase',
          letterSpacing: '0.5px'
        };
        line = line.replace(/^###\s*/, '');
      } else if (line.startsWith('##')) {
        style = {
          color: FINCEPT.ORANGE,
          fontWeight: 700,
          fontSize: '11px',
          marginTop: '12px',
          marginBottom: '6px',
          borderBottom: `1px solid ${FINCEPT.BORDER}`,
          paddingBottom: '4px'
        };
        line = line.replace(/^##\s*/, '');
      } else if (line.startsWith('#')) {
        style = {
          color: FINCEPT.ORANGE,
          fontWeight: 700,
          fontSize: '12px',
          marginTop: '12px',
          marginBottom: '6px'
        };
        line = line.replace(/^#\s*/, '');
      }
      // Table rows
      else if (line.startsWith('|')) {
        const cells = line.split('|').map(c => c.trim()).filter(c => c);
        if (cells.every(c => /^[-:]+$/.test(c))) return; // Skip separator

        elements.push(
          <div key={i} style={{
            display: 'flex',
            gap: SPACING.MD,
            padding: '4px 0',
            borderBottom: `1px solid ${FINCEPT.BORDER}40`,
            fontFamily: FONT_FAMILY,
            fontSize: '9px'
          }}>
            {cells.map((cell, cellIdx) => (
              <div
                key={cellIdx}
                style={{
                  flex: cellIdx === 0 ? '0 0 120px' : '1',
                  color: cellIdx === 0 ? FINCEPT.CYAN : FINCEPT.WHITE,
                  fontWeight: cellIdx === 0 ? 700 : 400,
                  padding: '2px 4px',
                  overflow: 'hidden',
                  textOverflow: 'ellipsis'
                }}
              >
                {cell.replace(/\*\*/g, '')}
              </div>
            ))}
          </div>
        );
        return;
      }
      // Bullet points
      else if (line.startsWith('- ') || line.startsWith('• ')) {
        const content = line.replace(/^[-•]\s*/, '');
        elements.push(
          <div key={i} style={{ ...style, marginLeft: '12px', display: 'flex', gap: '6px' }}>
            <span style={{ color: FINCEPT.GREEN }}>•</span>
            <span>{content.replace(/\*\*/g, '')}</span>
          </div>
        );
        return;
      }
      // Numbered lists
      else if (/^\d+\.\s/.test(line)) {
        style = { ...style, marginLeft: '12px' };
        line = line.replace(/\*\*/g, '');
      }
      // Bold text
      else if (line.includes('**')) {
        const parts = line.split(/\*\*([^*]+)\*\*/g);
        elements.push(
          <div key={i} style={style}>
            {parts.map((part, idx) =>
              idx % 2 === 1 ? (
                <span key={idx} style={{ color: FINCEPT.GREEN, fontWeight: 700 }}>{part}</span>
              ) : (
                <span key={idx}>{part}</span>
              )
            )}
          </div>
        );
        return;
      }
      // Empty lines
      else if (line.trim() === '') {
        elements.push(<div key={i} style={{ height: '6px' }} />);
        return;
      }

      elements.push(<div key={i} style={style}>{line || '\u00A0'}</div>);
    });

    return <>{elements}</>;
  };

  const renderData = (obj: any, depth = 0) => {
    if (!obj || typeof obj !== 'object') {
      const strValue = formatValue(obj);
      if (typeof obj === 'string' && (obj.includes('##') || obj.includes('|') || obj.length > 200)) {
        return (
          <div style={{
            backgroundColor: FINCEPT.DARK_BG,
            border: `1px solid ${FINCEPT.BORDER}`,
            borderRadius: '2px',
            padding: '8px'
          }}>
            {renderMarkdown(obj)}
          </div>
        );
      }
      return (
        <div style={{
          color: FINCEPT.WHITE,
          fontSize: '10px',
          fontFamily: FONT_FAMILY,
          whiteSpace: 'pre-wrap'
        }}>
          {strValue}
        </div>
      );
    }

    return (
      <div style={{ marginLeft: depth > 0 ? '12px' : '0' }}>
        {Object.entries(obj).map(([key, value]) => (
          <div key={key} style={{ marginBottom: '8px' }}>
            <div style={{
              color: FINCEPT.ORANGE,
              fontSize: '9px',
              fontWeight: 700,
              marginBottom: '4px',
              textTransform: 'uppercase',
              letterSpacing: '0.5px'
            }}>
              {key.replace(/_/g, ' ')}
            </div>
            {typeof value === 'object' && value !== null ? (
              Array.isArray(value) ? (
                <div style={{
                  color: FINCEPT.CYAN,
                  fontSize: '9px',
                  marginLeft: SPACING.MD,
                  fontFamily: FONT_FAMILY
                }}>
                  {value.map((item, i) => (
                    <div key={i} style={{ marginBottom: '2px' }}>
                      • {typeof item === 'object' ? JSON.stringify(item) : String(item)}
                    </div>
                  ))}
                </div>
              ) : (
                renderData(value, depth + 1)
              )
            ) : (
              typeof value === 'string' && (value.includes('##') || value.includes('|') || value.length > 200) ? (
                <div style={{
                  backgroundColor: FINCEPT.DARK_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '2px',
                  padding: '8px',
                  marginLeft: '8px'
                }}>
                  {renderMarkdown(value)}
                </div>
              ) : (
                <div style={{
                  color: FINCEPT.WHITE,
                  fontSize: '10px',
                  fontFamily: FONT_FAMILY,
                  marginLeft: SPACING.MD,
                  whiteSpace: 'pre-wrap',
                  wordBreak: 'break-word'
                }}>
                  {formatValue(value)}
                </div>
              )
            )}
          </div>
        ))}
      </div>
    );
  };

  return (
    <BaseNode
      selected={selected}
      minWidth="280px"
      borderColor={FINCEPT.GREEN}
      handles={[
        { type: 'target', position: Position.Left, color: FINCEPT.GREEN },
      ]}
    >
      <div style={{ width: `${width}px` }}>
        {/* Header */}
        <NodeHeader
          icon={<Eye size={14} />}
          title={data.label}
          color={FINCEPT.GREEN}
          rightActions={
            <>
              <IconButton
                icon={expanded ? <Minimize2 size={10} /> : <Maximize2 size={10} />}
                onClick={() => setExpanded(!expanded)}
                title={expanded ? 'Collapse' : 'Expand'}
              />
              <IconButton
                icon={copied ? <Check size={10} /> : <Copy size={10} />}
                onClick={handleCopy}
                disabled={!data.inputData}
                active={copied}
                title="Copy to clipboard"
              />
            </>
          }
        />

        {/* Results Display */}
        {expanded && (
          <div
            onWheel={(e) => e.stopPropagation()}
            onMouseDown={(e) => e.stopPropagation()}
            style={{
              padding: SPACING.LG,
              maxHeight: `${height}px`,
              overflow: 'auto',
              backgroundColor: FINCEPT.PANEL_BG,
            }}
          >
            {data.inputData ? (
              <div
                style={{
                  backgroundColor: FINCEPT.DARK_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: BORDER_RADIUS.SM,
                  padding: '10px',
                }}
              >
                {renderData(data.inputData)}
              </div>
            ) : (
              <EmptyState
                icon={<Eye size={24} />}
                title="No data received yet"
                subtitle="Connect a node output to see results"
              />
            )}
          </div>
        )}

        {/* Resize Handle */}
        <div
          onMouseDown={(e) => {
            e.preventDefault();
            e.stopPropagation();
            const startX = e.clientX;
            const startY = e.clientY;
            const startWidth = width;
            const startHeight = height;

            const handleMouseMove = (moveEvent: MouseEvent) => {
              moveEvent.preventDefault();
              const deltaX = moveEvent.clientX - startX;
              const deltaY = moveEvent.clientY - startY;
              setWidth(Math.max(280, startWidth + deltaX));
              setHeight(Math.max(200, startHeight + deltaY));
            };

            const handleMouseUp = () => {
              document.removeEventListener('mousemove', handleMouseMove);
              document.removeEventListener('mouseup', handleMouseUp);
            };

            document.addEventListener('mousemove', handleMouseMove);
            document.addEventListener('mouseup', handleMouseUp);
          }}
          style={{
            position: 'absolute',
            bottom: '2px',
            right: '2px',
            width: '20px',
            height: '20px',
            cursor: 'nwse-resize',
            backgroundColor: FINCEPT.DARK_BG,
            border: `1px solid ${FINCEPT.BORDER}`,
            borderBottomRightRadius: BORDER_RADIUS.SM,
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            fontSize: '12px',
            color: FINCEPT.GRAY,
            transition: 'all 0.2s',
            zIndex: 10,
            opacity: 0.6
          }}
          onMouseEnter={(e) => {
            e.currentTarget.style.backgroundColor = FINCEPT.GREEN;
            e.currentTarget.style.color = FINCEPT.DARK_BG;
            e.currentTarget.style.borderColor = FINCEPT.GREEN;
            e.currentTarget.style.opacity = '1';
          }}
          onMouseLeave={(e) => {
            e.currentTarget.style.backgroundColor = FINCEPT.DARK_BG;
            e.currentTarget.style.color = FINCEPT.GRAY;
            e.currentTarget.style.borderColor = FINCEPT.BORDER;
            e.currentTarget.style.opacity = '0.6';
          }}
          title="Drag to resize"
        >
          ⇲
        </div>
      </div>
    </BaseNode>
  );
};

export default ResultsDisplayNode;
