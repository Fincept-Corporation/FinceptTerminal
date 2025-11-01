// ResultsDisplayNode.tsx - Display results from previous nodes
import React, { useState } from 'react';
import { Handle, Position } from 'reactflow';
import { Eye, EyeOff, Copy, Check } from 'lucide-react';

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
  const [width, setWidth] = useState(500);
  const [height, setHeight] = useState(400);

  // Debug logging
  console.log('[ResultsDisplayNode] Rendering with data:', data);
  console.log('[ResultsDisplayNode] inputData present:', !!data.inputData);
  console.log('[ResultsDisplayNode] inputData value:', data.inputData);

  // Bloomberg colors
  const ORANGE = '#FFA500';
  const WHITE = '#FFFFFF';
  const GRAY = '#787878';
  const DARK_BG = '#0a0a0a';
  const PANEL_BG = '#1a1a1a';
  const BORDER = '#2d2d2d';

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
    let inTable = false;

    lines.forEach((line, i) => {
      let style: React.CSSProperties = {
        color: WHITE,
        fontSize: '10px',
        lineHeight: '1.7',
        marginBottom: '3px',
        fontFamily: 'Consolas, monospace'
      };

      // Skip markdown separators
      if (line.trim() === '---' || line.trim() === '***' || /^-{3,}$/.test(line.trim())) {
        elements.push(
          <div key={i} style={{
            borderTop: `1px solid ${BORDER}`,
            margin: '12px 0',
            opacity: 0.5
          }} />
        );
        return;
      }

      // Headers
      if (line.startsWith('###')) {
        style = {
          color: '#10b981',
          fontWeight: 'bold',
          fontSize: '11px',
          marginTop: '12px',
          marginBottom: '6px',
          borderBottom: `1px solid ${BORDER}`,
          paddingBottom: '4px'
        };
        line = line.replace(/^###\s*/, '');
      } else if (line.startsWith('##')) {
        style = {
          color: ORANGE,
          fontWeight: 'bold',
          fontSize: '13px',
          marginTop: '16px',
          marginBottom: '8px',
          borderBottom: `2px solid ${ORANGE}40`,
          paddingBottom: '6px'
        };
        line = line.replace(/^##\s*/, '');
      } else if (line.startsWith('#')) {
        style = {
          color: ORANGE,
          fontWeight: 'bold',
          fontSize: '14px',
          marginTop: '16px',
          marginBottom: '8px'
        };
        line = line.replace(/^#\s*/, '');
      }
      // Table detection
      else if (line.startsWith('|')) {
        inTable = true;
        const cells = line.split('|').map(c => c.trim()).filter(c => c);

        // Check if it's a separator row
        if (cells.every(c => /^[-:]+$/.test(c))) {
          return; // Skip separator rows
        }

        // Render table row
        elements.push(
          <div key={i} style={{
            display: 'flex',
            gap: '8px',
            padding: '4px 0',
            borderBottom: `1px solid ${BORDER}30`,
            fontFamily: 'Consolas, monospace',
            fontSize: '9px'
          }}>
            {cells.map((cell, cellIdx) => (
              <div
                key={cellIdx}
                style={{
                  flex: cellIdx === 0 ? '0 0 150px' : '1',
                  color: cellIdx === 0 ? ORANGE : '#d4d4d4',
                  fontWeight: cellIdx === 0 ? 'bold' : 'normal',
                  padding: '2px 6px',
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
        style = {
          ...style,
          marginLeft: '16px',
          color: '#d4d4d4',
          display: 'flex',
          alignItems: 'flex-start'
        };
        const content = line.replace(/^[-•]\s*/, '');
        elements.push(
          <div key={i} style={style}>
            <span style={{ color: '#10b981', marginRight: '8px' }}>•</span>
            <span>{content.replace(/\*\*/g, '')}</span>
          </div>
        );
        return;
      }
      // Numbered lists
      else if (/^\d+\.\s/.test(line)) {
        style = {
          ...style,
          marginLeft: '16px',
          color: '#d4d4d4'
        };
        line = line.replace(/\*\*/g, '');
      }
      // Bold text inline
      else if (line.includes('**')) {
        const parts = line.split(/\*\*([^*]+)\*\*/g);
        elements.push(
          <div key={i} style={style}>
            {parts.map((part, idx) =>
              idx % 2 === 1 ? (
                <span key={idx} style={{ color: '#10b981', fontWeight: 'bold' }}>
                  {part}
                </span>
              ) : (
                <span key={idx}>{part}</span>
              )
            )}
          </div>
        );
        return;
      }
      // Italic text in parentheses
      else if (line.startsWith('*(') || line.startsWith('*')) {
        style = { ...style, color: GRAY, fontStyle: 'italic', fontSize: '9px' };
        line = line.replace(/^\*\(/, '(').replace(/\)$/, ')').replace(/^\*/, '').replace(/\*$/, '');
      }
      // Empty lines
      else if (line.trim() === '') {
        elements.push(<div key={i} style={{ height: '8px' }} />);
        return;
      }

      elements.push(<div key={i} style={style}>{line || '\u00A0'}</div>);
    });

    return <>{elements}</>;
  };

  const renderData = (obj: any, depth = 0) => {
    if (!obj || typeof obj !== 'object') {
      const strValue = formatValue(obj);
      // Check if it looks like markdown text (has headers, tables, etc)
      if (typeof obj === 'string' && (obj.includes('##') || obj.includes('|') || obj.length > 200)) {
        return (
          <div style={{
            backgroundColor: DARK_BG,
            border: `1px solid ${BORDER}`,
            borderRadius: '4px',
            padding: '8px',
            marginTop: '4px'
          }}>
            {renderMarkdown(obj)}
          </div>
        );
      }
      return (
        <div style={{ color: WHITE, fontSize: '10px', fontFamily: 'monospace', whiteSpace: 'pre-wrap' }}>
          {strValue}
        </div>
      );
    }

    return (
      <div style={{ marginLeft: depth > 0 ? '12px' : '0' }}>
        {Object.entries(obj).map(([key, value]) => (
          <div key={key} style={{ marginBottom: '8px' }}>
            <div style={{
              color: ORANGE,
              fontSize: '10px',
              fontWeight: 'bold',
              marginBottom: '4px',
              textTransform: 'uppercase',
              letterSpacing: '0.5px'
            }}>
              {key.replace(/_/g, ' ')}:
            </div>
            {typeof value === 'object' && value !== null ? (
              Array.isArray(value) ? (
                <div style={{
                  color: '#10b981',
                  fontSize: '9px',
                  marginLeft: '8px',
                  fontFamily: 'monospace'
                }}>
                  {value.map((item, i) => (
                    <div key={i} style={{ marginBottom: '2px' }}>• {item}</div>
                  ))}
                </div>
              ) : (
                renderData(value, depth + 1)
              )
            ) : (
              typeof value === 'string' && (value.includes('##') || value.includes('|') || value.length > 200) ? (
                <div style={{
                  backgroundColor: '#0f0f0f',
                  border: `1px solid ${BORDER}`,
                  borderRadius: '4px',
                  padding: '8px',
                  marginLeft: '8px',
                  marginTop: '4px'
                }}>
                  {renderMarkdown(value)}
                </div>
              ) : (
                <div style={{
                  color: WHITE,
                  fontSize: '10px',
                  fontFamily: 'monospace',
                  marginLeft: '8px',
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
    <div style={{
      backgroundColor: PANEL_BG,
      border: `2px solid ${selected ? ORANGE : data.color}`,
      borderRadius: '8px',
      padding: '12px',
      width: `${width}px`,
      fontFamily: 'Consolas, monospace',
      boxShadow: selected ? `0 0 16px ${ORANGE}60` : `0 2px 8px rgba(0,0,0,0.3)`,
      position: 'relative'
    }}>
      {/* Input Handle */}
      <Handle
        type="target"
        position={Position.Left}
        style={{
          background: data.color,
          width: '12px',
          height: '12px',
          border: `2px solid ${DARK_BG}`,
          left: '-6px'
        }}
      />

      {/* Header */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        marginBottom: '10px',
        paddingBottom: '8px',
        borderBottom: `1px solid ${BORDER}`
      }}>
        <div style={{
          color: WHITE,
          fontSize: '12px',
          fontWeight: 'bold'
        }}>
          📊 {data.label}
        </div>
        <div style={{ display: 'flex', gap: '4px' }}>
          <button
            onClick={() => setExpanded(!expanded)}
            style={{
              backgroundColor: 'transparent',
              border: `1px solid ${BORDER}`,
              color: GRAY,
              padding: '4px',
              cursor: 'pointer',
              borderRadius: '3px',
              display: 'flex',
              alignItems: 'center'
            }}
            title={expanded ? 'Collapse' : 'Expand'}
          >
            {expanded ? <EyeOff size={12} /> : <Eye size={12} />}
          </button>
          <button
            onClick={handleCopy}
            disabled={!data.inputData}
            style={{
              backgroundColor: 'transparent',
              border: `1px solid ${BORDER}`,
              color: copied ? '#10b981' : GRAY,
              padding: '4px',
              cursor: data.inputData ? 'pointer' : 'not-allowed',
              borderRadius: '3px',
              display: 'flex',
              alignItems: 'center',
              opacity: data.inputData ? 1 : 0.5
            }}
            title="Copy to clipboard"
          >
            {copied ? <Check size={12} /> : <Copy size={12} />}
          </button>
        </div>
      </div>

      {/* Results Display */}
      {expanded && (
        <div
          onWheel={(e) => e.stopPropagation()}
          onMouseDown={(e) => e.stopPropagation()}
          style={{
            backgroundColor: DARK_BG,
            border: `1px solid ${BORDER}`,
            borderRadius: '4px',
            padding: '10px',
            maxHeight: `${height}px`,
            overflow: 'auto'
          }}>
          {data.inputData ? (
            renderData(data.inputData)
          ) : (
            <div style={{
              color: GRAY,
              fontSize: '10px',
              fontStyle: 'italic',
              textAlign: 'center',
              padding: '20px'
            }}>
              No data received yet.
              <br />
              Connect this node to an output to see results.
            </div>
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
            setWidth(Math.max(300, startWidth + deltaX));
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
          bottom: '4px',
          right: '4px',
          width: '24px',
          height: '24px',
          cursor: 'nwse-resize',
          backgroundColor: DARK_BG,
          border: `2px solid ${BORDER}`,
          borderBottomRightRadius: '6px',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          fontSize: '16px',
          color: GRAY,
          transition: 'all 0.2s',
          zIndex: 10
        }}
        onMouseEnter={(e) => {
          e.currentTarget.style.backgroundColor = ORANGE;
          e.currentTarget.style.color = 'white';
          e.currentTarget.style.borderColor = ORANGE;
        }}
        onMouseLeave={(e) => {
          e.currentTarget.style.backgroundColor = DARK_BG;
          e.currentTarget.style.color = GRAY;
          e.currentTarget.style.borderColor = BORDER;
        }}
        title="Drag to resize"
      >
        ⇲
      </div>
    </div>
  );
};

export default ResultsDisplayNode;
